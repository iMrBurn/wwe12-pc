@echo off
rem Essai du jukebox : au retour aux menus apres un match, le dernier theme dentree entendu
rem doit remplacer la musique des menus.
rem Journal dans jukebox.log (lignes « [jukebox] »).
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --wwe12_jukebox_test=true %* --log_file "%~dp0..\..\jukebox.log" --log_flush_interval 1
