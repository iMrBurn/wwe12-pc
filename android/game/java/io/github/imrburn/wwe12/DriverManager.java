// WWE '12 PC - Android (from Yexclsv's work): custom GPU drivers (Mesa Turnip and similar, the same .zip packages used by
// Winlator, Eden, Skyline...). The launcher imports a driver package into the app's INTERNAL
// folder (Android refuses to load libraries from Android/data, which is mounted no-exec) and the
// game process reads the choice from environment variables set by WWE12Activity.prepareFolder.
//
// Package format (what the community builds ship):
//   meta.json  {"name": "...", "libraryName": "vulkan.ad07XX.so", "driverVersion": "...", ...}
//   <libraryName> next to it (a package with no meta.json is accepted if it holds one .so file).
package io.github.imrburn.wwe12;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public class DriverManager {
    public static final String PREF_SELECTED = "gpu_driver";   // folder name, "" = the phone's own driver
    static final long MAX_ENTRY = 128L * 1024 * 1024;           // a driver is a few MB: refuse zip bombs

    public static class Driver {
        public final String id;       // folder name
        public final String name;
        public final String version;
        public final String library;  // file name of the .so inside the folder
        public final File dir;

        Driver(String id, String name, String version, String library, File dir) {
            this.id = id; this.name = name; this.version = version; this.library = library; this.dir = dir;
        }

        @Override public String toString() {
            return version.isEmpty() ? name : name + " (" + version + ")";
        }
    }

    public static File root(Context c) {
        File d = new File(c.getFilesDir(), "gpu_drivers");
        d.mkdirs();
        return d;
    }

    public static List<Driver> list(Context c) {
        List<Driver> out = new ArrayList<>();
        File[] dirs = root(c).listFiles();
        if (dirs == null) return out;
        Arrays.sort(dirs);
        for (File d : dirs) {
            Driver drv = read(d);
            if (drv != null) out.add(drv);
        }
        return out;
    }

    static Driver read(File dir) {
        if (!dir.isDirectory()) return null;
        String name = dir.getName(), version = "", lib = null;
        File meta = new File(dir, "meta.json");
        if (meta.isFile()) {
            try {
                JSONObject j = new JSONObject(new String(readAll(new java.io.FileInputStream(meta)), "UTF-8"));
                name = j.optString("name", name);
                version = j.optString("driverVersion", "");
                lib = j.optString("libraryName", null);
            } catch (Exception ignored) {
            }
        }
        if (lib == null || !new File(dir, lib).isFile()) lib = firstSo(dir);
        return lib == null ? null : new Driver(dir.getName(), name, version, lib, dir);
    }

    static String firstSo(File dir) {
        File[] files = dir.listFiles();
        if (files == null) return null;
        Arrays.sort(files);
        for (File f : files) if (f.isFile() && f.getName().endsWith(".so")) return f.getName();
        return null;
    }

    public static Driver selected(Context c) {
        String id = c.getSharedPreferences("launcher", Context.MODE_PRIVATE).getString(PREF_SELECTED, "");
        if (id == null || id.isEmpty()) return null;
        return read(new File(root(c), id));
    }

    public static void select(Context c, String id) {
        c.getSharedPreferences("launcher", Context.MODE_PRIVATE).edit().putString(PREF_SELECTED, id == null ? "" : id).apply();
    }

    /** Imports a driver .zip picked with the file picker. Returns the new driver's folder name. */
    public static String importZip(Context c, Uri uri) throws IOException {
        File tmp = new File(root(c), ".import-" + System.nanoTime());
        if (!tmp.mkdirs()) throw new IOException("cannot create " + tmp);
        try {
            int files = 0;
            try (InputStream in = c.getContentResolver().openInputStream(uri)) {
                if (in == null) throw new IOException("cannot open the file");
                ZipInputStream zin = new ZipInputStream(in);
                for (ZipEntry e; (e = zin.getNextEntry()) != null; ) {
                    if (e.isDirectory()) continue;
                    // Flat extraction by file name only: no path from the zip is ever used (zip-slip).
                    String leaf = e.getName();
                    leaf = leaf.substring(leaf.lastIndexOf('/') + 1);
                    if (leaf.isEmpty() || leaf.startsWith(".")) continue;
                    if (!leaf.equals("meta.json") && !leaf.endsWith(".so")) continue;
                    try (OutputStream o = new FileOutputStream(new File(tmp, leaf))) {
                        byte[] buf = new byte[65536];
                        long total = 0;
                        for (int n; (n = zin.read(buf)) > 0; ) {
                            total += n;
                            if (total > MAX_ENTRY) throw new IOException("file too big in the package");
                            o.write(buf, 0, n);
                        }
                    }
                    files++;
                }
            }
            if (files == 0) throw new IOException("no driver (.so) found in this zip");
            Driver d = read(tmp);
            if (d == null) throw new IOException("this zip has no valid driver library");
            String id = sanitize(d.name);
            File dest = new File(root(c), id);
            for (int i = 2; dest.exists(); i++) dest = new File(root(c), id + "-" + i);
            if (!tmp.renameTo(dest)) throw new IOException("cannot install the driver");
            return dest.getName();
        } catch (IOException | RuntimeException e) {
            deleteTree(tmp);
            throw e;
        }
    }

    public static void remove(Context c, String id) {
        if (id == null || id.isEmpty() || id.contains("/") || id.contains("..")) return;
        deleteTree(new File(root(c), id));
        if (id.equals(c.getSharedPreferences("launcher", Context.MODE_PRIVATE).getString(PREF_SELECTED, ""))) select(c, "");
    }

    /** Variables read by the native side (port/src/android/wwe12_driver.cpp). Empty = system driver. */
    public static void exportEnvironment(Context c) {
        try {
            Driver d = selected(c);
            if (d == null) {
                android.system.Os.unsetenv("WWE12_DRIVER_DIR");
                return;
            }
            File tmp = new File(c.getCodeCacheDir(), "driver_tmp");
            tmp.mkdirs();
            android.system.Os.setenv("WWE12_DRIVER_DIR", d.dir.getAbsolutePath() + "/", true);
            android.system.Os.setenv("WWE12_DRIVER_LIB", d.library, true);
            android.system.Os.setenv("WWE12_DRIVER_NAME", d.toString(), true);
            android.system.Os.setenv("WWE12_DRIVER_TMP", tmp.getAbsolutePath() + "/", true);
        } catch (Exception e) {
            android.util.Log.e("WWE12", "driver environment failed", e);
        }
    }

    static String sanitize(String s) {
        String r = s.replaceAll("[^A-Za-z0-9._-]+", "_").replaceAll("^[._]+", "");
        return r.isEmpty() ? "driver" : (r.length() > 60 ? r.substring(0, 60) : r);
    }

    static void deleteTree(File f) {
        File[] kids = f.listFiles();
        if (kids != null) for (File k : kids) deleteTree(k);
        f.delete();
    }

    static byte[] readAll(InputStream in) throws IOException {
        try (InputStream i = in) {
            ByteArrayOutputStream b = new ByteArrayOutputStream();
            byte[] buf = new byte[8192];
            for (int n; (n = i.read(buf)) > 0; ) b.write(buf, 0, n);
            return b.toByteArray();
        }
    }
}
