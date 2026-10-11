@echo off
rem Test : rendu interne en 720p (resolution d'origine Xbox) au lieu de 1440p.
rem Sert a verifier si les fines "fissures" sur la peau des catcheurs viennent de la montee en resolution.
cd /d "%~dp0port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --log_file "%~dp0diagnostic.log" --log_flush_interval 1 --log_max_file_size_mb 200 --svr_render_scale 1
