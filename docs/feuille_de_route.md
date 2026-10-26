# Feuille de route : idées de l'utilisateur (bloc-notes du 2026-10-26)

Liste écrite par l'utilisateur (iMrBurn), rangée par la conversation Main. Les difficultés sont des
**estimations**, pas des mesures. Les détails techniques sont dans `CLAUDE.md`, `docs/univers.md`,
`docs/limit_breaking.md`, `docs/mod_maker.md` et `docs/suggestions_joueurs.md`.

## Bloc-notes tel quel

**Stipulations à ajouter** : I Quit, arbitre invité spécial (« compliqué, voir avec Kaiko »).

**1. Mode Univers**
- Titres utilisés vacants ? (pas possible, ça casse le mode)
- Menu Stat Tracking comme WWE '13 (historiques des vainqueurs du Royal Rumble, MITB, KOTR…)
- Tournoi pour couronner des titres vacants
- Modifier dans le code le booking (ex. défenses de titre spontanées en weekly)
- Restreindre des catcheurs à certains titres (Cena : titres majeurs seulement, Evan Bourne : mineurs)
- Backport des cutscenes de WWE '13 et 2K14
- Intégrer les cutscenes de Create a Story et les déclencher comme les vraies cinématiques de l'Univers
- Enlever le minimum de 15 catcheurs par show (univers 100 % CAW)
- Un seul show majeur possible (le jeu en réclame 2 minimum)

**2. Modifs de jeu**
- Sang pour les femmes ? (pas sûr)
- 5 contre 5 ? Plus de 6 superstars sur le ring ?
- Touches de clavier à la place des boutons Xbox / Sony
- Jukebox dans les menus
- Moments OMG de WWE '13
- Enlever les restrictions du Match Creator
- De 50 slots CAW à 100/200
- De 20 logos personnalisés à 100/200
- Autres résolutions (360p, 480p…)
- Widescreen (16:10)

**Gros chantiers** : Mod Maker pour la 1.0 (fait, voir `docs/mod_maker.md`) ; jeu en full natif sans
aucune émulation ; DX12 en plus de Vulkan, peut-être DX11.

## État et difficulté (estimation, 2026-10-26)

### Déjà fait ou décidé
| Idée | État |
|---|---|
| Mod Maker | Fait en grande partie (arènes, catcheurs ajoutés, textures ; `docs/mod_maker.md`). |
| Clavier au lieu des boutons | Le clavier marche (joueur 1 ou 2). Reste : Shift/Ctrl/Alt seuls (issue #18, 1.0). Les icônes affichées restent celles de la manette. |
| Jukebox dans les menus | Lot 1.0 choisi, attend le « go » (Gameplay ; page OPTIONS > JUKEBOX, cvar `wwe12_jukebox`, défaut off). |
| Minimum de 15 catcheurs par show | Lot 1.0 choisi, étudié (Univers ; cvar `wwe12_universe_min_roster` ; test sur une COPIE de la sauvegarde). |
| Sang pour les femmes | Reporté à la 1.0. On connaît l'option BLOOD ; le test homme/femme du saignement n'est pas trouvé (voir `CLAUDE.md`). |
| I Quit, arbitre spécial | Étudiés (Limit breaking). « I Quit » simple = abandon seulement ; l'arbitre spécial (type 83) demande un test en jeu avec l'accord de l'utilisateur. |
| Titres vacants utilisés par un show | Impossible (casse le mode). Seuls les titres sans émission peuvent être vacants : fait. |

### Faisable, travail raisonnable (à étudier)
- Un seul show majeur au lieu de deux (même famille que le minimum de 15 ; Univers).
- Restreindre des catcheurs à certains titres (règles dans les tables de l'Univers).
- Défenses de titre spontanées : seulement si c'est simple (règle de l'utilisateur).
- Résolutions 360p / 480p : on a 540p ; plus bas = très flou, à tester.
- Widescreen 16:10 : à vérifier ce que fait le jeu aujourd'hui avant de promettre.
- Restrictions du Match Creator : à préciser lesquelles.
- Plus de slots CAW / logos (100/200) : touche les sauvegardes et la mémoire, risqué.

### Difficile ou gros
- Stat Tracking : le jeu ne garde ni victoires ni historique, il faut les noter nous-mêmes.
- Tournoi pour titres vacants ; cutscenes de WWE '13 / 2K14 ; cutscenes de Create a Story dans l'Univers.
- Moments OMG de WWE '13.
- Plus de 6 superstars sur le ring / 5 contre 5 (tableaux à 6 cases, mémoire, animations).

### Gros chantiers
- Jeu en full natif : long terme, par étapes (décompilation progressive, système par système).
- DX12 (plume le gère) en plus de Vulkan ; DX11 à part.

## Règles rappelées
- On peut porter des fonctions de WWE '13 / 2K14, mais **rien de dérivé du jeu ne se publie**.
- Rien n'est promis aux joueurs tant que ce n'est pas testé en jeu.
