// SPDX-License-Identifier: MIT
// usdl2 native module (MicroPython + CircuitPython unix/windows).

#include "py/obj.h"
#include "py/runtime.h"
#include "py/binary.h"
#include "py/mphal.h"
#include "usdl2.h"
#include "displayif/soft_reset.h"

#include <stdlib.h>
#include <string.h>

/* Lifecycle: idempotent SDL_Quit + timer free via soft-reset registry
 * (see displayif/docs/idempotent-lifecycle.md). BSS survives soft reset. */
static bool s_soft_reset_registered;
static bool s_sdl_inited;

static void usdl2_host_teardown(void);
static void usdl2_ensure_soft_reset_registered(void);

// --- SDL_DEFINE_PIXELFORMAT --------------------------------------

static mp_obj_t usdl2_define_pixelformat(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    mp_int_t type = mp_obj_get_int(args[0]);
    mp_int_t order = mp_obj_get_int(args[1]);
    mp_int_t layout = mp_obj_get_int(args[2]);
    mp_int_t bits = mp_obj_get_int(args[3]);
    mp_int_t bytes = mp_obj_get_int(args[4]);
    return mp_obj_new_int(USDL2_DEFINE_PIXELFORMAT(type, order, layout, bits, bytes));
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(usdl2_SDL_DEFINE_PIXELFORMAT_fun_obj, 5, 5, usdl2_define_pixelformat);

// --- Event type --------------------------------------------------

_Static_assert(sizeof(SDL_Event) == USDL2_EVENT_SIZE,
    "usdl2: SDL_Event size changed; update USDL2_EVENT_SIZE and event layout");

typedef struct {
    mp_obj_base_t base;
    uint8_t data[USDL2_EVENT_SIZE];
} usdl2_event_obj_t;

typedef struct {
    mp_obj_base_t base;
    mp_obj_t parent;
    uint16_t offset;
} usdl2_subview_obj_t;

static const mp_obj_type_t usdl2_subview_type;

const mp_obj_type_t usdl2_event_type;

static uint8_t *usdl2_parent_data(mp_obj_t parent) {
    usdl2_event_obj_t *self = MP_OBJ_TO_PTR(parent);
    return self->data;
}

static uint32_t usdl2_read_u32(const uint8_t *data, uint16_t offset) {
    uint32_t value;
    memcpy(&value, data + offset, sizeof(value));
    return value;
}

static int32_t usdl2_read_i32(const uint8_t *data, uint16_t offset) {
    int32_t value;
    memcpy(&value, data + offset, sizeof(value));
    return value;
}

static int16_t usdl2_read_i16(const uint8_t *data, uint16_t offset) {
    int16_t value;
    memcpy(&value, data + offset, sizeof(value));
    return value;
}

static bool usdl2_is_joystick_event(uint32_t type) {
    return type >= SDL_JOYAXISMOTION && type <= SDL_JOYDEVICEREMOVED;
}

static bool usdl2_is_finger_event(uint32_t type) {
    return type == SDL_FINGERDOWN || type == SDL_FINGERUP || type == SDL_FINGERMOTION;
}

static int64_t usdl2_read_i64(const uint8_t *data, uint16_t offset) {
    int64_t value;
    memcpy(&value, data + offset, sizeof(value));
    return value;
}

static float usdl2_read_f32(const uint8_t *data, uint16_t offset) {
    float value;
    memcpy(&value, data + offset, sizeof(value));
    return value;
}

static mp_obj_t usdl2_subview_make(mp_obj_t parent, uint16_t offset) {
    usdl2_subview_obj_t *view = mp_obj_malloc(usdl2_subview_obj_t, &usdl2_subview_type);
    view->parent = parent;
    view->offset = offset;
    return MP_OBJ_FROM_PTR(view);
}

static void usdl2_subview_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    usdl2_subview_obj_t *self = MP_OBJ_TO_PTR(self_in);
    const uint8_t *data = usdl2_parent_data(self->parent);
    const uint16_t base = self->offset;

    if (dest[0] != MP_OBJ_NULL) {
        return;
    }

    switch (attr) {
        case MP_QSTR_windowID:
            // SDL_TouchFingerEvent.windowID follows pressure (union+36).
            if (usdl2_is_finger_event(usdl2_read_u32(data, 0))) {
                dest[0] = mp_obj_new_int_from_uint(usdl2_read_u32(data, base + 36));
            } else {
                dest[0] = mp_obj_new_int_from_uint(usdl2_read_u32(data, base + 0));
            }
            return;
        case MP_QSTR_which:
            if (usdl2_is_joystick_event(usdl2_read_u32(data, 0))) {
                dest[0] = mp_obj_new_int(usdl2_read_i32(data, base + 0));
            } else {
                dest[0] = mp_obj_new_int_from_uint(usdl2_read_u32(data, base + 4));
            }
            return;
        case MP_QSTR_touchId:
            dest[0] = mp_obj_new_int_from_ll(usdl2_read_i64(data, base + 0));
            return;
        case MP_QSTR_fingerId:
            dest[0] = mp_obj_new_int_from_ll(usdl2_read_i64(data, base + 8));
            return;
        case MP_QSTR_state: {
            uint32_t type = usdl2_read_u32(data, 0);
            if (base == 8 && type == SDL_MOUSEMOTION) {
                dest[0] = mp_obj_new_int_from_uint(usdl2_read_u32(data, base + 8));
            } else if (type == SDL_JOYBUTTONDOWN || type == SDL_JOYBUTTONUP) {
                dest[0] = mp_obj_new_int(data[base + 5]);
            } else {
                dest[0] = mp_obj_new_int_from_uint(data[base + 4]);
            }
            return;
        }
        case MP_QSTR_x:
            // SDL_TouchFingerEvent.x/y are normalized floats at union+16/+20.
            if (usdl2_is_finger_event(usdl2_read_u32(data, 0))) {
                dest[0] = mp_obj_new_float(usdl2_read_f32(data, base + 16));
            } else {
                dest[0] = mp_obj_new_int(usdl2_read_i32(data, base + 12));
            }
            return;
        case MP_QSTR_y:
            if (usdl2_is_finger_event(usdl2_read_u32(data, 0))) {
                dest[0] = mp_obj_new_float(usdl2_read_f32(data, base + 20));
            } else {
                dest[0] = mp_obj_new_int(usdl2_read_i32(data, base + 16));
            }
            return;
        case MP_QSTR_dx:
            dest[0] = mp_obj_new_float(usdl2_read_f32(data, base + 24));
            return;
        case MP_QSTR_dy:
            dest[0] = mp_obj_new_float(usdl2_read_f32(data, base + 28));
            return;
        case MP_QSTR_pressure:
            dest[0] = mp_obj_new_float(usdl2_read_f32(data, base + 32));
            return;
        case MP_QSTR_xrel: {
            uint32_t type = usdl2_read_u32(data, 0);
            if (type == SDL_JOYBALLMOTION) {
                dest[0] = mp_obj_new_int(usdl2_read_i16(data, base + 8));
            } else {
                dest[0] = mp_obj_new_int(usdl2_read_i32(data, base + 20));
            }
            return;
        }
        case MP_QSTR_yrel: {
            uint32_t type = usdl2_read_u32(data, 0);
            if (type == SDL_JOYBALLMOTION) {
                dest[0] = mp_obj_new_int(usdl2_read_i16(data, base + 10));
            } else {
                dest[0] = mp_obj_new_int(usdl2_read_i32(data, base + 24));
            }
            return;
        }
        case MP_QSTR_button: {
            uint32_t type = usdl2_read_u32(data, 0);
            if (type == SDL_JOYBUTTONDOWN || type == SDL_JOYBUTTONUP) {
                dest[0] = mp_obj_new_int(data[base + 4]);
            } else {
                dest[0] = mp_obj_new_int(data[base + 8]);
            }
            return;
        }
        case MP_QSTR_clicks:
            dest[0] = mp_obj_new_int(data[base + 10]);
            return;
        case MP_QSTR_direction:
            dest[0] = mp_obj_new_int_from_uint(usdl2_read_u32(data, base + 16));
            return;
        case MP_QSTR_preciseX: {
            float value;
            memcpy(&value, data + base + 20, sizeof(value));
            dest[0] = mp_obj_new_float(value);
            return;
        }
        case MP_QSTR_preciseY: {
            float value;
            memcpy(&value, data + base + 24, sizeof(value));
            dest[0] = mp_obj_new_float(value);
            return;
        }
        case MP_QSTR_repeat:
            dest[0] = mp_obj_new_int(data[base + 5]);
            return;
        case MP_QSTR_keysym:
            dest[0] = usdl2_subview_make(self->parent, base + 8);
            return;
        case MP_QSTR_scancode:
            dest[0] = mp_obj_new_int(usdl2_read_i32(data, base + 0));
            return;
        case MP_QSTR_sym:
            dest[0] = mp_obj_new_int(usdl2_read_i32(data, base + 4));
            return;
        case MP_QSTR_mod:
            dest[0] = mp_obj_new_int_from_uint(usdl2_read_u32(data, base + 8) & 0xffff);
            return;
        case MP_QSTR_axis:
            dest[0] = mp_obj_new_int(data[base + 4]);
            return;
        case MP_QSTR_value: {
            uint32_t type = usdl2_read_u32(data, 0);
            if (type == SDL_JOYAXISMOTION) {
                dest[0] = mp_obj_new_int(usdl2_read_i16(data, base + 8));
            } else if (type == SDL_JOYHATMOTION) {
                dest[0] = mp_obj_new_int(data[base + 5]);
            } else {
                dest[0] = MP_OBJ_NULL;
            }
            return;
        }
        case MP_QSTR_ball:
            dest[0] = mp_obj_new_int(data[base + 4]);
            return;
        case MP_QSTR_hat:
            dest[0] = mp_obj_new_int(data[base + 4]);
            return;
        case MP_QSTR_event:
            // SDL_WindowEvent.event (Uint8) at union+4.
            dest[0] = mp_obj_new_int(data[base + 4]);
            return;
        default:
            dest[0] = MP_OBJ_NULL;
            return;
    }
}

