# Feuille de route (réorganisée le 2026-11-03, validée par l'utilisateur)

Les détails techniques sont dans `CLAUDE.md`, `docs/univers.md`, `docs/limit_breaking.md`, `docs/mod_maker.md`, `docs/camera.md` et `docs/suggestions_joueurs.md`. L'ancienne feuille de route (textes de l'utilisateur, notes, historique) est gardée en bas, sous « Détails et historique ».

## Règles du jeu des numéros
- **1.0.x** : corrections seulement, jamais de nouveauté. On part du tag précédent et on ne prend que les corrections choisies.
- **1.x** : un thème par version, 5 éléments au plus. Ce qui n'est pas testé en jeu passe à la version suivante.
- **2.0** : seulement si les sauvegardes, le format des mods ou la façon d'installer changent.
- Rien n'est promis aux joueurs tant que ce n'est pas testé en jeu. Une seule liste officielle (ce fichier) ; les issues GitHub n'y sont que des liens.
- Rien de dérivé du jeu ne se publie (pas de données de jeu dans un zip ni sur GitHub).

> **Décision de l'utilisateur, 2026-11-04 : pas de 1.0.1, on sort directement une 1.1** qui contient les corrections ci-dessous et le travail déjà testé des autres sessions. Une 1.0.1 ne sortira que pour un bug grave (crash au lancement, sauvegarde cassée, mise à jour qui échoue). Règle de la 1.1 : on n'y met que ce qui est **testé en jeu** ; ce qui n'est pas prêt passe à la 1.2. Les tableaux « 1.0.1 » plus bas sont la liste des corrections de la 1.1. Pour Android : versionCode 10100.

