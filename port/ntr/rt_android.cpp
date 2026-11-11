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
// Shared semantics with the PC rt.cpp HBlank path: a registered IRQ handler
// must see DISPSTAT's HBlank status bit on each scanline, including the 71
// non-visible lines, or wipe transitions never swap their line buffers.
constexpr uint32_t DISPSTAT = 0x04000004u;
constexpr uint32_t WIN0H = 0x04000040u;
constexpr uint32_t WIN0H_SUB = 0x04001040u;
constexpr uint16_t HBLANK_STATUS = 0x0002u;
uint64_t hblank_deliveries = 0, hblank_window_writes = 0, hblank_modeask = 0;
void (*irq_mode_enter)() = nullptr;
void (*irq_mode_exit)() = nullptr;
uint16_t (*irq_pending_flag_read)() = nullptr;

void run_scanlines() {
    for (uint16_t line = 0; line < 263; ++line) {
        r16(VCOUNT) = line;
        // runtime.cpp's original IRQ registry makes this exact 5-gate
        // decision, including handler, IE, IME, DISPSTAT and CPSR I bit.
        if (!rt_hblank_armed()) continue;
        const uint32_t main_before = r32(WIN0H);
        const uint32_t sub_before = r32(WIN0H_SUB);
        r16(DISPSTAT) |= HBLANK_STATUS;
        r32(IF) |= IRQ_HBLANK;
        const bool pending_before =
            irq_pending_flag_read && irq_pending_flag_read() != 0;
        if (irq_mode_enter) irq_mode_enter();
        rt_hblank_dispatch();
        if (irq_mode_exit) irq_mode_exit();
        if (!pending_before && irq_pending_flag_read &&
            irq_pending_flag_read() != 0)
            ++hblank_modeask;
        r32(IF) &= ~IRQ_HBLANK;
        r16(DISPSTAT) &= ~HBLANK_STATUS;
        ++hblank_deliveries;
        if (r32(WIN0H) != main_before || r32(WIN0H_SUB) != sub_before)
            ++hblank_window_writes;
    }
}
} // namespace

void rt_vblank_wait() {
    if (!g.running || !g.host_sp) return;
    r32(IF) |= IRQ_VBLANK;
    sm64ds_arm_switch(&g.game_sp, g.host_sp);
}
void rt_scanout_frame() { run_scanlines(); }
void rt_hblank_counters(unsigned long long *deliveries,
                        unsigned long long *window_writes) {
    if (deliveries) *deliveries = hblank_deliveries;
    if (window_writes) *window_writes = hblank_window_writes;
}
void rt_hblank_modeask(unsigned long long *modeask) {
    if (modeask) *modeask = hblank_modeask;
}
void rt_install_irq_mode_hook(void (*enter)(), void (*exit)(),
                              uint16_t (*pending_flag_read)()) {
    irq_mode_enter = enter;
    irq_mode_exit = exit;
    irq_pending_flag_read = pending_flag_read;
}
void rt_irq_boot_state() {
    // This seat reproduces the original boot IRQ init (as the PC rt.cpp
    // does): VBlank enabled and IME set, without overwriting game-enabled
    // HBlank/GXFIFO bits in the IE register.
    r32(IME) = 1;
    r32(IE) |= IRQ_VBLANK;
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
    rt_irq_boot_state();
    bool stop = false;
    while (!g.finished && !stop) {
        sm64ds_arm_switch(&g.host_sp, g.game_sp);
        if (g.finished) break;
        rt_scanout_frame();
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
