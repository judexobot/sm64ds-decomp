#include "assets_extract.h"
#include "rom_loader.h"
#include "ntr/fs.h"
#include <array>
#include <algorithm>
#include <iterator>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

namespace {
void w16(std::vector<uint8_t> &v, size_t at, unsigned value) {
    v[at] = value & 255; v[at+1] = (value >> 8) & 255;
}
void w32(std::vector<uint8_t> &v, size_t at, unsigned value) {
    for (int i = 0; i < 4; ++i) v[at+i] = (value >> (i*8)) & 255;
}
const size_t OV0 = 0x300, OVLEN = 0x7000, ASSET = OV0+OVLEN;
std::vector<uint8_t> fake_rom() {
    // This completely synthetic, copyright-free cartridge has the original
    // required metadata STRUCTURE but no original Nintendo program or assets.
    std::vector<uint8_t> rom(ASSET+4, 0);
    std::memcpy(rom.data() + 0x0C, "ASMP", 4);
    w32(rom, 0x40, 0x200); w32(rom, 0x44, 34);
    w32(rom, 0x48, 0x240); w32(rom, 0x4C, 16);
    w32(rom, 0x50, 0x260); w32(rom, 0x54, 32);
    // FNT directory records, one root subdir named "data".
    w32(rom, 0x200, 16); w16(rom, 0x204, 1); w16(rom, 0x206, 2);
    w32(rom, 0x208, 24); w16(rom, 0x20C, 1); w16(rom, 0x20E, 0xF000);
    rom[0x210] = 0x84; std::memcpy(rom.data()+0x211, "data", 4);
    w16(rom, 0x215, 0xF001); rom[0x217] = 0;
    rom[0x218] = 8; std::memcpy(rom.data()+0x219, "test.bmd", 8);
    rom[0x221] = 0;
    // FAT: file 0 overlay image; file 1 is the named BMD test blob.
    w32(rom, 0x240, OV0); w32(rom, 0x244, OV0+OVLEN);
    w32(rom, 0x248, ASSET); w32(rom, 0x24C, ASSET+4);
    // Overlay 0 metadata: its RAM base covers 0x020BD4B8.
    const unsigned ram = 0x020BD000;
    w32(rom, 0x260, 0); w32(rom, 0x264, ram);
    w32(rom, 0x268, OVLEN); w32(rom, 0x278, 0);
    for (unsigned i = 0; i < 0x80A; ++i)
        w32(rom, OV0 + 0x4B8 + i*4u, ram + 0x3000);
    std::memcpy(rom.data() + OV0 + 0x3000, "data/test.bmd", 14);
    std::memcpy(rom.data() + ASSET, "BMD0", 4);
    return rom;
}

// Construct a BLZ-compressed overlay 0 without any Nintendo data: an intact
// 0x6000-byte prefix (including the original handle table), followed by 4096
// repeated zero bytes encoded as backward-LZ literals and distance-3 matches.
// This exercises the actual compressed ROM path, not just raw overlays.
std::vector<uint8_t> compressed_rom() {
    const auto image = fake_rom();
    struct Token { bool backref; int length; };
    std::vector<Token> tokens;
    int remaining = 0x1000;
    for (int i = 0; i < 3; ++i) { tokens.push_back({false, 1}); --remaining; }
    while (remaining >= 3) {
        const int len = std::min(18, remaining);
        tokens.push_back({true, len});
        remaining -= len;
    }
    while (remaining > 0) {
        tokens.push_back({false, 1});
        --remaining;
    }
    std::vector<std::vector<uint8_t>> groups;
    for (size_t start = 0; start < tokens.size(); start += 8) {
        std::vector<uint8_t> group;
        const int count = int(std::min(size_t(8), tokens.size() - start));
        uint8_t flags = 0;
        for (int j = 0; j < count; ++j)
            if (tokens[start + j].backref) flags |= uint8_t(0x80u >> j);
        for (int j = count - 1; j >= 0; --j) {
            const auto &token = tokens[start + j];
            if (token.backref) {
                group.push_back(0); // displacement 3
                group.push_back(uint8_t((token.length - 3) << 4));
            } else group.push_back(0);
        }
        group.push_back(flags);
        groups.push_back(std::move(group));
    }
    std::vector<uint8_t> overlay(image.begin() + OV0, image.begin() + OV0 + 0x6000);
    for (auto it = groups.rbegin(); it != groups.rend(); ++it)
        overlay.insert(overlay.end(), it->begin(), it->end());
    const uint32_t compressed_length = uint32_t(overlay.size() - 0x6000 + 8);
    overlay.resize(overlay.size() + 8);
    w32(overlay, overlay.size() - 8, (8u << 24) | compressed_length);
    w32(overlay, overlay.size() - 4, unsigned(OVLEN - overlay.size()));
    std::vector<uint8_t> rom(image.begin(), image.begin() + OV0);
    rom.insert(rom.end(), overlay.begin(), overlay.end());
    const unsigned asset_offset = unsigned(rom.size());
    rom.insert(rom.end(), image.begin() + ASSET, image.end());
    w32(rom, 0x244, asset_offset);
    w32(rom, 0x248, asset_offset);
    w32(rom, 0x24C, asset_offset + 4);
    w32(rom, 0x278, 0x01000000u | unsigned(overlay.size()));
    return rom;
}

bool write_file(const std::string &path, const std::vector<uint8_t> &v) {
    std::FILE *file = std::fopen(path.c_str(), "wb");
    if (!file) return false;
    const bool ok = std::fwrite(v.data(), 1, v.size(), file) == v.size();
    return std::fclose(file) == 0 && ok;
}
std::string read_text(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}
}
int main() {
    char folder[] = "/tmp/sm64ds-assets-XXXXXX";
    char *tmp = mkdtemp(folder);
    assert(tmp != nullptr);
    const std::string base = std::string(tmp);
    assert(mkdir((base + "/game").c_str(), 0700) == 0);
    const std::string rom = base + "/game/sm64ds_eur.nds";
    auto image = fake_rom();
    assert(write_file(rom, image));
    sm64ds_android::RomInfo info{};
    assert(sm64ds_android::mount_european_rom(rom.c_str(), &info) ==
           sm64ds_android::RomStatus::ok);
    assert(info.file_count == 2);
    assert(ntr::fs_id_by_path("data/test.bmd") == 1);
    sm64ds_android::AssetStats st{};
    const std::string target = base + "/game/pc-assets";
    auto status = sm64ds_android::prepare_pc_assets(rom.c_str(), target.c_str(), &st);
    if (status != sm64ds_android::AssetStatus::ok)
        std::fprintf(stderr, "asset error: %s\n",
                     sm64ds_android::asset_status_text(status));
    assert(status == sm64ds_android::AssetStatus::ok);
    assert(st.named_files == 1 && st.handles == 0x80A && st.extracted_bytes == 4);
    assert(read_text(target+"/extracted/dsd/files/data/test.bmd") == "BMD0");
    const auto files = read_text(target+"/build/assets/files.tsv");
    assert(files.find("1\t0x0001\tdata/test.bmd\tmodel\t4") != std::string::npos);
    const auto handles = read_text(target+"/build/assets/handles.tsv");
    assert(handles.find("2057\t0x0809\t1\t0x0001\tdata/test.bmd\tmodel\t4") !=
           std::string::npos);
    const auto metadata = read_text(target+"/build/assets/nitrofs.tsv");
    assert(metadata.find("ovt9_offset\t608") != std::string::npos);
    assert(read_text(target+"/build/assets/nitrofs_fnt.bin").size() == 34);
    assert(read_text(target+"/build/assets/nitrofs_ovt9.bin").size() == 32);
    assert(!read_text(target+"/build/assets/android_extract.ready").empty());

    // A BLZ-compressed overlay decodes the same full handle table and assets.
    image = compressed_rom();
    assert(write_file(rom, image));
    assert(sm64ds_android::mount_european_rom(rom.c_str()) ==
           sm64ds_android::RomStatus::ok);
    assert(sm64ds_android::prepare_pc_assets(rom.c_str(), target.c_str(), &st) ==
           sm64ds_android::AssetStatus::ok);
    assert(st.handles == 0x80A && st.extracted_bytes == 4);
    assert(read_text(target+"/extracted/dsd/files/data/test.bmd") == "BMD0");

    // Wrong version of the handle table must refuse, NOT fabricate a map.
    image = fake_rom();
    w32(image, OV0 + 0x4B8, 0xDEADBEEFu);
    assert(write_file(rom, image));
    assert(sm64ds_android::mount_european_rom(rom.c_str()) ==
           sm64ds_android::RomStatus::ok);
    assert(sm64ds_android::prepare_pc_assets(rom.c_str(), target.c_str()) ==
           sm64ds_android::AssetStatus::invalid_handle_table);
    // A failed import never leaves an old ready marker from a previous ROM.
    assert(read_text(target+"/build/assets/android_extract.ready").empty());

    // A traversal path inside the ROM must not escape private storage.
    image = fake_rom();
    std::memcpy(image.data()+0x219, "../x.bmd", 8);
    assert(write_file(rom, image));
    assert(sm64ds_android::mount_european_rom(rom.c_str()) ==
           sm64ds_android::RomStatus::ok);
    assert(sm64ds_android::prepare_pc_assets(rom.c_str(), target.c_str()) ==
           sm64ds_android::AssetStatus::invalid_file_path);
    std::puts("ASMP PC HAL native assets: FNT/FAT, 2058 handles, images and guards passed");
    return 0;
}