## 1. Maintenant : corrections (ex-1.0.1, intégrées à la 1.1)
Voir le tableau de `docs/feuille_de_route.md` (section « 1.0.1 »).
- Univers : plantage avec 5 matchs par show (fait, testé, poussé).
- D3D12 sur Intel UHD 620 (#7) : fait, à confirmer par le joueur.
- Bugs de la 1.0 signalés d'ici là.
- Date : dans 3 à 7 jours, ou tout de suite si un bug empêche de jouer.

## 2. Prochaines versions (un thème chacune)

### 1.1 : « Mod Maker et entrées » (liste de l'utilisateur, 2026-11-04 : tout son écran est pour la 1.1, une seule version, sans date)
Déjà fait et coché par l'utilisateur :
- Univers : plantage avec 5 matchs par show (poussé).
- D3D12 sur Intel UHD 620 (#7).
- Tenues Threads blanches + peau GT 430 (corrections de Kaiko, crédit).
- Mod Maker : gel arènes + catcheurs ajoutés, mémoire pour 50 catcheurs, pages d'arènes.
- Mod Maker : prises, entrées et musiques SvR 2007-2011 / WWE '13 (prises SvR 2011 = expérimental).
À finir / tester :
- Android : vieux noyaux + journal GPU (fait en local, à confirmer sur un vrai noyau 4.14 : joueur de #41).
- Mod Maker : .pac en 2e tenue, visages de loin, tenues d'entrée 2K16, Fatal 4 Way (faits en local, à tester en jeu).
- Ceinture à la taille (fait), tenues de The Miz (abandonné), CAW dans l'effectif Univers (en attente d'un signalement).
- Mesure CPU par image (Windows) (fait, ff941e5).
Aussi dans la 1.1 (liste de l'utilisateur) :
- Arènes SvR 2007 : foule au bon endroit (chantier de Mods).
- Scènes de victoire (Winning*.pac), musique de victoire, vidéos WWE '13.
- Tenue des catcheurs ajoutés (ceinture à la taille, etc.).
- À tester avant de promettre : prises de SvR 2011 / '13 / 2K14-2K16 (marquées expérimentales).
- Changer les renders des CAW (à préciser avec l'utilisateur : portraits des CAW dans les menus ?).
Note : les notes publiques de la 1.0 annoncent « squash, finishers de départ, I Quit, Special Referee, scènes de victoire, vidéos WWE '13, deux mallettes MITB, icônes de touches » pour la 1.1 : la 1.1 ne contient que la partie « Mod Maker et entrées » ; les notes de la 1.1 doivent dire ce qui est repoussé.

### 1.2 : « Les corrections de THQ » (à découvrir)
- Comparaison des 2 mises à jour officielles (en cours, session Gameplay) : liste des corrections, tri, test en jeu.
- Bugs du jeu d'origine à tester : finishers qui gèlent, cage / barrières de Hell in a Cell, tables sous le ring, sons après Backstage Brawl, movesets CAW qui se réinitialisent, évasion de tombé en Elimination Chamber.
- Tenues de The Miz verrouillées (signalé par un joueur : DLC).
- Chargements plus rapides (autres attentes de la console), avertissement HDD dans le launcher.

### 1.3 : « Modes de match »
- Stipulations : I Quit (la version légère d'abord), Special Guest Referee.
- Matchs squash (vie de départ réduite).
- Stock de finishers de départ (1 / 2 / 3 / infini, comme 2K14).
- Univers : deux mallettes Money in the Bank (RAW / SmackDown), embuscade en coulisses.

### 1.4 : « Écrans et plateformes » (à décider selon les retours)
- 16:10 / 21:9 : vérifier D3D11/D3D12, menu pause, HUD en 21:9.
- Icônes de touches de clavier dans les menus.
- Android : vrais téléphones, boutons PlayStation, ce que disent les joueurs.
- Shaders qui font saccader à la première fois (Rendu).

## 3. Plus tard (sans date)
- Plus de 6 superstars sur le ring (tableaux à 6 cases, mémoire, animations : très long).
- 100 / 200 slots CAW et logos (touche les sauvegardes : risqué, probablement 2.0).
- Le jeu en full natif, sans émulation (long terme, par étapes).
- Univers : backport des cinématiques WWE '13 / 2K14, Create a Story, tournoi pour titres vacants.
- Choix de caméra de match, option 30 fps partout.

## 4. Abandonné / pas prévu
- Moments OMG de WWE '13 (15 à 30 séances, décision du 2026-10-28).
- Mise à jour complète du jeu (Title Update) comme version du code : remplacée par « refaire nous-mêmes les corrections » (1.2).
- GM Mode (n'existe pas dans WWE '12), le DLC dans les textes publics, support des sauvegardes Wii.

## Comment suivre l'avancement (proposition)
- Chaque élément a un état : **idée → étudié → fait (commit local) → testé en jeu → publié**.
- À chaque version : on décide les éléments au début, on les teste en jeu au milieu, on publie à la fin ; ce qui n'est pas testé sort de la version.
- Une ligne de notes de version écrite au fur et à mesure (pas à la fin).

---

# Détails et historique (ancienne feuille de route)

## Feuille de route (version de l'utilisateur, mise à jour le 2026-10-27)

Liste écrite par l'utilisateur (iMrBurn), rangée par la conversation Main. Les détails techniques sont dans
`CLAUDE.md`, `docs/univers.md`, `docs/limit_breaking.md`, `docs/mod_maker.md`, `docs/camera.md` et
`docs/suggestions_joueurs.md`. Règle : on ne livre que ce qui est testé en jeu ; ce qui n'est pas prêt passe à la version suivante.

## Roadmap 1.0 (texte de l'utilisateur)

**Modifs de jeu** (liste restante, 2026-10-28)
- ~~Ajouter les moments OMG de WWE '13~~ : **abandonné (décision de l'utilisateur, 2026-10-28)**. Étude dans `docs/omg_finishers.md` : les vrais OMG coûtent 15 à 30 séances, le ring qui s'effondre plus de 30 ; pas prévu.
- ~~Choix des finishers comme 2K14~~ (stock de finishers de départ 1 / 2 / 3 / infini) : **passé en 1.1 (décision de l'utilisateur, 2026-10-28)**. Étude dans `docs/omg_finishers.md` §2 : la jauge de momentum est le levier, l'« infini » existe dans le moteur ; 2-3 séances + 1 pour une ligne d'options, 3-5 de plus pour un choix par catcheur à l'écran de sélection.
- Squash matchs : **passé en 1.1 (décision de l'utilisateur, 2026-10-31)** (avant : « plus tard dans la 1.0 », 2026-10-28). Étude dans `docs/squash.md` : le levier est STARTING HEALTH de Create a Story (vie par zone au départ), option « prochain match : squash » ; 2 à 4 séances ; il faut une sonde en jeu (go de l'utilisateur). Blessures qui durent dans l'Univers et attaque après le tombé = 1.1.

Fait depuis la liste précédente : défenses de titre en weekly et qualifications MITB / Elimination Chamber (Univers, testé) ;
Match Creator étendu (testé, activé par défaut) ; résolutions 360p / 480p (testées) ; format d'écran AUTO / ÉTIRER
(le vrai 16:10 : **remis dans la 1.0 par l'utilisateur, 2026-10-31**, voir ci-dessous) ; DX11 (testé sur RTX 11_1 / 11_0 et Intel intégré, reste la décision sur
la taille du zip) ; préréglage PC modeste ; scan de textes allégé. Les icônes de touches de clavier dans les menus
ne sont pas faites (le clavier lui-même marche, Shift / Ctrl / Alt aussi).
Aussi demandé : travailler sur le Mod Maker (voir `docs/mod_maker.md`).
- **Support 16:10 (décision de l'utilisateur, 2026-10-31)** : à étudier par Rendu d'après le port SvR 2011 de Kaiko 
  (`work/kaiko-svr2011/port/src/native/native_renderer.cpp` l. 869-915 : cvar `native_widescreen`, constante de rapport de la 
  caméra patchée, cibles plein écran élargies/agrandies en hauteur `g_wide`/`g_tall`, HUD 2D au milieu en 16:9, menus restent 16:9). 
  Kaiko a déjà traité le 16:10 (« tall »).
  **Fait et testé en jeu le 2026-10-31** (fenêtres 1280x800 et ~21:9, Vulkan ; case dans le launcher RÉGLAGES > AFFICHAGE ; cvar `wwe12_widescreen`, défaut true) :
  entrées et matchs en plein cadre, HUD au milieu, menus 16:9. **Pas encore vérifiés** : menu pause, HUD en 21:9, cvar coupée, barres de vie, D3D11/D3D12,
  création de catcheur après un match (l'utilisateur : « sinon tickets »). Détails : `docs/format_16_10.md`.
- Correctif du gel d'entrée de Rey Mysterio (`wwe12_job_pool_fix`) : testé par l'utilisateur sur 4 fils (3 entrées complètes, aucun gel) ; commit local, à publier avec la 1.0.

**Ajouts 1.0 (décision de l'utilisateur, 2026-11-02)** : ~~support des fichiers `.bin` pour les sauvegardes~~ (le `.bin` signalé était une sauvegarde **Wii** chiffrée, autre jeu, non importable ; le launcher dit maintenant pourquoi un fichier est refusé, commit 4e1d7d5 de Launcher) ; bouton « Ajouter des DLC... » dans le launcher (fait, à tester) ; réduire les temps de chargement (piste trouvée le 2026-11-02 : pause de 500 ms après chaque lecture de plus de 256 Ko autour des entrées, `wwe12_read_throttle`, commit 71bcf03 ; chercher d'autres attentes de la console).

(Décision du 2026-10-27 : le jeu en full natif et les slots CAW / logos 100/200 passent en 1.1.)

## Roadmap 1.1 (texte de l'utilisateur)

**Stipulations à ajouter** : I Quit ; Special Guest Referee

**Modifs de jeu** : matchs squash (vie de départ réduite, `docs/squash.md`) ; stock de finishers de départ (1 / 2 / 3 / infini, comme 2K14) ; plus de 6 superstars en même temps sur le ring ; passer de 50 slots CAW à 100/200 ; passer de 20 logos custom à 100/200

**Gros chantier** : le jeu en full natif, sans aucune émulation

**Mode Univers**
- Backport des cutscenes de WWE '13 et 2K14
- Intégrer les cutscenes du mode Create a Story et les faire se déclencher comme les vraies cinématiques de l'Univers
- Tournoi pour couronner des titres vacants
- Avoir des titres utilisés vacants ?

## Déjà fait (retiré de la liste, ne plus le promettre comme à faire)
- Mod Maker, jukebox dans les menus, Shift/Ctrl/Alt seuls (#18), D3D12 en plus de Vulkan (2 exe, shaders pré-liés).
- Univers : émissions de moins de 15 catcheurs (`wwe12_universe_min_roster`), statistiques façon 2K14, historique des titres,
  réglages d'aspirant, calendrier modifiable, classements (testé sur une copie de la sauvegarde ; voir `docs/univers.md`).
- Titres vacants pour les titres sans émission.

## Notes de la Main (estimations, pas des mesures)
- **DX11** : Kaiko l'a fait pour SvR 2011 et nous a autorisés à adapter son travail avec crédit. Estimation Rendu : au moins 10 séances. Dans la liste 1.0 ; à ne livrer que s'il est testé.
- **100/200 slots CAW, 100/200 logos** : touche les sauvegardes et la mémoire, risqué.
- **Résolutions 360p/480p** : on a 540p ; plus bas est très flou, à tester. **16:10** : vérifier ce que fait le jeu avant de promettre.
- **Match Creator** : préciser quelles restrictions.
- **Défenses spontanées, qualifications Rumble/Chamber/MITB** : demander à Univers une estimation avant de promettre.
- **Moments OMG, plus de 6 superstars** : gros chantiers (animations, tableaux à 6 cases, mémoire).
- **Full natif** : long terme, par étapes.

## Idées de joueurs, pas dans la liste (étudiées le 2026-10-27, après la 1.0 si jamais)
- Choix de caméra de match comme WWE 2K25/2K26 (`docs/camera.md` : le jeu n'a que CAMERA CUTS ; réglages simples 2-4 séances, vraies caméras 8-15).
- Option 30 fps partout : le gain vient surtout de la carte graphique, pas du processeur ; mesure à faire plus tard.
- GM Mode : n'existe pas dans WWE '12, il faudrait créer un mode entier. Pas prévu.

## Règles rappelées
- On peut porter des fonctions de WWE '13 / 2K14, mais **rien de dérivé du jeu ne se publie**.
- Rien n'est promis aux joueurs tant que ce n'est pas testé en jeu.

## Ajouts 1.1 (décision de l'utilisateur, 2026-10-31, suggestions de joueurs, voir `docs/suggestions_joueurs.md`)
- Univers : interférence « embuscade en coulisses » (cinématiques Create a Story), deux mallettes MITB (RAW / SmackDown).
- Mod Maker : porter n'importe quelle scène de victoire (même format que les entrées : Winning*.pac) ; musique de victoire et vidéo WWE '13 si pas finies pour la 1.0 ; entrées des autres jeux.
- ~~Mod Maker : vrais noms des catcheurs pour les entrées et mouvements prédéfinis.~~ **Fait en 1.0** (2026-10-31).
- Launcher : touches de clavier comme le port SvR 2011 (à préciser). À étudier : chronomètre de match, interférence forcée sur un bouton, types de match du port SvR 2011.

## Après le Mod Maker : chargements et jeu le plus natif possible (demande de l'utilisateur, 2026-11-02)
- Objectif : des temps de chargement courts comme le port SvR 2011 de Kaiko, et un jeu le plus natif possible (cf. « full natif » en 1.1).
- Réponse de Kaiko (Discord, 2026-11-02) : rien de magique ; compiler les shaders au démarrage (on a `pipelines_seed.w12p` + `bd_pso_precache`), avertir si le jeu est sur un disque dur (HDD), et surtout **la vitesse dépend de la façon dont le jeu lit ses données**, pas seulement du disque.
- Déjà mesuré chez nous (voir CLAUDE.md, « Chargements, suite ») : lectures et zlib rapides (32 Mo lus en 12 ms), le chargeur avance au rythme des images ; `wwe12_loading` (x2) livré, `wwe12_load_turbo` coupé par défaut (interblocage à 4 fils). Pistes : lecture d'1 s pour 750 Ko après les voix du speaker ; avertissement HDD dans le launcher ; comparer avec la façon de lire de Kaiko.
- **Mod Maker, arène RAW de SvR 2007 (2026-11-03, 1.1)** : la foule de l'arène de base de WWE '12 est placée sur la géométrie de la base ; les gradins 2007 sont à d'autres distances, donc les premiers rangs tombent au niveau du ring (devant la caméra) et les gradins 2007 restent vides. Les arènes 2008-2011 ne sont pas touchées. Pistes : refaire des fichiers de places depuis les modèles de foule 2007 (JBOY 20000-21247 + 40000-40191), recaler les places de la base sur les gradins 2007, ou au minimum retirer les places trop proches du ring. Contournement : décocher « public » à l'étape 4 ou choisir une autre base. **Fait le 2026-11-05 (hors jeu)** : les places de l'arène 2007 elle-même (voir `docs/plan_svr_anciens.md`).

## 1.0.1 : corrections seulement (décision de l'utilisateur, 2026-11-03)
Règle : **aucune nouveauté**. On part de la 1.0.0 (tag v1.0.0 = 5d42801) et on ne prend que les corrections choisies, pas tout `main` (les commits 1.1 des autres sessions, ex. Mod Maker, restent dehors). Même procédure que la 1.0 (build, zip, contrôle sans données de jeu ni DLL Microsoft, test d'une installation propre) ; l'APK Android est refait aussi (versionCode 10001, même code source). Publication : l'utilisateur seul ; rien n'est poussé sans son go. Date visée : dans 3 à 7 jours, après les premiers retours de la 1.0 ; tout de suite si un bug empêche de jouer (crash au lancement, sauvegarde cassée, mise à jour qui échoue).

| Correction | État | Remarque |
|---|---|---|
| Univers : plantage avec 5 matchs RAW/SmackDown (+ qualifications) | fait, testé en jeu, poussé (52cbdeb) | cause : modèle de match absent pour le 5e match après certains PPV ; docs/univers.md §39 |
| D3D12 : catcheurs transparents sur Intel UHD 620 (#7) : profondeur remise dans l'ordre | fait (fe20ea4, local), vérifié sur RTX 5070 ; **à confirmer par le joueur de #7** (build de test envoyée) | réglages `svr_d3d12_depth_flip` (true) et `svr_d3d12_depth32` (false, 2e piste) ; les shaders changent : refaire `tools/gen_shaders.ps1` |
| Tenues Superstar Threads abîmées / toutes blanches sur petits PC (Discord, Nothing) : correctif de Kaiko adapté (commit d8de438 de son SvR 2011) | go de l'utilisateur 2026-11-04, Rendu à reproduire puis corriger | **à créditer Kaiko dans le code, la doc et les notes** ; le jeu cuit la tenue couche par couche, notre cache ne relit la texture qu'une fois par image |
| GT 430, peaux cassées en D3D11 (#55) : correctif de Kaiko « old NVIDIA cards » | go 2026-11-04 : récupérer son dépôt à jour et vérifier | 1.0.1 si petit et clair, sinon 1.1 ; crédit Kaiko |
| Mod Maker, nuit du 2026-11-04 (commits locaux de Mods, rien poussé) : un .pac de WWE '12 en 2e tenue pris tel quel (Jay n°3) ; tenue d'entrée 2K16/2K17 choisie par le joueur (Rollins de 2K16 marche en entier : tenue, mur, ruban, plus de gel avec une arène ajoutée) ; visage de loin corrigé (Jay n°1) ; entrée plus sautée après un gel ; vidéos d'entrée et musique faites comme celles du jeu ; écran des arènes avec pages LB/RB et nom de l'arène ; plus de plantage avec beaucoup de catcheurs ajoutés (testé à 50) ; aide du Mod Maker : section tenues d'entrée | fait en local, à tester en jeu par l'utilisateur | aussi à vérifier hors jeu : visages pixélisés de loin (n°1), tenues d'entrée 2K16 (n°7), base « Fatal 4 Way » pour les arènes 2K14 (n°5) ; le tron sans minitron (Max Matus) si c'est un mauvais nom servi |
| Mod Maker : gel au chargement avec arènes ajoutées + catcheurs ajoutés (LA Max, Rollins) : l'arène ajoutée servait sa propre copie de `evtmov.pac`, qui masquait les murs / rubans des catcheurs ajoutés | corrigé en local (c07865a, 151f116, 2bb436a), go de l'utilisateur 2026-11-04 | vrai bug, pas une limite de nombre |
| Android : mémoire sur vieux noyaux (Poco X3 Pro, #41) + journal quand le GPU s'arrête (#54) | corrigé en local (8b61261), non testé sur vieux noyau | PC identique ; APK refait avec le PC (10001) ; vérifier les modifs non commitées de svr_resources / hooks/state |
| Tenues Superstar Threads blanches (867181b) et peau cassée GT 430 en D3D11 (106b74c) | faits, testés en jeu, à faire confirmer par Nothing et le joueur de #55 | crédit Kaiko dans les notes |
| Mesure « temps CPU par image » sur Windows (diagnostic seul) | proposé par Rendu, attend le go | aucun effet sur le rendu |
| Bugs de la 1.0 signalés d'ici là | à venir | GitHub / Discord |

Pas en 1.0.1 (1.1) : tout le Mod Maker (prises, entrées, arènes), foule SvR 2007, squash, finishers, I Quit / Special Referee, scènes de victoire, vidéos WWE '13, deux mallettes MITB, icônes de touches, chargements / jeu plus natif, bugs du jeu d'origine à tester (tenues de The Miz verrouillées, etc.).

## 1.1 : limite du nombre de catcheurs ajoutés (LA Max, 2026-11-04)
- Plantage vers 44-50 catcheurs ajoutés (écran de choix ou chargement du match) : limite de **mémoire** du jeu (512 Mo Xbox), pas un nombre fixe ; le pack fabriqué fait 250 Mo, portraits de menu (DLC_HD) 3 fois plus gros que ceux des vrais DLC. Suite : bissection 10 / 20 / 30, alléger ce qui est chargé en entier, avertissement dans le Mod Maker au-delà du seuil. Les 190 / 222 sont des places libres, pas une capacité testée (nombre sûr connu : 10 ; 20-25 conseillés en attendant).

## Décisions de l'utilisateur du 2026-11-04 (soir)
- **Ceinture à la taille : fait** (confirmé par l'utilisateur). **Tenues de The Miz verrouillées : on laisse tomber.** **CAW dans l'effectif Univers : en attente** (voir la note ci-dessous).
- **Go** : cas Jay n°5 (arène NXT de 2K14 sur « Fatal 4 Way », probablement le même gel que celui corrigé cette nuit) ; ligne « cpu per frame » sur Windows (diagnostic seul).
- Note CAW dans l'effectif : la mise à jour TU2 retire un test qui écartait les CAW (numéros < 50) du comptage des catcheurs d'une émission ; notre recomptage les écarte encore. Effet attendu faible, à faire seulement si un joueur signale des émissions sans assez de catcheurs avec des CAW.

## Chantier performances, Steam Deck et jeu natif (demande de l'utilisateur, 2026-11-05)
- Objectif : optimiser, régler les soucis Steam Deck, enlever des couches d'émulation et se rapprocher d'un vrai jeu natif PC, comme Jarrod dans ses dernières mises à jour de WWE 2K14. **Après la 1.1** (rien ne retarde la sortie).
- Point de départ : `docs/optimisation_jarrod.md` (points 1 à 3 faits ; 4 à 10 non faits). Mesure de Rendu : en scène lourde le fil de rendu coûte ~1 ms pour ~6 ms de « work » : le goulot est le **code du jeu recompilé**, pas notre rendu.
- Pistes : top 10 des fonctions chaudes du fil principal (échantillons de piles), réécriture native des plus chaudes (méthode de Jarrod : sortie identique, contrôle image par image), couches d'émulation restantes (accès mémoire invité avec inversion d'octets, noyau émulé, audio), code généré qui garde plus de registres.
- Études en cours (lecture seule) : Rendu (Steam Deck et performances) et Gameplay (natif / code du jeu), résultat dans `docs/perf_natif_steam_deck.md`.
- Jarrod : accord verbal pour s'inspirer de ses mises à jour ; copier du code = demander d'abord ; crédit.

## Décision de l'utilisateur du 2026-11-05 : performances sans changer tout le code, le reste en 1.2
- **Maintenant (sans régénérer le code du jeu), Rendu** : télécommande intelligente (état du jeu, captures faites par le jeu, « lancer un match », « jouer N secondes et écrire le résumé »), memcpy / memset natifs (`sub_82A76060` / `sub_82A76590`), image en retard affichée tout de suite si le gain est mesuré.
- **1.2, tout ensemble (une seule régénération du code, un seul gros test)** : nouveau xex patché avec les 2 vraies mises à jour de THQ (TU1 puis TU2) + options de registres locaux du générateur de code + modèle « volatile » ; remappage des 751 entrées de `wwe12_config.toml` et des ~280 adresses de `port/src`. Gameplay prépare le plan unique.

## Après la 1.1 PC : version Android (demande de l'utilisateur, 2026-11-05)
- **Ajouts** : la liste du co-mainteneur Android de l'utilisateur (à transmettre à la session Android).
- **Optimisation et performances** Android.
- **Surtout : remettre le replay d'après match** (coupé par défaut depuis l'alpha 6 à cause des gels, #23 / #24 / #29 / #51). Piste : l'attente de l'enregistreur de replay (même cause que le « match gagné qui ne finit jamais » de Kaiko sur Steam Deck ?).
- La session Android prépare `docs/android_apres_1_1.md` (lecture seule, sans retarder la 1.1).

## Décision de l'utilisateur du 2026-11-05 (séance de tests) : tout en 1.1
- Dans la 1.1 : alliés / rivaux à 0 / 0 des catcheurs ajoutés (Univers : `Wwe12UniverseForgetRelations`, à tester sur une copie de sauvegarde), image des NEWS Elimination Chamber / MITB (logo du PPV au lieu du globe), base neutre automatique de NOUVEAU CATCHEUR (au lieu de Ted DiBiase), entrée 2K14 de Damien Sandow (ni musique ni titantron).
- **Page ROSTERS du launcher** (marques hors de l'Univers, étude `docs/launcher_rosters.md`, 3 séances, écrit dans la sauvegarde : risque) : **1.3** (décision de l'utilisateur, 2026-11-05).

## À étudier : version Linux native (idée d'un joueur Discord, 2026-11-06)
- Steam Deck et PC Linux sans Proton (pas de boîtes de fichier Windows ni de détection Wine). Pas pour la 1.1 ni la 1.2.
- Coût : SDK + jeu pour Linux x86_64 (code du port avec appels Windows), launcher Linux (le nôtre est Win32 + D3D11), paquet (archive / AppImage / AUR, jamais de données du jeu), tests sans vrai Deck. Gain de performance incertain : le goulot est le code du jeu, pas Proton.
- Étude de faisabilité en lecture seule confiée à la session Android (`docs/linux_natif_etude.md`).

## Steam Deck / petit PC (demande de l'utilisateur, 2026-11-06)
- A : profil « Steam Deck / petit PC » du launcher (fait et essayé sur ce PC avec un Deck simulé, pas encore sur un vrai Deck), guide dans la FAQ. Confort Deck du launcher (manette, hauteur de fenêtre, boutons) : à faire.
- B : test de performance intégré (Rendu, vise la 1.2).
