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
  `yanm::Read`**. **Mêmes os (Biped : Bip, Pelvis, Spine, Spine1, Neck, Head, bras, jambes, `vector`, `root`)** :
  banque FINI, 76 fiches de WWE '13 contre 58 de WWE '12 ; dans la moitié des fiches de WWE '13 les pistes des mains
  et des orteils manquent (20 pistes au lieu de 24 : ces os restent au repos, ou viennent des pistes `#fdd95b2x` /
  `#fc5bf1xx` propres à WWE '13, sans doute la cible IK dont parle Kaiko). Les clés sont des mêmes sortes (rot8,
  charnières, positions). → **une copie presque telle quelle des animations de WWE '13 semble possible** (à essayer
  en jeu : une prise de WWE '13 mise sur un numéro de WWE '12) ; SvR 2011 (YMKs) et les 2K demanderont une vraie
  conversion.
- **Ce qui manque pour l'étape 3, dans le jeu** : un « pack de prises » (l'équivalent de `move_packs.cpp` de Kaiko,
  hors de ses outils) : ajouter des fiches ZMVM à `m.pac` (270 Mo : pas de copie, une redirection d'entrées comme
  pour les vidéos d'arènes) et cloner les fiches WAZE de `misc.pac` (petit : copie patchée par l'overlay, comme
  `menu.pac`) ; les autres données d'une prise (dégâts, contres, événements) ne sont décodées nulle part : on part
  d'une prise « modèle » de WWE '12 du même genre (méthode de Kaiko).
- **Une prise de WWE '12, vue de près** (2026-10-27, outils `mtree` / `wazerec` du bloc-notes) :
  - `misc.pac` n'a que MOVS/WAZE pour les prises (les autres entrées : BATS/BATH INIT = tables du jeu STG, COS,
    RUL, OPT…, CMAI, RVSL) → **les données d'une prise sont sa fiche WAZE de 160 o** : +0x00 genre (0x0C), +0x03
    catégorie ?, +0x0E/+0x0F drapeaux, nom +0x10, réglages +0x70..+0x8F (dégâts, contres ?), numéro u16 +0x90 (AA 4
    = 0x96C4, STF = 0xEEE3), puis FF.
  - `m.pac` ZMVM/FINI : PACH → 112 → 20 sous-groupes (0, 1, 2…) → **numéro d'animation = numéro de la prise** (fiche WAZE +0x90 lue en petit-boutiste : 33002 = « Scissors kick » de
    Booker T, prise DLC ; Attitude Adjustment 4 = 0xC496 = 50326, trouvée dans SPE1 et STN1) ; le
    sous-groupe 0 (10 Ko) et le 1 (35 Ko) ont les mêmes blocs (deux qualités, ou corps / mains-visage ?). Blocs
    `PPPPWWWW` : phase, personne (0000/0001), jusqu'à 0x0011xxxx et 0x0021xxxx (contres ?). Nos outils lisent ces numéros en gros-boutiste dans WAZE comme dans PRO : cohérent entre eux, mais
    le vrai numéro (celui des animations) est l'autre sens.
- **Mesures 2K** (2026-10-27) : 2K14 / 2K15 / 2K16 lus (noms dans `createmode.pac`) ; 2K15 : 79 % / 8 % / 13 % ;
  2K16 : 84 % / 9 % / 7 % (moins de cases mises en correspondance : leur moveset est plus réorganisé). 2K17 non lu.
