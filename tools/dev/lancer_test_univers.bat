@echo off
rem Test du mode Univers (wwe12_universe.cpp) : sonde du menu Univers et remise a zero de test.
rem F8 dans le menu de l'Univers fait la remise a zero ; ressortir puis re-entrer pour la voir.
rem A n'utiliser que sur une COPIE de la sauvegarde. Journal : univers_test.log a la racine du depot.
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --wwe12_game_log=true --wwe12_universe_dev=true --log_file "%~dp0..\..\univers_test.log" --log_flush_interval 1 --log_max_file_size_mb 200
