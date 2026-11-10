@echo off
rem Test de performance integre (1.3, comme celui du recomp WWE 2K14) : le jeu parcourt seul les menus jusqu a un
rem 1 contre 1 avec ses 2 entrees, mesure les entrees puis 30 s de match, une passe, sur un dossier de donnees neuf
rem (Documents\wwe12_benchmark, efface a chaque lancement : la sauvegarde du joueur n est jamais touchee).
rem Resume + bloc [results] : Documents\wwe12_benchmark\logs\benchmark_*.txt ; historique benchmark_history.csv a cote de l exe.
rem Option : %1 = exe (wwe12.exe par defaut, ou wwe12_d3d12.exe / wwe12_d3d11.exe).
set BIN=%~dp0..\..\port\out\build\win-amd64-release
set EXE=%1
if "%EXE%"=="" set EXE=wwe12.exe
set ROOT=%USERPROFILE%\Documents\wwe12_benchmark
if exist "%ROOT%" rmdir /s /q "%ROOT%"
cd /d "%BIN%"
start "" "%BIN%\%EXE%" --user_data_root "%ROOT%" --wwe12_benchmark=true
exit 0
