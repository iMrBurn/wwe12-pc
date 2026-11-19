@echo off
rem Filet des animations absentes (1.4.2, wwe12_motion_guard) : essai SANS joueur.
rem Le benchmark joue seul un 1 contre 1 (menus, entrees, puis le match) sur un dossier de donnees neuf
rem (Documents\wwe12_benchmark, efface a chaque lancement : la vraie sauvegarde n est pas touchee).
rem %1 = on (filet actif, par defaut) ou off (filet coupe : le temoin, il doit rester fige).
rem %2 = vide : 12 s apres le chargement du match, l acteur 0 recoit la motion 1852, qu aucune banque du jeu n a
rem      (essai chaque seconde, 5 fois au plus).
rem %2 = prise : des 12 s, le catcheur PRIS dans la premiere prise vue recoit la motion 1852 ; le filet doit liberer
rem      les deux hommes ensemble. Match de 90 s pour laisser le temps a une prise d arriver.
rem Journal : test_filet_motion_<on|off>[_prise].log a la racine du depot ; lignes "[motion guard]".
set BIN=%~dp0..\..\port\out\build\win-amd64-release
set MODE=%1
if "%MODE%"=="" set MODE=on
set GUARD=true
if /i "%MODE%"=="off" set GUARD=false
set TEST=--wwe12_motion_guard_test=1852
set NAME=%MODE%
if /i "%2"=="prise" set TEST=--wwe12_motion_guard_test_pair=1852 --wwe12_benchmark_match_s=90
if /i "%2"=="prise" set NAME=%MODE%_prise
set ROOT=%USERPROFILE%\Documents\wwe12_benchmark
if exist "%ROOT%" rmdir /s /q "%ROOT%"
cd /d "%BIN%"
start "" wwe12.exe --user_data_root "%ROOT%" --wwe12_benchmark=true --wwe12_motion_guard=%GUARD% %TEST% --log_file "%~dp0..\..\test_filet_motion_%NAME%.log" --log_flush_interval 1
exit 0
