@echo off
rem Simule une sortie son indisponible au demarrage (pilote SDL inexistant) : sans correctif le jeu
rem plantait (crash Discord 2026-10-20) ; avec, il doit continuer sans son. Journal : sans_son.log.
set SDL_AUDIO_DRIVER=pilote_inexistant
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe %* --log_file "%~dp0..\..\sans_son.log" --log_flush_interval 1
