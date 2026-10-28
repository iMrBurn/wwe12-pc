# Moments OMG et choix libre des finishers : étude (2026-10-28)

Demande de l'utilisateur (via Main) pour la feuille de route 1.0. **Étude du code et des données seulement** : rien n'a
été compilé et le jeu n'a pas été lancé. Les données ont été lues hors jeu avec les petits outils déjà faits
(`movelist`, `wazerec`, `wazerule` : fiches PRO de `pac/ch/chEtc.pac`, fiches de prises de `pac/misc.pac` MOVS/WAZE).

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

## 2. Choix libre des finishers (comme WWE 2K14)

**Où WWE '12 range les finishers d'un catcheur** : fiche PRO de `pac/ch/chEtc.pac` (1 072 octets, nombres de 16 bits
gros-boutistes), **signatures à +0x022 et +0x024, finishers à +0x026 et +0x028** (Cena : Five Knuckle Shuffle,
Diving Five Knuckle Shuffle, Attitude Adjustment 4, STF). La sauvegarde garde sa propre copie de la fiche (1 068
octets) : c'est elle que CUSTOM MOVE-SET modifie.

**Ce qui limite le choix** : chaque prise de `misc.pac` MOVS/WAZE (fiches de 160 octets) commence par 16 octets de
drapeaux, un bit par liste de l'éditeur (le compteur de chaque liste est dans l'en-tête, u16 à 0x08 + 2 × bit).
**Les bits 2 et 3 (signature, finisher) sont mis sur les mêmes 751 prises** sur 1 937 prises nommées : c'est la liste
« SPECIAL MOVES ». Sur tout le roster, les finishers et les signatures viennent tous de cette liste, de banques
variées : au sol (33xxx), en course (42xxx), plongeons (45xxx), prises (50xxx-51xxx), coin / cordes (54xxx-56xxx),
soumissions (58xxx). Une prise ordinaire (ex. Punch to Head, drapeau 0x40 seulement) ne peut pas être choisie.
Aucun drapeau ne réserve une prise à un catcheur : **dans l'éditeur, n'importe quel catcheur peut déjà prendre
n'importe laquelle des 751 prises spéciales** (Create-A-Finisher en ajoute). À confirmer en jeu en une minute
(CUSTOM MOVE-SET sur un catcheur du roster, SPECIAL MOVES). Le message 50323 « This data cannot be edited » vise
sans doute des données téléchargées ; à vérifier au même moment.

**Ce qu'on peut faire pour la 1.0** : « toute prise peut être signature ou finisher ». Mettre les bits 2 et 3 sur
d'autres prises de WAZE et corriger les compteurs de l'en-tête, dans une copie de `misc.pac` servie par l'overlay
(comme `menu.pac`), avec un réglage `wwe12_any_finisher` (désactivé par défaut au début).

**Risques** :
- La touche du finisher joue la prise selon la position (debout de face, au sol, coin, course, plongeon). Une prise
  ordinaire mise en finisher doit être jouable dans la position où on la déclenche : il faut savoir quel champ de la
  fiche dit la position (sans doute vers +0x70..+0x8F, pas décodé) et n'ouvrir que les prises « de face, debout »
  d'abord.
- Dégâts : une prise ordinaire en finisher garde ses dégâts faibles (champs non décodés).
- Sauvegarde : un finisher ordinaire reste dans la sauvegarde si le réglage est coupé ensuite ; il faut vérifier que
  le jeu le joue quand même (il joue la prise par son numéro), sinon remettre le finisher d'origine au chargement.
- Commentaires, caméra de finisher, FIRED UP / MOVE THIEF : à essayer.

**Estimation** : 3-5 séances (lecture des champs de position, copie de `misc.pac`, réglage, 2 essais en jeu).

**Proposition** : 1.0 = la vérification en jeu (CUSTOM MOVE-SET permet déjà de changer les finishers de tous ? à dire
aux joueurs dans la FAQ si oui) + « toute prise peut être finisher » limité aux prises debout de face, réglage
désactivé par défaut tant qu'il n'est pas testé. 1.1 = les autres positions et les dégâts.
