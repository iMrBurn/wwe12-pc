@echo off
rem Pages d'arenes (wwe12_arena_pages.cpp) : dans le choix d'arene, RB = page suivante, LB = page precedente.
rem Test : la page 2 montre les 21 arenes en ordre inverse. Le journal note chaque page ([arena pages]).
set BIN=%~dp0..\..\port\out\build\win-amd64-release
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --log_file "%~dp0..\..\diagnostic_pages_arenes.log" --log_flush_interval 1 --wwe12_arena_pages_test=true %*
exit 0
