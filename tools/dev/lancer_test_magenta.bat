@echo off
rem Issues #7/#11/#13 : chaque nouvelle cible hote est remplie de magenta. Une zone magenta a l'ecran
rem = le jeu lit une image avant de l'avoir dessinee (noir sur NVIDIA, restes de memoire sur Intel/AMD).
set BIN=%~dp0..\..\port\out\build\win-amd64-release
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --log_file "%~dp0..\..\diagnostic_magenta.log" --log_flush_interval 1 --svr_debug_magenta=true %*
exit 0
