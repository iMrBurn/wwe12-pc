@echo off
rem Interblocage du turbo sur 4 fils : turbo des chargements actif, jeu limite a 4 fils (affinite F),
rem manette pilotee a distance (remote_pad.txt a cote de l'exe, wwe12_remote_pad).
set BIN=%~dp0..\..\port\out\build\win-amd64-release
cd /d "%BIN%"
start "" /affinity F "%BIN%\wwe12.exe" --log_file "%~dp0..\..\diagnostic_turbo4.log" --log_flush_interval 1 --wwe12_load_turbo=true --wwe12_remote_pad=true %*
exit 0
