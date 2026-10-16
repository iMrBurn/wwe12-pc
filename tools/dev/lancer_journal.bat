@echo off
rem Lance la version de test avec les messages de debogage du jeu (lignes "[game ...]") dans journal_jeu.log :
rem mode Univers (cartes, classements, Draft), replays, etc.
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --wwe12_game_log=true --log_file "%~dp0..\..\journal_jeu.log" --log_flush_interval 1 --log_max_file_size_mb 200
