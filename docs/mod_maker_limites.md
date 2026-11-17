# Mod Maker : limites du nombre de mods (état des lieux, 2026-11-17)

Étude seule (rien compilé, rien lancé), demandée par l'utilisateur via la Main pour l'après 1.4. Sources : `docs/mod_maker.md`,
`docs/prises_svr11.md`, `docs/limit_breaking.md`, `wwe12_superstar_mods.cpp`, `wwe12_arena_pages.cpp`, `star_port.h`.
Les heures sont des estimations à la louche, pas des promesses.

| Quoi | Limite actuelle | Cause | Augmentable ? |
|---|---|---|---|
| Catcheurs ajoutés | ~100 se chargent (test 2026-11-13 : 100 copies placées en 108-235), **64 visibles** à l'écran de choix (constat de la Main / des testeurs) | Places = fiches vides du disque 100-372, puis DLC 85-99 (notre code, `superstar_slots.txt`) ; les 64 = sans doute une table / grille de taille fixe de l'écran de choix (jeu ; **cause exacte pas encore trouvée**) | Oui à étudier : trouver la table de l'écran de choix. Étude 4-8 h, puis correction 4-10 h ; risque moyen (écran de choix, sauvegarde) |
| Fiches de catcheurs | 373 numéros au total (0-49 créés, 50-99 DLC, 100-372 disque) | Tables du jeu par numéro (fiches, sauvegarde à 286 o par catcheur) | Difficile : agrandir les tableaux de la sauvegarde = gros risque. Pas avant 2.0 |
| Packs DLC maison | 1 pack `WWE12PCMODS`, numéro ≤ 9 | Places de packs DLC du jeu | Inutile pour l'instant : un pack contient tous les mods |
| Arènes ajoutées | **35** (cases `CAR_dummyNN` de `MatchHD.pac`) et **12,5 Mo par arène** | Bannières : entrées libres du fichier du jeu (place dans un fichier). Taille : tas 12 du jeu (≈ 18,7 Mo, mémoire Xbox) | 35 → plus : ajouter des entrées de bannières (8-15 h, risque moyen). 12,5 Mo → plus : agrandir le tas 12 (étude 6-12 h, **risque élevé** : plantages au chargement) |
| Entrées seules (Créer une entrée) | Numéros libres 100-499 qui ont un groupe Nyujyo (plusieurs dizaines) ; **tag team : 13** (757-769) ; trios restent en solo | Numéros d'entrées du jeu et groupes Nyujyo*.pac (place dans les fichiers) | Tag team : prendre d'autres numéros libres (3-6 h, risque faible-moyen) |
| Vidéos (titantrons) | ~280 numéros libres (726-899, 551-615, 428-499, 373-399) | Notre code (numéros que ni le disque ni les packs n'ont) | Pas utile pour l'instant |
| Tenues | **4 par catcheur** | Table COS de `misc.pac` : 4 cases par ligne (format du jeu) | Difficile (format de ligne fixe, menu CHANGE ATTIRE). Étude 6-10 h, risque moyen-élevé |
| Prises ajoutées | Cases libres de chaque « centaine » (famille de prise) ; deux mods sur une case : le second déplacé | Numéros d'animation du jeu (la centaine = genre de prise) | Rarement atteint ; rien à faire |
| Thèmes | 1 par catcheur ajouté / mod de prises ; joués **seulement sur leur catcheur** (pas dans Créer une entrée) ; Vorbis ≤ 10 min | Paquet musique du pack DLC (jeu) ; dans Créer une entrée : dossier `music` (USER PLAYLIST, bouton du 2026-11-17) | Thème choisissable en CAE : 6-12 h d'étude du chargement des paquets musique DLC, risque moyen |
| Capacités | 5 par catcheur | Fiche du jeu | Non |
| CAW / logos | 100 / 200 places (jeu) | Sauvegarde | Plus tard, sans doute 2.0 (`docs/limit_breaking.md`) |

## Recommandation pour janvier

Premier pas : **trouver pourquoi l'écran de choix n'affiche que 64 catcheurs ajoutés.** C'est la limite que les joueurs
touchent (100 se chargent déjà), elle se règle sans toucher au format de la sauvegarde, et l'étude seule dira si c'est
une table à agrandir (faisable) ou un tableau de la sauvegarde (à laisser). Méthode : 100 copies de test (outil déjà
utilisé le 2026-11-13, `SaveBackups/pack_test_copies_2026-11-13`), journal de l'écran de choix, puis lecture du code qui
remplit la grille. Ensuite, s'il reste du temps : plus de 13 entrées tag team (facile), puis la taille des arènes (risqué).
