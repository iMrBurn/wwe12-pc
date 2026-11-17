@echo off
rem Special Guest Referee (2026-11-16) : PLAY > SPECIALTY > SPECIAL REFEREE > un match > une stipulation
rem (port/src/native/wwe12_sref.cpp, coupe par defaut : --wwe12_sref=true ici). La derniere place de l ecran de choix
rem est l arbitre ; il compte avec A. Donnees de test : Documents\wwe12_test. Journal : test_arbitre3.log, lignes "[sref]".
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_sref=true --wwe12_match_probe=true --log_file "%~dp0..\..\test_arbitre3.log" --log_flush_interval 1
