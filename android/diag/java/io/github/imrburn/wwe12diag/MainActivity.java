// WWE '12 PC - Android device check. Shows one text report (device facts from Java + Vulkan,
// memory and CPU checks from libwwe12diag.so) that testers copy or share on Discord / GitHub.
package io.github.imrburn.wwe12diag;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Typeface;
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.os.StatFs;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;

public class MainActivity extends Activity {
    static final String VERSION = "1.0";

    static native String runNative(boolean bench);

    TextView text;
    String report = "";

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = (int) (12 * getResources().getDisplayMetrics().density);
        root.setPadding(pad, pad, pad, pad);
        root.setFitsSystemWindows(true);  // Android 15+ draws apps under the status bar

        LinearLayout buttons = new LinearLayout(this);
        buttons.setOrientation(LinearLayout.HORIZONTAL);
        Button copy = new Button(this);
        copy.setText("Copy");
        copy.setOnClickListener(new View.OnClickListener() { public void onClick(View v) { copyReport(); } });
        Button share = new Button(this);
        share.setText("Share");
        share.setOnClickListener(new View.OnClickListener() { public void onClick(View v) { shareReport(); } });
        Button again = new Button(this);
        again.setText("Run again");
        again.setOnClickListener(new View.OnClickListener() { public void onClick(View v) { run(); } });
        buttons.addView(copy);
        buttons.addView(share);
        buttons.addView(again);
        root.addView(buttons);

        ScrollView scroll = new ScrollView(this);
        text = new TextView(this);
        text.setTypeface(Typeface.MONOSPACE);
        text.setTextSize(12);
        text.setTextIsSelectable(true);
        scroll.addView(text);
        root.addView(scroll, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));
        setContentView(root);
        run();
    }

    void run() {
        text.setText("Checking your device (about 5 seconds)...");
        new Thread(new Runnable() { public void run() {
            String r;
            try {
                System.loadLibrary("wwe12diag");
                r = javaFacts() + "\n" + runNative(true);
            } catch (Throwable t) {
                r = javaFacts() + "\nNative check failed: " + t;
            }
            final String fr = "WWE '12 PC - Android device check " + VERSION + "\n\n" + r;
            runOnUiThread(new Runnable() { public void run() {
                report = fr;
                text.setText(fr);
                saveReport(fr);
            }});
        }}).start();
    }

    String javaFacts() {
        StringBuilder s = new StringBuilder();
        s.append("== Device\n");
        s.append("model: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL)
                .append(" (").append(Build.DEVICE).append(")\n");
        if (Build.VERSION.SDK_INT >= 31) {
            s.append("chip: ").append(Build.SOC_MANUFACTURER).append(' ').append(Build.SOC_MODEL)
                    .append('\n');
        }
        s.append("board/hardware: ").append(Build.BOARD).append(" / ").append(Build.HARDWARE).append('\n');
        s.append("android: ").append(Build.VERSION.RELEASE).append(" (API ")
                .append(Build.VERSION.SDK_INT).append(")\n");
        s.append("ABIs: ").append(String.join(", ", Build.SUPPORTED_ABIS)).append('\n');

        ActivityManager am = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
        ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
        am.getMemoryInfo(mi);
        long ramMb = mi.totalMem >> 20;
        s.append("RAM: ").append(ramMb).append(" MB total, ").append(mi.availMem >> 20)
                .append(" MB free\n");

        File dir = getExternalFilesDir(null);
        if (dir != null) {
            StatFs fs = new StatFs(dir.getPath());
            s.append("free storage: ").append(fs.getAvailableBytes() >> 30).append(" GB\n");
        }

        DisplayMetrics dm = new DisplayMetrics();
        Display d = getWindowManager().getDefaultDisplay();
        d.getRealMetrics(dm);
        s.append("screen: ").append(Math.max(dm.widthPixels, dm.heightPixels)).append('x')
                .append(Math.min(dm.widthPixels, dm.heightPixels)).append(" @ ")
                .append(Math.round(d.getRefreshRate())).append(" Hz\n");

        if (Build.VERSION.SDK_INT >= 29) {
            PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
            s.append("thermal status: ").append(pm.getCurrentThermalStatus()).append(" (0 = cool)\n");
        }

        boolean arm64 = false;
        for (String abi : Build.SUPPORTED_ABIS) arm64 |= abi.equals("arm64-v8a");
        if (!arm64) s.append("[X] not a 64-bit ARM device: the port needs arm64-v8a\n");
        if (ramMb < 5500) s.append("[!] less than 6 GB of RAM: the game may be closed by Android\n");
        return s.toString();
    }

    void saveReport(String r) {
        try {
            File dir = getExternalFilesDir(null);
            if (dir == null) return;
            try (FileOutputStream o = new FileOutputStream(new File(dir, "wwe12_device_check.txt"))) {
                o.write(r.getBytes("UTF-8"));
            }
        } catch (Exception ignored) {
        }
    }

    void copyReport() {
        ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        cm.setPrimaryClip(ClipData.newPlainText("WWE12 device check", report));
        Toast.makeText(this, "Report copied", Toast.LENGTH_SHORT).show();
    }

    void shareReport() {
        Intent i = new Intent(Intent.ACTION_SEND);
        i.setType("text/plain");
        i.putExtra(Intent.EXTRA_SUBJECT, "WWE12 device check");
        i.putExtra(Intent.EXTRA_TEXT, report);
        startActivity(Intent.createChooser(i, "Share report"));
    }
}
