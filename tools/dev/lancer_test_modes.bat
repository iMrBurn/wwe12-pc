@echo off
rem Modes de match 1.4 (session Gameplay) : 2 finishers au depart pour tous, et squash (le 2e catcheur
rem commence a 20 %% de vie, le 1er avec son finisher), plus la sonde. Donnees de test : Documents\wwe12_test.
rem Journal dans test_modes.log a la racine du depot (lignes "[match]", "[finisher]", "[vie]").
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_starting_finishers=2 --wwe12_squash=1 --wwe12_match_probe=true %* --log_file "%~dp0..\..\test_modes.log" --log_flush_interval 1
