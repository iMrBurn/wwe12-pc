@echo off
rem Test du plantage de la Beast Bomb (Batista DLC, prise contre les cordes).
rem Donnees isolees (Documents\wwe12_test) : la vraie sauvegarde n est pas touchee.
rem A jouer : match avec Batista, adversaire dans les cordes, sa prise contre les cordes.
rem Journal dans beast_bomb_test.log (lignes "[move_events]").
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" %* --log_file "%~dp0..\..\beast_bomb_test.log" --log_flush_interval 1
