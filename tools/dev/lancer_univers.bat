@echo off
rem Lance la version de test avec le journal du mode Univers (lignes "[uni]") dans univers.log.
rem Pour le Draft : jouer un "WWE DRAFT MATCH" apres WrestleMania jusqu'au bout, puis quitter.
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --wwe12_universe_log=true --log_file "%~dp0..\..\univers.log" --log_flush_interval 1 --log_max_file_size_mb 200
