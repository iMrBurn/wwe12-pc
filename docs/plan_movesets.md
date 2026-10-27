# Prises et entrées des catcheurs portés : plan (2026-10-27)

Demande de l'utilisateur (« gros chantier, je veux une super 1.0 ») : un catcheur porté d'un autre jeu (SvR 2011,
WWE '13, 2K14-2K17) garde **son moveset et son entrée**, comme le fait le Mod Maker de SvR 2011 de Kaiko (autorisation
de lire et d'adapter son code, avec crédit). Aujourd'hui il prend ceux de sa base WWE '12.

Source : lecture des outils Python de Kaiko (`Desktop\SVR 2011 KAIKO\Mod Maker Tools` : `moveset_port.py`,
`moves_port.py`, `entrance_port.py`, `wwe13_entrance.py`, `moveset_maps.json`, `pro_slots.json`, `superstar_port.py`).
Son sens est autre jeu → SvR 2011 ; il **lit** les formats de WWE '12 (source), il ne les écrit pas.

## Ce qu'on sait de WWE '12 (d'après son code, à vérifier chez nous)

- **Fiche de prises** `pac/ch/chEtc.pac` CHAR/PRO, 1 072 o par catcheur, u16 gros-boutistes. Prises de +0x00A à +0x1C0
  (220 cases, même ordre que SvR 2011 décalé de +4 octets : ring, provocations, signatures, finishers, Rumble, échelle,
  coups, prises avant / arrière, au sol, coin, plongeons, apron, course, Irish whip, équipe, plongeons dehors,
  contres, soumissions). **Entrée à +0x1D4** : musique, vidéo, entrée, autre (4 × u16). La sauvegarde garde sa copie
  (1 068 o, déjà refaite pour les catcheurs ajoutés : `wwe12_superstar_mods.cpp`).
- **Noms des prises** : `misc.pac` MOVS/WAZE, fiches de 160 o depuis 0x430, nom à +0x10, numéro u16 à +0x90.
- **Animations** : `m.pac` ZMVM (déjà lu par la visionneuse d'animations, `motions.h`), YANM. Les autres champs des
  fiches de prises (dégâts, contres, événements) ne sont décodés nulle part.
- **Entrées** : `pac/evt/Nyujyo0-9,A.pac`, groupes EVP (script, même disposition que SvR 2011, cm) et EVZ (pistes
  YANM par objet). Pyro : `prtcls.pac` PCE6/LPE6, même format que SvR 2011. Thème : `sound/Music.pck`.
- **Correspondance des cases** entre jeux : `moveset_maps.json` (cases du jeu source → SvR 2011 ; WWE '12 ≈ SvR 2011
  + 4 octets) : on obtient WWE '13 / 2K → WWE '12 en composant avec l'inverse de la table « wwe12 ».

## Étapes

1. **Moveset traduit par le nom des prises** (pas d'animation portée). Lire la fiche PRO du catcheur source, passer
   chaque case dans la case WWE '12, trouver la prise de WWE '12 du même nom (nom normalisé) qui a une animation et
   que le roster de WWE '12 met dans cette case ou le même groupe (un coup ne va pas dans une case de prise) ; sinon
   la prise de la base reste. Le mod garde la liste des cases changées ; le jeu l'applique à la fiche du catcheur
   ajouté (et à la copie de la sauvegarde). Rapport dans le Mod Maker : chaque case, prise trouvée ou gardée.
   Choix séparés « prises de… » / « entrée de… » parmi les catcheurs de WWE '12 au passage.
2. **Entrée par le nom** : l'entrée (+0x1D4) d'un catcheur de WWE '12 choisi, ou trouvée par le nom du catcheur
   source quand WWE '12 l'a ; sa musique et sa vidéo suivent (la vidéo portée existe déjà).
3. **Prises absentes de WWE '12, animations portées** : écrire du YANM (aucun encodeur chez Kaiko), recibler vers le
   squelette de WWE '12 (FK, comme ses Biped), copier les fiches d'une prise « modèle » de WWE '12 sur un numéro libre,
   et un chargeur de « packs de prises » dans le jeu (l'équivalent de son `move_packs.cpp`, absent de ses outils).
   Raccourci à essayer : WWE '13 / 2K14 ont le même conteneur que WWE '12 (copie directe, avec un flux IK que WWE '12
   n'utilise pas). Gros travail de rétro-ingénierie.
4. **Entrées portées** (script EVP, pistes EVZ en YANM, caméras, pyro) : après l'étape 3 (même encodeur).

Les étapes 1 et 2 sont sûres et utiles seules ; 3 et 4 sont le vrai gros chantier.

## État (2026-10-27 au soir)

- **Étape 1 faite pour SvR 2011 et WWE '13** (`port/src/modmaker/moveset_port.h`, commits ed437af, 76fb312) :
  NEW SUPERSTAR, étape 2, case « Ses propres prises de … » (cochée par défaut) et détail case par case ; ligne
  `moves=OFF:NUMÉRO,...` du mod ; le jeu les met sur la fiche PRO du catcheur ajouté (`superstar_pack.h`), la copie
  de la sauvegarde suit. Correspondance des cases calculée à partir des catcheurs des deux jeux (paires de cases dont
  les prises ont le plus souvent le même nom, dans l'ordre, au moins 15 % : la méthode de `moveset_maps.py`) ; les
  fiches de noms trouvées où elles commencent (WWE '13 : pas à 0x430). Vérifié hors jeu : CM Punk de SvR → G.T.S.,
  Shining Wizard, Tiger Bomb, ses provocations ; de WWE '13 → G.T.S., Anaconda Vise ; Drew de SvR → Future Shock.
  **Pas encore vu en jeu.** Sur 223 catcheurs SvR, ~78 % des cases trouvent leur prise.
- **Étape 2 faite** (commit 1d9d064) : « Entrée (musique, vidéo, entrée) » = celle de la base ou d'un autre catcheur
  de WWE '12 ; ligne `entrance=NUMÉRO` ; le jeu recopie PRO +0x1D4..+0x1DE de ce catcheur (nombres de 16 bits
  **petit-boutistes** ici, alors que les prises sont gros-boutistes : Cena 139, 139, 139, 602, 602). Pas vu en jeu.
- **2K14-2K17 : pas encore.** Noms dans `createmode.pac` MOVS/WAZE (nom d'entrée avec des octets en plus après
  « WAZE ») ; la fiche PRO de 2K14 ne s'ouvre pas en arbre (0 enfant) ; 2K16/2K17 : tables à entrées de 20 octets et
  HSPC chez Kaiko (`moves_port.py` Game14, Game2K16, Game2K17 ; `superstar_port.py` 1084-1224).
- **Ce que l'étape 3 apporterait** (mesuré hors jeu, `moveset_port::FromGame` sur tout le roster) : SvR 2011, 222
  catcheurs, cases mises en correspondance : 83 % même nom, 12 % autre version, **5 % gardent la base** (220 prises
  distinctes manquent : Five Star Frog Splash, Rolling Thunder, Quake, Book of Ezekiel, Texas Piledriver…) ;
  WWE '13, 273 catcheurs : 84 % / 4 % / **12 %** (291 prises : Beast Bite, White Noise, Saito Suplex, Vise Grip,
  Glam Slam Stretch…).
- **Animations de WWE '13 : lisibles telles quelles.** Pas dans `m.pac` (30 Ko chez lui) mais dans `pac/mot06-08.pac`
  (EPK8 en clair dans la copie de l'utilisateur ; `dlc_mot01.pac` est xcompressé), mêmes banques ZMVM qu'en WWE '12
  (STAT, BASE, STN1/2, GRND, CORN, FINI, SPE0-2, RUMB, LADR, CELL… plus ABL0/1, LIMB) : **100 % des fiches lues par
  `yanm::Read`**. Différence : **20 pistes par personne en WWE '13, 24 en WWE '12** (Kaiko : WWE '13 a un flux IK,
  WWE '12 du FK pur) → copier tel quel ne suffira pas, il faut recalculer les pistes de WWE '12 (le visionneur
  d'animations du Mod Maker sait déjà les échantillonner).
- **Ce qui manque pour l'étape 3, dans le jeu** : un « pack de prises » (l'équivalent de `move_packs.cpp` de Kaiko,
  hors de ses outils) : ajouter des fiches ZMVM à `m.pac` (270 Mo : pas de copie, une redirection d'entrées comme
  pour les vidéos d'arènes) et cloner les fiches WAZE de `misc.pac` (petit : copie patchée par l'overlay, comme
  `menu.pac`) ; les autres données d'une prise (dégâts, contres, événements) ne sont décodées nulle part : on part
  d'une prise « modèle » de WWE '12 du même genre (méthode de Kaiko).
- Étape 4 : pas commencée.
