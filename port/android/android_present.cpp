#include "android_present.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <algorithm>
#include <cstdint>
#include <mutex>

namespace sm64ds_android {
namespace {
std::mutex mutex;
ANativeWindow *window = nullptr;

// AARRGGBB native host pixel -> 0xAABBGGRR little-endian RGBA_8888.
uint32_t rgba(uint32_t argb) {
    return (argb & 0xFF00FF00u) |
           ((argb & 0x00FF0000u) >> 16) |
           ((argb & 0x000000FFu) << 16);
}
}
void set_surface(JNIEnv *env, jobject surface) {
    ANativeWindow *replacement = surface
        ? ANativeWindow_fromSurface(env, surface) : nullptr;
    std::lock_guard<std::mutex> guard(mutex);
    if (window) ANativeWindow_release(window);
    window = replacement;
    if (window) ANativeWindow_setBuffersGeometry(window, 0, 0, WINDOW_FORMAT_RGBA_8888);
}
bool present_frame(const uint32_t *argb, int width, int height, int stride) {
    if (!argb || width <= 0 || height <= 0 || stride < width) return false;
    std::lock_guard<std::mutex> guard(mutex);
    if (!window) return false;
    ANativeWindow_Buffer buf{};
    if (ANativeWindow_lock(window, &buf, nullptr) != 0) return false;
    bool result = false;
    if (buf.format == WINDOW_FORMAT_RGBA_8888 && buf.bits && buf.width > 0 && buf.height > 0) {
        const float scale = std::min(float(buf.width) / width, float(buf.height) / height);
        const int dw = std::max(1, int(width * scale));
        const int dh = std::max(1, int(height * scale));
        const int left = (buf.width - dw) / 2;
        const int top = (buf.height - dh) / 2;
        auto *dst = static_cast<uint32_t *>(buf.bits);
        for (int y = 0; y < buf.height; ++y) {
            uint32_t *row = dst + y * buf.stride;
            const int sy = (y - top) * height / dh;
            for (int x = 0; x < buf.width; ++x) {
                if (x < left || y < top || x >= left + dw || y >= top + dh) {
                    row[x] = 0xFF000000u;
                } else {
                    const int sx = (x - left) * width / dw;
                    row[x] = rgba(argb[sy * stride + sx]);
                }
            }
        }
        result = true;
    }
    ANativeWindow_unlockAndPost(window);
    return result;
}
}
