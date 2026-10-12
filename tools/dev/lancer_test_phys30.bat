@echo off
rem Test ceinture a 60 fps : la physique avance par pas de 1/30 s (wwe12_phys30).
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --log_file "%~dp0..\..\diagnostic_phys30.log" --log_flush_interval 1 --log_max_file_size_mb 200 --wwe12_60fps=true --wwe12_phys30=true
exit 0
