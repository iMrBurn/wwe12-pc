// WWE '12 PC - Android: editor for the on-screen buttons (opened from the launcher, CONTROLS).
// Drag a button to move it, tap it to select it, then resize / hide it or delete it (custom buttons).
// ADD BUTTON makes a custom button that presses one or several gamepad inputs at once.
// SAVE writes the layout (TouchControls.PREF_LAYOUT); the game reads it at the next PLAY.
package io.github.imrburn.wwe12;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.ArrayAdapter;
import android.widget.TextView;
import android.widget.Toast;

public class LayoutEditorActivity extends Activity {
    static final int YELLOW = Color.rgb(240, 191, 61);

    TouchControls pad;
    TextView info;
    Button hideBtn, deleteBtn;
    LinearLayout body;
    float dp;

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        dp = getResources().getDisplayMetrics().density;
        int opacity = getSharedPreferences("launcher", MODE_PRIVATE).getInt("touch_opacity", 50);

        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(Color.rgb(24, 24, 24));
        pad = new TouchControls(this, opacity, true);
        root.addView(pad, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        pad.onSelectionChanged = new Runnable() {
            public void run() { refreshPanel(); }
        };

        // Fixed width: wrapped, the panel came out too narrow on some screens (buttons squeezed,
        // the SAVE row cut off).
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams((int) (380 * dp),
                ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER_HORIZONTAL | Gravity.TOP);
        lp.topMargin = (int) (64 * dp);
        root.addView(buildPanel(), lp);
        setContentView(root);
        refreshPanel();
    }

