@echo off
rem Contre-epreuve "petit PC" (1.3) : jeu limite a 4 fils (wwe12_dev_affinity F, pose 3 s apres le demarrage :
rem "start /affinity" ne marche pas), rendu sur la puce Intel, copie de la sauvegarde (Documents\wwe12_test).
rem Chargements rapides par defaut ; journal test_4fils_igpu.log (lignes [loadtime], [turbo], "[dev] processor affinity").
rem Arguments en plus (ex. --wwe12_native_mem=true --wwe12_native_mem_check=true) passes au jeu.
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --log_file "%~dp0..\..\test_4fils_igpu.log" --log_flush_interval 1 --log_max_file_size_mb 200 --svr_gpu_device "Intel(R) RaptorLake-S Mobile Graphics Controller" --svr_render_scale 1 --wwe12_dev_affinity F %*
