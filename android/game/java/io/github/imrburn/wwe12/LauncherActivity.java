// WWE '12 PC - Android launcher: first screen of the app (the PC launcher is Windows-only).
// Choose the game (an ISO picked with Android's file picker, or files copied into game/),
// a few settings written to wwe12.toml like the PC launcher does, share the last log, PLAY.
package io.github.imrburn.wwe12;

import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.database.Cursor;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.view.Gravity;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.RandomAccessFile;

public class LauncherActivity extends Activity {
    static final int PICK_ISO = 1;
    static final int PICK_DRIVER = 2;
    static final int PICK_DLC = 3;
    static final int PICK_DLC_FOLDER = 4;
    static final int YELLOW = Color.rgb(240, 191, 61);   // same accent as the PC launcher
    static final int BG = Color.rgb(17, 17, 17);
    static final int PANEL = Color.rgb(32, 32, 32);

    Config config;
    SharedPreferences prefs;
    TextView gameStatus;
    TextView controllerStatus;   // which controller the game will use (hides the on-screen buttons)
    LinearLayout driverBox;      // GPU DRIVER panel content, rebuilt after import / delete
    LinearLayout updateArea;     // UPDATE AVAILABLE panel, shown when GitHub has a newer version
    LinearLayout dlcBox;         // DLC panel content, rebuilt after add / remove
    TextView dlcStatus;
    LinearLayout deviceBox;      // device warning + summary, filled by checkDevice()
    volatile DeviceCheck device;
    Button forget;

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        prefs = getSharedPreferences("launcher", MODE_PRIVATE);
        File dir = getExternalFilesDir(null);
        WWE12Activity.prepareFolder(this, dir);
        config = new Config(new File(dir, "wwe12.toml"));
        if (!prefs.getBoolean("android_defaults", false)) {
            // First start: phone-friendly defaults (the PC file picks the internal resolution from the screen).
            config.set("svr_render_scale", "720");
            config.save();
            prefs.edit().putBoolean("android_defaults", true).apply();
        }
        setContentView(buildUi(dir));
        refreshGame();
        checkDevice();
        Updater.check(this, new Updater.Found() {
            public void found(Updater.Release r) { showUpdate(r); }
        });
        onNewIntent(getIntent());
    }

    // Android's answer to an update install (Updater.install's PendingIntent).
    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        if (intent == null || !Updater.ACTION_STATUS.equals(intent.getAction())) return;
        String error = Updater.handleStatus(this, intent);
        if (error != null) Toast.makeText(this, error, Toast.LENGTH_LONG).show();
    }

    void showUpdate(final Updater.Release r) {
        updateArea.removeAllViews();
        LinearLayout p = panel(updateArea, "UPDATE AVAILABLE");
        p.addView(text("New version: " + (r.name.isEmpty() ? r.tag : r.name)
                + ". Your saves, settings and game files are kept.", 14, Color.WHITE, false));
        final TextView status = text("", 12, Color.LTGRAY, false);
        final Button go = button("DOWNLOAD AND INSTALL", true);
        go.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                // Android asks once to allow installs from this app.
                if (!getPackageManager().canRequestPackageInstalls()) {
                    Toast.makeText(LauncherActivity.this, "Allow \"Install unknown apps\" for WWE '12 PC, "
                            + "then come back and tap again.", Toast.LENGTH_LONG).show();
                    startActivity(new Intent(android.provider.Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                            Uri.parse("package:" + getPackageName())));
                    return;
                }
                go.setEnabled(false);
                Updater.install(LauncherActivity.this, r, new Updater.Progress() {
                    public void progress(String text, boolean done, boolean failed) {
                        status.setText(text);
                        if (failed) go.setEnabled(true);
                    }
                });
            }
        });
        p.addView(go);
        p.addView(status);
        updateArea.setVisibility(View.VISIBLE);
    }

    @Override
    protected void onResume() {
        super.onResume();
        // A controller paired while the launcher was in the background shows up on return.
        String pad = TouchControls.realControllerName();
        controllerStatus.setText(pad != null
                ? "Controller detected: " + pad + " (the on-screen buttons hide while it is connected)"
                : "No controller detected: the on-screen buttons will show.");
    }

    // ---- UI ----------------------------------------------------------------------------------

    View buildUi(File dir) {
        float dp = getResources().getDisplayMetrics().density;
        int pad = (int) (16 * dp);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(pad, pad, pad, pad);
        root.setBackgroundColor(BG);

        TextView title = text("WWE '12 PC", 28, YELLOW, true);
        root.addView(title);
        root.addView(text("Android " + versionName() + " - early test build", 13, Color.LTGRAY, false));
        // Device warning: what will stop the game, or make it run badly, on this phone. Filled by
        // checkDevice() (the check needs a background thread).
        deviceBox = new LinearLayout(this);
        deviceBox.setOrientation(LinearLayout.VERTICAL);
        root.addView(deviceBox);
        // Filled by Updater.check when GitHub has a newer Android release.
        updateArea = new LinearLayout(this);
        updateArea.setOrientation(LinearLayout.VERTICAL);
        updateArea.setVisibility(View.GONE);
        root.addView(updateArea);

        // Game
        LinearLayout game = panel(root, "GAME");
        gameStatus = text("", 14, Color.WHITE, false);
        game.addView(gameStatus);
        Button pick = button("CHOOSE MY ISO", false);
        pick.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { pickIso(); }
        });
        game.addView(pick);
        forget = button("FORGET THIS ISO (use the game folder)", false);
        forget.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                prefs.edit().remove("iso_uri").remove("iso_name").apply();
                refreshGame();
            }
        });
        game.addView(forget);
        game.addView(text("Or copy your game files (or ISO) into:\n" + new File(dir, "game").getAbsolutePath(),
                12, Color.GRAY, false));

        // Saves (own screen, like the PC launcher's SAVES page)
        Button saves = button("MANAGE MY SAVES (backups, Xbox 360 import)", false);
        saves.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { startActivity(new Intent(LauncherActivity.this, SavesActivity.class)); }
        });
        game.addView(saves);

        // DLC: packages copied into dlc/, installed by the game at its next start (as on PC)
        dlcBox = panel(root, "DLC");
        buildDlcPanel();

        // Settings
        LinearLayout set = panel(root, "SETTINGS");
        set.addView(choice("Language", "user_language",
                new String[] {"English", "Deutsch", "Français", "Español", "Italiano"},
                new String[] {"1", "3", "4", "5", "6"}));
        set.addView(choice("Internal resolution (lower = faster)", "svr_render_scale",
                new String[] {"540p (fastest)", "720p (recommended)", "1080p", "Automatic (screen size)"},
                new String[] {"540", "720", "1080", "0"}));
        set.addView(choice("Picture shape", "bd_aspect_ratio",
                new String[] {"16:9 with black bars (original)", "Fill the whole screen (stretched)"},
                new String[] {"5", "6"}));
        set.addView(toggle("FPS counter", "wwe12_show_fps"));
        set.addView(toggle("Shadows", "wwe12_shadows"));
        set.addView(toggle("Crowd", "wwe12_crowd"));
        set.addView(toggle("60 fps in entrances (heavier)", "wwe12_60fps"));
        // From Yexclsv's work: more settings and one-tap presets.
        set.addView(toggle("Depth of field (blur, heavier)", "wwe12_dof"));
        set.addView(toggle("V-sync (no tearing)", "bd_vsync"));
        set.addView(choice("Texture sharpness at an angle (lower = faster)", "bd_anisotropy",
                new String[] {"Off (fastest)", "2x", "4x", "8x", "16x"},
                new String[] {"0", "2", "4", "8", "16"}));
        set.addView(choice("Loading screens", "wwe12_load_slice_ms",
                new String[] {"Original pace", "About 2x faster"}, new String[] {"0", "12"}));

        // Quick presets for phones that drop frames.
        LinearLayout presets = panel(root, "PERFORMANCE PRESETS");
        presets.addView(text("Sets resolution, shadows, crowd, blur and texture sharpness in one tap.",
                12, Color.GRAY, false));
        presets.addView(presetButton("LOW-END PHONE (fastest)", "540", "false", "false", "false", "0"));
        presets.addView(presetButton("BALANCED", "720", "true", "false", "false", "4"));
        presets.addView(presetButton("QUALITY (strong phones)", "1080", "true", "true", "true", "16"));

        // Controls (app settings, not wwe12.toml)
        // GPU driver: the phone's own, or a custom one (Turnip...) imported from a .zip
        driverBox = panel(root, "GPU DRIVER");
        buildDriverPanel();

        LinearLayout ctl = panel(root, "CONTROLS");
        final CheckBox touch = new CheckBox(this);
        touch.setText("On-screen buttons (hidden while a controller is connected)");
        touch.setTextColor(Color.WHITE);
        touch.setChecked(prefs.getBoolean("touch_controls", true));
        touch.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { prefs.edit().putBoolean("touch_controls", touch.isChecked()).apply(); }
        });
        ctl.addView(touch);
        // #38: on some phones a built-in device passes for a controller and hid the buttons.
        ctl.addView(prefToggle("Always show the on-screen buttons (if they vanish in the game)", "touch_always", false));
        ctl.addView(prefChoice("On-screen buttons opacity", "touch_opacity",
                new String[] {"30 %", "50 %", "70 %", "90 %"}, new int[] {30, 50, 70, 90}, 50));
        ctl.addView(prefChoice("On-screen buttons size", "touch_size",
                new String[] {"Small (70 %)", "Medium (85 %)", "Normal (100 %)", "Large (110 %)"},
                new int[] {70, 85, 100, 110}, 100));
        ctl.addView(prefToggle("Vibration when pressing a button", "touch_haptics", true));
        ctl.addView(prefChoice("Stick dead zone (raise it if the character drifts)", "touch_deadzone",
                new String[] {"None", "5 %", "10 %", "15 %", "20 %", "30 %"}, new int[] {0, 5, 10, 15, 20, 30}, 10));
        ctl.addView(prefChoice("Stick sensitivity", "touch_sens",
                new String[] {"Low (80 %)", "Normal", "High (115 %)", "Very high (130 %)"},
                new int[] {80, 100, 115, 130}, 100));
        Button editLayout = button("EDIT BUTTON LAYOUT (move, resize, custom buttons)", false);
        editLayout.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { startActivity(new Intent(LauncherActivity.this, LayoutEditorActivity.class)); }
        });
        ctl.addView(editLayout);
        controllerStatus = text("", 13, Color.WHITE, false);
        ctl.addView(controllerStatus);
        ctl.addView(text("Bluetooth / USB controllers (Xbox, PlayStation...) work too: pair it in Android's settings.",
                12, Color.GRAY, false));

        // Debug: tests asked of testers, without editing wwe12.toml by hand (Android/data is hard to
        // reach on recent phones). Both keys exist on PC too.
        LinearLayout dbg = panel(root, "DEBUG (only if asked)");
        dbg.addView(toggle("Release unused render targets (untick to test a freeze)",
                "svr_release_stale_targets", true));
        dbg.addView(toggle("Game debug messages in the log (Universe, replays)", "wwe12_game_log", false));
        dbg.addView(toggle("Instant replay after matches (can freeze the game for now)",
                "wwe12_instant_replay", false));
        dbg.addView(toggle("Empty the game's cache at each start", "wwe12_fresh_cache", false));
        dbg.addView(toggle("Log the replay file's reads and writes", "replay_io_log", false));
        dbg.addView(text("Then play until the problem, and send the log with SHARE LAST LOG.",
                12, Color.GRAY, false));

        // Play
        Button play = button("PLAY", true);
        play.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { play(); }
        });
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, (int) (64 * dp));
        lp.topMargin = pad;
        root.addView(play, lp);

        Button share = button("SHARE LAST LOG (for bug reports)", false);
        share.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { shareLog(); }
        });
        root.addView(share);

        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(BG);
        scroll.setFitsSystemWindows(true);  // below the status bar; root keeps its own margins
        scroll.addView(root);
        return scroll;
    }

    TextView text(String s, int sp, int color, boolean bold) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(sp);
        t.setTextColor(color);
        if (bold) t.setTypeface(Typeface.DEFAULT_BOLD, Typeface.ITALIC);
        return t;
    }

    LinearLayout panel(LinearLayout parent, String name) {
        float dp = getResources().getDisplayMetrics().density;
        TextView header = text(name, 16, YELLOW, true);
        header.setPadding(0, (int) (16 * dp), 0, (int) (4 * dp));
        parent.addView(header);
        LinearLayout p = new LinearLayout(this);
        p.setOrientation(LinearLayout.VERTICAL);
        int pad = (int) (12 * dp);
        p.setPadding(pad, pad, pad, pad);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(PANEL);
        bg.setCornerRadius(8 * dp);
        p.setBackground(bg);
        parent.addView(p);
        return p;
    }

    Button button(String label, boolean big) {
        Button b = new Button(this);
        b.setText(label);
        b.setTextColor(big ? Color.BLACK : Color.WHITE);
        b.setTextSize(big ? 22 : 14);
        b.setTypeface(Typeface.DEFAULT_BOLD, Typeface.ITALIC);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(big ? YELLOW : Color.rgb(60, 60, 60));
        bg.setCornerRadius(6 * getResources().getDisplayMetrics().density);
        b.setBackground(bg);
        return b;
    }

    // App setting (not wwe12.toml): a checkbox stored in the launcher preferences.
    View prefToggle(String label, final String key, boolean def) {
        final CheckBox c = new CheckBox(this);
        c.setText(label);
        c.setTextColor(Color.WHITE);
        c.setChecked(prefs.getBoolean(key, def));
        c.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { prefs.edit().putBoolean(key, c.isChecked()).apply(); }
        });
        return c;
    }

    // One-tap graphics preset: resolution, shadows, crowd, depth of field, texture sharpness.
    Button presetButton(String label, final String scale, final String shadows, final String crowd,
                        final String dof, final String aniso) {
        Button b = button(label, false);
        b.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                config.set("svr_render_scale", scale);
                config.set("wwe12_shadows", shadows);
                config.set("wwe12_crowd", crowd);
                config.set("wwe12_dof", dof);
                config.set("bd_anisotropy", aniso);
                config.set("wwe12_60fps", "false");
                config.save();
                recreate();  // rebuilds the settings with the new values
            }
        });
        return b;
    }

    View toggle(String label, final String key) {
        return toggle(label, key, false);
    }

    // def = the game's default when wwe12.toml has no line for the key.
    View toggle(String label, final String key, boolean def) {
        final CheckBox c = new CheckBox(this);
        c.setText(label);
        c.setTextColor(Color.WHITE);
        c.setChecked("true".equals(config.get(key, def ? "true" : "false")));
        c.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                config.set(key, c.isChecked() ? "true" : "false");
                config.save();
            }
        });
        return c;
    }

    View choice(String label, final String key, String[] names, final String[] values) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.VERTICAL);
        row.addView(text(label, 13, Color.LTGRAY, false));
        Spinner s = new Spinner(this);
        ArrayAdapter<String> a = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, names) {
            @Override
            public View getView(int pos, View convert, android.view.ViewGroup parent) {
                View v = super.getView(pos, convert, parent);
                ((TextView) v).setTextColor(Color.WHITE);
                return v;
            }
        };
        a.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        s.setAdapter(a);
        String current = config.get(key, values[0]);
        for (int i = 0; i < values.length; i++)
            if (values[i].equals(current)) s.setSelection(i);
        s.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            public void onItemSelected(AdapterView<?> p, View v, int pos, long id) {
                if (!values[pos].equals(config.get(key, ""))) {
                    config.set(key, values[pos]);
                    config.save();
                }
            }
            public void onNothingSelected(AdapterView<?> p) {}
        });
        row.addView(s);
        return row;
    }

    View prefChoice(String label, final String key, String[] names, final int[] values, int def) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.VERTICAL);
        row.addView(text(label, 13, Color.LTGRAY, false));
        Spinner s = new Spinner(this);
        ArrayAdapter<String> a = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, names) {
            @Override
            public View getView(int pos, View convert, android.view.ViewGroup parent) {
                View v = super.getView(pos, convert, parent);
                ((TextView) v).setTextColor(Color.WHITE);
                return v;
            }
        };
        a.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        s.setAdapter(a);
        int current = prefs.getInt(key, def);
        for (int i = 0; i < values.length; i++)
            if (values[i] == current) s.setSelection(i);
        s.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            public void onItemSelected(AdapterView<?> p, View v, int pos, long id) {
                prefs.edit().putInt(key, values[pos]).apply();
            }
            public void onNothingSelected(AdapterView<?> p) {}
        });
        row.addView(s);
        return row;
    }

    String versionName() {
        try {
            PackageInfo pi = getPackageManager().getPackageInfo(getPackageName(), 0);
            return pi.versionName;
        } catch (Exception e) {
            return "?";
        }
    }

    // ---- GPU driver ----------------------------------------------------------------------------

    void buildDriverPanel() {
        driverBox.removeAllViews();
        final java.util.List<DriverManager.Driver> drivers = DriverManager.list(this);
        driverBox.addView(text("GPU: " + gpuName(), 12, Color.LTGRAY, false));
        if (gpuLooksUnsupported())
            driverBox.addView(text("This GPU looks like a Mali / Tensor / Exynos chip: the game may show a black screen"
                    + " on it, even with a custom driver (custom drivers only exist for Snapdragon / Adreno).",
                    12, Color.rgb(255, 140, 90), false));
        driverBox.addView(text("Driver used by the game", 13, Color.LTGRAY, false));

        final String[] ids = new String[drivers.size() + 1];
        String[] names = new String[drivers.size() + 1];
        ids[0] = "";
        names[0] = "System driver (default)";
        String current = prefs.getString(DriverManager.PREF_SELECTED, "");
        int sel = 0;
        for (int i = 0; i < drivers.size(); i++) {
            ids[i + 1] = drivers.get(i).id;
            names[i + 1] = drivers.get(i).toString();
            if (drivers.get(i).id.equals(current)) sel = i + 1;
        }
        Spinner s = new Spinner(this);
        ArrayAdapter<String> a = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, names) {
            @Override
            public View getView(int pos, View convert, android.view.ViewGroup parent) {
                View v = super.getView(pos, convert, parent);
                ((TextView) v).setTextColor(Color.WHITE);
                return v;
            }
        };
        a.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        s.setAdapter(a);
        s.setSelection(sel);
        s.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            public void onItemSelected(AdapterView<?> p, View v, int pos, long id) {
                DriverManager.select(LauncherActivity.this, ids[pos]);
            }
            public void onNothingSelected(AdapterView<?> p) {}
        });
        driverBox.addView(s);

        Button imp = button("IMPORT DRIVER (.zip)", false);
        imp.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                i.addCategory(Intent.CATEGORY_OPENABLE);
                i.setType("*/*");
                startActivityForResult(i, PICK_DRIVER);
            }
        });
        driverBox.addView(imp);

        if (sel > 0) {
            final String id = ids[sel];
            Button del = button("DELETE THIS DRIVER", false);
            del.setOnClickListener(new View.OnClickListener() {
                public void onClick(View v) {
                    DriverManager.remove(LauncherActivity.this, id);
                    buildDriverPanel();
                }
            });
            driverBox.addView(del);
        }
        driverBox.addView(text("If the game shows a black screen, try a recent Turnip driver (Adreno only). "
                + "Takes effect at the next PLAY. The log tells which driver was really used.", 12, Color.GRAY, false));
    }

    static String gpuName() {
        String soc = android.os.Build.VERSION.SDK_INT >= 31 ? android.os.Build.SOC_MODEL : "";
        return android.os.Build.MANUFACTURER + " " + android.os.Build.MODEL
                + " (" + android.os.Build.HARDWARE + (soc.isEmpty() || soc.equals("unknown") ? "" : ", " + soc) + ")";
    }

    // Best guess from the chip name: Java cannot ask Vulkan directly. Only used for a hint.
    static boolean gpuLooksUnsupported() {
        String soc = android.os.Build.VERSION.SDK_INT >= 31 ? android.os.Build.SOC_MODEL : "";
        String s = (android.os.Build.HARDWARE + " " + soc + " " + android.os.Build.BOARD).toLowerCase();
        return s.contains("exynos") || s.contains("s5e") || s.contains("tensor") || s.contains("gs101")
                || s.contains("gs201") || s.contains("zuma") || s.contains("mt6") || s.contains("mt8")
                || s.contains("mediatek") || s.contains("kirin") || s.contains("unisoc");
    }

    // ---- Device warning ------------------------------------------------------------------------

    void checkDevice() {
        final File dir = getExternalFilesDir(null);
        new Thread(new Runnable() {
            public void run() {
                final DeviceCheck d = new DeviceCheck(LauncherActivity.this, dir);
                runOnUiThread(new Runnable() {
                    public void run() {
                        device = d;
                        deviceBox.removeAllViews();
                        if (!d.ok()) {
                            LinearLayout warn = panel(deviceBox, d.blockers.isEmpty() ? "THIS PHONE MAY STRUGGLE"
                                    : "THIS PHONE CANNOT RUN THE GAME");
                            for (String s : d.blockers) warn.addView(text("• " + s, 14, Color.rgb(255, 110, 90), false));
                            for (String s : d.warnings) warn.addView(text("• " + s, 14, Color.rgb(255, 190, 90), false));
                        }
                        deviceBox.addView(text(d.summary, 11, Color.GRAY, false));
                    }
                });
            }
        }).start();
    }

    // ---- DLC ---------------------------------------------------------------------------------

    void buildDlcPanel() {
        dlcBox.removeAllViews();
        java.util.List<GameFiles.Dlc> list = GameFiles.listDlc(this);
        dlcBox.addView(text(list.isEmpty() ? "No DLC yet." : list.size() + " DLC package(s):", 14, Color.WHITE, false));
        for (final GameFiles.Dlc d : list) {
            LinearLayout row = new LinearLayout(this);
            row.setOrientation(LinearLayout.HORIZONTAL);
            row.setGravity(Gravity.CENTER_VERTICAL);
            TextView name = text(d.file.getName() + "\n" + GameFiles.formatSize(d.file.length())
                    + (d.installed ? ", installed" : ", installed at the next start"), 12, Color.LTGRAY, false);
            row.addView(name, new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1));
            Button del = button("REMOVE", false);
            del.setOnClickListener(new View.OnClickListener() {
                public void onClick(View v) {
                    new android.app.AlertDialog.Builder(LauncherActivity.this)
                            .setMessage("Remove " + d.file.getName() + "? Its wrestlers and items leave the game.")
                            .setPositiveButton("Remove", new android.content.DialogInterface.OnClickListener() {
                                public void onClick(android.content.DialogInterface di, int w) {
                                    if (GameFiles.gameRunning(LauncherActivity.this)) {
                                        Toast.makeText(LauncherActivity.this, "Close the game first.", Toast.LENGTH_LONG).show();
                                        return;
                                    }
                                    GameFiles.removeDlc(LauncherActivity.this, d);
                                    buildDlcPanel();
                                }
                            })
                            .setNegativeButton("Cancel", null).show();
                }
            });
            row.addView(del);
            dlcBox.addView(row);
        }
        dlcStatus = text("", 12, Color.rgb(240, 191, 61), false);
        dlcBox.addView(dlcStatus);
        Button add = button("ADD DLC FILES", false);
        add.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                i.addCategory(Intent.CATEGORY_OPENABLE);
                i.setType("*/*");
                i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
                startActivityForResult(i, PICK_DLC);
            }
        });
        dlcBox.addView(add);
        Button addFolder = button("ADD A DLC FOLDER", false);
        addFolder.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), PICK_DLC_FOLDER);
            }
        });
        dlcBox.addView(addFolder);
        dlcBox.addView(text("Pick the DLC packages of your Xbox 360 (files with long names such as 961C4A43...54, "
                + "from Content\\0000000000000000\\54510890\\00000002), several at once, or a whole folder: "
                + "only WWE '12 DLC packages are taken from it.", 12, Color.GRAY, false));
    }

    void addDlc(Intent data, boolean folder) {
        final java.util.List<Uri> uris = new java.util.ArrayList<>();
        final Uri tree = folder ? data.getData() : null;
        if (folder) {
            if (tree == null) return;
        } else if (data.getClipData() != null)
            for (int k = 0; k < data.getClipData().getItemCount(); k++) uris.add(data.getClipData().getItemAt(k).getUri());
        else if (data.getData() != null)
            uris.add(data.getData());
        if (tree == null && uris.isEmpty()) return;
        if (GameFiles.gameRunning(this)) {
            Toast.makeText(this, "Close the game first.", Toast.LENGTH_LONG).show();
            return;
        }
        dlcStatus.setText(tree != null ? "Looking in the folder..." : "Copying...");
        new Thread(new Runnable() {
            public void run() {
                if (tree != null) uris.addAll(GameFiles.treeFiles(LauncherActivity.this, tree, 5000));
                final String report = GameFiles.addDlc(LauncherActivity.this, uris, new GameFiles.Progress() {
                    public void progress(final String t) {
                        runOnUiThread(new Runnable() { public void run() { dlcStatus.setText("Copying " + t); } });
                    }
                });
                runOnUiThread(new Runnable() {
                    public void run() {
                        buildDlcPanel();
                        dlcStatus.setText(report);
                    }
                });
            }
        }).start();
    }

    // ---- Game --------------------------------------------------------------------------------

    void pickIso() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        i.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {
                "application/x-iso9660-image", "application/octet-stream", "application/x-cd-image"});
        startActivityForResult(i, PICK_ISO);
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request == PICK_DLC || request == PICK_DLC_FOLDER) {
            if (result == RESULT_OK && data != null) addDlc(data, request == PICK_DLC_FOLDER);
            return;
        }
        if (request == PICK_DRIVER && result == RESULT_OK && data != null && data.getData() != null) {
            try {
                String id = DriverManager.importZip(this, data.getData());
                DriverManager.select(this, id);
                Toast.makeText(this, "Driver imported and selected.", Toast.LENGTH_SHORT).show();
            } catch (Exception e) {
                Toast.makeText(this, "Import failed: " + e.getMessage(), Toast.LENGTH_LONG).show();
            }
            buildDriverPanel();
            return;
        }
        if (request != PICK_ISO || result != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        try {
            getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (Exception e) {
            // Some pickers give no persistable access: it still works until the app is closed.
        }
        prefs.edit().putString("iso_uri", uri.toString()).putString("iso_name", displayName(uri)).apply();
        refreshGame();
    }

    String displayName(Uri uri) {
        try (Cursor c = getContentResolver().query(uri, new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (c != null && c.moveToFirst()) return c.getString(0);
        } catch (Exception ignored) {
        }
        return uri.getLastPathSegment();
    }

    void refreshGame() {
        String local = WWE12Activity.findLocalGame(getExternalFilesDir(null));
        String iso = prefs.getString("iso_name", null);
        forget.setVisibility(prefs.getString("iso_uri", null) != null ? View.VISIBLE : View.GONE);
        if (prefs.getString("iso_uri", null) != null && iso != null) {
            gameStatus.setText("Game: " + iso);
        } else if (local != null) {
            gameStatus.setText(local.toLowerCase().endsWith(".iso")
                    ? "Game: " + new File(local).getName() + " (game folder)"
                    : "Game: files copied into the game folder");
        } else {
            gameStatus.setText("No game yet: choose your WWE '12 ISO (Xbox 360).");
        }
    }

    // The ARM64 game is built for ARMv8.2 (port/cmake/android.cmake): on an older core it would
    // crash at its first atomic instruction. ARMv8.2 always has LSE atomics ("atomics" in
    // /proc/cpuinfo). Only ARM lists "Features": an x86 device (emulator) is never refused.
    static boolean cpuTooOld() {
        try (java.io.BufferedReader r = new java.io.BufferedReader(new java.io.FileReader("/proc/cpuinfo"))) {
            for (String line; (line = r.readLine()) != null; ) {
                if (line.startsWith("Features"))
                    return !(" " + line.substring(line.indexOf(':') + 1) + " ").contains(" atomics ");
            }
        } catch (Exception ignored) {
        }
        return false;
    }

    // PLAY: when the device warning found something, say so first (Yexclsv's idea). The player can
    // still go on; for warnings only, also stop asking.
    void play() {
        final DeviceCheck d = device;
        if (d != null && !d.ok() && (!d.blockers.isEmpty() || !prefs.getBoolean("ignore_device_warning", false))) {
            StringBuilder sb = new StringBuilder();
            for (String s : d.blockers) sb.append("• ").append(s).append("\n\n");
            for (String s : d.warnings) sb.append("• ").append(s).append("\n\n");
            sb.append("The game may not start, or may run badly, on this phone.");
            android.app.AlertDialog.Builder b = new android.app.AlertDialog.Builder(this)
                    .setTitle("This phone may not run the game")
                    .setMessage(sb.toString())
                    .setNegativeButton("CANCEL", null)
                    .setPositiveButton("PLAY ANYWAY", new android.content.DialogInterface.OnClickListener() {
                        public void onClick(android.content.DialogInterface di, int w) { startGame(); }
                    });
            if (d.blockers.isEmpty())
                b.setNeutralButton("PLAY, DON'T ASK AGAIN", new android.content.DialogInterface.OnClickListener() {
                    public void onClick(android.content.DialogInterface di, int w) {
                        prefs.edit().putBoolean("ignore_device_warning", true).apply();
                        startGame();
                    }
                });
            b.show();
            return;
        }
        startGame();
    }

    void startGame() {
        if (cpuTooOld()) {
            Toast.makeText(this, "This processor is too old for the game (ARMv8.2 needed).", Toast.LENGTH_LONG).show();
            return;
        }
        Intent i = new Intent(this, WWE12Activity.class);
        String uri = prefs.getString("iso_uri", null);
        if (uri != null) {
            // Checked here; the game process opens it again for itself.
            try (android.os.ParcelFileDescriptor pfd =
                         getContentResolver().openFileDescriptor(Uri.parse(uri), "r")) {
                if (pfd == null) throw new java.io.IOException("no descriptor");
            } catch (Exception e) {
                Toast.makeText(this, "Cannot open the ISO any more: choose it again.", Toast.LENGTH_LONG).show();
                prefs.edit().remove("iso_uri").remove("iso_name").apply();
                refreshGame();
                return;
            }
            i.putExtra(WWE12Activity.EXTRA_GAME_URI, uri);
        } else if (WWE12Activity.findLocalGame(getExternalFilesDir(null)) == null) {
            Toast.makeText(this, "Choose your WWE '12 ISO first.", Toast.LENGTH_LONG).show();
            return;
        }
        startActivity(i);
    }

    // Last log (logs/wwe12_NNN.log), shared as text: testers paste it in Discord or GitHub.
    void shareLog() {
        File logs = new File(getExternalFilesDir(null), "logs");
        File[] files = logs.listFiles();
        File last = null;
        if (files != null)
            for (File f : files)
                if (f.getName().endsWith(".log") && (last == null || f.lastModified() > last.lastModified()))
                    last = f;
        if (last == null) {
            Toast.makeText(this, "No log yet: play once first.", Toast.LENGTH_SHORT).show();
            return;
        }
        // Shared as text, so it must stay small: the start of the log (phone, GPU, settings) and its
        // end (what happened last), without the pipeline detail lines that fill most of it.
        String body;
        try (java.io.BufferedReader r = new java.io.BufferedReader(
                new java.io.InputStreamReader(new java.io.FileInputStream(last), "UTF-8"))) {
            StringBuilder head = new StringBuilder();
            java.util.ArrayDeque<String> tail = new java.util.ArrayDeque<>();
            long tailSize = 0, max = 180 * 1024;
            int n = 0, dropped = 0;
            for (String l; (l = r.readLine()) != null; ) {
                if (l.contains("SvR pipeline: element") || l.contains("vertex declaration 0x")
                        || l.contains("WWE12 shader 0x")) continue;
                if (n++ < 150) { head.append(l).append('\n'); continue; }
                tail.addLast(l);
                tailSize += l.length() + 1;
                while (tailSize > max) { tailSize -= tail.removeFirst().length() + 1; dropped++; }
            }
            StringBuilder b = new StringBuilder(head);
            if (dropped > 0) b.append("\n[... ").append(dropped).append(" lines left out ...]\n\n");
            for (String l : tail) b.append(l).append('\n');
            body = b.toString();
        } catch (Exception e) {
            body = "cannot read " + last + ": " + e;
        }
        Intent i = new Intent(Intent.ACTION_SEND);
        i.setType("text/plain");
        i.putExtra(Intent.EXTRA_SUBJECT, "WWE12 Android log " + last.getName());
        i.putExtra(Intent.EXTRA_TEXT, body);
        startActivity(Intent.createChooser(i, "Share log"));
    }

    // ---- wwe12.toml: "key = value" lines, edited in place like the PC launcher -----------------

    static class Config {
        final File file;
        final java.util.List<String> lines = new java.util.ArrayList<>();

        Config(File file) {
            this.file = file;
            try (java.io.BufferedReader r = new java.io.BufferedReader(new java.io.FileReader(file))) {
                for (String l; (l = r.readLine()) != null; ) lines.add(l);
            } catch (Exception ignored) {
            }
        }

        int find(String key) {
            for (int i = 0; i < lines.size(); i++) {
                String l = lines.get(i).trim();
                if (l.startsWith(key) && l.substring(key.length()).trim().startsWith("=")) return i;
            }
            return -1;
        }

        String get(String key, String def) {
            int i = find(key);
            if (i < 0) return def;
            String v = lines.get(i).substring(lines.get(i).indexOf('=') + 1).trim();
            int hash = v.indexOf(" #");
            if (hash >= 0) v = v.substring(0, hash).trim();
            return v.replace("\"", "");
        }

        void set(String key, String value) {
            int i = find(key);
            String line = key + " = " + value;
            if (i >= 0) lines.set(i, line);
            else lines.add(line);
        }

        void save() {
            try (java.io.PrintWriter w = new java.io.PrintWriter(file, "UTF-8")) {
                for (String l : lines) w.println(l);
            } catch (Exception ignored) {
            }
        }
    }
}
