@echo off
rem Diagnostic : enregistre les textures decodees dans work\texdump2 (pour les yeux noirs).
set SVR_DUMP_TEXTURES=%~dp0work\texdump2
cd /d "%~dp0port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --log_file "%~dp0diagnostic.log" --log_flush_interval 1 --log_max_file_size_mb 200
