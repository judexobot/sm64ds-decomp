package dev.judexobot.sm64ds;

import android.content.Context;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.FrameLayout;

/**
 * The screen + touch layer shared by adventure and minigames.
 * Android lifecycle supplies the native window; the game's native frame
 * callback presents its real GX/PPU composite through ANativeWindow.
 */
public final class NativeScreenLayout extends FrameLayout implements SurfaceHolder.Callback {
    private final SurfaceView surface;
    private final TouchControlsView controls;
    private int renderScale = 2;

    public NativeScreenLayout(Context context) {
        super(context);
        surface = new SurfaceView(context);
        controls = new TouchControlsView(context);
        addView(surface, new LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        addView(controls, new LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        surface.getHolder().addCallback(this);
    }
    public TouchControlsView controls() { return controls; }
    public void setRenderScale(int scale) {
        renderScale = Math.max(1, Math.min(4, scale));
        if (getWidth() > 0 && getHeight() > 0)
            NativeBridge.nativeConfigureDisplay(getWidth(), getHeight(), renderScale);
    }
    public void onPause() { controls.setVisibleControls(false); }
    public void onResume() { controls.setVisibleControls(true); }

    @Override public void surfaceCreated(SurfaceHolder holder) {
        NativeBridge.nativeSetSurface(holder.getSurface());
    }
    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        NativeBridge.nativeSetSurface(holder.getSurface());
        NativeBridge.nativeConfigureDisplay(width, height, renderScale);
    }
    @Override public void surfaceDestroyed(SurfaceHolder holder) {
        controls.setVisibleControls(false);
        NativeBridge.nativeSetSurface(null);
    }
    @Override protected void onDetachedFromWindow() {
        surface.getHolder().removeCallback(this);
        NativeBridge.nativeSetSurface(null);
        super.onDetachedFromWindow();
    }
}
