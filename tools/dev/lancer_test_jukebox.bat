@echo off
rem OPTIONS > JUKEBOX : les menus jouent les themes dentree des catcheurs.
rem Donnees isolees (Documents\wwe12_test, comme lancer_isole.bat) : la vraie sauvegarde nest pas touchee.
rem Journal dans jukebox.log (lignes « [jukebox] »).
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" %* --log_file "%~dp0..\..\jukebox.log" --log_flush_interval 1
