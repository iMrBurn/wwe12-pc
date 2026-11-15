@echo off
rem Special Guest Referee, essai 3 (2026-11-15) : la ligne TRIPLE THREAT > NORMAL joue la regle 83 (3e place = arbitre),
rem ecran de choix a 3 places, et le catcheur-arbitre compte avec A (port/src/native/wwe12_sref.cpp).
rem Donnees de test : Documents\wwe12_test. Journal : test_arbitre3.log, lignes "[sref]" et "[regle]".
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_sref=true --wwe12_rule_from=13 --wwe12_rule_to=83 --wwe12_rule_to_b=180 --wwe12_match_probe=true --log_file "%~dp0..\..\test_arbitre3.log" --log_flush_interval 1
