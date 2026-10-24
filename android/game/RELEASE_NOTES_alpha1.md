# WWE '12 PC – Android alpha 1

**First public test of the Android version.** It is the same port as WWE '12 PC (0.9), built for
Android phones and handhelds. This is an **alpha**: it may run slowly, show graphical bugs or close
on some devices. That is exactly what we want to find out, so your reports matter!

**No game files are included.** You need your own copy of WWE '12 (Xbox 360).

## What you need

- Android 9 or newer, 64-bit (arm64) device
- A GPU with Vulkan 1.2 and BC texture support: **Snapdragon (Adreno) works.**
  **Mali GPUs (Samsung Exynos, MediaTek, Google Tensor) are not supported yet.**
- Recommended: 8 GB RAM, Snapdragon 8 Gen 2 or newer. Older or mid-range chips: unknown yet, tell us how it runs!
- About 8 GB of free storage for the game
- Not sure about your device? Install **WWE12 Device Check** first (separate release): it tells you in 5 seconds.

## Install

1. Download `WWE12-PC-Android-alpha1.apk` below and open it on your device
   (allow "install unknown apps" for your browser or file manager if Android asks).
2. Open **WWE '12 PC**. The launcher opens.
3. Tap **CHOOSE MY ISO** and pick your WWE '12 Xbox 360 ISO.
   Or copy your extracted game files (or the ISO) into
   `Android/data/io.github.imrburn.wwe12/files/game` (over USB from a PC).
4. Tap **PLAY**.

## Controls

- **On-screen buttons** (Xbox layout) appear over the game. Opacity can be changed in the launcher.
- A **Bluetooth or USB controller** (Xbox, PlayStation…) works too; the on-screen buttons hide while one is connected.
- The Android **Back** button closes the game and returns to the launcher.

## Settings (launcher)

Language, internal resolution (720p by default; **540p if it runs slowly**), FPS counter, shadows,
crowd (turn it off for more speed), 60 fps entrances.

## Known limitations

- Performance on real phones is unknown: this alpha is mainly here to measure it.
- No PlayStation button icons yet (Xbox icons only).
- The game's EXIT menu entry does not close the app: use Android's Back button.
- On-screen buttons cannot be moved or resized yet.
- **Uninstalling the app deletes its folder (`Android/data/io.github.imrburn.wwe12`): your saves and
  any game files copied there.** Installing a newer version over this one keeps them.

## Reporting a problem

In the launcher, tap **SHARE LAST LOG** and post it (Discord or a GitHub issue) with:
your **device model**, what you did, and what happened. Tell us the FPS you get too (FPS counter in the launcher).

## SHA256

```
8215649fb7761215dbf8d58822973dea8b07514d388502fc3b3be6a77d8a6c64  WWE12-PC-Android-alpha1.apk
```
