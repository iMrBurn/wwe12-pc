// WWE '12 PC - Android: loading screen over the game while it starts. SDL shows a black surface for a few
// seconds (native libraries, shaders, first frames): this shows the same yellow-and-black loading bar as the
// launcher until the game has drawn its first frame, then fades out.
// "First frame" = the game log says so: the first presented frame writes "Android performance hint: ..."
// (wwe12_perf_hint.cpp). If that line never shows (a log that is not flushed, a crash), the screen goes away
// by itself after 90 seconds, and a tap never blocks anything: the touch buttons are not under it for long.
package io.github.imrburn.wwe12;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.ViewGroup;

import java.io.File;
import java.io.RandomAccessFile;

final class GameLoading {
    static final String MARKER = "Android performance hint";
    static final long GIVE_UP_MS = 90_000;
    static final String[] TIPS = {
        "The first matches can stutter while the game builds its graphics cache.",
        "Phone getting warm? Lower the resolution in the launcher (SETTINGS).",
        "Your save is kept on the phone: MANAGE MY SAVES in the launcher can back it up.",
        "A Bluetooth or USB controller works too: pair it in Android's settings."
    };

    private GameLoading() {}

    static void start(final Activity activity) {
        final Ui.Loading view = new Ui.Loading(activity, "LOADING", TIPS);
        activity.addContentView(view, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        view.startTips();
        final long began = SystemClock.elapsedRealtime();
        final long wallStart = System.currentTimeMillis() - 3000;  // log files older than this belong to a previous run
        final File logs = new File(String.valueOf(activity.getExternalFilesDir(null)), "logs");
        final Handler h = new Handler(Looper.getMainLooper());
        h.postDelayed(new Runnable() {
            public void run() {
                if (view.getParent() == null) return;
                if (firstFrameLogged(logs, wallStart) || SystemClock.elapsedRealtime() - began > GIVE_UP_MS) {
                    view.dismiss();
                    return;
                }
                h.postDelayed(this, 500);
            }
        }, 2000);
    }

    // True when the newest log of this run already has the first-frame line (its last 128 KB are searched).
    static boolean firstFrameLogged(File logs, long since) {
        File[] files = logs.listFiles();
        if (files == null) return false;
        File last = null;
        for (File f : files)
            if (f.getName().endsWith(".log") && f.lastModified() >= since && (last == null || f.lastModified() > last.lastModified()))
                last = f;
        if (last == null) return false;
        try (RandomAccessFile r = new RandomAccessFile(last, "r")) {
            long size = r.length(), from = Math.max(0, size - 128 * 1024);
            byte[] buf = new byte[(int) (size - from)];
            r.seek(from);
            r.readFully(buf);
            return new String(buf, "UTF-8").contains(MARKER);
        } catch (Exception e) {
            return false;
        }
    }
}
