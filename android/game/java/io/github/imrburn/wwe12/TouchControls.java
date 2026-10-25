// WWE '12 PC - Android: on-screen controller drawn over the game. Its state goes to a virtual SDL
// gamepad (port/src/android/wwe12_touch.cpp) that the game sees like a real controller.
// Hidden while a real controller is connected. Layout in fractions of the screen (landscape).
//
// Layout editor, custom buttons and stick options: from Yexclsv's work. The layout is customisable: every control can be moved, resized or hidden, and the player can
// add custom buttons (one or several gamepad inputs on one button, e.g. A+B). The layout is saved
// as JSON in the "launcher" preferences (key touch_layout). LayoutEditorActivity uses this same
// view in edit mode (no native calls: the editor runs in the launcher process, without the game libraries).
package io.github.imrburn.wwe12;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.DashPathEffect;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.hardware.input.InputManager;
import android.os.Handler;
import android.os.Looper;
import android.view.HapticFeedbackConstants;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

public class TouchControls extends View implements InputManager.InputDeviceListener {
    static native boolean nativeAttach();
    static native void nativeDetach();
    static native void nativeUpdate(int buttons, float lx, float ly, float rx, float ry, float lt, float rt);

    static final String PREF_LAYOUT = "touch_layout";

    // SDL_GamepadButton numbers.
    static final int A = 0, B = 1, X = 2, Y = 3, BACK = 4, START = 6, LS = 7, RS = 8, LB = 9, RB = 10,
            UP = 11, DOWN = 12, LEFT = 13, RIGHT = 14;
    // Triggers are axes, not buttons: bit flags in Control.trig.
    static final int TRIG_L = 1, TRIG_R = 2;

    // Inputs a custom button can press (name, button bit or trigger flag). Used by the editor.
    static final String[] INPUT_NAMES = {"A", "B", "X", "Y", "LB", "RB", "LT", "RT", "LS (click)", "RS (click)",
            "BACK", "START", "D-pad up", "D-pad down", "D-pad left", "D-pad right"};

    static final String[] COLOR_NAMES = {"Grey", "Green", "Red", "Blue", "Yellow", "Purple", "Orange"};
    static final int[] COLORS = {Color.rgb(150, 150, 150), Color.rgb(90, 180, 70), Color.rgb(210, 60, 60),
            Color.rgb(60, 110, 220), Color.rgb(230, 190, 50), Color.rgb(150, 80, 200), Color.rgb(230, 130, 40)};

    static final float MIN_R = 0.035f, MAX_R = 0.30f;

    static class Control {
        final String key;       // stable id used in the saved layout
        String label;
        int mask;               // gamepad buttons pressed (bit per SDL_GamepadButton)
        int trig;               // TRIG_L / TRIG_R flags
        final int stick;        // 0 none, 1 left stick, 2 right stick
        int colorIndex;
        final boolean custom;   // added by the player (can be deleted)
        float fx, fy, fr;       // centre (fraction of width / height) and radius (fraction of height)
        final float dfx, dfy, dfr;  // defaults, for RESET
        boolean visible = true;
        boolean latch;          // custom button: tap = on, tap again = off (e.g. a held run button)
        boolean latched;
        float cx, cy, r;
        float textSize, textOff;  // cached at layout time, not per frame
        int pointer = -1;       // touch holding it
        float ax, ay;           // stick knob position (-1..1), what is drawn
        float ox, oy;           // stick value sent to the game (after dead zone and sensitivity)

        Control(String key, String label, int mask, int trig, int stick, float fx, float fy, float fr,
                int colorIndex, boolean custom) {
            this.key = key; this.label = label; this.mask = mask; this.trig = trig; this.stick = stick;
            this.fx = dfx = fx; this.fy = dfy = fy; this.fr = dfr = fr;
            this.colorIndex = colorIndex; this.custom = custom;
        }

        boolean isStick() { return stick != 0; }
        boolean on() { return pointer >= 0 || latched; }
        int color() { return COLORS[Math.max(0, Math.min(COLORS.length - 1, colorIndex))]; }
    }

    final List<Control> controls = new ArrayList<>();
    final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    final Handler handler = new Handler(Looper.getMainLooper());
    final SharedPreferences prefs;
    final int alpha;          // 0-255, from the launcher's opacity setting
    final float scale;        // launcher's size setting, applied to the default layout (1 = normal)
    final boolean editMode;   // layout editor: drag to move, nothing is sent to the game
    boolean attached;
    boolean hidden;
    int viewW, viewH;

