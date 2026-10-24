// WWE '12 PC - Android: on-screen controller drawn over the game. Its state goes to a virtual SDL
// gamepad (port/src/android/wwe12_touch.cpp) that the game sees like a real controller.
// Hidden while a real controller is connected. Layout in fractions of the screen (landscape).
package io.github.imrburn.wwe12;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.hardware.input.InputManager;
import android.os.Handler;
import android.os.Looper;
import android.view.HapticFeedbackConstants;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

public class TouchControls extends View implements InputManager.InputDeviceListener {
    static native boolean nativeAttach();
    static native void nativeDetach();
    static native void nativeUpdate(int buttons, float lx, float ly, float rx, float ry, float lt, float rt);

    // SDL_GamepadButton numbers.
    static final int A = 0, B = 1, X = 2, Y = 3, BACK = 4, START = 6, LS = 7, RS = 8, LB = 9, RB = 10,
            UP = 11, DOWN = 12, LEFT = 13, RIGHT = 14;
    static final int LT = 100, RT = 101;  // triggers: axes, not buttons

    static class Control {
        final String label;
        final int id;          // button, trigger, or -1 / -2 for left / right stick
        final float fx, fy, fr; // centre (fraction of width / height) and radius (fraction of height)
        final int color;
        float cx, cy, r;
        int pointer = -1;      // touch holding it
        float ax, ay;          // stick position (-1..1)

        Control(String label, int id, float fx, float fy, float fr, int color) {
            this.label = label; this.id = id; this.fx = fx; this.fy = fy; this.fr = fr; this.color = color;
        }

        boolean isStick() { return id < 0; }
    }

    final List<Control> controls = new ArrayList<>();
    final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    final Handler handler = new Handler(Looper.getMainLooper());
    final int alpha;          // 0-255, from the launcher's opacity setting
    final float scale;        // from the launcher's size setting (1 = normal)
    boolean attached;
    boolean hidden;

    public TouchControls(Context context, int opacityPercent, int sizePercent) {
        super(context);
        alpha = Math.max(30, Math.min(255, opacityPercent * 255 / 100));
        scale = Math.max(50, Math.min(150, sizePercent)) / 100f;
        int green = Color.rgb(90, 180, 70), red = Color.rgb(210, 60, 60), blue = Color.rgb(60, 110, 220),
                yellow = Color.rgb(230, 190, 50), grey = Color.rgb(150, 150, 150);
        // Left side: stick, d-pad, LB/LT.
        controls.add(new Control("", -1, 0.13f, 0.66f, 0.17f, grey));
        controls.add(new Control("▲", UP, 0.30f, 0.70f, 0.055f, grey));
        controls.add(new Control("▼", DOWN, 0.30f, 0.90f, 0.055f, grey));
        controls.add(new Control("◀", LEFT, 0.25f, 0.80f, 0.055f, grey));
        controls.add(new Control("▶", RIGHT, 0.35f, 0.80f, 0.055f, grey));
        controls.add(new Control("LT", LT, 0.06f, 0.16f, 0.075f, grey));
        controls.add(new Control("LB", LB, 0.17f, 0.16f, 0.075f, grey));
        // Right side: face buttons, right stick, RB/RT.
        controls.add(new Control("A", A, 0.86f, 0.80f, 0.08f, green));
        controls.add(new Control("B", B, 0.94f, 0.62f, 0.08f, red));
        controls.add(new Control("X", X, 0.78f, 0.62f, 0.08f, blue));
        controls.add(new Control("Y", Y, 0.86f, 0.44f, 0.08f, yellow));
        controls.add(new Control("", -2, 0.66f, 0.80f, 0.12f, grey));
        controls.add(new Control("RB", RB, 0.83f, 0.16f, 0.075f, grey));
        controls.add(new Control("RT", RT, 0.94f, 0.16f, 0.075f, grey));
        // Middle top: BACK / START.
        controls.add(new Control("BACK", BACK, 0.42f, 0.08f, 0.06f, grey));
        controls.add(new Control("START", START, 0.58f, 0.08f, 0.06f, grey));

        stroke.setStyle(Paint.Style.STROKE);
        text.setTextAlign(Paint.Align.CENTER);
        text.setTypeface(Typeface.DEFAULT_BOLD);
        setHapticFeedbackEnabled(true);

        InputManager im = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
        im.registerInputDeviceListener(this, handler);
        hidden = realControllerConnected();
        setVisibility(hidden ? GONE : VISIBLE);
        handler.post(attachLoop);
    }

    // The game starts SDL's gamepads a moment after the activity: retry until the pad exists.
    final Runnable attachLoop = new Runnable() {
        public void run() {
            if (hidden) return;
            attached = nativeAttach();
            if (!attached) handler.postDelayed(this, 500);
        }
    };

    // ---- Real controllers: hide the touch pad while one is connected --------------------------

