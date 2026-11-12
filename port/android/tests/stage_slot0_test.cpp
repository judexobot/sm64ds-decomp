// Execute the actual PC slot-0 probe adapter with an observed Stage boundary.
// This validates adapter ordering, not real ROM initialization or gameplay.
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdint>
extern "C" {
unsigned char *data_0209f340;
signed char data_0209f2f8 = 2, data_02092110 = -1;
alignas(4) unsigned char data_02092208[52 * 4]{};
void *data_0209f324, *data_0209f5bc;
int port_slot0_rom_init(void *self);
}
static int calls, mode;
static void *expected;
alignas(8) static unsigned char wipes[7 * 0x60]{};
struct Stage { int InitResources(); };
int Stage::InitResources() {
    assert(this == expected);
    ++calls;
    assert(data_02092110 == (mode >= 3 ? 2 : -1));
    unsigned row;
    std::memcpy(&row, data_02092208 + 8, 4);
    assert(row == (mode >= 2 ? static_cast<unsigned>(reinterpret_cast<uintptr_t>(data_0209f340)) : 0));
    data_0209f324 = wipes;
    return 1;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    mode = std::atoi(argv[1]);
    assert(setenv("SM64DS_SLOT0_ROM", argv[1], 1) == 0);
    alignas(8) unsigned char stage[0x9c8]{}, overlay[64]{};
    expected = stage;
    data_0209f340 = overlay;
    // The first call starts without old wipes and installs a new array. The
    // diagnostic must inspect the new objects without dereferencing old null.
    assert(port_slot0_rom_init(stage) == (mode > 0 ? 1 : -2));
    assert(calls == (mode > 0 ? 1 : 0));
    // Call again: verify the adapter forwards each explicit probe invocation.
    // Production still must replace the hosted path before opening this gate.
    assert(port_slot0_rom_init(stage) == (mode > 0 ? 1 : -2));
    assert(calls == (mode > 0 ? 2 : 0));
}
