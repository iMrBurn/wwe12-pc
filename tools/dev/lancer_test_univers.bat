@echo off
rem Test du mode Univers (wwe12_universe.cpp) : EDIT SUPERSTAR devient EDIT UNIVERSE, qui propose
rem de remettre l'Univers a zero (deux questions du jeu). A n'utiliser que sur une COPIE de la
rem sauvegarde. Journal : univers_test.log a la racine du depot.
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --wwe12_game_log=true --wwe12_universe_edit=true --log_file "%~dp0..\..\univers_test.log" --log_flush_interval 1 --log_max_file_size_mb 200
