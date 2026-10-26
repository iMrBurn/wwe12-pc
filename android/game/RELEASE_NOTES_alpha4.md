# WWE '12 PC - Android alpha 4

Fourth test build of the Android version. It installs **over any earlier alpha** and keeps your
saves and settings. If you have alpha 3, the launcher offers it by itself (**UPDATE AVAILABLE**).

**No game files are included.** You need your own copy of WWE '12 (Xbox 360).

## What's new

**MY SAVES (new screen)**
- Tap **MANAGE MY SAVES** in the launcher: make backups of your save, restore one, or start from
  scratch. Restoring or deleting always backs up your current save first.
- **Import your Xbox 360 save**: pick the save files copied from your console, or a whole folder.
  The game installs them the next time you start it.
- **Export a backup as a .zip** (and add one back): handy, since Android hides the app's folder.
  Backups use the same layout as the PC version.

**DLC**
- New **DLC** panel: add your Xbox 360 DLC packages (several files at once, or a whole folder such
  as `Content\0000000000000000\54510890\00000002`). They are installed at the next game start, and
  can be removed from the same panel.

**Custom button icons**
- In **EDIT BUTTON LAYOUT**, select a button and tap **SET ICON (PNG)** to give it your own
  picture (or a stick its own knob). **REMOVE ICON** brings the normal look back.
- The on-screen button size setting now also applies to a layout you edited.
- Thanks to Yexclsv for this feature.

**Device check**
- The launcher tells you when your phone is below what the game needs (processor, memory, Vulkan,
  GPU, free storage) and asks before PLAY. It also shows a one-line summary of your phone (chip,
  GPU, memory): please include it in bug reports.

**Performance**
- The game now tells Android its 60 fps target every frame (Android 12+ "performance hint"), so the
  system keeps it on fast cores at a fitting speed instead of slowing down.
- A background task that renames some menu texts took 2-3 seconds of a processor core every 10
  seconds, even in matches: it is now 20 times faster and only runs around loading screens.
- Faster preparation of the graphics data on phones (ARM NEON).

**Other**
- New app icon, and the app is now called **WWE '12**.
- **SHARE LAST LOG** now keeps the start of the log too (phone, settings) and leaves out thousands
  of useless lines, so more of what matters fits.
- **DEBUG** panel at the bottom of the launcher, only for tests we ask you to do.

## What you need

- Android 9 or newer, 64-bit, **Snapdragon 845 / 7xx (2018) or newer**.
- A GPU with Vulkan 1.2 and BC texture support: **Snapdragon (Adreno) works.**
  **Mali GPUs (Samsung Exynos, MediaTek, Google Tensor) are not supported yet.**
- 6 GB of memory or more is recommended, and about 8 GB of free storage.

## Install

1. With alpha 3: open the launcher and tap **DOWNLOAD AND INSTALL** when it offers the update.
2. Otherwise: download `WWE12-PC-Android-alpha4.apk` below and open it on your device.
3. Open **WWE '12**, tap **CHOOSE MY ISO** (first time only), then **PLAY**.

## Known issues

- **Stuck after a match** (the winner freezes, no victory scene, cannot leave). It comes from the
  instant replay after the match: its playback never starts on Android. **For now, turn the instant
  replay off in the game's options**: the match then ends normally. We are working on a fix.
- No PlayStation button icons yet (Xbox icons only).
- Uninstalling the app deletes its folder (`Android/data/io.github.imrburn.wwe12`): your saves and
  any game files copied there. Updating keeps them. Export a backup first (MY SAVES).

## Reporting a problem

In the launcher, tap **SHARE LAST LOG** and post it (Discord or a GitHub issue) with your **phone
model**, what you did and what happened. After a match, the log now also tells where your phone
spends its time (`[perf] render thread split` line): this helps us speed the game up.

## SHA256

```
8a6b04a194022191b6e6b5ffe5af2ed64b373d1423523a051c7cd104e31ecbd9  WWE12-PC-Android-alpha4.apk
```
