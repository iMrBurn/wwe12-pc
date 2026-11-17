@echo off
rem Test "combien de catcheurs ajoutes tiennent" (1.4) sur la COPIE de la sauvegarde : Documents\wwe12_test (la vraie,
rem Documents\wwe12, n est pas touchee). Copies faites avant par tools\dev\copies_catcheurs_test.ps1 -N 60.
rem Journal dans test_catcheurs.log a la racine du depot (lignes "[heaps]" : remplissage de chaque tas, echecs).
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_heap_watermark=true --wwe12_roster_probe=true %* --log_file "%~dp0..\..\test_catcheurs.log" --log_flush_interval 1
