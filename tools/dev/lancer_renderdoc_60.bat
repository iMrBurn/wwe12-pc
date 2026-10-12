@echo off
rem Lance le jeu sous RenderDoc (couche Vulkan par variables d'environnement).
rem En jeu, F12 = capture d'une image, enregistree dans work\rdc.
set RD=C:\Users\melfo\Documents\WWE12-PC\renderdoc\RenderDoc_1.46_64
set VK_ADD_LAYER_PATH=%RD%
set VK_LAYER_PATH=%RD%
set VK_INSTANCE_LAYERS=VK_LAYER_RENDERDOC_Capture
set ENABLE_VULKAN_RENDERDOC_CAPTURE=1
set BIN=%~dp0..\..\port\out\build\win-amd64-relwithdebinfo
start "" "%RD%\renderdoccmd.exe" capture -d "%BIN%" -c "%~dp0..\..\work\rdc\capture" "%BIN%\wwe12.exe" --log_file "%~dp0..\..\diagnostic_rdc.log" --log_flush_interval 1 --log_max_file_size_mb 200 --wwe12_60fps=true