static MP_DEFINE_CONST_OBJ_TYPE(
    usdl2_subview_type,
    MP_QSTR_usdl2_subview,
    MP_TYPE_FLAG_NONE,
    attr, usdl2_subview_attr
    );

uint8_t *usdl2_event_buffer(mp_obj_t self_in) {
    usdl2_event_obj_t *self = MP_OBJ_TO_PTR(self_in);
    return self->data;
}

mp_int_t usdl2_event_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    (void)flags;
    usdl2_event_obj_t *self = MP_OBJ_TO_PTR(self_in);
    bufinfo->buf = self->data;
    bufinfo->len = USDL2_EVENT_SIZE;
    bufinfo->typecode = 'B';
    return 0;
}

mp_obj_t usdl2_event_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 1, false);
    // SDL_Event(None) or SDL_Event() -> new zero-filled event.
    // SDL_Event(<existing SDL_Event>) -> the same instance (identity), matching
    // the pre-consolidation SDL_Event() helper's pass-through behavior.
    // SDL_Event(<bytes-like>) -> a new event copied from the buffer.
    if (n_args == 1 && args[0] != mp_const_none) {
        if (mp_obj_is_type(args[0], type)) {
            return args[0];
        }
        usdl2_event_obj_t *self = mp_obj_malloc(usdl2_event_obj_t, type);
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[0], &bufinfo, MP_BUFFER_READ);
        if (bufinfo.len < USDL2_EVENT_SIZE) {
            mp_raise_ValueError(MP_ERROR_TEXT("event buffer too small"));
        }
        memcpy(self->data, bufinfo.buf, USDL2_EVENT_SIZE);
        return MP_OBJ_FROM_PTR(self);
    }
    usdl2_event_obj_t *self = mp_obj_malloc(usdl2_event_obj_t, type);
    memset(self->data, 0, USDL2_EVENT_SIZE);
    return MP_OBJ_FROM_PTR(self);
}

static void usdl2_event_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    usdl2_event_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (dest[0] != MP_OBJ_NULL) {
        return;
    }

    switch (attr) {
        case MP_QSTR_type:
            dest[0] = mp_obj_new_int_from_uint(usdl2_read_u32(self->data, 0));
            return;
        case MP_QSTR_timestamp:
            dest[0] = mp_obj_new_int_from_uint(usdl2_read_u32(self->data, 4));
            return;
        case MP_QSTR_motion:
        case MP_QSTR_key:
        case MP_QSTR_button:
        case MP_QSTR_wheel:
        case MP_QSTR_jaxis:
        case MP_QSTR_jball:
        case MP_QSTR_jhat:
        case MP_QSTR_jbutton:
        case MP_QSTR_tfinger:
        case MP_QSTR_window:
            dest[0] = usdl2_subview_make(self_in, 8);
            return;
        default:
            dest[0] = MP_OBJ_NULL;
            return;
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    usdl2_event_type,
    MP_QSTR_SDL_Event,
    MP_TYPE_FLAG_NONE,
    make_new, usdl2_event_make_new,
    attr, usdl2_event_attr,
    buffer, usdl2_event_get_buffer
    );

