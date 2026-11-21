// Gives other apps (Discord, mail, Drive...) read access to the game's log files, so "Share last log" can
// attach the .log file itself instead of pasting a huge text (which many phones refuse). A minimal
// stand-in for androidx FileProvider (the APK is built with the plain Android SDK, no AndroidX).
// Only files directly inside <app files>/logs/ with a .log name can be opened, read-only.
package io.github.imrburn.wwe12;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;

public class LogProvider extends ContentProvider {
    static final String AUTHORITY = "io.github.imrburn.wwe12.logs";

    private File resolve(Uri uri) throws FileNotFoundException {
        String name = uri.getLastPathSegment();
        if (name == null || !name.endsWith(".log") || name.contains("/") || name.contains("\\") || name.contains(".."))
            throw new FileNotFoundException("not a log: " + uri);
        File dir = getContext().getExternalFilesDir(null);
        if (dir == null) throw new FileNotFoundException("no files folder");
        File f = new File(new File(dir, "logs"), name);
        if (!f.isFile()) throw new FileNotFoundException(name);
        return f;
    }

    @Override public boolean onCreate() { return true; }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (!"r".equals(mode)) throw new FileNotFoundException("read only");
        return ParcelFileDescriptor.open(resolve(uri), ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] args, String sort) {
        try {
            File f = resolve(uri);
            MatrixCursor c = new MatrixCursor(new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE});
            c.addRow(new Object[] {f.getName(), f.length()});
            return c;
        } catch (FileNotFoundException e) {
            return null;
        }
    }

    @Override public String getType(Uri uri) { return "text/plain"; }
    @Override public Uri insert(Uri uri, ContentValues values) { throw new UnsupportedOperationException(); }
    @Override public int delete(Uri uri, String selection, String[] args) { throw new UnsupportedOperationException(); }
    @Override public int update(Uri uri, ContentValues values, String selection, String[] args) {
        throw new UnsupportedOperationException();
    }
}
