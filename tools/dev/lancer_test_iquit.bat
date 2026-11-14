@echo off
rem I Quit (session Gameplay, 1.4) : le match SUBMISSION devient I QUIT, plus la sonde (fiche du match).
rem Donnees de test : Documents\wwe12_test. Journal dans test_iquit.log a la racine du depot.
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_iquit=true --wwe12_iquit_ref_anim=13144 --wwe12_iquit_victim_anim=13145 --wwe12_match_probe=true %* --log_file "%~dp0..\..\test_iquit.log" --log_flush_interval 1
