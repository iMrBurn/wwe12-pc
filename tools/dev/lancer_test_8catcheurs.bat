@echo off
rem 8 catcheurs, palier 0 (2.0, session Gameplay, docs/etude_plus_de_6_catcheurs.md) : les deux personnages des
rem cases 7-8 deviennent des catcheurs (superstars 123 et 139 par defaut : wwe12_dev_eight_ids "12302,13902").
rem A FAIRE : le match a 6 (6-MAN TAG ou ELIMINATION CHAMBER) doit etre le PREMIER match apres le lancement
rem (le jeu cree les cases 7-8 une seule fois). Jouer 2 minutes, puis quitter.
rem Les tas 37 et 38 (cases 7-8, 492 Ko) sont agrandis de 660 Ko, a la taille des cases 0-5 (1152 Ko).
rem A lire dans test_8catcheurs.log : [eight] (cases, roles, positions, tas), [perf] (images), [heaps] (manques).
rem Donnees de test : Documents\wwe12_test.
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_dev_eight_wrestlers=true --wwe12_heap_grow="12:16384,15:4096,37:660,38:660" --wwe12_heap_grow_from=0 --wwe12_heap_watermark=true %* --log_file "%~dp0..\..\test_8catcheurs.log" --log_flush_interval 1
