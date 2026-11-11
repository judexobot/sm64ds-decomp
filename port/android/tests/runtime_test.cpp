#include "ntr/gx.h"
#include "ntr/lz77.h"
#include "ntr/rt.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {
int waits = 0, hooks = 0;
void guest() {
    for (int i = 0; i < 4; ++i) { ++waits; ntr::rt_vblank_wait(); }
}
bool frame_hook(uint64_t frame) { ++hooks; return frame <= 4; }
uint32_t pack(int16_t x, int16_t y) {
    return static_cast<uint16_t>(x) |
           (static_cast<uint32_t>(static_cast<uint16_t>(y)) << 16);
}
void vertex(int16_t x, int16_t y, int16_t z) {
    ntr::gx_write_port(0x0400048Cu, pack(x, y));
    ntr::gx_write_port(0x0400048Cu, static_cast<uint16_t>(z));
}
}
int main() {
    const uint8_t input[] = {0x10, 3, 0, 0, 0, 'D', 'S', '!'};
    uint8_t out[3] = {};
    if (ntr::lz77_unpack(input, sizeof(input), out, sizeof(out)) != 3 ||
        std::memcmp(out, "DS!", 3) != 0) return 1;
    ntr::gx_reset();
    ntr::gx_write_port(0x04000500u, 0u); // triangle list
    ntr::gx_write_port(0x04000480u, 0x001Fu);
    vertex(-1024, -1024, 0);
    vertex(1024, -1024, 0);
    vertex(0, 1024, 0);
    size_t count = 0;
    ntr::gx_polygons(count);
    if (count != 1) return 2;
    if (ntr::rt_run(guest, frame_hook, 8) != 4 || waits != 4 || hooks != 4)
        return 3;
    std::puts("ARMv7 NTR self-test passed");
    return 0;
}