// --- SDL bindings ------------------------------------------------

void *usdl2_ptr_from_obj(mp_obj_t obj) {
    return (void *)(uintptr_t)mp_obj_get_int_truncated(obj);
}

mp_obj_t usdl2_ptr_obj(void *ptr) {
    return mp_obj_new_int_from_uint((uintptr_t)ptr);
}

bool usdl2_obj_is_none_or_null(mp_obj_t obj) {
    if (obj == mp_const_none) {
        return true;
    }
    if (mp_obj_is_int(obj)) {
        return mp_obj_get_int_truncated(obj) == 0;
    }
    return false;
}

void usdl2_rect_from_bytes(mp_obj_t obj, SDL_Rect *rect) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(obj, &bufinfo, MP_BUFFER_READ);
    if (bufinfo.len < (mp_uint_t)sizeof(SDL_Rect)) {
        mp_raise_ValueError(MP_ERROR_TEXT("rect buffer too small"));
    }
    const int32_t *values = (const int32_t *)bufinfo.buf;
    rect->x = values[0];
    rect->y = values[1];
    rect->w = values[2];
    rect->h = values[3];
}

void usdl2_rect_to_bytes(const SDL_Rect *rect, mp_obj_t obj) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(obj, &bufinfo, MP_BUFFER_WRITE);
    if (bufinfo.len < sizeof(SDL_Rect)) {
        mp_raise_ValueError(MP_ERROR_TEXT("rect buffer too small"));
    }
    int32_t *values = (int32_t *)bufinfo.buf;
    values[0] = rect->x;
    values[1] = rect->y;
    values[2] = rect->w;
    values[3] = rect->h;
}

void usdl2_display_mode_to_bytes(const SDL_DisplayMode *mode, mp_obj_t obj) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(obj, &bufinfo, MP_BUFFER_WRITE);
    if (bufinfo.len < (mp_uint_t)sizeof(SDL_DisplayMode)) {
        mp_raise_ValueError(MP_ERROR_TEXT("display mode buffer too small"));
    }
    uint32_t *u32 = (uint32_t *)bufinfo.buf;
    int32_t *i32 = (int32_t *)bufinfo.buf;
    u32[0] = mode->format;
    i32[1] = mode->w;
    i32[2] = mode->h;
    i32[3] = mode->refresh_rate;
}

SDL_Rect *usdl2_optional_rect(mp_obj_t obj, SDL_Rect *storage) {
    if (usdl2_obj_is_none_or_null(obj)) {
        return NULL;
    }
    usdl2_rect_from_bytes(obj, storage);
    return storage;
}

mp_obj_t usdl2_init(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    usdl2_ensure_soft_reset_registered();
    int rc = SDL_Init((Uint32)mp_obj_get_int(args[0]));
    if (rc == 0) {
        s_sdl_inited = true;
    }
    return mp_obj_new_int(rc);
}

mp_obj_t usdl2_init_subsystem(mp_obj_t flags_in) {
    usdl2_ensure_soft_reset_registered();
    int rc = SDL_InitSubSystem((Uint32)mp_obj_get_int(flags_in));
    if (rc == 0) {
        s_sdl_inited = true;
    }
    return mp_obj_new_int(rc);
}

mp_obj_t usdl2_quit(void) {
    usdl2_host_teardown();
    return mp_const_none;
}

mp_obj_t usdl2_get_error(void) {
    const char *err = SDL_GetError();
    if (err == NULL || err[0] == '\0') {
        return mp_obj_new_str("", 0);
    }
    return mp_obj_new_str(err, strlen(err));
}

mp_obj_t usdl2_set_hint(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    const char *name = mp_obj_str_get_str(args[0]);
    const char *value = mp_obj_str_get_str(args[1]);
    return mp_obj_new_int(SDL_SetHint(name, value) ? 1 : 0);
}

mp_obj_t usdl2_window_title(mp_obj_t title_in) {
    if (mp_obj_is_str(title_in)) {
        return title_in;
    }
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(title_in, &bufinfo, MP_BUFFER_READ);
    return mp_obj_new_str(bufinfo.buf, bufinfo.len);
}

mp_obj_t usdl2_create_window(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    usdl2_ensure_soft_reset_registered();
    const char *title = mp_obj_str_get_str(usdl2_window_title(args[0]));
    SDL_Window *window = SDL_CreateWindow(
        title,
        (int)mp_obj_get_int(args[1]),
        (int)mp_obj_get_int(args[2]),
        (int)mp_obj_get_int(args[3]),
        (int)mp_obj_get_int(args[4]),
        (Uint32)mp_obj_get_int(args[5]));
    if (window != NULL) {
        s_sdl_inited = true;
    }
    return usdl2_ptr_obj(window);
}

mp_obj_t usdl2_destroy_window(mp_obj_t win_in) {
    SDL_DestroyWindow((SDL_Window *)usdl2_ptr_from_obj(win_in));
    return mp_const_none;
}

mp_obj_t usdl2_get_window_id(mp_obj_t win_in) {
    return mp_obj_new_int_from_uint(
        SDL_GetWindowID((SDL_Window *)usdl2_ptr_from_obj(win_in)));
}

mp_obj_t usdl2_set_window_size(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    SDL_SetWindowSize(
        (SDL_Window *)usdl2_ptr_from_obj(args[0]),
        (int)mp_obj_get_int(args[1]),
        (int)mp_obj_get_int(args[2]));
    return mp_const_none;
}

mp_obj_t usdl2_set_window_resizable(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    SDL_SetWindowResizable(
        (SDL_Window *)usdl2_ptr_from_obj(args[0]),
        mp_obj_is_true(args[1]) ? SDL_TRUE : SDL_FALSE);
    return mp_const_none;
}

mp_obj_t usdl2_set_window_minimum_size(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    SDL_SetWindowMinimumSize(
        (SDL_Window *)usdl2_ptr_from_obj(args[0]),
        (int)mp_obj_get_int(args[1]),
        (int)mp_obj_get_int(args[2]));
    return mp_const_none;
}

mp_obj_t usdl2_set_window_maximum_size(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    SDL_SetWindowMaximumSize(
        (SDL_Window *)usdl2_ptr_from_obj(args[0]),
        (int)mp_obj_get_int(args[1]),
        (int)mp_obj_get_int(args[2]));
    return mp_const_none;
}

mp_obj_t usdl2_create_renderer(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    SDL_Renderer *renderer = SDL_CreateRenderer(
        (SDL_Window *)usdl2_ptr_from_obj(args[0]),
        (int)mp_obj_get_int(args[1]),
        (Uint32)mp_obj_get_int(args[2]));
    return usdl2_ptr_obj(renderer);
}

