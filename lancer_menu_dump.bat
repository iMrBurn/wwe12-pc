@echo off
rem Diagnostic : a la premiere validation dans un menu, enregistre le menu dans work\menudump.bin.
set WWE12_MENU_DUMP=%~dp0work\menudump.bin
cd /d "%~dp0port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --log_file "%~dp0diagnostic.log" --log_flush_interval 1
