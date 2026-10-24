// WWE '12 PC - Android. SDL3 runs the game: this activity only says where things are.
// The app's files folder (Android/data/io.github.imrburn.wwe12/files, reachable over USB from a
// PC) plays the role of the PC game folder: game/ (the player's ISO or extracted files),
// wwe12.toml, logs/, mods/, dlc/, and wwe12/ for saves (like Documents\wwe12 on PC).
// Started by LauncherActivity, which gives the game to run in EXTRA_GAME (a folder or file path)
// or EXTRA_GAME_URI (an ISO picked with Android's file picker). The game has its own process
// (":game" in the manifest), ended when the game closes: the runtime cannot start twice in one.
package io.github.imrburn.wwe12;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.system.Os;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

public class WWE12Activity extends SDLActivity {
    static final String TAG = "WWE12";
    static final String EXTRA_GAME = "io.github.imrburn.wwe12.GAME";
    static final String EXTRA_GAME_URI = "io.github.imrburn.wwe12.GAME_URI";

    // The ISO picked in the launcher stays open for the whole game process: the game reads it
    // through /proc/self/fd/<n> (Android gives apps no plain path to other folders' files).
    static ParcelFileDescriptor pickedIso;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        prepareFolder(this, getExternalFilesDir(null));
        super.onCreate(savedInstanceState);
    }

    // Folder layout, default settings and the variables the runtime reads
    // (rexglue-sdk-android.patch: app folder, saves, plugins).
    static void prepareFolder(Context context, File dir) {
        if (dir == null) return;
        new File(dir, "game").mkdirs();
        copyAssetIfMissing(context, "wwe12.toml", new File(dir, "wwe12.toml"));
        try {
            Os.setenv("REX_APP_DIR", dir.getAbsolutePath(), true);
            Os.setenv("XDG_DATA_HOME", dir.getAbsolutePath(), true);
            Os.setenv("REX_NATIVE_LIB_DIR", context.getApplicationInfo().nativeLibraryDir, true);
            // SDL hint read from the environment: landscape only, whatever the phone's rotation lock.
            Os.setenv("SDL_ORIENTATIONS", "LandscapeLeft LandscapeRight", true);
        } catch (Exception e) {
            Log.e(TAG, "setenv failed", e);
        }
    }

    static void copyAssetIfMissing(Context context, String name, File out) {
        if (out.exists()) return;
        try (InputStream in = context.getAssets().open(name); OutputStream o = new FileOutputStream(out)) {
            byte[] buf = new byte[16384];
            for (int n; (n = in.read(buf)) > 0; ) o.write(buf, 0, n);
        } catch (Exception e) {
            Log.e(TAG, "cannot copy " + name, e);
        }
    }

    // Game copied by hand: game/default.xex (extracted disc) or the first .iso in game/.
    static String findLocalGame(File dir) {
        if (dir == null) return null;
        File game = new File(dir, "game");
        if (new File(game, "default.xex").isFile()) return game.getAbsolutePath();
        File[] files = game.listFiles();
        if (files != null)
            for (File f : files)
                if (f.isFile() && f.getName().toLowerCase().endsWith(".iso")) return f.getAbsolutePath();
        return null;
    }

    // Opens the ISO picked with Android's file picker and returns a path the game can open.
    static String openPickedIso(Context context, Uri uri) {
        try {
            if (pickedIso != null) pickedIso.close();
            pickedIso = context.getContentResolver().openFileDescriptor(uri, "r");
            return pickedIso != null ? "/proc/self/fd/" + pickedIso.getFd() : null;
        } catch (Exception e) {
            Log.e(TAG, "cannot open " + uri, e);
            pickedIso = null;
            return null;
        }
    }

    // SDL is inside librexruntime.so; the game (with main()) is libwwe12.so, loaded last.
    @Override
    protected void onDestroy() {
        super.onDestroy();
        // Leave nothing of the runtime behind for the next PLAY (see the process note above).
        if (isFinishing()) System.exit(0);
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {"c++_shared", "rexruntime", "wwe12"};
    }

    @Override
    protected String getMainFunction() {
        return "main";
    }

    @Override
    protected String[] getArguments() {
        String root = null;
        if (getIntent() != null) {
            String uri = getIntent().getStringExtra(EXTRA_GAME_URI);
            root = uri != null ? openPickedIso(this, Uri.parse(uri)) : getIntent().getStringExtra(EXTRA_GAME);
        }
        if (root == null) root = findLocalGame(getExternalFilesDir(null));
        Log.i(TAG, "game data: " + (root != null ? root : "NOT FOUND"));
        return root != null ? new String[] {"--game_data_root=" + root} : new String[0];
    }
}
