# WWE '12 PC - Android alpha 5

Fifth test build of the Android version. It installs **over any earlier alpha** and keeps your
saves and settings. With alpha 4, the launcher offers it by itself (**UPDATE AVAILABLE**).

**No game files are included.** You need your own copy of WWE '12 (Xbox 360).

## What's new

**Stuck after a match: instant replay off for now** (issues #22, #23, #24, #29, #31)
- After the bell, the winner froze, no victory scene, and the game could not be left. It comes
  from the instant replay after the match: on Android the game sometimes stops saving the
  match's highlights, and the replay then never starts. We found where it stops, not yet why.
- Until it is fixed, the instant replay and MATCH HIGHLIGHTS are **off** on Android: the match
  ends straight on the victory scene. You can turn the replay back on in the launcher's DEBUG
  panel, at your own risk.
- If a match still freezes after the bell, please send the log (see below).

**Full screen** (issue #32)
- New **Picture shape** setting in the launcher: keep the original 16:9 picture with black bars,
  or **fill the whole screen** (the picture is stretched).

**Other**
- DEBUG panel: switches for the instant replay and for emptying the game's cache at each start,
  and a log of the replay file, for the tests we ask you to do.

## What you need

- Android 9 or newer, 64-bit, **Snapdragon 845 / 7xx (2018) or newer**.
- A GPU with Vulkan 1.2 and BC texture support: **Snapdragon (Adreno) works.**
  **Mali GPUs (Samsung Exynos, MediaTek, Google Tensor) are not supported yet.**
- 6 GB of memory or more is recommended, and about 8 GB of free storage.
- Custom Turnip drivers: if the game stops at start with "lacks Vulkan features the game needs",
  that driver version does not offer what the game needs: use the phone's own driver or
  another Turnip version (issue #30).

## Install

1. With alpha 4: open the launcher and tap **DOWNLOAD AND INSTALL** when it offers the update.
2. Otherwise: download `WWE12-PC-Android-alpha5.apk` below and open it on your device.
3. Open **WWE '12**, tap **CHOOSE MY ISO** (first time only), then **PLAY**.

## Known issues

- A match loading can sometimes never end (tips screen, right after the loading starts): close
  the game and start it again, and send the log. We are working on it.
- No PlayStation button icons yet (Xbox icons only).
- Uninstalling the app deletes its folder (`Android/data/io.github.imrburn.wwe12`): your saves and
  any game files copied there. Updating keeps them. Export a backup first (MY SAVES).

## Reporting a problem

In the launcher, tap **SHARE LAST LOG** and post it (Discord or a GitHub issue) with your **phone
model**, what you did and what happened. If a match freezes, tick **Game debug messages in the
log** in the DEBUG panel first: the log then shows where the game stopped.

## SHA256

```
7d93d289b652f7217c3fcd5e400b3c3114aeb08d62a34a8b5169b692b339c663  WWE12-PC-Android-alpha5.apk
```
