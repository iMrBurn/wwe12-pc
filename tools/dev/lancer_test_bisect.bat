@echo off
rem Bissection de la ceinture a 60 fps. Usage : lancer_test_bisect.bat "sub_XXXXXXXX,sub_YYYYYYYY,..." [valeur du champ +60 seul, ex 4.0]
rem Les fonctions listees voient les valeurs 30 fps de la structure d'images pendant leur appel.
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
if "%~2"=="" (set F60=0) else (set F60=%~2)
start "" wwe12.exe --log_file "%~dp0..\..\diagnostic_bisect.log" --log_flush_interval 1 --log_max_file_size_mb 200 --wwe12_60fps=true --wwe12_spoof30="%~1" --wwe12_spoof_f60=%F60%
