@echo off
rem Jeu limite a 4 fils (affinite F) sans la mesure des piles : cadence du jeu ([vblank], [perf]) dans diagnostic_4fils_nm.log.
set BIN=%~dp0..\..\port\out\build\win-amd64-release
cd /d "%BIN%"
start "" /affinity F "%BIN%\wwe12.exe" --log_file "%~dp0..\..\diagnostic_4fils_nm.log" --log_flush_interval 1 %*
exit 0
