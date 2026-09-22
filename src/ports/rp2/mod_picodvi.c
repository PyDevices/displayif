// SPDX-License-Identifier: MIT
// picodvi module — RP2040 PIO (libdvi) and RP2350 HSTX backends.

#include "py/runtime.h"
#include "displayif/build.h"
#include "py/obj.h"
#include "machine_pin.h"

#include "displayif/fb_fill.h"

#if defined(PICO_RP2040) && !defined(PICO_RP2350)
#include "picodvi_rp2040.h"
#elif defined(PICO_RP2350)
#include "picodvi_rp2350.h"
#endif

static const mp_obj_type_t picodvi_framebuffer_type;

static int picodvi_pin_num(mp_obj_t pin_obj) {
    const machine_pin_obj_t *pin = machine_pin_find(pin_obj);
    return pin->id;
}

static mp_obj_t picodvi_framebuffer_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    enum {
        ARG_width,
        ARG_height,
        ARG_clk_dp,
        ARG_clk_dn,
        ARG_red_dp,
        ARG_red_dn,
        ARG_green_dp,
        ARG_green_dn,
        ARG_blue_dp,
        ARG_blue_dn,
        ARG_color_depth,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_width, MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_clk_dp, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_clk_dn, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_red_dp, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_red_dn, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_green_dp, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_green_dn, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_blue_dp, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_blue_dn, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_color_depth, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 8 } },
    };
    mp_arg_val_t vals[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, args, MP_ARRAY_SIZE(allowed_args), allowed_args, vals);

    picodvi_framebuffer_obj_t *self = mp_obj_malloc(picodvi_framebuffer_obj_t, type);

#if defined(PICO_RP2040) && !defined(PICO_RP2350)
    picodvi_rp2040_construct(self,
        vals[ARG_width].u_int, vals[ARG_height].u_int,
        picodvi_pin_num(vals[ARG_clk_dp].u_obj), picodvi_pin_num(vals[ARG_clk_dn].u_obj),
        picodvi_pin_num(vals[ARG_red_dp].u_obj), picodvi_pin_num(vals[ARG_red_dn].u_obj),
        picodvi_pin_num(vals[ARG_green_dp].u_obj), picodvi_pin_num(vals[ARG_green_dn].u_obj),
        picodvi_pin_num(vals[ARG_blue_dp].u_obj), picodvi_pin_num(vals[ARG_blue_dn].u_obj),
        vals[ARG_color_depth].u_int);
#elif defined(PICO_RP2350)
    picodvi_rp2350_construct(self,
        vals[ARG_width].u_int, vals[ARG_height].u_int,
        picodvi_pin_num(vals[ARG_clk_dp].u_obj), picodvi_pin_num(vals[ARG_clk_dn].u_obj),
        picodvi_pin_num(vals[ARG_red_dp].u_obj), picodvi_pin_num(vals[ARG_red_dn].u_obj),
        picodvi_pin_num(vals[ARG_green_dp].u_obj), picodvi_pin_num(vals[ARG_green_dn].u_obj),
        picodvi_pin_num(vals[ARG_blue_dp].u_obj), picodvi_pin_num(vals[ARG_blue_dn].u_obj),
        vals[ARG_color_depth].u_int);
#else
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("picodvi not supported on this MCU"));
#endif

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t picodvi_framebuffer_refresh(mp_obj_t self_in) {
    (void)self_in;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(picodvi_framebuffer_refresh_obj, picodvi_framebuffer_refresh);

static void picodvi_framebuffer_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    picodvi_framebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_NULL) {
        if (attr == MP_QSTR_width) {
            dest[0] = mp_obj_new_int(self->width);
        } else if (attr == MP_QSTR_height) {
            dest[0] = mp_obj_new_int(self->height);
        } else if (attr == MP_QSTR_row_stride) {
#if defined(PICO_RP2040) && !defined(PICO_RP2350)
            dest[0] = mp_obj_new_int(self->pitch * sizeof(uint32_t));
#elif defined(PICO_RP2350)
            dest[0] = mp_obj_new_int(self->pitch * sizeof(uint32_t));
#endif
        }
    }
}

static mp_int_t picodvi_framebuffer_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    (void)flags;
    picodvi_framebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    bufinfo->buf = self->framebuffer;
    bufinfo->typecode = (self->color_depth == 16) ? 'H' : 'B';
    bufinfo->len = self->framebuffer_len * sizeof(uint32_t);
    return 0;
}

