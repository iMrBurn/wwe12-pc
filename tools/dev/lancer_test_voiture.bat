@echo off
rem Test : voiture d'Alberto Del Rio dessinee sans melange alpha (wwe12_car_opaque).
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --log_file "%~dp0..\..\diagnostic_car.log" --log_flush_interval 1 --log_max_file_size_mb 200 --wwe12_car_opaque=true