mp_obj_t usdl2_destroy_renderer(mp_obj_t renderer_in) {
    SDL_DestroyRenderer((SDL_Renderer *)usdl2_ptr_from_obj(renderer_in));
    return mp_const_none;
}

mp_obj_t usdl2_set_render_draw_color(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    int rc = SDL_SetRenderDrawColor(
        (SDL_Renderer *)usdl2_ptr_from_obj(args[0]),
        (Uint8)mp_obj_get_int(args[1]),
        (Uint8)mp_obj_get_int(args[2]),
        (Uint8)mp_obj_get_int(args[3]),
        (Uint8)mp_obj_get_int(args[4]));
    return mp_obj_new_int(rc);
}

mp_obj_t usdl2_set_render_target(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    SDL_Texture *texture = NULL;
    if (!usdl2_obj_is_none_or_null(args[1])) {
        texture = (SDL_Texture *)usdl2_ptr_from_obj(args[1]);
    }
    int rc = SDL_SetRenderTarget((SDL_Renderer *)usdl2_ptr_from_obj(args[0]), texture);
    return mp_obj_new_int(rc);
}

mp_obj_t usdl2_render_clear(mp_obj_t renderer_in) {
    SDL_RenderClear((SDL_Renderer *)usdl2_ptr_from_obj(renderer_in));
    return mp_const_none;
}

mp_obj_t usdl2_render_copy(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    SDL_Rect src_storage;
    SDL_Rect dst_storage;
    SDL_Rect *src = usdl2_optional_rect(args[2], &src_storage);
    SDL_Rect *dst = usdl2_optional_rect(args[3], &dst_storage);
    SDL_RenderCopy(
        (SDL_Renderer *)usdl2_ptr_from_obj(args[0]),
        (SDL_Texture *)usdl2_ptr_from_obj(args[1]),
        src,
        dst);
    return mp_const_none;
}

mp_obj_t usdl2_render_copy_ex(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    SDL_Rect src_storage;
    SDL_Rect dst_storage;
    SDL_Rect *src = usdl2_optional_rect(args[2], &src_storage);
    SDL_Rect *dst = usdl2_optional_rect(args[3], &dst_storage);
    SDL_Point center_storage;
    SDL_Point *center = NULL;
    if (!usdl2_obj_is_none_or_null(args[5])) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[5], &bufinfo, MP_BUFFER_READ);
        if (bufinfo.len < (mp_uint_t)sizeof(int32_t) * 2) {
            mp_raise_ValueError(MP_ERROR_TEXT("center buffer too small"));
        }
        const int32_t *values = (const int32_t *)bufinfo.buf;
        center_storage.x = values[0];
        center_storage.y = values[1];
        center = &center_storage;
    }
    SDL_RenderCopyEx(
        (SDL_Renderer *)usdl2_ptr_from_obj(args[0]),
        (SDL_Texture *)usdl2_ptr_from_obj(args[1]),
        src,
        dst,
        mp_obj_get_float(args[4]),
        center,
        (int)mp_obj_get_int(args[6]));
    return mp_const_none;
}

mp_obj_t usdl2_render_present(mp_obj_t renderer_in) {
    SDL_RenderPresent((SDL_Renderer *)usdl2_ptr_from_obj(renderer_in));
    return mp_const_none;
}

mp_obj_t usdl2_render_fill_rect(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    SDL_Rect rect;
    usdl2_rect_from_bytes(args[1], &rect);
    int rc = SDL_RenderFillRect((SDL_Renderer *)usdl2_ptr_from_obj(args[0]), &rect);
    return mp_obj_new_int(rc);
}

mp_obj_t usdl2_render_set_logical_size(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    int rc = SDL_RenderSetLogicalSize(
        (SDL_Renderer *)usdl2_ptr_from_obj(args[0]),
        (int)mp_obj_get_int(args[1]),
        (int)mp_obj_get_int(args[2]));
    return mp_obj_new_int(rc);
}

mp_obj_t usdl2_create_texture(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    SDL_Texture *texture = SDL_CreateTexture(
        (SDL_Renderer *)usdl2_ptr_from_obj(args[0]),
        (Uint32)mp_obj_get_int(args[1]),
        (int)mp_obj_get_int(args[2]),
        (int)mp_obj_get_int(args[3]),
        (int)mp_obj_get_int(args[4]));
    return usdl2_ptr_obj(texture);
}

mp_obj_t usdl2_destroy_texture(mp_obj_t texture_in) {
    SDL_DestroyTexture((SDL_Texture *)usdl2_ptr_from_obj(texture_in));
    return mp_const_none;
}

mp_obj_t usdl2_set_texture_blend_mode(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    int rc = SDL_SetTextureBlendMode(
        (SDL_Texture *)usdl2_ptr_from_obj(args[0]),
        (SDL_BlendMode)mp_obj_get_int(args[1]));
    return mp_obj_new_int(rc);
}

mp_obj_t usdl2_update_texture(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    SDL_Rect rect_storage;
    SDL_Rect *rect = usdl2_optional_rect(args[1], &rect_storage);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[2], &bufinfo, MP_BUFFER_READ);
    int rc = SDL_UpdateTexture(
        (SDL_Texture *)usdl2_ptr_from_obj(args[0]),
        rect,
        bufinfo.buf,
        (int)mp_obj_get_int(args[3]));
    return mp_obj_new_int(rc);
}

static uint8_t *usdl2_event_buf_from_obj(mp_obj_t event_in) {
    if (mp_obj_is_type(event_in, &usdl2_event_type)) {
        return usdl2_event_buffer(event_in);
    }
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(event_in, &bufinfo, MP_BUFFER_WRITE);
    if (bufinfo.len < USDL2_EVENT_SIZE) {
        mp_raise_ValueError(MP_ERROR_TEXT("event buffer too small"));
    }
    return (uint8_t *)bufinfo.buf;
}

// Forward declaration: defined below (near the SDL timer machinery), used
// here to keep the MP scheduler moving on every poll (see its doc comment).
static mp_int_t usdl2_scheduler_drain(mp_int_t limit);

mp_obj_t usdl2_poll_event(mp_obj_t event_in) {
    SDL_Event event;
    int rc = SDL_PollEvent(&event);
    if (rc) {
        uint8_t *buf = usdl2_event_buf_from_obj(event_in);
        memcpy(buf, &event, USDL2_EVENT_SIZE);
    }
    usdl2_scheduler_drain(32);
    return mp_obj_new_bool(rc);
}

