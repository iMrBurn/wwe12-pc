# WWE '12 PC – Android alpha 3

Third test build of the Android version. It installs **over alpha 1 or 2** and keeps your saves and
settings (no need to uninstall).

**No game files are included.** You need your own copy of WWE '12 (Xbox 360).

## What's new

**Updates from the launcher (new!)**
- From now on, the launcher checks for new versions when you open it. When one is out, an
  **UPDATE AVAILABLE** panel appears: tap **DOWNLOAD AND INSTALL**, then **Update** in Android's
  window. Your saves and settings are kept.
- The first time, Android asks you to allow "Install unknown apps" for WWE '12 PC.
- This version still has to be installed by hand, the next ones won't.

**Custom GPU drivers (Turnip), Snapdragon only**
- New **GPU DRIVER** panel in the launcher: import a Turnip driver `.zip` (the same packages used by
  Winlator, Eden, Skyline…) and pick it. This can fix black screens or missing Vulkan features
  with old phone drivers (for example the Snapdragon 865 / Adreno 650).
- "System driver (default)" keeps the phone's own driver, as before. The log says which driver was used.
- Thanks to Yexclsv for this feature. **It is new and untested: please report how it goes.**

**Fixes**
- **Black screen after leaving the app** (Home button, notification…) and coming back: the picture
  now comes back. This is probably also the black screen on the **Retroid Pocket 6** (GitHub #21).
- A rare crash when the game starts is fixed.

**Performance**
- The graphics pipelines are now **kept between launches**: fewer stutters from the second time
  you play a match, arena or wrestler.
- The first match load prepares pipelines faster (3 threads instead of 1): fewer long freezes on
  mid-range phones.
- Less work per frame for the graphics thread (about a third less in our tests).

## What you need

- Android 9 or newer, 64-bit, **Snapdragon 845 / 7xx (2018) or newer**.
- A GPU with Vulkan 1.2 and BC texture support: **Snapdragon (Adreno) works.**
  **Mali GPUs (Samsung Exynos, MediaTek, Google Tensor) are not supported yet.**
- About 8 GB of free storage for the game.

## Install

1. Download `WWE12-PC-Android-alpha3.apk` below and open it on your device (over your current
   version if you have one).
2. Open **WWE '12 PC**, tap **CHOOSE MY ISO** (first time only), then **PLAY**.

## Known issues

- **Stuck after a match** (post-match replays not loading, or endless loading), seen in Universe and
  Extreme Rules. We are looking for the cause: if it happens, please send the log (see below).
- No PlayStation button icons yet (Xbox icons only).
- On-screen buttons cannot be moved yet.
- Uninstalling the app deletes its folder (`Android/data/io.github.imrburn.wwe12`): your saves and
  any game files copied there. Updating keeps them.

## Reporting a problem

In the launcher, tap **SHARE LAST LOG** and post it (Discord or a GitHub issue) with your **phone
model**, what you did and what happened. The log now also contains the driver's own error messages,
which helps a lot with black screens.

## SHA256

```
1d5bd1f894483719575da7c6a73b2825aa12cbab72b515b534c088125c0f12bf  WWE12-PC-Android-alpha3.apk
```
