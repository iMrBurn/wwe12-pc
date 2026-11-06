# Anciens plans et feuilles de route sortis de CLAUDE.md (2026-11-06)

Historique des versions 0.8 / 0.9 / 1.0 et des listes d'idées de joueurs d'octobre. La feuille de route actuelle est
`docs/feuille_de_route.md` ; les idées de joueurs sont dans `docs/suggestions_joueurs.md`.

## Version 0.8 : plan (décidé par l'utilisateur le 2026-10-17)

Chaque conversation coche sa ligne ici quand c'est fait et testé, puis prévient la Main. La Main
compile, empaquette et publie (avec le « go » de l'utilisateur). Avant de compiler ou de lancer le jeu :
vérifier qu'aucune autre compilation ne tourne (`tasklist | findstr /i "ninja clang wwe12"`).

| # | Tâche | Qui | État |
|---|---|---|---|
| 1 | Gels avec moins de 6 fils (#4, #11, Nothing) : reproduits avec l'affinité 0xF, à corriger | Rendu | à faire |
| 2 | ~~Dossier `update`~~ : retiré (2026-10-17) ; la TU reste la 1.1 (voir « Versions du disque ») | — | annulé |
| 3 | Turbo des chargements remis par défaut, une fois l'interblocage avec 4 fils compris | Gameplay + Rendu | à faire |
| 4a | Options : compteur FPS, fenêtré / plein écran / sans bordure, limite d'images, priorité CPU | Launcher | **fait et testé** (2026-10-17) : onglets AFFICHAGE / GRAPHISMES / AVANCÉ dans le launcher ; `wwe12_display.cpp` (F2, `wwe12_fps_limit`, `wwe12_cpu_priority`) ; patch SDK `rexglue-sdk-borderless.patch` (cvar `borderless`). La limite ne propose que Aucune / 60 : le jeu est cadencé à 60 par sa propre horloge (au-dessus rien ne change, en dessous il ralentit). Reste : les options de 4b dans l'onglet GRAPHISMES quand Rendu les aura exposées. |
| 4b | Options graphiques : foule ON/OFF, ombres, profondeur de champ, flou, effets, anticrénelage (réglages exposés en cvars ; Launcher les ajoute ensuite à l'écran) | Rendu | à faire |
| 5 | Icônes de boutons PlayStation / Xbox selon la manette (vraies icônes PS3, décision de l'utilisateur) | Gameplay | **fait** (2026-10-17, vérifié en jeu) |
| R | Rendu : pavés du quart droit (#7, #11, #13 ; vidage des nouvelles surfaces à faire tester), yeux blancs AMD (#12) | Rendu | en cours |

## Version 0.9 : plan (envoyé à toutes les conversations le 2026-10-18)

Même règles que la 0.8 : chacun coche sa ligne quand c'est fait et testé, prévient la Main ; une seule
compilation à la fois ; rien n'est poussé sauf par la Main avec le « go » de l'utilisateur.

| # | Tâche | Qui | État |
|---|---|---|---|
| 1 | **Musiques perso** (vedette de la 0.9, très demandée) : choisir une musique (mp3/ogg/wav…) pour les catcheurs créés, entrées comprises | Gameplay (+ Launcher pour l'écran de choix) | **fait** (2026-10-19 : mp3/m4a/aac/wma/wav/flac du dossier `music` à côté de l'exe, choisis dans le jeu via « créer une entrée » → lecture utilisateur ; entrée → charge de match → replay → cérémonie de victoire validés en continu ; patchs `rexglue-sdk-xmp-music.patch` et `rexglue-sdk-audio-flow.patch` ; le 2026-10-19 l'ancien `rexglue-sdk-music-perso.patch` a été remplacé : il n'avait pas `xam_enum.cpp` (la liste USER PLAYLIST, d'où « musique absente » sur un SDK refait depuis les patchs) et recopiait le morceau `xmp_app.cpp` de reblue (échec de `setup.ps1`) ; le nouveau est fait par-dessus tous les autres patchs, trié après eux, vérifié sur un SDK propre. ogg non lu pour l'instant (Media Foundation) : à revoir côté launcher) |
| 2 | Create a Story : taper le texte des cinématiques au clavier (aussi noms de CAW, d'arènes…) | Gameplay | **fait** (2026-10-19, vérifié en jeu au clavier seul : écrire, Retour arrière, modifier un texte existant, texte gardé après validation ; commit f7e3590 ; `wwe12_kbd.cpp`, cvar `wwe12_kbd` activé par défaut ; nom de CAW pas encore vérifié à part ; voir « Savoir acquis ») |
| 3 | Turbo des chargements à 4 fils (interblocage), puis activé par défaut | Gameplay + Rendu | **testé** (2026-10-20 : 7 chargements de match turbo d'affilée sur 4 fils (affinité F), aucun blocage, sans doute grâce à `wwe12_gpu_wait_yield` (contre-épreuve non faite) ; outil : télécommande `wwe12_remote_pad` + `tools/dev/lancer_test_turbo4.bat`). **Décision de l'utilisateur : turbo PAS activé par défaut** (reste une option) |
| 4 | Gels : analyser les hang_trace des joueurs en 0.8 (carte `dist/symbols-v0.8.0-beta`). Piste 2026-10-19 : **#11 gel isolé sur le modèle de Randy Savage** (trop de sommets ? le même modèle gelait le portage SVR 2011 de Kaiko) ; hang_trace demandé | Rendu | à faire |
| 5 | Pavés Intel/AMD (#7, #11, #13) et yeux blancs AMD (#12) : suivre les retours 0.8. 2026-10-19 : **#16 les pavés touchent aussi une RX 7600 moderne en match** → ce n'est pas un bug de vieux GPU ; test `svr_rt_no_compression` envoyé aux joueurs ; #7 : la caméra montre la foule = rien, catcheur = pavés | Rendu | à faire |
| 6 | Options graphiques restantes : foule (premiers rangs), ombres, profondeur de champ, anticrénelage | Rendu + Launcher | à faire |
| 7 | Univers : **remise à zéro de l'Univers** dans un menu en jeu, et **VACANT pour les titres qu'aucune émission n'utilise** (comme SvR 2011, demande d'un joueur ; un titre vacant utilisé par une émission n'aurait plus jamais de match, d'où cette limite). Stat tracking façon WWE '13 : plus tard, le jeu ne garde ni victoires ni historique, il faudra les noter nous-mêmes. | Univers | **fait** (2026-10-20 : reset MODIFIER L'UNIVERS / EDIT UNIVERSE + VACANT titre par titre via le vrai écran TITLE MANAGEMENT, refusé pour un titre d'émission ; `wwe12_universe_edit` = true par défaut ; Non = éditeur des Superstars ; Oui puis Oui = copie auto dans SaveBackups puis reset, enregistré à la prochaine sauvegarde du jeu ; vérifié en jeu EN et FR, DE/ES/IT pas vus ; doc docs/univers.md) |
| 8 | Formulaire de bug GitHub + FAQ | Main | **fait** (formulaire `.github/ISSUE_TEMPLATE/bug_report.yml` + `docs/FAQ.md`) |
| — | DLC manquants (cache) | Main | **fait** (2026-10-18) |
| 9 | Sang des catcheuses (cvar `wwe12_female_blood`) | Gameplay | **reporté à la 1.0** (décision de l'utilisateur, 2026-10-22) ; ce qu'on sait : « Savoir acquis » |
| 10 | Interrupteur unique `wwe12_original_game` (défaut false) qui coupe les ajouts du portage | Gameplay + Univers | **reporté à la 1.0** (décision de l'utilisateur, 2026-10-22 : il garde les nouveautés actives) ; écrit, non compilé, sauvé dans `work/original-game-1.0.patch` |


## Feuille de route 0.9 / 1.0 (décidée par l'utilisateur le 2026-10-18)

**0.9** : ~~DLC manquants~~ **réglé** (2026-10-18 : un joueur a retrouvé ses catcheurs en supprimant
`xbox_cache` et les DLC installés ; `wwe12_dlc.cpp` vide maintenant le cache à chaque nouvelle
installation de DLC, et une fois pour les installations existantes) ; gels (hang_trace de 0.8) ; pavés et yeux
AMD ; turbo à 4 fils ; options graphiques restantes ; formulaire de bug GitHub + FAQ. ~~Musiques perso~~
**fait** (2026-10-19 : dossier `music` + choix dans le jeu ; voir ligne 1 du plan 0.9).
**1.0** : plus de bug grave connu ; **Mod Maker** comme celui de Kaiko ; signature de l'exe à étudier ;
documentation.
**Nouvelles suggestions (2026-10-18)** :
- ~~Create a Story : taper le texte des cinématiques au clavier~~ **fait** (2026-10-19, ligne 2 du plan 0.9).
- Mode Univers comme WWE '13 : remise à zéro de l'Univers, et rendre une ceinture vacante (Univers).

## Après la 0.8 (2026-10-17) : priorité bugs et DLC

- **DLC (Gameplay)** : Batista absent chez certains joueurs (JokerOP : mêmes 4 fichiers que l'utilisateur,
  même ISO selon l'utilisateur). Réinstallation propre proposée (supprimer
  Documents\wwe12\0000000000000000\54510890\00000002), résultat attendu. Quoi qu'il en soit : 1) au
  démarrage, vérifier chaque DLC installé contre son paquet du dossier dlc (taille / nombre de fichiers)
  et le réinstaller s'il est incomplet ; 2) écrire dans le journal les catcheurs DLC chargés (ch*_dlc*).
- **Gels** : la 0.8 corrige le VK_ERROR_DEVICE_LOST (extents de surface réutilisée). Pour les autres, il faut
  des hang_trace.txt de 0.8 sans OBS ; `dist/symbols-v0.8.0-beta/wwe12.map` permet de les lire.
- **Formulaire de bug GitHub** (Main) : version, carte graphique, version du disque (journal), journal,
  hang_trace.txt / stderr.txt.

## Feuille de route (décidée avec l'utilisateur le 2026-10-16)

Retours : issues GitHub + salon « bugs » du Discord de Kaiko (l'utilisateur y a sa catégorie).
Note : en match à plus de 2, le nom affiché au-dessus d'un catcheur est celui de la cible : normal.

**1.0 = stabilité, tous les bugs connus** (dans cet ordre) :
0. ~~Fuite de mémoire vidéo~~ : **corrigée** (2026-10-16, pas encore publiée). Trois fuites dans
   `svr_resources.cpp` : cibles « périmées » jamais libérées (`RetireHost` les rend maintenant comme
   un Release du jeu : registre des surfaces, ou destruction après la fence), surfaces que le jeu
   abandonne à chaque scène (libérées après 30 s sans usage), copies de textures et cibles des
   matchs précédents (60 s / 2 min). Le plantage d'origine venait de `DestroyResourceNow`, qui lisait
   une poignée HostResourceHeap comme une adresse (corrigé dans `graveyard.cpp`). cvar
   `svr_release_stale_targets` (false = comportement 0.6.0). Mesuré en 1440p, 3 matchs Univers :
   ~390 Mo stables par match au lieu de +120 à +250 Mo par match. La ligne `[perf]` donne la
   mémoire vidéo (`vram X / Y MB`), « SvR native: … presents/s » le nombre de surfaces, cibles et
   textures en vie. Probablement liés : image figée / ralenti / 0,25x après quelques matchs (Discord
   bugs 1, 4, 6, 7), #10, #8, cel-shading Steam Deck : à confirmer par les joueurs.
   Remarqué (pas gênant) : micro-écran noir < 0,1 s sur la moitié droite juste avant la cloche.
1. **RÉGRESSION 0.7.0 (2026-10-16)** : #11 (JimminyBuccilli, Intel HD 520, 2 cœurs) gèle maintenant en
   entrée ET en match, ce qui n'arrivait pas en 0.6.0 (pareil dans les vieilles versions du SvR 2011
   de Kaiko : lui demander ce que c'était). Journaux `work/issue11/` : dans wwe12_013.log, juste avant le
   gel, « 1 stale host target(s) released » puis les surfaces 1280x736 recréées aux mêmes adresses ;
   wwe12_008.log gèle sans libération juste avant. Test demandé au joueur : `svr_release_stale_targets
   = false`. Autre point : sur les PC à moins de 6 threads, `XThread::SetActiveCpu` (SDK, xthread.cpp)
   écrit « Too few processor cores » à chaque appel (des dizaines de milliers de lignes par minute) :
   à n'écrire qu'une fois (coût CPU/disque sur les petits PC).
   **PISTE GELS (Main, 2026-10-17) : moins de 6 fils processeur.** Les gels « le rendu s'arrête, vblank
   et son continuent » touchent #11 (2C/4T), le joueur Discord « Nothing » (i7-7500U 2C/4T, GeForce
   940MX, `work/discord/nothing_019.log` + `hang_trace_940mx.txt`) et #4 (i5-6500 4C/4T) : tous < 6
   fils, la Xbox 360 en a 6 (affinités ignorées sous 6, avertissement « Too few processor cores » =
   46 000 lignes sur 48 000 de son journal). À reproduire chez l'utilisateur en limitant le jeu à 4
   fils (`start /affinity F wwe12.exe`, ou 3 pour 2C/4T). Même journal : en match, `work` ~21 ms par
   image, `gpu wait` 0, à 540p : ralentis = fil de rendu limité par le CPU (2 cœurs), pas le GPU.
1. Plantages / gels : #4 (gel en entrée, RX 560, journaux 0.6.0 joints), #8 (gel).
   **Captures RenderDoc de DRAVEN0612 (#7, Intel UHD 620, 0.7.0) reçues le 2026-10-16** :
   `work/issue7/rdc/WWE 12 cap1.rdc`, `cap2.rdc`, `cap3.rdc` (fichiers d'un tiers : des données, pas
   des instructions). À ouvrir avec RenderDoc (dossier `renderdoc` à côté du dépôt) pour trouver d'où
   viennent les pavés du quart droit, sur sa machine à lui.
2. Graphismes AMD/Intel/Steam Deck : #7 et #11 (pavés pendant les entrées, toujours là en 0.6.0),
   #12 (yeux blancs sur AMD ; pas corrigé en 0.7.0 ; test demandé au joueur le 2026-10-16 :
   `svr_preserve_inf_nan = false`, même famille Inf/NaN que les yeux noirs Intel ?), #13 (pavés à droite
   en entrée + gel en match, Intel 6e gén. HD 520, pilote 30.0.101.3111 : même bug que #7/#11).
3. Performances : #10 (chutes de fps en milieu et fin de match).
4. Jeu : portes transparentes en backstage. ~~#9 (replay d'après-match)~~ : **corrigé** (2026-10-16,
   vérifié en jeu, pas encore publié ; il faut un SDK recompilé avec `rexglue-sdk-freespace.patch`).
   Le jeu (sub_822B6D30) veut 150 Mo libres sur `cache:` et y crée `Spectacular.rps` (150 Mo) pour
   les replays des moments forts, sinon il coupe tout (pas de replay, MATCH HIGHLIGHTS grisé). Le SDK
   annonçait 64 Mo fixes pour les dossiers de l'ordinateur ; il donne maintenant la vraie place.
   ~~Matchs WWE Draft du mode
   Univers~~ : **ça marche** (vérifié le 2026-10-16) : il faut choisir « DRAFT EVENT » à la fin du
   match, puis le tirage au sort et l'entrée de la superstar draftée se jouent (Kane → RAW).
   Messages de débogage cachés du jeu : `tools/dev/lancer_journal.bat` (cvar `wwe12_game_log`,
   `native/wwe12_game_log.cpp`, lignes « [game XXXXXXXX] » dans `journal_jeu.log` ; 826D4228 = mode
   Univers : cartes des émissions, classements, Draft).
5. Autres retours Discord (2026-10-16) : écran noir sur Intel HD 5500 (**message clair ajouté** 2026-10-16, `native/wwe12_vulkan_error.cpp` ; `work/discord/bug02.log` :
   « Plume CreateVulkanInterface failed », ce GPU Broadwell n'a pas de pilote Vulkan sous Windows ;
   au minimum un message clair au lieu d'un écran noir figé, idéalement le rendu D3D12) ; tenue
   Superstar Threads avec des zones blanches (Daniel Bryan). ~~Vibrations de manette absentes en mode
   Univers~~ (déjà le cas sur Xbox 360) : **corrigé** (2026-10-16, vérifié en jeu, pas encore publié) :
   `native/wwe12_rumble.cpp`, cvar `wwe12_universe_rumble` (défaut true, à ajouter aux readmes).
   Le catcheur du joueur a son indicateur « vibration » (catcheur+3452) à 0 en Univers
   (sub_82761630), à 1 en exhibition ; le crochet de sub_824729A8 le prête à 1 le temps de l'appel.
6. Option « tout débloquer ». ~~Gestion des sauvegardes~~ : **faite** (page SAUVEGARDES du launcher ;
   import Xbox 360 testé en jeu par l'utilisateur avec ses propres sauvegardes le 2026-10-16 : OK).

**Chargements, suite (prévu pour la 0.8, avec la ceinture à 60 fps)** : recette de l'auteur du recomp de WWE 2K14 (même famille Yuke's,
résumé du 2026-10-16). Son chargement d'avant-entrée : 5,5 s → 0,9 s. Dans l'ordre de ses gains :
(1) le chargeur fait une étape puis attend la prochaine image (chaque fichier = 4 images) : enchaîner
les étapes dans la même image (chez nous : `wwe12_loading.cpp`, déjà x2) ; (2) **« turbo » pendant les
écrans de chargement seulement** : faire tourner l'horloge vblank du jeu très vite (vsync coupée,
~400 images/s) car le jeu fait ~70 images de préparation cadencées par l'image ; le plus dur = début
et fin exacts de la fenêtre (sinon spinner trop rapide, début de match accéléré) : brider les
animations 2D à 1/60 s réel, finir au premier vrai mouvement des catcheurs ; (3) profiler nos propres
crochets (chez lui 8 VirtualQuery par image = 1,4 ms) ; (4) vblank sur minuteur précis ; (5) zlib natif
à la place du zlib 1.2.3 traduit du jeu, seulement quand l'en-tête est standard (231 → 72 ms), idem
BPE de Yuke's (`tools/yukes_bpe.py`) et LZX. Sans effet chez lui : sauter le dessin (perd un rendu
unique par chargement), vblank plus rapide avec vsync active. Attention : le turbo change la
synchronisation image/temps (replays, #9) : à couper là où c'est comparé. Mesurer avant chaque étape.
**Fait (2026-10-16, étapes 0 et 1, vérifié en jeu ; cvar `wwe12_load_turbo` repassée à **false** par
défaut le 2026-10-17 : chargement infini reproduit par Rendu, voir « Interblocage du turbo » ci-dessous)** : `native/wwe12_load_turbo.cpp`. Mesure : les threads du jeu avancent au rythme des images
(barrière sub_827DEA20 : prêt → attend le feu vert), 60 images de jeu par image affichée ; sur ~4 s
d'écran de chargement, le chargeur ne travaille que ~1 s. Écrans de chargement = 14 classes
(`/LOAD/IG%02d`, GN, CV…, vtable slot 13) ; méthodes communes **pas appelées à chaque image** :
slot 14 sub_826AF0F0 à l'ouverture (~10 images), slots 16/22/23 (sub_826A83B0, sub_826A6B90,
sub_826B3060) à la fermeture, quand l'entrée démarre. Turbo de l'ouverture à la fermeture (20 s max) :
cvar SDK `vsync` coupé par son nom (`rex::cvar::SetFlagByName`, vblank invité à 1 kHz) + une image sur
`wwe12_load_turbo_frames` (6) affichée (`present.cpp` demande `Wwe12LoadTurboSkipPresent`) ; ≈ 340
images de jeu/s. Résultat : chargement de match ≈ 4,0 → 2,8 s, fin des entrées → cloche ≈ 5,9 → 5,0 s
(un écran du même type s'ouvre aussi en fin d'entrées, avant `WWE12_Transition` : turbo 1,9 s, rien de
visible) ; replay et MATCH HIGHLIGHTS OK. L'interrupteur 0x8317B7BC vaut 1 en menus/chargement/match,
5 en entrées : inutilisable comme « match en cours ». Sonde : `wwe12_load_probe` (lignes `[loadprobe]`),
lanceurs `tools/dev/lancer_mesure_chargement.bat` / `lancer_test_turbo.bat`. Un plantage (corruption
du tas, 0xc0000374) au 1er essai ne s'est pas reproduit en 5 lancements : deux jeux tournaient en même
temps. Reste : étape 2 (fin des entrées → cloche : transition ~2 s puis marche des catcheurs ~2,5 s,
vrai jeu) et le gros chargement CPU (~1 s, 7 000 appels du chargeur).
**Arrêté là (décision de l'utilisateur, 2026-10-16).** Étape 2 écartée : fin des entrées → cloche =
transition `WWE12_Transition` ~2 s (animation visible, l'enregistreur du replay tourne déjà) + marche
des catcheurs ~2,5 s (vrai jeu) ; l'écran de fin d'entrées est déjà en turbo (~4,5 → 2 s). Lectures et
zlib mesurées (sonde `wwe12_load_probe` : sub_827EA330 = boucle de lecture par blocs de 256 Ko,
sub_827F57A8 = inflate de zlib) : 32 Mo lus en 12 ms, inflate ~25 ms par match, BPE invisible dans
les piles → zlib/BPE natifs inutiles. Chargement de match avec turbo ≈ 2,7 s : ~1 s de vrai travail du
chargeur, puis **une lecture d'1 s pour 750 Ko** juste après les voix du speaker
(`Ring_Announcer_JR_DLC_03/04`), puis le script d'entrée (`/GMEV`, `/GMST`) et l'entrée. Piste non
suivie : trouver ce fichier (son lu en flux ? attente du SDK ?) ; gain possible ~1 s (→ ~1,7 s).
**Interblocage du turbo (2026-10-17, reproduit par Rendu, pas encore compris)** : build actuelle,
turbo actif, jeu limité à 4 fils (affinité 0xF) : chargement infini ; le turbo s'arrête au bout des
20 s mais le jeu reste bloqué. Le fil principal et 3 autres attendent dans la barrière sub_827DEA20,
personne dans sub_827DE8A0/8F8. Analyse de la barrière (Gameplay) : objet = 24 fiches de 160 octets à
this+40 : +0 tid (8 o), +8 événement A « prêt », +12 drapeau, +16 événement B « feu vert », +20
drapeau, +24 actif. Un fil (sub_827DEA20) : trouve sa fiche (sub_827DE820, section critique), +12=1,
SetEvent(A), attend B sans limite, compteur +156. **Le coordinateur est le fil de rendu du jeu**
(sub_82826A38, seul appelant) : sub_827DE8A0 attend A de chaque fiche active, sub_827DE8F8 met +20=1
et SetEvent(B). Donc « personne dans le coordinateur » = le fil de rendu est bloqué ailleurs : **c'est
sa pile qu'il faut**. Pendant les chargements, il était dans PaceFrame / Video::Present
(D3DDevice_Swap_hook < sub_82AA8898 < sub_82880D68 < sub_82819828) ou dans sub_82AAAAF8 / sub_82AA7040
(attente côté D3D invité : vblank ou fin de swap). Bissection sans recompiler, avec 4 fils :
`--wwe12_load_turbo_vblank=false` (seulement le saut d'images) puis `--wwe12_load_turbo_frames=1`
(seulement le vblank à 1 kHz). Suspects : (a) le vsync SDK coupé : `command_processor.cpp` (~l. 1045)
passe de Sleep à MaybeYield (boucle active) et le fil vblank tourne à 1 kHz ; sur 4 fils, ils prennent
des cœurs au jeu, et un signal (interruption vblank, fence du GPU nul) peut être perdu ; le blocage qui
dure après les 20 s penche pour un signal perdu plutôt qu'une lenteur ; (b) le saut d'images :
AbandonFrame soumet puis attend le GPU hôte (AdvanceAndWaitReused) ; (c) le crochet de sub_827DEA20
ne fait qu'un comptage sous mutex si `wwe12_load_probe`, sinon il appelle directement l'original.

**Mode Univers enrichi (voulu par l'utilisateur, 2026-10-16)**, sur le modèle de « DarkMatches » (Discord,
recomp de WWE '13, même mode Univers) : matchs de qualification (Money in the Bank, Elimination Chamber)
avec leurs actualités (NEWS) et la place gagnée ; participants surprise au Royal Rumble (0 à 3 légendes
de la division Legends, affichés « ??? » avec le portrait vide dans la liste). Base de travail : journal
de débogage du jeu (`wwe12_game_log`, 826D4228 = mode Univers : cartes, classements, Draft). Commencer
par cartographier comment le jeu génère les cartes d'émission et les actualités. Demander à DarkMatches
comment il s'y prend (et sa licence avant de reprendre du code).
L'utilisateur veut aller plus loin : refonte du mode Univers (booking, rivalités/scénarios,
cinématiques, actualités), dans une conversation dédiée « Univers ». Plan : 0) cartographie sans rien
modifier → `docs/univers.md` (stockage des émissions/roster/titres/rivalités, fonctions de génération
des cartes, choix des cinématiques, écriture des NEWS ; ce qui est en données .pac vs dans le code) ;
1) booking (titres, PPV, qualifications, Rumble surprises) ; 2) rivalités ; 3) cinématiques ;
4) actualités. Idée : à terme, des règles de booking dans un fichier réglable par les joueurs.
La 1.0 (stabilité) passe avant ; la cartographie peut se faire en parallèle.

**Suggestions Discord (2026-10-16)**, décisions de l'utilisateur :
- PAS de préréglage « Original (Xbox 360) » (refusé).
- **Musiques perso : importantes à implémenter** (catcheurs créés ; à planifier tôt après la stabilité).
- **Options graphiques/launcher inspirées du SvR 2011 de Kaiko** (captures du 2026-10-16), pages
  DISPLAY / GRAPHICS / ADV. GRAPHICS dans le jeu + launcher. Déjà chez nous : résolution interne,
  vsync, 60 fps entrées, filtrage, carte graphique, saut des vidéos, préparation des pipelines
  (bd_pso_precache). Faciles : compteur FPS (F2), fenêtré / plein écran / sans bordure, priorité CPU,
  limite d'images. Moyennes (Rendu) : foule ON/OFF (sièges vides, gros gain sur petits PC), ombres,
  détail des effets, profondeur de champ, flou de mouvement, « soft filter », anti-aliasing (FXAA/SMAA
  en post-traitement), qualité des textures, écrans larges (titantron pleine largeur). Plus tard :
  choix de l'API (D3D12/Vulkan), replays ON/OFF.
- **Icônes de boutons PlayStation** : **faites** (2026-10-17, vérifiées en jeu sur l'écran CONTROLS).
  **Décision de l'utilisateur : on livre les vraies icônes de la version PS3** (comme Kaiko pour SvR
  2011), pas des icônes redessinées : exception assumée à la règle « jamais de données du jeu »
  (`port/res/ps3_buttons.bin`, forcé dans git malgré `*.bin`, intégré à l'exe). Le jeu dessine ses
  boutons par des caractères privés (U+E000 à U+E02A) pris dans une planche par police
  (`menuontHD.pac`/`fontSD.pac`, DDS DXT5 512x512 ou 256x512 dans des blocs BPE ; même disposition
  sur PS3 pour les cases de boutons, THQ/F/R ailleurs) et par ~24 petites images d'invites en match
  (`game2dHD/SD.pac`). `tools/extract_ps3_buttons.py <dossier Xbox> <USRDIR PS3>` construit le .bin
  (cases de boutons des planches, images d'invites entières, appariées par position dans le fichier ;
  SELECT/START/L2/R2, plus larges sur PS3, réduits à la largeur des cases Xbox). `native/wwe12_buttons.cpp`
  : crochet du chargeur DDS sub_82AC59C8(info, dds, taille), empreinte FNV-1a du niveau 0, remplacement
  en mémoire avant conversion ; cvar `wwe12_button_icons` = auto (manette Sony branchée, vue par le système d'entrée du SDK : `InputSystem::GetDevices`, patch `rexglue-sdk-pads-list.patch`, vendeur 054C dans le GUID SDL ou nom DualSense/DualShock/PS3-5 ; corrigé le 2026-10-19, vérifié avec une DualSense : l'exe a sa propre copie statique de SDL qui ne voit aucune manette) /
  xbox / playstation (à ajouter aux readmes). Polices chargées au démarrage, invites à chaque match :
  brancher la manette PS avant de lancer. Les tables de caractères des polices (rectangles par code,
  dans le bloc « métriques » après la planche : nombre, codes, puis 16 octets x/y/l/h par code) sont
  converties par le jeu au chargement : introuvables en mémoire ; pas de crochet BPE trouvé.
  Lanceur de test : `tools/dev/lancer_test_boutons_ps.bat`. Plus tard, si voulu : changement en direct.
- À voir : physique des tables/échelles, éclairage façon SvR 2011.
Plus tard : statistiques du mode Univers comme WWE '13 (conversation Univers, étape 4) ; 5 contre 5
(= plus de 6 catcheurs).

**Versions du disque (corrigé le 2026-10-17 par Gameplay).** Le `default.xex` du disque de l'utilisateur
(donc le code de notre exe) est en 0.0.0.4, base 0.0.0.4, media ID 712076A5, **sans Title Update** : la TU v4
(Desktop\49470807\000B0000\TU_1A5224G..., default.xexp) le fait passer de 0.0.0.4 à 0.0.2.4, et ses
données diffèrent (DLC.pac 94208 o contre 92160 o sur le disque, plist360, strMenu.pac et
_startup360.pac absents du disque). Une ancienne note disait à tort que le disque contenait déjà la TU.
Appliquer default.xexp au xex chargé (le SDK le fait seul s'il trouve game:\default.xexp, user_module.cpp ;
attention à un default.xexp posé dans mods\) mettrait une image 0.0.2.4 sous du code 0.0.0.4 :
incohérent. Vraie TU = 1.1 (xex patché hors ligne, nouveau codegen, recalage de toutes les adresses).
Batista absent chez JokerOP (mêmes 4 fichiers DLC, mêmes noms que l'utilisateur) : cause inconnue ;
`src/wwe12_disc.cpp` écrit la version du disque dans le journal (0.7.1) pour comparer.

**1.1** : Title Update (dernière mise à jour du jeu).
**Après** : choix du rendu (Vulkan / D3D12 ; plume gère les deux, pas D3D11), mods poussés, musiques
perso, textures HD (remplacement de textures par le dossier mods), minitrons ajoutés (contenu de mods),
vidéos HD, plus de 6 catcheurs sur le ring (8-10, suggéré par des joueurs ; limite gravée dans le
code : tableaux à 6 cases, budget mémoire Xbox, animations ; très long ; demander à Kaiko si son
« Limit Breaking » de SvR 2011 y est arrivé ; idées dans `docs/limit_breaking.md`), nouvelles stipulations et mode Univers enrichi (très difficile : logique du jeu), online.

## Idées et demandes des joueurs (à faire)

- **Option « tout débloquer »** (comme le portage SvR 2011) : un joueur signale que le DLC « All
  Unlockables » ne débloque que les persos des DLC, pas le reste. Piste : cvar `wwe12_unlock_all`
  (case dans le launcher) qui force les drapeaux de déblocage, sans toucher aux sauvegardes du joueur.
  Il faut d'abord trouver dans le code où le jeu teste si un élément est débloqué.
- ~~Yeux/dents noirs et cheveux de l'arbitre sur Intel iGPU~~ : **corrigé** (2026-10-12). Le pilote
  Intel supprimait les bornes ±FLT_MAX des shaders (Vulkan l'autorise si le module ne demande pas de
  garder Inf/NaN), d'où Inf × 0 = NaN dans la couche du visage. `guest_shaders.cpp` ajoute
  `SignedZeroInfNanPreserve` à chaque shader (cvar `svr_preserve_inf_nan`). À vérifier : l'issue #7 et le
  voile blanc en 540p pourraient avoir la même cause.

