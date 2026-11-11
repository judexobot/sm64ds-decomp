#include "rom_loader.h"
#include "ntr/fs.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
namespace {
void word(std::vector<unsigned char>& v, size_t i, unsigned x) {
    v[i] = x & 255; v[i+1] = (x >> 8) & 255;
    v[i+2] = (x >> 16) & 255; v[i+3] = (x >> 24) & 255;
}
bool save(const char* path, const std::vector<unsigned char>& v) {
    auto* f = std::fopen(path, "wb");
    if (!f) return false;
    bool ok = std::fwrite(v.data(), 1, v.size(), f) == v.size();
    std::fclose(f); return ok;
}
}
int main() {
    const char* path = "synthetic.nds";
    std::vector<unsigned char> rom(1024);
    std::memcpy(rom.data() + 0x0C, "ASMP", 4);
    word(rom, 0x40, 0x200); word(rom, 0x44, 0x20);
    word(rom, 0x48, 0x280); word(rom, 0x4C, 8);
    word(rom, 0x200, 8); rom[0x206] = 1;
    rom[0x208] = 8; std::memcpy(rom.data() + 0x209, "test.bin", 8);
    word(rom, 0x280, 0x300); word(rom, 0x284, 0x303);
    std::memcpy(rom.data() + 0x300, "ABC", 3);
    assert(save(path, rom));
    sm64ds_android::RomInfo info;
    assert(sm64ds_android::mount_european_rom(path, &info) ==
           sm64ds_android::RomStatus::ok);
    assert(info.file_count == 1);
    const int id = ntr::fs_id_by_path("test.bin");
    assert(id == 0);
    char output[4] = {};
    assert(ntr::fs_read(id, output, 3) == 3);
    assert(std::memcmp(output, "ABC", 3) == 0);
    ntr::fs_unmount();
    rom[0x0F] = 'E'; assert(save(path, rom));
    assert(sm64ds_android::mount_european_rom(path) ==
           sm64ds_android::RomStatus::wrong_region);
    rom[0x0F] = 'P'; word(rom, 0x284, 0x80000000);
    assert(save(path, rom));
    assert(sm64ds_android::mount_european_rom(path) ==
           sm64ds_android::RomStatus::bad_filesystem);
    std::remove(path);
    std::puts("European ROM NitroFS test passed");
}
