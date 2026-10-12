@echo off
rem Test de la piste VBlank (Banshee) pour la ceinture a 60 fps. vblank_precise=true.
rem Lance une entree (ex. Orton) et regarde la ceinture sur l'epaule. Le journal donne les lignes [vblank].
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --log_file "%~dp0..\..\diagnostic_vblank_precis.log" --log_flush_interval 1 --log_max_file_size_mb 200 --wwe12_60fps=true --vblank_precise=true
