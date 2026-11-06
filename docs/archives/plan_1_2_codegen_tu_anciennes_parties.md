# plan_1_2_codegen_tu.md : anciennes parties (archivées le 2026-11-06)

Sorties de `docs/plan_1_2_codegen_tu.md` lors du ménage des documents. Gardées telles quelles.

<!-- Voie A (xex patché de la TU2), écartée le 2026-11-05 -->

### Voie A : générer le code depuis le xex patché (TU2, v0.0.2.4)

- Le SDK sait déjà appliquer un `default.xexp` posé à côté du xex, au codegen comme au lancement.
- **Problème de fond : au lancement, le runtime charge le xex du joueur.** Il en prend `.rdata`, `.data` et les
  tables. Un code TU2 sur un xex du disque (0.0.0.4) ne marche pas : les données sont ailleurs (voir
  « Versions du disque » dans CLAUDE.md, et le recomp de WWE '13 qui « exige la TU exacte »). Donc :
  - **chaque joueur devrait fournir lui-même le `default.xexp` de la TU2**. On ne peut pas le distribuer : c'est
    une donnée du jeu, règle absolue. Sans ce fichier, l'exe ne démarre pas ;
  - ou bien il faut livrer **deux exe** (disque / TU2) : le double de compilation, de tests et de crochets ;
  - les fichiers de données de la TU (`strMenu.pac`, `_startup360.pac`, `DLC.pac`, `plist360`) sont lus par le
    code TU2 sous `UPDATE:\`. Il faut vérifier que le code retombe sur les fichiers du disque quand ils manquent.
    Sinon, par exemple, les nouveaux textes des menus de l'Univers sont vides.
- **Remappage** (mesuré avec les images disque et TU2, scripts `work/tu_study/tools2026_11_03/` + `remap.py`) :
  - `port/wwe12_config.toml`, 751 entrées (fonctions sans `.pdata`) :
    - 328 retrouvées par leur code, une seule fois ;
    - 402 dont le code existe plusieurs fois, presque toutes départagées par le voisin le plus proche (même
      décalage) ;
    - **21 introuvables** (signature trop courte, à refaire à la main).
  - `port/src`, environ 2 245 adresses citées, sondes de développement comprises :
    - 1 489 = début d'une fonction au code identique, remappage automatique ;
    - ~650 = fonctions sans `.pdata` ou adresses à l'intérieur d'une fonction (retours d'appel comme
      `kLeaveCountReturn`) : par signature, environ 50 à vérifier à la main ;
    - 48 en `.data`/`.bss` (0x83…) : la section a la même adresse et la même taille dans la TU2, probablement
      inchangées, à vérifier ;
    - 29 en `.rdata` : la section a grandi, à retrouver par leur contenu ;
    - **22 dans des fonctions CHANGÉES par THQ et 1 SUPPRIMÉE** : à relire une par une.
      - Univers : `sub_82545A70`, `825463E8`, `82583B78`, `82589958`, `82599DB0` et son retour `8259A028`,
        `826C8690`, `826CD010`, `826D3B58` et son retour `826D3C84`. **`sub_826CEF28` (crochet des résultats)
        est réécrite.**
      - Autres : `828F2410` (superstar mods), `829F9F90` (pages d'arènes), `825AB3E8` (draw.cpp),
        `82240748` (state.cpp), plus des sondes.
  - Nos crochets qui recopient une logique du disque devraient suivre la TU2. Exemple : le recomptage de l'effectif
    Univers, où la TU2 compte les CAW.
- **Coût :** très gros. C'est un recalage complet, et il faut rejouer tout ce qui est accroché : Univers, Mod Maker,
  entrées, menus, ceinture, chargements, jukebox… La décision « joueurs avec TU seulement » ou « deux exe »
  appartient à l'utilisateur.
- **Gain :** toutes les corrections de THQ d'un coup, y compris celles qu'on n'a pas comprises (armes, réglages,
  réseau, ralentis).
