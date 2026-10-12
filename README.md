# WWE '12 PC — unofficial native port

A native Windows port of **WWE '12** (Yuke's/THQ, 2011) from its **Xbox 360** version, made by
**static recompilation** with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk), and a
**native Vulkan renderer** adapted from [nefariousjosiah](https://github.com/nefariousjosiah)'s
SmackDown vs. Raw ports (itself built on [re:Blue](https://github.com/zolaware/reblue)).

> This repository contains **no game data**. You need your own copy of the Xbox 360 game.

*Français : portage PC natif de WWE '12 (version Xbox 360). Aucune donnée du jeu n'est fournie :
il faut votre propre copie. Téléchargez la dernière version dans « Releases », mettez l'image
.iso de votre jeu dans le dossier `game`, puis lancez `wwe12.exe`. Mode d'emploi complet en
français dans `LISEZMOI.txt`.*

## Playing

1. Download the latest zip from [Releases](https://github.com/iMrBurn/wwe12-pc/releases).
2. Unzip it, then put the disc image (`.iso`) of **your** WWE '12 (Xbox 360) in the `game`
   folder (or the disc contents: `default.xex`, `pac\`, `movies\`…).
3. Run `wwe12.exe`.

Requirements: Windows 10/11 64-bit, a Vulkan 1.2 graphics card, an Xbox or PlayStation controller
(DualShock 4, DualSense) or the keyboard (ZQSD/WASD, layout in `README.txt`).
Settings (language, internal resolution…) are in `wwe12.toml`, explained in `README.txt`.

## Status (v0.3.0-beta)

- Menus, matches (1 to 6 wrestlers) and entrances render natively at 60 fps (entrances run at
  30 fps, like on Xbox 360).
- Internal resolution from 720p to 4K. Automatic mode picks it from the screen, and stays at
  720p on Steam Deck and integrated GPUs (an Intel iGPU holds 60 fps there).
- All five languages of the European release (English, French, German, Spanish, Italian). The
  game's "Xbox 360 console" texts say "PC".
- An "EXIT" entry in the main menu (in place of SHOP, the dead Xbox LIVE store); Xbox LIVE is
  labelled "Online".
- Compiled pipelines are kept in `Documents\wwe12\cache` to avoid stutter on later runs.
- Custom attires (Superstar Threads), wrestlers' eyes and teeth, and blood render correctly.
- Sharp textures at an angle (16x anisotropic filtering).
- A `mods` folder: files in it replace the game's own (HD titantron videos, textures...).
- A `dlc` folder: Xbox 360 DLC packages of your own copy put there are installed on startup
  (into `Documents\wwe12\0000000000000000\54510890\00000002`) and seen by the game.
- Xbox and PlayStation controllers, and the keyboard (together with a controller).

Known issues: the titantron video is low-resolution; long loading screens; on-screen button
prompts stay Xbox ones with a PlayStation controller.

Planned: an in-game graphics options page and a real 8th "QUIT" entry, 60 fps entrances and
cutscenes, a launcher, HD textures and videos, online play.

## Building

```powershell
# 1. Fetch, build and install the ReXGlue SDK (long the first time)
./setup.ps1

# 2. Copy default.xex from your game, then run setup again to generate the port/ project
Copy-Item <path>\default.xex port\assets\
./setup.ps1

# 3. Translate the game code
./build.ps1 -Codegen

# 4. Extract and convert the game's shaders for the native renderer (needs the extracted disc
#    contents, not the .iso): builds reblue-XenosRecomp, writes port/generated/native/
./tools/gen_shaders.ps1 -GameDir <path to the extracted disc>

# 5. Build (debug, relwithdebinfo or release)
./build.ps1 -Config release

# 6. Assemble dist\WWE12-PC and its zip (refuses to include any game data)
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

Thanks to nefariousjosiah (native renderer of the SmackDown vs. Raw ports), the ReXGlue and re:Blue
authors, and [wwe-svr2011-pc](https://github.com/KaikoClanworth1/wwe-svr2011-pc) for showing the
way (no code taken from it).

This is a fan project, not affiliated with WWE, THQ or Yuke's.
