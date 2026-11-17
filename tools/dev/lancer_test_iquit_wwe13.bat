@echo off
rem I Quit comme WWE 13 (2026-11-15) : les 17 animations de WWE 13 (mod mods\.svrmods\Motions\iquit_wwe13_all,
rem 132NN -> 131(NN+50), faites depuis le WWE 13 du joueur) et ses images (dossier hud du meme mod).
rem Deroule : B pres de l adversaire au sol -> l attaquant appelle l arbitre, l arbitre vient poser la question,
rem la jauge ; NO ou I QUIT ; la croix directionnelle = la menace.
rem A regarder aussi : un match NORMAL, provocation au releve (l adversaire doit se relever comme d habitude).
rem Journal : test_iquit_wwe13.log (lignes [motion] et [iquit]).
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_iquit=true --wwe12_iquit_wwe13_motions=true --wwe12_motion_trace=true --wwe12_match_probe=true %* --log_file "%~dp0..\..\test_iquit_wwe13.log" --log_flush_interval 1
