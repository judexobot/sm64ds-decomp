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
    /** Native DS lower-screen stylus (0..255 by 0..191). */
    public static native void nativeSetStylus(boolean down, int x, int y);
    /** Port PC aspect ratio / render scale; call before the first game frame. */
    public static native void nativeConfigureDisplay(int width, int height, int renderScale);
    /** Create PC HAL files.tsv, handles.tsv and original NitroFS files off the UI thread. */
    public static native int nativePrepareAssets(String appPrivateRom, String appPrivateRoot);
    /** Point the PC game's original fs.cpp/sdat/romdata loaders at private assets.
     * Call only after verifying the kit recipe and before starting gameplay. */
    public static native boolean nativeSetAssetRoot(String appPrivateRoot);
    /** 0: EUR ASMP cart mounted. See port/android/rom_loader.h for errors. */
    public static native int nativeMountRom(String appPrivatePath);
}
