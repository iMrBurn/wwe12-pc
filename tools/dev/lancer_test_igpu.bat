@echo off
rem Test "PC modeste" : rendu sur la carte graphique Intel integree, en 720p (issues #7 / #11).
rem Les mesures [perf] et [load] vont dans diagnostic_igpu.log.
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --log_file "%~dp0..\..\diagnostic_igpu.log" --log_flush_interval 1 --log_max_file_size_mb 200 --svr_gpu_device "Intel(R) RaptorLake-S Mobile Graphics Controller" --svr_render_scale 1
