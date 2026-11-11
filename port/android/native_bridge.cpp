#include "touch_input.h"
#include "android_present.h"
#include "rom_loader.h"
#include "ntr/ppu.h"
#include <jni.h>

// Native bridge is intentionally tied to the real gameplay seams and the
// local ROM. It does not run a fake screen or bypass the game boot.
extern "C" JNIEXPORT void JNICALL
Java_dev_judexobot_sm64ds_NativeBridge_nativeSetInput(
        JNIEnv *, jclass, jint buttons, jint lx, jint ly,
        jint rx, jint ry, jint rt) {
    sm64ds_android::touch_set(static_cast<uint16_t>(buttons), lx, ly, rx, ry, rt);
}
extern "C" JNIEXPORT void JNICALL
Java_dev_judexobot_sm64ds_NativeBridge_nativeReleaseInput(JNIEnv *, jclass) {
    sm64ds_android::touch_clear();
}
extern "C" JNIEXPORT void JNICALL
Java_dev_judexobot_sm64ds_NativeBridge_nativeSetSurface(
        JNIEnv *env, jclass, jobject surface) {
    sm64ds_android::set_surface(env, surface);
}
extern "C" JNIEXPORT jint JNICALL
Java_dev_judexobot_sm64ds_NativeBridge_nativeMountRom(
        JNIEnv *env, jclass, jstring path) {
    if (!path) return static_cast<jint>(sm64ds_android::RomStatus::missing_path);
    const char *utf = env->GetStringUTFChars(path, nullptr);
    if (!utf) return static_cast<jint>(sm64ds_android::RomStatus::cannot_open);
    auto result = sm64ds_android::mount_european_rom(utf);
    env->ReleaseStringUTFChars(path, utf);
    return static_cast<jint>(result);
}

extern "C" JNIEXPORT void JNICALL
Java_dev_judexobot_sm64ds_NativeBridge_nativeSetStylus(
        JNIEnv *, jclass, jboolean down, jint x, jint y) {
    sm64ds_android::stylus_set(down == JNI_TRUE, x, y);
}
extern "C" JNIEXPORT void JNICALL
Java_dev_judexobot_sm64ds_NativeBridge_nativeConfigureDisplay(
        JNIEnv *, jclass, jint width, jint height, jint scale) {
    if (width > 0 && height > 0) {
        // Preserve the real PC port's runtime aspect/scale behavior.
        ntr::configure_aspect(double(width) / double(height),
                              scale >= 1 && scale <= 4 ? scale : 2);
    }
}
