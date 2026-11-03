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

## Une prise dans SvR 2011

- Fiche : `misc.pac` MOVS/WAZE, 160 octets, numéro u16 petit-boutiste +0x90 (Arm Drag 1 = 4809).
- Animations : `m.pac`, banques YMKs (lecteur : `svr11_entrance::detail::ReadBank`), clés (id, x, y) :
  y0 attaquant, y1 victime, y50 / y51 / y57 caméra et objets, x90 / x91 branches de contre. La même prise est dans
  plusieurs familles : MOT/BM00/n (le jeu complet), MOT/RRnn (sources des banques de match), MVMT/<catégorie>/n
  (STN2, SPE1...). Exemple : 4812 (y0 201 images, y1 172 images).
- **Arm Drag 1 (4809) n'a pas d'animation dans SvR 2011** : le jeu garde des fiches de prises supprimées, catégories
  effacées (noté par Kaiko). Pour comparer, prendre une prise présente dans les deux jeux avec ses animations.

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
