#include "hal/title_handoff_shared.h"
#include <cassert>
#include <cstdio>
#include <vector>

static std::vector<int> calls;
static int commit_result = 0;
static int finish_frames = -1;
extern "C" {
void port_rom_frame_report(void) { calls.push_back(1); }
int port_scene_finish(int frame) {
    calls.push_back(2);
    finish_frames = frame;
    return 37;
}
int port_title_entry_commit(void) {
    calls.push_back(3);
    return commit_result;
}
}
int main() {
    commit_result = 1;
    auto entered = sm64ds_port::title_finish_and_commit(25);
    assert(entered.entered_level);
    assert(entered.scene_result == 37 && finish_frames == 25);
    assert((calls == std::vector<int>{1,2,3}));
    calls.clear();
    commit_result = 0; // ROM did NOT stage a real level
    auto refused = sm64ds_port::title_finish_and_commit(300);
    assert(!refused.entered_level);
    assert(refused.scene_result == 37 && finish_frames == 300);
    assert((calls == std::vector<int>{1,2,3}));
    std::puts("original PC title: frame report -> scene finish -> ROM level commit verified");
}
