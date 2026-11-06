@echo off
rem Test de performance intégré (wwe12_benchmark) : match d'exhibition par défaut joué 2 fois (caches froids puis
rem chauds), sans personne à la manette, sur un dossier de données neuf (Documents\wwe12_benchmark, effacé à chaque
rem lancement : la sauvegarde du joueur n'est jamais touchée). Résumé : Documents\wwe12_benchmark\logs\benchmark_*.txt.
rem Option : %1 = exe (wwe12.exe par défaut, ou wwe12_d3d12.exe / wwe12_d3d11.exe).
set BIN=%~dp0..\..\port\out\build\win-amd64-release
set EXE=%1
if "%EXE%"=="" set EXE=wwe12.exe
set ROOT=%USERPROFILE%\Documents\wwe12_benchmark
if exist "%ROOT%" rmdir /s /q "%ROOT%"
cd /d "%BIN%"
start "" "%BIN%\%EXE%" --user_data_root "%ROOT%" --wwe12_benchmark=true
exit 0
