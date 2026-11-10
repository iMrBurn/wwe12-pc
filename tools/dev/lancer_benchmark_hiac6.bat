@echo off
rem Benchmark (1.3), match fixe : Hell in a Cell Armageddon a 6, menus parcourus seuls (trajet wwe12_benchmark_route),
rem dossier de donnees neuf (Documents\wwe12_benchmark, efface), 2 passes. Resume + bloc [results] :
rem Documents\wwe12_benchmark\logs\benchmark_*.txt ; historique benchmark_history.csv a cote de l exe.
rem Option : %1 = exe (wwe12.exe par defaut, ou wwe12_d3d12.exe / wwe12_d3d11.exe).
set BIN=%~dp0..\..\port\out\build\win-amd64-release
set EXE=%1
if "%EXE%"=="" set EXE=wwe12.exe
set ROOT=%USERPROFILE%\Documents\wwe12_benchmark
if exist "%ROOT%" rmdir /s /q "%ROOT%"
cd /d "%BIN%"
start "" "%BIN%\%EXE%" --user_data_root "%ROOT%" --wwe12_benchmark=true --wwe12_benchmark_scenario=hiac6
exit 0
