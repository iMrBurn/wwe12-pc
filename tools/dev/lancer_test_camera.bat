@echo off
rem Sonde camera (Rendu, 2026-11-27, docs/etude_camera_libre.md palier A) : trouve la camera de match du jeu.
rem Usage : lancer_test_camera.bat          (donnees de test Documents\wwe12_test : lancer un match, bouger son
rem                                          catcheur dans le ring pendant ~1 minute, puis quitter)
rem         lancer_test_camera.bat bench    (le benchmark, sans joueur : son match fait bouger la camera ; dossier
rem                                          Documents\wwe12_benchmark refait a neuf)
rem Journal : test_camera.log a la racine du depot (lignes [camprobe] ; la sonde ecrit "done" quand elle a fini).
rem Les options sont ecrites en "--nom valeur" (un .bat coupe les arguments sur le signe =).
set BIN=%~dp0..\..\port\out\build\win-amd64-release
set ROOT=%USERPROFILE%\Documents\wwe12_test
set EXTRA=
if /i "%1"=="bench" (
  set ROOT=%USERPROFILE%\Documents\wwe12_benchmark
  set EXTRA=--wwe12_benchmark true
)
if /i "%1"=="bench" if exist "%USERPROFILE%\Documents\wwe12_benchmark" rmdir /s /q "%USERPROFILE%\Documents\wwe12_benchmark"
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --user_data_root "%ROOT%" --wwe12_camera_probe true %EXTRA% --log_file "%~dp0..\..\test_camera.log" --log_flush_interval 1
