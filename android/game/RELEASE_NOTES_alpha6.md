# WWE '12 PC - Android alpha 6

Sixth test build of the Android version. It installs **over any earlier alpha** and keeps your
saves and settings. With alpha 4 or 5, the launcher offers it by itself (**UPDATE AVAILABLE**).

**No game files are included.** You need your own copy of WWE '12 (Xbox 360).

## What's new

**Full screen works** (issues #32, #35, #39)
- The game never read the **Picture shape** setting, so it always kept the 16:9 picture with black
  bars. "Fill the whole screen" now fills the screen.
- Same cause for **V-sync** and **texture sharpness**: what you chose in the launcher was ignored.
  They now apply.

**Mali GPUs (MediaTek, Exynos, Tensor): first try** (issues #34, #36)
- Mali GPUs cannot read the compressed textures of the Xbox 360 game, so the whole picture was
  black and white. The game now converts these textures for them by itself.
- **Not tested on a real Mali phone yet**: please try it and tell us (log with SHARE LAST LOG).
  Expect more memory use and longer loadings than on Snapdragon.

**Custom drivers (Turnip)** (issue #30)
- With some driver packages the game silently went back to the phone's own driver. It now tries
  harder to use the driver you picked, and the log says which driver is really used (the GPU name
  starts with "Turnip" when Turnip is used).
- Snapdragon 845 (Adreno 630): the phone's own driver cannot run the game. Please try a recent
  Turnip build made for Adreno 6xx and send the log.

**On-screen buttons** (issue #38)
- New option under CONTROLS: **Always show the on-screen buttons**, for phones where they
  vanished in the game (a built-in device passed for a game controller).

**Launcher updates** (issue #40)
- On slow or unstable connections, the update download now resumes where it stopped instead of
  failing. (This helps from the next update on: alpha 6 itself is downloaded by your current
  launcher.)

## What you need

- Android 9 or newer, 64-bit.
- **Snapdragon (Adreno) 7xx/8xx from 2019 or newer** works best. Mali GPUs: first try, see above.
- A GPU with Vulkan 1.2 (bufferDeviceAddress and descriptorIndexing).
- 6 GB of memory or more is recommended, and about 8 GB of free storage.
- Custom Turnip drivers: if the game stops at start with "lacks Vulkan features the game needs",
  that driver does not offer what the game needs: try the phone's own driver or another Turnip
  version.

## Install

1. With alpha 4 or 5: open the launcher and tap **DOWNLOAD AND INSTALL** when it offers the update.
2. Otherwise: download `WWE12-PC-Android-alpha6.apk` below and open it on your device.
3. Open **WWE '12**, tap **CHOOSE MY ISO** (first time only), then **PLAY**.

## Known issues

- Instant replay after matches is still **off** on Android (it could freeze the game after the
  match). You can turn it back on in the DEBUG panel, at your own risk.
- A match loading can sometimes never end (tips screen, right after the loading starts): close
  the game and start it again, and send the log. We are working on it.
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
cbe41333950c830f91eb6ec864b58ee48a00833ac58a5e4a1ab33ad08c526738  WWE12-PC-Android-alpha6.apk
```
