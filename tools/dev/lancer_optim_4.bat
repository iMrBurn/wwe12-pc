@echo off
rem Test d'optimisation 4 : Intel 720p, chargements accélérés (wwe12_load_slice_ms 12). Mesures dans optim_4.log.
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --log_file "%~dp0..\..\optim_4.log" --log_flush_interval 1 --log_max_file_size_mb 200 --svr_gpu_device "Intel(R) RaptorLake-S Mobile Graphics Controller" --svr_render_scale 1
