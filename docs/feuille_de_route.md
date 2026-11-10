# Feuille de route (mise à jour le 2026-11-06, validée par l'utilisateur)

Les détails techniques sont dans `CLAUDE.md`, `docs/univers.md`, `docs/mod_maker.md`, `docs/title_update_corrections.md`, `docs/plan_1_2_codegen_tu.md`, `docs/perf_natif_steam_deck.md`, `docs/launcher_amelioration.md`, `docs/android_apres_1_1.md` et `docs/suggestions_joueurs.md`. L'ancienne feuille de route (textes de l'utilisateur, notes, décisions jour par jour) est gardée en bas, sous « Détails et historique ».

## Règles du jeu des numéros
- **1.0.x** : seulement pour un bug grave (plantage au lancement, sauvegarde cassée, mise à jour qui échoue). Pas de 1.0.1 prévue (décision du 2026-11-04).
- **1.x** : un thème par version. Ce qui n'est pas testé en jeu passe à la version suivante.
- **2.0** : seulement si les sauvegardes, le format des mods ou la façon d'installer changent.
- Rien n'est promis aux joueurs tant que ce n'est pas testé en jeu. Une seule liste officielle (ce fichier) ; les issues GitHub n'y sont que des liens.
- Rien de dérivé du jeu ne se publie (pas de données de jeu dans un zip ni sur GitHub).
- Estimations en **heures de travail hors tests de l'utilisateur** (pas en « séances »).

## 1.1 : « Mod Maker et entrées » (en cours, une seule version, sans date)

