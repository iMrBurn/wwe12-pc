# Prises de SvR 2011 ajoutées à WWE '12 : étude (2026-11-03)

But : qu'un catcheur porté de SvR 2011 (puis 2007-2010) garde aussi les prises que WWE '12 n'a pas (finishers,
signatures...), comme on le fait déjà pour WWE '13 (`port/src/modmaker/newmoves13.h`). Aujourd'hui une telle case
garde la prise de la base. Rien n'est encore codé ; ce document note ce qu'on a vu.

## Où on en est pour les movesets (fait)

- Moveset traduit par le nom des prises : SvR 2011, WWE '13, 2K14-2K17 (`moveset_port.h FromGame`), et depuis le
  2026-11-03 SvR 2007-2010 (`FromOld`, tables de places de Kaiko dans `svr_old_slots.h`). Jeff Hardy (SvR 2010) :
  76 cases sur 157 ; vu en jeu (Elbow Smash 5).
- Prises ajoutées (animations) : WWE '13 seulement.

## Une prise dans WWE '12

- Fiche : `misc.pac` MOVS/WAZE, 160 octets (nom +0x10, numéro u16 +0x90).
- Animations : `m.pac` ZMOT/CA01..CA08 (les DLC : CA09, CA10 ; nos mods : CA11+), arbre PACH : un id de tête, puis
  des enfants par paires (enfant pair = fiches « légères », impair = « complètes »), chacun un PACH d'animations par
  numéro. Numéro d'animation = numéro de la prise aux octets inversés (`newmoves13::MotionOf`).
