@echo off
rem Lance la version "rendu natif" avec un journal (diagnostic.log).
rem En match ou pendant une entree, appuie sur F9 quand l'image est deformee :
rem le detail de cette image est ecrit dans diagnostic.log.
cd /d "%~dp0port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --log_file "%~dp0diagnostic.log" --log_flush_interval 1 --log_max_file_size_mb 200
