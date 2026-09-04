// SPDX-License-Identifier: MIT
// jpegio: CircuitPython-compatible baseline JPEG decoder (TJpgDec R0.03) as a
// MicroPython native module.
//
// Port of CircuitPython's shared-bindings/jpegio + shared-module/jpegio
// (Copyright (c) 2023 Jeff Epler for Adafruit Industries, MIT) with the
// platform's RGB565 buffer or a per-block callback in place of
// displayio.Bitmap. The API contract is in README.md next to this file.

#include <stdint.h>
#include <string.h>

#include "py/obj.h"
#include "py/objarray.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "py/builtin.h"
#include "py/mperrno.h"

#include "tjpgd.h"

#if JD_FORMAT != 1
#error "jpegio needs TJpgDec built with JD_FORMAT 1 (RGB565) -- see tjpgd/tjpgdcnf.h"
#endif

// Defined by micropython.mk / micropython.cmake only when the lvgl-micropython
// sibling usermod is in the same build: jpegio then also registers an LVGL
// image decoder on its TJpgDec (lvgl_decoder.c).
#ifndef JPEGIO_LVGL_DECODER
#define JPEGIO_LVGL_DECODER 0
#endif
#if JPEGIO_LVGL_DECODER
#include "lvgl_decoder.h"
#endif

// CircuitPython's work-area size (shared-module/jpegio/JpegDecoder.h). Holds
// TJpgDec's 512-byte stream buffer, the quantiser and Huffman tables, and the
// MCU + IDCT scratch for a 16x16 (4:2:0) MCU. Allocated once, inside the object.
#define TJPGD_WORKSPACE_SIZE 3500

typedef struct _jpegio_jpegdecoder_obj_t {
    mp_obj_base_t base;
    JDEC decoder;
    uint8_t workspace[TJPGD_WORKSPACE_SIZE];
    // Source handed to open(): a buffer-protocol object or a binary stream.
    mp_obj_t source;            // MP_OBJ_NULL once closed
    mp_buffer_info_t bufinfo;   // unread remainder of a buffer source
    bool owns_source;           // opened from a str path by us: close it when done
    bool ready;                 // jd_prepare succeeded and decode() has not consumed it
    uint16_t width, height;     // of the last successfully opened image; 0 after a failed open()
    // Target of the decode() in progress. Exactly one of callback / pixels is live.
    mp_obj_t callback;          // callable target, else MP_OBJ_NULL
    mp_obj_t target;            // buffer target object, kept alive while its pointer is used
    uint16_t *pixels;           // buffer target, native-order RGB565
    size_t stride;              // buffer row stride in pixels
    mp_int_t x, y;              // placement of the decoded image
    mp_obj_t block_view;        // reusable memoryview handed to the callback per block
} jpegio_jpegdecoder_obj_t;

#define JPEGIO_MIN(a, b) ((a) < (b) ? (a) : (b))

// decode()'s x / y limit: the image size is a uint16 in TJpgDec (and in
// CircuitPython's Bitmap), so a placement beyond it is never meaningful. It
// also keeps every coordinate sum below 2^17, so the buffer-fit arithmetic
// cannot overflow and the callback's coordinates always fit a small int.
#define JPEGIO_MAX_COORD 0xFFFF

// --- TJpgDec result → exception --------------------------------------------

static void jpegio_raise_jresult(JRESULT rc) {
    switch (rc) {
        case JDR_OK:
            return;
        case JDR_INTR:
            mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("JDR_INTR: interrupted by output function"));
        case JDR_INP:
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("JDR_INP: input ended early (no SOI marker, or truncated JPEG)"));
        case JDR_MEM1:
            mp_raise_msg_varg(&mp_type_MemoryError, MP_ERROR_TEXT("JDR_MEM1: image needs more than the %d-byte work area"), TJPGD_WORKSPACE_SIZE);
        case JDR_MEM2:
            mp_raise_msg_varg(&mp_type_MemoryError, MP_ERROR_TEXT("JDR_MEM2: a JPEG segment is larger than the %d-byte input buffer"), JD_SZBUF);
        case JDR_PAR:
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("JDR_PAR: parameter error"));
        case JDR_FMT1:
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("JDR_FMT1: unsupported or malformed JPEG (missing tables or corrupt data)"));
        case JDR_FMT2:
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("JDR_FMT2: right format but not supported"));
        case JDR_FMT3:
        default:
            mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("JDR_FMT3: progressive JPEG is not supported (nor lossless, arithmetic, CMYK, or 4:1:1 / 4:4:0 subsampling)"));
    }
}

