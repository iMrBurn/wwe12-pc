# omg_finishers.md : anciennes parties (archivées le 2026-11-06)

Sorties de `docs/omg_finishers.md` lors du ménage des documents. Gardées telles quelles.

<!-- Moments OMG (abandonnés) -->

## 1. Moments OMG de WWE '13

**Dans WWE '13** : pendant le match, quand la jauge le permet (un « OMG » en réserve, gagné comme un finisher), le
joueur déclenche une action spéciale liée à un endroit : casser la barricade avec un spear, passer à travers la table
des commentateurs, effondrer le ring avec un superplex, attraper l'adversaire en plein vol (finisher « catch »). Chaque
moment a ses animations, une caméra et parfois un ralenti, et des objets du décor qui cassent.

**Ce que WWE '12 a déjà** :
- **Moments de replay** : `sub_821EEE58(ctrl, sorte, r5)` note un moment (puis `sub_821EE520` le range dans 20 cases
  de 20 octets à (*0x8317C84C)+128+6200 ; sonde « [replay] » de `wwe12_game_log.cpp`). **34 appels** dans le code,
  sortes 1, 3, 4, 10, 11 (13 appels, sans doute les finishers / signatures), 12, 15, 21, 23, 24, 27-32. C'est ce qui
  remplit MATCH HIGHLIGHTS. Les noms des sortes ne sont pas encore décodés (une partie de sonde suffit : chaque
  sorte loguée avec ce qui vient de se passer).
- **Caméra** : `docs/camera.md` (CameraAIDirectorThread, `sub_821D8430` → `sub_821D8040`). Les plans des prises et
  des finishers sont dans les données des prises, pas dans ce fil.
- **Décor cassable** : les tables (match TABLE, FLAMING TABLE). Pas de barricade cassable : les morceaux de coin
  `ar_fence01/02_c/f/g/h` existent dans les arènes de **WWE '13** (voir `mod_maker.md`), placés par son code à lui.
- **Ralenti en direct** : pas trouvé. Le replay a un ralenti ; en match, rien de connu.

**Ce qui manque pour de vrais moments OMG** : les animations (deux catcheurs, synchronisées avec le décor), le
déclencheur (endroit + jauge + bouton), le décor qui casse (barricade, ring), la caméra. Les animations de WWE '13
se lisent telles quelles (`plan_movesets.md` : mêmes os, mêmes banques), mais il faut d'abord savoir **ajouter des
prises** au jeu (étape 3 de `plan_movesets.md`, pas commencée). Le ring qui s'effondre demande un modèle cassé et
son changement en plein match : rien de connu là-dessus.

**Estimation** :
| Version | Séances | Risques |
|---|---|---|
| Sonde : nommer les 20 sortes de moments | 1-2 | faible (une partie de jeu) |
| « Moment » léger : après un gros moment déjà connu du jeu (finisher à travers une table, du haut de la cage), un replay immédiat au ralenti, avec le système de replay existant | 4-8 | moyen : relire un replay en plein match n'existe pas dans WWE '12 ; synchronisation, son, fin de match |
| Vrais moments OMG de WWE '13 (barricade, table des commentateurs, catch) | 15-30, **après** l'étape 3 des prises | fort : animations à deux, décor cassable, déclencheurs, caméras |
| Ring qui s'effondre | inconnu (> 30) | très fort |

**Proposition** : rien pour la 1.0. Pour la 1.1 : la sonde (1-2 séances) puis, si l'utilisateur le veut, le moment
léger. Les vrais moments OMG passent après les prises ajoutées (étape 3), sans doute en 1.2 ou plus tard.
