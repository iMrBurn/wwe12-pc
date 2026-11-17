@echo off
rem Sonde des tombes (session Gameplay, 1.4) : etat des catcheurs et de l arbitre, et un enregistrement
rem pin_dump.bin (dossier de l exe) pour l analyse. Donnees de test : Documents\wwe12_test.
rem Journal dans test_tombe.log a la racine du depot (lignes "[tombe]").
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_pin_probe=true %* --log_file "%~dp0..\..\test_tombe.log" --log_flush_interval 1
