@echo off
rem Profil du processeur sans limite de fils (ex. Elimination Chamber) : SVR_SAMPLE_STACKS note toutes les
rem 5 ms la pile de chaque fil dans Documents\wwe12\logs\samples.txt. Manette pilotable par remote_pad.txt.
set BIN=%~dp0..\..\port\out\build\win-amd64-release
set SVR_SAMPLE_STACKS=5
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --log_file "%~dp0..\..\diagnostic_profil.log" --log_flush_interval 1 --wwe12_remote_pad=true %*
exit 0
