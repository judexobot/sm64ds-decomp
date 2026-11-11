package dev.judexobot.sm64ds;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

/**
 * Transparent multi-touch pad over the game's native SurfaceView.
 * No fake game: events become XInput-compatible pad states through JNI.
 * Touches on multiple controls can be held simultaneously; release/cancel
 * always pushes zeroed state instead of leaving stuck buttons.
 */
public final class TouchControlsView extends View {
    private static final int UP = 0x0001, DOWN = 0x0002, LEFT = 0x0004,
            RIGHT = 0x0008, START = 0x0010, SELECT = 0x0020,
            L = 0x0100, R = 0x0200,
            A = 0x1000, B = 0x2000, X = 0x4000, Y = 0x8000;
    private static final int[] MASKS = {A, B, X, Y, L, R, START, SELECT, 0};
    private static final String[] LABELS = {"A", "B", "X", "Y", "L", "R", "Start", "Select", "R2"};
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF[] pads = new RectF[MASKS.length];
    private final float[] px = new float[16], py = new float[16];
    private final int[] ids = new int[16];
    private int pointers = 0;
    private float stickX, stickY, stickRadius;
    private int width, height;
    private int buttons, xAxis, yAxis, rTrigger;
    private boolean show = true;

    public TouchControlsView(Context context) { this(context, null); }
    public TouchControlsView(Context context, AttributeSet attrs) {
        super(context, attrs);
        for (int i = 0; i < pads.length; i++) pads[i] = new RectF();
        setFocusable(true);
        setLayerType(View.LAYER_TYPE_SOFTWARE, null);
    }
    public void setVisibleControls(boolean visible) {
        show = visible; if (!visible) release(); invalidate();
    }
    private void place(int i, float cx, float cy, float radius) {
        pads[i].set(cx - radius, cy - radius, cx + radius, cy + radius);
    }
    @Override protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        width = w; height = h;
        float unit = Math.min(w / 2.05f, h);
        float rad = unit * .064f;
        stickX = w * .17f; stickY = h * .73f; stickRadius = unit * .15f;
        final float bx = w * .84f, by = h * .73f, gap = rad * 1.65f;
        place(0, bx, by + gap, rad);
        place(1, bx + gap, by, rad);
        place(2, bx - gap, by, rad);
        place(3, bx, by - gap, rad);
        place(4, w * .12f, h * .17f, rad * 1.25f);
        place(5, w * .88f, h * .17f, rad * 1.25f);
        place(6, w * .57f, h * .88f, rad * .95f);
        place(7, w * .43f, h * .88f, rad * .95f);
        place(8, w * .78f, h * .18f, rad * .82f);
        invalidate();
    }

    private boolean inStick(float x, float y) {
        return Math.hypot(x - stickX, y - stickY) <= stickRadius * 1.4f;
    }
    private void publish() {
        NativeBridge.nativeSetInput(buttons, xAxis, yAxis, 0, 0, rTrigger);
    }
    private void release() {
        buttons = xAxis = yAxis = rTrigger = pointers = 0;
        NativeBridge.nativeReleaseInput();
    }
    private void updateState() {
        int mask = 0, ax = 0, ay = 0, rt = 0;
        float sx = 0, sy = 0;
        boolean stickHeld = false;
        for (int p = 0; p < pointers; p++) {
            float x = px[p], y = py[p];
            if (inStick(x, y)) {
                stickHeld = true;
                sx = Math.max(-1f, Math.min(1f, (x - stickX) / stickRadius));
                sy = Math.max(-1f, Math.min(1f, (stickY - y) / stickRadius));
            }
            for (int i = 0; i < pads.length; i++) {
                if (pads[i].contains(x, y)) {
                    mask |= MASKS[i];
                    if (i == 8) rt = 255;
                }
            }
        }
        if (stickHeld) {
            float len = (float) Math.hypot(sx, sy);
            if (len > 1f) { sx /= len; sy /= len; }
            if (Math.abs(sx) < .14f) sx = 0;
            if (Math.abs(sy) < .14f) sy = 0;
            ax = Math.round(sx * 32767);
            ay = Math.round(sy * 32767);
            if (sx < -.25f) mask |= LEFT;
            if (sx > .25f) mask |= RIGHT;
            if (sy < -.25f) mask |= DOWN;
            if (sy > .25f) mask |= UP;
        }
        if (buttons != mask || xAxis != ax || yAxis != ay || rTrigger != rt) {
            buttons = mask; xAxis = ax; yAxis = ay; rTrigger = rt;
            publish(); invalidate();
        }
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        if (!show) return false;
        int action = event.getActionMasked(), lifted = event.getActionIndex();
        if (action == MotionEvent.ACTION_CANCEL) {
            release(); invalidate(); return true;
        }
        if (action == MotionEvent.ACTION_DOWN ||
                action == MotionEvent.ACTION_POINTER_DOWN ||
                action == MotionEvent.ACTION_MOVE ||
                action == MotionEvent.ACTION_UP ||
                action == MotionEvent.ACTION_POINTER_UP) {
            pointers = 0;
            for (int i = 0; i < event.getPointerCount() && pointers < ids.length; i++) {
                if ((action == MotionEvent.ACTION_UP ||
                     action == MotionEvent.ACTION_POINTER_UP) && i == lifted) continue;
                ids[pointers] = event.getPointerId(i);
                px[pointers] = event.getX(i);
                py[pointers] = event.getY(i);
                ++pointers;
            }
            updateState();
            return true;
        }
        return true;
    }

    @Override protected void onDetachedFromWindow() {
        release(); super.onDetachedFromWindow();
    }
    @Override protected void onDraw(Canvas canvas) {
        if (!show || width <= 0 || height <= 0) return;
        float unit = Math.min(width / 2.05f, height);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.argb(72, 18, 28, 45));
        canvas.drawCircle(stickX, stickY, stickRadius, paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(unit * .005f);
        paint.setColor(Color.argb(160, 255, 255, 255));
        canvas.drawCircle(stickX, stickY, stickRadius, paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.argb(150, 200, 218, 255));
        canvas.drawCircle(stickX + xAxis / 32767f * stickRadius * .64f,
                stickY - yAxis / 32767f * stickRadius * .64f,
                unit * .038f, paint);

        for (int i = 0; i < pads.length; i++) {
            RectF b = pads[i];
            paint.setColor(Color.argb(
                (MASKS[i] != 0 && (buttons & MASKS[i]) != 0) ||
                (i == 8 && rTrigger > 0) ? 160 : 87, 18, 28, 45));
            canvas.drawOval(b, paint);
            paint.setColor(Color.argb(225, 250, 250, 255));
            paint.setTextAlign(Paint.Align.CENTER);
            paint.setTextSize(Math.min(b.width() * .46f, unit * .041f));
            paint.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
            canvas.drawText(LABELS[i], b.centerX(),
                    b.centerY() - (paint.ascent() + paint.descent()) / 2, paint);
        }
    }
}
