# WWE12-PC v0.4.0 (Beta) [By Banshee]

DLC update by **Banshee**, based on **iMrBurn's** WWE '12 PC port ([v0.3.0-beta](https://github.com/iMrBurn/wwe12-pc/releases/tag/v0.3.0-beta)).

- No game data included. You need your own legally acquired WWE '12 (Xbox 360) files.

## What's new

- **DLC support**: put your Xbox 360 DLC files in the new `dlc` folder and they install automatically on startup.

## Installation

1. Unzip the download anywhere.
2. Put your WWE '12 (Xbox 360) `.iso` in the `game` folder.
3. Put your DLC files in the `dlc` folder.
4. Run `wwe12.exe`.

**Requirements:** Windows 10/11 64-bit, Vulkan 1.2 graphics card, controller or keyboard.

## Getting your DLC files

The DLC files are in this folder on your Xbox 360:
`Content\0000000000000000\54510890\00000002\`

- **Regular console:** Settings → System → Storage → Hard Drive → Games and Apps → WWE '12 → copy the DLC to a USB stick. Then open the stick on your PC with **Horizon**, **Party Buffalo** or **Velocity** and extract the files from that folder.
- **Modded console (RGH/JTAG):** copy them over FTP from `Hdd1:\Content\0000000000000000\54510890\00000002\`.
- Or you can just check out this cool [link](https://gofile.io/d/dOx11hyj).

Copy those files into the `dlc` folder. Don't put them in `Documents\wwe12` yourself, because the game can't read them there.

## WWE '12 DLC

✅ = tested and working.

| DLC | Contents | Status |
|---|---|---|
| **Extra DLC** (free) | Kane (Masked) alternate attire | ✅ |
| **The Rock Pre-Order Pack** | The Rock | ✅ |
| **WrestleMania Pack** | Christian (Classic), Edge (Classic), Shawn Michaels, Jerry Lawler, Jim Ross, Michael Cole, Animal & Hawk and Edge & Christian alternate attires | ✅ |
| **Divas Pack** | Trish Stratus, Kharma, Brie Bella, Nikki Bella, Vickie Guerrero, Alicia Fox (free) | ✅ |
| **Legends Pack** | Batista, Mick Foley, Macho Man Randy Savage, Brodus Clay (free) | ✅ |
| **All Unlockables** | Unlocks 60+ Superstars, Legends, Divas, attires, arenas and titles | ✅ |
| **Superstar Attribute Customizer** | Edit overall ratings and attributes for every Superstar | ✅ |


## Troubleshooting

- After launching, check the newest file in `logs` for lines starting with `DLC`. They show what was installed or skipped, and why.
- To reinstall a DLC, delete its folder in `Documents\wwe12\0000000000000000\54510890\00000002\` and restart.
- Your saves are in `Documents\wwe12`, in the profile folder (e.g. `B13E...`). Don't delete it.

## Credits

- **iMrBurn**: original WWE '12 PC port
- **nefariousjosiah**: native renderer from the SmackDown vs. Raw ports
- **ReXGlue** and **re:Blue** authors
- **Banshee** ([@itsbybanshee](https://x.com/itsbybanshee)): DLC support and build fixes

Fan project, not affiliated with WWE, THQ or Yuke's.
