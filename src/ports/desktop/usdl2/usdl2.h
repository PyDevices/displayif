// SPDX-License-Identifier: MIT
// usdl2 — desktop PyDevices SDL2 subset for MicroPython and CircuitPython.

#pragma once

#include "py/obj.h"
#include <SDL.h>
#include <stdint.h>

#define USDL2_EVENT_SIZE 56

extern const mp_obj_type_t usdl2_event_type;
extern const mp_obj_type_t usdl2_timer_cb_type;

// Binding fun objs referenced by usdl2_module_globals.inc (CP shared-bindings + MP).
extern const mp_obj_fun_builtin_var_t usdl2_SDL_DEFINE_PIXELFORMAT_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_Init_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_InitSubSystem_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_Quit_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_GetError_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_SetHint_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_CreateWindow_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_DestroyWindow_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_GetWindowID_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_SetWindowSize_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_SetWindowResizable_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_SetWindowMinimumSize_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_SetWindowMaximumSize_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_CreateRenderer_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_DestroyRenderer_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_SetRenderDrawColor_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_SetRenderTarget_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_RenderClear_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_RenderCopy_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_RenderCopyEx_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_RenderPresent_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_RenderFillRect_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_RenderSetLogicalSize_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_CreateTexture_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_DestroyTexture_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_SetTextureBlendMode_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_UpdateTexture_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_PollEvent_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_GetKeyName_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_NumJoysticks_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_JoystickOpen_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_JoystickClose_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_JoystickInstanceID_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_Rect_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_Point_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_AudioSpec_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_GetNumAudioDevices_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_GetAudioDeviceName_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_OpenAudioDevice_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_CloseAudioDevice_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_PauseAudioDevice_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_QueueAudio_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_DequeueAudio_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_GetQueuedAudioSize_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_ClearQueuedAudio_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_LockAudioDevice_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_UnlockAudioDevice_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_TimerCallback_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_AddTimer_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_RemoveTimer_fun_obj;
extern const mp_obj_fun_builtin_fixed_t SDL_PumpEvents_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_GetDisplayUsableBounds_fun_obj;
extern const mp_obj_fun_builtin_var_t SDL_GetDesktopDisplayMode_fun_obj;

void *usdl2_ptr_from_obj(mp_obj_t obj);
mp_obj_t usdl2_ptr_obj(void *ptr);
bool usdl2_obj_is_none_or_null(mp_obj_t obj);
void usdl2_rect_from_bytes(mp_obj_t obj, SDL_Rect *rect);
void usdl2_rect_to_bytes(const SDL_Rect *rect, mp_obj_t obj);
void usdl2_display_mode_to_bytes(const SDL_DisplayMode *mode, mp_obj_t obj);
SDL_Rect *usdl2_optional_rect(mp_obj_t obj, SDL_Rect *storage);

mp_obj_t usdl2_event_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args);
uint8_t *usdl2_event_buffer(mp_obj_t self);
mp_int_t usdl2_event_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags);