/* fill_rect(x, y, w, h, color) -- a C fill, at whatever colour depth this
 * framebuffer was opened with (displayif#34).
 *
 * The other five backends here are fixed at 16bpp and share
 * `displayif_fb_fill_rect16`. This one is not: `color_depth` is 1, 2, 4, 8,
 * 16 or 32, so it goes through the packed fill, which does the partial-byte
 * masking at the two edges that a sub-byte depth needs and none of the others
 * has to think about.
 *
 * Without it, `displaydev.FBDisplay.fill_rect` falls off the end of its
 * dispatch ladder onto per-row `memoryview` assigns from Python. On the
 * ESP32-P4, where the same fallback was measured on hardware, a 400x240 fill
 * costs 2.46 s that way against 3.5 ms through the native fill -- a factor of
 * 698 (displayif#28). Nobody has measured it on an rp2, but the ladder is the
 * same one, and a DVI app that clears a region reads as a hung program rather
 * than a slow one.
 *
 * Bounds through the shared `displayif_fb_rect_ok`, so this refuses exactly
 * the rectangles the other five refuse.
 */
static mp_obj_t picodvi_framebuffer_fill_rect(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    picodvi_framebuffer_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    const int x = mp_obj_get_int(args[1]);
    const int y = mp_obj_get_int(args[2]);
    const int w = mp_obj_get_int(args[3]);
    const int h = mp_obj_get_int(args[4]);
    const uint32_t color = (uint32_t)mp_obj_get_int_truncated(args[5]);
    // Not decoration: picodvi_rp2350_deinit() m_free()s the framebuffer and
    // sets it NULL, so a fill after deinit would write through freed memory.
    // RuntimeError, and the wording, to match rgbmatrix's guard beside it.
    if (self->framebuffer == NULL) {
        mp_raise_msg(&mp_type_RuntimeError,
            MP_ERROR_TEXT("picodvi.Framebuffer is deinited"));
    }
    if (!displayif_fb_rect_ok(x, y, w, h, self->width, self->height)) {
        mp_raise_ValueError(MP_ERROR_TEXT("fill_rect out of range"));
    }
    displayif_fb_fill_rect_packed((uint8_t *)self->framebuffer,
        (size_t)self->pitch * sizeof(uint32_t), x, y, w, h, color,
        (int)self->color_depth);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(picodvi_framebuffer_fill_rect_obj, 6, 6,
    picodvi_framebuffer_fill_rect);

static mp_obj_t picodvi_framebuffer_del(mp_obj_t self_in) {
    picodvi_framebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
#if defined(PICO_RP2040) && !defined(PICO_RP2350)
    picodvi_rp2040_deinit(self);
#elif defined(PICO_RP2350)
    picodvi_rp2350_deinit(self);
#endif
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(picodvi_framebuffer_del_obj, picodvi_framebuffer_del);

static const mp_rom_map_elem_t picodvi_framebuffer_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_refresh), MP_ROM_PTR(&picodvi_framebuffer_refresh_obj) },
    { MP_ROM_QSTR(MP_QSTR_fill_rect), MP_ROM_PTR(&picodvi_framebuffer_fill_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&picodvi_framebuffer_del_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&picodvi_framebuffer_del_obj) },
};
static MP_DEFINE_CONST_DICT(picodvi_framebuffer_locals_dict, picodvi_framebuffer_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    picodvi_framebuffer_type,
    MP_QSTR_Framebuffer,
    MP_TYPE_FLAG_NONE,
    make_new, picodvi_framebuffer_make,
    attr, picodvi_framebuffer_attr,
    locals_dict, &picodvi_framebuffer_locals_dict,
    buffer, picodvi_framebuffer_get_buffer
);

static const mp_rom_map_elem_t picodvi_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_picodvi) },
    DISPLAYIF_REVISION_ENTRY,
    { MP_ROM_QSTR(MP_QSTR_Framebuffer), MP_ROM_PTR(&picodvi_framebuffer_type) },
};
static MP_DEFINE_CONST_DICT(picodvi_module_globals, picodvi_module_globals_table);

const mp_obj_module_t picodvi_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&picodvi_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_picodvi, picodvi_user_cmodule);