// --- source lifecycle -------------------------------------------------------

static void jpegio_close(jpegio_jpegdecoder_obj_t *self) {
    if (self->owns_source && self->source != MP_OBJ_NULL) {
        mp_stream_close(self->source);
    }
    self->owns_source = false;
    self->source = MP_OBJ_NULL;
    self->ready = false;
    memset(&self->bufinfo, 0, sizeof(self->bufinfo));
}

// TJpgDec input function for a buffer-protocol source: hand out the next
// `len` bytes of the caller's buffer (no copy of the source itself; TJpgDec
// fills its own 512-byte stream buffer from it). NULL dest means skip.
static size_t jpegio_buffer_input(JDEC *jd, uint8_t *dest, size_t len) {
    jpegio_jpegdecoder_obj_t *self = jd->device;
    size_t n = JPEGIO_MIN(len, self->bufinfo.len);
    if (dest != NULL) {
        memcpy(dest, self->bufinfo.buf, n);
    }
    self->bufinfo.buf = (uint8_t *)self->bufinfo.buf + n;
    self->bufinfo.len -= n;
    return n;
}

// TJpgDec input function for a native binary stream (file, io.BytesIO,
// socket, ...). NULL dest means skip: read it through a scratch buffer rather
// than seeking, so unseekable streams work.
static size_t jpegio_stream_input(JDEC *jd, uint8_t *dest, size_t len) {
    jpegio_jpegdecoder_obj_t *self = jd->device;
    if (dest == NULL) {
        uint8_t skip[128];
        size_t total = 0;
        while (total < len) {
            size_t n = jpegio_stream_input(jd, skip, JPEGIO_MIN(len - total, sizeof(skip)));
            if (n == 0) {
                break;
            }
            total += n;
        }
        return total;
    }
    int errcode = 0;
    mp_uint_t n = mp_stream_rw(self->source, dest, len, &errcode, MP_STREAM_RW_READ);
    if (errcode != 0) {
        // An I/O failure is more useful than the JDR_INP it would become.
        mp_raise_OSError(errcode);
    }
    return n;
}

// --- TJpgDec output functions -----------------------------------------------

// Buffer target: copy each block's rows into place. TJpgDec hands blocks in
// raster order, already clipped to the image edge, as w*h native-order RGB565.
static int jpegio_buffer_output(JDEC *jd, void *data, JRECT *rect) {
    jpegio_jpegdecoder_obj_t *self = jd->device;
    size_t w = (size_t)rect->right - rect->left + 1;
    size_t h = (size_t)rect->bottom - rect->top + 1;
    const uint16_t *src = data;
    // decode() proved (y + dh - 1) * stride + x + dw <= the buffer's pixel
    // count without overflowing, so every index formed here is inside it.
    uint16_t *dst = self->pixels + ((size_t)self->y + rect->top) * self->stride + (size_t)self->x + rect->left;
    for (size_t row = 0; row < h; row++) {
        memcpy(dst + row * self->stride, src + row * w, w * sizeof(uint16_t));
    }
    return 1;
}

