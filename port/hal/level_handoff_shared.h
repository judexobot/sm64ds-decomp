#pragma once
#include <cstddef>

extern "C" {
int port_level_host_boot_target(void);
int port_level_is_mounted(int level);
void *port_stage_create(void);
void port_stage_a2_seat(void);
void *port_stage_a_boot(void *collider, int spawn);
}
namespace sm64ds_port {
// The original Stage owns its collider. game_abi_test pins this offset against
// the real Stage header; walk_window uses this same address calculation.
inline constexpr std::size_t stage_collision_offset = 0x91c;
inline void *stage_collision(void *stage) {
    return stage ? static_cast<char *>(stage) + stage_collision_offset : nullptr;
}
enum class LevelBootStatus { booted, invalid_target, unmounted, failed };
struct LevelBoot {
    LevelBootStatus status;
    void *stage;
    void *level_data;
};
inline LevelBoot title_boot_committed_level(int level) {
    // Never fall back to the environment's default level after StartFile.
    if (level < 0 || port_level_host_boot_target() != level)
        return {LevelBootStatus::invalid_target, nullptr, nullptr};
    if (!port_level_is_mounted(level))
        return {LevelBootStatus::unmounted, nullptr, nullptr};
    void *stage = port_stage_create();
    if (!stage) return {LevelBootStatus::failed, nullptr, nullptr};
    port_stage_a2_seat();
    void *data = port_stage_a_boot(stage_collision(stage), 1);
    return {data ? LevelBootStatus::booted : LevelBootStatus::failed, stage, data};
}
}
