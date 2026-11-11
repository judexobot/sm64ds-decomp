#pragma once
#include <cstdint>
#include "hal/pad_backend.h"

namespace sm64ds_android {
// XInput-compatible bitmask consumed by the original port's input paths.
enum Button : uint16_t {
    UP=0x0001, DOWN=0x0002, LEFT=0x0004, RIGHT=0x0008,
    START=0x0010, SELECT=0x0020, L=0x0100, R=0x0200,
    A=0x1000, B=0x2000, X=0x4000, Y=0x8000
};

// The Android main/UI thread publishes the *entire* pad state atomically,
// including releases; the game reads one consistent snapshot each frame.
// +Y means UP, unlike Android pixel coordinates.
void touch_set(uint16_t buttons, int lx, int ly, int rx, int ry, int rt);
void touch_clear();
bool touch_poll(PortPadState *out);
struct StylusState {
    bool down = false;
    int x = 0; // DS sub-screen 0..255
    int y = 0; // DS sub-screen 0..191
};
void stylus_set(bool down, int x, int y);
bool stylus_poll(StylusState *out);
}
