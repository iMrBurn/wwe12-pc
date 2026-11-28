@echo off
rem Sonde souris dans les menus (Rendu, 2026-11-28, docs/etude_souris_menus.md etape 0). Donnees de test
rem Documents\wwe12_test. Journal : test_souris.log a la racine du depot ; dessins des menus dans
rem Documents\wwe12_test\logs\mouse_layout_NN.txt et images dans Documents\wwe12_test\logs\shots\mouse_NN.bmp.
set BIN=%~dp0..\..\port\out\build\win-amd64-release
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_mouse_probe true --log_file "%~dp0..\..\test_souris.log" --log_flush_interval 1
