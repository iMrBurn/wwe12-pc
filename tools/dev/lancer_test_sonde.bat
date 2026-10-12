@echo off
rem Sonde pour la ceinture a 60 fps : 60 fps + journal [dtprobe] des fonctions qui lisent les pas de temps.
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --log_file "%~dp0..\..\diagnostic_sonde.log" --log_flush_interval 1 --log_max_file_size_mb 200 --wwe12_60fps=true --wwe12_dtprobe=true
