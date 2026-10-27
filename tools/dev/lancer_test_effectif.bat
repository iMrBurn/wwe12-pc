@echo off
rem Univers : emissions de moins de 15 catcheurs (wwe12_universe_min_roster, ici 8).
rem Donnees isolees (Documents\wwe12_test_effectif) : une COPIE de la vraie sauvegarde y est faite
rem au premier lancement, la vraie nest jamais touchee. Supprimer ce dossier pour repartir de zero.
rem Journal dans effectif_test.log (lignes « [univers] roster »).
set "TEST=%USERPROFILE%\Documents\wwe12_test_effectif"
if not exist "%TEST%\B13EBABEBABEBABE" (
  xcopy /e /i /q "%USERPROFILE%\Documents\wwe12\B13EBABEBABEBABE" "%TEST%\B13EBABEBABEBABE" >nul
  if exist "%USERPROFILE%\Documents\wwe12\achievements" xcopy /e /i /q "%USERPROFILE%\Documents\wwe12\achievements" "%TEST%\achievements" >nul
)
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%TEST%" --wwe12_universe_min_roster=8 %* --log_file "%~dp0..\..\effectif_test.log" --log_flush_interval 1
