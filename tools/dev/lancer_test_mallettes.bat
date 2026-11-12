@echo off
rem Univers : deux mallettes Money in the Bank (wwe12_universe_mitb_briefcases = 2).
rem Donnees isolees (Documents\wwe12_test) : une COPIE de la vraie sauvegarde y est faite
rem au premier lancement, la vraie nest jamais touchee.
rem Journal dans mallettes_test.log (lignes « [univers] Money in the Bank »).
set "TEST=%USERPROFILE%\Documents\wwe12_test"
if not exist "%TEST%\B13EBABEBABEBABE" (
  xcopy /e /i /q "%USERPROFILE%\Documents\wwe12\B13EBABEBABEBABE" "%TEST%\B13EBABEBABEBABE" >nul
  if exist "%USERPROFILE%\Documents\wwe12\achievements" xcopy /e /i /q "%USERPROFILE%\Documents\wwe12\achievements" "%TEST%\achievements" >nul
)
if "%WWE12_BUILD%"=="" set WWE12_BUILD=win-amd64-release
cd /d "%~dp0..\..\port\out\build\%WWE12_BUILD%"
start "" wwe12.exe --user_data_root "%TEST%" --wwe12_universe_mitb_briefcases=2 %* --log_file "%~dp0..\..\mallettes_test.log" --log_flush_interval 1
