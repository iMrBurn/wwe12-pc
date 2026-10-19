@echo off
rem Clavier physique dans la grille de lettres du jeu (Create a Story, noms de CAW...).
rem Ouvrir une grille de lettres puis taper au clavier ; Retour arriere efface.
rem Journal dans clavier.log (lignes « [kbd] grille de lettres ouverte »).
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --wwe12_kbd=true --wwe12_kbd_log=true %* --log_file "%~dp0..\..\clavier.log" --log_flush_interval 1