    @Override
    public void onWindowFocusChanged(boolean focus) {
        super.onWindowFocusChanged(focus);
        if (focus) {
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
    }

    // ---- Panel ---------------------------------------------------------------------------------

    View buildPanel() {
        LinearLayout panel = new LinearLayout(this);
        panel.setOrientation(LinearLayout.VERTICAL);
        int p = (int) (8 * dp);
        panel.setPadding(p, p, p, p);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.argb(215, 20, 20, 20));
        bg.setCornerRadius(8 * dp);
        panel.setBackground(bg);

        final Button toggle = small("LAYOUT EDITOR  ▾", true);
        panel.addView(toggle);

        body = new LinearLayout(this);
        body.setOrientation(LinearLayout.VERTICAL);
        panel.addView(body);

        info = new TextView(this);
        info.setTextColor(Color.WHITE);
        info.setTextSize(13);
        info.setGravity(Gravity.CENTER);
        body.addView(info);

        LinearLayout size = row();
        size.addView(small("−", false, new Runnable() { public void run() { pad.scaleControl(pad.selected, 0.9f); } }));
        size.addView(label("SIZE"));
        size.addView(small("+", false, new Runnable() { public void run() { pad.scaleControl(pad.selected, 1.1f); } }));
        size.addView(small("ALL −", false, new Runnable() { public void run() { pad.scaleAll(0.92f); } }));
        size.addView(small("ALL +", false, new Runnable() { public void run() { pad.scaleAll(1.08f); } }));
        body.addView(size);

        LinearLayout edit = row();
        hideBtn = small("HIDE", false, new Runnable() {
            public void run() {
                if (pad.selected == null) return;
                pad.selected.visible = !pad.selected.visible;
                refreshPanel();
                pad.invalidate();
            }
        });
        edit.addView(hideBtn);
        deleteBtn = small("DELETE", false, new Runnable() {
            public void run() {
                pad.deleteControl(pad.selected);
                refreshPanel();
                pad.invalidate();
            }
        });
        edit.addView(deleteBtn);
        edit.addView(small("ADD BUTTON", false, new Runnable() { public void run() { addButtonDialog(); } }));
        body.addView(edit);

        LinearLayout end = row();
        end.addView(small("RESET", false, new Runnable() { public void run() { confirmReset(); } }));
        end.addView(small("CANCEL", false, new Runnable() { public void run() { finish(); } }));
        Button save = small("SAVE", true, new Runnable() {
            public void run() {
                pad.saveLayout();
                Toast.makeText(LayoutEditorActivity.this, "Layout saved", Toast.LENGTH_SHORT).show();
                finish();
            }
        });
        end.addView(save);
        body.addView(end);

        toggle.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                boolean open = body.getVisibility() != View.VISIBLE;
                body.setVisibility(open ? View.VISIBLE : View.GONE);
                toggle.setText(open ? "LAYOUT EDITOR  ▾" : "LAYOUT EDITOR  ▸");
            }
        });
        return panel;
    }

    void refreshPanel() {
        TouchControls.Control c = pad.selected;
        if (c == null) {
            info.setText("Drag a button to move it. Tap one to select it.");
            hideBtn.setEnabled(false);
            deleteBtn.setEnabled(false);
        } else {
            String name = c.isStick() ? (c.stick == 1 ? "Left stick" : "Right stick")
                    : (c.label.isEmpty() ? c.key : c.label);
            info.setText("Selected: " + name + (c.visible ? "" : " (hidden)"));
            hideBtn.setEnabled(true);
            hideBtn.setText(c.visible ? "HIDE" : "SHOW");
            deleteBtn.setEnabled(c.custom);
        }
    }

    // ---- Dialogs -------------------------------------------------------------------------------

    void confirmReset() {
        new AlertDialog.Builder(this)
                .setTitle("Reset the layout?")
                .setMessage("Every button goes back to its original place and size. Custom buttons are removed.")
                .setPositiveButton("RESET", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface d, int w) {
                        pad.resetLayout();
                        pad.clearSavedLayout();
                        refreshPanel();
                        pad.invalidate();
                    }
                })
                .setNegativeButton("CANCEL", null)
                .show();
    }

    // Custom button: a name, one or more inputs pressed together, a colour.
    void addButtonDialog() {
        LinearLayout form = new LinearLayout(this);
        form.setOrientation(LinearLayout.VERTICAL);
        int p = (int) (16 * dp);
        form.setPadding(p, p / 2, p, 0);

        final EditText name = new EditText(this);
        name.setHint("Label (max 4 characters)");
        name.setSingleLine(true);
        name.setFilters(new android.text.InputFilter[] {new android.text.InputFilter.LengthFilter(4)});
        form.addView(name);

        TextView t = new TextView(this);
        t.setText("Presses (tick one or several, e.g. A + B):");
        t.setPadding(0, p / 2, 0, 0);
        form.addView(t);

        final CheckBox[] boxes = new CheckBox[TouchControls.INPUT_NAMES.length];
        for (int i = 0; i < boxes.length; i++) {
            boxes[i] = new CheckBox(this);
            boxes[i].setText(TouchControls.INPUT_NAMES[i]);
            form.addView(boxes[i]);
        }

        final CheckBox latch = new CheckBox(this);
        latch.setText("Latch: tap = on, tap again = off");
        form.addView(latch);

        final Spinner color = new Spinner(this);
        color.setAdapter(new ArrayAdapter<String>(this, android.R.layout.simple_spinner_dropdown_item,
                TouchControls.COLOR_NAMES));
        form.addView(color);

        ScrollView scroll = new ScrollView(this);
        scroll.addView(form);
        new AlertDialog.Builder(this)
                .setTitle("New button")
                .setView(scroll)
                .setPositiveButton("ADD", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface d, int w) {
                        int mask = 0, trig = 0;
                        StringBuilder auto = new StringBuilder();
                        for (int i = 0; i < boxes.length; i++) {
                            if (!boxes[i].isChecked()) continue;
                            int[] m = inputBits(i);
                            mask |= m[0];
                            trig |= m[1];
                            if (auto.length() > 0) auto.append('+');
                            auto.append(TouchControls.INPUT_NAMES[i].split(" ")[0]);
                        }
                        if (mask == 0 && trig == 0) {
                            Toast.makeText(LayoutEditorActivity.this, "Tick at least one input",
                                    Toast.LENGTH_SHORT).show();
                            return;
                        }
                        String label = name.getText().toString().trim();
                        if (label.isEmpty()) label = auto.length() > 4 ? "•" : auto.toString();
                        TouchControls.Control c = pad.addCustom(label, mask, trig, color.getSelectedItemPosition(), latch.isChecked());
                        pad.select(c);  // lands in the middle: drag it where you want it
                        pad.invalidate();
                    }
                })
                .setNegativeButton("CANCEL", null)
                .show();
    }

    // INPUT_NAMES index -> {button bits, trigger flags}.
    static int[] inputBits(int i) {
        switch (i) {
            case 0: return new int[] {1 << TouchControls.A, 0};
            case 1: return new int[] {1 << TouchControls.B, 0};
            case 2: return new int[] {1 << TouchControls.X, 0};
            case 3: return new int[] {1 << TouchControls.Y, 0};
            case 4: return new int[] {1 << TouchControls.LB, 0};
            case 5: return new int[] {1 << TouchControls.RB, 0};
            case 6: return new int[] {0, TouchControls.TRIG_L};
            case 7: return new int[] {0, TouchControls.TRIG_R};
            case 8: return new int[] {1 << TouchControls.LS, 0};
            case 9: return new int[] {1 << TouchControls.RS, 0};
            case 10: return new int[] {1 << TouchControls.BACK, 0};
            case 11: return new int[] {1 << TouchControls.START, 0};
            case 12: return new int[] {1 << TouchControls.UP, 0};
            case 13: return new int[] {1 << TouchControls.DOWN, 0};
            case 14: return new int[] {1 << TouchControls.LEFT, 0};
            default: return new int[] {1 << TouchControls.RIGHT, 0};
        }
    }

    // ---- Small UI helpers ----------------------------------------------------------------------

    LinearLayout row() {
        LinearLayout r = new LinearLayout(this);
        r.setOrientation(LinearLayout.HORIZONTAL);
        r.setGravity(Gravity.CENTER);
        return r;
    }

    TextView label(String s) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextColor(Color.LTGRAY);
        t.setTextSize(12);
        t.setPadding((int) (6 * dp), 0, (int) (6 * dp), 0);
        return t;
    }

    Button small(String s, boolean accent) {
        Button b = new Button(this);
        b.setText(s);
        b.setAllCaps(false);
        b.setTextSize(12);
        b.setTypeface(Typeface.DEFAULT_BOLD);
        b.setTextColor(accent ? Color.BLACK : Color.WHITE);
        b.setMinWidth(0);
        b.setMinimumWidth(0);
        b.setMinHeight((int) (36 * dp));
        b.setMinimumHeight((int) (36 * dp));
        b.setPadding((int) (10 * dp), 0, (int) (10 * dp), 0);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(accent ? YELLOW : Color.rgb(70, 70, 70));
        bg.setCornerRadius(6 * dp);
        b.setBackground(bg);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.setMargins((int) (3 * dp), (int) (3 * dp), (int) (3 * dp), (int) (3 * dp));
        b.setLayoutParams(lp);
        return b;
    }

    Button small(String s, boolean accent, final Runnable action) {
        Button b = small(s, accent);
        b.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { action.run(); }
        });
        return b;
    }
}
