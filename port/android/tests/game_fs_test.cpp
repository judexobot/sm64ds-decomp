// Execute PC's actual SharedFilePtr::Load/card seam with synthetic files.
// Allocator observations isolate file loading; this does not test game heaps.
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace Memory {
static unsigned allocations;
void *Allocate(unsigned size) { ++allocations; return std::malloc(size); }
void *Allocate(unsigned size, int align) {
    assert(align >= 4); return Allocate(size);
}
void Deallocate(void *ptr) { std::free(ptr); }
}
extern "C" {
struct port_arc_entry { uint16_t base, end; const char *narc; };
port_arc_entry port_archive_map[13]{}; // no NARC interiors in this fixture
struct FilePtr { uint16_t fileID; uint8_t refs; void *bytes; };
void *_ZN13SharedFilePtr4LoadEv(FilePtr *);
uint16_t func_02018a24(unsigned);
extern unsigned long port_fs_loads, port_fs_load_miss;
}
static void write(const std::filesystem::path &path, const char *data, unsigned size) {
    std::ofstream output(path, std::ios::binary);
    output.write(data, size); assert(output.good());
}
int main() {
    char temporary[] = "/tmp/sm64ds-game-fs-XXXXXX";
    const char *created = mkdtemp(temporary); assert(created);
    const std::filesystem::path root(created);
    std::filesystem::create_directories(root / "build/assets");
    std::filesystem::create_directories(root / "extracted/dsd/files");
    const char files[] = "id\tsize\tpath\t\n3\t4\tplain.bin\t\n4\t13\tcompressed.bin\t\n";
    const char handles[] = "handle\taddress\tfile_id\n0\t0\t3\n1\t0\t4\n";
    const char compressed[] = {'L','Z','7','7',0x10,8,0,0,0x20,'A','B',0x30,1};
    write(root / "build/assets/files.tsv", files, sizeof files - 1);
    write(root / "build/assets/handles.tsv", handles, sizeof handles - 1);
    write(root / "extracted/dsd/files/plain.bin", "DATA", 4);
    write(root / "extracted/dsd/files/compressed.bin", compressed, sizeof compressed);
    assert(setenv("SM64DS_ASSET_ROOT", created, 1) == 0);
    assert(func_02018a24(0) == 3 && func_02018a24(1) == 4);
    assert(func_02018a24(0x8007) == 0x8007);
    FilePtr plain{3, 0, nullptr}, packed{4, 0, nullptr};
    void *first = _ZN13SharedFilePtr4LoadEv(&plain);
    assert(first && std::memcmp(first, "DATA", 4) == 0 && plain.bytes == first);
    std::memset(first, 0, 4); // a caller cannot mutate the resident master
    std::filesystem::remove(root / "extracted/dsd/files/plain.bin");
    void *second = _ZN13SharedFilePtr4LoadEv(&plain);
    assert(second && first != second && std::memcmp(second, "DATA", 4) == 0);
    void *decoded = _ZN13SharedFilePtr4LoadEv(&packed);
    assert(decoded && std::memcmp(decoded, "ABABABAB", 8) == 0);
    assert(Memory::allocations == 3 && port_fs_loads == 3 && port_fs_load_miss == 2);
    Memory::Deallocate(first); Memory::Deallocate(second); Memory::Deallocate(decoded);
    std::filesystem::remove_all(root);
}
