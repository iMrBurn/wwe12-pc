@echo off
rem Diagnostic : journal tres detaille (appels systeme) dans trace.log. A utiliser peu de temps.
cd /d "%~dp0..\..\port\out\build\win-amd64-relwithdebinfo"
start "" wwe12.exe --log_file "%~dp0..\..\trace.log" --log_flush_interval 1 --log_max_file_size_mb 2000 --log_level debug --log_high_frequency_kernel_calls true
