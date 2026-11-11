// Cross-compiled by CI. Running this executable requires a 32-bit Android
// runtime; compile success alone is not evidence of Galaxy A15 compatibility.
#include "hal/host_call.h"
#include "hal/port_d16.h"
#include "hal/level_handoff_shared.h"
#include "Stage.h"
#include <cassert>
#include <cstddef>
#include <type_traits>

static int __fastcall slot(void *self PORT_EDX, unsigned a, unsigned b) {
    return *static_cast<int *>(self) + a * 10 + b;
}
static void * __fastcall destroy(void *self PORT_EDX) { return self; }
struct PmfProbe { void method(); };
static_assert(sizeof(void *) == 4 && sizeof(unsigned) == 4);
static_assert(sizeof(void (PmfProbe::*)()) == 8);
// Width is necessary, but does NOT establish that ARM's PMF bit encoding is
// mwcc's encoding. ROM PMF records still require the existing dispatch seams.
static_assert(sizeof(Stage) == 0x9c8);
static_assert(offsetof(Stage, mMeshCollider) == sm64ds_port::stage_collision_offset);
static_assert(std::is_same_v<decltype(&slot), int (*)(void *, unsigned, unsigned)>);
static_assert(std::is_same_v<decltype(PORT_D16(destroy)), int (*)(void *)>);
int main() {
    port_dtorcall_verbose = 0;
    int self = 7;
    assert(slot(&self, 2, 3) == 30);
    assert(PORT_D16(destroy)(&self) == static_cast<int>(reinterpret_cast<std::size_t>(&self)));
    assert(port_dtorcall_dispatches == 1);
}