- Une fiche d'animation (Arm Drag 1, n° 37572, animation 50322, ZMOT/CA01 tête 56, paire 6/7 ; légère 3 040 o,
  complète 11 776 o) : un en-tête de 0xC0 octets, puis **deux blocs YANM** (sans doute l'attaquant puis la victime),
  chacun suivi de POF0 et d'un bloc **YAT2** (attributs ou événements, avec un tableau « AABB BFBB BBBB... »).
  YANM = le format que la conversion des entrées SvR produit déjà (`svr11_entrance.h`).

### L'en-tête d'une fiche d'animation WWE '12 (gros-boutiste, vu sur Arm Drag 1, Back Suplex 2, Pedigree 1)

| Offset | Valeur | Sens (probable) |
|---|---|---|
| +0x00 | 0xA0 / 0x190 | taille de la table (en-tête sans les 0x20 premiers octets) |
| +0x04 | taille | taille des données |
| +0x08 / +0x0C | 0x20 / 0xC0 (0x1C0) | début de la table / du premier YANM |
| +0x20 | 0x00020001 (Pedigree 0x00990001) | ? |
| +0x24 | « root » | |
| +0x48 | 2 (Pedigree 8) | nombre d'entrées |
| +0x4C | « 51036 » (texte) | numéro de l'animation |
| puis 0x28 o par entrée | {0xFF, u24 images ; « xxxx » ; « yyyy » ; ...} | une piste : x et y en texte |

Entrées : x « 0000 » y « 0000 » = attaquant, y « 0001 » = victime ; le Pedigree a aussi x « 0090 », « 0091 »,
« 0092 » (y 0 et 1) : **les mêmes clés que SvR 2011** (x90 / x91 = branches de contre). Durées : Back Suplex 2
198 / 191 images (SvR 2011 : 107 / 107), Arm Drag 1 197 / 165, Pedigree 1 366 / 364 (SvR 2011 : 226 / 225) :
WWE '12 compte environ deux fois plus d'images (60 / s contre 30 ?). Un YANM (+ POF0 + YAT2) par entrée, dans
l'ordre de la table.

782 prises ont le même nom dans les deux jeux avec leurs animations dans SvR 2011 (MOT/BM00, y0 et y1) : de quoi
valider la conversion (cas d'étude : Back Suplex 2, SvR 5377 / WWE '12 23751 ; Pedigree 1, SvR 5136 / WWE '12 31940).

## Une prise dans SvR 2011

- Fiche : `misc.pac` MOVS/WAZE, 160 octets, numéro u16 petit-boutiste +0x90 (Arm Drag 1 = 4809).
- Animations : `m.pac`, banques YMKs (lecteur : `svr11_entrance::detail::ReadBank`), clés (id, x, y) :
  y0 attaquant, y1 victime, y50 / y51 / y57 caméra et objets, x90 / x91 branches de contre. La même prise est dans
  plusieurs familles : MOT/BM00/n (le jeu complet), MOT/RRnn (sources des banques de match), MVMT/<catégorie>/n
  (STN2, SPE1...). Exemple : 4812 (y0 201 images, y1 172 images).
- **Arm Drag 1 (4809) n'a pas d'animation dans SvR 2011** : le jeu garde des fiches de prises supprimées, catégories
  effacées (noté par Kaiko). Pour comparer, prendre une prise présente dans les deux jeux avec ses animations.

## Premier essai de conversion (2026-11-03, outil d'étude hors dépôt)

SvR 2011 (MOT/BM00, y0 / y1) converti par la conversion des corps des entrées (`BodyKeys` + `BodyBlock`, modèle de
pistes = les blocs de la prise WWE '12, lus par `yanm::Read` : les YANM des prises sont « à gabarit », refusés par
`ParseRaw`), comparé os par os à la prise WWE '12 (angle entre rotations locales, 22 os « Bip ») :

| Prise | Images (SvR → WWE '12) | Attaquant | Victime |
|---|---|---|---|
| Back Suplex 2 | 107 → 200 (200) | 30,0° | 30,6° (sans doute refaite) |
| Pedigree 1 | 226 → 448 (448) | 17,8° | 9,1° |
| Tiger Bomb 3 | 148 → 292 (292) | 16,2° | 9,8° |
| Arm Wrench Slap | 117 → 302 (302) | 19,2° | 8,6° |
| Throat Thrust 3 | 51 → 169 (169) | 19,0° | 10,2° |

La durée tombe pile (2 × images SvR, comme pour les entrées). L'écart n'est pas un décalage constant (il change le
long de la prise). Bassin et orteils < 2° ; les plus gros écarts : Bip (racine), bras, cuisses. Pour les entrées,
LA Max mesurait 0,5-3,6°. À chercher : la racine (Bip) et le repère des prises, puis bras / cuisses (l'attaquant
est toujours moins bon que la victime).

### Ce qui fait l'écart (affiné le même jour)

- **La racine de l'attaquant** : dans les prises WWE '12, son Bip est tourné d'un demi-tour autour de l'axe
  vertical et placé ~155 unités plus loin en z (Pedigree, Tiger Bomb, Arm Wrench Slap : axe (0, -1, 0), 167-180°).
  Corrigé dans l'outil de mesure : q' = (0, -1, 0, 0) × q, p' = (-x, y, -z - 155) → racine 1,5-3,8° d'écart moyen,
  position à quelques cm (dérive jusqu'à ~25 cm vers la fin d'Arm Wrench Slap). La victime n'a pas ce décalage.
- **Jambes, colonne, cou, tête, clavicules** : 0,5-5° (comme les entrées). Avec la racine corrigée, l'attaquant
  passe de 16-19° à 8-11° en tout.
- **Avant-bras et mains** : ~20-40° os par os, mais **avant-bras × main ensemble** 3,5-15° (un cas à 32°) : la
  torsion est répartie autrement. Les gabarits WWE '12 font de l'avant-bras une **charnière à un axe** (0x0800) :
  toute la torsion va dans la main. **Essayé** : l'avant-bras converti réduit à sa rotation autour de l'axe de la
  charnière WWE '12 (axe 1 ; « twist » de la décomposition swing-twist), le reste multiplié dans la main
  (main' = reste × main) : avant-bras 1,6-16° (au lieu de 22-39°), main 2,4-26° (au lieu de 19-45°). À reprendre
  dans la vraie conversion des prises.
- Back Suplex 2 (30°) : sans doute refaite pour WWE '12.

## Premier essai en jeu (2026-11-03 au soir)

Outil `makemove` (sources d'étude dans `tools_local\moves_study`, hors dépôt) : Side Effect (SvR 2011, n° 5233) sur
le modèle Swing Neckbreaker 1 de WWE '12 ; numéro 59645 (animation 65000) ; fiches légère / complète du modèle,
blocs x 0000 y 0000 / 0001 remplacés (YANM + POF0), le reste gardé (YAT2, branches de contre 0090-0092) ; fiche
WAZE du modèle, nom et numéro changés. Installée par un mod Moves de test pour John Morrison (place 0xAA,
Front Facelock LS haut + A ; la place 0x7E n'est pas dans les menus de prises enchaînées de WWE '12) : le portage
l'ajoute (« 1 added ») et elle apparaît dans le jeu. **En match : la prise part, mais les deux catcheurs
s'éloignent l'un de l'autre et restent figés.** Le mod de test est coupé (`disabled`).

Ce que montre la racine (Bip, position / rotation) à l'image 0, au milieu et à la fin :

| | attaquant f0 | attaquant milieu | attaquant fin | victime fin |
|---|---|---|---|---|
| modèle Swing Neckbreaker 1 | (-18, 9, -86) | (-72, 20, -105) | (-84, 9, -101) | (-23, 80, 25) |
| notre Side Effect | (-5, 12, -135) | (-40, -3, 48) | (19, 9, 162) | (-5, 80, 17) |

→ notre attaquant **part de -135 en z et finit à +162** : la correction de racine (z inversé puis -155) envoie le
mouvement dans le mauvais sens (il traverse la victime puis s'éloigne). Dans les prises WWE '12, l'attaquant reste
vers -85 / -105 tout du long. À revoir : le sens du demi-tour pour les positions (faire tourner autour de la
victime ? ne pas inverser le déplacement ?), puis le gel (sans doute les événements YAT2 du modèle, réglés pour sa
durée et ses positions, ou la fin de prise jamais atteinte).

### Ce qu'on a vu ensuite (2026-11-04, hors jeu)

- **Notre correction de racine est la même que celle de Kaiko** (`moves_port.py` FACING_GAP / `w13_retarget.facing_frame` :
  l'attaquant de WWE '12 est rangé tourné d'un demi-tour et 1 500 unités (150 cm) plus loin en -z ; la victime telle
  quelle). Le sens n'est donc pas en cause.
- **Les grands déplacements sont normaux** : Attitude Adjustment 1 de WWE '12, attaquant -150 → +221 en z (3,7 m),
  victime 0 → 214. Le modèle Swing Neckbreaker 1 bouge peu, mais ce n'est pas une règle.
- **Le vrai souci de Side Effect** : dans SvR 2011, l'attaquant (5233 y0) va de z -20 à -317 cm et la victime reste
  vers 0 / +17 : avec la même mise en place, ils finissent ~1,3 m l'un de l'autre au sol. Sur d'autres prises SvR, les
  sens de déplacement de l'attaquant et de la victime varient d'une prise à l'autre (Mule Kick 2 : les deux en +z
  dans leurs repères ; Attitude Adjustment 1 : opposés). Kaiko, pour aller vers SvR 2011, tourne les deux catcheurs
  vers « le cap du modèle » (`heading_turn`, `template_travel`) : la direction du déplacement dépend sans doute de la
  prise (fiche WAZE ou événements), pas d'une règle fixe. Piste suivante : chercher ce qui oriente la paire dans SvR
  2011 (Kaiko, `SVR2011_TEST_POS_LOG`), et choisir un modèle WWE '12 dont le déplacement ressemble à la prise.
- Outils : `travel` (déplacement des racines des 782 prises communes), `svrroot` (racines d'une prise SvR).

## Méthode prévue (celle de Kaiko, à l'envers)

Kaiko (`Mod Maker Tools\moves_port.py`, `svr10_moves.py`) porte des prises vers SvR 2011 : une prise « modèle » du
jeu d'arrivée, de même catégorie et de durée proche, garde sa fiche, ses données par clé et ses événements (recalés
sur la nouvelle durée) ; ses animations attaquant / victime sont remplacées par celles de la source, convertie au
squelette d'arrivée. Pour nous : modèle = une prise de WWE '12 (son en-tête, ses YAT2, sa fiche WAZE avec un nouveau
numéro et le nom de la prise), YANM attaquant / victime = la prise SvR 2011 convertie comme les corps des entrées.

## Étapes suivantes

1. Choisir une prise présente avec ses animations dans les deux jeux ; comparer sa version WWE '12 à sa version
   SvR 2011 convertie (comme LA Max a validé les entrées sur celle du Rock).
2. Comprendre l'en-tête de 0xC0 octets et les YAT2 (événements : coups, sons, fin de prise).
3. Fabriquer une prise depuis un modèle WWE '12 ; l'ajouter au paquet des mods comme `newmoves13` (misc.pac, m_dlcNN).
4. Brancher dans le Mod Maker (`star_port.h` : `new_move` pour SvR 2011) ; tester en match (un finisher).
5. Puis SvR 2007-2009 (mêmes banques sans l'en-tête de 16 octets, d'après Kaiko) et SvR 2010 (format à étudier).

## Placement mesuré sur les prises communes (2026-11-08, hors jeu)

Outil `pairfit` (`tools_local\moves_study`) : pour les 770 prises que les deux jeux ont (même nom, animations SvR
MOT/BM00 x0 y0 / y1 et fiche complète WWE '12), on cherche le demi-tour et le décalage (cm, plan xz) qui envoient la
racine SvR (0,1 × root) de chaque catcheur sur le Bip de WWE '12 (image 2f). 570 se recalent à moins de 15 cm :

- **la victime garde son repère** (angle 0 dans 532 cas sur 570) ;
- **l'attaquant est tourné d'un demi-tour** dans ~430 cas, pas tourné dans ~90 ;
- **l'écart attaquant - victime dépend de la prise** : tourné et 150 cm (241 prises, la règle de Kaiko et du premier
  essai), tourné et 0 (120), tourné et 100 (39), non tourné et +100 / +150 (72), non tourné et 0 (15) ;
- c'est **la catégorie SvR de la prise** qui le dit : l'octet +0x91 de la fiche SvR (l'octet haut du numéro, la
  « banque » de la prise) prédit la famille dans 90 % des cas, +0x73 dans 88 % ;
- les grands déplacements sont normaux, et **finir à 1,5-2 m l'un de l'autre aussi** : le modèle Stone Cold Stunner 2
  de WWE '12 finit avec 2,2 m entre les deux racines.

D'où la nouvelle méthode (outil `makemove2`) : **modèle = une prise commune aux deux jeux, de la même catégorie SvR
(+0x91, puis +0x73), bien recalée, de durée la plus proche** ; chaque catcheur de la prise portée est placé avec le
demi-tour et le décalage mesurés sur ce modèle ; les fiches légère / complète du modèle gardent tout sauf les blocs
x 0000 y 0000 / 0001, **et les événements YAT2 de ces blocs sont recalés sur la nouvelle durée** (avant : laissés à la
durée du modèle, piste du gel). Side Effect (5233) : modèle Stone Cold Stunner 2 (attaquant tourné, -143 cm ; victime
telle quelle), 178 / 166 images (modèle 180). Mis dans le mod de test `test_side_effect` (v1.1, réactivé) ; l'ancienne
version est gardée dans le bloc-notes de la session. **À voir en jeu.**

Inventaire (`survey`) : 111 prises de SvR 2011 que WWE '12 n'a pas (par le nom) avec leurs animations à deux.

**Essai en jeu du 2026-11-08 : même échec, corps étirés.** Cause trouvée : dans la table de l'en-tête, le u16 à +2 de
chaque entrée est **la taille du bloc en unités de 32 octets** (Pedigree 1 : 366 / 364 pour 448 images ; Stone Cold
Stunner 2 : 188 / 245 pour 272), pas le nombre d'images (« FF + u24 images » plus haut est faux). Les deux essais y
écrivaient le nombre d'images : le jeu ne lisait qu'une partie de chaque bloc. Corrigé dans `makemove2` (taille
recalculée, événements recalés sur le nombre d'images du bloc du modèle) ; mod de test v1.2.

## Les prises ajoutées ne se jouent pas du tout (essais du 2026-11-08)

Témoins en jeu (Front Facelock, stick gauche haut + A) :
- copie exacte du Stone Cold Stunner 2 ajoutée (n° 59645 puis 3295) : gel ; le vrai Stunner (33476) dans la même case :
  **marche** → la case n'est pas en cause ;
- copie exacte du Suplex 3 de Cena ajoutée (n° 3295, animation 57100) : **gel**, même après la correction de la tête de
  groupe (185 = un groupe de CA03 du disque → 188, commit b77c71a) → **le mécanisme d'ajout ne suffit pas, quelle que
  soit l'animation**. La Mandible Claw de Mankind « vue en jeu » était celle du DLC 4 installé.

Ce qui manque très probablement : **`m3.pac` (MDAT/PRM), les paramètres de chaque animation**. Les DLC 3 et 4 livrent
une copie complète de `m3.pac` ; celle du DLC 4 grossit (tables 0/1 +336 o, 0/3 +776, 0/4 +2 300, 0/7 +280, 0/8 +284,
5 +640). Les tables sont des tableaux triés par numéro d'animation (u16 petit-boutiste en tête de fiche, sans en-tête) :
0/3 fiches de 8 o {animation, partie, début u16 et nombre u16 dans 0/4}, 0/4 des paires {événement u16, image u16}
(« 67 00 83 00 » : des temps à 60 Hz), 0/7 fiches de 20 o, 0/1 fiches de 28 o {animation u32, autre animation u32, ...},
5 (« SMB ») fiches de 20 o finissant par un numéro qui se suit (0CF2, 0CF3…). Suplex 3 (50176) a des fiches dans 0/1,
0/2, 0/3, 0/6 (12), 0/7 et 5 ; la prise 50429 du DLC 4 dans 0/1, 0/2, 0/3, 0/6, 0/7. 0/2 et 0/6 gardent leur taille
(cases réutilisées ?). Après une insertion, des champs plus loin changent (0/1, 0/7, 0/8 : indices décalés ?).

**Conséquence pour la 1.1** : les « prises ajoutées » de WWE '13 et 2K14-2K17 (case « Prises ajoutées
(expérimental) », décochée par défaut) ne se jouent sans doute pas non plus en jeu. Piste : ajouter à notre paquet une
copie de `m3.pac` (le plus récent) avec, pour chaque prise ajoutée, les fiches de la prise modèle (ou de sa propre
animation d'origine) recopiées sous le nouveau numéro, sans dupliquer 0/4 ; il faut d'abord comprendre les champs
d'indices.

## Les prises ajoutées se jouent (2026-11-08, vu en jeu)

Trois défauts, corrigés l'un après l'autre avec des témoins en jeu (Suplex 3 recopié, Side Effect de SvR 2011) :
1. **tête de groupe** du m_dlcNN : 185 (un groupe de CA03) → 177 + n° de banque = 188 (b77c71a) ;
2. **paramètres d'animation dans `m3.pac`** (`m3_params.h`, fichier `<numéro>.m3` du mod ; 625aaea) : avec eux, la
   prise dure le bon temps et fait ses sons, mais les catcheurs restaient figés ;
3. **la fiche légère doit être compressée (BPE)**, la complète non : c'est le cas des 5 544 paires du disque et des
   DLC (26b39d8). Avec les trois : **la prise se joue** (« pas parfait » : à détailler).
Animations placées en fin de centaine (57098, 57099 après 57007 du DLC 4). Le journal `--wwe12_dir_log=true` montre
`/ZMOT/CA11 found` ; sonde `wwe12_moves_probe` (sub_827D8E30) : les animations ne passent pas par là.
Reste : le Mod Maker doit écrire le `.m3` des prises ajoutées (WWE '13 / 2K : le m3.pac du jeu d'origine si même format,
sinon la prise modèle), la table 0/8 (non comprise), et la qualité de la conversion SvR.

## Importer la vraie prise, comme Kaiko (2026-11-08)

Constats en jeu : la copie de Suplex 3 joue proprement une fois sa fiche complète mise aussi à la place de la légère
(96d4200 ; le jeu ne trouve pas la complète d'une prise ajoutée, cause inconnue) ; Side Effect « presque parfaite », sauf
la fin (l'attaquant ne se couche pas vraiment). WWE '12 a déjà Side Effect 1 (50185) : **même animation** que le Side
Effect de SvR (Bip à 3 cm près) ; ses différences avec notre conversion : la piste **« vector »** (cap du catcheur, se
retourne de 180° quand l'attaquant est au sol, marque de hauteur à l'image 96), figée chez nous (celle du modèle), et les
événements du modèle (Stunner) au lieu des siens.

Kaiko (`svr10_moves.py`, SvR 2010 → 2011) importe la **vraie** prise : ses animations ET ses données de `misc.pac
WAZA/DATA` (EXH 32 → 36 octets, +u32 « pris de la fiche la plus proche », événements sonores, MBD, bits de catégorie
WAZE pris de prises « donneuses »). Chez nous, ces données ont déménagé dans `m3.pac` :
- **SvR EVT** (fiches 16 o {prise, partie, …, +8 début, +10 nombre} + événements 4 o, 30 images/s) = **0/3 + 0/4** :
  quasi identiques (Side Effect 5233 et Side Effect 1 : mêmes genres et images, 2-3 différences : 0BB9 → 0BC4, un BBB
  ajouté) ;
- **SvR EXH** (36 o) se retrouve en partie dans 0/1 (+6 = EXH+10, 39 %) et 0/2 (+4 = EXH+18, 46 %) ;
- 0/6, 0/7, 5 et le reste de 0/1 / 0/2 n'ont pas d'équivalent direct (outil `rosetta`, 1 778 lignes comparées).
Plan : prendre de la prise SvR ce qui a un équivalent (événements, champs EXH), le reste de la prise WWE '12 la plus
proche, la piste « vector » tirée de l'animation elle-même (cap du Bip) ou de cette prise proche.

## Porter n'importe quelle prise (demande du 2026-11-08) : les m3.pac des autres jeux

WWE '13, 2K14, 2K15, 2K16 et 2K17 ont tous un `m3.pac` (2K16 / 2K17 : réorganisé, 2K17 en HSPC). WWE '13 / 2K14 : les
mêmes tables, renumérotées et élargies (outil `m3map`, `fieldmap`, appris sur les prises communes, mêmes numéros) :
| WWE '12 | WWE '13 / 2K14 | conversion apprise (WWE '13, lignes comparées) |
|---|---|---|
| 0/1 (28 o) | 0/11 (28 o) | champs déplacés : +6 ← +4, +8 ← +6, +10 ← +12, +12 ← +14 (95-99 %) (4 275) |
| 0/2 (20 o) | 0/12 (20 o) | même forme (97-100 %) (674) |
| 0/3 (8 o) | 0/14 (12 o) | début ← +4, nombre ← +8 (11 204) ; table d'événements à confirmer |
| 0/6 (36 o) | 0/10 (40 o) | +4..+30 ← mêmes places (96-100 %), +34 ← (FFFF 77 %) (14 952) |
| 0/7 (20 o) | 0/16 (32 o) | les 20 premiers octets (88-100 %) (5 565) |
Donc pour WWE '13 / 2K14 on peut importer les vraies données de la prise (champ par champ), comme Kaiko. Restent :
la table d'événements de WWE '13, les sons (5), 2K15-2K17, et SvR (WAZA/DATA, plus loin).

## Prises de 2K14 en jeu (2026-11-08)

- Le Suplex 3 de 2K14 (son animation, ses données m3 converties par `m3_params::FromOther`, son aperçu) rangé en 50499 :
  **parfait** une fois les jambes converties ; le Neutralizer de Cesaro (2K14) rangé en 50498 : se joue, « un peu moins »
  bien (à préciser).
- **Gel** de toutes les prises du Cesaro de 2K14 porté tel quel : leurs animations tombent dans des centaines que
  WWE '12 n'a pas (505xx...) - la centaine est le genre de prise. Rangées dans une centaine connue, elles se jouent. À
  faire : le Mod Maker doit renuméroter une prise ajoutée dont la centaine n'existe pas dans WWE '12 (une case libre de
  la centaine de sa prise « donneuse »), et ses cases du moveset avec.
- 2K14 pilote **mollets et avant-bras par IK** (cibles) : `yanm_ik.h` les cuit en rotations (jambes comme pour les
  entrées de WWE '13, bras ajoutés), et écrit maintenant la taille des blocs dans la table.
- Bits de catégorie de la fiche WAZE : pris de prises « donneuses » (`FitCategories`), sinon « None » dans le moveset.
- Aperçus : `motd` de 2K14 → deux lignes `motn`.

## Cesaro 2K14 porté : tout marche en match (2026-11-08, vu en jeu)

Avec le Mod Maker du commit 3fce4b4 : ses 12 prises que WWE '12 n'a pas (finisher Neutralizer, taunts compris) se
jouent en match. Ce qu'il a fallu, en plus des correctifs du paquet (groupe 188, m3.pac, fiche légère = complète) :
données m3 de la vraie prise (`FromOther`), bits de catégorie de donneuses, IK des jambes et des bras cuit, événements
de genres inconnus retirés, **renumérotation par famille** : la centaine du numéro d'animation est le genre de prise ;
une prise ne la garde que si les catcheurs de WWE '12 l'utilisent dans la même case du moveset (taunts de 2K14 en
374xx → 373xx, Neutralizer 505xx → 504xx).
Reste : Create a Moveset « pas vraiment » (à préciser) ; deux mods peuvent renuméroter vers le même numéro (le jeu
devrait répartir les numéros en construisant le paquet) ; 2K15-2K17 et SvR.

## Suite (2026-11-08 soir)

- Deux mods qui renumérotent vers le même numéro : `superstar_pack::SpreadNewMoves` (jeu, avant le paquet) donne au
  second une autre case libre de la même centaine ; ses fiches et son moveset suivent ; même prise = partagée (c453f95).
- Create a Moveset : fiches RGRP/MAGG de 2K14 converties au format de WWE '12 (a94fbb9), bits de catégorie pris d'une
  prise de WWE '12 de la même case (982c49d) ; taunts et Very European Uppercut vus dans les listes, le reste à revoir ;
  aucun aperçu de prise ajoutée ne se joue (à étudier : l'aperçu du DLC 4 marche-t-il ?). Mis de côté (1.2).
- 2K15 : mêmes tables m3 que 2K14 (FromOther marche). 2K16 : tables plus triées par animation, une table « 14 » (24 o,
  8 lignes pour Suplex 3) semble l'index ; 2K17 : HSPC. Tant qu'ils ne sont pas lus, leurs prises ajoutées ne sont pas
  ajoutées (sans données m3, elles gèleraient).

## 2K16 et 2K17 lus (2026-11-08, nuit)

- 2K16 : seulement les tables 0/0-0/7 dans l'ordre d'octets de la console ; l'ordre est appris sur les deux copies de
  2K14 (`m3_params::FromOther`, 4ae3914). 2K17 : un HSPC dont l'entrée est un SHDC rangé comme un HSPC (index à 0x800,
  table {id, décalage, taille, 0} après) ; son enfant 21 = les 0/0-0/11 de 2K16 (mêmes octets), ses enfants 1-14 = ceux
  de 2K16 (`detail::OtherPrm`, 7e6adde). Contre WWE '12 sur ~4 530 animations communes : 2K17 aussi bon que 2K16
  (événements 78 %, 1 397 animations identiques). Roster de 2K17 : 1 436 des 1 444 prises ajoutées ont leurs lignes,
  aperçu et fiches CAM ; les 8 autres (JBL 5, Emma 1 et 2, Emma Lock, Konnor) n'en ont pas et gardent la prise de base.
  Pas encore vu en jeu.

## SvR 2011 dans le Mod Maker (2026-11-08, « Moveset update »)

`port/src/modmaker/svr_moves.h` (branché dans `star_port.h` : une case gardée parce que WWE '12 n'a pas de prise de ce
nom devient une prise ajoutée). **Vient de la vraie prise SvR** : attaquant et victime de chaque segment (x 0, 1, 2,
80…) convertis, le **cap** (piste « vector ») recalculé depuis le corps converti, ses **événements** (WAZA/DATA EVT →
m3 0/3 + 0/4) et deux champs EXH (+10 → 0/1 +6, +18 → 0/2 +4). **Vient d'un modèle WWE '12** (prise commune aux deux
jeux, même catégorie SvR, mêmes segments, durée la plus proche, bien recalée) : le placement de chaque catcheur (demi-tour
et décalage mesurés), l'en-tête et les commandes YAT2 (recalées), les renvois de contre (x 90-92), les autres lignes m3
(recalées), la fiche WAZE (nom et numéro changés), l'aperçu et les fiches CAM. Comme Kaiko dans l'autre sens.

- **Piste « vector »** (outils `vectest*`) : indépendante de Bip (racine à part) ; dans WWE '12 elle reste à 20° du cap
  de l'axe z de Bip - 90° dans 84 % des images (2 990 blocs). Calcul : ce cap après le demi-tour du placement, moyenné
  sur ±8 images. Sa composante y (des creux au moment des chutes, 73 % des blocs) n'est pas comprise : laissée à 0.
- **Validation** (`tools_local\moves_study\makemove3`, VALIDATE : une prise commune convertie avec un autre modèle,
  comparée à celle de WWE '12) sur 56 prises : quand l'animation SvR est celle de WWE '12 (Bip < 10°, 68 % des blocs), le
  cap est à 12° du vrai (celui du modèle : 52°). Pedigree 1 : Bip 2° / 6 cm. Événements : 46 % identiques dans l'ordre.
- Pas encore pris : les prises sur banques YMBs (soumissions comme Walls of Jericho), les taunts (une seule personne),
  SvR 2007-2010. Batista (160) : 2 Batista Bomb ajoutées ; Matt Hardy : Diving Elbow, Wheelbarrow. **Pas encore vu en jeu.**
- **Premier essai en jeu (Batista Bomb de SvR 2011 sur Batista DLC, modèle Gory Bomb 2) : « parfaite à 95 % »**, sauf la
  fin : la victime finit sur le dos (comme dans SvR) puis se retourne d'un coup sur le ventre. Dans la vraie Gory Bomb 2
  de WWE '12, la victime finit sur le ventre : **le jeu applique la position de fin du modèle**. Aucun champ de m3.pac
  (tables à clé), de la fiche WAZE ni des commandes YAT2 ne la prédit sur 979 prises (`downstate`, `downwaze`,
  `downyat`, `downev`). Piste : la table **0/8** (listes {genre, image} finies par {0, 0}, non indexée par numéro ;
  le DLC 4 y ajoute 21 listes, `diff08`, `lists08`), sans doute référencée par le rang d'une ligne d'une autre table.
  Essai : choisir un modèle qui finit pareil (The Last Ride) → « ça bug pas mal » en jeu, annulé (87d1322).
- **Tests en jeu du 2026-11-09 (Batista Bomb SvR sur Batista DLC, comparée à la Batista Bomb officielle du DLC 4,
  animation 50429)** : l'animation de la victime du DLC est **la nôtre** à 1 cm près. Corrigé : l'image d'impact
  (m3 0/6 ligne de l'attaquant +34 = image de la commande 73, 959 des 1 005 prises de WWE '12 ; le modèle gardait la
  sienne → flashs du finisher en retard, **vu réglé en jeu**), la ligne EXH de la prise (partie 0, pas celle du contre),
  la durée commune des deux personnes, la personne des événements. **Reste : la victime se retourne à la fin.** Écartés par
  des tests : événements (A1), tables 0/1 + 0/2 (A2) ; l'octet 0/7 victime +4 du DLC (0) fige la prise s'il est seul (A3, C).
  Avec **toutes** les tables du DLC (A), plus de retournement. Écartés hors jeu : commande 56, +8 de 0/6 (pas une image).
  Différences restantes avec le DLC : 0/6 attaquant +5 (0E/0F) et +8 (19/120), 0/7 victime +4 (2/0), sons (5), commandes
  15 (prises de main, celles du modèle). Prise « contre les cordes » SvR (31074, anime aussi y60 / y61) : gèle ; retirée.
- **Bug du jeu, pas du Mod Maker** : la Beast Bomb du Batista DLC (case 0x4E) plante sans aucun mod
  (sub_824A5458 ← sub_82481C78, lecture 0x5C) ; transmis à Gameplay le 2026-11-09.
- **Retournement de la victime : RÉGLÉ le 2026-11-09 (aa9cc6d, vérifié en jeu : « nickel », sonde de Gameplay).** La
  position au sol de la victime vient de la commande 81 du jeu (`sub_824AE820`), fabriquée depuis la **ligne 0/6 de la
  victime** (segment 0, personne 1) : **+20 u16 = animation au sol, 0 sur le dos, 1 sur le ventre**. On gardait celle du
  modèle (Gory Bomb 2 : 1). Sur 910 prises de WWE '12, elle suit exactement la dernière pose de la victime (axe x du Bip
  vers le bas → 0 : 690/690 ; vers le haut → 1 : 218/218, outil down20) : le Mod Maker la calcule maintenant d'après notre
  animation. Fausses pistes réfutées en jeu : l'ordre des listes d'événements (a9b8530, gardé car c'est l'ordre du jeu)
  et l'événement 0325/032D de la victime. Sonde : `wwe12_down_probe`, `tools/dev/lancer_test_au_sol_copie.bat`.
- **Vérification hors jeu de toutes les prises SvR 2011 (2026-11-09, outil `svrcheck`, copie dans `tools_local/moves_study`)** :
  sur 2 780 prises, **730 fabriquées** et relues sans défaut bloquant (deux personnes de même durée, m3 lisible, position
  au sol de la victime posée : 505 sur le dos, 158 sur le ventre, 67 debout ou sans ligne). Refusées : 1 887 sans animation à
  deux dans SvR (prises seules), **83 à trois personnes (y 2) et 41 avec un objet (y 60/61, cas de la Batista Bomb « Rope »
  qui gelait le jeu)** : refus ajoutés au Mod Maker ; 37 dont l'animation ne se convertit pas, 2 sans modèle. Sans
  conséquence : 270 sans fiches MAGG (aperçu seulement, facultatif), 4 avec des événements inconnus (retirés par star_port).
  Les autres pièces de SvR (y 10-12 / 30-32 cordes, 50-57) n'existent plus dans les prises de WWE '12 (outil `w12y`) et
  sont laissées de côté comme avant. Reste à voir en jeu : quelques prises de chaque famille.
