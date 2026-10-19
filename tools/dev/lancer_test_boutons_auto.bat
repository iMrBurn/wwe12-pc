@echo off
rem Icônes de boutons en mode auto (réglage par défaut) : brancher la manette PlayStation AVANT de lancer.
rem Journal dans boutons.log (lignes « Boutons PlayStation : texture ... remplacée »).
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --wwe12_button_icons=auto --log_file "%~dp0..\..\boutons.log" --log_flush_interval 1
