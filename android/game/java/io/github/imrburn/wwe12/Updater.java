// WWE '12 PC - Android: launcher updates from the project's GitHub releases (like the PC launcher,
// port/src/launcher/updater.h). An Android release is tagged android-<word><number> (android-alpha3,
// android-beta-1...): that number is the APK's versionCode, so a higher one is an update. The APK is
// streamed into Android's PackageInstaller; Android then asks the player to confirm (it never
// installs silently) and only accepts an APK signed with the same key, so saves are kept.
package io.github.imrburn.wwe12;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageInstaller;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Updater {
    static final String RELEASES = "https://api.github.com/repos/iMrBurn/wwe12-pc/releases?per_page=15";
    // Only files of this project's own releases are ever downloaded.
    static final String DOWNLOADS = "https://github.com/iMrBurn/wwe12-pc/releases/download/";
    static final Pattern TAG = Pattern.compile("^android-([a-z]+)-?(\\d+)$");
    static final String ACTION_STATUS = "io.github.imrburn.wwe12.INSTALL_STATUS";

    public static class Release {
        public final String tag, name, url;
        public final long size;
        public final int number;

        Release(String tag, String name, String url, long size, int number) {
            this.tag = tag; this.name = name; this.url = url; this.size = size; this.number = number;
        }
    }

    public interface Found { void found(Release r); }
    public interface Progress { void progress(String text, boolean done, boolean failed); }

    static final Handler ui = new Handler(Looper.getMainLooper());

    static int versionCode(Activity a) {
        try {
            PackageInfo pi = a.getPackageManager().getPackageInfo(a.getPackageName(), 0);
            return (int) (Build.VERSION.SDK_INT >= 28 ? pi.getLongVersionCode() : pi.versionCode);
        } catch (Exception e) {
            return Integer.MAX_VALUE;  // unknown: never offer an update
        }
    }

    /** Looks for a newer Android release in the background; calls back on the UI thread if one exists. */
    public static void check(final Activity a, final Found found) {
        final int current = versionCode(a);
        new Thread(new Runnable() {
            public void run() {
                try {
                    JSONArray releases = new JSONArray(new String(get(RELEASES), "UTF-8"));
                    Release best = null;
                    for (int i = 0; i < releases.length(); i++) {
                        JSONObject r = releases.getJSONObject(i);
                        if (r.optBoolean("draft")) continue;
                        Matcher m = TAG.matcher(r.optString("tag_name"));
                        if (!m.matches() || m.group(1).equals("check")) continue;
                        int number = Integer.parseInt(m.group(2));
                        if (number <= current || (best != null && number <= best.number)) continue;
                        JSONArray assets = r.optJSONArray("assets");
                        if (assets == null) continue;
                        for (int j = 0; j < assets.length(); j++) {
                            JSONObject f = assets.getJSONObject(j);
                            String url = f.optString("browser_download_url");
                            if (f.optString("name").endsWith(".apk") && url.startsWith(DOWNLOADS)) {
                                best = new Release(r.optString("tag_name"), r.optString("name"), url,
                                        f.optLong("size"), number);
                                break;
                            }
                        }
                    }
                    final Release result = best;
                    if (result != null)
                        ui.post(new Runnable() { public void run() { found.found(result); } });
                } catch (Exception ignored) {
                    // offline, rate-limited by GitHub...: no update shown, try again next time
                }
            }
        }).start();
    }

    /** Downloads the APK into an install session and asks Android to install it. */
    public static void install(final Activity a, final Release r, final Progress progress) {
        new Thread(new Runnable() {
            public void run() {
                PackageInstaller.Session session = null;
                try {
                    PackageInstaller installer = a.getPackageManager().getPackageInstaller();
                    PackageInstaller.SessionParams params =
                            new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
                    params.setAppPackageName(a.getPackageName());
                    if (r.size > 0) params.setSize(r.size);
                    int id = installer.createSession(params);
                    session = installer.openSession(id);
                    HttpURLConnection c = open(r.url);
                    long total = r.size > 0 ? r.size : c.getContentLengthLong(), done = 0;
                    int lastPercent = -1;
                    try (InputStream in = c.getInputStream();
                         OutputStream out = session.openWrite("base.apk", 0, total > 0 ? total : -1)) {
                        byte[] buf = new byte[1 << 16];
                        for (int n; (n = in.read(buf)) > 0; ) {
                            out.write(buf, 0, n);
                            done += n;
                            int percent = total > 0 ? (int) (done * 100 / total) : -1;
                            if (percent != lastPercent) {
                                lastPercent = percent;
                                post(progress, "Downloading " + r.tag + "... " + (percent >= 0 ? percent + " %" : done / 1048576 + " MB"), false, false);
                            }
                        }
                        session.fsync(out);
                    }
                    Intent status = new Intent(a, LauncherActivity.class).setAction(ACTION_STATUS);
                    int flags = PendingIntent.FLAG_UPDATE_CURRENT | (Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0);
                    PendingIntent pi = PendingIntent.getActivity(a, id, status, flags);
                    post(progress, "Download complete: confirm the installation.", true, false);
                    session.commit(pi.getIntentSender());
                    session.close();
                } catch (Exception e) {
                    if (session != null) session.abandon();
                    post(progress, "Update failed: " + e.getMessage(), true, true);
                }
            }
        }).start();
    }

    /** LauncherActivity.onNewIntent: Android's answer to the install request. */
    public static String handleStatus(Activity a, Intent intent) {
        int status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS, PackageInstaller.STATUS_FAILURE);
        if (status == PackageInstaller.STATUS_PENDING_USER_ACTION) {
            Intent confirm = intent.getParcelableExtra(Intent.EXTRA_INTENT);
            if (confirm != null) a.startActivity(confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
            return null;
        }
        if (status == PackageInstaller.STATUS_SUCCESS) return null;  // the app restarts updated
        String message = intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE);
        return "Update not installed" + (message != null ? ": " + message : ".");
    }

    static void post(final Progress p, final String text, final boolean done, final boolean failed) {
        ui.post(new Runnable() { public void run() { p.progress(text, done, failed); } });
    }

    static HttpURLConnection open(String url) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setRequestProperty("User-Agent", "WWE12-PC-Android");
        c.setRequestProperty("Accept", "application/vnd.github+json");
        c.setConnectTimeout(15000);
        c.setReadTimeout(30000);
        c.setInstanceFollowRedirects(true);  // release files redirect to GitHub's storage host
        if (c.getResponseCode() != 200) throw new Exception("HTTP " + c.getResponseCode());
        return c;
    }

    static byte[] get(String url) throws Exception {
        HttpURLConnection c = open(url);
        try (InputStream in = c.getInputStream()) {
            ByteArrayOutputStream b = new ByteArrayOutputStream();
            byte[] buf = new byte[16384];
            for (int n; (n = in.read(buf)) > 0; ) b.write(buf, 0, n);
            return b.toByteArray();
        }
    }
}
