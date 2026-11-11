#pragma once
#include <cstdint>
#include <jni.h>

namespace sm64ds_android {
// Accepts the actual game frame buffer (0xAARRGGBB), NOT a test image.
// Call after the full game's frame/3D+HUD composition is linked to Android.
void set_surface(JNIEnv *env, jobject surface);
bool present_frame(const uint32_t *argb, int width, int height, int stride);
}