// Callable target: callback(x, y, w, h, block) with `block` a view of TJpgDec's
// own output buffer -- valid only until the callback returns. One view object
// is reused for every block, so the callback path allocates nothing per block.
static int jpegio_callback_output(JDEC *jd, void *data, JRECT *rect) {
    jpegio_jpegdecoder_obj_t *self = jd->device;
    size_t w = (size_t)rect->right - rect->left + 1;
    size_t h = (size_t)rect->bottom - rect->top + 1;
    mp_obj_array_t *view = MP_OBJ_TO_PTR(self->block_view);
    view->items = data;
    #if MICROPY_PY_BUILTINS_MEMORYVIEW
    view->len = w * h;          // memoryview('H'): one element per pixel
    #else
    view->len = w * h * 2;      // bytearray fallback: length in bytes
    #endif
    mp_obj_t args[5] = {
        MP_OBJ_NEW_SMALL_INT(self->x + rect->left),
        MP_OBJ_NEW_SMALL_INT(self->y + rect->top),
        MP_OBJ_NEW_SMALL_INT(w),
        MP_OBJ_NEW_SMALL_INT(h),
        self->block_view,
    };
    mp_call_function_n_kw(self->callback, 5, 0, args);
    return 1;
}

static void jpegio_decode_done(jpegio_jpegdecoder_obj_t *self) {
    if (self->block_view != MP_OBJ_NULL) {
        // A reference the callback kept must not read the workspace later.
        mp_obj_array_t *view = MP_OBJ_TO_PTR(self->block_view);
        view->items = NULL;
        view->len = 0;
    }
    self->callback = MP_OBJ_NULL;
    self->target = MP_OBJ_NULL;
    self->pixels = NULL;
    jpegio_close(self);
}

// --- JpegDecoder ------------------------------------------------------------

static mp_obj_t jpegio_jpegdecoder_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    (void)all_args;
    mp_arg_check_num(n_args, n_kw, 0, 0, false);
    jpegio_jpegdecoder_obj_t *self = mp_obj_malloc(jpegio_jpegdecoder_obj_t, type);
    memset(&self->decoder, 0, sizeof(self->decoder));
    self->source = MP_OBJ_NULL;
    memset(&self->bufinfo, 0, sizeof(self->bufinfo));
    self->owns_source = false;
    self->ready = false;
    self->width = 0;
    self->height = 0;
    self->callback = MP_OBJ_NULL;
    self->target = MP_OBJ_NULL;
    self->pixels = NULL;
    self->stride = 0;
    self->x = 0;
    self->y = 0;
    self->block_view = MP_OBJ_NULL;
    return MP_OBJ_FROM_PTR(self);
}

static void jpegio_jpegdecoder_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    jpegio_jpegdecoder_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->ready) {
        mp_printf(print, "<JpegDecoder %ux%u>", self->width, self->height);
    } else {
        mp_printf(print, "<JpegDecoder>");
    }
}

// open(source) -> (width, height). Positional-only, like CircuitPython.
static mp_obj_t jpegio_jpegdecoder_open(mp_obj_t self_in, mp_obj_t source) {
    jpegio_jpegdecoder_obj_t *self = MP_OBJ_TO_PTR(self_in);
    jpegio_close(self);
    // width / height track the last open(): a failed one reports nothing.
    self->width = 0;
    self->height = 0;

    bool owns = false;
    if (mp_obj_is_str(source)) {
        source = mp_call_function_2(MP_OBJ_FROM_PTR(&mp_builtin_open_obj), source, MP_OBJ_NEW_QSTR(MP_QSTR_rb));
        owns = true;
    }

    size_t (*infunc)(JDEC *, uint8_t *, size_t);
    if (mp_get_buffer(source, &self->bufinfo, MP_BUFFER_READ)) {
        infunc = jpegio_buffer_input;
    } else {
        // Not mp_get_stream(): that reads the type's protocol slot unguarded,
        // and a type without one (int, None, list, a Python class with a
        // read() method) yields a wild pointer -- a segfault, not a TypeError.
        const mp_stream_p_t *stream = MP_OBJ_TYPE_GET_SLOT_OR_NULL(mp_obj_get_type(source), protocol);
        if (stream != NULL && stream->read != NULL && !stream->is_text) {
            infunc = jpegio_stream_input;
        } else {
            mp_raise_TypeError(MP_ERROR_TEXT("source must be a str path, a bytes-like object, or a binary stream"));
        }
    }
    self->source = source;
    self->owns_source = owns;

    // Nothing is sniffed here: jd_prepare scans for SOI itself and then takes
    // the segments in whatever order the stream carries them (UVC MJPEG frames
    // are not JFIF-first). A stream error inside it is raised by the input
    // function; close the source on that path too.
    JRESULT rc = JDR_OK;
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        rc = jd_prepare(&self->decoder, infunc, self->workspace, sizeof(self->workspace), self);
        nlr_pop();
    } else {
        jpegio_close(self);
        nlr_jump(nlr.ret_val);
    }
    if (rc != JDR_OK) {
        jpegio_close(self);
        jpegio_raise_jresult(rc);
    }
    self->ready = true;
    self->width = self->decoder.width;
    self->height = self->decoder.height;

    mp_obj_t elems[] = {
        MP_OBJ_NEW_SMALL_INT(self->width),
        MP_OBJ_NEW_SMALL_INT(self->height),
    };
    return mp_obj_new_tuple(MP_ARRAY_SIZE(elems), elems);
}
static MP_DEFINE_CONST_FUN_OBJ_2(jpegio_jpegdecoder_open_obj, jpegio_jpegdecoder_open);

