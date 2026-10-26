@echo off
rem Lance la version de test (relwithdebinfo) avec ses PROPRES donnees : Documents\wwe12_test au lieu de
rem Documents\wwe12 (sauvegarde, DLC installes, pack des catcheurs ajoutes, caches). Les essais ne touchent
rem donc plus la vraie sauvegarde ni les DLC du joueur (un essai avec un dossier mods vide avait retire le pack
rem des catcheurs ajoutes et la 0.9 ne chargeait plus la sauvegarde, 2026-10-26).
rem Les arguments sont passes au jeu : lancer_isole.bat --wwe12_jukebox_test=true ...
rem Autre build : set WWE12_BUILD=win-amd64-release avant de lancer.
if "%WWE12_BUILD%"=="" set WWE12_BUILD=win-amd64-relwithdebinfo
cd /d "%~dp0..\..\port\out\build\%WWE12_BUILD%"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" %*
