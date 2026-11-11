#pragma once
#include <cstdint>
namespace sm64ds_android {
enum class RomStatus { ok, missing_path, cannot_open, unreadable, wrong_game,
                       wrong_region, bad_filesystem, mount_failed };
struct RomInfo { uint32_t file_count = 0, size_bytes = 0; };
// Pass an app-private local path copied from Android's document picker.
// Existing NTR backend mounts FNT/FAT directly; no Nintendo assets are bundled.
RomStatus mount_european_rom(const char *path, RomInfo *info = nullptr);
const char *rom_status_text(RomStatus result);
}
