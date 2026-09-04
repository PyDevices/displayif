// SPDX-License-Identifier: MIT
// jpegio's LVGL image decoder: baseline JPEG through jpegio's own TJpgDec,
// registered with LVGL's public lv_image_decoder_create API. Phase 2 of the
// org's docs/jpegio-vision.md: LVGL's built-in TJPGD is off (LV_USE_TJPGD 0
// in lvgl-bindings' lv_conf.h on MicroPython), so a firmware with both
// usermods carries one TJpgDec -- jpegio's, vendored in tjpgd/ -- and this
// file is that object code's second user.
//
// Structure follows lvgl/src/libs/tjpgd/lv_tjpgd.c (read, never modified)
// for registration and the info/open/close split. The pixel path does not:
// lv_tjpgd.c decodes MCU by MCU from get_area_cb through LVGL-only exports
// (jd_mcu_load, jd_mcu_output, jd_restart, JDEC.rst/rsc/pool_original) that
// ChaN's R0.03 -- the copy jpegio ships -- does not have, and it hardcodes
// RGB888 (header.cf/stride at its lines 95-98, 117-120, 207-210, 264). This
// decoder does what LVGL's own full-decode decoders do (lv_lodepng.c at
// v9.5.0): open() decodes the whole image once with jd_decomp into a draw
// buffer from LVGL's image-cache handlers, hands it back as dsc->decoded,
// and close() destroys it unless the image cache kept it. No get_area_cb:
// LVGL only calls that when open() returns no pixels, and ChaN's jd_decomp
// is all-or-nothing, so an incremental path would re-decode the frame per
// area. Output is native-order RGB565 (JD_FORMAT 1 in tjpgd/tjpgdcnf.h,
// LV_COLOR_16_SWAP unset), labelled LV_COLOR_FORMAT_RGB565, stride w * 2.
//
// Source sniff is SOI only (FF D8), for a variable source's first two bytes
// or a file's -- UVC MJPEG frames are not JFIF-first and lv_tjpgd.c's
// 10-byte JFIF signature rejects every one of them. jd_prepare then judges:
// decoder_info parses the headers for both source kinds, so width/height are
// the stream's (not echoed from the caller's lv_image_dsc_t header) and a
// pixel buffer that happens to start FF D8 falls through to LVGL's built-in
// decoder instead of being claimed.

#include "lvgl.h"
#include "src/draw/lv_image_decoder_private.h"  // lv_image_decoder_t.info_cb, lv_image_cache_data_t
#include "src/core/lv_global.h"                 // LV_GLOBAL_DEFAULT()->image_cache_draw_buf_handlers

#include <string.h>

#include "tjpgd.h"
#include "lvgl_decoder.h"

#if JD_FORMAT != 1
#error "jpegio's LVGL decoder labels its output RGB565: TJpgDec must be built with JD_FORMAT 1"
#endif
#if defined(LV_COLOR_16_SWAP) && LV_COLOR_16_SWAP   /* -Wundef clean: lv_conf.h does not define it */
#error "jpegio's LVGL decoder emits native-order RGB565; LV_COLOR_16_SWAP is not supported"
#endif

// CircuitPython's TJpgDec work-area size, the same 3500 bytes jpegio.c gives
// its JpegDecoder (TJPGD_WORKSPACE_SIZE there). Enough for the 512-byte
// stream buffer, the tables, and a 16x16 MCU; allocated per open, freed
// before it returns.
#define JPEGIO_LVGL_WORKSPACE_SIZE 3500

#define image_cache_draw_buf_handlers &(LV_GLOBAL_DEFAULT()->image_cache_draw_buf_handlers)

// One decode session: the input TJpgDec pulls from and the RGB565 rows it
// pushes to. Exactly one of buf / file is live.
typedef struct {
    const uint8_t *buf;     // variable source: unread remainder
    size_t len;
    lv_fs_file_t *file;     // file source, else NULL
    uint8_t *dst;           // decoded->data (NULL while only preparing)
    uint32_t stride;        // decoded->header.stride, bytes
} jpegio_lvgl_session_t;

static lv_result_t decoder_info(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc, lv_image_header_t *header);
static lv_result_t decoder_open(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc);
static void decoder_close(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc);

// --- registration -----------------------------------------------------------

static lv_image_decoder_t *find_registered(void) {
    lv_image_decoder_t *dec = NULL;
    while ((dec = lv_image_decoder_get_next(dec)) != NULL) {
        if (dec->info_cb == decoder_info) {
            return dec;
        }
    }
    return NULL;
}

