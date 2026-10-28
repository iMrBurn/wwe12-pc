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

## 2. Finishers de départ (comme WWE 2K14) : demande corrigée

**La demande (corrigée par Main le 2026-10-28, d'après la capture d'un joueur)** : dans WWE 2K14, l'écran de
sélection des catcheurs a, à côté de ATTIRE, une ligne FINISHERS qui règle combien de finishers le catcheur a **dès
le début du match** (1, 2, 3 ou infini). Ce n'est pas le choix de la prise (étude gardée plus bas, §3).

**La jauge de momentum de WWE '12 (trouvée dans le code, à vérifier en jeu)** :
- Les commandes de script du jeu (Road to WrestleMania) ont des noms : table {nom, fonction} vers 0x8200C800,
  dont `SetMomentum` (`sub_823D5328`), `SetMomentumMax` (`sub_823D53D0`), `SetMomentumRate` (`sub_823D5460`),
  `GetFinisherNum` (`sub_823D3790`), `IsFinisher` (`sub_823D46D8`), `SetFinishIcon` (`sub_823D5F18`).
- Objet momentum d'un catcheur : `acteur = sub_8246D3E8(n)`, composant `sub_827D5C28([acteur+3436], 'CHP ')`,
  objet = composant + 24. Champs :
  - **+152 : la jauge (float, 0 à 1 ; 1 = pleine = finisher disponible)** ; lue par `sub_82246DE0`, écrite par
    `sub_82246E48(objet, valeur)` (SetMomentum passe le pourcentage × 0,01) ; `sub_82248218` = jauge ≥ 1 ;
  - +176 : écrit par SetMomentumMax ;
  - **+180 : mode ; 2 = la jauge est remise à 1 à chaque image (pleine pour toujours)**, 1 = un autre état forcé
    (fin de `sub_8224BB58`) : le « infini » existe donc déjà dans le moteur, pour les scripts ;
  - +188 : minuteur de l'état « finisher » (fixé quand la jauge arrive à 1, d'après une table par difficulté ;
    à 0, la jauge est vidée) ; +196/+200/+204 : autres minuteurs.
- Mise à jour par image : `sub_8224BB58(objet)`.
- **Finisher utilisé → jauge vidée** : `sub_82349648(acteur, objet, prise)` : si la jauge est pleine et que la prise
  est un finisher (méthode +200 de la prise), `sub_8224B008` puis `sub_822483F0` (jauge à 0, +188 = -1,
  acteur+2792 = 0). Rien de fait si l'octet acteur+998 est mis (sans doute FIRED UP, qui permet jusqu'à 3 finishers).
- `IsFinisher` = l'acteur est en train de faire un finisher (acteur+1804, +1816). `GetFinisherNum` = un compteur
  de statistiques (n° 33, `sub_82236AF8`), pas un stock.
- **Il n'y a pas de stock de plusieurs finishers dans WWE '12** : un finisher = la jauge pleine. Un « stock » de 2
  ou 3 se fait en ne vidant pas la jauge après les N-1 premiers finishers (crochet sur `sub_822483F0` appelé depuis
  `sub_82349648`, compteur par catcheur), comme FIRED UP.

**Ce qu'on ferait** :
- Au début du match (première image de `sub_8224BB58` du match) : jauge à 1 si FINISHERS ≥ 1.
- Après chaque finisher : jauge gardée pleine tant qu'il reste du stock ; INFINITE = mode +180 = 2 (ou jauge
  remise à 1 après chaque finisher).
- **Où le régler** : l'écran de sélection de WWE '12 est un écran du code (pas une page Lua comme nos options) ;
  ATTIRE y est une boîte de choix ouverte par un bouton (« CHANGE ATTIRE », texte 280), pas une ligne LB/RB.
  Deux choix :
  - **simple (1.0)** : une ligne « STARTING FINISHERS » (OFF / 1 / 2 / 3 / INFINITE) dans une page d'options native
    (comme JUKEBOX), pour tous les catcheurs, ou deux lignes joueurs / CPU ;
  - **par catcheur, sur l'écran de sélection (1.1)** : un bouton qui ouvre la boîte de choix du jeu, comme la tenue
    de l'Univers (`docs/univers.md` §15, écran des participants `sub_825ADD48`).

**Estimation** : stock et infini 2-3 séances (dont 1 essai en jeu avec l'accord de l'utilisateur) ; page d'options
+1 ; choix par catcheur sur l'écran de sélection +3-5.

**Risques** : minuteur +188 (la jauge pleine se vide seule au bout d'un moment : à garder pleine tant qu'il reste du
stock) ; FIRED UP (acteur+998) ; Royal Rumble, FINISHER MATCH, MOVE THIEF ; l'IA en infini ; Univers et Road to
WrestleMania (leurs scripts utilisent SetMomentum : ne rien changer quand un script a la main).

