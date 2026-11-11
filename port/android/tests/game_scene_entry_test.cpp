// Run the Android adapter's actual state machine with explicit observations at
// unavailable game/window seams. This is a transition test, not a ROM boot.
#include "game_scene_entry.h"
#include "android_present.h"
#include "ntr/ppu.h"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

static bool commit = true, dead = false;
static int ticks, finishes, commits, boots;
static std::vector<int> handoff;
static ntr::Framebuffer framebuffer;
static char stage[0x9c8], level[32];
namespace ntr { int active_w = 256, active_h = 192; }
extern "C" {
unsigned char data_0209f250;
char data_0209f49c[4 * 0x18], data_0209f49e[4 * 0x18];
unsigned short data_020a0e58[8], data_020a0e5a[8];
void port_scene_layout_propose() {}
int port_scene_begin(void *, int zoom) { assert(zoom == 1); return 0; }
const void *port_scene_framebuffer() { return &framebuffer; }
void port_host_keys_set(unsigned short) {}
void port_rom_frame_begin(const char *) {}
int port_rom_frame_checked(int frame, const char *) { return frame; }
void port_rom_frame_phase6() {}
void port_scene_tick(int, int tick) { assert(!dead && tick); ++ticks; }
void sdat_host_tick() { assert(false); }
int port_title_entry_should_stop() { return ticks == 2; }
void port_rom_frame_report() { handoff.push_back(1); }
int port_scene_finish(int n) { assert(n == 2); dead = true; ++finishes; handoff.push_back(2); return 0; }
int port_title_entry_commit() { assert(dead); ++commits; handoff.push_back(3); return commit; }
int port_level_host_boot_target() { assert(dead && commit); return 1; }
int port_level_is_mounted(int n) { assert(n == 1); return 1; }
void *port_stage_create() { handoff.push_back(4); return stage; }
void port_stage_a2_seat() { handoff.push_back(5); }
void *port_stage_a_boot(void *mc, int spawn) {
    assert(mc == stage + 0x91c && spawn == 1);
    ++boots; handoff.push_back(6); return level;
}
int hal_sub_screen_stacked() { return 0; }
unsigned *hal_sub_screen_stacked_image(const unsigned *) { assert(false); return nullptr; }
void hal_sub_screen_stacked_size(int *, int *) { assert(false); }
}
namespace sm64ds_android {
bool present_frame(const uint32_t *, int w, int h, int stride) {
    assert(w == ntr::active_w && h == ntr::active_h && stride == ntr::SCREEN_W);
    return true;
}
}
int main(int argc, char **) {
    using namespace sm64ds_android;
    commit = argc == 1;
    assert(scene_game_frame() == SceneGameStatus::not_started);
    assert(scene_game_begin(nullptr) == SceneGameStatus::no_assets);
    char root[] = "/tmp/sm64ds-adapter-XXXXXX";
    assert(mkdtemp(root));
    const auto assets = std::filesystem::path(root) / "build/assets";
    std::filesystem::create_directories(assets);
    std::ofstream(assets / "romdata.ready").put('\n'); // synthetic test marker only
    assert(scene_game_begin(root) == SceneGameStatus::ok);
    assert(scene_game_frame() == SceneGameStatus::ok);
    const auto expected = commit ? SceneGameStatus::level_loop_required : SceneGameStatus::handoff_refused;
    assert(scene_game_frame() == expected);
    assert(scene_game_frame() == expected); // never tick a destroyed title/reboot Stage
    assert(ticks == 2 && finishes == 1 && commits == 1 && boots == (commit ? 1 : 0));
    assert(scene_game_pending_level() == (commit ? 1 : -1));
    assert(handoff == (commit ? std::vector<int>{1,2,3,4,5,6} : std::vector<int>{1,2,3}));
    scene_game_stop_input();
    std::filesystem::remove_all(root);
}
