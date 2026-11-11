#include "rom_loader.h"
#include "ntr/fs.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>
namespace sm64ds_android {
namespace {
uint32_t rd32(const uint8_t *p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
uint16_t rd16(const uint8_t *p) {
    return uint16_t(p[0] | (unsigned(p[1]) << 8));
}
bool good_span(uint32_t start, uint32_t size, uint64_t total) {
    return start >= 0x200 && size && start <= total && size <= total - start;
}
bool read_at(std::FILE *f, uint32_t offset, void *dst, size_t bytes) {
    return std::fseek(f, static_cast<long>(offset), SEEK_SET) == 0 &&
           std::fread(dst, 1, bytes, f) == bytes;
}
}
RomStatus mount_european_rom(const char *path, RomInfo *info) {
    if (info) *info = {};
    ntr::fs_unmount();
    if (!path || !*path) return RomStatus::missing_path;
    std::FILE *f = std::fopen(path, "rb");
    if (!f) return RomStatus::cannot_open;
    auto fail = [&](RomStatus status) { std::fclose(f); return status; };
    if (std::fseek(f, 0, SEEK_END) != 0)
        return fail(RomStatus::unreadable);
    const long len = std::ftell(f);
    if (len < 0x200 || static_cast<uint64_t>(len) > 0xFFFFFFFFu)
        return fail(RomStatus::unreadable);
    std::array<uint8_t, 512> h{};
    if (!read_at(f, 0, h.data(), h.size()))
        return fail(RomStatus::unreadable);
    if (std::memcmp(h.data() + 0x0C, "ASM", 3) != 0)
        return fail(RomStatus::wrong_game);
    if (h[0x0F] != 'P') return fail(RomStatus::wrong_region);
    const uint32_t fnt = rd32(h.data() + 0x40);
    const uint32_t fnt_size = rd32(h.data() + 0x44);
    const uint32_t fat = rd32(h.data() + 0x48);
    const uint32_t fat_size = rd32(h.data() + 0x4C);
    if (!good_span(fnt, fnt_size, len) || fnt_size < 8 ||
        !good_span(fat, fat_size, len) || fat_size % 8 != 0 ||
        fat_size / 8 > 65536)
        return fail(RomStatus::bad_filesystem);
    std::array<uint8_t, 8> root{};
    if (!read_at(f, fnt, root.data(), root.size()))
        return fail(RomStatus::unreadable);
    if (rd32(root.data()) >= fnt_size ||
        rd16(root.data() + 4) > fat_size / 8)
        return fail(RomStatus::bad_filesystem);
    std::vector<uint8_t> fat_bytes(fat_size);
    if (!read_at(f, fat, fat_bytes.data(), fat_bytes.size()))
        return fail(RomStatus::unreadable);
    for (uint32_t i = 0; i < fat_size; i += 8) {
        const uint32_t first = rd32(fat_bytes.data() + i);
        const uint32_t last = rd32(fat_bytes.data() + i + 4);
        if (last < first || last > static_cast<uint64_t>(len))
            return fail(RomStatus::bad_filesystem);
    }
    std::fclose(f);
    if (!ntr::fs_mount_rom(path)) return RomStatus::mount_failed;
    if (info) {
        info->file_count = uint32_t(ntr::fs_file_count());
        info->size_bytes = uint32_t(len);
    }
    return RomStatus::ok;
}
const char *rom_status_text(RomStatus result) {
    switch (result) {
        case RomStatus::ok: return "European ASMP ROM mounted";
        case RomStatus::missing_path: return "Missing ROM path";
        case RomStatus::cannot_open: return "Cannot open ROM";
        case RomStatus::unreadable: return "Cannot read ROM";
        case RomStatus::wrong_game: return "Not Super Mario 64 DS";
        case RomStatus::wrong_region: return "Requires EUR ASMP ROM";
        case RomStatus::bad_filesystem: return "Invalid NDS file system";
        case RomStatus::mount_failed: return "NTR filesystem mount failed";
    }
    return "Unknown ROM error";
}
}
