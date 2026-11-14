@echo off
rem Special Referee, essai 2 (2026-11-14) : la regle 83 jouee DES LE MENU (la ligne TRIPLE THREAT NORMAL, regle 13,
rem donne 83 / 180 comme la ligne inutilisee du jeu), pour que l ecran de choix et les participants soient construits
rem depuis la fiche 83 (3e place = arbitre). Donnees de test : Documents\wwe12_test. Journal : test_arbitre2.log,
rem lignes "[regle]". Autre regle de depart : lancer_test_arbitre2.bat 0  (0 = ONE ON ONE).
set FROM=%1
if "%FROM%"=="" set FROM=13
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_rule_from=%FROM% --wwe12_rule_to=83 --wwe12_rule_to_b=180 --wwe12_match_probe=true --log_file "%~dp0..\..\test_arbitre2.log" --log_flush_interval 1
