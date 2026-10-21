@echo off
rem Sang des catcheuses. Sonde : changer BLOOD (et rien d'autre) dans GAMEPLAY > IN-GAME OPTIONS,
rem puis quitter la page. Journal dans sang.log (lignes « [sang] »).
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --wwe12_blood_probe=true %* --log_file "%~dp0..\..\sang.log" --log_flush_interval 1
