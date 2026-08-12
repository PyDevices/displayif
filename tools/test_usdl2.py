# SPDX-License-Identifier: MIT
"""Smoke test for usdl2 (MicroPython unix/windows or CircuitPython unix)."""

import struct
import usdl2

# SDL_INIT_VIDEO
assert usdl2.SDL_INIT_VIDEO == 0x00000020
assert usdl2.SDL_HINT_ORIENTATIONS == "SDL_IOS_ORIENTATIONS"
assert callable(usdl2.SDL_SetHint)
assert usdl2.SDL_Init(usdl2.SDL_INIT_VIDEO) == 0

# Joystick subsystem API (parity with the original displaydev SDL implementation)
assert usdl2.SDL_InitSubSystem(usdl2.SDL_INIT_JOYSTICK) == 0
assert isinstance(usdl2.SDL_NumJoysticks(), int)

win = usdl2.SDL_CreateWindow("usdl2 test", 100, 100, 320, 240, 0)
assert win

renderer = usdl2.SDL_CreateRenderer(win, -1, usdl2.SDL_RENDERER_ACCELERATED)
if not renderer:
    renderer = usdl2.SDL_CreateRenderer(win, -1, usdl2.SDL_RENDERER_SOFTWARE)
assert renderer

usdl2.SDL_SetRenderDrawColor(renderer, 32, 64, 128, 255)
usdl2.SDL_RenderClear(renderer)
usdl2.SDL_RenderPresent(renderer)

# SDL_Rect / SDL_Point helpers
rect = usdl2.SDL_Rect(1, 2, 3, 4)
assert len(rect) == 16
point = usdl2.SDL_Point(5, 6)
assert len(point) == 8

# Synthetic joystick events: verify Event subviews match SDL2 layout.
def _pack_event(event_type, payload):
    buf = bytearray(56)
    struct.pack_into("<I", buf, 0, event_type)
    buf[8:8 + len(payload)] = payload
    return usdl2.SDL_Event(buf)


axis_evt = _pack_event(
    usdl2.SDL_JOYAXISMOTION,
    struct.pack("<iBxxxh", 1, 2, -16384),
)
assert axis_evt.jaxis.which == 1
assert axis_evt.jaxis.axis == 2
assert axis_evt.jaxis.value == -16384

ball_evt = _pack_event(
    usdl2.SDL_JOYBALLMOTION,
    struct.pack("<iBxxxhh", 3, 1, 10, -5),
)
assert ball_evt.jball.which == 3
assert ball_evt.jball.ball == 1
assert ball_evt.jball.xrel == 10
assert ball_evt.jball.yrel == -5

hat_evt = _pack_event(
    usdl2.SDL_JOYHATMOTION,
    struct.pack("<iBBxx", 4, 0, 0x05),
)
assert hat_evt.jhat.which == 4
assert hat_evt.jhat.hat == 0
assert hat_evt.jhat.value == 0x05

btn_evt = _pack_event(
    usdl2.SDL_JOYBUTTONDOWN,
    struct.pack("<iBBBB", 5, 7, 1, 0, 0),
)
assert btn_evt.jbutton.which == 5
assert btn_evt.jbutton.button == 7
assert btn_evt.jbutton.state == 1

# SDL_TouchFingerEvent (event.tfinger.*) — SDL2 layout only.
assert usdl2.SDL_FINGERDOWN == 0x700
assert usdl2.SDL_FINGERUP == 0x701
assert usdl2.SDL_FINGERMOTION == 0x702
finger_evt = _pack_event(
    usdl2.SDL_FINGERDOWN,
    struct.pack("<qqfffffI", 9, 3, 0.25, 0.5, 0.01, -0.02, 0.75, 42),
)
assert finger_evt.tfinger.touchId == 9
assert finger_evt.tfinger.fingerId == 3
assert abs(finger_evt.tfinger.x - 0.25) < 1e-6
assert abs(finger_evt.tfinger.y - 0.5) < 1e-6
assert abs(finger_evt.tfinger.dx - 0.01) < 1e-6
assert abs(finger_evt.tfinger.dy - (-0.02)) < 1e-6
assert abs(finger_evt.tfinger.pressure - 0.75) < 1e-6
assert finger_evt.tfinger.windowID == 42

# SDL_Event helper
empty = usdl2.SDL_Event()
assert empty.type == 0
wrapped = usdl2.SDL_Event(btn_evt)
assert wrapped.jbutton.button == 7
assert usdl2.SDL_Event(btn_evt) is btn_evt

# Timer API (multimer / displaydev)
assert usdl2.SDL_INIT_TIMER == 0x00000001
assert usdl2.SDL_Init(usdl2.SDL_INIT_TIMER) == 0
assert usdl2.SDL_InitSubSystem(usdl2.SDL_INIT_TIMER) == 0
assert callable(usdl2.SDL_PumpEvents)

_timer_ticks = []


def _timer_cb(interval, _param):
    _timer_ticks.append(interval)
    return interval


_tcb = usdl2.SDL_TimerCallback(_timer_cb)
_timer = usdl2.SDL_AddTimer(50, _tcb, None)
assert _timer

import time

_poll = usdl2.SDL_Event()
_deadline = time.time() + 0.25
while time.time() < _deadline:
    usdl2.SDL_PumpEvents()
    while usdl2.SDL_PollEvent(_poll):
        pass
    time.sleep(0.01)

assert usdl2.SDL_RemoveTimer(_timer)
assert len(_timer_ticks) > 0

event = usdl2.SDL_Event()
while usdl2.SDL_PollEvent(event):
    if event.type == usdl2.SDL_QUIT:
        break

usdl2.SDL_DestroyRenderer(renderer)
usdl2.SDL_DestroyWindow(win)
usdl2.SDL_Quit()

print("usdl2 smoke test ok")
