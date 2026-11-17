@echo off
rem Controle des formats d'ecran (Rendu, 2026-11-17) : fenetre 16:10 ou 21:9 sur Vulkan / D3D12 / D3D11.
rem Usage : lancer_test_format.bat <exe> <largeur> <hauteur>
rem   ex. lancer_test_format.bat wwe12.exe 1280 800      (16:10)
rem       lancer_test_format.bat wwe12_d3d12.exe 1680 720 (21:9)
rem Donnees de test : Documents\wwe12_test. Journal : test_format.log a la racine du depot.
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" %1 --user_data_root "%USERPROFILE%\Documents\wwe12_test" --fullscreen=false --window_width=%2 --window_height=%3 --log_file "%~dp0..\..\test_format.log" --log_flush_interval 1
