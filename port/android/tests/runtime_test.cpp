#include "ntr/gx.h"
#include "ntr/lz77.h"
#include "ntr/rt.h"
#include "ntr/mmio.h"
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

// The real ntr/runtime.cpp IRQ registry, not a stand-in.
extern "C" void _ZN3IRQ13SetIRQHandlerEjPFvvE(unsigned, void (*)());
extern "C" unsigned _ZN3IRQ10EnableIRQsEj(unsigned);
extern "C" unsigned _ZN3IRQ11DisableIRQsEj(unsigned);
namespace {
int hblank_calls = 0;
void hblank_handler() {
    ++hblank_calls;
    // The ROM's fade handler writes window bounds during HBlank.
    *reinterpret_cast<volatile uint32_t *>(0x04000040u) =
        static_cast<uint32_t>(hblank_calls);
    // The game's own handler may disarm in the middle of a frame.
    if (hblank_calls == 11) _ZN3IRQ11DisableIRQsEj(ntr::IRQ_HBLANK);
}
void guest_hblank() {
    *reinterpret_cast<volatile uint16_t *>(0x04000004u) |= 0x10u;
    _ZN3IRQ13SetIRQHandlerEjPFvvE(ntr::IRQ_HBLANK, hblank_handler);
    _ZN3IRQ10EnableIRQsEj(ntr::IRQ_HBLANK);
    ntr::rt_vblank_wait();  // 11 deliveries, then disarmed
    _ZN3IRQ10EnableIRQsEj(ntr::IRQ_HBLANK);
    ntr::rt_vblank_wait();  // full 263 scanlines
}
} // namespace

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
    unsigned long long before = 0, before_windows = 0;
    ntr::rt_hblank_counters(&before, &before_windows);
    if (ntr::rt_run(guest_hblank, nullptr, 2) != 2 ||
        hblank_calls != 274)
        return 4;
    unsigned long long after = 0, after_windows = 0;
    ntr::rt_hblank_counters(&after, &after_windows);
    if (after - before != 274 || after_windows - before_windows != 274)
        return 5;
    if (*reinterpret_cast<volatile uint16_t *>(0x04000006u) != 262u ||
        (*reinterpret_cast<volatile uint16_t *>(0x04000004u) & 0x0002u))
        return 6;
        std::puts("ARMv7 NTR self-test passed");
    return 0;
}
