# WWE '12 PC - Android alpha 8

Eighth test build of the Android version. It installs **over any earlier alpha** and keeps your
saves and settings. With alpha 4 or newer, the launcher offers it by itself (**UPDATE AVAILABLE**).

**No game files are included.** You need your own copy of WWE '12 (Xbox 360).

## What's new

**Clear messages instead of a black screen** (issues #41, #30, #42)
- If the game's files are incomplete (for example a game folder with `default.xex` but without the
  `pac` folder), the launcher now tells you **before** PLAY, and the game stops with a clear message
  instead of staying on a black screen.
- If the graphics cannot start (many Snapdragon 845/855/870 phones: their own driver lacks features
  the game needs), the launcher now explains it and suggests a Turnip driver.
- The log now says which game files the game found, and the limits of your graphics driver (this
  will help us support more Snapdragon 6xx phones).
- The yellow "may not run" warning no longer claims the picture will stay black: Android only
  *announces* a Vulkan level, and the game checks the real driver itself when it starts.

**Lighter settings on weaker phones**
- On a **first install**, phones with 4 GB of memory or less (or slow cores) start with the
  LOW-END PHONE preset, phones with less than 8 GB start with a lighter BALANCED one (crowd and
  blur off). Change them any time under PERFORMANCE PRESETS. Existing installs keep their settings.

**For testers**
- The log now also times the graphics driver's pipeline builds (`[pso]` lines), to find out why some
  screens freeze (#49).

## What you need

- Android 9 or newer, 64-bit.
- **Snapdragon (Adreno) 7xx/8xx from 2019 or newer** works best. Mali GPUs (MediaTek, Exynos,
  Tensor): the picture should be in color since alpha 6, but no real Mali phone has confirmed yet.
- A GPU with Vulkan 1.2 (bufferDeviceAddress and descriptorIndexing).
- 6 GB of memory or more is recommended, and about 8 GB of free storage.
- Custom Turnip drivers: if the game stops at start with "lacks Vulkan features the game needs",
  that driver does not offer what the game needs: try the phone's own driver or another Turnip
  version.

## Install

1. With alpha 4 or newer: open the launcher and tap **DOWNLOAD AND INSTALL** when it offers the update.
2. Otherwise: download `WWE12-PC-Android-alpha8.apk` below and open it on your device.
3. Open **WWE '12**, tap **CHOOSE MY ISO** (first time only), then **PLAY**.

## Known issues

- Instant replay after matches is still **off** on Android (it could freeze the game after the
  match). You can turn it back on in the DEBUG panel, at your own risk.
- Some freezes are still being investigated: after a Universe match (#46), during a Triple H
  entrance (#47), in Create a Wrestler when choosing a logo (#49), and after some 1v1 matches (#51).
  Send the log if it happens.
- The phone gets warm: the whole game runs at 60 fps. Lower the internal resolution, or turn off
  shadows and crowd, to make it lighter.
- No PlayStation button icons yet (Xbox icons only).
- Uninstalling the app deletes its folder (`Android/data/io.github.imrburn.wwe12`): your saves and
  any game files copied there. Updating keeps them. Export a backup first (MY SAVES).

## Reporting a problem

In the launcher, tap **SHARE LAST LOG** and post it (Discord or a GitHub issue) with your **phone
model**, what you did and what happened. If a match freezes, tick **Game debug messages in the
log** in the DEBUG panel first: the log then shows where the game stopped.

## SHA256

```
b3d87805cf2175c72fa138d54aa3d8342ae452403f697bc188bc330381040c2e  WWE12-PC-Android-alpha8.apk
```