bool jpegio_lvgl_decoder_registered(void) {
    return lv_is_initialized() && find_registered() != NULL;
}

const char *jpegio_lvgl_decoder_name_at(size_t i) {
    if (!lv_is_initialized()) {
        return NULL;
    }
    lv_image_decoder_t *dec = NULL;
    while ((dec = lv_image_decoder_get_next(dec)) != NULL) {
        if (i-- == 0) {
            return dec->name != NULL ? dec->name : "";
        }
    }
    return NULL;
}

bool jpegio_lvgl_decoder_register(void) {
    if (!lv_is_initialized()) {
        return false;
    }
    if (find_registered() != NULL) {
        return true;    // registering twice adds nothing
    }
    // lv_image_decoder_create inserts at the head of LVGL's list, so this
    // decoder is consulted before the built-in one (which claims any
    // variable source with a known cf) -- same position lv_tjpgd_init took.
    lv_image_decoder_t *dec = lv_image_decoder_create();
    if (dec == NULL) {
        return false;
    }
    lv_image_decoder_set_info_cb(dec, decoder_info);
    lv_image_decoder_set_open_cb(dec, decoder_open);
    lv_image_decoder_set_close_cb(dec, decoder_close);
    dec->name = JPEGIO_LVGL_DECODER_NAME;
    return true;
}

// --- TJpgDec input / output -------------------------------------------------

static size_t input_func(JDEC *jd, uint8_t *dest, size_t len) {
    jpegio_lvgl_session_t *s = jd->device;
    if (s->file != NULL) {
        if (dest != NULL) {
            uint32_t rn = 0;
            if (lv_fs_read(s->file, dest, (uint32_t)len, &rn) != LV_FS_RES_OK) {
                return 0;
            }
            return rn;
        }
        uint32_t pos = 0;
        lv_fs_tell(s->file, &pos);
        lv_fs_seek(s->file, (uint32_t)(pos + len), LV_FS_SEEK_SET);
        return len;
    }
    size_t n = len < s->len ? len : s->len;
    if (dest != NULL) {
        memcpy(dest, s->buf, n);
    }
    s->buf += n;
    s->len -= n;
    return n;
}

// TJpgDec hands MCU blocks in raster order, clipped to the image edge, as
// w*h native-order RGB565: copy each row into the draw buffer at its place.
static int output_func(JDEC *jd, void *bitmap, JRECT *rect) {
    jpegio_lvgl_session_t *s = jd->device;
    size_t w = (size_t)rect->right - rect->left + 1;
    size_t h = (size_t)rect->bottom - rect->top + 1;
    const uint8_t *src = bitmap;
    uint8_t *dst = s->dst + (size_t)rect->top * s->stride + (size_t)rect->left * 2;
    for (size_t row = 0; row < h; row++) {
        memcpy(dst + row * s->stride, src + row * w * 2, w * 2);
    }
    return 1;
}

static bool starts_with_soi(const uint8_t *p, size_t n) {
    return n >= 2 && p[0] == 0xFF && p[1] == 0xD8;
}

// Point the session at dsc's source. For a file the caller supplies the open
// handle (decoder_info gets dsc->file from LVGL, decoder_open opens its own).
static bool session_from_source(jpegio_lvgl_session_t *s, const lv_image_decoder_dsc_t *dsc, lv_fs_file_t *file) {
    memset(s, 0, sizeof(*s));
    if (dsc->src_type == LV_IMAGE_SRC_VARIABLE) {
        const lv_image_dsc_t *img = dsc->src;
        if (!starts_with_soi(img->data, img->data_size)) {
            return false;
        }
        s->buf = img->data;
        s->len = img->data_size;
        return true;
    }
    if (dsc->src_type == LV_IMAGE_SRC_FILE && file != NULL) {
        uint8_t soi[2];
        uint32_t rn = 0;
        if (lv_fs_seek(file, 0, LV_FS_SEEK_SET) != LV_FS_RES_OK
            || lv_fs_read(file, soi, sizeof(soi), &rn) != LV_FS_RES_OK
            || !starts_with_soi(soi, rn)
            || lv_fs_seek(file, 0, LV_FS_SEEK_SET) != LV_FS_RES_OK) {
            return false;
        }
        s->file = file;
        return true;
    }
    return false;
}

// --- LVGL decoder callbacks -------------------------------------------------

