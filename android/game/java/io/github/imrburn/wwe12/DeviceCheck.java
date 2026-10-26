// What the launcher can tell about the phone before PLAY: a one-line summary (for bug reports) and
// the reasons the game will not run, or will run badly. Only what Android says without starting
// the game: processor (ARMv8.2 atomics, fastest core), memory, Vulkan level, chip maker.
// Thresholds from the testers' reports (2026-10): Snapdragon 865 / 7s Gen 2 and newer play; a
// Snapdragon 480 (2 cores at 2.2 GHz, Adreno 619) does not get past the title screen.
package io.github.imrburn.wwe12;

import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;

public class DeviceCheck {
    static final long MIN_RAM_GB = 6;         // below: out-of-memory kills during matches
    static final int MIN_FAST_CORE_MHZ = 2300; // fastest core below that: the game's main thread can't keep up

    final String summary;
    final List<String> blockers = new ArrayList<>();  // the game cannot start
    final List<String> warnings = new ArrayList<>();  // it starts, but expect trouble

    DeviceCheck(Context context) {
        long ramGb = Math.round(totalRam(context) / (1024.0 * 1024 * 1024));
        int mhz = fastestCoreMhz();
        String chip = chipName();
        boolean adreno = !LauncherActivity.gpuLooksUnsupported();
        summary = chip + " | " + ramGb + " GB RAM | fastest core " + (mhz > 0 ? (mhz / 100) / 10.0 + " GHz" : "?")
                + " | Vulkan " + vulkanText(context) + " | Android " + Build.VERSION.RELEASE;

        if (LauncherActivity.cpuTooOld())
            blockers.add("The processor is too old: the game needs ARMv8.2 (Snapdragon 845 / 7xx from 2018 or newer).");
        if (!hasVulkan(context, 0x401000))
            blockers.add("No Vulkan 1.1 graphics driver: the game cannot draw anything on this phone.");
        if (ramGb > 0 && ramGb < MIN_RAM_GB)
            warnings.add(ramGb + " GB of memory: Android may close the game during matches (6 GB or more recommended).");
        if (mhz > 0 && mhz < MIN_FAST_CORE_MHZ)
            warnings.add("The processor is slow for this game (fastest core " + (mhz / 100) / 10.0
                    + " GHz): expect very low speed, or a crash on the title screen.");
        if (!adreno)
            warnings.add("This phone does not use a Snapdragon (Adreno) GPU: other GPUs (Mali, Xclipse, PowerVR) "
                    + "are barely tested yet and may show graphics bugs or not start.");
    }

    boolean ok() { return blockers.isEmpty() && warnings.isEmpty(); }

    static long totalRam(Context context) {
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        ActivityManager.MemoryInfo info = new ActivityManager.MemoryInfo();
        if (am == null) return 0;
        am.getMemoryInfo(info);
        return info.totalMem;
    }

    // Highest cpuinfo_max_freq of all cores, in MHz (0 when Android hides it).
    static int fastestCoreMhz() {
        int best = 0;
        for (int i = 0; i < 16; i++) {
            File f = new File("/sys/devices/system/cpu/cpu" + i + "/cpufreq/cpuinfo_max_freq");
            if (!f.exists()) continue;
            try (BufferedReader r = new BufferedReader(new FileReader(f))) {
                best = Math.max(best, Integer.parseInt(r.readLine().trim()) / 1000);
            } catch (Exception ignored) {
            }
        }
        return best;
    }

    static String chipName() {
        if (Build.VERSION.SDK_INT >= 31 && !Build.SOC_MODEL.isEmpty() && !"unknown".equals(Build.SOC_MODEL))
            return Build.SOC_MANUFACTURER + " " + Build.SOC_MODEL;
        return Build.HARDWARE;
    }

    static boolean hasVulkan(Context context, int version) {
        return context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_VERSION, version);
    }

    static String vulkanText(Context context) {
        if (hasVulkan(context, 0x403000)) return "1.3";
        if (hasVulkan(context, 0x402000)) return "1.2";
        if (hasVulkan(context, 0x401000)) return "1.1";
        if (hasVulkan(context, 0x400000)) return "1.0";
        return "none";
    }
}
