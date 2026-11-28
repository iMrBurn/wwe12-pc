# Feuille de route 2.0 (brouillon, 2026-11-21)

Source : le bloc-notes de iMrBurn (liste « 2.0 » du 2026-11-21) + les idées de la 1.6 et les étapes de la régénération.
Efforts et risques : estimations de la Main, pas des mesures. Rien n'est promis, pas de date : la 2.0 se fait sans se presser,
par paliers mesurés et testés avant de passer au suivant.

## La liste de l'utilisateur, triée

| # | Idée | Ce qu'on sait déjà | Effort (estim.) | Risque | Session |
|---|---|---|---|---|---|
| 1 | **Plus de 6 catcheurs sur le ring** | Étude faite (`docs/etude_plus_de_6_catcheurs.md`) : le 6 est à 3 endroits (liste de 9 cases lue par 969 fonctions, fiches de règles à 6 places, tests « < 6 »). Palier 0 = 8 catcheurs en test de dev. | Gros (paliers 0 à 3) | Élevé | Gameplay |
| 2 | **Régénération du code** | Palier 3 : −14 % de calcul en match, exe 100,5 → 84,4 Mo, 4 crochets prouvés en jeu. Reste : validation en jeu, retirer `volatile`, palier 2 (66 fonctions), gros test de 1 h 30 à 2 h. | Moyen à gros | Moyen (mesuré à chaque palier) | 2.0 Régénération |
| 3 | **Le jeu « full natif », sans émulation** | La régénération en est la première moitié. Retirer la couche D3D/PM4 du jeu : mesuré inutile pour les fps (`docs/rendu_2_0.md`). Donc « full natif » = surtout code régénéré + moins d'émulation autour. | Gros, long | Moyen | 2.0 Régénération + Rendu |
| 4 | **100 / 200 places de CAW et de logos** | `docs/limit_breaking.md` : format de sauvegarde et mémoire du jeu à étudier (probablement 2.0). | Gros | Élevé (sauvegardes) | à créer (Limit breaking) |
| 5 | **Choix de la caméra de match** | Rien d'étudié. À regarder : où le jeu choisit sa caméra, tables de caméras existantes. | Moyen | Moyen | Gameplay / Rendu |
| 6 | **GM Mode ?** | Rien d'étudié. Très gros : un mode entier (budget, drafts, rivalités). S'appuierait sur l'Univers (`docs/univers.md`). À cadrer avant de décider. | Très gros | Élevé | Univers (étude d'abord) |
| 7 | **Plus d'arènes** | Le Mod Maker porte déjà des arènes d'autres jeux. Reste : nombre de places, sélection, menus. | Moyen | Moyen | Mod Maker / Rendu |
| 8 | **Physique des armes de SvR 2011** | À étudier (code de Kaiko, SvR 2011 : relecture de la licence et accord avant de s'en inspirer). | Moyen à gros | Moyen | Gameplay |
| 9 | **Entrées avec mallettes (Money in the Bank) comme sur 2K18** | Rien d'étudié ; touche les entrées, modèles et animations. | Moyen | Moyen | Mods / Gameplay |
| 10 | **Cinématiques de WWE '13 et 2K14 dans WWE '12 (Create a Story)** | Étude faite (`docs/etude_story_designer_univers.md`) ; reprise prévue vers janvier 2027 à l'étape A. Écran noir = le lecteur de scènes demande le mode 3. | Gros | Moyen | Univers / Story Designer |
| 11 | **Liberté du calendrier Univers, comme 2K14** | Dans `docs/suggestions_joueurs.md` (« moyen ») : créer / déplacer / supprimer des émissions, PPV n'importe quel dimanche. | Moyen | Moyen | Univers |

## Les idées techniques de la 1.6 (à faire avant ou pendant la 2.0)

| Idée | Effort | Remarque |
|---|---|---|
| Lecture manette synchrone (si #81 confirmé) | Petit | ~15 lignes SDK ; garder PlayStation et manettes génériques |
| Réglages textures, ombres, plafond de résolution | Moyen | vu chez d'autres créateurs |
| Bon GPU sur portable + message si le pilote plante | Petit | piège classique |
| Option « manettes = joueur 1 » | Petit | |
| Priorités des fils et copie de textures plus économe | Moyen | gain estimé 1-1,5 ms par image |
| Profil « petit PC » dans le launcher | Moyen | |
| Idées Univers rapides (saignement des catcheuses, challengers, Légendes du Rumble) | Petit à moyen | |
| Retirer `volatile` ; palier 2 avec les 66 fonctions contrôlées | Moyen | étapes de la régénération |

## Ordre proposé

1. **Valider le palier 3** en vrai match (test de l'utilisateur) : c'est la porte de toute la régénération.
2. **Palier 0 des 8 catcheurs** (réglage de dev, test mesurable) : il dira si le rêve est réaliste.
3. En parallèle, côté Rendu : priorités des fils, textures, réglages de qualité, bon GPU (petites victoires, utiles tout de suite).
4. **Retirer `volatile`, puis palier 2** avec les 66 fonctions contrôlées, puis le gros test de l'utilisateur.
5. Ensuite seulement, selon ce que les paliers 1 et 2 ont montré : caméras, armes, mallettes, arènes, 100/200 CAW.
6. GM Mode et cinématiques de 13/2K14 : études d'abord, décision après.

## Règles de travail

- Une étape = ce qui est mesuré avant et après, et vu en jeu par l'utilisateur.
- Pas de régénération publiée sans un essai complet de tous les modes (menus, entrées, match, Univers, jukebox, Mod Maker).
- Une compile ou un jeu à la fois ; la Main prévient les sessions.
- Données de jeu jamais dans le dépôt ni dans le zip ; DLC jamais cité publiquement.

## Décisions en attente de l'utilisateur

1. Quel thème d'abord : « plus de 6 catcheurs » ou « régénération » (ou les deux en parallèle, comme proposé) ?
2. GM Mode : on cadre une étude, ou on le laisse pour une version ultérieure ?
3. Un nom pour la 2.0 (par exemple « Régénération » ou « Full natif »).

## Décisions de l'utilisateur du 2026-11-27

Gardés pour la 2.0 : **choix de la caméra de match** et **caméra libre pendant un match** (réclamées par des joueurs ; Kaiko et Jarrod
les ont dans leurs recomps), l'Univers (calendrier libre, rivalités d'une marque, tournois, statistiques), le **support de la souris dans les
menus**, le contenu (plus d'arènes, plus de places CAW, armes SvR 2011, mallettes) et le technique (régénération, jeu natif).
En réflexion : un menu de réglages complet dans le jeu (touche F1, comme les portages SvR de nefariousjosiah) en plus de OPTIONS > GRAPHICS.
Étude demandée à Rendu : caméra libre / choix de la caméra (code du jeu + ce que font Kaiko et Jarrod, sans copier de code).

## État au 2026-11-27 (soir)

### Liste du 3 novembre
| Idée | État |
|---|---|
| Plus de 6 superstars | Partiel : 8 apparaissent en test, ils ne bougent pas (IA des cases 7-8 à faire). Garder arbitre et commentateurs au-delà de 8 : pas fait (long). |
| 100 / 200 slots CAW et logos | Pas commencé (étude `docs/limit_breaking.md`, risque sauvegardes). |
| Jeu full natif | Partiel : régénération avancée ; retrait de la couche D3D/PM4 mesuré inutile. |
| Choix de caméra de match | Fait en test : normale, rampe (réglée, nette), caméra libre. Reste : choix dans le launcher. |
| GM Mode | Pas étudié. |
| Plus d'arènes | Pas commencé (le Mod Maker en porte déjà). |
| Physique des armes SvR 2011 | Pas commencé. |
| Régénération du code | Paliers 1, 3, 4 (plus de `volatile`) et 2b validés. Reste : gros test de 1 h 30 à 2 h, fusion. |
| Mallettes comme 2K18 | Pas commencé. |
| Cinés de 13 et 2K14 | Étude faite, reprise vers janvier 2027. |
| Calendrier Univers libre | Pas fait (seul le mois de départ au RESET UNIVERSE est fait). |

### Liste du 21 novembre
| Idée | État |
|---|---|
| Manette synchrone (#81) | Pas fait, on attend le joueur. |
| Réglages textures, ombres, plafond de résolution | Pas fait. |
| Bon GPU sur portable + message | Pas fait. |
| Option « manettes = joueur 1 » | Pas fait. |
| Priorités des fils, copie de textures | Mesuré, aucun gain : abandonné. |
| Profil « petit PC » | Pas fait. |
| Idées Univers rapides | Partiel : #89 et mois de départ faits ; saignement, challengers, Légendes du Rumble non. |
| Retirer `volatile` | Fait, validé (palier 4). |
| Palier 2, 66 fonctions | Fait, validé (palier 2b). |

### Hors bloc-notes
- Fait : vue rampe (réglages de l'essai C2), caméra libre (vol, figée pour jouer, F7 interface, F8 pause), flou de profondeur coupé en rampe, tri des objets avec notre champ de vision, écran géant : image miroir retirée (`wwe12_live_screen`) et logo de l'émission à la place (`wwe12_titantron_logo`, en test).
- À ajouter : support de la souris dans les menus, menu de réglages F1 (en réflexion), rivalités par marque / tournois / statistiques Univers, idées de Kaiko et Jarrod.
- Autres : Android 1.6 ou 2.0 (USB et manette #87, APK 1.5.0 non testé sur téléphone), issues ouvertes (#69, #74, #82 à #87, #92), iOS en pause, page ROSTERS du launcher, chaos de l'Univers (0 à 3).

## Ajouts de l'utilisateur du 2026-11-28 (efforts : estimations de la Main)

### Nouvelles idées 2.0
| Idée | Effort | Session probable |
|---|---|---|
| Option pour désactiver le replay d'après match | petit | Gameplay |
| Changer de catcheur en plein match (touche dédiée, sans pause) | gros | Gameplay |
| Jukebox : activer / couper des chansons, playlist perso en musique du menu principal (comme le portage SvR 2011) | petit à moyen | Univers (jukebox) |
| Importer des images dans le Mod Maker | moyen | Mod Maker |
| Outils d'arènes personnalisées (plus de scènes, tailles, calques, extérieur) | gros | Mod Maker / Rendu |
| Ceintures personnalisées (éditeur de titres / Create a Championship) | gros | Mod Maker / Univers |
| Grandes émissions / PPV personnalisés dans l'Univers | moyen | Univers |
| Créations de la communauté (partage en ligne : CAW, arènes, logos, histoires, entrées, finishers, Match Creator) | très gros (serveur, modération, sécurité) | à cadrer |

### Idées de matchs et de gameplay (liste « Match Types » collée par l'utilisateur)
- Types de matchs : 2 vs 2 vs 2 (de SvR 2008), 6-Man Tornado Tag (+ Elimination, + Hardcore Triple Threat), King of the Ring, Beat the Clock Sprint,
  2 vs 2 Elimination Table / Extreme Rules Tornado Tag, 2 vs 3 Tornado Tag, Slobber Knocker, 3 Stages of Hell, Gauntlet Turmoil (sous-menu du nombre d'adversaires).
- Règles libres pour n'importe quel match (ex. 2 sur 3 Last Man Standing, HIAC / Inferno) + option du launcher « configurations sans restriction » (avec avertissement : le match peut planter).
- Gameplay : déclencher une interférence avec un bouton, spectateurs qui donnent des armes, monter sur les tables des commentateurs, afficher les commandes de chaque catcheur à l'écran, chronomètre visible dans tous les types de matchs.

### Liste courte du 1.x (rappel)
Lecture manette synchrone (#81), profil « petit PC », saignement des catcheuses, idées Univers (challengers, Légendes du Rumble, mois de départ : fait).
