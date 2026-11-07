// WWE '12 PC - Android: tells the launcher how the last game run went, so it can offer a safe mode after two
// starts in a row that never reached the first frame (idea seen in Jarrod's Android builds: "safe mode offered
// after repeated failures"). A small file in the app's files folder, shared by the game and launcher processes:
//   started  the game activity was created
//   ok       the first frame was drawn
//   clean    the game was closed normally
// Read (and deleted) by the launcher the next time it shows. "started" alone = the run died before any picture.
package io.github.imrburn.wwe12;

import android.content.Context;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FileInputStream;

final class RunState {
    private RunState() {}

    static File file(Context c) {
        File dir = c.getExternalFilesDir(null);
        return dir == null ? null : new File(dir, "run_state.txt");
    }

    static void mark(Context c, String state) {
        File f = file(c);
        if (f == null) return;
        try (FileOutputStream o = new FileOutputStream(f)) {
            o.write(state.getBytes("UTF-8"));
        } catch (Exception ignored) {
        }
    }

    /** The last state written ("" if none), then forgets it. */
    static String take(Context c) {
        File f = file(c);
        if (f == null || !f.isFile()) return "";
        String s = "";
        try (FileInputStream in = new FileInputStream(f)) {
            byte[] b = new byte[32];
            int n = in.read(b);
            if (n > 0) s = new String(b, 0, n, "UTF-8").trim();
        } catch (Exception ignored) {
        }
        f.delete();
        return s;
    }
}
