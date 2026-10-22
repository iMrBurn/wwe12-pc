@echo off
rem Sang des catcheuses : piege sur le drapeau « pas de sang » du match (wwe12_blood_trap) et
rem manette pilotee a distance (remote_pad.txt a cote de l'exe). Journal : sang_piege.log.
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --wwe12_blood_trap=true %* --log_file "%~dp0..\..\sang_piege.log" --log_flush_interval 1
