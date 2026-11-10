@echo off
rem Test ROSTERS : le jeu avec des donnees isolees (Documents\wwe12_test_launcher), la vraie sauvegarde nest pas touchee.
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test_launcher" %*
