#pragma once
#include <cstdint>
#include "hal/pad_backend.h"

namespace sm64ds_android {

// The DS holds [u16 held, u16 pressed] for each of four players. The Ctrl
// block has the different REMAPPED convention and a 0x18-byte player stride.
// Keep this translation independent of Win32, and use the ROM's actual
// conventions exactly as PC's host_ds_buttons/host_btn_to_raw_keys do.
struct SceneKeys {
    uint16_t ctrl_held = 0;
    uint16_t ctrl_pressed = 0;
    uint16_t ds_held = 0;
    uint16_t ds_pressed = 0;
};
class SceneInput {
public:
    SceneKeys update(const PortPadState &pad, int player_slot,
                     bool pause_game = false);
    void reset();
private:
    uint16_t was_ctrl_[4]{};
    uint16_t was_raw_[4]{};
};
struct GameInputBuffers {
    // Writable 16-bit words inside the ORIGINAL game globals.
    uint8_t *ctrl_held = nullptr;
    uint8_t *ctrl_pressed = nullptr;
    uint8_t *pad_held = nullptr;
    uint8_t *pad_pressed = nullptr;       // PadData[i].pressed
    uint8_t *split_pressed = nullptr;     // IsButtonInputValid alias
};
bool scene_write_input(const GameInputBuffers &dest, int slot,
                       const SceneKeys &keys);
}
