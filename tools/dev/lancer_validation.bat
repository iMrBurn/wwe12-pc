@echo off
rem Pavés de couleur (#7 #11 #13 #16) : couche de validation Vulkan avec la validation de synchronisation
rem (lectures/écritures d'images sans barrière, invisibles sur NVIDIA). Messages dans vulkan_validation.txt.
rem Le jeu est beaucoup plus lent sous la validation. Manette pilotable par remote_pad.txt.
set BIN=%~dp0..\..\port\out\build\win-amd64-release
set VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
set VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true
set VK_KHRONOS_VALIDATION_DEBUG_ACTION=VK_DBG_LAYER_ACTION_LOG_MSG
set VK_KHRONOS_VALIDATION_REPORT_FLAGS=error
set VK_KHRONOS_VALIDATION_LOG_FILENAME=%~dp0..\..\vulkan_validation.txt
set VK_KHRONOS_VALIDATION_DUPLICATE_MESSAGE_LIMIT=3
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --log_file "%~dp0..\..\diagnostic_validation.log" --log_flush_interval 1 --wwe12_remote_pad=true %*
exit 0
