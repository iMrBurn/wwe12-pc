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
