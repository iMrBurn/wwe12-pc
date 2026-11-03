# Feuille de route (version de l'utilisateur, mise à jour le 2026-10-27)

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
- **Mod Maker, arène RAW de SvR 2007 (2026-11-03, 1.1)** : la foule de l'arène de base de WWE '12 est placée sur la géométrie de la base ; les gradins 2007 sont à d'autres distances, donc les premiers rangs tombent au niveau du ring (devant la caméra) et les gradins 2007 restent vides. Les arènes 2008-2011 ne sont pas touchées. Pistes : refaire des fichiers de places depuis les modèles de foule 2007 (JBOY 20000-21247 + 40000-40191), recaler les places de la base sur les gradins 2007, ou au minimum retirer les places trop proches du ring. Contournement : décocher « public » à l'étape 4 ou choisir une autre base.
