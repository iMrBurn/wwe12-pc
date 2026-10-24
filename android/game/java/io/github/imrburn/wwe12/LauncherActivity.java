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
    static final int YELLOW = Color.rgb(240, 191, 61);   // same accent as the PC launcher
    static final int BG = Color.rgb(17, 17, 17);
    static final int PANEL = Color.rgb(32, 32, 32);

    Config config;
    SharedPreferences prefs;
    TextView gameStatus;
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

        // Settings
        LinearLayout set = panel(root, "SETTINGS");
        set.addView(choice("Language", "user_language",
                new String[] {"English", "Deutsch", "Français", "Español", "Italiano"},
                new String[] {"1", "3", "4", "5", "6"}));
        set.addView(choice("Internal resolution (lower = faster)", "svr_render_scale",
                new String[] {"540p (fastest)", "720p (recommended)", "1080p", "Automatic (screen size)"},
                new String[] {"540", "720", "1080", "0"}));
        set.addView(toggle("FPS counter", "wwe12_show_fps"));
        set.addView(toggle("Shadows", "wwe12_shadows"));
        set.addView(toggle("Crowd", "wwe12_crowd"));
        set.addView(toggle("60 fps in entrances (heavier)", "wwe12_60fps"));

        // Controls (app settings, not wwe12.toml)
        LinearLayout ctl = panel(root, "CONTROLS");
        final CheckBox touch = new CheckBox(this);
        touch.setText("On-screen buttons (hidden while a controller is connected)");
        touch.setTextColor(Color.WHITE);
        touch.setChecked(prefs.getBoolean("touch_controls", true));
        touch.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { prefs.edit().putBoolean("touch_controls", touch.isChecked()).apply(); }
        });
        ctl.addView(touch);
        ctl.addView(prefChoice("On-screen buttons opacity", "touch_opacity",
                new String[] {"30 %", "50 %", "70 %", "90 %"}, new int[] {30, 50, 70, 90}, 50));
        ctl.addView(text("Bluetooth / USB controllers (Xbox, PlayStation...) work too: pair it in Android's settings.",
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

    View toggle(String label, final String key) {
        final CheckBox c = new CheckBox(this);
        c.setText(label);
        c.setTextColor(Color.WHITE);
        c.setChecked("true".equals(config.get(key, "false")));
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

    void play() {
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
        String body;
        try (RandomAccessFile f = new RandomAccessFile(last, "r")) {
            long size = f.length(), max = 200 * 1024;  // the end of the log is what matters
            f.seek(Math.max(0, size - max));
            byte[] buf = new byte[(int) Math.min(size, max)];
            f.readFully(buf);
            body = new String(buf, "UTF-8");
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
