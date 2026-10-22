# WWE '12 PC — unofficial native port

A native Windows port of **WWE '12** (Yuke's/THQ, 2011) from its **Xbox 360** version, made by
**static recompilation** with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), and a
**native Vulkan renderer** adapted from [nefariousjosiah](https://github.com/nefariousjosiah)'s
SmackDown vs. Raw ports (itself built on [re:Blue](https://github.com/zolaware/reblue)).

> This repository contains **no game data**. You need your own copy of the Xbox 360 game.

*Français : portage PC natif de WWE '12 (version Xbox 360). Aucune donnée du jeu n'est fournie :
il faut votre propre copie. Téléchargez la dernière version dans « Releases », mettez l'image
.iso de votre jeu dans le dossier `game`, puis lancez `WWE12 Launcher.exe` (les mises à jour
suivantes se font en un clic depuis le launcher). Mode d'emploi complet en français dans
`LISEZMOI.txt`.*

## Playing

1. Download the latest zip from [Releases](https://github.com/iMrBurn/wwe12-pc/releases).
2. Unzip it, then put the disc image (`.iso`) of **your** WWE '12 (Xbox 360) in the `game`
   folder (or the disc contents: `default.xex`, `pac\`, `movies\`…).
3. Run `WWE12 Launcher.exe` (or `wwe12.exe` to start the game directly).

Requirements: Windows 10/11 64-bit, a graphics card with a Vulkan 1.2 driver (most cards since
2016; not Intel HD 4000/5000/6000 series, which have no Vulkan driver on Windows), an Xbox or PlayStation controller
(DualShock 4, DualSense) or the keyboard (ZQSD/WASD, layout in `README.txt`).
Settings (language, internal resolution…) are in `wwe12.toml`, explained in `README.txt`.

## Status (v0.9.0-beta)

- Menus, matches (1 to 6 wrestlers) and entrances render natively at 60 fps. Entrances and
  cutscenes run at 30 fps like on Xbox 360, or at 60 fps with an experimental option.
- **Launcher** (`WWE12 Launcher.exe`): settings, keyboard controls, mods, **saves** (backups,
  restore, **import of your Xbox 360 saves**), diagnostic info for bug reports, and **one-click
  updates** from this page's releases.
- **In-game graphics settings** (OPTIONS > GRAPHICS, built with the game's own menus):
  internal resolution from 540p to 4K (automatic by default: 720p on Steam Deck, integrated and
  small GPUs), texture filtering, crowd, shadows, depth of field, vertical sync, 60 fps entrances.
- **Match loading about twice as fast** as on the console, start-up videos skipped.
- DLC support (see below).
- All five languages of the European release (English, French, German, Spanish, Italian).
- An "EXIT" entry in the main menu (in place of SHOP, the dead Xbox LIVE store); Xbox LIVE is
  labelled "Online".
- Custom attires (Superstar Threads), eyes and teeth (NVIDIA and Intel), blood, post-match
  replays and MATCH HIGHLIGHTS, controller vibration (Universe mode included).
- **Custom theme music for created wrestlers**: drop your tracks (mp3, m4a, aac, wav, flac) in
  the `music` folder and pick them where the game chooses entrance themes. They play on the
  entrance, the post-match replay and the victory ceremony, with automatic loudness leveling.
- **Keyboard typing** in Create a Story and CAW name screens.
- **WWE Universe**: reset your Universe (with an automatic save backup), vacate a title no show
  uses, choose each Superstar's attire, manual rankings, full contender lists for minor, tag team
  and Divas titles, and SAVE a match you edited on the card without playing it.
- Video memory stays stable over long sessions (no more slowdowns after a few matches).
- A `mods` folder: files in it replace the game's own (HD titantron videos, textures...).
- Xbox and PlayStation controllers (with PlayStation button icons), and the keyboard, either
  together with a controller or as player 2 (two players with one controller).
- Display options: borderless window, FPS counter (F2), frame limit, CPU priority.

Known issues: coloured blocks during entrances and matches on some Intel and AMD graphics
(#7, #11, #13, #16: a probable fix is in 0.9, please report); white eyes on some AMD cards (#12);
white body with a custom colour for some wrestlers; a rare freeze fixed in 0.9 (#4, #8, #11:
please report if it still happens).

Planned for 1.0: every known bug fixed. Then: the title update (1.1), even shorter loading, a
richer Universe mode (qualifying matches, Royal Rumble surprise entrants, booking), a mod tool,
HD textures and videos, online play.

## Reporting a bug

Read the [FAQ](docs/FAQ.md) first: it answers the most common questions (DLC, antivirus, slow PCs,
black screen, freezes, saves).

Open an [issue](https://github.com/iMrBurn/wwe12-pc/issues) with: the version, what you were doing
(mode, wrestlers), a screenshot, and the text from the launcher's **ABOUT > Copy diagnostic info**
button (graphics card, driver, settings; no personal data). Logs are in `Documents\wwe12\logs`
(**ABOUT > Open the logs folder**). If the game freezes, wait 15 seconds, close it and attach
`hang_trace.txt` from that folder.

Antivirus warnings: the exes are not digitally signed, so Windows Defender may flag them by mistake.
Each release lists the SHA-256 of its files, so you can check yours against it.

## DLC

Put the DLC packages of **your** WWE '12 (Xbox 360) in the `dlc` folder: the files with long
hexadecimal names from the console's `Content\0000000000000000\54510890\00000002\` folder
(copied to a USB stick from the console's storage settings, then read on the PC with a tool such
as Horizon, Party Buffalo or Velocity). They are installed on the next start; lines starting with
`DLC` in the log say what was installed or skipped. DLC support by
[Banshee](https://github.com/itsbybanshee), who tested every pack:

| DLC | Contents |
|---|---|
| Extra DLC (free) | Kane (masked) alternate attire |
| The Rock Pre-Order Pack | The Rock |
| WrestleMania Pack | Christian and Edge (classic), Shawn Michaels, Jerry Lawler, Jim Ross, Michael Cole, Animal & Hawk, alternate attires |
| Divas Pack | Trish Stratus, Kharma, Brie Bella, Nikki Bella, Vickie Guerrero, Alicia Fox |
| Legends Pack | Batista, Mick Foley, Macho Man Randy Savage, Brodus Clay |
| All Unlockables | Superstars, Legends, Divas, attires, arenas and titles |
| Superstar Attribute Customizer | Edit ratings and attributes |

## Building

```powershell
# 1. Fetch, build and install the ReXGlue SDK (long the first time)
./setup.ps1

# 2. Copy default.xex from your game, then run setup again to generate the port/ project
Copy-Item <path>\default.xex port\assets\
./setup.ps1

# 3. Translate the game code, then build (debug, relwithdebinfo or release)
./build.ps1 -Codegen
./build.ps1 -Config release

# 4. Assemble dist\WWE12-PC and its zip (refuses to include any game data)
powershell -ExecutionPolicy Bypass -File tools\package.ps1
```

Requires Visual Studio 2022 with the "Desktop development with C++" workload, LLVM 20+
installed separately (in `C:\Program Files\LLVM`; VS's own clang is too old), plus Git,
CMake 3.25+, Ninja and Python 3. Local changes to the SDK and to
XenosRecomp are in `port/patches/` and applied by `setup.ps1`.

## Layout

```
setup.ps1, build.ps1   SDK install, code generation and build
port/                  the ReXGlue project
  src/native/          WWE '12 native renderer hooks, menu and text patches
  src/reblue/          renderer core (re:Blue, adapted)
  config/wwe12.toml    default settings, copied next to the exe
  dist/                readmes and notices shipped with releases
  patches/             changes to the SDK and XenosRecomp
tools/                 analysis scripts (function finder, shader tools, packaging)
  dev/                 test launchers (diagnostic log, texture dumps, 720p, iGPU)
```

### Finding missing functions

When `rexglue codegen` fails with "UnresolvedCall", pass its log to the finder:

```powershell
pip install cryptography   # for encrypted XEX files (retail discs)
python tools/find_fptrs.py port/assets/default.xex --codegen-log codegen.log --known port/wwe12_config.toml
```

It prints the lines to add to `[functions]` in `port/wwe12_config.toml`. After a first
successful codegen, `--registered port/generated/default/wwe12_register.cpp --merged` on the
decrypted image (`--dump-image image.bin`) lists the pointer-called functions ReXGlue missed,
and `--check-config` flags entries that cut a real function in two.

## Licence and credits

The project's code is under the **GNU GPL v3.0** ([LICENSE](LICENSE)). Third-party components
keep their own licences: see [THIRD_PARTY.md](THIRD_PARTY.md); releases carry every licence
text in their `licenses` folder.

Thanks to nefariousjosiah (native renderer of the SmackDown vs. Raw ports), Banshee
([@itsbybanshee](https://github.com/itsbybanshee): DLC support and build fixes), the ReXGlue and
re:Blue authors, and [wwe-svr2011-pc](https://github.com/KaikoClanworth1/wwe-svr2011-pc) for showing the
way (no code taken from it).

This is a fan project, not affiliated with WWE, THQ or Yuke's.
