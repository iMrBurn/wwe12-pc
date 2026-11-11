@echo off
rem Test groupe (session Mods) sur une COPIE de la sauvegarde : Documents\wwe12_test (la vraie, Documents\wwe12,
rem n est pas touchee). Catcheurs ajoutes (places, Univers, createur de prises), entree gardee, prises ajoutees.
rem Journal dans test_groupe.log a la racine du depot (lignes "[superstar mods]").
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" %* --log_file "%~dp0..\..\test_groupe.log" --log_flush_interval 1
