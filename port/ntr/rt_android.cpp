// ARMv7 native VBlank scheduler. One thread, two cooperatively-switched stacks.
#include "ntr/rt.h"
#include "ntr/mmio.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

#if !defined(__ANDROID__) || !defined(__arm__) || !defined(__ARM_ARCH_7A__)
#error "Android ARMv7 (armeabi-v7a) required"
#endif
extern "C" void sm64ds_arm_switch(void **save_sp, void *restore_sp);

namespace ntr {
namespace {
constexpr uint32_t IME = 0x04000208, IE = 0x04000210;
constexpr uint32_t IF = 0x04000214, VCOUNT = 0x04000006;
constexpr size_t STACK_BYTES = 1024 * 1024;
struct Runtime {
    void *host_sp = nullptr, *game_sp = nullptr, *mapping = nullptr;
    size_t mapping_bytes = 0;
    void (*entry)() = nullptr;
    uint64_t frame = 0;
    uint32_t cpsr_i = 0;
    bool running = false, finished = false;
};
Runtime g;
inline volatile uint32_t &r32(uint32_t addr) {
    return *reinterpret_cast<volatile uint32_t *>(static_cast<uintptr_t>(addr));
}
inline volatile uint16_t &r16(uint32_t addr) {
    return *reinterpret_cast<volatile uint16_t *>(static_cast<uintptr_t>(addr));
}
void trampoline() {
    g.entry();
    g.finished = true;
    for (;;) sm64ds_arm_switch(&g.game_sp, g.host_sp);
}
bool allocate_game_stack() {
    const long page = sysconf(_SC_PAGESIZE);
    if (page <= 0) return false;
    g.mapping_bytes = STACK_BYTES + static_cast<size_t>(page);
    g.mapping = mmap(nullptr, g.mapping_bytes, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g.mapping == MAP_FAILED) { g.mapping = nullptr; return false; }
    if (mprotect(g.mapping, static_cast<size_t>(page), PROT_NONE) != 0) {
        munmap(g.mapping, g.mapping_bytes);
        g.mapping = nullptr;
        return false;
    }
    uintptr_t top = reinterpret_cast<uintptr_t>(g.mapping) + g.mapping_bytes;
    top &= ~static_cast<uintptr_t>(7);
    // The assembly switch restores r4-r11 + PC, then SP is 8-byte aligned.
    uint32_t *saved = reinterpret_cast<uint32_t *>(top) - 9;
    std::memset(saved, 0, 9 * sizeof(uint32_t));
    saved[8] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&trampoline));
    g.game_sp = saved;
    return true;
}
void run_scanlines() {
    for (uint16_t line = 0; line < 263; ++line) r16(VCOUNT) = line;
}
} // namespace

void rt_vblank_wait() {
    if (!g.running || !g.host_sp) return;
    r32(IF) |= IRQ_VBLANK;
    sm64ds_arm_switch(&g.game_sp, g.host_sp);
}
uint64_t rt_frame() { return g.frame; }
uint32_t rt_irq_disable() {
    const uint32_t old = g.cpsr_i; g.cpsr_i = 0x80; return old;
}
uint32_t rt_irq_enable() {
    const uint32_t old = g.cpsr_i; g.cpsr_i = 0; return old;
}
uint32_t rt_irq_restore(uint32_t previous) {
    const uint32_t old = g.cpsr_i; g.cpsr_i = previous & 0x80; return old;
}
bool rt_irq_masked() { return g.cpsr_i != 0; }
uint64_t rt_run(void (*game)(), FrameHook hook, uint64_t max_frames) {
    if (!game || g.running || !io_init()) return 0;
    g = Runtime{};
    g.entry = game;
    if (!allocate_game_stack()) {
        std::fprintf(stderr, "rt_run: game stack allocation failed\n");
        return 0;
    }
    g.running = true;
    r32(IME) = 1;
    r32(IE) = IRQ_VBLANK;
    bool stop = false;
    while (!g.finished && !stop) {
        sm64ds_arm_switch(&g.host_sp, g.game_sp);
        if (g.finished) break;
        run_scanlines();
        ++g.frame;
        if (hook && !hook(g.frame)) stop = true;
        if (max_frames && g.frame >= max_frames) stop = true;
        r32(IF) &= ~IRQ_VBLANK;
    }
    const uint64_t frames = g.frame;
    // Safe: the game is suspended and no other thread touches its stack.
    munmap(g.mapping, g.mapping_bytes);
    g = Runtime{};
    return frames;
}
} // namespace ntr