- **WWE '13 et les 2K gardent la numérotation des prises de WWE '12** (le numéro = celui de l'animation), certaines
  renommées : « White Noise 1 » de WWE '13 = « Celtic Cross 1 » de WWE '12 (animation 50024, mêmes blocs, 526
  images). `moveset_port` prend donc aussi le même numéro quand le nom manque, pour un jeu qui partage la
  numérotation (même numéro dans la même case pour plus d'un tiers des catcheurs communs ; SvR 2011 non).
  Cases gardant la base : WWE '13 12,3 → 11,6 %, 2K15 13 → 10 %. Les prises qui manquent encore sont de vraies
  nouveautés (Vertical Suplex, Beast Bite, Saito Suplex…) : étape 3.
- **Fiches WAZE de WWE '13 contre WWE '12** (STF, même numéro 0xEEE3) : nom +0x10, réglages +0x70..+0x8F proches
  (STF : +0x7C `02 00 01` en WWE '12, `0A 00 05` en WWE '13), numéro +0x90 identique ; **les 16 premiers octets sont
  rangés autrement** (WWE '12 `0C 00 00 01 … 84 00` : genre +0x00, drapeaux +0x0E ; WWE '13 `… 08 01` vers +0x07)
  et WWE '13 a des octets après le numéro (+0x98…) où WWE '12 a des FF. Une nouvelle prise de WWE '13 (ex. Vertical
  Suplex, 0xC520) se recopiera avec ces 16 octets traduits (correspondance à apprendre sur les prises communes).
  **Mesuré sur 1 781 prises communes (même numéro et nom, outil `wazefit`)** : +0x70..+0x97 identiques à 97-100 % ;
  +0x01..+0x0E identiques en place à 83-98 % (STF était une exception) ; seuls +0x00 (43 %), +0x03 (71 %), +0x04
  (66 %), +0x0B (66 %), +0x0C (69 %), +0x0F (59 %) changent souvent (genre / drapeaux revus entre les jeux) ;
  +0x98..+0x9F toujours FF en WWE '12. → recopier la fiche de WWE '13 telle quelle avec +0x98.. = FF ; les 6 octets
  variables : garder ceux de WWE '13, ou mieux, les prendre d'une prise de WWE '12 de même genre (à essayer en jeu).
- **Plan concret de l'étape 3 pour WWE '13 / 2K** : (1) fiche WAZE recopiée (16 premiers octets traduits) ajoutée à
  `misc.pac` (copie patchée par l'overlay, comme `menu.pac`) ; (2) ses animations (même numéro, mêmes banques et
  sous-groupes que dans WWE '13) ajoutées aux arbres ZMVM de `m.pac` : servir une copie de la banque touchée
  (1 à 16 Mo) par une redirection d'entrée comme pour les vidéos d'arènes ; (3) vérifier que ce numéro est libre en
  WWE '12 ; (4) les mains / orteils absents restent au repos. SvR 2011 (YMKs, autre numérotation) viendra après.
- **Notes publiques de Kaiko lues le 2026-10-27** (téléchargées avec l'accord de l'utilisateur dans le bloc-notes de
  la session, hors dépôt : `port/docs/MOVE_PACKS.md`, `OTHER_GAMES_ENTRANCES.md`, `docs/ARENA_MOD_MAKER_PLAN.md`,
  `docs/OTHER_GAMES.md` de KaikoClanworth1/wwe-svr2011-pc) :
  - **Les 16 premiers octets d'une fiche WAZE = les bits de catégorie qui décident dans quelles cases du moveset la
    prise peut aller** (d'où les 6 octets qui changent entre WWE '13 et WWE '12 : les prendre d'une prise de WWE '12
    du même genre).
  - **Son chargeur de packs de prises (SvR 2011, même famille)** : au démarrage, chaque pack est fusionné dans des
    **copies** de `m.pac`, `misc.pac` (et `mpsp.pac`) dans un dossier overlay (entrées inchangées recopiées, entrées
    changées reconstruites et recompressées en BPE ; `stamp.txt` pour ne refaire la fusion que si quelque chose
    change, ~3 s) ; la liste des pacs du jeu (copies de `plist360.h`) nomme les copies ; comme le répertoire
    précalculé `plist360.arc` ne connaît que les originaux (entrées agrandies = plantage ou blocage au chargement
    d'un match), le jeu monte alors les pacs en lisant leur propre table (chez lui `sub_825953B0` au lieu de
    `sub_82595428` : **adresses de SvR 2011, à retrouver dans WWE '12**). DLC avec leur propre `m_dlcNN.pac` :
    fusionner aussi dans des copies, servies par liens au montage, catalogues recopiés aux nouvelles places.
    Pièges : les motions YMBs se gardent par leur place dans la banque (en ajouter après les existantes) ; la fin
    d'une piste de victime (op 49) nomme la motion suivante.
  - **Entrées (étape 4) : WWE '13 → WWE '12 devrait être presque une copie** : WWE '12 = `pac/evt/Nyujyo0-9,A.pac`,
    groupes EVP (script, disposition de SvR 2011 mais positions en cm) et EVZ (pistes YANM + YAT2 à 60 Hz, corps en
    FK) ; WWE '13 = `pac/evt/Entrance00..08.pac`, **même script en cm et mêmes pistes** (317 entrées, dont des
    légendes sur des numéros libres) ; PRO : WWE '12 +0x1D4 musique, vidéo, entrée, autre, +0x1DC champion ; WWE '13
    +0x230. Accessoires : à vérifier dans `evtobj.pac` ; pyro `prtcls.pac` même format ; titantrons de WWE '13 en
    Bink 2 (déjà convertis par le Mod Maker pour les 2K) ; thèmes Wwise `Music.pck`. SvR 2011 / 2K → WWE '12 : il
    faudra recibler (méthode `w13_retarget` de Kaiko, à l'envers).
- **Le chargeur de WWE '12 existe déjà chez nous** (2026-10-27) : le jeu trouve chaque entrée par son répertoire,
  `sub_827F29F0(répertoire, "/GROUPE/NOM", out)` (out : début en secteurs de 2 Ko, taille en unités de 256 o, numéro
  de fichier à +12), et `native/wwe12_arena_pages.cpp` le **crochète déjà** pour les arènes ajoutées : des données
  ajoutées au bout d'une copie servie d'un pac (MatchHD, evtgdhd, LoadHD, evtmov), le répertoire répondant avec leur
  place. C'est notre équivalent du montage par table propre de Kaiko (`sub_825953B0`), sans toucher au répertoire.
  → Étape 4 : une entrée de WWE '13 (EVP + EVZ) ajoutée au bout d'une copie servie de `Nyujyo*.pac`, le crochet
  répondant à ses `/EVPx/0NNN` et `/EVZx/0NNN` (même pour un nom absent du répertoire : à vérifier), la fiche PRO
  (+0x1D4) pointant sur ce numéro. → Étape 3 : la banque ZMVM touchée (avec la prise ajoutée) au bout d'une copie
  servie, et la fiche WAZE dans une copie de `misc.pac`. Pièges connus du même fichier : le jeu lit parfois plus que
  la taille donnée (un bgNN complet est servi), et la vidéo ne marchait pas servie depuis un autre fichier.
- Étape 4 : pas commencée (voir juste au-dessus : commencer par WWE '13). **Vérifié le 2026-10-27** (outil `evtlist`) :
  les `Entrance00-08.pac` de WWE '13 (xcompressés) **se lisent avec notre `XcompressDecode`** ; WWE '12 a 247
  entrées (`Nyujyo0-9,A`), WWE '13 305 (sans les variantes 10000+), 207 en commun, **98 entrées de WWE '13 sur des
  numéros libres dans WWE '12** (104, 114, 124, 145, 147, 152, 166, 177, 212, 238, 245 Cactus Jack, 246 Mankind,
  247, 271, 294-297, 302-321 Attitude Era, 344-358, 423-427, 450-455, 530, 531, 550, 564-579…). **Entrées communes
  comparées** (`evtcmp`) : Cena 139, EVP « 0FOP » 89 908 o (WWE '12) / 85 304 o (WWE '13), **93 % des octets
  identiques** ; EVZ = PACH de 20 / 19 pistes ; Orton 161 : EVP 93 % identiques, EVZ 17 / 18 pistes. Même format
  confirmé. Prochaine étape : mettre une entrée de WWE '13 sur un numéro libre (ex. Mankind 246) dans une copie de
  `Nyujyo*.pac` servie par l'overlay, la fiche PRO du catcheur pointant dessus, et la regarder en jeu.
- **Écrans VS** (demandés le 2026-10-27) : les outils de Kaiko sur le bureau ne font que le fond
  (`arena_port.py` VS_BACKGROUND). `VS_STYLES.md` / `vs_port.py` **ne sont pas dans son dépôt public**
  (KaikoClanworth1/wwe-svr2011-pc, arbre complet lu le 2026-10-27 avec l'accord de l'utilisateur). Sa méthode est dans
  `docs/ARENA_MOD_MAKER_PLAN.md` (« New arenas and custom VS screens ») : une arène ajoutée garde le thème VS de sa
  base (`MatchHD.pac` M<nn>I, DXT5, non tuilées en mémoire, mots de 16 bits inversés) ; **pendant qu'elle est
  choisie, un fil de fond (`arena_mods.cpp` VsLoop) retrouve ces textures en mémoire par leur contenu et écrit les
  images du mod dessus ; les originales reviennent quand on choisit une autre arène**. → Piste pour notre défaut des
  bases partagées (le jeu ne recharge pas l'écran VS si le numéro d'arène ne change pas) : ne plus compter sur le
  rechargement, réécrire les textures en mémoire au choix de l'arène (nous avons déjà des crochets de textures,
  `wwe12_buttons.cpp`, et le choix d'arène). À essayer en jeu.
