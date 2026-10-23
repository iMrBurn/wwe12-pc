// WWE '12 PC - Android. SDL3 runs the game: this activity only says where things are.
// The app's files folder (Android/data/io.github.imrburn.wwe12/files, reachable over USB from a
// PC) plays the role of the PC game folder: game/ (the player's ISO or extracted files),
// wwe12.toml, logs/, mods/, dlc/, and wwe12/ for saves (like Documents\wwe12 on PC).
package io.github.imrburn.wwe12;

import android.os.Bundle;
import android.system.Os;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

public class WWE12Activity extends SDLActivity {
    static final String TAG = "WWE12";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        File dir = getExternalFilesDir(null);
        if (dir != null) {
            new File(dir, "game").mkdirs();
            copyAssetIfMissing("wwe12.toml", new File(dir, "wwe12.toml"));
            try {
                // Read by the runtime (rexglue-sdk-android.patch): app folder, saves, plugins.
                Os.setenv("REX_APP_DIR", dir.getAbsolutePath(), true);
                Os.setenv("XDG_DATA_HOME", dir.getAbsolutePath(), true);
                Os.setenv("REX_NATIVE_LIB_DIR", getApplicationInfo().nativeLibraryDir, true);
            } catch (Exception e) {
                Log.e(TAG, "setenv failed", e);
            }
            Log.i(TAG, "game folder: " + dir.getAbsolutePath());
        }
        super.onCreate(savedInstanceState);
    }

    void copyAssetIfMissing(String name, File out) {
        if (out.exists()) return;
        try (InputStream in = getAssets().open(name); OutputStream o = new FileOutputStream(out)) {
            byte[] buf = new byte[16384];
            for (int n; (n = in.read(buf)) > 0; ) o.write(buf, 0, n);
        } catch (Exception e) {
            Log.e(TAG, "cannot copy " + name, e);
        }
    }

    // SDL is inside librexruntime.so; the game (with main()) is libwwe12.so, loaded last.
    @Override
    protected String[] getLibraries() {
        return new String[] {"c++_shared", "rexruntime", "wwe12"};
    }

    @Override
    protected String getMainFunction() {
        return "main";
    }

    // Game files: game/default.xex (extracted disc) or the first .iso in game/.
    @Override
    protected String[] getArguments() {
        File dir = getExternalFilesDir(null);
        if (dir == null) return new String[0];
        File game = new File(dir, "game");
        String root = null;
        if (new File(game, "default.xex").isFile()) {
            root = game.getAbsolutePath();
        } else {
            File[] files = game.listFiles();
            if (files != null) {
                for (File f : files) {
                    if (f.isFile() && f.getName().toLowerCase().endsWith(".iso")) {
                        root = f.getAbsolutePath();
                        break;
                    }
                }
            }
        }
        Log.i(TAG, "game data: " + (root != null ? root : "NOT FOUND in " + game.getAbsolutePath()));
        return root != null ? new String[] {"--game_data_root=" + root} : new String[0];
    }
}
