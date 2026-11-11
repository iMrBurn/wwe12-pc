@echo off
rem Corrections THQ 1.3 (session Gameplay) : les CAW comptent dans l'effectif des emissions Univers,
rem controles des numeros de titre. Donnees de test : Documents\wwe12_test (la vraie sauvegarde,
rem Documents\wwe12, n'est jamais touchee). Minimum d'effectif du jeu (15) pour ce test.
rem Journal dans test_tu.log a la racine du depot (lignes "[tu]" et "[univers] roster").
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_universe_min_roster=15 %* --log_file "%~dp0..\..\test_tu.log" --log_flush_interval 1
