@echo off
rem Couleur perso toute blanche : meme test sur la carte Intel integree en 720p, manette pilotable par
rem remote_pad.txt, journal dans diagnostic_couleur_igpu.log.
set BIN=%~dp0..\..\port\out\build\win-amd64-release
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --log_file "%~dp0..\..\diagnostic_couleur_igpu.log" --log_flush_interval 1 --wwe12_remote_pad=true --svr_gpu_device "Intel(R) RaptorLake-S Mobile Graphics Controller" --svr_render_scale 1 %*
exit 0
