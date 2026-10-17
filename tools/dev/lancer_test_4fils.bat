@echo off
rem Gels sur les PC a moins de 6 fils (#4, #11, Nothing) et profil du processeur : le jeu est ensuite
rem limite a 4 fils (affinite 0xF, deux coeurs P avec hyperthreading, comme un 2C/4T). SVR_SAMPLE_STACKS
rem note toutes les 5 ms la pile de chaque fil dans Documents\wwe12\logs\samples.txt (crash_trace.cpp).
set BIN=%~dp0..\..\port\out\build\win-amd64-release
set SVR_SAMPLE_STACKS=5
cd /d "%BIN%"
start "" /affinity F "%BIN%\wwe12.exe" --log_file "%~dp0..\..\diagnostic_4fils.log" --log_flush_interval 1 %*
exit 0