static lv_result_t decoder_info(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc, lv_image_header_t *header) {
    LV_UNUSED(decoder);
    jpegio_lvgl_session_t s;
    // LVGL opened dsc->file for a file source and seeks it to 0 before each
    // decoder's info_cb; it closes it afterwards.
    if (!session_from_source(&s, dsc, dsc->src_type == LV_IMAGE_SRC_FILE ? &dsc->file : NULL)) {
        return LV_RESULT_INVALID;
    }
    uint8_t *work = lv_malloc(JPEGIO_LVGL_WORKSPACE_SIZE);
    if (work == NULL) {
        return LV_RESULT_INVALID;
    }
    JDEC jd;
    JRESULT rc = jd_prepare(&jd, input_func, work, JPEGIO_LVGL_WORKSPACE_SIZE, &s);
    lv_free(work);
    if (rc != JDR_OK) {
        LV_LOG_INFO("jpegio: jd_prepare error %d", rc);
        return LV_RESULT_INVALID;
    }
    header->cf = LV_COLOR_FORMAT_RGB565;
    header->w = jd.width;
    header->h = jd.height;
    header->stride = (uint32_t)jd.width * 2;
    return LV_RESULT_OK;
}

static lv_result_t decoder_open(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc) {
    lv_fs_file_t *f = NULL;
    if (dsc->src_type == LV_IMAGE_SRC_FILE) {
        f = lv_malloc(sizeof(lv_fs_file_t));
        if (f == NULL) {
            return LV_RESULT_INVALID;
        }
        if (lv_fs_open(f, dsc->src, LV_FS_MODE_RD) != LV_FS_RES_OK) {
            lv_free(f);
            return LV_RESULT_INVALID;
        }
    }
    jpegio_lvgl_session_t s;
    lv_draw_buf_t *decoded = NULL;
    uint8_t *work = NULL;
    JRESULT rc = JDR_FMT1;

    if (session_from_source(&s, dsc, f)) {
        work = lv_malloc(JPEGIO_LVGL_WORKSPACE_SIZE);
        if (work != NULL) {
            JDEC jd;
            rc = jd_prepare(&jd, input_func, work, JPEGIO_LVGL_WORKSPACE_SIZE, &s);
            if (rc == JDR_OK) {
                decoded = lv_draw_buf_create_ex(image_cache_draw_buf_handlers, jd.width, jd.height,
                    LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
                if (decoded != NULL) {
                    s.dst = decoded->data;
                    s.stride = decoded->header.stride;
                    rc = jd_decomp(&jd, output_func, 0);
                } else {
                    rc = JDR_MEM1;
                }
            }
            lv_free(work);
        } else {
            rc = JDR_MEM1;
        }
    }
    if (f != NULL) {
        lv_fs_close(f);
        lv_free(f);
    }
    if (rc != JDR_OK) {
        LV_LOG_WARN("jpegio: decode error %d", rc);
        if (decoded != NULL) {
            lv_draw_buf_destroy(decoded);
        }
        return LV_RESULT_INVALID;
    }

    // From here the same as lv_lodepng.c's decoder_open at v9.5.0.
    lv_draw_buf_t *adjusted = lv_image_decoder_post_process(dsc, decoded);
    if (adjusted == NULL) {
        lv_draw_buf_destroy(decoded);
        return LV_RESULT_INVALID;
    }
    if (adjusted != decoded) {
        lv_draw_buf_destroy(decoded);
        decoded = adjusted;
    }
    dsc->decoded = decoded;

    if (dsc->args.no_cache || !lv_image_cache_is_enabled()) {
        return LV_RESULT_OK;
    }
    lv_image_cache_data_t search_key;
    search_key.src_type = dsc->src_type;
    search_key.src = dsc->src;
    search_key.slot.size = decoded->data_size;
    lv_cache_entry_t *entry = lv_image_decoder_add_to_cache(decoder, &search_key, decoded, NULL);
    if (entry == NULL) {
        lv_draw_buf_destroy(decoded);
        dsc->decoded = NULL;
        return LV_RESULT_INVALID;
    }
    dsc->cache_entry = entry;
    return LV_RESULT_OK;
}

static void decoder_close(lv_image_decoder_t *decoder, lv_image_decoder_dsc_t *dsc) {
    LV_UNUSED(decoder);
    if (dsc->args.no_cache || !lv_image_cache_is_enabled()) {
        lv_draw_buf_destroy((lv_draw_buf_t *)dsc->decoded);
    }
}
