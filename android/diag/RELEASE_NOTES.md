# WWE '12 PC - Android device check 1.0

**This is NOT the game.** It is a small app that checks whether your Android phone or handheld
could run a future Android version of WWE '12 PC. It contains no game files and does not need your ISO.

## How to help

1. Download `WWE12-DeviceCheck-1.0.apk` below and open it on your device
   (allow "install unknown apps" for your browser or file manager if Android asks).
2. Open **WWE12 Device Check** and wait about 5 seconds (the phone may get a bit warm: it runs a short benchmark).
3. Tap **Share** (or **Copy**) and post the whole report in the Discord or in a GitHub issue,
   with your **phone/handheld model** and whether you have a **Bluetooth/USB controller**.

The report lists: device model, chip, Android version, RAM, Vulkan features and texture formats
the game's renderer needs, the memory layout test and a CPU score. Nothing is sent anywhere by the
app itself; you choose what to share. A copy is saved in
`Android/data/io.github.imrburn.wwe12diag/files/wwe12_device_check.txt`.

Needs Android 8.0 or newer. 64-bit ARM (arm64-v8a) devices are the target.

## SHA256

```
c1c7157aeb2e5e8484685a823312fdfc29d53a02298104f0aa5b81c0cabd9676  WWE12-DeviceCheck-1.0.apk
```
