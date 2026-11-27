@echo off
rem Test de la camera RAMP (Rendu, 2026-11-27) : en match, F5 passe de NORMAL a RAMP (vue vers la rampe, comme au
rem Royal Rumble) et retour. Donnees de test : Documents\wwe12_test. Journal : test_rampe.log a la racine du depot.
set BIN=%~dp0..\..\port\out\build\win-amd64-release
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --user_data_root "%USERPROFILE%\Documents\wwe12_test" --log_file "%~dp0..\..\test_rampe.log" --log_flush_interval 1