mp_obj_t usdl2_get_key_name(mp_obj_t sym_in) {
    const char *name = SDL_GetKeyName((SDL_Keycode)mp_obj_get_int(sym_in));
    if (name == NULL) {
        return mp_obj_new_str("", 0);
    }
    return mp_obj_new_str(name, strlen(name));
}

mp_obj_t usdl2_num_joysticks(void) {
    return mp_obj_new_int(SDL_NumJoysticks());
}

mp_obj_t usdl2_joystick_open(mp_obj_t index_in) {
    SDL_Joystick *joystick = SDL_JoystickOpen((int)mp_obj_get_int(index_in));
    return usdl2_ptr_obj(joystick);
}

mp_obj_t usdl2_joystick_close(mp_obj_t joystick_in) {
    SDL_JoystickClose((SDL_Joystick *)usdl2_ptr_from_obj(joystick_in));
    return mp_const_none;
}

mp_obj_t usdl2_joystick_instance_id(mp_obj_t joystick_in) {
    return mp_obj_new_int(SDL_JoystickInstanceID((SDL_Joystick *)usdl2_ptr_from_obj(joystick_in)));
}

mp_obj_t usdl2_rect_helper(size_t n_args, const mp_obj_t *args) {
    int32_t values[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < n_args && i < 4; i++) {
        values[i] = (int32_t)mp_obj_get_int(args[i]);
    }
    return mp_obj_new_bytes((const byte *)values, sizeof(values));
}

mp_obj_t usdl2_point_helper(size_t n_args, const mp_obj_t *args) {
    int32_t values[2] = {0, 0};
    for (size_t i = 0; i < n_args && i < 2; i++) {
        values[i] = (int32_t)mp_obj_get_int(args[i]);
    }
    return mp_obj_new_bytes((const byte *)values, sizeof(values));
}

// --- Timers ------------------------------------------------------

#define USDL2_TIMER_MAX 8

typedef struct {
    mp_obj_base_t base;
    mp_obj_t callback;
} usdl2_timer_cb_obj_t;

typedef struct usdl2_timer_entry {
    struct usdl2_timer_entry *next;
    SDL_TimerID id;
    mp_obj_t callback;
    mp_obj_t user_param;
    Uint32 interval;
    Uint32 last_interval;
    uint8_t slot;
    // Set while a dispatch is queued/running so SDL ticks coalesce instead of
    // flooding MICROPY_SCHEDULER_DEPTH (see displaydev SDL timer queue full).
    volatile uint8_t pending;
} usdl2_timer_entry_t;

const mp_obj_type_t usdl2_timer_cb_type;

static usdl2_timer_entry_t *usdl2_timer_list;
static usdl2_timer_entry_t *usdl2_timer_by_slot[USDL2_TIMER_MAX];
static uint32_t usdl2_sched_full_last_ms;

static usdl2_timer_entry_t *usdl2_timer_find(SDL_TimerID id) {
    for (usdl2_timer_entry_t *entry = usdl2_timer_list; entry != NULL; entry = entry->next) {
        if (entry->id == id) {
            return entry;
        }
    }
    return NULL;
}

static int usdl2_timer_alloc_slot(usdl2_timer_entry_t *entry) {
    for (int i = 0; i < USDL2_TIMER_MAX; i++) {
        if (usdl2_timer_by_slot[i] == NULL) {
            usdl2_timer_by_slot[i] = entry;
            entry->slot = (uint8_t)i;
            return i;
        }
    }
    return -1;
}

static void usdl2_timer_free_slot(usdl2_timer_entry_t *entry) {
    if (entry->slot < USDL2_TIMER_MAX) {
        usdl2_timer_by_slot[entry->slot] = NULL;
    }
}

static void usdl2_timer_unlink(usdl2_timer_entry_t *entry) {
    usdl2_timer_entry_t **prev = &usdl2_timer_list;
    while (*prev != NULL) {
        if (*prev == entry) {
            *prev = entry->next;
            usdl2_timer_free_slot(entry);
            // Entries are malloc'd (not m_new): SDL holds raw pointers across
            // GC collections; GC heap blocks were swept while timers still lived.
            free(entry);
            return;
        }
        prev = &(*prev)->next;
    }
}

static mp_obj_t usdl2_timer_dispatch(mp_obj_t slot_in) {
    int slot = MP_OBJ_SMALL_INT_VALUE(slot_in);
    if (slot < 0 || slot >= USDL2_TIMER_MAX) {
        return mp_const_none;
    }
    usdl2_timer_entry_t *entry = usdl2_timer_by_slot[slot];
    if (entry == NULL) {
        return mp_const_none;
    }
    entry->pending = 0;
    mp_obj_t args[2] = {
        mp_obj_new_int_from_uint(entry->last_interval),
        entry->user_param,
    };
    mp_call_function_n_kw(entry->callback, 2, 0, args);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(usdl2_timer_dispatch_obj, usdl2_timer_dispatch);

static Uint32 SDLCALL usdl2_timer_trampoline(Uint32 interval, void *param) {
    usdl2_timer_entry_t *entry = (usdl2_timer_entry_t *)param;
    if (entry == NULL) {
        return 0;
    }

    entry->last_interval = interval;

    #if MICROPY_ENABLE_SCHEDULER
    // At most one queued dispatch per timer (SDL thread vs VM drain).
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
    if (entry->pending) {
        MICROPY_END_ATOMIC_SECTION(atomic_state);
        return entry->interval;
    }
    entry->pending = 1;
    MICROPY_END_ATOMIC_SECTION(atomic_state);

    if (!mp_sched_schedule(
        MP_OBJ_FROM_PTR(&usdl2_timer_dispatch_obj),
        MP_OBJ_NEW_SMALL_INT(entry->slot))) {
        atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
        entry->pending = 0;
        MICROPY_END_ATOMIC_SECTION(atomic_state);
        // Rate-limit: a full queue previously flooded stderr every SDL tick.
        uint32_t now = mp_hal_ticks_ms();
        if ((uint32_t)(now - usdl2_sched_full_last_ms) >= 1000u) {
            usdl2_sched_full_last_ms = now;
            mp_printf(MICROPY_ERROR_PRINTER, "SDL timer schedule queue full\n");
        }
    }
    #else
    usdl2_timer_dispatch(MP_OBJ_NEW_SMALL_INT(entry->slot));
    #endif

    return entry->interval;
}

mp_obj_t usdl2_timer_callback(mp_obj_t callback_in) {
    if (!mp_obj_is_callable(callback_in)) {
        mp_raise_TypeError(MP_ERROR_TEXT("callback must be callable"));
    }
    usdl2_timer_cb_obj_t *self = mp_obj_malloc(usdl2_timer_cb_obj_t, &usdl2_timer_cb_type);
    self->callback = callback_in;
    return MP_OBJ_FROM_PTR(self);
}

mp_obj_t usdl2_add_timer(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    if (!mp_obj_is_type(args[1], &usdl2_timer_cb_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("callback must be from SDL_TimerCallback()"));
    }

    usdl2_timer_cb_obj_t *cb = MP_OBJ_TO_PTR(args[1]);
    mp_obj_t user_param = mp_const_none;
    if (!usdl2_obj_is_none_or_null(args[2])) {
        user_param = args[2];
    }

    usdl2_timer_entry_t *entry = malloc(sizeof(usdl2_timer_entry_t));
    if (entry == NULL) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("out of memory"));
    }
    entry->next = usdl2_timer_list;
    entry->callback = cb->callback;
    entry->user_param = user_param;
    entry->interval = (Uint32)mp_obj_get_int(args[0]);
    entry->last_interval = entry->interval;
    entry->id = 0;
    entry->pending = 0;

    if (usdl2_timer_alloc_slot(entry) < 0) {
        free(entry);
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("too many SDL timers"));
    }

    entry->id = SDL_AddTimer(entry->interval, usdl2_timer_trampoline, entry);
    if (entry->id == 0) {
        usdl2_timer_free_slot(entry);
        free(entry);
        return mp_obj_new_int(0);
    }

    usdl2_timer_list = entry;
    return usdl2_ptr_obj((void *)(uintptr_t)entry->id);
}