**Fait et testé en jeu (poussé sur GitHub)**
- Univers : plantage à 5 matchs par show ; catcheurs ajoutés sans alliés ni rivaux, marque gardée ; image du PPV dans les news de qualification ; jukebox arrêté à chaque chargement.
- Rendu : D3D12 sur Intel UHD 620 (#7), tenues Threads blanches et peau GT 430 (corrections de Kaiko, crédit), saccades de shaders (shaders préparés au démarrage, `declarations_seed.w12d` validé pour le zip), mesure du CPU par image.
- Mod Maker : gel arènes + catcheurs ajoutés, mémoire pour 50+ catcheurs (tas 12 / 15), pages d'arènes, genre des catcheuses, titantron d'avant-match, entrées 2K14 (Sandow), Goldberg 2K17, yeux de SvR 2009/2010, DLC comme base, catcheurs installés modifiables, foule et victoires SvR 2007, prises : couverture améliorée (2K14 48 → 73 %), MOVES & ENTRANCES.
- Launcher : profil « Steam Deck / petit PC » (testé sur ce PC avec un Deck simulé, pas sur un vrai Deck) ; FAQ Steam Deck ; Android : vieux noyaux + journal GPU, nouveau look (Yexclsv), graine `declarations`.

**À tester avant la sortie**
- Arène RAW de SvR 2007 refaite dans le Mod Maker (transition compressée) : avant / après match sans plantage.
- Étape 1 des prises WWE '13 (case « Prises ajoutées (expérimental) ») : sinon marquée expérimentale dans les notes.
- Royal Rumble de Jay avec ses mods (à lui faire retester) ; PPV à une seule marque (Univers) ; réglage « Largeur » du portrait (685c950, local).
- Android : correctif des vieux noyaux (joueur de #41), look sur vrai téléphone (Yexclsv).

**Dans la 1.1 sur décision de l'utilisateur (2026-11-06), NON testé en jeu, annoncé « expérimental »** : reprise automatique des notes / taille / genre depuis SvR 2007-2011 (8fc2fc1, remis dans `main`). **Hors 1.1** : vraies animations de prises SvR 2011 / 2K, personnes cachées des entrées 2K, 2K17, détail des yeux SvR 2010 en courant, tenues de The Miz (abandonné), base neutre de NOUVEAU CATCHEUR (refusée).

**Notes publiques** : les notes de la 1.0 annonçaient pour la 1.1 « squash, finishers de départ, I Quit, Special Referee, deux mallettes MITB, icônes de touches » : les notes de la 1.1 disent que c'est repoussé (1.3).

## 1.2 : « Moveset update » : PUBLIÉE le 2026-11-10 (PC + Android, tag v1.2.0, zip SHA ab13bf6d…, APK 2c72767b…)

**Livré** : prises SvR 2007-2011 et WWE '13 / 2K14-2K17 dans le Mod Maker (expérimental) + page MOVES & ENTRANCES ; choix de la marque (RAW / SmackDown / aucune) d'un catcheur ajouté, marque gardée (sauvegarde, match, reset de l'Univers) ; genre Homme / Femme qui compte enfin (0xEE autorisé : une diva sur base d'homme est une femme) ; entrée choisie dans le jeu gardée ; moveset copié gardé ; titantron d'équipe respecté ; news de l'Univers avec le vrai nom ; noms de fichiers à accents / japonais lus en UTF-8 (jeu, launcher, Mod Maker : plantage au démarrage et page MUSIC) ; gamma (curseur du jeu actif, image un peu plus sombre par défaut, option « Console picture ») ; saccade de début de match (read-backs différés, ~10 fois moins d'attente) ; moins de CPU (blocs de constantes réutilisés, hachage 4096) ; mode 30 fps EXPÉRIMENTAL en matchs (jamais par défaut ; liste « Frame rate » du launcher PC et Android) ; journal des étapes de l'Univers ; message clair au-delà de 35 arènes ajoutées. Android : ligne Frame rate, APK signé avec la même clé.
**Non vérifié en jeu avant la sortie** : moveset copié après relance, diva « Femme » dans l'Univers (matchs / classements Divas), gamma sur un vrai téléphone, hachage 4096 et read-backs différés sur Android.
**Ouvert après la 1.2** (voir 1.3) : plantage des équipes avec des modèles SvR 2011 (il faut le crash_trace du joueur), gel des 2K17 de l'issue #60 (journal du joueur ; à refaire avec le nouveau Mod Maker), blocage de fin de match de l'Univers sur Steam Deck (le journal d'étapes dira où), image noire avant la cloche (capture RenderDoc), arènes 2K14 / SvR 2009 (défauts graphiques ; « la précédente reste » : journal du joueur).

(Texte d'origine de la section, gardé pour l'historique : décision de l'utilisateur, 2026-11-08 : pas de 1.1.1, tout va directement dans la 1.2.)

Même chemin que la 1.1 pour la sortie : « go build », gel des sessions, APK à refaire si le code partagé change.

**Moveset update**
- Port des prises de SvR 2007-2011 et de 2K14-2K17 dans le Mod Maker (état au 2026-11-08 : prises ajoutées WWE '13 / 2K jouables en jeu « pas parfait » depuis 625aaea ; il manque l'écriture des paramètres `m3` pour WWE '13 / 2K, sans eux elles gèlent ; notes dans `docs/prises_svr11.md`). Mods continue 2K17 puis SvR.
- Puis une grosse session de corrections de bugs et quelques améliorations.

**Retours de joueurs sur la 1.1.0 (ex-« 1.1.1 »)**
- **Entrées de catcheurs ajoutés non sauvegardées** (retour d'un joueur, 1.1.0 ; fait en 1.2) : cause probable trouvée par Mods, correctif d'une ligne (`wwe12_superstar_mods.cpp`, test « fiche étrangère » à partir de 0x1EC), commit local 6e7d1d1. Touche le jeu. À tester : catcheur ajouté, choisir une entrée, sauvegarder, relancer. À confirmer avec le joueur qu'il s'agit bien de catcheurs ajoutés.
- **Gamma / luminosité** (retour d'un joueur) : étude de Rendu faite. Le curseur du jeu ne fait rien (la rampe gamma du jeu est captée par le GPU « null » du SDK mais personne ne la lit) ; la Xbox 360 corrige aussi pour une télé. Plan : (1) faire marcher le curseur, 3-5 h ; (2) réglage « Image : CONSOLE (TV) / PC » dans GRAPHICS et le launcher, 1-2 h, défaut PC proposé. Étape 0 : une ligne de journal pour confirmer avant de coder (build + lancement, avec le « vas-y » de l'utilisateur).
- **Page MOVES & ENTRANCES au niveau de celle de Kaiko** (comparaison de Mods) : (~2 jours, fait par Mods : onglets Prises / Provocations, « Jouer » en 3D, entrée d'un autre catcheur, commits 0a28d9c 35362d2 4448fb8, vérifié hors jeu seulement) l'entrée d'un AUTRE catcheur de WWE '12 choisie par nom (3-5 h), changer le moveset case par case avec les prises du jeu (6-10 h), raccourcis signatures / finishers (2 h) ; si le temps le permet : provocations (3-4 h). Plus tard : aperçu complet d'une entrée (25 h et plus), packs de prises autonomes (10-15 h), porter UNE prise dans une case (8-12 h). Mise en page de Kaiko reprise avec crédit (MIT).
- **Univers : après un match, le jeu ne passe pas à la victoire** (retour d'un joueur) : rien de bloquant dans le code Univers (relu). Hypothèses : scène de victoire d'un catcheur ajouté / mod de prises, ou blocage du moteur. À confirmer avec le joueur (version, image figée ou non, `hang_trace.txt`, mods, match exact, même résultat en Exhibition). Pas de correctif sans ces réponses.
- **Saccade de début de match + clignotement d'écran (Intel RaptorLake, Vulkan, journal wwe12_048)** : les shaders ne sont plus la cause (0 pipeline en match). Cause probable (Rendu) : read-backs d'une cible par image pendant les 32 premières images (~10 cibles 64x64 + 512x512), qui font attendre le GPU ; clignotement : nouvelle cible 1280x720 remplie en noir. Plan : (a) diagnostic seul dans la ligne [load] (~1 h), (b) passer l'erreur « unknown guest format 0xC000000F » en debug (voulue), (c) vrai correctif (regrouper les read-backs) après mesure et test des tenues Threads ; clignotement : capture RenderDoc d'abord.

- Restent pour plus tard les aperçus 3D complets d'une entrée (25 h et plus), les packs de prises autonomes (10-15 h) et le port d'UNE prise dans une case choisie (8-12 h), si non faits.
- **Tout ce qui était prévu pour l'ancienne 1.2 est décalé en 1.3, l'ancienne 1.3 en 1.4, l'ancienne « 1.4 et après » en 1.5 et après.** Les sections « Détails et historique » plus bas gardent l'ancienne numérotation (1.2 = THQ / vitesse, 1.3 = modes de match).

## 1.3 : « Les corrections de THQ, la vitesse et le test de performance » (liste de l'utilisateur, 2026-11-07 ; ex-1.2, décalée le 2026-11-08)

**Reporté de la 1.2 (2026-11-10)**
- Mode 30 fps plus fluide pour les petits processeurs (issues #53, #57, #41) : la voie Kaiko (une image sur deux, logique à 60 Hz) a été mesurée : ~15 % de CPU en moins seulement, et des prises qui paraissent buguées ; le mode natif gagne ~60 % mais des prises sont moins fluides. Autres pistes de CPU : code D3D du jeu dans nos crochets (~0,7 ms, risque de gel élevé, plan `docs/couches.md`), constantes (~0,3 ms, demande de modifier XenosRecomp).
- Gamma : vraie table (LUT) pour les réglages extrêmes (une simple puissance colle mal) ; image noire d'une image avant la cloche (capture RenderDoc automatique).
- Mod Maker : importer un FBX comme tenue ; tenue d'entrée pour une tenue d'un catcheur AJOUTÉ (Cody American Dream) ; limite de 35 arènes ajoutées (textures de bannière libres du paquet) ; défauts graphiques des arènes 2K14 / SvR 2009 ; « option intergender » limitée à l'Univers (sub_82A39EF8) ; alerte CHECK MODS « catcheuse sur base d'homme ? » (fausses alertes possibles).
- Plantage des équipes SvR 2011, gel 2K17 de #60, fin de match de l'Univers sur Steam Deck : selon les traces des joueurs.

**THQ (voie B : nos propres crochets sur le code du disque, un par un, chacun avec son réglage)**
- Comparaison des 2 mises à jour officielles : faite (`docs/title_update_corrections.md`, session Gameplay) ; reste le tri et le test de chaque correctif en jeu.
- **Objectif de l'utilisateur (bloc-notes Discord du 2026-11-07, relu le 2026-11-10) : tester les bugs du jeu d'origine et appliquer nous-mêmes les DEUX Title Updates** (nos propres crochets, voie B). Bugs du jeu d'origine à tester : finishers qui gèlent (TU2, 6-7 crochets), cage / barrières de Hell in a Cell, tables sous le ring, sons après Backstage Brawl, movesets CAW qui se réinitialisent, évasion de tombé en Elimination Chamber, aperçu des prises DLC dans l'éditeur de CAW (TU1). Cage Hell in a Cell et évasion de tombé : rien trouvé dans le code, à vérifier en jeu.

**Vitesse**
- Chargements plus rapides (étude : `docs/chargements_1_2.md`) : turbo de chargement (`wwe12_load_turbo`, existe mais coupé par défaut : match ~4,0 → 2,8 s, fin des entrées ~4,5 → 2 s), bridage des animations 2D réécrit par nous (2-3 h), contre-épreuve sur 4 fils, décision de l'utilisateur sur le turbo par défaut ; avertissement disque dur dans le launcher.
- 30 fps partout : le mode 30 du jeu imposé aux matchs (clé `wwe12_frame_mode` = original / 30 / 60_entrances), PC + Android (~7-8 h).
- Code natif plus rapide : `memcpy` / `memset` (fait, désactivé par défaut), registres locaux du générateur de code, patch « volatile » (une seule régénération du code, un seul gros test).
- Idées de Jarrod v0.2.19 (BSD-3, méthodes seulement, crédit THIRD_PARTY.md ; étude de Rendu dans `docs/perf_natif_steam_deck.md`) : mode mailbox (déchirement, 2-3 h), netteté de l'agrandissement 720p (3-4 h), test minimiser / restaurer (5 min), foule réglable (3-5 h). Plus tard : résolution dynamique de la 3D, foule en gros lots.

**Benchmark (test de performance)**
- Match imposé : Hell in a Cell à 6 sans menu (voie 2 : `sub_82449428`, 9 à 15 h) ; repli = méthode de Kaiko (6 à 9 h).
- Résumé + verdict (ok / cpu / gpu / failed), condition « faible » (2 cœurs), bouton dans le launcher.

**Autres**
- Yeux de SvR 2010 en courant.
- Page ROSTERS du launcher (marques hors de l'Univers, `docs/launcher_rosters.md`, 3 séances, écrit dans la sauvegarde).
- Encodeur Vorbis (Wwise) à nous, pour les thèmes portés de plus de 20 Mo (en 1.1 : coupés à 100 s).

## 1.4 : « Modes de match et Univers » (liste de l'utilisateur, 2026-11-07 ; ex-1.3, décalée le 2026-11-08)

**Modes de match**
- I Quit (la version légère d'abord), Special Guest Referee.
- Matchs squash (vie de départ réduite).
- Stock de finishers de départ (1 / 2 / 3 / infini, comme 2K14).
- Changer de joueur en plein match.
- Améliorer le système de tombé (sortie de tombé automatique ou jauge de kickout plus grande : suggestion d'un joueur).

**Univers**
- Deux mallettes Money in the Bank (RAW / SmackDown), embuscade en coulisses.
- Débloquer Edge et Christian etc. pour le mode Univers.
- Choisir le type d'intervention (bouton Y), par exemple attaque en coulisses ou autre.
- Backport des cinématiques WWE '13 / 2K14, Create a Story, tournoi pour titres vacants.
- Réglage pour couper les blessures et les fins de carrière.
- Classement des titres par équipe modifiable.
- Rivalités menées jusqu'au PPV, et rivalité inter-marques sans match (forcer un match au PPV ?).
- Titres de SmackDown dans EDIT SUPERSTAR (R1), si c'est le jeu d'origine.
- Photo propre aux arènes ajoutées.
- AJOUTER des logos et des matchup screens (les listes du jeu sont pleines).
- Nouveau show comme NXT.

## 1.5 et après : plateformes et finitions (à décider selon les retours)
- Launcher : améliorations (`docs/launcher_amelioration.md`, 20 à 28 h : état du jeu, messages, confort Steam Deck à la manette, bouton « Test de performance », finitions) ; pas de refonte du design.
- Android après la 1.1 PC : replay d'après match, performances, ajouts de Yexclsv (`docs/android_apres_1_1.md`).
- Prises avec leurs animations d'autres jeux (SvR 2011, puis 2K) ; yeux de SvR 2010.
- Version Linux native (étude faite, `docs/linux_natif_etude.md` : 8 à 12 séances).
- 16:10 / 21:9 (D3D11/D3D12, menu pause, HUD en 21:9) à confirmer ; icônes de touches de clavier ; test sur un vrai Steam Deck.
- Android : vrais téléphones, boutons PlayStation, ce que disent les joueurs.
- Shaders qui font saccader à la première fois (en 1.1 : préparés en arrière-plan ; reste à voir ce qui saccade encore, voir l'issue #53 : surtout le processeur).

## Plus tard (sans date)
- Plus de 6 superstars sur le ring ; 100 / 200 slots CAW et logos (probablement 2.0) ; le jeu en full natif ; Univers : cinématiques WWE '13 / 2K14, Create a Story, tournoi pour titres vacants ; choix de caméra de match.

## Abandonné / pas prévu
- Moments OMG de WWE '13 ; Title Update complète comme version du code (remplacée par la voie B) ; GM Mode ; le DLC dans les textes publics ; sauvegardes Wii ; tenues de The Miz verrouillées ; base neutre de NOUVEAU CATCHEUR.

## Comment suivre l'avancement
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

## Yeux des catcheurs de SvR 2009 / 2010 qui restent derrière en courant (1.2)
- Le bug principal (yeux détachés) est corrigé et poussé (f543cec). Le détail restant (les globes restent un peu derrière quand le catcheur court) : cause non trouvée, même os et mêmes poids que SvR 2011 (où ça va). Pistes pour la 1.2 : la 3e pièce d'œil de SvR 2010 (format de sommet 7, disque d'iris plat de 0,08 cm), ou une capture RenderDoc comparée image par image (2 à 3 h après la capture). L'utilisateur le laisse tomber pour l'instant (« si y a encore le bug, on me dira »). Dans les notes de la 1.1 : « connu ».

## Mode 30 fps (issue #57, 2026-11-06) : étude, 1.3 (ex-1.2)
- Dessiner une image sur deux en gardant la vitesse du jeu (un simple plafond le ralentirait), pour téléphones moyens, Steam Deck et petits PC ; à la manière du port 2K14 de Jarrodfeng (accord verbal, voir `credits-and-permissions`). Étude Android + Rendu (gain, risques : read-backs, tenues cuites, cibles de rendu). Dans la liste Steam Deck / petit PC. **Décision de l'utilisateur (2026-11-06) : l'option doit exister aussi dans la version PC** (réglage, page OPTIONS > GRAPHICS du jeu, case du launcher, profil Steam Deck) ; Rendu pilote la partie PC, Android la partie téléphone.

## Mode 30 fps : conclusion des études Rendu + Android (2026-11-06, 1.3, ex-1.2)
- Le jeu avance d'un pas de logique par image : un plafond à 30 le ralentirait de moitié. Mais il a SON PROPRE mode 30 (celui des entrées, logique deux fois plus par image) : on l'impose partout par un crochet de SetFrameRate (voie A). CPU et GPU font environ moitié moins par seconde. Ne pas dessiner une image sur deux (voie B) : déconseillé (peu de gain, casse la cuisson des tenues et les read-backs).
- Une clé commune `wwe12_frame_mode` = original / 30 / 60_entrances (l'ancienne `wwe12_60fps` est relue) ; ligne « FRAME RATE » d'ADVANCED en jeu, liste dans les launchers PC et Android, profil Steam Deck / petit PC à 30 ; test de performance : `--wwe12_frame_mode=30`.
- Estimation hors tests : ~5 h code commun (Rendu) + ~2,5 h Android. Risque : le jeu n'a sans doute jamais tourné en match à 30 (prises, contres, physique, replays, chronomètres) : à tester en jeu avec des joueurs.
