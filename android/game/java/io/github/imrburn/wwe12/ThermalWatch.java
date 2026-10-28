// WWE '12 PC - Android: tells the player when the phone is getting hot (Android 10+).
// At "severe" Android starts slowing the apps down on its own, which shows as a game that suddenly stutters.
// A short message at that moment (not more than once every two minutes) says why and what helps.
// Started by WWE12Activity (launcher SETTINGS: "Warn me when the phone gets too hot"). By Yexclsv.
package io.github.imrburn.wwe12;

import android.app.Activity;
import android.os.PowerManager;
import android.widget.Toast;

class ThermalWatch implements PowerManager.OnThermalStatusChangedListener {
    final Activity activity;
    final PowerManager power;
    long lastWarning;

    ThermalWatch(Activity activity) {
        this.activity = activity;
        power = (PowerManager) activity.getSystemService(Activity.POWER_SERVICE);
        if (power != null) power.addThermalStatusListener(activity.getMainExecutor(), this);
    }

    void stop() {
        if (power != null) power.removeThermalStatusListener(this);
    }

    @Override
    public void onThermalStatusChanged(int status) {
        if (status < PowerManager.THERMAL_STATUS_SEVERE) return;
        long now = System.currentTimeMillis();
        if (now - lastWarning < 120000) return;
        lastWarning = now;
        int t = TempMonitor.lastTenths;
        String temp = t > -1000 ? " (" + Math.round(t / 10f) + "C)" : "";
        // Short: Android 12+ cuts a toast after two lines.
        Toast.makeText(activity, status >= PowerManager.THERMAL_STATUS_CRITICAL
                ? "Phone too hot" + temp + ": stop playing for a while."
                : "Phone hot" + temp + ": Android slows the game. Pause, or lower the graphics.", Toast.LENGTH_LONG).show();
    }
}
