// WWE '12 PC - Android: the phone's temperature for the FPS counter (shown after the FPS, in C).
// Reads the battery temperature from Android's battery broadcast (EXTRA_TEMPERATURE, tenths of a degree,
// accurate to 0.1 C) and passes it to native code (wwe12_temp.cpp). It is the one temperature Android gives
// to apps: the CPU / GPU sensors need system permission. Started by WWE12Activity when the setting is on. By Yexclsv.
package io.github.imrburn.wwe12;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;

class TempMonitor extends BroadcastReceiver {
    static native void nativeSetTemp(int tenthsOfDegree);

    static volatile int lastTenths = -1000;  // also used by ThermalWatch's message
    final Context context;

    TempMonitor(Context context) {
        this.context = context;
        // The sticky broadcast gives the current value right away; the receiver keeps it up to date.
        Intent now = context.registerReceiver(this, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        if (now != null) onReceive(context, now);
    }

    void stop() {
        try {
            context.unregisterReceiver(this);
        } catch (Exception ignored) {
        }
        publish(-1000);
    }

    @Override
    public void onReceive(Context c, Intent intent) {
        int t = intent.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, Integer.MIN_VALUE);
        if (t != Integer.MIN_VALUE && t > -500 && t < 1500) publish(t);
    }

    static void publish(int tenths) {
        lastTenths = tenths;
        try {
            nativeSetTemp(tenths);
        } catch (UnsatisfiedLinkError ignored) {  // libraries not loaded (test builds): no counter
        }
    }
}
