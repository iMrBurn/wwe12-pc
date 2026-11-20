@echo off
rem Test (Rendu, 2026-11-20) : la fonction des demi-flottants de la deformation des catcheurs (sub_8282D920) en code PC,
rem comparee a celle du jeu. Le test de performance integre (une passe : menus -> 1 contre 1 avec entrees -> 30 s de
rem match, ~3 min, le jeu se ferme seul).
rem Usage : lancer_test_native_vmx.bat <mode> [petit]
rem   mode = check : les deux versions a chaque appel, comparees ; ligne "[nativevmx] ... checked N, different D"
rem                  toutes les 10 s : D doit rester 0 sur tout le match (plus lent, ne pas lire les i/s).
rem          on    : la version PC seule (mesure de vitesse).
rem          off   : la version du jeu (reference de vitesse, meme parcours).
rem   petit = petit PC simule : le jeu sur 2 coeurs lents (affinite 0x30000) et l'iGPU Intel en 720p.
rem Chaque passe ecrit aussi "[perf] cpu per frame by thread (ms)" (temps CPU de chaque fil, sans biais).
rem Donnees : Documents\wwe12_benchmark (efface a chaque lancement). Journal : native_vmx_<mode>[_petit].log a la racine.
set MODE=%1
if "%MODE%"=="" set MODE=check
set OPT=
if /i "%MODE%"=="check" set OPT=--wwe12_native_vmx_check=true
if /i "%MODE%"=="on" set OPT=--wwe12_native_vmx=true
if /i "%MODE%"=="off" set OPT=--wwe12_native_vmx=false
set NOM=%MODE%
set AFF=
set GPU=
rem (pas de bloc entre parentheses : le "(R)" du nom de la carte Intel le casserait)
if /i "%2"=="petit" set NOM=%MODE%_petit
if /i "%2"=="petit" set AFF=/affinity 30000
if /i "%2"=="petit" set GPU=--svr_gpu_device "Intel(R) RaptorLake-S Mobile Graphics Controller" --svr_render_scale 1
set BIN=%~dp0..\..\port\out\build\win-amd64-release
set ROOT=%USERPROFILE%\Documents\wwe12_benchmark
if exist "%ROOT%" rmdir /s /q "%ROOT%"
cd /d "%BIN%"
start "" %AFF% "%BIN%\wwe12.exe" --user_data_root "%ROOT%" --wwe12_benchmark=true %OPT% %GPU% --log_file "%~dp0..\..\native_vmx_%NOM%.log" --log_flush_interval 1
exit 0
