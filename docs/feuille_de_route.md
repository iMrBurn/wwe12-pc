# Feuille de route (version de l'utilisateur, mise à jour le 2026-10-27)

Liste écrite par l'utilisateur (iMrBurn), rangée par la conversation Main. Les détails techniques sont dans
`CLAUDE.md`, `docs/univers.md`, `docs/limit_breaking.md`, `docs/mod_maker.md`, `docs/camera.md` et
`docs/suggestions_joueurs.md`. Règle : on ne livre que ce qui est testé en jeu ; ce qui n'est pas prêt passe à la version suivante.

## Roadmap 1.0 (texte de l'utilisateur)

**Modifs de jeu** (liste restante, 2026-10-28)
- ~~Ajouter les moments OMG de WWE '13~~ : **abandonné (décision de l'utilisateur, 2026-10-28)**. Étude dans `docs/omg_finishers.md` : les vrais OMG coûtent 15 à 30 séances, le ring qui s'effondre plus de 30 ; pas prévu.
- Choix des finishers comme 2K14 : stock de finishers de départ 1 / 2 / 3 / infini sur l'écran de sélection (étude demandée à Gameplay)
- Squash matchs : **à faire plus tard dans la 1.0** (décision de l'utilisateur, 2026-10-28). Étude dans `docs/squash.md` : le levier est STARTING HEALTH de Create a Story (vie par zone au départ), option « prochain match : squash » ; 2 à 4 séances ; il faut une sonde en jeu (go de l'utilisateur). Blessures qui durent dans l'Univers et attaque après le tombé = 1.1.

Fait depuis la liste précédente : défenses de titre en weekly et qualifications MITB / Elimination Chamber (Univers, testé) ;
Match Creator étendu (testé, activé par défaut) ; résolutions 360p / 480p (testées) ; format d'écran AUTO / ÉTIRER
(le vrai 16:10 est un gros chantier, 1.1) ; DX11 (testé sur RTX 11_1 / 11_0 et Intel intégré, reste la décision sur
la taille du zip) ; préréglage PC modeste ; scan de textes allégé. Les icônes de touches de clavier dans les menus
ne sont pas faites (le clavier lui-même marche, Shift / Ctrl / Alt aussi).
Aussi demandé : travailler sur le Mod Maker (voir `docs/mod_maker.md`).

(Décision du 2026-10-27 : le jeu en full natif et les slots CAW / logos 100/200 passent en 1.1.)

## Roadmap 1.1 (texte de l'utilisateur)

**Stipulations à ajouter** : I Quit ; Special Guest Referee

**Modifs de jeu** : plus de 6 superstars en même temps sur le ring ; passer de 50 slots CAW à 100/200 ; passer de 20 logos custom à 100/200

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