// decode(target, scale=0, x=0, y=0, *, stride=None) -> None
static mp_obj_t jpegio_jpegdecoder_decode(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    jpegio_jpegdecoder_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);

    enum { ARG_target, ARG_scale, ARG_x, ARG_y, ARG_stride };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_target, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_scale, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_x, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_y, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_stride, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (!self->ready) {
        mp_raise_msg_varg(&mp_type_RuntimeError, MP_ERROR_TEXT("%q() without %q()"), MP_QSTR_decode, MP_QSTR_open);
    }
    mp_int_t scale = args[ARG_scale].u_int;
    if (scale < 0 || scale > 3) {
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("%q must be 0..3, not %d"), MP_QSTR_scale, (int)scale);
    }
    mp_int_t x = args[ARG_x].u_int;
    mp_int_t y = args[ARG_y].u_int;
    if (x < 0 || x > JPEGIO_MAX_COORD || y < 0 || y > JPEGIO_MAX_COORD) {
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("x, y must be 0..%d, not (%ld, %ld)"), JPEGIO_MAX_COORD, (long)x, (long)y);
    }
    size_t dw = self->width >> scale;
    size_t dh = self->height >> scale;

    mp_obj_t target = args[ARG_target].u_obj;
    int (*outfunc)(JDEC *, void *, JRECT *);
    if (mp_obj_is_callable(target)) {
        if (args[ARG_stride].u_obj != mp_const_none) {
            mp_raise_TypeError(MP_ERROR_TEXT("stride applies to buffer targets only"));
        }
        if (self->block_view == MP_OBJ_NULL) {
            #if MICROPY_PY_BUILTINS_MEMORYVIEW
            self->block_view = mp_obj_new_memoryview('H', 0, NULL);
            #else
            self->block_view = mp_obj_new_bytearray_by_ref(0, NULL);
            #endif
        }
        self->callback = target;
        self->target = MP_OBJ_NULL;
        self->pixels = NULL;
        self->stride = 0;
        outfunc = jpegio_callback_output;
    } else {
        mp_buffer_info_t buf;
        mp_get_buffer_raise(target, &buf, MP_BUFFER_WRITE);
        bool stride_given = args[ARG_stride].u_obj != mp_const_none;
        size_t stride = dw;
        if (stride_given) {
            mp_int_t s = mp_obj_get_int(args[ARG_stride].u_obj);
            if (s < 0) {
                mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("%q must be >= 0, not %ld"), MP_QSTR_stride, (long)s);
            }
            stride = s;
        }
        if ((size_t)x + dw > stride) {
            if (stride_given) {
                mp_raise_msg_varg(&mp_type_ValueError,
                    MP_ERROR_TEXT("x + decoded width (%d + %d) exceeds stride %lu"),
                    (int)x, (int)dw, (unsigned long)stride);
            }
            // The default stride is the decoded width: x > 0 can never fit it.
            mp_raise_msg_varg(&mp_type_ValueError,
                MP_ERROR_TEXT("x + decoded width (%d + %d) exceeds the default stride %d (the decoded width): pass stride=<target row width in pixels> to place at x > 0"),
                (int)x, (int)dw, (int)stride);
        }
        // Buffer fit. The last pixel written is at index last_row * stride +
        // x + dw - 1, so the buffer needs last_row * stride + tail pixels.
        // x, y, dw, dh are all < 2^16, so the sums cannot overflow; the
        // product can (stride is any mp_int_t, and size_t is 32 bits on the
        // MCU ports), so it is compared by division instead of computed --
        // a wrapped product must not slip under buf.len.
        if (dw != 0 && dh != 0) {
            size_t npix = buf.len / sizeof(uint16_t);
            size_t tail = (size_t)x + dw;
            size_t last_row = (size_t)y + dh - 1;
            bool fits = tail <= npix && (last_row == 0 || stride <= (npix - tail) / last_row);
            if (!fits) {
                if (last_row == 0 || stride <= (SIZE_MAX / 2 - tail) / last_row) {
                    size_t need = (last_row * stride + tail) * sizeof(uint16_t);
                    mp_raise_msg_varg(&mp_type_ValueError,
                        MP_ERROR_TEXT("target too small: %dx%d at (%d, %d) with stride %lu needs %lu bytes, buffer has %lu"),
                        (int)dw, (int)dh, (int)x, (int)y, (unsigned long)stride, (unsigned long)need, (unsigned long)buf.len);
                }
                mp_raise_msg_varg(&mp_type_ValueError,
                    MP_ERROR_TEXT("target too small: %dx%d at (%d, %d) with stride %lu: the last pixel's offset overflows size_t; buffer has %lu bytes"),
                    (int)dw, (int)dh, (int)x, (int)y, (unsigned long)stride, (unsigned long)buf.len);
            }
        }
        self->callback = MP_OBJ_NULL;
        self->target = target;
        self->pixels = buf.buf;
        self->stride = stride;
        outfunc = jpegio_buffer_output;
    }
    self->x = x;
    self->y = y;

    // One-shot, like CircuitPython: the stream is consumed, so open() again
    // before the next decode().
    self->ready = false;
    JRESULT rc = JDR_OK;
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        rc = jd_decomp(&self->decoder, outfunc, (uint8_t)scale);
        nlr_pop();
    } else {
        jpegio_decode_done(self);
        nlr_jump(nlr.ret_val);
    }
    jpegio_decode_done(self);
    if (rc != JDR_OK && rc != JDR_INTR) {
        jpegio_raise_jresult(rc);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(jpegio_jpegdecoder_decode_obj, 1, jpegio_jpegdecoder_decode);

// width / height: read-only; track the last open() (valid after a successful
// one, through its decode(), until the next open(); 0 after a failed one).
static void jpegio_jpegdecoder_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    if (dest[0] != MP_OBJ_NULL) {
        return; // store / delete: not supported (leaves dest[0] set -> AttributeError)
    }
    if (attr == MP_QSTR_width || attr == MP_QSTR_height) {
        jpegio_jpegdecoder_obj_t *self = MP_OBJ_TO_PTR(self_in);
        if (self->width == 0 && self->height == 0) {
            mp_raise_msg_varg(&mp_type_RuntimeError, MP_ERROR_TEXT("%q needs a successful %q()"), attr, MP_QSTR_open);
        }
        dest[0] = MP_OBJ_NEW_SMALL_INT(attr == MP_QSTR_width ? self->width : self->height);
        return;
    }
    dest[1] = MP_OBJ_SENTINEL; // continue the lookup in locals_dict (open, decode)
}

