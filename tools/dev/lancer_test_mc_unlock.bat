@echo off
rem Match Creator sans restrictions (wwe12_mc_unlock) : regles et conditions de victoire au choix.
rem Donnees isolees (Documents\wwe12_test) : la vraie sauvegarde nest pas touchee.
rem Journal dans match_creator.log (lignes « [mc] unlock »).
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_mc_unlock=true %* --log_file "%~dp0..\..\match_creator.log" --log_flush_interval 1
