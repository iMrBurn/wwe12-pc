WWE '12 PC - unofficial native port
===================================

This port runs WWE '12 (Xbox 360) natively on PC. It contains NO game data: you need your own
copy of the Xbox 360 game.

Readme in other languages: LISEZMOI.txt (français), LIESMICH.txt (Deutsch), LEEME.txt (español),
LEGGIMI.txt (italiano).


Installation
------------

1. Put the disc image (.iso) of YOUR WWE '12 game (Xbox 360 version) in the "game" folder,
   for example game\WWE12.iso. It is read directly, no extraction needed.
   (You can also copy the disc contents there instead: game\default.xex, game\pac\..., etc.)

2. Run "WWE12 Launcher.exe": settings, controls, saves, mods and updates, then PLAY.
   (wwe12.exe starts the game directly with the current settings.)


Requirements
------------

- Windows 10 or 11, 64-bit.
- A graphics card with Vulkan 1.2 support (up-to-date drivers).
- An Xbox or PlayStation controller (DualShock 4, DualSense), or the keyboard.

Laptop with two graphics cards: in Settings > System > Display > Graphics, set wwe12.exe to
"High performance", and plug in the charger (some laptops turn off the dedicated graphics card
on battery).


Settings (wwe12.toml file)
--------------------------

- user_language: game language (1 English, 3 German, 4 French, 5 Spanish, 6 Italian).
- svr_render_scale: internal resolution (0 = automatic, 1 = 720p, 2 = 1440p, 3 = 4K).
  In automatic mode, the game stays at 720p on Steam Deck and on graphics built into the
  processor (Intel, AMD). If the game runs slowly on your PC, set it to 1.
- wwe12_60fps: 60 fps in entrances and cutscenes (experimental, off by default: title belts
  carried on a shoulder can move oddly).
- bd_anisotropy: sharpness of textures seen at an angle (0, 2, 4, 8 or 16).
- wwe12_quit_entry: the "EXIT" entry of the main menu, in place of SHOP (Xbox LIVE is
  labelled "ONLINE").

Keyboard (player 1, works together with a controller; change keys in wwe12.toml):
  Z Q S D / W A S D                         left stick (move)
  Arrows                                    right stick
  J / K / L / I                             X / A / B / Y
  U / O                                     LB / RB
  R / Space                                 LT / RT
  T / F / G / H                             D-pad
  Enter or Esc / Tab or Backspace           Start / Back
  C / N                                     L3 / R3


DLC
---

Put the DLC packages of YOUR game (Xbox 360) in the "dlc" folder: the files with long
hexadecimal names from the console's Content\0000000000000000\54510890\00000002\ folder.
They are installed the next time you start wwe12.exe (lines starting with "DLC" in logs\).
See dlc\HOW_TO_INSTALL_DLC.txt.


Mods
----

Files put in the "mods" folder replace the game's own, at the same path as on the disc
(example: mods\movies\titantron\100.bik). See mods\HOW_TO_USE_MODS.txt.


Where is my data?
-----------------

Saves, shader cache and game cache: Documents\wwe12


This is a fan project, not affiliated with WWE, THQ or Yuke's.