static const mp_rom_map_elem_t jpegio_jpegdecoder_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_open), MP_ROM_PTR(&jpegio_jpegdecoder_open_obj) },
    { MP_ROM_QSTR(MP_QSTR_decode), MP_ROM_PTR(&jpegio_jpegdecoder_decode_obj) },
};
static MP_DEFINE_CONST_DICT(jpegio_jpegdecoder_locals_dict, jpegio_jpegdecoder_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    jpegio_jpegdecoder_type,
    MP_QSTR_JpegDecoder,
    MP_TYPE_FLAG_NONE,
    make_new, jpegio_jpegdecoder_make_new,
    print, jpegio_jpegdecoder_print,
    attr, jpegio_jpegdecoder_attr,
    locals_dict, &jpegio_jpegdecoder_locals_dict
    );

// --- LVGL image decoder (beside the lvgl-micropython usermod only) ----------

#if JPEGIO_LVGL_DECODER
// register_lvgl_decoder() -> bool: register jpegio's decoder with the running
// LVGL. True when this call added it, False when it was already registered
// (a second registration never adds a second decoder). RuntimeError when
// LVGL is not initialised: call lv.init() first. Needed after `import jpegio`
// ran before lv.init(), and after every lv.deinit() / lv.init() cycle (LVGL
// clears its decoder list on deinit).
static mp_obj_t jpegio_register_lvgl_decoder(void) {
    if (jpegio_lvgl_decoder_registered()) {
        return mp_const_false;
    }
    if (!jpegio_lvgl_decoder_register()) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("register_lvgl_decoder() needs an initialised LVGL: lv.init() first"));
    }
    return mp_const_true;
}
static MP_DEFINE_CONST_FUN_OBJ_0(jpegio_register_lvgl_decoder_obj, jpegio_register_lvgl_decoder);

