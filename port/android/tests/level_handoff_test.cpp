#include "hal/level_handoff_shared.h"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace sm64ds_port;
static int target = 1;
static bool mounted = true, allocate = true, load = true;
static char stage[0x9c8], level[32];
static std::vector<int> calls;
extern "C" {
int port_level_host_boot_target() { calls.push_back(1); return target; }
int port_level_is_mounted(int n) { calls.push_back(2); assert(n == target); return mounted; }
void *port_stage_create() { calls.push_back(3); return allocate ? stage : nullptr; }
void port_stage_a2_seat() { calls.push_back(4); }
void *port_stage_a_boot(void *collider, int spawn) {
    calls.push_back(5);
    assert(collider == stage + 0x91c && spawn == 1);
    return load ? level : nullptr;
}
}
int main() {
    auto boot = title_boot_committed_level(1);
    assert(boot.status == LevelBootStatus::booted && boot.stage == stage && boot.level_data == level);
    assert((calls == std::vector<int>{1,2,3,4,5}));
    calls.clear();
    assert(title_boot_committed_level(-1).status == LevelBootStatus::invalid_target);
    assert(calls.empty());
    assert(title_boot_committed_level(2).status == LevelBootStatus::invalid_target);
    assert((calls == std::vector<int>{1}));
    calls.clear(); mounted = false;
    assert(title_boot_committed_level(1).status == LevelBootStatus::unmounted);
    assert((calls == std::vector<int>{1,2}));
    calls.clear(); mounted = true; allocate = false;
    assert(title_boot_committed_level(1).status == LevelBootStatus::failed);
    assert((calls == std::vector<int>{1,2,3}));
    calls.clear(); allocate = true; load = false;
    assert(title_boot_committed_level(1).status == LevelBootStatus::failed);
    assert((calls == std::vector<int>{1,2,3,4,5}));
    std::puts("Stage boot order, collider, full spawn and target refusal: PASS (observation hooks)");
}
