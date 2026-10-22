@echo off
rem Test Special Referee (type 83, absent des menus de WWE '12) : le type choisi dans les menus
rem (par defaut 13 = TRIPLE THREAT) est remplace par 83. Journal : diagnostic_arbitre.log,
rem lignes "[kind]" et "[role]". Sonde compilee avec WWE12_DEV_PROBES=1 a build.ps1, ou WWE12_KIND_PROBE_LOCAL a 1 dans wwe12_kind_probe.cpp. Autre type de depart : lancer_test_arbitre.bat 0  (0 = ONE ON ONE).
set FROM=%1
if "%FROM%"=="" set FROM=13
set BIN=%~dp0..\..\port\out\build\win-amd64-release
cd /d "%BIN%"
start "" "%BIN%\wwe12.exe" --wwe12_kind_from %FROM% --wwe12_kind_to 83 --svr_render_scale 720 --log_file "%~dp0..\..\diagnostic_arbitre.log" --log_flush_interval 1
exit 0
