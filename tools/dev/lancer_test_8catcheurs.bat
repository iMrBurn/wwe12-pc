@echo off
rem 8 catcheurs, palier 0 (2.0, session Gameplay, docs/etude_plus_de_6_catcheurs.md) : les deux personnages des
rem cases 7-8 deviennent des catcheurs (les deux premiers de wwe12_dev_eight_ids qui ne sont pas deja dans le match).
rem A FAIRE : le match a 6 (6-MAN TAG ou ELIMINATION CHAMBER) doit etre le PREMIER match apres le lancement
rem (le jeu cree les cases 7-8 une seule fois). Jouer 2 minutes, puis quitter.
rem Les tas 37 et 38 (cases 7-8, 492 Ko) sont agrandis de 660 Ko, a la taille des cases 0-5 (1152 Ko) ;
rem les tas 12 / 15 ne sont plus agrandis (20 Mo inutiles ici).
rem A lire dans test_8catcheurs.log : [eight] (cases, roles, positions, tas), [perf] (images), [heaps] (manques).
rem Essai 3 : role remis a 0 toutes les 100 ms ([eight] force) ; [compte] adresse surveillee = le code du jeu
rem qui remet la fiche de la case 7 au role 4 (wwe12.exe+X, a traduire avec wwe12.map).
rem Donnees de test : Documents\wwe12_test.
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_dev_eight_wrestlers=true --wwe12_heap_grow="37:660,38:660" --wwe12_heap_grow_from=0 --wwe12_heap_watermark=true --wwe12_pin_trap=true %* --log_file "%~dp0..\..\test_8catcheurs.log" --log_flush_interval 1
