@echo off
rem Mesure des chargements : piles des threads toutes les 20 ms (Documents\wwe12\logs\samples.txt)
rem et messages du jeu (repères : écran de chargement, entrée, cloche) dans journal_chargement.log.
rem Faire un match exhibition 1 contre 1 avec les entrées, jusqu'à la cloche, puis quitter.
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
set SVR_SAMPLE_STACKS=20
start "" wwe12.exe --wwe12_game_log=true --wwe12_load_probe=true %* --log_file "%~dp0..\..\journal_chargement.log" --log_flush_interval 1 --log_max_file_size_mb 200