mp_obj_t usdl2_init(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_init_subsystem(mp_obj_t flags_in);
mp_obj_t usdl2_quit(void);
mp_obj_t usdl2_get_error(void);
mp_obj_t usdl2_set_hint(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_create_window(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_destroy_window(mp_obj_t win_in);
mp_obj_t usdl2_get_window_id(mp_obj_t win_in);
mp_obj_t usdl2_set_window_size(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_set_window_resizable(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_set_window_minimum_size(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_set_window_maximum_size(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_create_renderer(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_destroy_renderer(mp_obj_t renderer_in);
mp_obj_t usdl2_set_render_draw_color(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_set_render_target(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_render_clear(mp_obj_t renderer_in);
mp_obj_t usdl2_render_copy(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_render_copy_ex(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_render_present(mp_obj_t renderer_in);
mp_obj_t usdl2_render_fill_rect(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_render_set_logical_size(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_create_texture(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_destroy_texture(mp_obj_t texture_in);
mp_obj_t usdl2_set_texture_blend_mode(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_update_texture(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_poll_event(mp_obj_t event_in);
mp_obj_t usdl2_get_key_name(mp_obj_t sym_in);
mp_obj_t usdl2_num_joysticks(void);
mp_obj_t usdl2_joystick_open(mp_obj_t index_in);
mp_obj_t usdl2_joystick_close(mp_obj_t joystick_in);
mp_obj_t usdl2_joystick_instance_id(mp_obj_t joystick_in);
mp_obj_t usdl2_rect_helper(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_point_helper(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_window_title(mp_obj_t title_in);
mp_obj_t usdl2_timer_callback(mp_obj_t callback_in);
mp_obj_t usdl2_add_timer(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_remove_timer(mp_obj_t timer_in);
mp_obj_t usdl2_pump_events(void);
mp_obj_t usdl2_get_display_usable_bounds(size_t n_args, const mp_obj_t *args);
mp_obj_t usdl2_get_desktop_display_mode(size_t n_args, const mp_obj_t *args);

#define USDL2_DEFINE_PIXELFORMAT(type, order, layout, bits, bytes) \
    ((1 << 28) | ((type) << 24) | ((order) << 20) | ((layout) << 16) | ((bits) << 8) | (bytes))

#define USDL2_CONSTANTS_TABLE \
    { MP_ROM_QSTR(MP_QSTR_AUDIO_U8), MP_ROM_INT(AUDIO_U8) }, \
    { MP_ROM_QSTR(MP_QSTR_AUDIO_S8), MP_ROM_INT(AUDIO_S8) }, \
    { MP_ROM_QSTR(MP_QSTR_AUDIO_U16LSB), MP_ROM_INT(AUDIO_U16LSB) }, \
    { MP_ROM_QSTR(MP_QSTR_AUDIO_S16LSB), MP_ROM_INT(AUDIO_S16LSB) }, \
    { MP_ROM_QSTR(MP_QSTR_AUDIO_U16MSB), MP_ROM_INT(AUDIO_U16MSB) }, \
    { MP_ROM_QSTR(MP_QSTR_AUDIO_S16MSB), MP_ROM_INT(AUDIO_S16MSB) }, \
    { MP_ROM_QSTR(MP_QSTR_AUDIO_S32LSB), MP_ROM_INT(AUDIO_S32LSB) }, \
    { MP_ROM_QSTR(MP_QSTR_AUDIO_S32MSB), MP_ROM_INT(AUDIO_S32MSB) }, \
    { MP_ROM_QSTR(MP_QSTR_AUDIO_F32LSB), MP_ROM_INT(AUDIO_F32LSB) }, \
    { MP_ROM_QSTR(MP_QSTR_AUDIO_F32MSB), MP_ROM_INT(AUDIO_F32MSB) }, \
    { MP_ROM_QSTR(MP_QSTR_AUDIO_U16SYS), MP_ROM_INT(AUDIO_U16SYS) }, \
    { MP_ROM_QSTR(MP_QSTR_AUDIO_S16SYS), MP_ROM_INT(AUDIO_S16SYS) }, \
    { MP_ROM_QSTR(MP_QSTR_AUDIO_S32SYS), MP_ROM_INT(AUDIO_S32SYS) }, \
    { MP_ROM_QSTR(MP_QSTR_AUDIO_F32SYS), MP_ROM_INT(AUDIO_F32SYS) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_ARRAYORDER_ABGR), MP_ROM_INT(6) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_ARRAYORDER_ARGB), MP_ROM_INT(3) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_ARRAYORDER_BGR), MP_ROM_INT(4) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_ARRAYORDER_BGRA), MP_ROM_INT(5) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_ARRAYORDER_NONE), MP_ROM_INT(0) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_ARRAYORDER_RGB), MP_ROM_INT(1) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_ARRAYORDER_RGBA), MP_ROM_INT(2) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_BITMAPORDER_1234), MP_ROM_INT(2) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_BITMAPORDER_4321), MP_ROM_INT(1) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_BITMAPORDER_NONE), MP_ROM_INT(0) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_BLENDMODE_ADD), MP_ROM_INT(3) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_BLENDMODE_BLEND), MP_ROM_INT(2) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_BLENDMODE_MOD), MP_ROM_INT(4) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_BLENDMODE_MUL), MP_ROM_INT(5) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_BLENDMODE_NONE), MP_ROM_INT(1) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_BUTTON_LMASK), MP_ROM_INT(1) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_BUTTON_MMASK), MP_ROM_INT(2) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_BUTTON_RMASK), MP_ROM_INT(4) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_HAT_CENTERED), MP_ROM_INT(0) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_HAT_DOWN), MP_ROM_INT(4) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_HAT_LEFT), MP_ROM_INT(8) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_HAT_RIGHT), MP_ROM_INT(2) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_HAT_UP), MP_ROM_INT(1) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_INIT_AUDIO), MP_ROM_INT(16) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_INIT_EVENTS), MP_ROM_INT(16384) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_INIT_EVERYTHING), MP_ROM_INT(15) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_INIT_GAMECONTROLLER), MP_ROM_INT(8192) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_INIT_HAPTIC), MP_ROM_INT(4096) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_INIT_JOYSTICK), MP_ROM_INT(512) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_INIT_NOPARACHUTE), MP_ROM_INT(1048576) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_INIT_TIMER), MP_ROM_INT(1) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_INIT_VIDEO), MP_ROM_INT(32) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_JOYAXISMOTION), MP_ROM_INT(1536) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_JOYBALLMOTION), MP_ROM_INT(1537) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_JOYBUTTONDOWN), MP_ROM_INT(1539) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_JOYBUTTONUP), MP_ROM_INT(1540) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_JOYDEVICEADDED), MP_ROM_INT(1541) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_JOYDEVICEREMOVED), MP_ROM_INT(1542) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_JOYHATMOTION), MP_ROM_INT(1538) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_FINGERDOWN), MP_ROM_INT(1792) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_FINGERMOTION), MP_ROM_INT(1794) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_FINGERUP), MP_ROM_INT(1793) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_KEYDOWN), MP_ROM_INT(768) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_KEYUP), MP_ROM_INT(769) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_MOUSEBUTTONDOWN), MP_ROM_INT(1025) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_MOUSEBUTTONUP), MP_ROM_INT(1026) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_MOUSEMOTION), MP_ROM_INT(1024) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_MOUSEWHEEL), MP_ROM_INT(1027) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDLAYOUT_1010102), MP_ROM_INT(8) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDLAYOUT_1555), MP_ROM_INT(3) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDLAYOUT_2101010), MP_ROM_INT(7) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDLAYOUT_332), MP_ROM_INT(1) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDLAYOUT_4444), MP_ROM_INT(2) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDLAYOUT_5551), MP_ROM_INT(4) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDLAYOUT_565), MP_ROM_INT(5) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDLAYOUT_8888), MP_ROM_INT(6) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDLAYOUT_NONE), MP_ROM_INT(0) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDORDER_ABGR), MP_ROM_INT(7) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDORDER_ARGB), MP_ROM_INT(3) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDORDER_BGRA), MP_ROM_INT(8) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDORDER_BGRX), MP_ROM_INT(6) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDORDER_NONE), MP_ROM_INT(0) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDORDER_RGBA), MP_ROM_INT(4) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDORDER_RGBX), MP_ROM_INT(2) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDORDER_XBGR), MP_ROM_INT(5) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PACKEDORDER_XRGB), MP_ROM_INT(1) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_ABGR1555), MP_ROM_INT(359862274) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_ABGR4444), MP_ROM_INT(359796738) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_ABGR8888), MP_ROM_INT(376840196) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_ARGB1555), MP_ROM_INT(355667970) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_ARGB2101010), MP_ROM_INT(372711428) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_ARGB4444), MP_ROM_INT(355602434) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_ARGB8888), MP_ROM_INT(372645892) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_BGR24), MP_ROM_INT(390076419) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_BGR444), MP_ROM_INT(357698562) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_BGR555), MP_ROM_INT(357764866) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_BGR565), MP_ROM_INT(357896194) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_BGR888), MP_ROM_INT(374740996) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_BGRA4444), MP_ROM_INT(360845314) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_BGRA5551), MP_ROM_INT(360976386) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_BGRA8888), MP_ROM_INT(377888772) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_BGRX8888), MP_ROM_INT(375789572) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_INDEX1LSB), MP_ROM_INT(286261504) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_INDEX1MSB), MP_ROM_INT(287310080) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_INDEX4LSB), MP_ROM_INT(303039488) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_INDEX4MSB), MP_ROM_INT(304088064) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_INDEX8), MP_ROM_INT(318769153) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_RGB24), MP_ROM_INT(386930691) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_RGB332), MP_ROM_INT(336660481) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_RGB444), MP_ROM_INT(353504258) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_RGB555), MP_ROM_INT(353570562) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_RGB565), MP_ROM_INT(353701890) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_RGB888), MP_ROM_INT(370546692) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_RGBA4444), MP_ROM_INT(356651010) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_RGBA5551), MP_ROM_INT(356782082) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_RGBA8888), MP_ROM_INT(373694468) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_RGBX8888), MP_ROM_INT(371595268) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_UNKNOWN), MP_ROM_INT(0) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_XBGR1555), MP_ROM_INT(357764866) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_XBGR4444), MP_ROM_INT(357698562) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_XBGR8888), MP_ROM_INT(374740996) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_XRGB1555), MP_ROM_INT(353570562) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_XRGB4444), MP_ROM_INT(353504258) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELFORMAT_XRGB8888), MP_ROM_INT(370546692) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELTYPE_ARRAYF16), MP_ROM_INT(10) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELTYPE_ARRAYF32), MP_ROM_INT(11) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELTYPE_ARRAYU16), MP_ROM_INT(8) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELTYPE_ARRAYU32), MP_ROM_INT(9) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELTYPE_ARRAYU8), MP_ROM_INT(7) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELTYPE_INDEX1), MP_ROM_INT(1) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELTYPE_INDEX4), MP_ROM_INT(2) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELTYPE_INDEX8), MP_ROM_INT(3) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELTYPE_PACKED16), MP_ROM_INT(5) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELTYPE_PACKED32), MP_ROM_INT(6) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELTYPE_PACKED8), MP_ROM_INT(4) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_PIXELTYPE_UNKNOWN), MP_ROM_INT(0) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_POLLSENTINEL), MP_ROM_INT(32512) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_QUIT), MP_ROM_INT(256) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOWEVENT), MP_ROM_INT(512) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOWEVENT_CLOSE), MP_ROM_INT(14) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_RENDERER_ACCELERATED), MP_ROM_INT(2) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_RENDERER_PRESENTVSYNC), MP_ROM_INT(4) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_RENDERER_SOFTWARE), MP_ROM_INT(1) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_RENDERER_TARGETTEXTURE), MP_ROM_INT(8) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_TEXTUREACCESS_STATIC), MP_ROM_INT(0) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_TEXTUREACCESS_STREAMING), MP_ROM_INT(1) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_TEXTUREACCESS_TARGET), MP_ROM_INT(2) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOWPOS_CENTERED), MP_ROM_INT(805240832) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOWPOS_UNDEFINED), MP_ROM_INT(536805376) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_ALLOW_HIGHDPI), MP_ROM_INT(8192) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_ALWAYS_ON_TOP), MP_ROM_INT(32768) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_BORDERLESS), MP_ROM_INT(16) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_FULLSCREEN), MP_ROM_INT(1) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_FULLSCREEN_DESKTOP), MP_ROM_INT(4097) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_HIDDEN), MP_ROM_INT(8) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_INPUT_FOCUS), MP_ROM_INT(512) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_INPUT_GRABBED), MP_ROM_INT(256) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_MAXIMIZED), MP_ROM_INT(128) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_MINIMIZED), MP_ROM_INT(64) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_MOUSE_CAPTURE), MP_ROM_INT(16384) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_MOUSE_FOCUS), MP_ROM_INT(1024) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_OPENGL), MP_ROM_INT(2) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_POPUP_MENU), MP_ROM_INT(524288) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_RESIZABLE), MP_ROM_INT(32) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_SHOWN), MP_ROM_INT(4) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_SKIP_TASKBAR), MP_ROM_INT(65536) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_TOOLTIP), MP_ROM_INT(262144) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_UTILITY), MP_ROM_INT(131072) }, \
    { MP_ROM_QSTR(MP_QSTR_SDL_WINDOW_VULKAN), MP_ROM_INT(268435456) },