    // Options from the launcher (CONTROLS).
    final boolean haptics;
    final float deadzone;     // 0..0.5, fraction of the stick travel ignored around the centre
    final float sens;         // stick travel needed for full tilt: 1.0 = whole circle, 1.25 = a bit less
    final DashPathEffect selDash;
    final Rect clip = new Rect();

    // Last state sent to the game: events that change nothing are not sent (saves a JNI call each).
    boolean haveLast;
    int lastButtons;
    float lastLx, lastLy, lastRx, lastRy, lastLt, lastRt;

    // Editor state.
    Control selected;
    float dragDx, dragDy;
    int dragPointer = -1;
    Runnable onSelectionChanged;

    // In the game (WWE12Activity).
    public TouchControls(Context context, int opacityPercent, int sizePercent) {
        this(context, opacityPercent, sizePercent, false);
    }

    // In the layout editor (LayoutEditorActivity): same size setting as the game.
    public TouchControls(Context context, int opacityPercent, boolean editMode) {
        this(context, opacityPercent,
                context.getSharedPreferences("launcher", Context.MODE_PRIVATE).getInt("touch_size", 100), editMode);
    }

    TouchControls(Context context, int opacityPercent, int sizePercent, boolean editMode) {
        super(context);
        this.editMode = editMode;
        scale = clamp(sizePercent, 50, 150) / 100f;
        prefs = context.getSharedPreferences("launcher", Context.MODE_PRIVATE);
        int a = Math.max(30, Math.min(255, opacityPercent * 255 / 100));
        alpha = editMode ? Math.max(a, 150) : a;
        haptics = prefs.getBoolean("touch_haptics", true);
        deadzone = clamp(prefs.getInt("touch_deadzone", 10) / 100f, 0f, 0.5f);
        sens = clamp(prefs.getInt("touch_sens", 100) / 100f, 0.5f, 2f);
        float d = context.getResources().getDisplayMetrics().density;
        selDash = new DashPathEffect(new float[] {8 * d, 5 * d}, 0);
        buildDefaults();
        loadLayout();

        stroke.setStyle(Paint.Style.STROKE);
        text.setTextAlign(Paint.Align.CENTER);
        text.setTypeface(Typeface.DEFAULT_BOLD);
        setHapticFeedbackEnabled(haptics);

        if (editMode) return;  // no controller detection, no native calls in the editor

        InputManager im = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
        im.registerInputDeviceListener(this, handler);
        hidden = realControllerConnected();
        setVisibility(hidden ? GONE : VISIBLE);
        handler.post(attachLoop);
    }

    // ---- Layout: defaults, save, load ----------------------------------------------------------

    void buildDefaults() {
        controls.clear();
        // Left side: stick, d-pad, LB/LT.
        add("STICK_L", "", 0, 0, 1, 0.13f, 0.66f, 0.17f, 0);
        add("UP", "▲", 1 << UP, 0, 0, 0.30f, 0.70f, 0.055f, 0);
        add("DOWN", "▼", 1 << DOWN, 0, 0, 0.30f, 0.90f, 0.055f, 0);
        add("LEFT", "◀", 1 << LEFT, 0, 0, 0.25f, 0.80f, 0.055f, 0);
        add("RIGHT", "▶", 1 << RIGHT, 0, 0, 0.35f, 0.80f, 0.055f, 0);
        add("LT", "LT", 0, TRIG_L, 0, 0.06f, 0.16f, 0.075f, 0);
        add("LB", "LB", 1 << LB, 0, 0, 0.17f, 0.16f, 0.075f, 0);
        // Right side: face buttons, right stick, RB/RT.
        add("A", "A", 1 << A, 0, 0, 0.86f, 0.80f, 0.08f, 1);
        add("B", "B", 1 << B, 0, 0, 0.94f, 0.62f, 0.08f, 2);
        add("X", "X", 1 << X, 0, 0, 0.78f, 0.62f, 0.08f, 3);
        add("Y", "Y", 1 << Y, 0, 0, 0.86f, 0.44f, 0.08f, 4);
        add("STICK_R", "", 0, 0, 2, 0.66f, 0.80f, 0.12f, 0);
        add("RB", "RB", 1 << RB, 0, 0, 0.83f, 0.16f, 0.075f, 0);
        add("RT", "RT", 0, TRIG_R, 0, 0.94f, 0.16f, 0.075f, 0);
        // Middle top: BACK / START.
        add("BACK", "BACK", 1 << BACK, 0, 0, 0.42f, 0.08f, 0.06f, 0);
        add("START", "START", 1 << START, 0, 0, 0.58f, 0.08f, 0.06f, 0);
    }

