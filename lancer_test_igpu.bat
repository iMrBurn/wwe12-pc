@echo off
rem Test "PC modeste" : rendu sur la carte graphique Intel integree, en 720p.
rem Les mesures [perf] et [load] vont dans diagnostic.log.
cd /d "%~dp0port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --log_file "%~dp0diagnostic.log" --log_flush_interval 1 --log_max_file_size_mb 200 --svr_gpu_device "Intel(R) RaptorLake-S Mobile Graphics Controller" --svr_render_scale 1