// lvgl_decoders() -> tuple of str: the names of LVGL's registered image
// decoders in the order LVGL consults them (`("jpegio", "LODEPNG", "BIN")`
// once registered); empty before lv.init(). A diagnostic: MicroPython's
// builtin-method self check keeps `lv.image_decoder_t.get_next(None)` from
// walking the list in Python, and this is how the test proves a second
// registration added nothing.
static mp_obj_t jpegio_lvgl_decoders(void) {
    size_t n = 0;
    while (jpegio_lvgl_decoder_name_at(n) != NULL) {
        n++;
    }
    mp_obj_tuple_t *names = MP_OBJ_TO_PTR(mp_obj_new_tuple(n, NULL));
    for (size_t i = 0; i < n; i++) {
        const char *name = jpegio_lvgl_decoder_name_at(i);
        names->items[i] = mp_obj_new_str(name, strlen(name));
    }
    return MP_OBJ_FROM_PTR(names);
}
static MP_DEFINE_CONST_FUN_OBJ_0(jpegio_lvgl_decoders_obj, jpegio_lvgl_decoders);

// Module __init__: MicroPython calls it on `import jpegio` (every import
// statement, on ports with MICROPY_MODULE_BUILTIN_INIT). If LVGL is already
// initialised, register now, so `import lvgl; lv.init(); import jpegio`
// needs nothing more. Idempotent, and silent when LVGL is not up yet.
static mp_obj_t jpegio___init__(void) {
    jpegio_lvgl_decoder_register();     // no-op (false) until lv.init() has run
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(jpegio___init___obj, jpegio___init__);
#endif

// --- module -----------------------------------------------------------------

static const mp_rom_map_elem_t jpegio_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_jpegio) },
    { MP_ROM_QSTR(MP_QSTR_JpegDecoder), MP_ROM_PTR(&jpegio_jpegdecoder_type) },
    #if JPEGIO_LVGL_DECODER
    { MP_ROM_QSTR(MP_QSTR___init__), MP_ROM_PTR(&jpegio___init___obj) },
    { MP_ROM_QSTR(MP_QSTR_register_lvgl_decoder), MP_ROM_PTR(&jpegio_register_lvgl_decoder_obj) },
    { MP_ROM_QSTR(MP_QSTR_lvgl_decoders), MP_ROM_PTR(&jpegio_lvgl_decoders_obj) },
    #endif
};
static MP_DEFINE_CONST_DICT(jpegio_module_globals, jpegio_module_globals_table);

const mp_obj_module_t jpegio_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&jpegio_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_jpegio, jpegio_module);
