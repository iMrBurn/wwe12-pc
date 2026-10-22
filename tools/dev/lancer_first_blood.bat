@echo off
rem Plantage apres une victoire par First Blood (sub_82488DE0) : jeu normal, journal dans diagnostic_fb.log.
set BIN=%~dp0..\..\port\out\build\win-amd64-release
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --log_file "%~dp0..\..\diagnostic_fb.log" --log_flush_interval 1 %*
exit 0
