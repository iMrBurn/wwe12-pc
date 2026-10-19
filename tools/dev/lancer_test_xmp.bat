@echo off
rem Messages du lecteur de musique Xbox (XMP) : journal détaillé dans xmp.log.
rem Aller dans l'éditeur d'entrée d'un catcheur créé > musique > USER PLAYLIST, puis quitter.
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --log_level=debug %* --log_file "%~dp0..\..\xmp.log" --log_flush_interval 1 --log_max_file_size_mb 300
