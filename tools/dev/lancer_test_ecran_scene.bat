@echo off
rem Sonde de l'ecran de scene (wwe12_screen_probe) : videos des ecrans d'arene, de l'entree au match.
rem Donnees isolees (Documents\wwe12_test) : la vraie sauvegarde nest pas touchee.
rem A jouer : un match exhibition simple dans une arene de base, avec les entrees, puis 20 s de match.
rem Journal dans ecran_scene.log (lignes « [screen] »).
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_screen_probe=true %* --log_file "%~dp0..\..\ecran_scene.log" --log_flush_interval 1
