@echo off
rem Test du Discord Rich Presence (wwe12_discord) : Discord doit etre ouvert sur ce PC.
rem Donnees isolees (Documents\wwe12_test) : la vraie sauvegarde n est pas touchee.
rem A jouer : menus, un match exhibition, retour au menu, puis le mode Univers.
rem Journal dans discord_test.log (lignes "[discord]").
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_discord=true --wwe12_discord_app_id=1568590628952998010 %* --log_file "%~dp0..\..\discord_test.log" --log_flush_interval 1
