#include "scene_input.h"
#include <cstring>

namespace sm64ds_android {
namespace {
constexpr uint16_t X_UP=0x0001,X_DOWN=0x0002,X_LEFT=0x0004,X_RIGHT=0x0008;
constexpr uint16_t X_START=0x0010,X_SELECT=0x0020,X_L=0x0100,X_R=0x0200;
constexpr uint16_t X_A=0x1000,X_B=0x2000,X_X=0x4000,X_Y=0x8000;

uint16_t native_ds_buttons(const PortPadState &pad) {
    uint16_t raw = 0;
    // Xbox A = DS B (jump); Xbox B = DS A (punch);
    // Xbox X = DS Y (run), Xbox Y = DS X (special/minigames).
    if (pad.buttons & X_A) raw |= 0x0002;
    if (pad.buttons & X_B) raw |= 0x0001;
    if (pad.buttons & X_X) raw |= 0x0800;
    if (pad.buttons & X_Y) raw |= 0x0400;
    // Unlike the walk-direction analog, DS d-pad is the actual PadData bits
    // consumed by the original title, file-select and minigame classes.
    if (pad.buttons & X_RIGHT || pad.lx > 8192) raw |= 0x0010;
    if (pad.buttons & X_LEFT  || pad.lx < -8192) raw |= 0x0020;
    if (pad.buttons & X_UP    || pad.ly > 8192) raw |= 0x0040;
    if (pad.buttons & X_DOWN  || pad.ly < -8192) raw |= 0x0080;
    if (pad.buttons & X_START) raw |= 0x0008;
    if (pad.buttons & X_SELECT) raw |= 0x0004;
    if (pad.buttons & X_L) raw |= 0x0200;     // DS L
    if (pad.buttons & X_R || pad.rt > 100) raw |= 0x0100; // DS R
    return raw;
}
uint16_t remap_ctrl(uint16_t raw) {
    uint16_t ctrl = 0;
    if (raw & 0x0001) ctrl |= 0x0001; // DS A -> attack
    if (raw & 0x0002) ctrl |= 0x0002; // DS B -> jump
    if (raw & 0x0100) ctrl |= 0x0400; // DS R -> crouch
    if (raw & 0x0200) ctrl |= 0x4000; // DS L -> snap
    if (raw & 0x0400) ctrl |= 0x8000; // DS X -> special
    if (raw & 0x0800) ctrl |= 0x0800; // DS Y -> run
    return ctrl;
}
void store16(uint8_t *dest, uint16_t word) {
    // Bit-exact little-endian DS word; avoids relying on host aliasing.
    dest[0] = static_cast<uint8_t>(word);
    dest[1] = static_cast<uint8_t>(word >> 8);
}
}
SceneKeys SceneInput::update(const PortPadState &pad, int slot, bool pause_game) {
    if (slot < 0 || slot > 3) return {};
    const uint16_t raw = pause_game ? 0 : native_ds_buttons(pad);
    const uint16_t ctrl = pause_game ? 0 : remap_ctrl(raw);
    SceneKeys keys{};
    keys.ds_held = raw;
    keys.ds_pressed = static_cast<uint16_t>(raw & ~was_raw_[slot]);
    keys.ctrl_held = ctrl;
    keys.ctrl_pressed = static_cast<uint16_t>(ctrl & ~was_ctrl_[slot]);
    was_raw_[slot] = raw;
    was_ctrl_[slot] = ctrl;
    return keys;
}
void SceneInput::reset() {
    std::memset(was_raw_, 0, sizeof was_raw_);
    std::memset(was_ctrl_, 0, sizeof was_ctrl_);
}
bool scene_write_input(const GameInputBuffers &dest, int slot,
                       const SceneKeys &keys) {
    if (slot < 0 || slot > 3 || !dest.ctrl_held || !dest.ctrl_pressed ||
        !dest.pad_held || !dest.pad_pressed || !dest.split_pressed)
        return false;
    store16(dest.ctrl_held + slot * 0x18, keys.ctrl_held);
    store16(dest.ctrl_pressed + slot * 0x18, keys.ctrl_pressed);
    store16(dest.pad_held + slot * 4, keys.ds_held);
    store16(dest.pad_pressed + slot * 4, keys.ds_pressed);
    store16(dest.split_pressed + slot * 4, keys.ds_pressed);
    return true;
}
}
