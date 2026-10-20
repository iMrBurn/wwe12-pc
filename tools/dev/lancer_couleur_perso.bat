@echo off
rem Tenue en couleur perso toute blanche en match (Discord 2026-10-20) : jeu normal, manette pilotable
rem par remote_pad.txt, journal dans diagnostic_couleur.log.
set BIN=%~dp0..\..\port\out\build\win-amd64-release
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --log_file "%~dp0..\..\diagnostic_couleur.log" --log_flush_interval 1 --wwe12_remote_pad=true %*
exit 0
