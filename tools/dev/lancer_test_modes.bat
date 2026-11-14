@echo off
rem Modes de match 1.4 (session Gameplay) : 2 finishers au depart pour tous, et squash a 100 %% (le moins bien note
rem commence a 20 %% de vie, l autre avec un finisher), plus la sonde. Donnees de test : Documents\wwe12_test.
rem Journal dans test_modes.log a la racine du depot (lignes "[match]", "[finisher]", "[vie]").
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_starting_finishers=2 --wwe12_squash_chance=100 --wwe12_select_finishers=true --wwe12_match_probe=true %* --log_file "%~dp0..\..\test_modes.log" --log_flush_interval 1
