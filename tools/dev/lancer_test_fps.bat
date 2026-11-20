@echo off
rem Test (Rendu, 2026-11-20) : le nouveau compteur d'images (1.5), centre en haut, affiche des le lancement
rem (F2 le cache / le montre). Donnees de test : Documents\wwe12_test.
rem Usage : lancer_test_fps.bat [petit]   (petit = fenetre 1280x720, pour voir la taille sur un petit ecran)
set BIN=%~dp0..\..\port\out\build\win-amd64-release
set WIN=
if /i "%1"=="petit" set WIN=--fullscreen=false --window_width=1280 --window_height=720
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_show_fps=true %WIN% --log_file "%~dp0..\..\test_fps.log" --log_flush_interval 1
