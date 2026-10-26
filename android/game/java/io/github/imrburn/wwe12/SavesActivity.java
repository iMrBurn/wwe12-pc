// MY SAVES screen: the PC launcher's SAVES page (port/src/launcher/saves.h) for Android.
// The save lives in wwe12/ like in Documents\wwe12 on PC:
//   B13EBABEBABEBABE/54510890/...   the save packages (00000001/SaveData.dat...) and their headers
//   achievements/54510890.toml      unlocked achievements
// A backup mirrors those two paths under wwe12/SaveBackups/<date>/ (same layout as on PC, so a
// backup made on one can be restored on the other). Restoring, deleting the save or importing
// first makes an automatic backup; the latest 10 automatic ones are kept.
// Android hides Android/data from most file managers, so backups can also leave and come back
// as .zip files through Android's file picker. Xbox 360 save packages are copied into
// saves_import/ and listed in saves_import.txt: the game installs them at its next start
// (port/src/wwe12_saves.cpp) and writes saves_import_result.txt.
package io.github.imrburn.wwe12;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

public class SavesActivity extends Activity {
    static final int PICK_XBOX = 1;
    static final int PICK_ZIP = 2;
    static final int EXPORT_ZIP = 3;
    static final int PICK_XBOX_FOLDER = 4;
    static final int KEEP_AUTOMATIC = 10;

    LinearLayout root;
    TextView status;
    EditText label;
    String exporting;   // backup waiting for the place to write its .zip
    boolean busy;

