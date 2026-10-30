# WWE '12 PC - Android alpha 7

Seventh test build of the Android version. It installs **over any earlier alpha** and keeps your
saves and settings. With alpha 4 or newer, the launcher offers it by itself (**UPDATE AVAILABLE**).

**No game files are included.** You need your own copy of WWE '12 (Xbox 360).

## What's new

**Match loading that never ends: fixed** (the "tips screen" loading)
- Two causes were found and fixed. One was the game waiting forever for the screen thread; the other
  was a loading thread that kept losing a race against the render thread, so each step took seconds.
  On our test device, 6 match loadings in a row now finish, where about one in two used to hang.
- If a loading still hangs on your phone, send the log: it now says when a thread waits too long.

**Sound** (issue #45)
- Sound is now always sent as stereo, which fixes the missing audio on some phones.
  Please tell us if you still hear nothing (and send the log).

**Phone temperature and heat warning** (thanks to Yexclsv)
- New options: show the phone temperature next to the FPS counter, and warn when the phone is hot.
- The game's rumble (hits, etc.) can now make the phone vibrate, with an adjustable strength
  (CONTROLS: **Game vibration (rumble) on the phone**).

**Graphics drivers: clearer messages** (issues #30, #41, #42, #43)
- When the game refuses a phone because the graphics driver lacks Vulkan features, the message and
  the log now say which driver answered, its version, and exactly which features are missing.
  This will help us decide how to support more Snapdragon 845/855/870 phones.
- If the game crashes, the log now says where it happened.

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
2. Otherwise: download `WWE12-PC-Android-alpha7.apk` below and open it on your device.
3. Open **WWE '12**, tap **CHOOSE MY ISO** (first time only), then **PLAY**.

## Known issues

- Instant replay after matches is still **off** on Android (it could freeze the game after the
  match). You can turn it back on in the DEBUG panel, at your own risk.
- Some freezes are still being investigated: after a Universe match (#46), during a Triple H
  entrance (#47), and in Create a Wrestler when choosing a logo (#49). Send the log if it happens.
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
(filled in when the APK is built)
```
