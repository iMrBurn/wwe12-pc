@echo off
rem I Quit, animations de WWE '13 (2026-11-14, apres les deux corrections : fiche complete "ZLIB" + listes 0/8 de
rem m3.pac). Le mod de test mods\.svrmods\Motions\iquit_referee_test met 3 animations d arbitre de WWE '13 sous les
rem numeros 13120 / 13121 / 13122 ; wwe12_iquit_added_motions les fait connaitre au repertoire d animations du jeu.
rem L arbitre joue une animation differente a chaque demande, a tour de role : 13120 = copie exacte de la 13106 du
rem jeu (temoin), 13121 = WWE '13 13207 (un seul bloc), 13122 = WWE '13 13200 (12 blocs, deux personnes).
rem A regarder : 1) match I QUIT : B pres de l adversaire au sol -> l arbitre joue la nouvelle animation ;
rem 2) match NORMAL : provocation au releve (l adversaire doit se relever comme d habitude).
rem Journal : test_iquit_wwe13.log (lignes "[motion]" et "[iquit]").
cd /d "%~dp0..\..\port\out\build\win-amd64-release"
start "" wwe12.exe --user_data_root "%USERPROFILE%\Documents\wwe12_test" --wwe12_iquit=true --wwe12_iquit_ref_anim=13120 --wwe12_iquit_ref_anim_cycle=3 --wwe12_iquit_added_motions=true --wwe12_motion_trace=true --wwe12_match_probe=true %* --log_file "%~dp0..\..\test_iquit_wwe13.log" --log_flush_interval 1
