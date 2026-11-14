@echo off
rem Sonde de match (session Gameplay, 1.4) : jauge de finishers et vie par zone des catcheurs, lignes
rem "[finisher]" et "[vie]". Ne change rien au jeu. Donnees de test : Documents\wwe12_test.
rem Journal dans test_finishers.log a la racine du depot.
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_match_probe=true %* --log_file "%~dp0..\..\test_finishers.log" --log_flush_interval 1