    File data() { return GameFiles.userData(this); }
    File saveDir() { return new File(data(), GameFiles.PROFILE + "/" + GameFiles.TITLE); }
    File achFile() { return new File(data(), "achievements/" + GameFiles.TITLE + ".toml"); }
    File backupsDir() { return new File(data(), "SaveBackups"); }
    File importList() { return new File(GameFiles.appDir(this), "saves_import.txt"); }
    File importResult() { return new File(GameFiles.appDir(this), "saves_import_result.txt"); }
    File importStage() { return new File(GameFiles.appDir(this), "saves_import"); }

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        float dp = getResources().getDisplayMetrics().density;
        int pad = (int) (16 * dp);
        root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(pad, pad, pad, pad);
        root.setBackgroundColor(LauncherActivity.BG);
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(LauncherActivity.BG);
        scroll.setFitsSystemWindows(true);
        scroll.addView(root);
        setContentView(scroll);
        rebuild();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (!busy) rebuild();
    }

    // ---- screen --------------------------------------------------------------------------------

    void rebuild() {
        root.removeAllViews();
        root.addView(text("MY SAVES", 28, LauncherActivity.YELLOW, true));
        root.addView(text("A backup copies your save and your achievements (not the DLC). Restoring, deleting or "
                + "importing always backs up your current save first.", 13, Color.LTGRAY, false));
        status = text("", 13, LauncherActivity.YELLOW, false);
        root.addView(status);
        final boolean running = GameFiles.gameRunning(this);
        if (running)
            root.addView(text("The game is running: close it to manage your saves.", 14, Color.rgb(255, 110, 90), true));

        LinearLayout cur = panel("CURRENT SAVE");
        if (saveDir().isDirectory())
            cur.addView(text(GameFiles.formatSize(GameFiles.size(saveDir())) + ", last saved "
                    + when(GameFiles.newest(saveDir())), 14, Color.WHITE, false));
        else
            cur.addView(text("No save yet: play and save in the game.", 14, Color.WHITE, false));
        label = new EditText(this);
        label.setHint("Backup name (optional)");
        label.setHintTextColor(Color.GRAY);
        label.setTextColor(Color.WHITE);
        label.setInputType(InputType.TYPE_CLASS_TEXT);
        cur.addView(label);
        cur.addView(action("CREATE A BACKUP", running, new Runnable() {
            public void run() {
                final String l = label.getText().toString().trim();
                work(new Job() {
                    public String run() throws IOException {
                        String made = makeBackup(false, l);
                        return made == null ? "Nothing to back up yet." : "Backup created: " + made;
                    }
                });
            }
        }));
        cur.addView(action("START FROM SCRATCH", running, new Runnable() {
            public void run() {
                confirm("Delete your current save and start from scratch? It is backed up first, so you can restore it.",
                        new Job() {
                            public String run() throws IOException {
                                String kept = makeBackup(true, "");
                                if (kept == null) return "Nothing to delete.";
                                GameFiles.deleteTree(saveDir());
                                return "Save removed. It was kept as " + kept;
                            }
                        });
            }
        }));

        LinearLayout imp = panel("IMPORT");
        boolean pending = importList().exists();
        if (pending)
            imp.addView(text("An Xbox 360 save waits for the next game start.", 14, LauncherActivity.YELLOW, false));
        else if (importResult().exists())
            imp.addView(text("Last Xbox 360 import: " + readResult(), 13, Color.LTGRAY, false));
        imp.addView(text("Xbox 360 save: pick the save files copied from your console (for example SaveData.dat, "
                + "00CreateSuperStar.cas). Packages with the same name as yours are replaced, your other saves stay.",
                12, Color.GRAY, false));
        imp.addView(action("IMPORT XBOX 360 SAVE FILES", running || pending, new Runnable() {
            public void run() { pick(PICK_XBOX, true); }
        }));
        imp.addView(action("IMPORT A WHOLE FOLDER", running || pending, new Runnable() {
            public void run() { startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), PICK_XBOX_FOLDER); }
        }));
        if (pending)
            imp.addView(action("CANCEL THE IMPORT", running, new Runnable() {
                public void run() {
                    importList().delete();
                    GameFiles.deleteTree(importStage());
                    rebuild();
                }
            }));
        imp.addView(text("Backup .zip: one exported here (from this phone, another phone, or zipped from the PC "
                + "version's SaveBackups folder). It is added to the list below; restore it from there.",
                12, Color.GRAY, false));
        imp.addView(action("ADD A BACKUP FROM A .ZIP", running, new Runnable() {
            public void run() { pick(PICK_ZIP, false); }
        }));

        LinearLayout list = panel("BACKUPS");
        List<File> backups = backups();
        if (backups.isEmpty()) list.addView(text("No backups yet.", 14, Color.WHITE, false));
        for (final File b : backups) {
            String[] info = info(b);
            String n = b.getName();  // 2026-10-13_10-29-42 -> 2026-10-13 10:29:42
            String shown = n.length() >= 19 ? n.substring(0, 10) + " " + n.substring(11, 19).replace('-', ':') : n;
            list.addView(text(shown + "  " + GameFiles.formatSize(GameFiles.size(b)),
                    14, Color.WHITE, true));
            list.addView(text("auto".equals(info[0]) ? "automatic, before a change" : info[1].isEmpty() ? "-" : info[1],
                    12, Color.LTGRAY, false));
            LinearLayout row = new LinearLayout(this);
            row.setOrientation(LinearLayout.HORIZONTAL);
            row.addView(action("RESTORE", running, new Runnable() {
                public void run() {
                    confirm("Restore this backup? Your current save is backed up first.", new Job() {
                        public String run() throws IOException { return restore(b); }
                    });
                }
            }), weight());
            row.addView(action("EXPORT .ZIP", false, new Runnable() {
                public void run() {
                    exporting = b.getName();
                    Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                    i.addCategory(Intent.CATEGORY_OPENABLE);
                    i.setType("application/zip");
                    i.putExtra(Intent.EXTRA_TITLE, "WWE12-save-" + b.getName() + ".zip");
                    startActivityForResult(i, EXPORT_ZIP);
                }
            }), weight());
            row.addView(action("DELETE", running, new Runnable() {
                public void run() {
                    confirm("Delete this backup for good?", new Job() {
                        public String run() {
                            GameFiles.deleteTree(b);
                            return "Backup deleted.";
                        }
                    });
                }
            }), weight());
            list.addView(row);
        }
    }

    LinearLayout.LayoutParams weight() {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1);
        lp.setMargins(4, 4, 4, 4);
        return lp;
    }

    TextView text(String s, int sp, int color, boolean bold) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(sp);
        t.setTextColor(color);
        if (bold) t.setTypeface(Typeface.DEFAULT_BOLD, Typeface.ITALIC);
        return t;
    }

    LinearLayout panel(String name) {
        float dp = getResources().getDisplayMetrics().density;
        TextView header = text(name, 16, LauncherActivity.YELLOW, true);
        header.setPadding(0, (int) (16 * dp), 0, (int) (4 * dp));
        root.addView(header);
        LinearLayout p = new LinearLayout(this);
        p.setOrientation(LinearLayout.VERTICAL);
        int pad = (int) (12 * dp);
        p.setPadding(pad, pad, pad, pad);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(LauncherActivity.PANEL);
        bg.setCornerRadius(8 * dp);
        p.setBackground(bg);
        root.addView(p);
        return p;
    }

    Button action(String label, boolean disabled, final Runnable r) {
        Button b = new Button(this);
        b.setText(label);
        b.setTextColor(Color.WHITE);
        b.setTextSize(13);
        b.setTypeface(Typeface.DEFAULT_BOLD, Typeface.ITALIC);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.rgb(60, 60, 60));
        bg.setCornerRadius(6 * getResources().getDisplayMetrics().density);
        b.setBackground(bg);
        b.setGravity(Gravity.CENTER);
        b.setEnabled(!disabled);
        b.setAlpha(disabled ? 0.4f : 1f);
        b.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { if (!busy) r.run(); }
        });
        return b;
    }

    interface Job { String run() throws IOException; }

    void confirm(String question, final Job job) {
        new AlertDialog.Builder(this).setMessage(question)
                .setPositiveButton("Yes", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface d, int w) { work(job); }
                })
                .setNegativeButton("Cancel", null).show();
    }

    // Runs a job off the screen's thread (copies can take a few seconds), then shows its result.
    void work(final Job job) {
        if (GameFiles.gameRunning(this)) {
            status.setText("The game is running: close it to manage your saves.");
            return;
        }
        busy = true;
        status.setText("Working...");
        new Thread(new Runnable() {
            public void run() {
                String msg;
                try {
                    msg = job.run();
                } catch (IOException e) {
                    msg = "Failed: " + e.getMessage();
                }
                final String m = msg;
                runOnUiThread(new Runnable() {
                    public void run() {
                        busy = false;
                        rebuild();
                        status.setText(m);
                    }
                });
            }
        }).start();
    }

    static String when(long ms) {
        return new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US).format(new Date(ms));
    }

    // ---- backups -------------------------------------------------------------------------------

    List<File> backups() {
        File[] dirs = backupsDir().listFiles();
        List<File> out = new ArrayList<>();
        if (dirs != null) for (File d : dirs) if (d.isDirectory()) out.add(d);
        java.util.Collections.sort(out, new java.util.Comparator<File>() {
            public int compare(File a, File b) { return b.getName().compareTo(a.getName()); }  // newest first
        });
        return out;
    }

    // backup.txt: "auto" or "manual", then the player's label (same file as the PC launcher).
    static String[] info(File backup) {
        String[] r = {"manual", ""};
        try (BufferedReader in = new BufferedReader(new FileReader(new File(backup, "backup.txt")))) {
            String kind = in.readLine(), l = in.readLine();
            if (kind != null) r[0] = kind.trim();
            if (l != null) r[1] = l.trim();
        } catch (IOException ignored) {
        }
        return r;
    }

    File newBackupDir() {
        String stamp = new SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.US).format(new Date());
        File dest = new File(backupsDir(), stamp);
        for (int n = 2; dest.exists(); n++) dest = new File(backupsDir(), stamp + "_" + n);
        return dest;
    }

    // Returns the new backup's name, or null when there is nothing to back up.
    String makeBackup(boolean automatic, String lbl) throws IOException {
        boolean hasSave = saveDir().isDirectory(), hasAch = achFile().isFile();
        if (!hasSave && !hasAch) return null;
        File dest = newBackupDir();
        try {
            if (hasSave) copyTree(saveDir(), new File(dest, GameFiles.PROFILE + "/" + GameFiles.TITLE));
            if (hasAch) copyFile(achFile(), new File(dest, "achievements/" + achFile().getName()));
            writeInfo(dest, automatic ? "auto" : "manual", lbl);
        } catch (IOException e) {
            GameFiles.deleteTree(dest);
            throw e;
        }
        if (automatic) pruneAutomatic();
        return dest.getName();
    }

    static void writeInfo(File dest, String kind, String lbl) throws IOException {
        try (OutputStream o = new FileOutputStream(new File(dest, "backup.txt"))) {
            o.write((kind + "\n" + lbl + "\n").getBytes("UTF-8"));
        }
    }

    void pruneAutomatic() {
        int seen = 0;
        for (File b : backups())
            if ("auto".equals(info(b)[0]) && ++seen > KEEP_AUTOMATIC) GameFiles.deleteTree(b);
    }

    String restore(File b) throws IOException {
        File save = new File(b, GameFiles.PROFILE + "/" + GameFiles.TITLE);
        File ach = new File(b, "achievements/" + achFile().getName());
        if (!save.isDirectory() && !ach.isFile()) return "This backup contains no save.";
        String kept = makeBackup(true, "");
        if (save.isDirectory()) {
            GameFiles.deleteTree(saveDir());
            copyTree(save, saveDir());
        }
        if (ach.isFile()) copyFile(ach, achFile());
        return kept == null ? "Backup restored." : "Backup restored. Your previous save was kept as " + kept;
    }

    static void copyFile(File from, File to) throws IOException {
        to.getParentFile().mkdirs();
        try (InputStream in = new FileInputStream(from); OutputStream out = new FileOutputStream(to)) {
            byte[] buf = new byte[1 << 16];
            for (int n; (n = in.read(buf)) > 0; ) out.write(buf, 0, n);
        }
        to.setLastModified(from.lastModified());
    }

    static void copyTree(File from, File to) throws IOException {
        if (from.isFile()) { copyFile(from, to); return; }
        to.mkdirs();
        File[] kids = from.listFiles();
        if (kids != null) for (File k : kids) copyTree(k, new File(to, k.getName()));
    }

    // ---- pickers -------------------------------------------------------------------------------

    void pick(int request, boolean multiple) {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        if (multiple) i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        startActivityForResult(i, request);
    }

    @Override
    protected void onActivityResult(int request, int result, final Intent data) {
        super.onActivityResult(request, result, data);
        if (result != RESULT_OK || data == null) return;
        if (request == EXPORT_ZIP && data.getData() != null && exporting != null) {
            final File b = new File(backupsDir(), exporting);
            final Uri to = data.getData();
            work(new Job() {
                public String run() throws IOException {
                    exportZip(b, to);
                    return "Backup exported.";
                }
            });
        } else if (request == PICK_ZIP && data.getData() != null) {
            final Uri from = data.getData();
            work(new Job() {
                public String run() throws IOException { return importZip(from); }
            });
        } else if (request == PICK_XBOX_FOLDER && data.getData() != null) {
            final Uri tree = data.getData();
            work(new Job() {
                public String run() throws IOException {
                    return importXbox(GameFiles.treeFiles(SavesActivity.this, tree, 5000));
                }
            });
        } else if (request == PICK_XBOX) {
            final List<Uri> uris = new ArrayList<>();
            if (data.getClipData() != null)
                for (int k = 0; k < data.getClipData().getItemCount(); k++) uris.add(data.getClipData().getItemAt(k).getUri());
            else if (data.getData() != null)
                uris.add(data.getData());
            work(new Job() {
                public String run() throws IOException { return importXbox(uris); }
            });
        }
    }

    // ---- zip -----------------------------------------------------------------------------------

    void exportZip(File backup, Uri to) throws IOException {
        try (OutputStream raw = getContentResolver().openOutputStream(to);
             ZipOutputStream zip = new ZipOutputStream(raw)) {
            if (raw == null) throw new IOException("cannot write there");
            addToZip(zip, backup, "");
        }
    }

    static void addToZip(ZipOutputStream zip, File f, String path) throws IOException {
        File[] kids = f.listFiles();
        if (kids == null) return;
        Arrays.sort(kids);
        for (File k : kids) {
            String name = path + k.getName();
            if (k.isDirectory()) { addToZip(zip, k, name + "/"); continue; }
            ZipEntry e = new ZipEntry(name);
            e.setTime(k.lastModified());
            zip.putNextEntry(e);
            try (InputStream in = new FileInputStream(k)) {
                byte[] buf = new byte[1 << 16];
                for (int n; (n = in.read(buf)) > 0; ) zip.write(buf, 0, n);
            }
            zip.closeEntry();
        }
    }

    // Keeps only the save and the achievements, wherever they sit in the zip (a backup folder
    // zipped whole on PC has one more folder level). Anything else is ignored.
    String importZip(Uri from) throws IOException {
        File dest = newBackupDir();
        String saveMark = GameFiles.PROFILE + "/" + GameFiles.TITLE + "/";
        String achMark = "achievements/" + GameFiles.TITLE + ".toml";
        int files = 0;
        try (InputStream raw = getContentResolver().openInputStream(from); ZipInputStream zip = new ZipInputStream(raw)) {
            if (raw == null) throw new IOException("cannot open the zip");
            long total = 0;
            for (ZipEntry e; (e = zip.getNextEntry()) != null; ) {
                String name = e.getName().replace('\\', '/');
                if (e.isDirectory() || name.contains("../")) continue;
                int at = name.indexOf(saveMark);
                String rel = at >= 0 ? name.substring(at) : name.endsWith(achMark) ? achMark : null;
                if (rel == null) continue;
                File out = new File(dest, rel);
                out.getParentFile().mkdirs();
                try (OutputStream o = new FileOutputStream(out)) {
                    byte[] buf = new byte[1 << 16];
                    for (int n; (n = zip.read(buf)) > 0; ) {
                        total += n;
                        if (total > (512L << 20)) throw new IOException("zip too big for a save");
                        o.write(buf, 0, n);
                    }
                }
                files++;
            }
        } catch (IOException e) {
            GameFiles.deleteTree(dest);
            throw e;
        }
        if (files == 0) {
            GameFiles.deleteTree(dest);
            return "No WWE '12 save found in this zip.";
        }
        writeInfo(dest, "manual", "imported zip");
        return "Backup added (" + files + " files): restore it below to use it.";
    }

    // ---- Xbox 360 saves ------------------------------------------------------------------------

    String importXbox(List<Uri> uris) throws IOException {
        GameFiles.deleteTree(importStage());
        int found = 0, skipped = 0;
        for (Uri u : uris) {
            byte[] h = GameFiles.head(this, u, 0x364);
            if (!GameFiles.isStfs(h) || GameFiles.contentType(h) != 1 || !GameFiles.forWwe12(h)) {
                if (h.length >= 4) skipped++;
                continue;
            }
            // One folder per file: the game installs a package under its file name, and two
            // folders of a console copy can hold files of the same name (from Yexclsv's importer).
            File one = new File(importStage(), String.valueOf(found + 1));
            GameFiles.copy(this, u, new File(one, GameFiles.safeName(GameFiles.displayName(this, u))), null, "");
            found++;
        }
        if (found == 0) {
            GameFiles.deleteTree(importStage());
            return "No WWE '12 save found in the picked files.";
        }
        makeBackup(true, "");
        importResult().delete();
        try (OutputStream o = new FileOutputStream(importList())) {
            o.write((importStage().getAbsolutePath() + "\n").getBytes("UTF-8"));
        }
        return "Ready: " + found + " save package(s) are installed the next time you start the game"
                + (skipped > 0 ? " (" + skipped + " other file(s) skipped)." : ".");
    }

    String readResult() {
        int installed = 0, failed = 0;
        try (BufferedReader in = new BufferedReader(new FileReader(importResult()))) {
            for (String l; (l = in.readLine()) != null; ) {
                if (l.startsWith("installed=")) installed = Integer.parseInt(l.substring(10).trim());
                else if (l.startsWith("failed=")) failed = Integer.parseInt(l.substring(7).trim());
            }
        } catch (Exception ignored) {
        }
        if (!importList().exists()) GameFiles.deleteTree(importStage());  // installed: the copies are no longer needed
        return installed + " installed, " + failed + " failed.";
    }
}
