# WWE '12 PC – Android alpha 2

Second test build of the Android version, made from your reports: thank you! It installs **over
alpha 1** and keeps your saves and settings (no need to uninstall).

**No game files are included.** You need your own copy of WWE '12 (Xbox 360).

## What's new

**Fixes**
- **RT (reversals) works with the on-screen buttons.** LT and RT were stuck half-pressed all the
  time, which also made some buttons do the wrong thing.
- **On-screen buttons missing on some phones** (Poco F7 and others): some phones have a built-in
  device that looked like a controller and hid the buttons. The launcher now shows which
  controller it detects.
- **Black screen on some Snapdragon phones** (Adreno 720 and similar): should be fixed (thanks
  Yexclsv). Tell us if you still get it.
- The game's **EXIT** menu entry now closes the game and returns to the launcher.

**Performance**
- Two background threads were spinning at 100 % for nothing, each keeping a CPU core busy all the
  time. Fixed: **less heat, better battery, more CPU left for the game.**
- Built for modern ARM processors (ARMv8.2): faster thread synchronisation on every phone that
  can run the game. Your FPS reports will tell us how much it helps.
- **Fewer stutters the first time a match loads**: the app now ships the list of graphics
  pipelines to prepare in advance, like the PC version.

**Launcher**
- New **on-screen buttons size** setting (70 % to 110 %).
- Shows the controller it detects, and warns if the processor is too old.

## What you need

- Android 9 or newer, 64-bit, **Snapdragon 845 / 7xx (2018) or newer**. Older processors (Snapdragon
  835, 660, 625…) cannot start the game: the launcher says so.
- A GPU with Vulkan 1.2 and BC texture support: **Snapdragon (Adreno) works.**
  **Mali GPUs (Samsung Exynos, MediaTek, Google Tensor) are not supported yet.**
- Tested so far: Snapdragon 8 Elite (60 fps at 720p), Snapdragon 7s Gen 2 (45-60 fps at 540p).
- About 8 GB of free storage for the game.

## Install

1. Download `WWE12-PC-Android-alpha2.apk` below and open it on your device.
   Alpha 1 already installed? Just install this one over it.
2. Open **WWE '12 PC**, tap **CHOOSE MY ISO** (first time only), then **PLAY**.

## Known issues

- One report of being stuck after a pinfall (no winning scene) in an Extreme Rules match. If it
  happens to you, please send the log and say which match it was.
- No PlayStation button icons yet (Xbox icons only).
- On-screen buttons cannot be moved yet.
- Uninstalling the app deletes its folder (`Android/data/io.github.imrburn.wwe12`): your saves and
  any game files copied there. Installing a newer version over it keeps them.

## Reporting a problem

In the launcher, tap **SHARE LAST LOG** and post it (Discord or a GitHub issue) with your **phone
model**, what you did and what happened. The log now also measures how busy the processor is,
which tells us what to optimize next on your phone.

## SHA256

```
a8c75f6a489455b3fa57bf37f92427c315cce9d322b5fca608ed843881b555b0  WWE12-PC-Android-alpha2.apk
```
