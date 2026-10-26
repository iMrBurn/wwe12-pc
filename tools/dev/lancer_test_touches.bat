@echo off
rem Issue #18 : Shift et Ctrl seuls sur un bouton. Clavier actif, A = Shift, B = Ctrl.
rem Donnees isolees (Documents\wwe12_test) : la vraie sauvegarde nest pas touchee.
rem Journal dans touches.log.
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --mnk_mode=true --keybind_a=Shift --keybind_b=Ctrl %* --log_file "%~dp0..\..\touches.log" --log_flush_interval 1
