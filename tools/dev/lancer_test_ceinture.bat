@echo off
rem Ceinture a 60 fps (wwe12_title_fix) : entree de John Cena avec sa ceinture, a 60 images/s.
rem Le journal garde la position de la ceinture a chaque image ([belt]) et les corrections ([title]).
rem Ajouter --wwe12_title_fix=false pour comparer sans la correction.
set BIN=%~dp0..\..\port\out\build\win-amd64-release
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --log_file "%~dp0..\..\diagnostic_ceinture.log" --log_flush_interval 1 --log_max_file_size_mb 400 --wwe12_60fps=true --wwe12_belt_probe=true --wwe12_title_fix_log=true %*
exit 0
