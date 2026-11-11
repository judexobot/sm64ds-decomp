#pragma once
#include <cstdint>

namespace sm64ds_android {
// This is the PC HAL's exact expected file layout, generated locally from
// the user's own mounted EUR ASMP cartridge. No assets are in the APK/repo.
enum class AssetStatus : int {
    ok = 0, rom_not_mounted, invalid_destination, invalid_rom_metadata,
    invalid_file_path, bad_overlay, invalid_handle_table, read_failure,
    write_failure
};
struct AssetStats {
    uint32_t named_files = 0;
    uint32_t handles = 0;
    uint64_t extracted_bytes = 0;
};
// Must be called after mount_european_rom, before the game's boot thread.
// Not safe to run concurrently with game reads. File generation may be slow;
// call off the Android UI thread. NEVER package its output in public builds.
AssetStatus prepare_pc_assets(const char *private_root, AssetStats *out = nullptr);
const char *asset_status_text(AssetStatus);
}
