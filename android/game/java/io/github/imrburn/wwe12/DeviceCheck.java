// What the launcher can tell about the phone before PLAY: a one-line summary (for bug reports) and
// the reasons the game will not run, or will run badly. Only what Android says without starting
// the game: processor (ARMv8.2 atomics, fastest core), memory, free storage, Vulkan level, GPU.
// Thresholds from the testers' reports (2026-10): Snapdragon 865 / 7s Gen 2 and newer play; a
// Snapdragon 480 (2 cores at 2.2 GHz, Adreno 619) does not get past the title screen.
// Merged with Yexclsv's DEVICE CHECK: the GPU's real name read through OpenGL ES, the free
// storage, Vulkan 1.2 (alpha 3 notes) and the question before PLAY (LauncherActivity.play).
// Not for the UI thread: it creates a tiny OpenGL context.
package io.github.imrburn.wwe12;

import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLSurface;
import android.opengl.GLES20;
import android.os.Build;
import android.os.StatFs;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public class DeviceCheck {
    static final long MIN_RAM_GB = 6;          // below: out-of-memory kills during matches
    static final int MIN_FAST_CORE_MHZ = 2300; // fastest core below that: the game's main thread can't keep up
    static final long MIN_FREE = 8L << 30;     // the game's files, caches and saves

    final String summary;
    final String gpu;                                 // "Adreno (TM) 740", "Mali-G78"... or ""
    final List<String> blockers = new ArrayList<>();  // the game cannot start
    final List<String> warnings = new ArrayList<>();  // it starts, but expect trouble

    DeviceCheck(Context context, File appDir) {
        gpu = glRenderer();
        long ramGb = Math.round(totalRam(context) / (1024.0 * 1024 * 1024));
        int mhz = fastestCoreMhz();
        summary = chipName() + (gpu.isEmpty() ? "" : " | " + gpu) + " | " + ramGb + " GB RAM | fastest core "
                + (mhz > 0 ? (mhz / 100) / 10.0 + " GHz" : "?") + " | Vulkan " + vulkanText(context)
                + " | Android " + Build.VERSION.RELEASE;

        if (LauncherActivity.cpuTooOld())
            blockers.add("The processor is too old: the game needs ARMv8.2 (Snapdragon 845 / 7xx from 2018 or newer).");
        if (Build.SUPPORTED_64_BIT_ABIS.length == 0)
            blockers.add("This phone is 32-bit only: the game needs a 64-bit phone.");
        if (!hasVulkan(context, 0x401000))
            blockers.add("No Vulkan graphics driver: the game cannot draw anything on this phone.");
        else if (!hasVulkan(context, 0x402000))
            // Some phones with an old driver declare less and still work with a custom driver.
            warnings.add("This phone does not declare Vulkan 1.2 (the game needs it): the picture may stay black. "
                    + "On a Snapdragon, a custom GPU driver (GPU DRIVER panel) can help.");
        if (ramGb > 0 && ramGb < MIN_RAM_GB)
            warnings.add(ramGb + " GB of memory: Android may close the game during matches (6 GB or more recommended).");
        if (mhz > 0 && mhz < MIN_FAST_CORE_MHZ)
            warnings.add("The processor is slow for this game (fastest core " + (mhz / 100) / 10.0
                    + " GHz): expect very low speed, or a crash on the title screen.");
        String g = gpu.toLowerCase(Locale.US);
        if (g.contains("mali"))
            warnings.add("This phone has a Mali GPU (" + gpu + "), not supported yet: expect a black screen or "
                    + "crashes. Snapdragon (Adreno) phones work.");
        else if (!g.contains("adreno") && LauncherActivity.gpuLooksUnsupported())
            warnings.add("This phone does not use a Snapdragon (Adreno) GPU: other GPUs (Mali, Xclipse, PowerVR) "
                    + "are barely tested yet and may show graphics bugs or not start.");
        try {
            long free = new StatFs((appDir != null ? appDir : context.getFilesDir()).getAbsolutePath()).getAvailableBytes();
            if (free < MIN_FREE)
                warnings.add(String.format(Locale.US, "Only %.1f GB of free storage: the game needs about 8 GB.",
                        free / 1073741824.0));
        } catch (Exception ignored) {
        }
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

    // Renderer name of the phone's GPU through a 1x1 OpenGL ES context (Java cannot ask Vulkan).
    static String glRenderer() {
        EGLDisplay d = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
        EGLContext c = EGL14.EGL_NO_CONTEXT;
        EGLSurface s = EGL14.EGL_NO_SURFACE;
        try {
            int[] v = new int[2];
            if (d == EGL14.EGL_NO_DISPLAY || !EGL14.eglInitialize(d, v, 0, v, 1)) return "";
            int[] attr = {EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT, EGL14.EGL_SURFACE_TYPE,
                    EGL14.EGL_PBUFFER_BIT, EGL14.EGL_NONE};
            EGLConfig[] cfg = new EGLConfig[1];
            int[] n = new int[1];
            if (!EGL14.eglChooseConfig(d, attr, 0, cfg, 0, 1, n, 0) || n[0] < 1) return "";
            c = EGL14.eglCreateContext(d, cfg[0], EGL14.EGL_NO_CONTEXT,
                    new int[] {EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE}, 0);
            s = EGL14.eglCreatePbufferSurface(d, cfg[0], new int[] {EGL14.EGL_WIDTH, 1, EGL14.EGL_HEIGHT, 1, EGL14.EGL_NONE}, 0);
            if (c == EGL14.EGL_NO_CONTEXT || s == EGL14.EGL_NO_SURFACE || !EGL14.eglMakeCurrent(d, s, s, c)) return "";
            String r = GLES20.glGetString(GLES20.GL_RENDERER);
            return r == null ? "" : r;
        } catch (Throwable t) {
            return "";
        } finally {
            try {
                EGL14.eglMakeCurrent(d, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT);
                if (s != EGL14.EGL_NO_SURFACE) EGL14.eglDestroySurface(d, s);
                if (c != EGL14.EGL_NO_CONTEXT) EGL14.eglDestroyContext(d, c);
            } catch (Throwable ignored) {
            }
        }
    }
}