    boolean realControllerConnected() {
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice d = InputDevice.getDevice(id);
            if (d == null || d.isVirtual()) continue;
            int s = d.getSources();
            if ((s & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                    || (s & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) return true;
        }
        return false;
    }

    void refreshControllers() {
        boolean real = realControllerConnected();
        if (real == hidden) return;
        hidden = real;
        setVisibility(hidden ? GONE : VISIBLE);
        if (hidden) {
            nativeDetach();
            attached = false;
        } else {
            handler.post(attachLoop);
        }
    }

    public void onInputDeviceAdded(int id) { refreshControllers(); }
    public void onInputDeviceRemoved(int id) { refreshControllers(); }
    public void onInputDeviceChanged(int id) { refreshControllers(); }

    // ---- Layout and drawing --------------------------------------------------------------------

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        // The size setting scales each button and its distance to its screen corner (or to the
        // top middle for BACK / START), so groups shrink towards the edges and stay together.
        // Only the top row (shoulders, BACK / START) hangs from the top edge.
        for (Control c : controls) {
            float axf = c.fx > 0.4f && c.fx < 0.6f ? 0.5f : (c.fx < 0.5f ? 0f : 1f);
            float ayf = c.fy < 0.25f ? 0f : 1f;
            c.cx = (axf + (c.fx - axf) * scale) * w;
            c.cy = (ayf + (c.fy - ayf) * scale) * h;
            c.r = c.fr * scale * h;
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        for (Control c : controls) {
            boolean down = c.pointer >= 0;
            fill.setColor(c.color);
            fill.setAlpha(down ? Math.min(255, alpha + 80) : alpha / 2);
            stroke.setColor(Color.WHITE);
            stroke.setAlpha(alpha);
            stroke.setStrokeWidth(c.r * 0.06f);
            canvas.drawCircle(c.cx, c.cy, c.r, fill);
            canvas.drawCircle(c.cx, c.cy, c.r, stroke);
            if (c.isStick()) {
                fill.setColor(Color.WHITE);
                fill.setAlpha(alpha);
                canvas.drawCircle(c.cx + c.ax * c.r * 0.6f, c.cy + c.ay * c.r * 0.6f, c.r * 0.4f, fill);
            } else {
                text.setColor(Color.WHITE);
                text.setAlpha(Math.min(255, alpha + 60));
                text.setTextSize(c.r * (c.label.length() > 2 ? 0.45f : 0.8f));
                canvas.drawText(c.label, c.cx, c.cy - (text.descent() + text.ascent()) / 2, text);
            }
        }
    }

    // ---- Touch ---------------------------------------------------------------------------------

    Control controlAt(float x, float y) {
        Control best = null;
        float bestDist = Float.MAX_VALUE;
        for (Control c : controls) {
            float dx = x - c.cx, dy = y - c.cy, d = dx * dx + dy * dy;
            float reach = c.r * (c.isStick() ? 1.4f : 1.15f);  // a little forgiving
            if (d <= reach * reach && d < bestDist) {
                best = c;
                bestDist = d;
            }
        }
        return best;
    }

    void moveStick(Control c, float x, float y) {
        float ax = (x - c.cx) / c.r, ay = (y - c.cy) / c.r;
        float len = (float) Math.sqrt(ax * ax + ay * ay);
        if (len > 1) { ax /= len; ay /= len; }
        c.ax = ax;
        c.ay = ay;
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        int action = e.getActionMasked();
        int index = e.getActionIndex();
        boolean pressed = false;
        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                int pid = e.getPointerId(index);
                Control c = controlAt(e.getX(index), e.getY(index));
                if (c != null && c.pointer < 0) {
                    c.pointer = pid;
                    if (c.isStick()) moveStick(c, e.getX(index), e.getY(index));
                    pressed = true;
                }
                break;
            }
            case MotionEvent.ACTION_MOVE:
                for (int i = 0; i < e.getPointerCount(); i++) {
                    int pid = e.getPointerId(i);
                    for (Control c : controls) {
                        if (c.pointer != pid) continue;
                        if (c.isStick()) {
                            moveStick(c, e.getX(i), e.getY(i));
                        } else if (controlAt(e.getX(i), e.getY(i)) != c) {
                            // Slid off a button onto another: release it, press the new one.
                            c.pointer = -1;
                            Control n = controlAt(e.getX(i), e.getY(i));
                            if (n != null && !n.isStick() && n.pointer < 0) {
                                n.pointer = pid;
                                pressed = true;
                            }
                        }
                    }
                }
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_CANCEL: {
                int pid = action == MotionEvent.ACTION_CANCEL ? -2 : e.getPointerId(index);
                for (Control c : controls) {
                    if (c.pointer == pid || pid == -2) {
                        c.pointer = -1;
                        c.ax = c.ay = 0;
                    }
                }
                break;
            }
        }
        if (pressed) performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
        send();
        invalidate();
        return true;
    }

    void send() {
        int buttons = 0;
        float lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
        for (Control c : controls) {
            boolean down = c.pointer >= 0;
            if (c.id == -1) { lx = c.ax; ly = c.ay; }
            else if (c.id == -2) { rx = c.ax; ry = c.ay; }
            else if (c.id == LT) lt = down ? 1 : 0;
            else if (c.id == RT) rt = down ? 1 : 0;
            else if (down) buttons |= 1 << c.id;
        }
        if (!attached) attached = nativeAttach();
        if (attached) nativeUpdate(buttons, lx, ly, rx, ry, lt, rt);
    }
}
