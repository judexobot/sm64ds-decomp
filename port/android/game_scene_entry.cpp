// Direct adapter to the ACTUAL scene/title/minigame loop in the PC port.
// This is deliberately NOT a self-contained game/demo. It cannot be linked
// into the JNI .so until all hal/scene_boot.cpp's game-side dependencies exist
// on Android, and the matching ROM-clean recipe is packaged.
#include "game_scene_entry.h"
#include "scene_input.h"
#include "touch_input.h"
#include "android_present.h"
#include "hal/scene_frame_shared.h"
#include "ntr/ppu.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

extern "C" {
void port_scene_layout_propose(void);
int port_scene_begin(void *hwnd, int zoom);
const void *port_scene_framebuffer(void);
void port_host_keys_set(unsigned short raw);
extern unsigned char data_0209f250;     // local slot, normally 0
extern char data_0209f49c[];            // Ctrl held (0x18 stride)
extern char data_0209f49e[];            // Ctrl pressed (0x18 stride)
extern unsigned short data_020a0e58[];  // PadData held+pressed, 4-byte stride
extern unsigned short data_020a0e5a[];  // split PadData pressed alias
int port_title_entry_should_stop(void); // original ROM file-select -> stage request
int hal_sub_screen_stacked(void);
unsigned int *hal_sub_screen_stacked_image(const unsigned int *top);
void hal_sub_screen_stacked_size(int *w, int *h);
}
namespace sm64ds_android {
namespace {
bool started = false;
bool handoff_pending = false;
int current_frame = 0;
SceneInput scene_input;
}
SceneGameStatus scene_game_begin(const char *asset_root) {
    if (started) return SceneGameStatus::already_started;
    if (!asset_root || asset_root[0] != '/')
        return SceneGameStatus::no_assets;
    const std::string root(asset_root);
    struct stat ready{};
    if (stat((root + "/build/assets/romdata.ready").c_str(), &ready) != 0 ||
        !S_ISREG(ready.st_mode))
        return SceneGameStatus::no_assets;
    if (setenv("SM64DS_ASSET_ROOT", asset_root, 1) != 0)
        return SceneGameStatus::no_assets;

    // The PC default-scene resolver chooses title scene 1. Never replace
    // this with a mock/title image or silently skip its real InitResources.
    port_scene_layout_propose();
    const int result = port_scene_begin(nullptr, 1);
    if (result) return SceneGameStatus::game_boot_failed;
    sm64ds_port::scene_frame_begin("Android original ROM scene loop");
    scene_input.reset();
    current_frame = 0;
    handoff_pending = false;
    started = true;
    return SceneGameStatus::ok;
}
SceneGameStatus scene_game_frame() {
    if (!started) return SceneGameStatus::not_started;
    if (handoff_pending) return SceneGameStatus::level_handoff_required;
    const auto *fb = static_cast<const ntr::Framebuffer *>(port_scene_framebuffer());
    if (!fb) return SceneGameStatus::frame_missing;

    PortPadState pad{};
    if (!touch_poll(&pad)) return SceneGameStatus::frame_missing;
    const int slot = data_0209f250 & 3;
    const SceneKeys keys = scene_input.update(pad, slot);
    GameInputBuffers buffers{};
    buffers.ctrl_held = reinterpret_cast<uint8_t *>(data_0209f49c);
    buffers.ctrl_pressed = reinterpret_cast<uint8_t *>(data_0209f49e);
    buffers.pad_held = reinterpret_cast<uint8_t *>(data_020a0e58);
    buffers.pad_pressed = reinterpret_cast<uint8_t *>(data_020a0e58) + 2;
    buffers.split_pressed = reinterpret_cast<uint8_t *>(data_020a0e5a);
    if (!scene_write_input(buffers, slot, keys))
        return SceneGameStatus::frame_missing;
    port_host_keys_set(keys.ds_held);

    sm64ds_port::scene_frame_simulate(current_frame, true);
    bool presented = false;
    if (hal_sub_screen_stacked()) {
        unsigned int *full =
            hal_sub_screen_stacked_image(&fb->px[0][0]);
        if (full) {
            int w = 0, h = 0;
            hal_sub_screen_stacked_size(&w, &h);
            presented = present_frame(full, w, h, w);
        }
    }
    if (!presented) {
        presented = present_frame(&fb->px[0][0], ntr::active_w,
                                  ntr::active_h, ntr::SCREEN_W);
    }
    sm64ds_port::scene_frame_complete(true);
    ++current_frame;
    // The original PC title exits its scene loop AFTER the frame foot when
    // file select requests Stage (id 3) and the scene has torn itself down.
    // Do not tick a dead scene, fake a level or silently restart the title.
    // The full native level boot must be linked before consuming the handoff.
    if (port_title_entry_should_stop()) {
        handoff_pending = true;
        return SceneGameStatus::level_handoff_required;
    }
    return presented ? SceneGameStatus::ok : SceneGameStatus::no_surface;
}
int scene_game_frame_count() { return current_frame; }
void scene_game_stop_input() {
    touch_clear();
    scene_input.reset();
    // The next tick publishes a release to all game-facing fields.
}
}
