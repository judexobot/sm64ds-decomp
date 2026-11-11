// Super Mario 64 DS PC HAL asset-layout preparation on Android.
// All output originates from a user-selected game cartridge in private storage.
// No hardcoded game asset bytes, no host Python, no repository-extracted data.
#include "assets_extract.h"
#include "blz.h"
#include "ntr/fs.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

namespace sm64ds_android {
namespace {
constexpr uint32_t TABLE_RAM_ADDRESS = 0x020BD4B8u;
constexpr uint32_t HANDLE_COUNT = 0x80Au; // tools/asset_catalog.py, EUR ASMP
constexpr size_t MAX_OVERLAY = 16u * 1024u * 1024u;
constexpr size_t MAX_FILE = 64u * 1024u * 1024u;
uint32_t rd32(const uint8_t *p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8)
         | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
bool span(uint32_t p, uint32_t n, uint64_t total) {
    return p >= 0x200 && n > 0 && p <= total && n <= total - p;
}
bool open_read(const char *path, std::vector<uint8_t> *out) {
    std::FILE *file = std::fopen(path, "rb");
    if (!file) return false;
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file); return false;
    }
    const long len = std::ftell(file);
    if (len < 0 || uint64_t(len) > 128u * 1024u * 1024u ||
            std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file); return false;
    }
    out->resize(static_cast<size_t>(len));
    const bool success = std::fread(out->data(), 1, out->size(), file) == out->size();
    std::fclose(file);
    return success;
}
bool good_part(const std::string &part) {
    if (part.empty() || part == "." || part == ".." || part.size() > 120)
        return false;
    for (unsigned char c : part)
        if (c <= 0x20 || c >= 0x7f || c == ':' || c == '\\' || c == '/')
            return false;
    return true;
}
bool valid_path(const std::string &path) {
    if (path.empty() || path.size() > 240 || path[0] == '/') return false;
    size_t pos = 0;
    while (pos < path.size()) {
        const size_t sep = path.find('/', pos);
        if (!good_part(path.substr(pos, sep == std::string::npos
                                        ? sep : sep - pos)))
            return false;
        if (sep == std::string::npos) break;
        pos = sep + 1;
    }
    return path.back() != '/';
}
bool mkdir_one(const std::string &path) {
    if (mkdir(path.c_str(), 0700) == 0) return true;
    if (errno != EEXIST) return false;
    struct stat info{};
    return lstat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}
