@echo off
rem Comme lancer_test_tu.bat, mais avec les correctifs THQ de l Univers COUPES (jeu d origine), pour comparer.
rem Journal dans test_tu_sans.log a la racine du depot.
cd /d "%~dp0..\..\port\outuild\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_universe_min_roster=15 --wwe12_tu_caw_roster=false --wwe12_tu_title_index=false %* --log_file "%~dp0..\..	est_tu_sans.log" --log_flush_interval 1
