@echo off
rem Test d'optimisation 1 : Intel 720p, horloge Windows normale (avant). Mesures [perf] et [load] dans optim_1.log.
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --log_file "%~dp0..\..\optim_1.log" --log_flush_interval 1 --log_max_file_size_mb 200 --svr_gpu_device "Intel(R) RaptorLake-S Mobile Graphics Controller" --svr_render_scale 720 --wwe12_precise_timer=false
