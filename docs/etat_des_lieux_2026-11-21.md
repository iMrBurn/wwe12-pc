# État des lieux et plan 1.5 / 1.6 / 2.0 (2026-11-21)

Écrit par la Main à la demande de iMrBurn. Rien ici n'est promis : c'est un plan de travail.

## État des lieux

| Sujet | État |
|---|---|
| Version publiée | 1.4.1 (PC + Android). Poussé sur GitHub : tout le code jusqu'au commit `42a0942`. |
| Gel « NOW LOADING » au démarrage | **Corrigé** (course dans le chargement des banques sonores, `wwe12_bank_race_fix`, 10/10 sans gel avec, 2/10 gels sans). Pas encore publié. |
| Vitesse sur petit PC (2 cœurs lents + iGPU Intel 720p) | ~58 fps en match, jouable (test de l'utilisateur 2026-11-21). Calcul du jeu 23 ms/image pour 16,7 ms de budget : à la limite. |
| Calculs natifs (déformation + demi-flottants) | Actifs par défaut, 0 différence vérifiée, ~4× plus rapides que l'original. |
| Compteur de fps | Refait (pastille centrée, vraies polices), validé. Température Android : non testée sur téléphone. |
| Univers | SmackDown 4/4 matchs (cause = titre vacant), « laisser vacant » retiré, réparation des repos. Testé en jeu. |
| Régénération du code (2.0) | Paliers 1 et 3 compilés. Palier 3 : −14 % de calcul en match, −9 % en entrées, images > 20 ms 100 → 61, exe 100,5 → 84,4 Mo. Les 4 crochets lus par le palier 3 sont prouvés en jeu (Univers). Un blocage vu 1 fois sur ~6 passes au chargement du match (non reproduit). |
| Android | APK 1.4.1 publié, « Share last log » corrigé (commit local, non testé). Retours : #85, #82-84 (journaux), PlayStation. |
| iOS | 1.4 non commencée (branche ios-13 à la 1.3). |
| Issues GitHub | 16 ouvertes ; réponses postées sur #69, #74, #81, #82-85. Beaucoup attendent un retour des joueurs. |
| Diagnostic du SDK | Sondes (gpu_wait, io_log, xam, sync, son, chargeur) incluses, éteintes par défaut. |

## Plan par version

### 1.5 « Petits PC » (prête à préparer)
| Élément | État | Reste à faire |
|---|---|---|
| Correction du gel au démarrage | fait | notes de version |
| Compteur de fps refait | fait | test téléphone (Android) |
| Calculs natifs par défaut | fait | — |
| Corrections Univers (SmackDown, titre vacant) | fait | — |
| Mesures petit PC | fait (58 fps) | à republier dans les notes |
| Android : « Share last log » | code écrit | build APK + test téléphone |
| Android : icônes / manette PlayStation | à éclaircir | détails du joueur |
| Version, notes (anglais), zip, vérification des fuites | à faire | Main |
| Publication | — | par l'utilisateur |

### 1.6 (idées rapides et sûres, par ordre d'intérêt)
| Idée | Pourquoi | Effort |
|---|---|---|
| Lecture manette synchrone sous Windows (#81 si confirmé) | petit retard de lecture ; ~15 lignes SDK | petit |
| Réglages textures / ombres / plafond de résolution | utile aux petits PC, vu chez d'autres créateurs | moyen |
| Bon GPU sur portable + message si le pilote plante | piège classique (mauvais GPU) | petit |
| Option « manettes = joueur 1 » | confort, vue chez d'autres | petit |
| Priorités des fils sur ≤ 4 fils, copie de textures plus économe | gagner sur les petits PC (1-1,5 ms) | moyen |
| Profil « petit PC » dans le launcher | réglages recommandés | moyen |
| Idées Univers rapides (saignement des catcheuses, challengers titres mineurs, Légendes du Rumble…) | demandes de joueurs triées | petit à moyen |

À vérifier avant de les lancer : plusieurs existent peut-être déjà (menu GRAPHICS dans le jeu, diagnostic, limite de fps).

### 2.0 (le gros chantier)
| Étape | État |
|---|---|
| Régénération palier 1 | fait |
| Palier 3 (non-volatiles en local) | compilé, mesuré, à valider en jeu |
| Retirer `volatile` | à faire |
| Palier 2 avec non-volatiles, 66 fonctions contrôlées | à faire |
| Blocage rare au chargement du match (palier 3) | à surveiller, non reproduit |
| Gros test de l'utilisateur (1 h 30 à 2 h, dernier palier qui tient) | à faire, par l'utilisateur |
| Option : retirer la couche D3D/PM4 du jeu | mesuré inutile pour les fps ; abandonné |

## Décisions en attente de l'utilisateur
1. Contenu exact de la 1.5 (ajouts ?).
2. Quand préparer et publier la 1.5.
3. Quelles idées des autres créateurs passent en 1.6.
