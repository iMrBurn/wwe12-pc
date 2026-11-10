@echo off
rem Benchmark (1.3) : enregistrer le trajet du match fixe (hiac6), UNE fois, a la manette (croix ou stick).
rem Dossier de donnees NEUF (Documents\wwe12_benchmark, efface) : comme le benchmark le verra. Depuis l ecran-titre :
rem START, puis les menus jusqu au Hell in a Cell Armageddon a 6, les 6 catcheurs, l arene par defaut, jusqu au chargement.
rem Le trajet est ecrit dans le journal (ligne "[route] full: ...") : trajet_benchmark.log a la racine du depot.
set BIN=%~dp0..\..\port\out\build\win-amd64-release
set ROOT=%USERPROFILE%\Documents\wwe12_benchmark
if exist "%ROOT%" rmdir /s /q "%ROOT%"
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --user_data_root "%ROOT%" --log_file "%~dp0..\..\trajet_benchmark.log" --log_flush_interval 1 --wwe12_benchmark_record_route=true
