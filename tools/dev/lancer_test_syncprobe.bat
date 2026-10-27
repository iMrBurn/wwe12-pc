@echo off
rem Sonde de synchro GPU (docs/couches.md) : journal [syncprobe] toutes les 2 s (attentes du jeu sur le GPU nul).
rem Compiler avec la variable d'environnement WWE12_DEV_PROBES definie avant la configuration (build.ps1).
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --log_file "%~dp0..\..\diagnostic_syncprobe.log" --log_flush_interval 1 --log_max_file_size_mb 200 --wwe12_syncprobe=true
