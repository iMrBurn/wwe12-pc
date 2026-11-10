@echo off
rem Chargements en "turbo" (wwe12_fast_loading) + mesure ([loadprobe], [turbo]) dans journal_chargement.log.
call "%~dp0lancer_mesure_chargement.bat" --wwe12_fast_loading=true