    // The size setting scales each default control and its distance to its screen corner (or to
    // the top middle for BACK / START), so groups shrink towards the edges and stay together. Only
    // the top row (shoulders, BACK / START) hangs from the top edge. A layout saved in the editor
    // replaces these positions.
    void add(String key, String label, int mask, int trig, int stick, float fx, float fy, float fr, int color) {
        float axf = fx > 0.4f && fx < 0.6f ? 0.5f : (fx < 0.5f ? 0f : 1f);
        float ayf = fy < 0.25f ? 0f : 1f;
        controls.add(new Control(key, label, mask, trig, stick, axf + (fx - axf) * scale,
                ayf + (fy - ayf) * scale, fr * scale, color, false));
    }

    Control addCustom(String label, int mask, int trig, int colorIndex, boolean latch) {
        Control c = new Control("C" + System.currentTimeMillis(), label, mask, trig, 0,
                0.5f, 0.5f, 0.075f, colorIndex, true);
        c.latch = latch;
        controls.add(c);
        applySize(c);
        return c;
    }

    void deleteControl(Control c) {
        if (c == null || !c.custom) return;
        controls.remove(c);
        if (selected == c) selected = null;
    }

    void resetLayout() {
        selected = null;
        buildDefaults();
        for (Control c : controls) applySize(c);
    }

    void saveLayout() {
        try {
            JSONArray arr = new JSONArray();
            for (Control c : controls) {
                JSONObject o = new JSONObject();
                o.put("k", c.key);
                o.put("x", c.fx);
                o.put("y", c.fy);
                o.put("r", c.fr);
                o.put("v", c.visible);
                if (c.custom) {
                    o.put("l", c.label);
                    o.put("m", c.mask);
                    o.put("t", c.trig);
                    o.put("c", c.colorIndex);
                    o.put("lt", c.latch);
                }
                arr.put(o);
            }
            prefs.edit().putString(PREF_LAYOUT, arr.toString()).apply();
        } catch (Exception ignored) {
        }
    }

    void clearSavedLayout() {
        prefs.edit().remove(PREF_LAYOUT).apply();
    }

    // Applies the saved layout over the defaults. A bad or missing value leaves the defaults.
    void loadLayout() {
        String json = prefs.getString(PREF_LAYOUT, null);
        if (json == null) return;
        try {
            JSONArray arr = new JSONArray(json);
            for (int i = 0; i < arr.length(); i++) {
                JSONObject o = arr.getJSONObject(i);
                String key = o.getString("k");
                Control c = null;
                for (Control d : controls) if (d.key.equals(key)) { c = d; break; }
                if (c == null) {
                    if (!o.has("m") && !o.has("t")) continue;  // unknown default control: ignore
                    c = new Control(key, o.optString("l", "?"), o.optInt("m", 0), o.optInt("t", 0), 0,
                            0.5f, 0.5f, 0.075f, o.optInt("c", 0), true);
                    c.latch = o.optBoolean("lt", false);
                    controls.add(c);
                }
                c.fx = clamp((float) o.optDouble("x", c.fx), 0.02f, 0.98f);
                c.fy = clamp((float) o.optDouble("y", c.fy), 0.02f, 0.98f);
                c.fr = clamp((float) o.optDouble("r", c.fr), MIN_R, MAX_R);
                c.visible = o.optBoolean("v", true);
            }
        } catch (Exception e) {
            buildDefaults();
        }
    }

    static float clamp(float v, float lo, float hi) {
        return Math.max(lo, Math.min(hi, v));
    }

    void applySize(Control c) {
        c.cx = c.fx * viewW;
        c.cy = c.fy * viewH;
        c.r = c.fr * viewH;
        if (!c.isStick()) {
            c.textSize = c.r * (c.label.length() > 2 ? 0.45f : 0.8f);
            text.setTextSize(c.textSize);
            c.textOff = -(text.descent() + text.ascent()) / 2;
        }
    }

    // Redraws only the area of one control (a whole-screen redraw per touch event costs frames).
    void redraw(Control c) {
        float m = c.r * 1.35f + 4;
        invalidate((int) (c.cx - m), (int) (c.cy - m), (int) (c.cx + m) + 1, (int) (c.cy + m) + 1);
    }

