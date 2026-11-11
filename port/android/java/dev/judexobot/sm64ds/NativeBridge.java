package dev.judexobot.sm64ds;

import android.view.Surface;

/** JNI bridge for the actual game runtime, not an independent game simulator. */
public final class NativeBridge {
    static { System.loadLibrary("sm64ds_android"); }
    private NativeBridge() {}

    public static native void nativeSetInput(
            int buttons, int leftX, int leftY, int rightX, int rightY, int rightTrigger);
    public static native void nativeReleaseInput();
    public static native void nativeSetSurface(Surface surface);
    /** 0: EUR ASMP cart mounted. See port/android/rom_loader.h for errors. */
    public static native int nativeMountRom(String appPrivatePath);
}
