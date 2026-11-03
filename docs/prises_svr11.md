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
