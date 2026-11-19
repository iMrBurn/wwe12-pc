@echo off
rem Filet des animations absentes (1.4.2, wwe12_motion_guard) : essai SANS joueur.
rem Le benchmark joue seul un 1 contre 1 (menus, entrees, 30 s de match) sur un dossier de donnees neuf
rem (Documents\wwe12_benchmark, efface a chaque lancement : la vraie sauvegarde n est pas touchee).
rem Pendant le match, le joueur 1 (debout depuis 2 s) recoit la motion 1852, qu aucune banque du jeu n a.
rem %1 = on (filet actif, par defaut) ou off (filet coupe : le temoin, il doit rester fige).
rem Journal : test_filet_motion_on.log / test_filet_motion_off.log a la racine du depot ; lignes "[motion guard]".
set BIN=%~dp0..\..\port\out\build\win-amd64-release
set MODE=%1
if "%MODE%"=="" set MODE=on
set GUARD=true
if /i "%MODE%"=="off" set GUARD=false
set ROOT=%USERPROFILE%\Documents\wwe12_benchmark
if exist "%ROOT%" rmdir /s /q "%ROOT%"
cd /d "%BIN%"
start "" wwe12.exe --user_data_root "%ROOT%" --wwe12_benchmark=true --wwe12_motion_guard=%GUARD% --wwe12_motion_guard_test=1852 --log_file "%~dp0..\..\test_filet_motion_%MODE%.log" --log_flush_interval 1
exit 0