bool mkdirs(const std::string &path) {
    if (path.empty()) return false;
    size_t pos = (path[0] == '/') ? 1 : 0;
    while (pos < path.size()) {
        pos = path.find('/', pos);
        if (pos == std::string::npos) return mkdir_one(path);
        if (pos != 0 && !mkdir_one(path.substr(0, pos))) return false;
        ++pos;
    }
    return true;
}
bool atomic_write(const std::string &path, const uint8_t *data, size_t length) {
    auto parent = path.substr(0, path.find_last_of('/'));
    if (parent.empty() || !mkdirs(parent)) return false;
    const std::string temp = path + ".part";
    std::FILE *f = std::fopen(temp.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(data, 1, length, f) == length &&
                    std::fflush(f) == 0 && fsync(fileno(f)) == 0;
    const int closed = std::fclose(f);
    if (!ok || closed != 0 || rename(temp.c_str(), path.c_str()) != 0) {
        unlink(temp.c_str());
        return false;
    }
    return true;
}
bool write_string(const std::string &path, const std::string &data) {
    return atomic_write(path, reinterpret_cast<const uint8_t *>(data.data()),
                        data.size());
}
const char *kind(const std::string &path) {
    const auto extpos = path.rfind('.');
    const std::string ext = extpos == std::string::npos ? "" : path.substr(extpos);
    if (ext == ".bca") return "animation";
    if (ext == ".bmd") return "model";
    if (ext == ".btp") return "texture-sequence";
    if (ext == ".kcl") return "collision";
    if (ext == ".narc") return "archive";
    if (ext == ".sdat") return "sound-archive";
    if (ext == ".bin") return "data";
    return "file";
}
std::string hex4(uint32_t n) {
    char t[16]; std::snprintf(t, sizeof t, "0x%04x", n); return t;
}
bool rom_span(const std::vector<uint8_t> &rom, uint32_t off, uint32_t bytes) {
    return span(off, bytes, rom.size());
}


struct Entry {
    uint32_t id = 0;
    std::string path;
    std::vector<uint8_t> data;
};
struct Handle { uint32_t id = 0; uint32_t file_id = 0; };

AssetStatus get_handles(const std::vector<uint8_t> &rom,
                        const std::vector<Entry> &files,
                        std::vector<Handle> *handles) {
    const uint32_t ovt_off = rd32(rom.data() + 0x50);
    const uint32_t ovt_size = rd32(rom.data() + 0x54);
    if (!rom_span(rom, ovt_off, ovt_size) || ovt_size % 32 != 0)
        return AssetStatus::invalid_rom_metadata;
    uint32_t ov0_offset = 0, ov0_size = 0, ram_addr = 0, ram_size = 0, flags = 0;
    for (uint32_t off = ovt_off; off < ovt_off + ovt_size; off += 32) {
        const uint8_t *row = rom.data() + off;
        if (rd32(row) != 0) continue;
        ram_addr = rd32(row + 4);
        ram_size = rd32(row + 8);
        const uint32_t fid = rd32(row + 24);
        flags = rd32(row + 28) >> 24;
        if (fid >= rd32(rom.data() + 0x4C) / 8)
            return AssetStatus::bad_overlay;
        const uint32_t fat_off = rd32(rom.data() + 0x48) + fid * 8;
        ov0_offset = rd32(rom.data() + fat_off);
        const uint32_t ov0_end = rd32(rom.data() + fat_off + 4);
        if (ov0_end < ov0_offset) return AssetStatus::bad_overlay;
        ov0_size = ov0_end - ov0_offset;
        break;
    }
    if (!ram_addr || !ram_size || !ov0_size || ov0_size > MAX_OVERLAY ||
        ram_size > MAX_OVERLAY || !rom_span(rom, ov0_offset, ov0_size))
        return AssetStatus::bad_overlay;
    std::vector<uint8_t> overlay(rom.begin() + ov0_offset,
                                 rom.begin() + ov0_offset + ov0_size);
    if (flags & 1) {
        if (!expand_blz(&overlay, ram_size))
            return AssetStatus::bad_overlay;
    }
    if (TABLE_RAM_ADDRESS < ram_addr ||
        uint64_t(TABLE_RAM_ADDRESS - ram_addr) + HANDLE_COUNT * 4u > overlay.size())
        return AssetStatus::invalid_handle_table;
    const size_t handle_offset = TABLE_RAM_ADDRESS - ram_addr;
    for (uint32_t i = 0; i < HANDLE_COUNT; ++i) {
        const uint32_t pointer = rd32(overlay.data() + handle_offset + i * 4u);
        if (pointer < ram_addr || uint64_t(pointer - ram_addr) >= overlay.size())
            return AssetStatus::invalid_handle_table;
        const size_t start = pointer - ram_addr;
        const auto null = std::find(overlay.begin() + start, overlay.end(), uint8_t(0));
        if (null == overlay.end() || null - (overlay.begin() + start) > 240)
            return AssetStatus::invalid_handle_table;
        const std::string path(reinterpret_cast<const char *>(overlay.data() + start),
                               static_cast<size_t>(null - (overlay.begin() + start)));
        if (!valid_path(path)) return AssetStatus::invalid_handle_table;
        // Mirror the Python catalog's exact overlay path lookup.
        auto target = std::find_if(files.begin(), files.end(),
            [&](const Entry &f) { return f.path == path; });
        if (target == files.end()) return AssetStatus::invalid_handle_table;
        handles->push_back({i, target->id});
    }
    return AssetStatus::ok;
}
} // namespace

