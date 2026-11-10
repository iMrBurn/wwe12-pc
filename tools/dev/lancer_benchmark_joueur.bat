@echo off
rem Test de performance, ancienne forme : le joueur choisit lui-meme un match d exhibition avec entrees (dossier neuf).
set BIN=%~dp0..\..\port\out\build\win-amd64-release
set ROOT=%USERPROFILE%\Documents\wwe12_benchmark
if exist "%ROOT%" rmdir /s /q "%ROOT%"
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --user_data_root "%ROOT%" --wwe12_benchmark=true --wwe12_benchmark_scenario=player_match
exit 0
