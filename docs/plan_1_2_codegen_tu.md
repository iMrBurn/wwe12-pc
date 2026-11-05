# Plan 1.2 : un seul nouveau code généré (TU de THQ + registres locaux + « volatile »)

Gameplay, 2026-11-05. **Décision de l'utilisateur :** tout ce qui change le code généré se fait ensemble après la
1.1, pour la 1.2 : une seule régénération, un seul gros test.
- Ce qui change le code généré : les options de registres locaux, le patch `volatile`, le passage au xex patché
  des mises à jour de THQ.
- **Rien n'est fait maintenant** (pas de codegen, pas de compilation).
- Bases : `docs/title_update_corrections.md` (ce que les TU corrigent) et `docs/perf_natif_steam_deck.md` (section
  « Natif / code du jeu »).

## 1. Deux voies pour les corrections de THQ

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

### Voie B : rester sur le code du disque et refaire nous-mêmes les corrections utiles

Chaque correctif a son propre réglage pour revenir en arrière.

| Correctif (voir `title_update_corrections.md`) | Coût | Remarque |
|---|---|---|
| Finishers (TU2) : 6 à 7 cas spéciaux (Spear 3, Sleeper Hold 2, Ankle Lock 3, Edgecution, Skull Crushing Finale, 37229, 57006) dans `sub_821D3F58`, `8223F440`, `8223F878`, `82280DF0`, `823CCDB8`, `823F24C8` | Petit à moyen : un crochet par fonction, chaque condition refaite au bon endroit (souvent au milieu de la fonction, donc il faut reproduire le chemin avant). Le plus simple : Skull Crushing Finale, en fin de fonction. | Il faut d'abord reproduire chaque gel en jeu, prise par prise. |
| Univers : les CAW comptent dans l'effectif (TU2) | Facile : 3 crochets, plus une ligne dans notre recomptage. | Change les règles : accord de l'utilisateur. |
| Univers : index < 23 (TU2, 5 fonctions) | Facile. | Protection contre une corruption. |
| Prises des DLC 3-4 (CA09/CA10) dans l'éditeur de moveset (TU1) | Moyen à gros : renuméroter les banques dans ~15 fonctions et agrandir un tableau sur la pile. | Utile aussi pour nos prises CA11+ ; perte des prises à la sauvegarde pas prouvée (essai en jeu d'abord). |
| Résultats Univers `sub_826CEF28` réécrite, booking du PPV, Draft | Inconnu (pas compris). | À étudier seulement si un bug y correspond. |
| Armes (paramètre ajouté), 3 réglages 40/60/80 → 70/80/90 | Moyen / très facile, mais sens inconnu. | Plus tard. |

**Coût de la voie B :** petit à moyen, par morceaux. Aucun recalage d'adresse, un seul exe, les joueurs n'ont
rien à fournir. **Limite :** seulement ce qu'on a compris.

**Décision de l'utilisateur (2026-11-05) : voie B** (nos propres correctifs sur le code du disque). Recommandation d'origine : la voie B. La voie A impose aux joueurs un fichier qu'on ne peut pas leur donner, ou deux exe.
À trancher par l'utilisateur.

## 2. Ordre de travail proposé pour la 1.2 (une seule sortie, mais des étapes internes pour retrouver une panne)

0. **Référence avant tout changement**, sur la compilation 1.1. Données isolées, `lancer_isole.bat`.
   - Scènes fixes, jouées à la télécommande (`wwe12_remote_pad`) : menu principal, éditeur de moveset (CAW), match
     1 contre 1, match à 6, émission Univers, une entrée, un replay d'après-match.
   - Pour chaque scène :
     - captures d'écran aux mêmes moments ;
     - `[perf] cpu per frame` (Rendu) ;
     - échantillons du fil principal (`SVR_SAMPLE_STACKS=5`, `samples.txt` effacé avant, carte `.map` de la même
       compilation) ;
     - un vidage de quelques zones de la mémoire du jeu après N images (fiches d'os d'un catcheur, état du
       match), pour comparer octet par octet.
1. **Options de registres locaux**, dans `port/wwe12_config.toml` :
   - d'abord `cr_as_local`, `ctr_as_local`, `xer_as_local`, `reserved_as_local` (risque faible) ;
   - puis `non_argument_as_local` ;
   - puis `non_volatile_as_local`.
   - À chaque palier : codegen, compilation, mêmes scènes, mêmes mesures. Les vidages mémoire doivent être
     identiques ; seul le temps doit changer.
   - Point à vérifier : nos crochets qui lisent `ctx` après l'appel de l'original. `ctx.rN` reste-t-il à jour pour
     les registres devenus locaux ? Le SDK doit les recopier aux appels et aux sorties : à confirmer dans le code
     généré avant de compiler.
2. **Patch `volatile`** du modèle `pch_h.inja` (patch SDK dans `port/patches/`) : on le garde pour MMIO et pour les
   boucles d'attente entre fils (à repérer). Mêmes contrôles.
3. **Corrections de THQ** :
   - en voie B, ajoutées une à une, chacune avec son réglage ;
   - en voie A, régénération sur le xex patché, puis remappage (scripts ci-dessus), puis relecture des 23
     fonctions changées ou supprimées.
4. **Gros test final** (le « un seul gros test » de l'utilisateur) :
   - toutes les scènes de l'étape 0 ;
   - plus Univers sur plusieurs semaines, Create a Superstar, Create an Entrance, Mod Maker, DLC ;
   - plus les 3 exe (Vulkan, D3D12, D3D11) et Android ;
   - plus le 4 fils.

Pourquoi des paliers internes malgré « une seule régénération » : si l'étape 4 casse quelque chose, on sait à quel
palier le chercher. Ces paliers ne sortent jamais chez les joueurs.

## 3. Ce qu'il faudra pour démarrer (go de l'utilisateur)

- Lancer le jeu pour l'étape 0 (mesure de référence).
- Pour la voie A seulement : décision « TU obligatoire » ou « deux exe ».
- Pour lire les patchs du SDK de Jarrod (registres, volatile) : téléchargement = go de l'utilisateur, reprise de
  code = accord de Jarrod.
