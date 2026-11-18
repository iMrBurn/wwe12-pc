@echo off
rem Ceinture d'un autre jeu (mods de type belt, Mod Maker > CEINTURES ET ARMES) sur la COPIE de la sauvegarde
rem (Documents\wwe12_test). Journal : test_ceinture.log (lignes "[belts]" et "[entrances] /SOBJ/... replaced").
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" %* --log_file "%~dp0..\..\test_ceinture.log" --log_flush_interval 1