AssetStatus prepare_pc_assets(const char *rom_path, const char *private_root, AssetStats *out) {
    if (out) *out = {};
    if (!ntr::fs_mounted()) return AssetStatus::rom_not_mounted;
    if (!rom_path || !*rom_path || rom_path[0] != '/' ||
        !private_root || !*private_root || private_root[0] != '/' ||
        std::strstr(private_root, "/../") || std::strstr(private_root, "/./") ||
        std::strlen(private_root) > 512)
        return AssetStatus::invalid_destination;
    const std::string root(private_root);
    if (!mkdirs(root)) return AssetStatus::write_failure;

    // Read the explicitly selected private cartridge path, not a guessed
    // sibling of the output folder. This also supports custom app layouts.
    const std::string ready = root + "/build/assets/android_extract.ready";
    unlink(ready.c_str());  // never leave a stale "complete" marker on failure
    std::vector<uint8_t> rom;
    if (!open_read(rom_path, &rom) || rom.size() < 0x200)
        return AssetStatus::read_failure;
    if (std::memcmp(rom.data() + 0x0C, "ASMP", 4) != 0)
        return AssetStatus::invalid_rom_metadata;
    const uint32_t fnt = rd32(rom.data() + 0x40), fnt_size = rd32(rom.data() + 0x44);
    const uint32_t fat = rd32(rom.data() + 0x48), fat_size = rd32(rom.data() + 0x4C);
    if (!rom_span(rom, fnt, fnt_size) || !rom_span(rom, fat, fat_size) ||
        fat_size % 8 || fat_size / 8 != uint32_t(ntr::fs_file_count()))
        return AssetStatus::invalid_rom_metadata;
    const int count = ntr::fs_file_count();
    if (count <= 0 || count > 65536) return AssetStatus::invalid_rom_metadata;
    std::vector<Entry> files;
    files.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        const char *p = ntr::fs_path(i);
        if (!p || !*p) continue; // overlays have FAT entries, no FNT names
        Entry e;
        e.id = static_cast<uint32_t>(i);
        e.path = p;
        if (!valid_path(e.path)) return AssetStatus::invalid_file_path;
        const int size = ntr::fs_size(i);
        if (size < 0 || static_cast<size_t>(size) > MAX_FILE)
            return AssetStatus::read_failure;
        files.push_back(std::move(e));
    }
    std::vector<Handle> handles;
    AssetStatus status = get_handles(rom, files, &handles);
    if (status != AssetStatus::ok) return status;

    std::string catalog = "file_id\thex_id\tpath\tkind\tsize\n";
    for (auto &file : files) {
        const int size = ntr::fs_size(static_cast<int32_t>(file.id));
        catalog += std::to_string(file.id) + "\t" + hex4(file.id) + "\t" +
                   file.path + "\t" + kind(file.path) + "\t" + std::to_string(size) + "\n";
    }
    std::string handle_catalog = "handle\thex_handle\tfile_id\thex_file_id\tpath\tkind\tsize\n";
    for (auto h : handles) {
        auto f = std::find_if(files.begin(), files.end(),
            [&](const Entry &e) { return e.id == h.file_id; });
        handle_catalog += std::to_string(h.id) + "\t" + hex4(h.id) + "\t" +
            std::to_string(h.file_id) + "\t" + hex4(h.file_id) + "\t" +
            f->path + "\t" + kind(f->path) + "\t" +
            std::to_string(ntr::fs_size(static_cast<int32_t>(h.file_id))) + "\n";
    }
    std::string nitrofs = "key\tvalue\n";
    const std::array<const char *, 8> keys = {"fnt_offset", "fnt_size",
        "fat_offset", "fat_size", "ovt9_offset", "ovt9_size", "ovt7_offset", "ovt7_size"};
    const std::array<uint32_t, 8> values = {fnt, fnt_size, fat, fat_size,
        rd32(rom.data() + 0x50), rd32(rom.data() + 0x54),
        rd32(rom.data() + 0x58), rd32(rom.data() + 0x5C)};
    for (size_t i = 0; i < 8; ++i)
        nitrofs += std::string(keys[i]) + "\t" + std::to_string(values[i]) + "\n";

    const std::string assets = root + "/build/assets/";
    if (!mkdirs(assets)) return AssetStatus::write_failure;
    for (int i = 0; i < 4; ++i) {
        if (values[2 * i + 1] == 0 && values[2 * i] == 0 && i >= 2)
            continue;
        if (!rom_span(rom, values[2 * i], values[2 * i + 1]))
            return AssetStatus::invalid_rom_metadata;
        const char *name[] = {"nitrofs_fnt.bin", "nitrofs_fat.bin",
                              "nitrofs_ovt9.bin", "nitrofs_ovt7.bin"};
        if (!atomic_write(assets + name[i], rom.data() + values[2 * i],
                          values[2 * i + 1]))
            return AssetStatus::write_failure;
    }
    if (!write_string(assets + "nitrofs.tsv", nitrofs) ||
        !write_string(assets + "files.tsv", catalog) ||
        !write_string(assets + "handles.tsv", handle_catalog))
        return AssetStatus::write_failure;

    uint64_t total = 0;
    const std::string dest = root + "/extracted/dsd/files/";
    for (const Entry &e : files) {
        const int sz = ntr::fs_size(static_cast<int32_t>(e.id));
        std::vector<uint8_t> contents(static_cast<size_t>(sz));
        if (sz && ntr::fs_read(static_cast<int32_t>(e.id),
                              contents.data(), sz) != sz)
            return AssetStatus::read_failure;
        if (!atomic_write(dest + e.path, contents.data(), contents.size()))
            return AssetStatus::write_failure;
        total += contents.size();
    }
    // Extract actual ARM9 overlay images with their ROM-assigned IDs, using
    // the same BLZ decoder used above for overlay 0. These are the source
    // images expected by the PC port's romdata.recipe.tsv pipeline. No ARM
    // program bytes are published to GitHub; all stay in app-private storage.
    const uint32_t ovt9_off = rd32(rom.data() + 0x50);
    const uint32_t ovt9_size = rd32(rom.data() + 0x54);
    if (!rom_span(rom, ovt9_off, ovt9_size) || ovt9_size % 32u != 0 ||
        ovt9_size / 32u > 512u)
        return AssetStatus::invalid_rom_metadata;
    const std::string overlays_folder = root + "/extracted/overlays/";
    if (!mkdirs(overlays_folder)) return AssetStatus::write_failure;
    std::vector<uint32_t> seen_ids;
    for (uint32_t off = ovt9_off; off < ovt9_off + ovt9_size; off += 32) {
        const uint8_t *row = rom.data() + off;
        const uint32_t id = rd32(row);
        const uint32_t ram_size = rd32(row + 8);
        const uint32_t fid = rd32(row + 24);
        const uint32_t flags = rd32(row + 28) >> 24;
        if (id > 9999 || fid >= uint32_t(count) ||
            ram_size == 0 || ram_size > MAX_OVERLAY ||
            std::find(seen_ids.begin(), seen_ids.end(), id) != seen_ids.end())
            return AssetStatus::bad_overlay;
        seen_ids.push_back(id);
        const uint32_t image_begin = rd32(rom.data() + fat + fid * 8u);
        const uint32_t image_end = rd32(rom.data() + fat + fid * 8u + 4u);
        if (image_end < image_begin ||
            !rom_span(rom, image_begin, image_end - image_begin) ||
            image_end - image_begin > MAX_OVERLAY)
            return AssetStatus::bad_overlay;
        std::vector<uint8_t> image(rom.begin() + image_begin,
                                   rom.begin() + image_end);
        if ((flags & 1u) != 0 && !expand_blz(&image, ram_size))
            return AssetStatus::bad_overlay;
        if (image.size() > ram_size) return AssetStatus::bad_overlay;
        char name[48];
        std::snprintf(name, sizeof name, "overlay_%04u.bin", id);
        if (!atomic_write(overlays_folder + name, image.data(), image.size()))
            return AssetStatus::write_failure;
    }

    // Also retain the ARM9/ARM7 sections from the user's ROM. ARM9 may
    // require the separate BLZ/postfix step before romdata.recipe can use it;
    // do not mislabel this raw compressed image as arm9_dec.bin.
    const uint32_t arm9_off = rd32(rom.data() + 0x20);
    const uint32_t arm9_size = rd32(rom.data() + 0x2c);
    const uint32_t arm7_off = rd32(rom.data() + 0x30);
    const uint32_t arm7_size = rd32(rom.data() + 0x3c);
    if (arm9_off || arm9_size) {
        if (!rom_span(rom, arm9_off, arm9_size) ||
            !atomic_write(root + "/extracted/arm9.bin",
                          rom.data() + arm9_off, arm9_size))
            return AssetStatus::invalid_rom_metadata;
    }
    if (arm7_off || arm7_size) {
        if (!rom_span(rom, arm7_off, arm7_size) ||
            !atomic_write(root + "/extracted/arm7.bin",
                          rom.data() + arm7_off, arm7_size))
            return AssetStatus::invalid_rom_metadata;
    }

    // Do not create a marker until all files and catalogs are written.
    const std::string marker = "asset-catalog-v1\t" + std::to_string(files.size()) +
                               "\t" + std::to_string(handles.size()) + "\t" +
                               std::to_string(seen_ids.size()) + "\n";
    if (!write_string(assets + "android_extract.ready", marker))
        return AssetStatus::write_failure;
    if (out) {
        out->named_files = static_cast<uint32_t>(files.size());
        out->handles = static_cast<uint32_t>(handles.size());
        out->extracted_bytes = total;
    }
    return AssetStatus::ok;
}
const char *asset_status_text(AssetStatus result) {
    switch (result) {
        case AssetStatus::ok: return "PC catalogs and NitroFS assets extracted";
        case AssetStatus::rom_not_mounted: return "Mount the user's EUR ROM first";
        case AssetStatus::invalid_destination: return "App-private asset root required";
        case AssetStatus::invalid_rom_metadata: return "ROM FAT/FNT/overlay metadata is invalid";
        case AssetStatus::invalid_file_path: return "Unsafe path in ROM FNT";
        case AssetStatus::bad_overlay: return "Cannot decode ROM overlay 0";
        case AssetStatus::invalid_handle_table: return "EUR overlay resource handle table mismatch";
        case AssetStatus::read_failure: return "Cannot read ROM or asset";
        case AssetStatus::write_failure: return "Cannot save extracted game assets";
    }
    return "Unknown asset extraction error";
}
} // namespace sm64ds_android
