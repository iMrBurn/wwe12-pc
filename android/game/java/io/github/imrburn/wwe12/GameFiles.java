// Files the launcher manages for the game, in the app folder (Android/data/.../files), laid out
// like the PC version: dlc/ (DLC packages, installed by the game at its next start, see
// port/src/wwe12_dlc.cpp) and wwe12/ (Documents\wwe12 on PC: saves, installed DLC, caches).
// Android's file picker hands content:// addresses, not paths: picked files are copied in.
package io.github.imrburn.wwe12;

import android.app.ActivityManager;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class GameFiles {
    static final String TITLE = "54510890";            // WWE '12
    static final String PROFILE = "B13EBABEBABEBABE";  // the runtime's fixed local profile

    interface Progress { void progress(String text); }

    static File appDir(Context c) { return c.getExternalFilesDir(null); }
    static File userData(Context c) { return new File(appDir(c), "wwe12"); }
    static File dlcDir(Context c) { return new File(appDir(c), "dlc"); }

    // The game runs in its own process (":game" in the manifest): its files must not change under it.
    static boolean gameRunning(Context c) {
        ActivityManager am = (ActivityManager) c.getSystemService(Context.ACTIVITY_SERVICE);
        List<ActivityManager.RunningAppProcessInfo> list = am != null ? am.getRunningAppProcesses() : null;
        if (list != null)
            for (ActivityManager.RunningAppProcessInfo p : list)
                if (p.processName.endsWith(":game")) return true;
        return false;
    }

    static String displayName(Context c, Uri uri) {
        try (Cursor cur = c.getContentResolver().query(uri, new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cur != null && cur.moveToFirst() && cur.getString(0) != null) return cur.getString(0);
        } catch (Exception ignored) {
        }
        String last = uri.getLastPathSegment();
        return last != null ? last.substring(last.lastIndexOf('/') + 1) : "file";
    }

    // A file name kept as is when harmless (a DLC's name is what the game installs it under).
    static String safeName(String name) {
        String s = name.replaceAll("[\\\\/:*?\"<>|\\x00-\\x1f]", "_").trim();
        return s.isEmpty() || s.equals(".") || s.equals("..") ? "file" : s;
    }

    // Every file under a folder picked with ACTION_OPEN_DOCUMENT_TREE (from Yexclsv's SaveImporter):
    // a console's Content folder can be picked whole. Stops after `max` entries (a whole storage
    // picked by mistake).
    static List<Uri> treeFiles(Context c, Uri tree, int max) {
        List<Uri> out = new ArrayList<>();
        walk(c, tree, android.provider.DocumentsContract.getTreeDocumentId(tree), out, new int[] {0}, max);
        return out;
    }

    private static void walk(Context c, Uri tree, String docId, List<Uri> out, int[] looked, int max) {
        Uri children = android.provider.DocumentsContract.buildChildDocumentsUriUsingTree(tree, docId);
        try (Cursor cur = c.getContentResolver().query(children, new String[] {
                android.provider.DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                android.provider.DocumentsContract.Document.COLUMN_MIME_TYPE}, null, null, null)) {
            if (cur == null) return;
            while (cur.moveToNext()) {
                if (++looked[0] > max) return;
                String id = cur.getString(0);
                if (android.provider.DocumentsContract.Document.MIME_TYPE_DIR.equals(cur.getString(1)))
                    walk(c, tree, id, out, looked, max);
                else
                    out.add(android.provider.DocumentsContract.buildDocumentUriUsingTree(tree, id));
            }
        } catch (Exception ignored) {
        }
    }

    static byte[] head(Context c, Uri uri, int n) throws IOException {
        byte[] b = new byte[n];
        try (InputStream in = c.getContentResolver().openInputStream(uri)) {
            if (in == null) throw new IOException("cannot open");
            int got = 0;
            for (int r; got < n && (r = in.read(b, got, n - got)) > 0; ) got += r;
            return got == n ? b : Arrays.copyOf(b, got);
        }
    }

    static boolean isStfs(byte[] h) {
        if (h.length < 4) return false;
        String m = new String(h, 0, 4, java.nio.charset.StandardCharsets.ISO_8859_1);
        return m.equals("CON ") || m.equals("LIVE") || m.equals("PIRS");
    }

    static long be32(byte[] h, int at) {
        return ((h[at] & 0xFFL) << 24) | ((h[at + 1] & 0xFFL) << 16) | ((h[at + 2] & 0xFFL) << 8) | (h[at + 3] & 0xFFL);
    }

    // STFS content type at 0x344, title id at 0x360 (big-endian), as saves.h on PC.
    static long contentType(byte[] h) { return h.length >= 0x348 ? be32(h, 0x344) : -1; }
    static boolean forWwe12(byte[] h) { return h.length >= 0x364 && be32(h, 0x360) == 0x54510890L; }

    // Copies through a temporary name, so a cut copy never looks like a whole package.
    static void copy(Context c, Uri from, File to, Progress progress, String what) throws IOException {
        File tmp = new File(to.getParentFile(), to.getName() + ".part");
        to.getParentFile().mkdirs();
        long done = 0, shown = 0;
        try (InputStream in = c.getContentResolver().openInputStream(from); OutputStream out = new FileOutputStream(tmp)) {
            if (in == null) throw new IOException("cannot open");
            byte[] buf = new byte[1 << 20];
            for (int n; (n = in.read(buf)) > 0; ) {
                out.write(buf, 0, n);
                done += n;
                if (progress != null && done - shown >= (8 << 20)) {
                    shown = done;
                    progress.progress(what + ": " + (done >> 20) + " MB");
                }
            }
        } catch (IOException e) {
            tmp.delete();
            throw e;
        }
        if (to.exists()) to.delete();
        if (!tmp.renameTo(to)) {
            tmp.delete();
            throw new IOException("cannot write " + to.getName());
        }
    }

    static boolean deleteTree(File f) {
        File[] kids = f.listFiles();
        if (kids != null) for (File k : kids) deleteTree(k);
        return f.delete() || !f.exists();
    }

    static long size(File f) {
        if (f.isFile()) return f.length();
        long total = 0;
        File[] kids = f.listFiles();
        if (kids != null) for (File k : kids) total += size(k);
        return total;
    }

    static long newest(File f) {
        if (f.isFile()) return f.lastModified();
        long best = 0;
        File[] kids = f.listFiles();
        if (kids != null) for (File k : kids) best = Math.max(best, newest(k));
        return best;
    }

    static String formatSize(long bytes) {
        return bytes >= (1 << 20) ? String.format(java.util.Locale.US, "%.1f MB", bytes / 1048576.0)
                : String.format(java.util.Locale.US, "%.0f KB", bytes / 1024.0);
    }

    // ---- DLC ----------------------------------------------------------------------------------

    static class Dlc {
        File file;
        boolean installed;   // already installed by the game (it installs a package only once)
    }

    // Name the game installs a package under (XCONTENT_AGGREGATE_DATA keeps 42 characters).
    static String installName(File f) {
        String n = f.getName();
        return n.length() > 42 ? n.substring(0, 42) : n;
    }

    static File installedDir(Context c, File f) {
        return new File(userData(c), "0000000000000000/" + TITLE + "/00000002/" + installName(f));
    }

    static List<Dlc> listDlc(Context c) {
        List<Dlc> out = new ArrayList<>();
        collect(c, dlcDir(c), out);
        return out;
    }

    private static void collect(Context c, File dir, List<Dlc> out) {
        File[] kids = dir.listFiles();
        if (kids == null) return;
        Arrays.sort(kids);
        for (File k : kids) {
            if (k.isDirectory()) { collect(c, k, out); continue; }
            if (k.getName().endsWith(".part") || k.length() < 0x364) continue;
            try (java.io.FileInputStream in = new java.io.FileInputStream(k)) {
                byte[] h = new byte[4];
                if (in.read(h) != 4 || !isStfs(h)) continue;
            } catch (IOException e) {
                continue;
            }
            Dlc d = new Dlc();
            d.file = k;
            d.installed = installedDir(c, k).isDirectory();
            out.add(d);
        }
    }

    // Copies the picked packages into dlc/. Returns a short report.
    static String addDlc(Context c, List<Uri> uris, Progress progress) {
        int added = 0, refused = 0, failed = 0;
        for (Uri u : uris) {
            String name = safeName(displayName(c, u));
            try {
                byte[] h = head(c, u, 0x364);
                // Content type 2 = marketplace content (DLC); a save (type 1) is refused here.
                if (!isStfs(h) || !forWwe12(h) || contentType(h) == 1) {
                    if (h.length >= 4) refused++;  // empty / tiny files of a picked folder: not worth a mention
                    continue;
                }
                copy(c, u, new File(dlcDir(c), name), progress, name);
                added++;
            } catch (IOException e) {
                failed++;
            }
        }
        String r = added + " DLC added";
        if (refused > 0) r += ", " + refused + " file(s) skipped (not a WWE '12 DLC package)";
        if (failed > 0) r += ", " + failed + " failed (storage full?)";
        return r + (added > 0 ? ". They are installed the next time you start the game." : ".");
    }

    // Removes the package and what the game installed from it.
    static void removeDlc(Context c, Dlc d) {
        String name = installName(d.file);
        deleteTree(installedDir(c, d.file));
        new File(userData(c), "0000000000000000/" + TITLE + "/Headers/00000002/" + name + ".header").delete();
        d.file.delete();
    }
}
