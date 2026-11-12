// Actual PC model primitives and original OAM::Reset, with synthetic state.
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdint>
extern "C" {
extern unsigned char port_oam_main[], data_0209e660;
extern unsigned int data_0209ea74[];
extern int data_0209e664, data_0209e668, data_0209e66c, data_0209e670;
int hal_oam_layout_check();
void MultiCopy_Int(int *, int *, int);
void Matrix4x3_LoadIdentity(int *);
void func_020527e8(int *, int, int, int);
void func_02052514(int *, int *);
void port_model_vram_reset();
extern unsigned data_020a4bc8, data_020a4be8, data_020a4be0, data_020a4bdc;
}
namespace OAM { void Reset(); }
int main() {
    unsetenv("SM64DS_PPU_AUDIT");
    assert(hal_oam_layout_check());
    std::memset(port_oam_main, 0xa5, 1024);
    std::memset(data_0209ea74, 0x5a, 1024);
    // Seed padding separately: Reset copies the whole first 8-byte entry,
    // including padding, along the overlapping forward-copy chain.
    port_oam_main[6] = 0x12; port_oam_main[7] = 0x34;
    data_0209e660 = 0;
    data_0209e664 = data_0209e668 = data_0209e66c = data_0209e670 = 17;
    OAM::Reset();
    const unsigned char expected[] = {0xc0,0,0,0,0,0,0x12,0x34};
    for (unsigned i = 0; i < 128; ++i)
        assert(std::memcmp(port_oam_main + 8 * i, expected, 8) == 0);
    assert(std::memcmp(port_oam_main, data_0209ea74, 1024) == 0);
    assert(!data_0209e664 && !data_0209e668 && !data_0209e66c && !data_0209e670);
    data_0209e660 = 1;
    std::memset(port_oam_main, 0xa5, 1024);
    std::memset(data_0209ea74, 0x5a, 1024);
    data_0209e66c = data_0209e670 = 17;
    OAM::Reset();
    const unsigned char single[] = {0xc0,0,0,0,0,0,0xa5,0xa5};
    for (unsigned i = 0; i < 128; ++i) {
        assert(std::memcmp(port_oam_main + 8 * i, single, 8) == 0);
        assert(data_0209ea74[i * 2] == 0x5a5a5a5a);
    }
    assert(!data_0209e664 && !data_0209e668 && data_0209e66c == 17 && data_0209e670 == 17);
    int matrix[16];
    std::memset(matrix, 0x7f, sizeof matrix);
    Matrix4x3_LoadIdentity(matrix);
    for (int i = 0; i < 12; ++i) assert(matrix[i] == (i == 0 || i == 4 || i == 8 ? 4096 : 0));
    func_020527e8(matrix, 1, 2, 3);
    for (int i = 0; i < 12; ++i) assert(matrix[i] == (i == 0 ? 1 : i == 4 ? 2 : i == 8 ? 3 : 0));
    int input[] = {1,2,3,4,5,6,7,8,9};
    func_02052514(input, matrix);
    const int expanded[] = {1,2,3,0,4,5,6,0,7,8,9,0,0,0,0,4096};
    assert(std::memcmp(matrix, expanded, sizeof matrix) == 0);
    // The byte-copy fallback must also preserve forward overlap.
    unsigned char unaligned[] = {1,2,3,4,5,6,7,8};
    MultiCopy_Int(reinterpret_cast<int *>(unaligned), reinterpret_cast<int *>(unaligned + 1), 7);
    for (auto byte : unaligned) assert(byte == 1);
    data_020a4bc8 = data_020a4be8 = data_020a4be0 = data_020a4bdc = 13;
    port_model_vram_reset();
    assert(data_020a4bc8 == 0 && data_020a4be8 == 0x20000 &&
           data_020a4be0 == 0x20000 && data_020a4bdc == 0x40000);
}
