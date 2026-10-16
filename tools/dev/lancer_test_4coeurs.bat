@echo off
rem Issue #4 : simule un processeur 4 coeurs sans hyperthreading (i5-6500) en limitant le jeu
rem aux coeurs P 0, 2, 4 et 6 (masque 0x55). Build release = exe de la 0.6.0.
set BIN=%~dp0..\..\port\out\build\win-amd64-release
cd /d "%BIN%"
start "" /affinity 55 "%BIN%\wwe12.exe" --log_file "%~dp0..\..\diagnostic_4coeurs.log" --log_flush_interval 1 %*
exit 0