**Proposition** : 1.0 = stock / infini + ligne dans une page d'options (3-4 séances). 1.1 = choix par catcheur sur
l'écran de sélection. **Pour le squash (Limit breaking)** : même objet (jauge +152, `sub_82246E48`, SetMomentumRate).

### Compléments de lecture (2026-10-28, après la proposition 1.0)

- **Nombre de finishers par jauge pleine** : `sub_82248000(objet)` = **3** si l'octet global [0x8317C850+6] est
  mis (`sub_8267CF38()` + 6 : réglage ou triche, à identifier), **1** si l'acteur n'a pas de fiche de capacités
  ([acteur+3380] = 0), sinon `sub_82276A98([acteur+3380]+322)` (sans doute 3 avec FIRED UP, 1 sinon).
- **Compteur des finishers faits pendant la jauge pleine : acteur+2776** (+1 dans `sub_8224B008` tant qu'il est sous
  le maximum ; remis à 0 quand la jauge repart, fin de `sub_8224BB58`, et par `sub_822483F0` via acteur+2792).
- Dans `sub_82349648`, la jauge est vidée après **chaque** finisher, sauf si l'octet acteur+998 est mis (état actif,
  sans doute FIRED UP lancé par LB + Y ; plusieurs fonctions l'écrivent, à confirmer à l'essai).
- Donc le plus sûr pour un stock de N : après le vidage de `sub_822483F0` appelé depuis `sub_82349648`, si notre
  compteur par catcheur n'est pas épuisé, remettre la jauge à 1 (`sub_82246E48(objet, 1.0)`). On ne touche ni
  à acteur+998 ni à `sub_82248000` (FIRED UP garde son fonctionnement).

### Plan de l'essai en jeu (quand l'utilisateur aura donné son go)

**Étape 1, sonde seule (rien de changé dans le jeu)** : cvar `wwe12_finisher_probe` (désactivé par défaut), une
ligne `[finisher]` dans le journal **seulement quand une valeur change**, pour chaque catcheur du match :
jauge (objet+152, arrondie à 0,01), mode (+180), minuteur (+188), acteur+998, acteur+2776, résultat de
`sub_82248000`, et chaque appel de `sub_82349648` / `sub_822483F0` / `sub_8224B008` (avec le n° du catcheur).
Lanceur isolé `tools/dev/lancer_test_finishers.bat` (`--user_data_root "%USERPROFILE%\Documents\wwe12_test"`).

Ce que l'utilisateur fait (exhibition 1 contre 1, un match par scénario, ~10 min en tout) :
1. **Normal** : remplir la jauge, attendre sans faire le finisher → on lit si +188 finit par vider la jauge, et en
   combien de temps. Puis faire un finisher → vidage, compteur +2776.
2. **FIRED UP** : un catcheur qui a la capacité (à trouver dans la liste des capacités), jauge pleine, LB + Y, puis
   2 ou 3 finishers → on vérifie acteur+998 et le maximum 3.
3. **MOVE THIEF** : voler le finisher adverse → quelle jauge est vidée.
4. **Royal Rumble** (FIRED UP y est coupé) et **FINISHER MATCH** (MATCH CREATOR > WIN CONDITION) : un finisher
   chacun, on regarde si le chemin est le même.

**Étape 2, le réglage** (`wwe12_starting_finishers` : 0 = jeu d'origine, 1, 2, 3, 4 = INFINITE ; page d'options) :
- jauge mise à 1 au début du match (premier passage de `sub_8224BB58` pour chaque catcheur) ;
- stock : après chaque finisher, si le stock restant > 0, jauge remise à 1 et stock - 1 ; INFINITE : toujours ;
- si +188 vide la jauge alors qu'il reste du stock : la remettre à 1 (sinon le stock serait perdu en attendant).

Essais (un match chacun) :
| Essai | Attendu |
|---|---|
| 1 finisher | jauge pleine à la cloche, vide après le 1er finisher, puis le jeu normal |
| 2 finishers | 2 finishers de suite, vide après le 2e |
| INFINITE | toujours pleine, plusieurs finishers |
| FIRED UP + 2 | FIRED UP marche encore (jusqu'à 3 pendant l'état), le stock ne se perd pas |
| Royal Rumble 1 | chaque entrant arrive avec sa jauge pleine (entrée en cours de match : premier passage de son objet) |
| FINISHER MATCH 1 | la victoire au finisher marche |
| CPU contre CPU INFINITE | l'IA ne bloque pas (enchaîne ou non, pas de gel) |
| Univers / Road to WrestleMania | réglage ignoré quand un script règle la jauge (SetMomentum / mode +180) : à vérifier qu'aucun script n'est cassé |

**Risques à surveiller** : le minuteur +188 ; l'état FIRED UP (acteur+998) ; MOVE THIEF (jauge de l'autre
catcheur) ; les entrants du Royal Rumble ; l'IA en INFINITE ; les scripts (Univers, RTWM, didacticiel).

## 3. Choix libre des finishers (première lecture de la demande, gardée pour plus tard)

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