// Drains up to `limit` pending scheduled callbacks (SDL timer trampolines
// scheduled via mp_sched_schedule()). Not part of the public API: SDL_PumpEvents
// and SDL_PollEvent call this internally so the MP scheduler is kept moving
// without requiring callers to pump it explicitly (there is no public SDL API
// for this, so it must not be exported as its own binding).
static mp_int_t usdl2_scheduler_drain(mp_int_t limit) {
    mp_int_t ran = 0;
    for (mp_int_t i = 0; i < limit; i++) {
        #if MICROPY_ENABLE_SCHEDULER
        if (mp_sched_num_pending() == 0) {
            break;
        }
        mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
        if (MP_STATE_VM(sched_state) == MP_SCHED_IDLE) {
            MP_STATE_VM(sched_state) = MP_SCHED_PENDING;
        }
        MICROPY_END_ATOMIC_SECTION(atomic_state);
        #else
        break;
        #endif
        mp_handle_pending(true);
        ran++;
    }

    return ran;
}

mp_obj_t usdl2_pump_events(void) {
    SDL_PumpEvents();
    usdl2_scheduler_drain(128);
    return mp_const_none;
}

mp_obj_t usdl2_remove_timer(mp_obj_t timer_in) {
    SDL_TimerID id = (SDL_TimerID)(uintptr_t)usdl2_ptr_from_obj(timer_in);
    usdl2_timer_entry_t *entry = usdl2_timer_find(id);
    if (entry == NULL) {
        return mp_obj_new_int(0);
    }

    SDL_bool removed = SDL_RemoveTimer(id);
    if (removed) {
        usdl2_timer_unlink(entry);
    }
    return mp_obj_new_int(removed ? 1 : 0);
}

MP_DEFINE_CONST_OBJ_TYPE(
    usdl2_timer_cb_type,
    MP_QSTR_usdl2_timer_cb,
    MP_TYPE_FLAG_NONE
    );

MP_REGISTER_ROOT_POINTER(struct usdl2_timer_entry *usdl2_timer_list);

// --- Soft-reset / idempotent host teardown -----------------------

static void usdl2_host_teardown(void) {
    /* Stop SDL timers and free malloc'd entries before the heap is swept.
     * Do not m_free GC objects — only release non-GC host resources. */
    usdl2_timer_entry_t *entry = usdl2_timer_list;
    usdl2_timer_list = NULL;
    while (entry != NULL) {
        usdl2_timer_entry_t *next = entry->next;
        if (entry->id != 0) {
            SDL_RemoveTimer(entry->id);
        }
        usdl2_timer_free_slot(entry);
        free(entry);
        entry = next;
    }
    memset(usdl2_timer_by_slot, 0, sizeof(usdl2_timer_by_slot));

    if (s_sdl_inited || SDL_WasInit(0) != 0) {
        SDL_Quit();
    }
    s_sdl_inited = false;
}

static void usdl2_ensure_soft_reset_registered(void) {
    if (!s_soft_reset_registered) {
        displayif_register_soft_reset(usdl2_host_teardown);
        s_soft_reset_registered = true;
    }
}

// --- Module bindings (shared by MP and CP) -----------------------

static mp_obj_t SDL_AudioSpec_obj(size_t n_args, const mp_obj_t *args) {
    SDL_AudioSpec spec;
    SDL_zero(spec);
    if (n_args > 0) {
        spec.freq = mp_obj_get_int(args[0]);
    }
    if (n_args > 1) {
        spec.format = (SDL_AudioFormat)mp_obj_get_int(args[1]);
    }
    if (n_args > 2) {
        spec.channels = (Uint8)mp_obj_get_int(args[2]);
    }
    if (n_args > 3) {
        spec.samples = (Uint16)mp_obj_get_int(args[3]);
    }
    return mp_obj_new_bytearray(sizeof(spec), &spec);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_AudioSpec_fun_obj, 0, 4, SDL_AudioSpec_obj);

static mp_obj_t SDL_GetNumAudioDevices_obj(mp_obj_t capture_in) {
    return mp_obj_new_int(SDL_GetNumAudioDevices(mp_obj_is_true(capture_in)));
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_GetNumAudioDevices_fun_obj, SDL_GetNumAudioDevices_obj);

static mp_obj_t SDL_GetAudioDeviceName_obj(size_t n_args, const mp_obj_t *args) {
    const char *name = SDL_GetAudioDeviceName(
        mp_obj_get_int(args[0]), mp_obj_is_true(args[1]));
    return name == NULL ? mp_const_none : mp_obj_new_str(name, strlen(name));
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_GetAudioDeviceName_fun_obj, 2, 2, SDL_GetAudioDeviceName_obj);

