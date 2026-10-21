@echo off
rem Ceinture d'Orton a 60 fps : captures RenderDoc automatiques pendant l'entree (series d'images
rem consecutives), dans work\rdc\orton. Version de diagnostic (WWE12_DEV_PROBES) : out\build\win-amd64-dev.
set RD=C:\Users\melfo\Documents\WWE12-PC\renderdoc\RenderDoc_1.46_64
set VK_ADD_LAYER_PATH=%RD%
set VK_LAYER_PATH=%RD%
set VK_INSTANCE_LAYERS=VK_LAYER_RENDERDOC_Capture
set ENABLE_VULKAN_RENDERDOC_CAPTURE=1
set BIN=%~dp0..\..\port\out\build\win-amd64-dev
start "" "%RD%\renderdoccmd.exe" capture -d "%BIN%" -c "%~dp0..\..\work\rdc\orton" "%BIN%\wwe12.exe" --log_file "%~dp0..\..\diagnostic_autocap.log" --log_flush_interval 1 --log_max_file_size_mb 200 --wwe12_60fps=true --wwe12_autocap=true --wwe12_autocap_delay_ms=8000 --wwe12_autocap_gap_ms=2000 --wwe12_autocap_series=6 --wwe12_autocap_frames=4 --wwe12_remote_pad=true %*
exit 0
