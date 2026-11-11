#pragma once
#include <cstdint>

namespace sm64ds_android {
// NOT a fake renderer. These link-time seams invoke the original game's
// port_scene_begin, port_scene_tick and framebuffer from hal/scene_boot.cpp.
// Compile this module in CI as an object/archive; a playable lib needs the
// entire PC game closure (currently blocked on Windows-specific sources).
enum class SceneGameStatus {
    ok, already_started, no_assets, game_boot_failed, not_started,
    frame_missing, no_surface, level_handoff_required
};
SceneGameStatus scene_game_begin(const char *asset_root);
SceneGameStatus scene_game_frame();
int scene_game_frame_count();
void scene_game_stop_input();
}
