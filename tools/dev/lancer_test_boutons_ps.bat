@echo off
rem Icônes de boutons PlayStation forcées (wwe12_button_icons = playstation), journal dans boutons.log.
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --wwe12_button_icons=playstation --log_file "%~dp0..\..\boutons.log" --log_flush_interval 1
