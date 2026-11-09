@echo off
rem Sonde dos/ventre en fin de prise (wwe12_down_probe), question de la session Mods.
rem Donnees isolees (Documents\wwe12_test) : la vraie sauvegarde n est pas touchee.
rem A jouer : Batista contre Mark Henry, chacun place son finisher (Batista Bomb SvR / officielle).
rem Journal dans au_sol_test2.log (lignes "[au_sol]").
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test2" --wwe12_down_probe=true %* --log_file "%~dp0..\..\au_sol_test2.log" --log_flush_interval 1
