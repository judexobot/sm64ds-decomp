#include "scene_input.h"
#include "hal/scene_frame_shared.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace sm64ds_android;

namespace {
int events[10]{}, event_count=0, seen_frame=-1;
void mark(int event) { events[event_count++] = event; }
}
extern "C" {
void port_rom_frame_begin(const char *) { mark(1); }
int port_rom_frame_checked(int frame, const char *) { seen_frame = frame; return frame; }
void port_scene_tick(int frame, int enabled) {
    assert(frame == seen_frame); assert(enabled == 1); mark(2);
}
void sdat_host_tick(void) { mark(3); }
void port_rom_frame_phase6(void) { mark(4); }
}
static uint16_t rd16(const uint8_t *bytes) {
    return uint16_t(bytes[0]) | uint16_t(bytes[1] << 8);
}
int main() {
    SceneInput input;
    PortPadState pad{};
    std::array<uint8_t, 4*0x18> ctrl{}, ctrl_pressed{};
    std::array<uint8_t, 4*4> raw{}, raw_pressed{}, split{};
    GameInputBuffers mem{ctrl.data(), ctrl_pressed.data(),
                         raw.data(), raw_pressed.data(), split.data()};
    assert(!scene_write_input(mem, -1, {}));
    assert(!scene_write_input(mem, 4, {}));

    // Android Xbox A/B/X, stick and Start enter exactly the game's raw PadData.
    pad.buttons = 0x1000 | 0x2000 | 0x4000 | 0x0010;
    pad.lx = 24000;
    SceneKeys keys = input.update(pad, 0);
    assert(keys.ds_held == (0x0002 | 0x0001 | 0x0800 | 0x0008 | 0x0010));
    assert(keys.ctrl_held == (0x0002 | 0x0001 | 0x0800));
    assert(keys.ds_pressed == keys.ds_held);
    assert(keys.ctrl_pressed == keys.ctrl_held);
    assert(scene_write_input(mem, 0, keys));
    assert(rd16(raw.data()) == keys.ds_held);
    assert(rd16(raw_pressed.data()) == keys.ds_pressed);
    assert(rd16(split.data()) == keys.ds_pressed);
    assert(rd16(ctrl.data()) == keys.ctrl_held);
    assert(rd16(ctrl_pressed.data()) == keys.ctrl_pressed);

    // Held buttons are not repeatedly new presses; releases propagate.
    keys = input.update(pad, 0);
    assert(keys.ds_pressed == 0 && keys.ctrl_pressed == 0);
    keys = input.update(PortPadState{}, 0);
    assert(keys.ds_held == 0 && keys.ctrl_held == 0);
    assert(scene_write_input(mem, 0, keys));
    assert(rd16(raw.data()) == 0 && rd16(split.data()) == 0);

    // The three bytes that matter for minigames: X/Y, Select, R2 crouch.
    pad = {};
    pad.buttons = 0x8000 | 0x0020 | 0x0001;
    pad.rt = 200;
    keys = input.update(pad, 2);
    assert(keys.ds_held == (0x0400 | 0x0004 | 0x0040 | 0x0100));
    assert(keys.ctrl_held == (0x8000 | 0x0400));
    assert(scene_write_input(mem, 2, keys));
    assert(rd16(raw.data()+8) == keys.ds_held);
    assert(rd16(ctrl.data()+2*0x18) == keys.ctrl_held);
    assert(rd16(split.data()+8) == keys.ds_pressed);

    // A pause releases everything, but does not invent a held edge.
    keys = input.update(pad, 2, true);
    assert(keys.ds_held == 0 && keys.ds_pressed == 0);
    keys = input.update(pad, 2);
    assert(keys.ds_pressed == keys.ds_held);

    // The extracted actual PC scene frame order: scene -> present -> foot.
    sm64ds_port::scene_frame_begin("real scene");
    sm64ds_port::scene_frame_simulate(42, true);
    mark(5); // platform-specific frame present between game and phase-6
    sm64ds_port::scene_frame_complete(true);
    assert(event_count == 4);
    assert(events[0] == 1 && events[1] == 2 &&
           events[2] == 5 && events[3] == 4);
    // A paused frame is handed to the same ROM scene and then pumps ARM7.
    event_count = 0;
    // Only the paused frame's sound is invoked in complete().
    sm64ds_port::scene_frame_complete(false);
    assert(event_count == 2 && events[0] == 3 && events[1] == 4);
    std::puts("Android touch => original scene PadData, Ctrl edges and frame order passed");
}
