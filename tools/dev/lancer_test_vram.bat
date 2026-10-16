@echo off
rem Fuite de memoire video (RetireHost) : plusieurs matchs d'affilee, la ligne [perf] du journal
rem donne la memoire video toutes les 10 s. 1440p comme le joueur du bug 01 (GTX 1650).
set BIN=%~dp0..\..\port\out\build\win-amd64-release
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --log_file "%~dp0..\..\diagnostic_vram.log" --log_flush_interval 1 --svr_render_scale=1440 %*
exit 0