    // Editor: grows or shrinks one control (factor 1.1 / 0.9).
    void scaleControl(Control c, float factor) {
        if (c == null) return;
        c.fr = clamp(c.fr * factor, MIN_R, MAX_R);
        applySize(c);
        invalidate();
    }

    void scaleAll(float factor) {
        for (Control c : controls) {
            c.fr = clamp(c.fr * factor, MIN_R, MAX_R);
            applySize(c);
        }
        invalidate();
    }

    // The game starts SDL's gamepads a moment after the activity: retry until the pad exists.
    final Runnable attachLoop = new Runnable() {
        public void run() {
            if (hidden) return;
            attached = nativeAttach();
            haveLast = false;
            if (!attached) handler.postDelayed(this, 500);
        }
    };

    // ---- Real controllers: hide the touch pad while one is connected --------------------------

    boolean realControllerConnected() {
        return realControllerName() != null;
    }

    // Name of a connected game controller, or null. The source bits alone are not enough: some
    // phones (Poco F7...) have built-in devices that claim SOURCE_GAMEPAD / SOURCE_JOYSTICK and
    // hid the on-screen buttons. Android gives a controller number (player slot) only to real
    // controllers, so that is required too.
    static String realControllerName() {
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice d = InputDevice.getDevice(id);
            if (d == null || d.isVirtual() || d.getControllerNumber() <= 0) continue;
            int s = d.getSources();
            if ((s & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                    || (s & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) return d.getName();
        }
        return null;
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

    public void onInputDeviceAdded(int id) { if (!editMode) refreshControllers(); }
    public void onInputDeviceRemoved(int id) { if (!editMode) refreshControllers(); }
    public void onInputDeviceChanged(int id) { if (!editMode) refreshControllers(); }

    // ---- Layout and drawing --------------------------------------------------------------------

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        viewW = w;
        viewH = h;
        for (Control c : controls) applySize(c);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        canvas.getClipBounds(clip);
        for (Control c : controls) {
            if (!c.visible && !editMode) continue;
            float m = c.r * 1.3f;
            if (c.cx + m < clip.left || c.cx - m > clip.right || c.cy + m < clip.top || c.cy - m > clip.bottom)
                continue;  // outside the area being redrawn
            boolean down = c.on();
            boolean ghost = !c.visible;  // hidden control, shown faintly in the editor only
            int a = ghost ? 40 : alpha;
            fill.setColor(c.color());
            fill.setAlpha(down ? Math.min(255, a + 80) : a / 2);
            stroke.setPathEffect(null);
            stroke.setColor(Color.WHITE);
            stroke.setAlpha(a);
            stroke.setStrokeWidth(c.r * 0.06f);
            canvas.drawCircle(c.cx, c.cy, c.r, fill);
            canvas.drawCircle(c.cx, c.cy, c.r, stroke);
            if (c.isStick()) {
                fill.setColor(Color.WHITE);
                fill.setAlpha(a);
                canvas.drawCircle(c.cx + c.ax * c.r * 0.6f, c.cy + c.ay * c.r * 0.6f, c.r * 0.4f, fill);
            } else {
                text.setColor(Color.WHITE);
                text.setAlpha(Math.min(255, a + 60));
                text.setTextSize(c.textSize);
                canvas.drawText(c.label, c.cx, c.cy + c.textOff, text);
            }
            if (editMode && c == selected) {
                stroke.setColor(Color.rgb(240, 191, 61));
                stroke.setAlpha(255);
                stroke.setStrokeWidth(Math.max(3f, c.r * 0.08f));
                stroke.setPathEffect(selDash);
                canvas.drawCircle(c.cx, c.cy, c.r * 1.12f, stroke);
            }
        }
    }

    // ---- Touch ---------------------------------------------------------------------------------

    Control controlAt(float x, float y) {
        Control best = null;
        float bestDist = Float.MAX_VALUE;
        for (Control c : controls) {
            if (!c.visible && !editMode) continue;
            float dx = x - c.cx, dy = y - c.cy, d = dx * dx + dy * dy;
            float reach = editMode ? Math.max(c.r, 0.05f * viewH)
                    : c.r * (c.isStick() ? 1.4f : 1.15f);  // a little forgiving
            if (d <= reach * reach && d < bestDist) {
                best = c;
                bestDist = d;
            }
        }
        return best;
    }

    // Returns true when the stick changed (so only then is anything redrawn / sent).
    boolean moveStick(Control c, float x, float y) {
        float ax = (x - c.cx) / c.r, ay = (y - c.cy) / c.r;
        float len = (float) Math.sqrt(ax * ax + ay * ay);
        if (len > 1) { ax /= len; ay /= len; len = 1; }
        // Output: sensitivity, then dead zone (rescaled so the stick still reaches 100 %).
        float out = Math.min(1f, len * sens);
        out = out <= deadzone ? 0f : (out - deadzone) / (1f - deadzone);
        float k = len > 0 ? out / len : 0f;
        float ox = ax * k, oy = ay * k;
        if (Math.abs(ax - c.ax) < 0.004f && Math.abs(ay - c.ay) < 0.004f && ox == c.ox && oy == c.oy)
            return false;
        c.ax = ax;
        c.ay = ay;
        c.ox = ox;
        c.oy = oy;
        return true;
    }

    void releaseStick(Control c) {
        c.ax = c.ay = c.ox = c.oy = 0;
    }

    void select(Control c) {
        if (selected == c) return;
        selected = c;
        if (onSelectionChanged != null) onSelectionChanged.run();
    }

    // Editor: a finger drags the control under it; a tap selects it.
    boolean onEditTouch(MotionEvent e) {
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: {
                Control c = controlAt(e.getX(), e.getY());
                select(c);
                if (c != null) {
                    dragPointer = e.getPointerId(0);
                    dragDx = e.getX() - c.cx;
                    dragDy = e.getY() - c.cy;
                    performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
                }
                break;
            }
            case MotionEvent.ACTION_MOVE: {
                int i = e.findPointerIndex(dragPointer);
                if (selected != null && i >= 0 && viewW > 0 && viewH > 0) {
                    redraw(selected);
                    selected.fx = clamp((e.getX(i) - dragDx) / viewW, 0.02f, 0.98f);
                    selected.fy = clamp((e.getY(i) - dragDy) / viewH, 0.02f, 0.98f);
                    applySize(selected);
                    redraw(selected);
                }
                break;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                dragPointer = -1;
                break;
        }
        if (e.getActionMasked() == MotionEvent.ACTION_DOWN) invalidate();  // selection ring moved
        return true;
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        if (editMode) return onEditTouch(e);
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
                    else if (c.latch) c.latched = !c.latched;
                    pressed = true;
                    redraw(c);
                }
                break;
            }
            case MotionEvent.ACTION_MOVE:
                for (int i = 0; i < e.getPointerCount(); i++) {
                    int pid = e.getPointerId(i);
                    for (Control c : controls) {
                        if (c.pointer != pid) continue;
                        if (c.isStick()) {
                            if (moveStick(c, e.getX(i), e.getY(i))) redraw(c);
                        } else if (!c.latch && controlAt(e.getX(i), e.getY(i)) != c) {
                            // Slid off a button onto another: release it, press the new one.
                            c.pointer = -1;
                            redraw(c);
                            Control n = controlAt(e.getX(i), e.getY(i));
                            if (n != null && !n.isStick() && !n.latch && n.pointer < 0) {
                                n.pointer = pid;
                                pressed = true;
                                redraw(n);
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
                    if (c.pointer == pid || (pid == -2 && c.pointer >= 0)) {
                        c.pointer = -1;
                        releaseStick(c);
                        redraw(c);
                    }
                }
                break;
            }
        }
        if (pressed && haptics) performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
        send();
        return true;
    }

    void send() {
        int buttons = 0;
        float lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
        for (Control c : controls) {
            if (c.stick == 1) { if (c.pointer >= 0) { lx = c.ox; ly = c.oy; } continue; }
            if (c.stick == 2) { if (c.pointer >= 0) { rx = c.ox; ry = c.oy; } continue; }
            if (!c.on() || !c.visible) continue;
            buttons |= c.mask;
            if ((c.trig & TRIG_L) != 0) lt = 1;
            if ((c.trig & TRIG_R) != 0) rt = 1;
        }
        if (!attached) return;  // attachLoop keeps trying
        if (haveLast && buttons == lastButtons && lx == lastLx && ly == lastLy && rx == lastRx
                && ry == lastRy && lt == lastLt && rt == lastRt) return;  // nothing changed
        haveLast = true;
        lastButtons = buttons;
        lastLx = lx; lastLy = ly; lastRx = rx; lastRy = ry; lastLt = lt; lastRt = rt;
        nativeUpdate(buttons, lx, ly, rx, ry, lt, rt);
    }
}
