#include "touch_input.h"
#include <cassert>
#include <cstdio>
int main() {
    PortPadState p{};
    assert(port_pad_init() == 1);
    assert(port_pad_poll(&p) == 1 && p.buttons == 0);
    const auto before = p.packet;
    sm64ds_android::touch_set(
        sm64ds_android::UP | sm64ds_android::A | sm64ds_android::L,
        3000, 40000, 0, -40000, 280);
    assert(port_pad_poll(&p) == 1);
    assert(p.packet > before);
    assert((p.buttons & 0x1101) == 0x1101);
    assert(p.ly == 32767 && p.ry == -32767 && p.rt == 255);
    sm64ds_android::touch_clear();
    assert(port_pad_poll(&p) == 1 && p.buttons == 0);
    assert(p.lx == 0 && p.ly == 0 && p.rt == 0);
    std::puts("Touch and XInput-compatible pad snapshots passed");
}
