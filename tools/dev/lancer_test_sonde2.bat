@echo off
rem Sonde 2 pour la ceinture a 60 fps : 60 fps + journal [dtprobe2] des fonctions qui lisent les champs +60 et +32.
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --log_file "%~dp0..\..\diagnostic_sonde2.log" --log_flush_interval 1 --log_max_file_size_mb 200 --wwe12_60fps=true --wwe12_dtprobe2=true