static mp_obj_t SDL_OpenAudioDevice_obj(size_t n_args, const mp_obj_t *args) {
    const char *name = args[0] == mp_const_none ? NULL : mp_obj_str_get_str(args[0]);
    mp_buffer_info_t desired_buf;
    mp_get_buffer_raise(args[2], &desired_buf, MP_BUFFER_READ);
    if (desired_buf.len < sizeof(SDL_AudioSpec)) {
        mp_raise_ValueError(MP_ERROR_TEXT("desired SDL_AudioSpec is too small"));
    }
    SDL_AudioSpec *obtained = NULL;
    mp_buffer_info_t obtained_buf;
    if (args[3] != mp_const_none) {
        mp_get_buffer_raise(args[3], &obtained_buf, MP_BUFFER_WRITE);
        if (obtained_buf.len < sizeof(SDL_AudioSpec)) {
            mp_raise_ValueError(MP_ERROR_TEXT("obtained SDL_AudioSpec is too small"));
        }
        obtained = (SDL_AudioSpec *)obtained_buf.buf;
    }
    SDL_AudioDeviceID id = SDL_OpenAudioDevice(
        name,
        mp_obj_is_true(args[1]),
        (const SDL_AudioSpec *)desired_buf.buf,
        obtained,
        mp_obj_get_int(args[4]));
    return mp_obj_new_int_from_uint(id);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_OpenAudioDevice_fun_obj, 5, 5, SDL_OpenAudioDevice_obj);

static mp_obj_t SDL_CloseAudioDevice_obj(mp_obj_t device_in) {
    SDL_CloseAudioDevice((SDL_AudioDeviceID)mp_obj_get_int(device_in));
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_CloseAudioDevice_fun_obj, SDL_CloseAudioDevice_obj);

static mp_obj_t SDL_PauseAudioDevice_obj(size_t n_args, const mp_obj_t *args) {
    SDL_PauseAudioDevice(
        (SDL_AudioDeviceID)mp_obj_get_int(args[0]), mp_obj_is_true(args[1]));
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_PauseAudioDevice_fun_obj, 2, 2, SDL_PauseAudioDevice_obj);

static mp_obj_t SDL_QueueAudio_obj(size_t n_args, const mp_obj_t *args) {
    mp_buffer_info_t buf;
    mp_get_buffer_raise(args[1], &buf, MP_BUFFER_READ);
    mp_uint_t length = mp_obj_get_int(args[2]);
    if (length > buf.len) {
        mp_raise_ValueError(MP_ERROR_TEXT("length exceeds buffer"));
    }
    int rc = SDL_QueueAudio((SDL_AudioDeviceID)mp_obj_get_int(args[0]), buf.buf, length);
    return mp_obj_new_int(rc);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_QueueAudio_fun_obj, 3, 3, SDL_QueueAudio_obj);

static mp_obj_t SDL_DequeueAudio_obj(size_t n_args, const mp_obj_t *args) {
    mp_buffer_info_t buf;
    mp_get_buffer_raise(args[1], &buf, MP_BUFFER_WRITE);
    mp_uint_t length = mp_obj_get_int(args[2]);
    if (length > buf.len) {
        mp_raise_ValueError(MP_ERROR_TEXT("length exceeds buffer"));
    }
    Uint32 count = SDL_DequeueAudio(
        (SDL_AudioDeviceID)mp_obj_get_int(args[0]), buf.buf, length);
    return mp_obj_new_int_from_uint(count);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_DequeueAudio_fun_obj, 3, 3, SDL_DequeueAudio_obj);

static mp_obj_t SDL_GetQueuedAudioSize_obj(mp_obj_t device_in) {
    return mp_obj_new_int_from_uint(
        SDL_GetQueuedAudioSize((SDL_AudioDeviceID)mp_obj_get_int(device_in)));
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_GetQueuedAudioSize_fun_obj, SDL_GetQueuedAudioSize_obj);

static mp_obj_t SDL_ClearQueuedAudio_obj(mp_obj_t device_in) {
    SDL_ClearQueuedAudio((SDL_AudioDeviceID)mp_obj_get_int(device_in));
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_ClearQueuedAudio_fun_obj, SDL_ClearQueuedAudio_obj);

static mp_obj_t SDL_LockAudioDevice_obj(mp_obj_t device_in) {
    SDL_LockAudioDevice((SDL_AudioDeviceID)mp_obj_get_int(device_in));
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_LockAudioDevice_fun_obj, SDL_LockAudioDevice_obj);

static mp_obj_t SDL_UnlockAudioDevice_obj(mp_obj_t device_in) {
    SDL_UnlockAudioDevice((SDL_AudioDeviceID)mp_obj_get_int(device_in));
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_UnlockAudioDevice_fun_obj, SDL_UnlockAudioDevice_obj);

static mp_obj_t SDL_Init_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_init(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_Init_fun_obj, 1, 1, SDL_Init_obj);

