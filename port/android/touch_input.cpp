#include "touch_input.h"
#include <cstring>
#include <mutex>

namespace sm64ds_android {
namespace {
std::mutex lock;
PortPadState state = {};
int clamp_axis(int n) { return n < -32767 ? -32767 : (n > 32767 ? 32767 : n); }
int clamp_trigger(int n) { return n < 0 ? 0 : (n > 255 ? 255 : n); }
}
void touch_set(uint16_t buttons, int lx, int ly, int rx, int ry, int rt) {
    std::lock_guard<std::mutex> guard(lock);
    ++state.packet;
    state.buttons = buttons;
    state.lt = 0;
    state.rt = static_cast<unsigned char>(clamp_trigger(rt));
    state.lx = static_cast<short>(clamp_axis(lx));
    state.ly = static_cast<short>(clamp_axis(ly));
    state.rx = static_cast<short>(clamp_axis(rx));
    state.ry = static_cast<short>(clamp_axis(ry));
}
void touch_clear() { touch_set(0, 0, 0, 0, 0, 0); }
bool touch_poll(PortPadState *out) {
    if (!out) return false;
    std::lock_guard<std::mutex> guard(lock);
    *out = state;
    return true;
}
}

// On the Android build these symbols replace the Win32 XInput/DirectInput
// implementation without modifying the game's consumers.
int port_pad_init(void) { sm64ds_android::touch_clear(); return 1; }
void port_pad_device_changed(void) {}
int port_pad_poll(PortPadState *out) {
    return sm64ds_android::touch_poll(out) ? 1 : 0;
}
const char *port_pad_describe(void) { return "Android touchscreen"; }
int port_pad_raw(PortPadRaw *out) {
    if (out) std::memset(out, 0, sizeof *out);
    return 0; // No DirectInput hardware on Android.
}
int port_pad_set_layout(const HostPadLayout *) { return 0; }
int port_pad_selftest(void) { return 1; }
