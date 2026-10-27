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