static mp_obj_t SDL_InitSubSystem_obj(mp_obj_t flags_in) {
    return usdl2_init_subsystem(flags_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_InitSubSystem_fun_obj, SDL_InitSubSystem_obj);

static mp_obj_t SDL_Quit_obj(void) {
    return usdl2_quit();
}
MP_DEFINE_CONST_FUN_OBJ_0(SDL_Quit_fun_obj, SDL_Quit_obj);

static mp_obj_t SDL_GetError_obj(void) {
    return usdl2_get_error();
}
MP_DEFINE_CONST_FUN_OBJ_0(SDL_GetError_fun_obj, SDL_GetError_obj);

static mp_obj_t SDL_SetHint_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_set_hint(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_SetHint_fun_obj, 2, 2, SDL_SetHint_obj);

static mp_obj_t SDL_CreateWindow_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_create_window(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_CreateWindow_fun_obj, 6, 6, SDL_CreateWindow_obj);

static mp_obj_t SDL_DestroyWindow_obj(mp_obj_t win_in) {
    return usdl2_destroy_window(win_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_DestroyWindow_fun_obj, SDL_DestroyWindow_obj);

static mp_obj_t SDL_GetWindowID_obj(mp_obj_t win_in) {
    return usdl2_get_window_id(win_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_GetWindowID_fun_obj, SDL_GetWindowID_obj);

static mp_obj_t SDL_SetWindowSize_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_set_window_size(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_SetWindowSize_fun_obj, 3, 3, SDL_SetWindowSize_obj);

static mp_obj_t SDL_SetWindowResizable_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_set_window_resizable(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_SetWindowResizable_fun_obj, 2, 2, SDL_SetWindowResizable_obj);

static mp_obj_t SDL_SetWindowMinimumSize_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_set_window_minimum_size(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_SetWindowMinimumSize_fun_obj, 3, 3, SDL_SetWindowMinimumSize_obj);

static mp_obj_t SDL_SetWindowMaximumSize_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_set_window_maximum_size(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_SetWindowMaximumSize_fun_obj, 3, 3, SDL_SetWindowMaximumSize_obj);

static mp_obj_t SDL_CreateRenderer_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_create_renderer(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_CreateRenderer_fun_obj, 3, 3, SDL_CreateRenderer_obj);

static mp_obj_t SDL_DestroyRenderer_obj(mp_obj_t renderer_in) {
    return usdl2_destroy_renderer(renderer_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_DestroyRenderer_fun_obj, SDL_DestroyRenderer_obj);

static mp_obj_t SDL_SetRenderDrawColor_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_set_render_draw_color(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_SetRenderDrawColor_fun_obj, 5, 5, SDL_SetRenderDrawColor_obj);

static mp_obj_t SDL_SetRenderTarget_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_set_render_target(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_SetRenderTarget_fun_obj, 2, 2, SDL_SetRenderTarget_obj);

static mp_obj_t SDL_RenderClear_obj(mp_obj_t renderer_in) {
    return usdl2_render_clear(renderer_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_RenderClear_fun_obj, SDL_RenderClear_obj);

static mp_obj_t SDL_RenderCopy_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_render_copy(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_RenderCopy_fun_obj, 4, 4, SDL_RenderCopy_obj);

static mp_obj_t SDL_RenderCopyEx_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_render_copy_ex(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_RenderCopyEx_fun_obj, 7, 7, SDL_RenderCopyEx_obj);

static mp_obj_t SDL_RenderPresent_obj(mp_obj_t renderer_in) {
    return usdl2_render_present(renderer_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_RenderPresent_fun_obj, SDL_RenderPresent_obj);

static mp_obj_t SDL_RenderFillRect_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_render_fill_rect(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_RenderFillRect_fun_obj, 2, 2, SDL_RenderFillRect_obj);

static mp_obj_t SDL_RenderSetLogicalSize_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_render_set_logical_size(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_RenderSetLogicalSize_fun_obj, 3, 3, SDL_RenderSetLogicalSize_obj);

static mp_obj_t SDL_CreateTexture_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_create_texture(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_CreateTexture_fun_obj, 5, 5, SDL_CreateTexture_obj);

static mp_obj_t SDL_DestroyTexture_obj(mp_obj_t texture_in) {
    return usdl2_destroy_texture(texture_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_DestroyTexture_fun_obj, SDL_DestroyTexture_obj);

static mp_obj_t SDL_SetTextureBlendMode_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_set_texture_blend_mode(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_SetTextureBlendMode_fun_obj, 2, 2, SDL_SetTextureBlendMode_obj);

static mp_obj_t SDL_UpdateTexture_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_update_texture(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_UpdateTexture_fun_obj, 4, 4, SDL_UpdateTexture_obj);

static mp_obj_t SDL_PollEvent_obj(mp_obj_t event_in) {
    return usdl2_poll_event(event_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_PollEvent_fun_obj, SDL_PollEvent_obj);

static mp_obj_t SDL_GetKeyName_obj(mp_obj_t sym_in) {
    return usdl2_get_key_name(sym_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_GetKeyName_fun_obj, SDL_GetKeyName_obj);

static mp_obj_t SDL_NumJoysticks_obj(void) {
    return usdl2_num_joysticks();
}
MP_DEFINE_CONST_FUN_OBJ_0(SDL_NumJoysticks_fun_obj, SDL_NumJoysticks_obj);

static mp_obj_t SDL_JoystickOpen_obj(mp_obj_t index_in) {
    return usdl2_joystick_open(index_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_JoystickOpen_fun_obj, SDL_JoystickOpen_obj);

static mp_obj_t SDL_JoystickClose_obj(mp_obj_t joystick_in) {
    return usdl2_joystick_close(joystick_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_JoystickClose_fun_obj, SDL_JoystickClose_obj);

static mp_obj_t SDL_JoystickInstanceID_obj(mp_obj_t joystick_in) {
    return usdl2_joystick_instance_id(joystick_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_JoystickInstanceID_fun_obj, SDL_JoystickInstanceID_obj);

static mp_obj_t SDL_Rect_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_rect_helper(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_Rect_fun_obj, 0, 4, SDL_Rect_obj);

static mp_obj_t SDL_Point_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_point_helper(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_Point_fun_obj, 0, 2, SDL_Point_obj);

static mp_obj_t SDL_TimerCallback_obj(mp_obj_t callback_in) {
    return usdl2_timer_callback(callback_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_TimerCallback_fun_obj, SDL_TimerCallback_obj);

static mp_obj_t SDL_AddTimer_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_add_timer(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_AddTimer_fun_obj, 3, 3, SDL_AddTimer_obj);

static mp_obj_t SDL_RemoveTimer_obj(mp_obj_t timer_in) {
    return usdl2_remove_timer(timer_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(SDL_RemoveTimer_fun_obj, SDL_RemoveTimer_obj);

static mp_obj_t SDL_PumpEvents_obj(void) {
    return usdl2_pump_events();
}
MP_DEFINE_CONST_FUN_OBJ_0(SDL_PumpEvents_fun_obj, SDL_PumpEvents_obj);

mp_obj_t usdl2_get_display_usable_bounds(size_t n_args, const mp_obj_t *args) {
    int display_index = (int)mp_obj_get_int(args[0]);
    SDL_Rect rect;
    int rc = SDL_GetDisplayUsableBounds(display_index, &rect);
    if (rc == 0 && n_args > 1) {
        usdl2_rect_to_bytes(&rect, args[1]);
    }
    return mp_obj_new_int(rc);
}

static mp_obj_t SDL_GetDisplayUsableBounds_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_get_display_usable_bounds(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_GetDisplayUsableBounds_fun_obj, 1, 2, SDL_GetDisplayUsableBounds_obj);

mp_obj_t usdl2_get_desktop_display_mode(size_t n_args, const mp_obj_t *args) {
    int display_index = (int)mp_obj_get_int(args[0]);
    SDL_DisplayMode mode;
    int rc = SDL_GetDesktopDisplayMode(display_index, &mode);
    if (rc == 0 && n_args > 1) {
        usdl2_display_mode_to_bytes(&mode, args[1]);
    }
    return mp_obj_new_int(rc);
}

static mp_obj_t SDL_GetDesktopDisplayMode_obj(size_t n_args, const mp_obj_t *args) {
    return usdl2_get_desktop_display_mode(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(SDL_GetDesktopDisplayMode_fun_obj, 1, 2, SDL_GetDesktopDisplayMode_obj);

// --- Module registration (MicroPython only; CP uses shared-bindings/usdl2/__init.c) ---

#if !CIRCUITPY

//| """Desktop SDL2 subset for PyDevices displaydev (linked against libSDL2)."""

static const mp_rom_map_elem_t usdl2_module_globals_table[] = {
#include "usdl2_module_globals.inc"
};

static MP_DEFINE_CONST_DICT(usdl2_module_globals, usdl2_module_globals_table);

const mp_obj_module_t usdl2_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&usdl2_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_usdl2, usdl2_module);

#endif
