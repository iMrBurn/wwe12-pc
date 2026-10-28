# Limit breaking — idées à tester

Réserve d'idées pour repousser les limites du jeu d'origine (bloc-notes de l'utilisateur,
2026-10-22, plus les idées de départ). On ajoute au fil de l'eau et on note ce qu'on découvre.
Voir aussi `docs/suggestions_joueurs.md` (liste d'un joueur triée par effort) et `docs/univers.md`.

## Contexte

- La 360 n'avait que 512 Mo partagés. La plupart des plafonds (catcheurs à l'écran,
  slots de création, roster) viennent probablement de là, pas du moteur.
- Le PC a beaucoup de marge : ~390 Mo de VRAM par match en 1440p.
- Probable premier mur : les tas mémoire du jeu (déjà des « PhysicalHeap::Alloc unable
  to alloc » au chargement).
- Outils déjà en place : patch de .pac au démarrage servi par l'overlay VFS
  (cf. wwe12_menu_patch.cpp), hooks de fonctions (menu, fps, intros), scanner mémoire F7
  (wwe12_memscan.cpp), journal de débogage du jeu (`wwe12_game_log`).
- Acquis : le 6-man Elimination Chamber tient 60 fps sans crash.
- Limite des 6 catcheurs gravée dans le code : tableaux à 6 cases, budget mémoire Xbox, animations.
- Kaiko a fait un « Limit Breaking » pour son portage SvR 2011 : lui demander jusqu'où il est allé
  (plus de 6 catcheurs ?) et comment. Le Gauntlet vient aussi de SvR 2011.

Colonne « Qui » : la conversation qui s'en charge (Univers, Gameplay, Rendu…) ; « LB » = celle-ci.

## Stipulations à ajouter

| Idée | Piste | Qui | Statut |
|------|-------|-----|--------|
| I Quit | Absent de WWE '12 (aucun texte). Base : victoire « SUBMISSION ONLY » du Match Creator + résultat « GIVEUP » qui existe déjà. Voir notes 2026-10-22 | LB | **prévu pour la 1.1** (décision de l'utilisateur, 2026-10-26). Étude chiffrée : version légère **1 à 2 séances**, risque faible ; version WWE '13 (arbitre au micro, menace, mini-jeu en Lua) 4 à 6 séances si les animations sont dans WWE '12 (voir « Étude chiffrée ») |
| Special Guest Referee | Le type existe dans la liste interne (n° 83, « SPECIAL REFEREE ») mais aucun menu ne l'offre : reste de SvR 2011, retiré dans '12. Logique encore là ? inconnu | LB | **prévu pour la 1.1** (décision de l'utilisateur, 2026-10-26). La fiche de règles 83 est intacte (3e place = arbitre) ; version légère 2 à 4 séances, complète (WWE '13) 10 à 15 séances |
| Gauntlet (SvR 2011) | **Déjà dans WWE '12** : JOUER > HANDICAP > GAUNTLET (et Match Creator). Même place dans SvR 2011 (Handicap > Gauntlet, apparu dans SvR 2009) : rien à ajouter. Seule piste : plus d'adversaires (voir « plus de catcheurs ») | LB | **existe déjà** |
| Squash match | Pas un type de match : un comportement (dégâts, jauge, IA du perdant). Étude : `docs/squash.md` | LB | **étudié 2026-10-28** : léger 3 à 5 séances, avec attaque après le tombé 6 à 10 |
| Combinaisons (cage en tag, HIAC à plus de 2…) | Mêmes tables de types de match | LB | idée |
| Enlever les restrictions du Match Creator | Menus grisés ; cf. `sub_825910F8` / `sub_82591750` (First Blood grisé) | LB | idée |

## Mode Univers (conversation Univers)

| Idée | Piste | Statut |
|------|-------|--------|
| Titres **utilisés** par une émission rendus vacants | Le jeu ne programmerait plus jamais de match pour eux ; il faut d'abord un tournoi (ligne suivante) | idée (vacant des titres non utilisés : **fait** 2026-10-20) |
| Tournoi pour couronner un titre vacant | Générer une série de matchs sur les cartes d'émission | idée |
| Menu Stat Tracking façon WWE '13 (+ historique des vainqueurs Rumble, MITB, KOTR…) | Le jeu ne garde ni victoires ni historique : à noter nous-mêmes | idée |
| Booking modifié (défenses de titre spontanées en weekly…) | Génération des cartes (docs/univers.md) | reporté à la 1.0 |
| Catcheurs réservés à certains titres (Cena majeurs, Bourne mineurs) | Filtre au moment du booking | idée |
| Backport des cinématiques Univers de WWE '13 / 2K14 | Scènes = données (`pac/evt`), choix = code (`sub_826DF6D8`) ; données d'autres jeux = question de droits | idée |
| Cinématiques de Create a Story déclenchées comme les vraies | Même mécanisme de choix (`sub_826DF6D8` / `sub_826ECE78`) | idée |
| Enlever le minimum de 15 catcheurs par émission (Univers 100 % CAW) | Trouver le test du minimum | idée |

## Modifs de jeu

| Idée | Piste | Qui | Statut |
|------|-------|-----|--------|
| Sang pour les catcheuses | `wwe12_blood.cpp`, voir « Savoir acquis » de CLAUDE.md | Gameplay | reporté à la 1.0 |
| 5 contre 5, plus de 6 catcheurs sur le ring | Tableaux à 6 cases, slots manettes/IA, spawn, mémoire | LB | idée |
| Royal Rumble : plus de catcheurs en même temps sur le ring | Même plafond | LB | idée |
| Touches du clavier affichées à la place des boutons Xbox/Sony | Même méthode que les icônes PS3 (planche d'icônes remplacée selon l'appareil) ; icônes à dessiner nous-mêmes | Gameplay | idée |
| Jukebox dans les menus | Lecteur de musique des menus (on sait déjà jouer des musiques perso) | Gameplay | idée |
| Moments OMG de WWE '13 | Nouvelles animations + déclencheurs : très lourd | LB | idée |
| 50 → 100/200 slots CAW | Zone des CAW : `sub_82684910`, fiche de 1 832 octets ; taille des sauvegardes | LB | idée |
| 20 → 100/200 logos perso | Idem, côté logos | LB | idée |
| Plus d'arènes créées, roster plus grand | Idem | LB | idée |
| Foule plus dense, distance d'affichage, LOD au max | Paramètres de la foule et des LOD | Rendu | idée |

## Gros chantiers

| Chantier | Notes | Statut |
|----------|-------|--------|
| Mod Maker pour la 1.0 | En parler avec Kaiko | prévu 1.0 |
| Jeu 100 % natif, sans émulation | Remplacer les fonctions recompilées par du C++ lisible, système par système (voir CLAUDE.md) ; en parler avec Jarrod (WWE 2K14) | long terme |
| D3D12 en plus de Vulkan (et peut-être D3D11) | plume gère Vulkan et D3D12, pas D3D11 ; aujourd'hui compilé Vulkan seul (`native_renderer.cmake`) | après la 1.0 |

## Notes de recherche

### Arènes : fichiers `pac/bg/bgNN.pac` (2026-10-22, lecture seule)

Un `bgNN.pac` = EPAC → section `STG ` n° `00NN` → PACH d'entrées compressées en BPE de Yuke's
(`tools/yukes_bpe.py`). Le jeu charge par numéro de scène (`/STG/%04d`, `sub_823BEE70`, `sub_823BEED0`,
`sub_823E1E30`) : **le numéro du fichier = le numéro de scène**. Aucune table texte « nom ↔ fichier » trouvée (le
choix d'arène affiche des logos) ; identification par le **sigle des objets 3D** dans chaque fichier (script
jetable : décompresser les blocs BPE et compter les préfixes `xxx_` des noms) :

| Fichier | Sigle | Arène WWE '12 |
|---|---|---|
| bg00 | sd | SmackDown |
| bg01 | raw | RAW |
| bg02 | nxt | NXT |
| bg03 | wss | Superstars |
| bg04 | otl | Over the Limit |
| bg05 | f4w | Fatal 4-Way |
| bg06 | mib | Money in the Bank |
| bg07 | ss | SummerSlam |
| bg08 | noc | Night of Champions |
| bg09 | hic | Hell in a Cell |
| bg10 | br | Bragging Rights |
| bg11 | sv | Survivor Series |
| bg12 | tlc | TLC |
| bg13 | rr | Royal Rumble |
| bg14 | elc | Elimination Chamber |
| bg15 | wm27 | WrestleMania XXVII |
| bg16 | er | Extreme Rules |
| bg17 | nit | WCW Nitro |
| bg18 | coc | WCW Clash of the Champions XXXV |
| bg19 | stc | Starrcade (texte 12334) |
| bg20 | iraq / mb | Tribute to the Troops |
| bg21 | elc | **match** Elimination Chamber (la structure) |
| bg22 | rr | **match** Royal Rumble |
| bg23 | raw | arène de l'éditeur de prises (Create a Moveset) |

**Fichiers reçus d'un tiers** (`Desktop\WWE '12 PC Port\mods\pac\bg\`, mêmes en-têtes EPAC/STG que le jeu ; rien
exécuté, rien copié dans le dépôt) : bg09 = Royal Rumble (sigle rr), bg10 = Elimination Chamber (ec), bg11 =
WrestleMania 26 (`ar_fen_wm26`), bg20 = 2e Elimination Chamber (ec + `ar_dwnmt_chem`). Ils portent les numéros
de **SvR 2011**, pas ceux de WWE '12 : dans le jeu ils remplacent **Hell in a Cell, Bragging Rights, Survivor
Series et Tribute to the Troops**. Renommages conseillés : reçu bg09 (RR 2010) → bg13 (PPV) ou bg22 (match),
bg10 (EC 2010) → bg14 (PPV), bg20 (EC version match) → bg21 (match EC), bg11 (WM26) → bg15.
**Testé le 2026-10-22 (jeu installé 0.9, fichiers renommés en bg13/14/15/21)** : WrestleMania 26 en bg15 =
**plantage** 0,15 s après que le VFS sert `bg15.pac` (lecture de guest 0xAF, thread de chargement). Les arènes
de SvR 2011 ne sont donc pas lisibles telles quelles par WWE '12 (même en-tête EPAC/STG, contenu de scène
différent) ; les 3 autres non essayées, même origine. Il faudrait les convertir (format de scène à comparer).

**Comparaison WM XXVII (WWE '12 bg15) / WM 26 (SvR 2011)** : même organisation. Après l'EPAC/STG, un PACH à 0x4000
(entrées {id, offset, taille}, 170 contre 233), chaque entrée compressée en BPE ; modèles JBOY (même en-tête),
400-402 (gros paquets de modèles), 995-999 (tables + « HMD »), 20000-20010 (sous-PACH), 30000/31010 (YMKs),
50001-50020 (textes de placement), 60200-60203, 61000 (DDS). Différences :
- **Présentes seulement dans WWE '12** : **980** = les cordes du ring (JBOY `ar_rop01`, `ar_rope10_low`, os
  `bone49`…, cordes physiques de WWE '12), **30002** = sous-PACH de 31 fiches de 784 octets, **800/804/809** =
  écrans (`titantron_movie`, `minitron_movie`, `arena_movie`), plus des décors propres à WM XXVII.
- **995/997** : tableaux de 20 éléments (WWE '12) contre 17 (SvR) ; **996/999 « HMD »** : en-tête 0xA4 contre 0x82.
- Plantage (lecture à 0xAF, donc un pointeur nul + décalage) : très probablement une entrée attendue absente
  (980 ou 30002 en tête de liste).
- **Piste de conversion** : reconstruire le PACH du fichier SvR en y ajoutant 980, 30002 et 800/804/809 pris dans
  l'arène WWE '12 d'origine du même numéro, et remplacer 995-999 par ceux de WWE '12 si besoin. Il faut un
  outil qui réécrit un PACH (entrées non compressées ou recompressées en BPE : à vérifier que le chargeur accepte
  des entrées brutes).
- **Outil `tools/dev/arena_convert.py`** (2026-10-22) : réécrit le PACH d'une arène SvR en y ajoutant des entrées
  d'une arène WWE '12 (copiées compressées telles quelles), recalcule EPAC+8 et STG+0x14, recopie la signature de
  fin (« EOP5/plugin version 1.25/1.26 »). Vérifié : un aller-retour sans ajout redonne les fichiers à l'octet près.
  Essai 1 : `work/arenes/bg15_wm26_v1.pac` = WM 26 + 980, 30002, 800, 804, 809 de WM XXVII.
- **Essais en jeu (2026-10-22)** : v1 (entrées ajoutées) et v3 (+ 995-999 et YMKs 30000/30001/31010 de WWE '12) :
  **même plantage**, 0,15 s après le chargement, lecture de 0xAF, lr 0x8283725C dans `sub_82837228`. Cette
  fonction construit les **index de triangles d'un modèle** : objet `[+204]` → `sub_8238B7A8` (= `[+12]`) → tableau
  de fiches de maillage de **184 octets** (à +20), +4 = nombre de bandes, **+156 = table des bandes** (fiches de
  12 octets {?, nombre, pointeur vers des index 16 bits}, 0xFFFF = coupure). Dans le fichier SvR, ce +156 vaut
  0xAB : **la disposition des maillages JBOY a changé entre SvR 2011 et WWE '12**, même si l'en-tête JBOY est
  identique. Ce n'est pas une entrée manquante.
- **Bilan conversion d'arènes** : il faudrait un **convertisseur de modèles JBOY** (comparer un même modèle dans
  les deux jeux, par ex. `ar_case0`/`ji_bl` présents dans les deux, trouver les champs ajoutés/décalés des fiches
  de 184 octets et des autres structures, réécrire chaque modèle). Faisable mais long (format non documenté,
  plusieurs centaines de modèles par arène).
- **Comparaison JBOY (2026-10-22)**, modèles présents dans les deux WM : 32000 « blood » (884 o des deux côtés), sièges
  16-21 (WWE '12 = SvR + 120 o à chaque fois), 951-974 (poteaux, cordes, table). Données en gros-boutiste.
  En-tête : « JBOY », taille, 0, taille, 0, 0, 1, 0x40, 1, 1, puis 3 offsets de sections (ex. 0x2D0, 0x320, 0x330).
  Différences trouvées :
  1. **Matériau** : après trois offsets (ex. 0x1FC, 0x270, 0x290), WWE '12 insère **un mot 0xB0020000** (drapeaux ?)
     avant le nom de technique (« yDefault ») ; tous les offsets internes situés après sont **+4**.
  2. **Paramètres de matériau** : liste autodécrite (nom, type 0x0D/0x0A/0x05/0x0F, taille 0x24/0x18, valeur) ;
     WWE '12 en ajoute (ex. `sampLightMapPrm`, offsets 0x9E68…0x9EC8 dans le siège 21).
  3. **Bloc de 8 flottants en plus** dans le siège (0x3F733333, 0x3F7E0000…, 0x3C000000 ×4) à ~0x7C14.
  4. Les fiches de maillage de 184 octets (+156 = bandes de triangles) restent à localiser et comparer.
  Un convertisseur = parseur JBOY complet (sections, matériaux, maillages, sommets, index) + réécriture avec
  recalcul de tous les offsets. Plusieurs séances de travail.
- **Trouvé ensuite (même jour)** : la « fiche de 184 octets » est la **fiche de maillage**, à 0x40, nombre à +0x18.
  **SvR 2011 = 180 octets, WWE '12 = 184** : un mot inséré à **+124** (pointeur vers un bloc lightmap dans le siège,
  0xB0020000 dans « blood »), tout le reste décalé de 4 (SvR +156 → WWE '12 +160). Les modèles finissent par une table
  **POF0** (Pointer Offset Fixup) = liste de tous les emplacements de pointeurs (pas de 1/2/4 octets, préfixes 01/10/11,
  en mots, comptés depuis l'octet 8) : 6 860/6 861 et 7 634/7 634 pointeurs valides ; décodage + réencodage
  identiques sur les 349 modèles des deux jeux. 400-402 = paquets de textures DDS (même format), 20000/20010 =
  sous-PACH avec 63 modèles chacun (la foule).
- **`tools/dev/jboy.py`** : `svr_to_w12()` insère le mot (0) à +124 de chaque maillage, bourre après la table pour
  garder l'alignement 16, corrige tous les pointeurs listés et réécrit POF0 et les tailles. **`tools/yukes_bpe.py`
  `store()`** : bloc BPE non compressé (table de paires vide `FF 80 FE`, sous-blocs ≤ 2 000 octets ; le jeu en a
  ≤ 2 028). **`arena_convert.py --models`** convertit tous les modèles (y compris imbriqués) puis ajoute les entrées.
  Essai `work/arenes/bg15_wm26_v4.pac` : 330 modèles convertis (2 sans maillage laissés), relu sans erreur,
  9 358/9 358 pointeurs valides.
- **Essais en jeu v4 et contrôle** : v4 (11,8 Mo, entrées converties en BPE non compressé) = **chargement infini** ;
  contrôle = WM XXVII d'origine réencodée en BPE non compressé (16,7 Mo) = **plantage** (écriture à 0x7099000B,
  lr 0x827E1430, juste au-delà d'une zone fixe). Le jeu charge l'arène dans un **tampon de taille fixe** (arènes
  d'origine ≤ ~10,9 Mo, bg05). **`yukes_bpe.compress()`** : vrai BPE de Gage, sous-blocs de 4 000 octets comme le
  jeu (mesuré : 4 000 décodés, ≤ 2 028 codés, ≤ 178 paires), aller-retour exact, ~16 % plus gros que le jeu.
  v5 = v4 recompressée : **9,7 Mo**, relue sans erreur.
- **v5 en jeu (2026-10-22) : ça charge et tourne à 60 fps !** Défauts (capture de l'utilisateur) : la **foule est mal
  placée** (spectateurs dans le ring et devant la caméra) et **tout est très sombre** (lightmap absente : pointeur
  +124 mis à 0 ; tables 995-999 peut-être). v6 = v5 + la foule de WM XXVII (`--replace 20000,20001,20002,20003,20010`).
- **v6 en jeu** : l'arène WM 26 s'affiche (logo, structures, éclairage bleu correct, plus sombre du tout) ; écran de
  présentation normal. Sur un plan (Cena au sol, Orton debout), des structures métalliques passent devant la caméra
  autour du ring : caméra d'entrée/fin de match de WWE '12 qui traverse le décor SvR, ou pièces mal placées
  (placement 50001-50020 et 995-999 gardés de SvR) ; à préciser en match.
- **v6 en match (capture)** : la salle WM 26 est **bien placée** (tribunes, scène à gauche, marches), mais le **ring est
  cassé** : seules les cordes ajoutées (980, WWE '12) s'affichent, pas le tapis, le tablier ni les poteaux ; ambiance
  sombre et rouge. Les pièces du ring sont des modèles **à os** (`bone401`…) ; SvR en a 4 copies pour certaines.
  Fichiers de placement 50003 (lumières de l'entrée : 960/2950 contre 90/305) et 50001 (attributs par objet, lumières
  101-108 dans SvR, 121-131 dans WWE '12) diffèrent ; 50005 identique.
  v7 = v6 + **tout le ring de WM XXVII** (`--replace 951,952,953,956,960,964,973,974 --remove 957-959,961-963,965-967`).
  Fichiers de test rangés dans
  `Desktop\WWE '12 PC Port\arenes_svr2011_desactivees\` (mods vide).
- **v7 vérifiée (2026-10-23)** : 9,76 Mo, 229 entrées (v6 moins les 9 pièces SvR retirées), 951-980 identiques à
  WM XXVII octet pour octet, toutes les entrées BPE se décompressent. Installée dans `mods\pac\bg\bg15.pac` à la
  place de la v6 (la v6 reste dans `work/arenes`).
- **v7 en jeu (capture, 60 fps)** : ring complet (poteaux, cordes, coins, tablier WM avec le logo SvR) mais **tapis
  jaune-vert à damier** et **pointes rouges** (couleur du bas du tablier) sur le bord du tapis et devant le tablier ;
  salle rouge et sombre, tribunes vides. Cause probable du tapis : le ring de WWE '12 cherche ses textures **par nom**
  dans le paquet 400 de l'arène, et il en manque 4 dans celui de SvR : `mat_noize` (tapis), `rin_ep01` (tablier 974),
  `rin_tb00_n`, `rin_tb00_s` (couvre-coins 964). Paquet de textures 400-402 : nombre, 0x100, 0, 0x10, puis fiches de
  32 octets {nom[16], « dds », taille, offset, 0}, données alignées sur 16 (aller-retour exact).
  `arena_convert.py --textures noms` les recopie de l'arène donneuse. Pointes : le tablier 973/974 a 44 os
  (`bs_ar_epron`), sans doute placés par les tables 995-999 restées celles de SvR.
  Essais : **v8** = v7 + les 4 textures (10,06 Mo) ; **v9** = v8 + 995/996/997/999 de WM XXVII (10,07 Mo).
  v8 installée le 2026-10-23.
- **v8 en jeu** : tapis gris correct, pointes toujours là, « la scène est en dessous » (remarque de l'utilisateur, juste).
  **Cause trouvée : les unités.** SvR 2011 compte en **10 cm**, WWE '12 en **cm** : même ring, tapis à 12 contre
  120, sphère 64,76 contre 647,6, sol ±263 contre ±2 630 ; zones de lumière 50003 : 90/305 contre 960/2 950 ;
  boîtes de visibilité 50020 idem. Toute l'arène SvR était donc dessinée **10 fois trop petite**, sous le ring
  (les pointes = scène et barrières miniatures qui dépassent du tapis). Les tribunes visibles étaient la foule de
  WM XXVII (20000/20010). Sommets : +0x68 de la fiche → une adresse par bloc de poids (+0x5C) → sommets de 28
  octets (position, normale, couleur ARGB) ; nœuds de 80 octets à l'en-tête +0x28 (+16 position, +64 sphère) ;
  sphère du maillage à +0xA4 (SvR) / +0xA8 (WWE '12) ; adresses comptées depuis l'octet 8. **`jboy.scale()`** et
  **`arena_convert.py --scale 10`** (modèles + 50003 + 50020) : le ring SvR ×10 donne exactement les sommets et
  sphères de celui de WWE '12 ; 189 modèles passés sans erreur.
  **v10** (depuis l'arène SvR d'origine, tout en une commande) : `--models --scale 10 --replace
  20000,20001,20002,20003,20010,951,952,953,956,960,964,973,974,995,996,997,999 --remove
  957,958,959,961,962,963,965,966,967 --textures mat_noize,rin_ep01,rin_tb00_n,rin_tb00_s` → 10,08 Mo. Lumières
  30000/31010 encore celles de SvR (format YMKs, positions sans doute à ×10 aussi). Installée le 2026-10-23.
- **v10 en jeu : plantage au chargement** (lecture de guest 0, lr 0x822FF498 dans `sub_822FF418`). Cette fonction
  lit une fiche `NODE MY_NAME = x PARENT_NAME = y` de 50020 (`sub_827E2DC0(lecteur, n)` = n-ième mot de la ligne,
  ici le mot 3 = nom, absent) : le lecteur s'est décalé dans le fichier. Seule différence de forme : `%.6g`
  écrivait des entiers (`0`, `-11`) alors que les fichiers du jeu ont toujours un point (`0.0`). Corrigé (toujours
  un point, pas d'exposant) → **v11**, même commande, installée. Si elle plante pareil, suspect suivant : les tables
  995-999 de WWE '12 (la v9 n'a jamais été essayée).
- **v11 en jeu : autre plantage, même phase** (écriture hors mémoire dans l'allocateur `sub_827D2548`, appelé pour
  une fiche de 224 octets par le lecteur de 50020 ; en-tête du tas écrasé par du texte). Le texte de 50020 a
  maintenant exactement la forme de l'original (mêmes mots par ligne, mêmes séparateurs). Décompression sur place
  vérifiée (simulation : marge de nos entrées = celle des entrées du jeu, ≤ 10 octets). Le lecteur de 50020 est
  donc sans doute victime d'une corruption antérieure ; suspect : les tables 995-999 de WM XXVII (décrites pour
  ses objets à elle). **v12** = v11 avec 995-999 de SvR (10,07 Mo), installée.
- **v12 en jeu : plantage dans le décodeur BPE** (écriture hors tampon, lr 0x827E1430 : même adresse que le contrôle
  WM XXVII stockée de 16,7 Mo). **Vraie cause des plantages v10 à v12 : la taille.** La limite n'est PAS ~10,9 Mo
  (la plus grosse arène) mais **la taille d'origine de l'arène remplacée** (comme le dit Kaiko pour SvR) : bg15
  = 10 063 872 octets ; la v8 (qui marchait) faisait pile 10 063 872 ; v10/v11/v12 = 10 074 112 à 10 080 256 →
  débordement qui abîme la mémoire à des endroits différents. Le format des nombres et les tables 995-999 n'étaient
  peut-être pour rien (à retester une fois la taille réglée).
  Le gâchis venait du paquet 400 (7 Mo) entièrement recompressé par notre BPE (~16 % moins bon) pour 4 textures.
  **`yukes_bpe.split()` / `compress_like()`** : garde tels quels les sous-blocs de Yuke's dont la tranche décodée
  est inchangée (taille décodée variable, ≤ 4 000 ; on suit leurs frontières) ; aller-retour du 400 SvR identique.
  **`add_textures()`** garde la taille de la table : un nom nouveau prend la fiche d'une texture qu'aucun modèle
  n'utilise plus (anciennes `rin_*` de SvR d'abord ; jamais `light*`, `titan*`, `mov*`), données ajoutées à la
  fin. **v13** = contenu de la v12 (995-999 de SvR), 9 953 280 octets (110 Ko de marge), 3 405 blocs sur 3 406
  gardés ; installée le 2026-10-23.
- **v13 en jeu : ça charge, l'arène WM 26 est à la bonne taille** (scène, rampe lumineuse, barrières au logo
  WM XXVI, marches, table des commentateurs ; 60 fps en match). Défauts : tout est **rouge-orangé pendant l'entrée** ;
  en match, **énorme nappe de lumière blanche et bleue** au milieu du ring. Banques de lumières YMKs (entrées
  16 octets à 0x114, petit-boutiste) : WWE '12 30000 = 274 pistes de 2 626 octets (101 images), 31010 idem ; SvR
  31010 = 96 pistes de 2 626 octets (même format) mais **SvR 30000 = 228 pistes de 54 octets** (format compact) :
  le jeu la lit sans doute de travers. **v14** = v13 + 30000/30001/31010 de WM XXVII (9 959 424 octets), installée.
  Attention : 50001 de SvR appelle les lumières 101-108 ; celles de WM XXVII vont de 100 à 106 et 120 à 131.
- **v14 en match (capture, 60 fps) : propre.** Ring complet et éclairé normalement, plus de nappe blanche, barrières
  WM XXVI, marches, table, foule de WM XXVII. **Reste : des défauts sur la scène** (capture à venir).
- **v14, entrées (4 captures)** : scène WM 26 complète (poutres, écrans avec les vidéos d'Orton et de Cena, caisses,
  rampe lumineuse, bandeau bleu étoilé = décor de WM 26 : 20000/20010 ne contiennent que la foule `aud_*`) ; Orton
  tout en rouge-orange, Cena tout en bleu (couleurs de leurs entrées). Bandeau « WrestleMania XXVII / Atlanta, GA »
  = écran du jeu pour le n° 15, pas dans l'arène. Défaut exact de la scène à préciser avec l'utilisateur.
- **Référence (capture SvR 2011 envoyée par l'utilisateur)** : fond de scène = **mur de pierres bleu bien éclairé**,
  nombreux faisceaux bleus/blancs/orange. Chez nous (v14) : mur presque noir, couleurs de WM XXVII → il faut les
  lumières de WM 26. **Correction de la v14** : les banques de lumières ont **le même format** dans les deux jeux
  (pistes de 54 octets = version abrégée « 79 63 » = même valeur 99 images, présentes aussi dans WWE '12). Image =
  26 octets : `0x79`, compteur de répétition, 3 flottants = **position** (SvR -230,5 contre WWE '12 -3 937 pour la
  piste 10/0), puis 12 octets (couleur, intensité…) ; des octets d'évènements (table de tailles à 0x10) peuvent
  séparer les images. **`scale_lights()`** (avec `--scale`) multiplie les positions de chaque image. **v15** = v13
  avec les lumières de WM 26 agrandies ×10 (9 963 520 octets), installée.
- **v15 en jeu** : en replay, **la scène de WM 26 est juste** (mur de pierres bleu éclairé, tous les faisceaux, comme
  la capture SvR). Mais en match la **nappe blanche revient** (elle venait donc des pistes de match de SvR, avec ou
  sans ×10), et pendant les entrées les **écrans (titantron, minitrons) bugguent** : les écrans 800/804/809 de
  WM XXVII ajoutés à la v1 (inutiles, le plantage d'alors venait des maillages) se mêlent à ceux de WM 26
  (801/803/806/807/808). **`--donor-lights 10,11,12,13,14`** : `merge_lights()` prend dans l'arène donneuse les
  pistes des jeux de lumières du match (10-14, d'après Kaiko ; 200 = entrées), même en-tête 0x110 et mêmes pistes
  (y = 0, 1, 10-13) dans les deux jeux ; réécriture sans changement identique à l'octet. **v16** = v15 + lumières
  de match de WM XXVII, sans 800/804/809 (`--add 980,30002`), 9 959 424 octets, installée.
  Commande complète : `--models --scale 10 --add 980,30002 --replace 20000,20001,20002,20003,20010,951,952,953,956,
  960,964,973,974 --remove 957,958,959,961,962,963,965,966,967 --donor-lights 10,11,12,13,14 --textures
  mat_noize,rin_ep01,rin_tb00_n,rin_tb00_s`.
- **v16 en jeu** : écrans de l'entrée corrects (murs vidéo de WM 26), scène belle ; **nappe blanche encore en match**.
  Mode d'emploi en japonais à la fin de 50001 (drapeaux par objet) : `a` additif, `z` sans z-buffer, `b` billboard,
  `m(n)` = animé par la piste n de la banque de lumières, `l(n)` = éclairé par la lumière n, `e` entrée seulement,
  `g` jeu seulement, `w` jeu : caméra des prises seulement, `p` projette une ombre, `r`/`y` copie miroir/tournée,
  `n` near-clip, `x` ne reçoit pas les projecteurs, `f(x.y.z)` disparaît près de la caméra, `i`/`j`/`o` ordre de
  dessin. Les cônes de WM 26 (230-269) = `a z m(101…120) b`. **Cause de la nappe** : `scale_lights()` multipliait
  toutes les pistes ; or beaucoup sont identiques dans les deux jeux (20, 30, 300, 450 octet pour octet ; cônes
  101-120 = (0, ±1000, 0), longueur du faisceau) → cônes 10 fois trop longs jusqu'au ring. Désormais seuls les
  jeux 10-14 et 200-299 sont agrandis. **v17** = v16 corrigée.
- **Foule de WM 26** : ses modèles (articulés, 17 os, 2 blocs de poids) n'ont qu'**un** tableau de sommets ; `scale()`
  ne suit plus que les adresses listées dans POF0 → 124/126 modèles convertis (2 sans maillage). 20001 (tables de
  répartition, octets) et 20002/20003 (textes) : pas de positions. **Compresseur** : blocs de Yuke's ≈ 1 100 octets
  décodés (médiane) ; `compress()` passe de 4 000 à **1 200** (foule : +0,5 % contre +13 %). **v18** = v17 avec la
  foule de WM 26 (`--replace` sans 20000-20010) : 9 857 024 octets, décompression sur place vérifiée (marge ≤ 6,
  comme le jeu), installée.
- **v18 en jeu : tout cassé** (entrée et match très sombres, spectateurs géants devant la caméra en match). Erreur :
  deux changements à la fois. La foule SvR agrandie est mal placée (le jeu la positionne sans doute par des données
  non trouvées : modèles 40000/40001/40100 du sous-PACH ? sièges ?) ; l'obscurité peut venir de la foule ou de
  `scale_lights` limité. Retour à la **v17** (lumières seules, foule de WM XXVII, construite avant le passage du
  compresseur à 1 200) pour tester une chose à la fois.
- **v17 en jeu : la nappe est toujours là** → ce n'était pas la longueur des cônes. Explication retenue : dans les
  pistes des cônes 101-120, les canaux y = 10-13 sont le **point visé** (SvR 4 677, -2 677, -11 054 ≈ 110 m en 10 cm ;
  lu en cm = 11 m → tous les projecteurs visent le ring). v13 : rien d'agrandi ; v15/v16 : tout, y compris les
  canaux 0/1 (longueur du faisceau, (0, ±1000, 0) dans les deux jeux) ; v17 : seulement les jeux 10-14/200+.
  **Nouvelle règle** : `scale_lights()` agrandit les **canaux y = 10-13 de toutes les pistes**, jamais 0, 1, 50+.
  **v19** = v17 avec cette règle (foule de WM XXVII, compresseur à 1 200) : 9 783 296 octets, installée.
- **v19 en jeu : nappe toujours là**, entrée inchangée (belle). Théorie du point visé fausse aussi (règle des canaux
  10-13 gardée). Vérification directe : **`--entrance-only`** ajoute le drapeau `e` (« seulement pendant les
  évènements ») aux objets donnés dans 50001. **v20** = v19 + `e` sur les cônes 230-269 et 300-305 (46 objets),
  9 783 296 octets, installée. Si la nappe disparaît : c'étaient bien les cônes, et la solution est trouvée.
- **v20 en jeu : match propre** (ring net, plus de nappe, 60 fps) → **c'étaient bien les cônes**. Reste : pendant la
  présentation sur le ring (speaker, encore un « évènement »), les cônes éblouissent toujours le ring. Pistes : les
  cacher aussi là (`w` ? autre drapeau), ou comprendre leur visée / leur intensité (rendu additif plus fort dans
  WWE '12 ?). Restent aussi la foule et les tribunes de WM 26 (v18 : foule SvR mal placée).
- **Shaders et champ +0x74 (2026-10-23)** : le drapeau `f(x.y.z)` n'est utilisé par aucune arène (piste abandonnée).
  Les cônes d'origine de WWE '12 restent affichés en match sans éblouir : le défaut vient des cônes SvR. **Toutes
  les arènes WWE '12 utilisent les shaders « décor »** `yBG_Default` (1 958 maillages sur 7 arènes), `yBG_UVScroll`,
  `yBG_BumpMap`, `yBG_Reflect` (+ `yProp_*` pour le ring) ; SvR utilise `yDefault`, `yUVScroll`, `yBumpMap`.
  Fiche de maillage WWE '12 (depuis son début, à 0x48 + i × 184) : +0x68 sommets, +0x6C poids, +0x70 UV, **+0x74 =
  le mot inséré = 2e adresse de la table de bandes (même valeur que +0x9C), toujours non nulle**, +0x78 index de
  matériau, +0x7C nom du shader (16 octets), +0x8C drapeaux, +0x94 nb de paramètres, +0x98 paramètres, +0x9C bandes.
  (L'ancienne note « lightmap à +124 » se trompait : +124 compté depuis 0x40 = +0x74 de la fiche.)
  `jboy.svr_to_w12()` y met maintenant l'adresse des bandes (déclarée dans POF0) au lieu de 0, pour **tous** les
  modèles ; `jboy.bg_shaders()` + **`--bg-shaders`** renomment les shaders en `yBG_*`. **v21** = v20 + ces deux
  corrections (shaders décor sur les 46 cônes seulement), 9 783 296 octets, installée.
- **v21 en jeu** : pas de plantage (le +0x74 rempli et `yBG_Default` sur les cônes sont acceptés), match propre ;
  plan d'ouverture : une lueur orange reste sur le ring (cônes ou lumières d'entrée d'Orton ?). L'utilisateur
  confirme : **ça éblouit encore pendant la présentation sur le ring**. Matériaux des cônes SvR : ambiante 1,5-2,0
  (WWE '12 : 1,0), additifs, 46 objets qui s'additionnent. **`jboy.dim()` + `--dim FACTEUR:ids`** multiplient le RGB
  de `g_f4MatAmbCol`/`g_f4MatDifCol` (paramètre type 0x0D, valeur à nom+20). **v22** = v21 + `--dim 0.3` sur les
  cônes, 9 785 344 octets, installée.
- **v22 en jeu : réglé.** Entrée de Cena : faisceaux colorés sur le ring, sans éblouissement ; match propre, 60 fps.
  **Commande de référence (WM 26 → bg15)** : `--models --scale 10 --bg-shaders CONES --dim 0.3:CONES --add 980,30002
  --replace 20000,20001,20002,20003,20010,951,952,953,956,960,964,973,974 --remove 957,958,959,961,962,963,965,966,967
  --donor-lights 10,11,12,13,14 --entrance-only CONES --textures mat_noize,rin_ep01,rin_tb00_n,rin_tb00_s`
  avec CONES = 230-269 et 300-305. Petit défaut vu : un cube blanc près des marches du ring. Reste : foule et
  tribunes de WM 26, spectateurs proches trop sombres.
- **EN PAUSE (décision de l'utilisateur, 2026-10-23)** : le résultat ne lui plaît pas assez. WM 26 retirée du jeu
  installé (`mods` ne contient plus que `HOW_TO_USE_MODS.txt`, WM XXVII d'origine revient) ; la v22 est rangée dans
  `Desktop\WWE '12 PC Port\arenes_svr2011_desactivees\bg15_wm26_v22.pac`, toutes les versions dans `work/arenes/`.
  **Où on en est** : l'outil fait tout le portage d'une commande (commande de référence ci-dessus) ; l'arène se
  charge à 60 fps, scène/écrans/cônes de WM 26 corrects. **À reprendre** : foule de WM 26 (v18 : convertie mais
  spectateurs géants devant la caméra, placement inconnu), spectateurs proches sombres, éclairage général (essayer
  `--bg-shaders` sur tous les modèles, pas seulement les cônes), cube blanc près des marches. **Leçon** : un seul
  changement par essai. Utile aussi pour le Mod Maker (1.0) et les arènes de WWE '13 (même format que WWE '12).
- **Pour la reprise : le code de Kaiko le sait déjà** (`work/kaiko-svr2011/port/modmaker/svrfmt/wwe13.h/.cpp`,
  conversion WWE '13 → SvR 2011, le sens inverse) : collision HMD en cm, arbre de visibilité, **fichiers de places de
  la foule dans le paquet de la foule (cm)** (sans doute la cause des spectateurs géants de la v18), banque de
  lumières, et un plafond de géométrie (≈ 65 450 sommets) dans `arena.h`. Voir `docs/mod_maker.md`.
- **RTX de l'utilisateur** : PC Lenovo 83JH, RTX 5070 Laptop **absente pour Windows** (mode GPU du PC = intégré) ;
  les essais tournent sur l'Intel UHD. À rallumer par l'utilisateur dans Lenovo Vantage / Legion Space.

Liste confirmée par un fil de smacktalks.org (« WWE 12 Arenas ID », collée par l'utilisateur), qui ajoute :
bg24/bg26 inutilisables, bg25 RAW avec un cadre blanc autour du ring, bg31 SmackDown éclairage Sin Cara,
bg32 arène de test RAW, bg66 fond de Create a Superstar, bg67 restaurant, bg68 bureau du GM, bg76/77/79
noirs, bg78 coulisses, bg200/bg201 Create an Arena. (Ceux de 24 à 32 n'existent pas sur le disque.)

### Stipulations : I Quit, Special Referee, Gauntlet (2026-10-22, lecture seule)

Étude sans compiler ni lancer le jeu : textes de `string.pac`, arbre de `menu/menu.pac`, code recompilé.
Outils (lisent les données du joueur, ne les copient pas) : `tools/dev/strpac.py` (toutes les tables de
textes → TSV) et `tools/dev/menutree.py` (arbre des menus avec les noms).

**La table `[gest+556]` n'est PAS le catalogue des types de match.** `sub_82782AD8` la construit avec
`+560 = 10` entrées de 52 octets seulement (remplies depuis l'objet `[+548]`) ; `sub_827829B0` l'indexe
par `+72` (borné par `+76`) × 10 + `+56` (< 10). Ce sont des fiches par place/participant (d'où le drapeau
« pas de sang » à +32), utilisées par un automate d'états (`sub_82783xxx`, champs +564/+568/+572).

**Catalogue des types de match** : textes **27400–27520** (121 types internes), indexés par l'octet 0 d'une
fiche de match de 104 octets (`sub_82554B68`, fiches à objet+3248 : `27400 + octet`). Extraits :
54 IRON MAN, 59 LAST MAN STANDING, 67 SUBMISSION, 76 MONEY IN THE BANK, 81 INFERNO, **82 GAUNTLET**,
**83 SPECIAL REFEREE**, 84–90 ARENA BRAWL. **Pas de I QUIT.** D'autres listes de noms existent (textes
85–135 et 40700–40786, avec LUMBERJACK, THE DIRT SHEET BRAWL, GM OFFICE… = Road to WrestleMania /
coulisses) : SPECIAL REFEREE y figure aussi (96, 40775), LUMBERJACK (82, 40780).

**Menus d'exhibition** (`menu.pac`, 2e copie à 0x154800 ; même chose sous MATCH CREATOR, pages 28–43) :
- HANDICAP (page 11) = ONE ON TWO, ONE ON THREE, TWO ON THREE TAG, **GAUNTLET** (fiche 74, lien 0x7D0).
- SPECIALTY (page 12) = CHAMPIONSHIP SCRAMBLE, FREE BRAWL, ROYAL RUMBLE, MATCH CREATOR.
- Aucune entrée SPECIAL REFEREE, LUMBERJACK ni I QUIT.

**Fin de match** : résultats possibles (« BY WAY OF », textes 2942–2950) = PINFALL, COUNT OUT, TIME OUT,
**GIVEUP**, K.O., DQ, REFEREE STOP, FIRST BLOOD, SUBMISSION. Match Creator > WIN CONDITION (3356–3366) =
PIN AND GIVE UP, 2 OUT OF 3 FALLS, IRONMAN, LAST MAN STANDING, FINISHER MATCH, FIRST BLOOD, FLAMING TABLE,
CLIMB OUT OF CAGE, ELIMINATION, FALLS COUNT ANYWHERE ; RULES : PIN ONLY / **SUBMISSION ONLY** / NO EXITING.
Les conditions sont donc des réglages combinables, pas du code par type : bonne base pour de nouvelles
stipulations « assemblées ».

**Ce qu'il faudrait faire :**
- **Gauntlet** : rien à ajouter, il existe. Demander à l'utilisateur ce que la version SvR 2011 (de Kaiko ?)
  a de plus : plus d'adversaires, format « le gagnant reste »… Ça relèverait alors de « plus de catcheurs ».
- **Special Referee** : 1) test en jeu (avec accord) : forcer le type 83 sur une fiche de match (crochet sur
  la mise en place) pour voir si la logique d'arbitre jouable existe encore ; 2) si oui, l'offrir dans un
  menu. Ajouter une entrée à menu.pac casse la navigation (voir CLAUDE.md, 8e entrée du menu principal) :
  plutôt une entrée existante détournée ou une option dans notre écran. Si la logique n'existe plus :
  comptes, arbitre contrôlé, IA, animations = très lourd (reprise de WWE '13/2K14 en local seulement).
- **I Quit** : version simple = match « SUBMISSION ONLY » renommé, sans DQ ni décompte, fin par GIVEUP,
  avec un texte/son à la place du micro (moyen ; il faut trouver où le type fixe ses règles). Vraie version
  avec l'arbitre qui tend le micro et les animations : très lourd (animations et logique à reprendre de
  WWE '13/2K14, extraites en local depuis la copie du joueur, jamais publiées).
**Test en jeu Special Referee (2026-10-22, accord de l'utilisateur, 720p)** : sonde
`port/src/native/wwe12_kind_probe.cpp` (dev, `WWE12_DEV_PROBES`), lanceur `tools/dev/lancer_test_arbitre.bat`.
- 1er essai, crochet sur `sub_826D4888` : sans effet. Cette fonction ne sert qu'aux fiches d'Univers et des menus
  (type 119 = vide, 40 fiches au démarrage) ; l'exhibition écrit le type ailleurs (plusieurs écrivains :
  `sub_82449428`, `sub_826F8D08`, `sub_823E3220`).
- 2e essai, crochet sur le lecteur `sub_8267CF38` : l'exhibition TRIPLE THREAT NORMAL = **type 13** (1 contre 1 =
  type 0). Remplacé par 83 au lancement du match : **pas de plantage, match jouable à 60 fps, mais rien de
  visible** : 3 catcheurs qui se battent et l'arbitre habituel (capture de l'utilisateur, ring Nitro).
- Lecture : le type seul ne suffit pas. Le rôle « arbitre » d'un catcheur se décide sans doute à la mise en place
  des participants (avant notre remplacement, ou dans les fiches de place à +16). Indices : `sub_8227EEC0` teste
  la place `r27 == 3` + l'octet 33 de la fiche, `sub_82328E28`/`sub_823292C8` mettent une valeur 4 dans un tableau
  de 4 rôles quand le type est 83, `sub_821D0530` lit l'acteur+3428.
- **Rôles des participants (3e essai, même jour)** : `sub_822CDF78(type, fiche, …)` crée les personnages du match ;
  type 1 = humain (classe `sub_82213DB0`, ou `sub_823A3310` si fiche+6 == 4). `sub_82213DB0` recopie l'octet
  **fiche+6 = rôle** dans **acteur+3428**. Journal d'un TRIPLE THREAT : n° 0-2 = les 3 catcheurs (rôle 0),
  **n° 3 = l'arbitre normal (rôle 1)**, n° 4-5 = rôle 4 (hors ring, commentateurs/chronométreur ?).
  **Rôle 1 = arbitre.** En type 83, le jeu traite le rôle 1 à part (`sub_824EFA78` : vrai pour 0/2/3, et pour 1
  seulement en type 83 ; `sub_821D0530` saute un traitement quand rôle 1 et type 83).
- Donner le rôle 1 au 3e catcheur à sa création (sonde `wwe12_ref_slot`/`wwe12_ref_role`) : écrit (journal
  « n 2 role 0 -> 1 ») mais **il se bat normalement** (l'utilisateur jouait le 1er) ; il y a alors 2 arbitres.
- Pistes suivantes : `sub_82386168` (qui teste aussi le Gauntlet) remet acteur+3428 à une valeur (r26) à
  3 endroits, sans doute au début du match → réécrire le rôle **après** ; le comportement « arbitre » (IA,
  compte des tombés) dépend peut-être aussi de la classe ou d'autres champs ; l'arbitre normal devrait ne pas
  être créé en type 83 (`sub_82328E28` retire l'entrée 102 de sa liste de 4 en type 83 : modèle de l'arbitre ?).
- **4e et 5e essais** : liste des personnages du match = **0x8317B770** (9 pointeurs : 0-5 catcheurs,
  6 = arbitre rôle 1, 7-8 rôle 4 ; tous au rôle 10 en fin de match). Le rôle 1 donné à la création est
  **remis à 0 en moins de 0,5 s** (par une mise en place des participants, pas `sub_82386168`, qui ne le remet
  à 0 que pour Gauntlet/Rumble/drapeau match+60). Réimposé toutes les 100 ms (`wwe12_ref_force`) : il tient
  tout le match, l'acteur prend aussi +998 = 1 comme l'arbitre, pas de plantage. **À l'écran** (utilisateur) :
  le catcheur ne se bat presque plus, tente parfois une prise que les autres « refusent » ; le vrai arbitre est
  toujours là et c'est lui qui arbitre. → Rôle 1 = **non-combattant/arbitre**, mais le travail d'arbitre
  (compte des tombés, IA) va à **un** arbitre désigné ailleurs (sans doute la case 6 ou un pointeur global).
- Prochaines étapes : 1) trouver qui remet le rôle à 0 juste après la création (piège d'écriture sur
  acteur+3428) pour le régler proprement ; 2) trouver le pointeur « arbitre du match » (qui compte les tombés :
  lecteurs de la case 6 de 0x8317B770) et le faire pointer sur le catcheur ; 3) ne pas créer l'arbitre normal
  en type 83 (liste de `sub_82328E28`, entrée 102) ; 4) donner une IA d'arbitre au catcheur s'il est à l'ordinateur.
- **Modèle WWE '13 (préparé 2026-10-22, rien compilé)** : `work/wwe13/` (hors dépôt) contient le recomp
  PromethenDev/wwe13-recomp (BSD-3) et son SDK (`tools/rexglue` = fork rexglue-sdk-yukes @987593c + leurs 99
  patchs, 22 sous-modules, cloné en `core.autocrlf=false` sinon les patchs échouent). Le jeu de l'utilisateur :
  `Desktop\WWE13-Recomp-x64\WWE 13\` (default.xex + default.xexp TU 2.0.1.0, empreinte vérifiée). Prochaine
  étape **sur secteur** : compiler seulement leur outil `rexglue` (leurs scripts visent Linux, adapter avec notre
  `tools/env.ps1`), copier xex+xexp dans `work/wwe13/wwe13-recomp/game/`, lancer `rexglue codegen
  wwe13_config.toml`, puis chercher le Special Referee (type, rôle 1, arbitre officiel) dans le C++ généré.
  Ce code généré ne sort jamais de `work/`.
- **Comparaison avec WWE '13 (code généré le 2026-10-22 dans `work/wwe13/wwe13-recomp/generated`, TU 2.0.1.0,
  outil `work/wwe13/tools/rexglue/out/win-amd64/Release/rexglue.exe`)** : la famille des tests de type est à
  `0x82285850` (76, 81, 20-23, 39/40, 67, 82 Gauntlet, 38…). À la place de notre `sub_82A2FB18` (type == 83),
  WWE '13 a **`sub_82285A60` = octet +45 du match en cours ≠ 0** : l'arbitre spécial y est une **option** qui
  s'ajoute à un match, plus un type. Surtout, ce test est appelé depuis **49 fonctions dans WWE '13 contre 5 dans
  WWE '12** : la logique (compte des tombés par le catcheur, IA, etc.) a été **retirée de WWE '12**, il n'en reste
  que des traces.
- **Conclusion Special Referee : très lourd.** Il faudrait refaire dans WWE '12 l'équivalent d'environ 45 points de
  logique de WWE '13 (chacun à comprendre puis réécrire pour les structures de WWE '12). Faisable morceau par morceau
  avec WWE '13 comme modèle, mais c'est un chantier long ; à reprendre seulement si l'utilisateur le décide.
  Liste des appelants WWE '13 de `sub_82285A60` (point de départ) : sub_8228ED68, sub_82291708/968/B68,
  sub_82292268, sub_822D7428, sub_822E19E8/2628, sub_822F5C48, sub_8230A060, sub_8230C510, sub_824015C0,
  sub_8243DF58, sub_82470300, sub_824979C8, sub_824A1FC8, sub_824BB318, sub_824CA710, sub_824F38C8,
  sub_824F4C88, sub_82500CF0, sub_82514260, sub_82524B18, sub_825378C8, sub_825398B0/9970, sub_8253CD18,
  sub_8253DDD8, sub_8254C820, sub_82564348, sub_82568370, sub_8256FAE0, sub_8257B2C0/B708, sub_825FC108,
  sub_826948E8, sub_82699CD8, sub_82709FD8, sub_8274E1C8, sub_82904168, sub_829C6960, sub_829EA5D8,
  sub_82A555C0, sub_82A72B98/C70/EE8, sub_831541A0.
- **Carte du Special Referee de WWE '13 (2026-10-22)** :
  1. Option du match : octet **+45** du match en cours (`sub_82285A60`). Liste des personnages : `0x83408DC0`.
  2. Un catcheur est l'arbitre si **son rôle (acteur+3336 dans WWE '13 = acteur+3428 dans WWE '12) vaut 1** et
     qu'un état global ([0x8347C554]+100) vaut 2 : `sub_82537D20(acteur)`, **54 appels** (IA, prises, tombés…).
     `sub_822D7428` = « un des 6 catcheurs est-il l'arbitre ? ».
  3. Un **composant de comportement « SREF »** (étiquette 0x53524546) accroché à l'acteur (recherche
     `sub_82B1ABB8(acteur+1088, 'SREF')`, objet à +24) : jauge à +116 (`sub_8256E3E8` la fait varier,
     `sub_8256E498` = jauge pleine), **27 fonctions** l'utilisent (dont le bloc `sub_826E84E8`–`sub_826EA2A8`,
     `sub_825D9388/9580/9780`, `sub_8256E548`, `sub_8256F358`, `sub_82550568`, `sub_824BB318`, `sub_829EA5D8`).
  4. **WWE '12 n'a aucune trace de SREF** (étiquette absente du code) : seuls restent le type 83, le rôle 1 et
     5 tests. Le rôle 1 est donc la seule pièce commune ; tout le comportement d'arbitre-catcheur est à refaire.
- **Bilan honnête** : porter le Special Referee de WWE '13 = réécrire pour WWE '12 un composant entier (≈ 30 à 80
  fonctions selon ce qu'on garde, IA comprise), en adaptant chaque structure (décalages différents, ex. rôle
  +3336 → +3428). Variante plus légère imaginable : arbitre spécial **contrôlé par un joueur** seulement (pas d'IA),
  l'arbitre normal retiré, et le compte des tombés déclenché par le joueur-arbitre via la logique de compte de
  WWE '12 ; à évaluer avant de s'engager.
- **Version légère, essais du 2026-10-22 soir** :
  - L'affectation des manettes (`sub_826E09D8`, `sub_826E0B18`) **saute les rôles 1 et 3** : un arbitre ne peut pas
    être joué tant qu'on n'y touche pas.
  - L'arbitre normal et les 2 rôle 4 sont **créés au premier match puis réutilisés** : le match de type 83 doit être
    le premier après le lancement pour agir sur leur création.
  - **Ne pas créer l'arbitre normal = plantage** (`sub_822D0618` écrit aussitôt dans l'acteur, lr 822D06D8 ; écriture
    à 0x6E0).
  - Test préparé, **pas encore lancé** : l'arbitre normal créé avec le **rôle 4** (cvar `wwe12_ref_normal_role`),
    le 3e catcheur seul rôle 1. Pour le refaire : `WWE12_KIND_PROBE_LOCAL` à 1, compiler, `lancer_test_arbitre.bat`,
    TRIPLE THREAT NORMAL en premier match.
- Suite possible : remplacer le type plus tôt (dès l'écran de choix des catcheurs) et lire ces fonctions pour
  trouver le champ « rôle » ; sinon chercher la logique dans WWE '13/2K14 (l'utilisateur les a, en local
  seulement).

- **Risque** : faible tant qu'on ne touche qu'aux réglages d'un match ; moyen dès qu'on force un type
  que les menus ne proposent plus (code mort possible, plantage en match) ; Univers et sauvegardes
  non concernés.

### Étude chiffrée I Quit et Special Referee (2026-10-26, lecture seule, demande de Main)

Rien lancé, rien compilé. Jeux regardés : WWE '13 (code généré dans `work/wwe13`), WWE 2K16 (code généré
de l'utilisateur, `Desktop\WWE 2K16 Recomp\wwe2k16-pc-src\generated`), WWE 2K14 (exécutable seulement, pas de
code lisible), 2K15/2K17 (ISO seulement), SvR 2011 (notes de Kaiko, `work/kaiko-svr2011/docs/MATCH_TYPES_RESEARCH.md`,
copie locale du 2026-10-22). Règle : on lit pour comprendre, on réécrit notre code ; rien de copié ni publié.

**Ce que WWE '12 a déjà (vérifié dans ses fichiers et son code)** :
- **Table des règles = la même que SvR 2011** : `misc.pac`, bloc `RUL\0` à 0x9630 (copie à 0x46C630),
  **119 fiches de 108 octets** (100 dans SvR), puis `OPT\0` à 0xC86C (119 fiches). Même début de fiche que
  chez Kaiko : +0 nombre de personnes (u16), +2..+19 six places de 3 octets {numéro, équipe, **genre**}.
  Lecteur : `sub_82A30590(id)` = `[0x8324E238] + id × 108` (l'équivalent de `sub_828B4A48` de SvR).
- **Fiche 83 SPECIAL REFEREE intacte** : 3 personnes, places (0,0,0) (1,1,0) **(2,2,1)** : la 3e place est de
  genre 1 = arbitre spécial, comme dans SvR. Notre essai du 22 avait forcé le type 83 sur une fiche déjà
  construite depuis le TRIPLE THREAT (13) : la 3e personne n'a jamais reçu le genre 1 par la règle.
- **Fiche 67 SUBMISSION** (1 contre 1) et sa ligne de menu **ONE ON ONE > SUBMISSION** (fiche de menu 44) :
  le match à soumission existe et se joue déjà.
- **Liste des types de match construite dans le code** : les 124 feuilles de menu de match (lien 0x7D0) ont
  +0x5C = 0, contrairement à SvR où la règle y est écrite. La correspondance est dans **`sub_82449898(menu,
  ligne, &a, &b)`** : `switch` sur la page (menu+48, 0 à 51) puis sur la ligne, rend deux numéros (on y voit
  les règles 54 IRON MAN, 59 LAST MAN STANDING, 67 SUBMISSION, 81 INFERNO et des numéros > 119, sans doute un
  2e catalogue ; lequel des deux est la règle selon la page : à confirmer au 1er essai). 9 appelants
  (`sub_8244B800`, `sub_82452E68` ×2, `sub_82456590`, `sub_82DD51F8`, `sub_82DD5920`, `sub_82DD8D40`,
  `sub_82DD9308`, `sub_82DD9870`, `sub_82DE2178`). **C'est l'équivalent du crochet de Kaiko** : un crochet
  sur cette fonction fait jouer une autre règle depuis une ligne existante, sans toucher à `menu.pac`.
- Fin de match : champ « type de fin » à +68 de l'objet de l'écran de résultat (`sub_82505BC8` construit le
  tableau des textes 2942–2950 et l'indexe par +68). 3 = GIVEUP.

**I Quit**
1. **Meilleure source : WWE '13.** C'est le successeur direct. Sa liste des fins (`sub_82316198`) est celle de
   WWE '12 **plus une valeur, 9 = texte 2951 « I QUIT »**, au même champ +68 ; elle vient d'une **raison de fin
   n° 12** (WWE '12 s'arrête à 11 : `sub_825065F0` contre `sub_82316CC0` de WWE '13 ; raison rangée à
   **match+336**, même décalage dans les deux jeux). **Correction (2026-10-26, même jour)** : ce n'est PAS une
   version simple, voir « I Quit dans WWE '13 » plus bas. 2K14/2K16 : moteur plus éloigné (2K14 sans code
   lisible). SvR 2011 : coupé, il ne reste que le nom (Kaiko : « Cut, with only names left »).
2. **Travail (version légère)** : environ **4 crochets** à écrire, **0 animation, 0 fichier** ; 1 texte du portage
   (« I QUIT », numéro libre à partir de 43100). Décalages : +68 (fin), +336 (raison) et le marquage du match.
3. Entrée : une ligne qui pointe sur la règle 67 via `sub_82449898` (renommer une ligne existante, ou insérer
   une ligne en mémoire à la manière de Kaiko : à essayer, l'insertion a cassé la navigation du menu principal).
4. **Plan (1 à 2 séances, risque de plantage faible)** :
   a. ligne « I QUIT » = règle 67 marquée par notre code ; essai en jeu : le match se lance et se joue ;
   b. pendant le match : pas de DQ ni de décompte à l'extérieur (octets des règles du match en cours, comme le
      « Three Stages of Hell » de Kaiko) ; essai ;
   c. fin par GIVEUP dans un match marqué → texte « I QUIT » sur l'écran de résultat et dans l'annonce
      (crochet sur le gestionnaire de textes `sub_821A4400` ou sur +68) ; essai ;
   d. Univers et historiques gardent GIVEUP (3) : rien à changer dans les sauvegardes.
   Version encore plus légère : seulement l'étape c, sur le match SUBMISSION existant (1 séance).
5. **Faisable maintenant : oui**, rien ne bloque. 1 séance pour a + c, 2 avec b et les essais.

**Special Referee**
1. **Meilleure source pour la logique : WWE '13** (seul jeu local avec le comportement complet et du code
   lisible). SvR 2011 n'apporte pas de logique : comme WWE '12, il garde la règle 0x53 (3e place genre 1) et
   un seul test en match (`sub_8260E150`), jamais essayé par Kaiko ; mais il confirme le mécanisme « genre 1
   dans la fiche de règles ». 2K14/2K16 : plus éloignés.
2. **Chiffres** :
   - **Version légère** (sans WWE '13) : 5 à 8 crochets. Faire jouer la règle 83 depuis une ligne (TRIPLE
     THREAT par ex.) ; recopier les champs de sélection de la règle 13 dans la fiche 83 pendant le choix des
     catcheurs (Kaiko a dû le faire pour Lumberjack, sinon l'écran montre 2 places et plante au 2e choix) ;
     voir si la 3e personne devient l'arbitre du match (case 6 de 0x8317B770) ; affectation des manettes
     (`sub_826E09D8`/`sub_826E0B18` sautent le rôle 1) pour le jouer. 0 animation nouvelle si l'arbitre
     utilise celles de l'arbitre normal (même classe d'acteur, comportement selon le rôle : à vérifier).
   - **Version complète** (WWE '13) : réécrire le composant SREF (27 fonctions) et adapter environ 50 points
     d'appel (54 appels de `sub_82537D20`, 49 de `sub_82285A60`) : **≈ 30 à 80 fonctions**. Décalages connus :
     rôle +3336 → +3428, liste des acteurs 0x83408DC0 → 0x8317B770, option du match +45 → type 83, état
     global [0x8347C554]+100 → à trouver dans WWE '12. Animations de l'arbitre spécial (compte rapide, refus de
     compter…) : à inventorier dans WWE '13, probablement absentes de WWE '12.
3. Entrée : même crochet `sub_82449898` (règle 83 au lieu d'une règle existante).
4. **Plan** :
   a. (1 séance) jouer la règle 83 depuis une ligne + champs de sélection de la 13 ; journal des rôles et des
      9 acteurs. Résultat attendu : 3 choix, la 3e personne au rôle 1 dès la création ;
   b. (1 séance) qui arbitre ? Si l'arbitre normal (modèle 102, retiré par `sub_82328E28` en type 83) n'est
      plus là et que la 3e personne compte les tombés : version légère presque finie ;
   c. (1 à 2 séances) manette pour l'arbitre ; compte déclenché par le joueur ;
   d. (8 à 12 séances, seulement si on veut la version complète) réécriture du comportement de WWE '13.
   **Légère : 2 à 4 séances, risque moyen** (sélection et création des acteurs : plantages possibles mais
   faciles à voir). **Complète : 10 à 15 séances, risque élevé.**

**Décision (2026-10-26)** : les deux matchs sont prévus pour la **1.1**.

**Recommandation** : I Quit en premier (1 à 2 séances, faible risque), pour la 1.0. Special Referee : faire
l'essai a/b de la version légère (2 séances) ; on décide après, la version complète irait en 1.1 au plus tôt.

**Les deux matchs dans WWE '13, regardés de plus près (2026-10-26, lecture seule)**

Découverte : dans WWE '13, **le comportement en match des personnages est écrit en Lua, en texte clair**
(`pac/yase/InGameLua.pac`, ≈ 1 700 changements d'état `ChgState`, fonctions natives `yGame.*`, `yChar.*`,
`yPad.*`, `ySound.*`). **WWE '12 n'a pas ce fichier** (pas de dossier `yase`, aucun script d'état dans ses
`.pac`) : chez lui tout est en C++. On ne peut donc pas reprendre le script ; il sert de **plan lisible** pour
écrire notre C++ (lu en local, jamais copié ni publié ; copie de travail dans le scratchpad uniquement).

*I Quit dans WWE '13* : une vraie stipulation, pas un simple texte.
- Le match est marqué par **la règle n° 1 du match = 3** (`yGame.GetRule(1)==3`, commenté « i quit match » ; la
  règle 0 = 67 marque le match SUBMISSION). Les menus ont des cartes de règles `MATCH_ON_1_1_IQUIT_COUNT/POINT`
  et `MATCH_OFF_1_1_IQUIT_TIME` (absentes de WWE '12, qui a celles de SUBMISSION).
- Déroulé : quand l'adversaire est au sol, le saisir **appelle l'arbitre** (`StateIQuitMiniGameCallReferee`) ;
  l'arbitre vient avec une animation dédiée (**mouvements 13200/13201**) et **pose la question**
  (`CheckIQuitAskHim`, commentaire `E_SC_IQUIT_ASK`) ; le bouton de provocation près de l'adversaire déclenche
  **une menace** (`StateIQuitMiniGameMakeAThreat`, `IQuit_CheckMakeAThreat`) qui enchaîne sur une prise de
  soumission ; une **boucle de soumission** (`SubmissionLoop_A`, `IQuitSubmissionRelease` pour lâcher) ; le
  perdant passe en `IQuitMiniGameLoop_B` au lieu de l'abandon normal ; la fin = raison 12 → « I QUIT ».
- Pièces : 7 états Lua (≈ 100 lignes), **5 fonctions natives** (`IQuit_CheckMakeAThreat`,
  `IsIQuitMiniGameExist`, `IQuitSubmissionRelease`, `CheckIQuitAskHim`, et la fin n° 12), la carte de règles,
  le commentaire, **les animations de l'arbitre au micro et de la menace** (à chercher dans WWE '12 ; sinon il
  faudrait les importer de WWE '13 = très lourd).
- **Version « comme WWE '13 » dans WWE '12** : réécrire en C++ ces états dans la machine d'états native de
  WWE '12 (à trouver : là où il gère l'abandon) + la fin n° 12 + la carte. **4 à 6 séances si les animations
  existent déjà dans WWE '12**, bloqué (import d'animations) sinon. La **version légère** (plus haut, 1 à 2
  séances) reste la bonne première étape : c'en est le socle (fin n° 12, marquage, texte).

*Special Referee dans WWE '13* : le script confirme la conception et la simplifie.
- `yGame.IsSpecialReferee(acteur)`, `GetSpecialRefereeState()` avec **8 situations** : regarder, compter un
  tombé, compter à l'extérieur, compte au sol (Last Man Standing), compte de DQ, vérifier une soumission,
  prononcer la DQ, avertir un manager. Selon la situation, la touche d'action du joueur-arbitre lance **les
  mêmes états que l'arbitre normal** (`StateRefereeFallCountStart`, `…RingOutCount`, `…DqCount`,
  `…LastManCount`, `…SubmissionCheckB/C`, `…PronounceDq`, `…CautionManager`), plus deux à lui : **compte
  rapide/lent** (`IsRefereeCountSliding`) et **« screw job »** ; lâcher la touche annule le compte
  (`IsAbortFallCount`). Il peut aussi quitter le match (`IsSpecialRefereeLeaving`).
- Cartes de règles `…SPREF_COUNT/POINT` pour normal, Extreme Rules, HIAC, Iron Man, Last Man Standing et
  Submission (absentes de WWE '12).
- **Conséquence pour WWE '12** : l'arbitre spécial n'a pas besoin d'un comportement neuf, il **pilote les
  actions de l'arbitre normal**, qui existent déjà en C++ dans WWE '12. Le gros du travail = (1) savoir dans
  quelle situation est le match (équivalent de `GetSpecialRefereeState`), (2) brancher la touche du joueur sur
  l'action d'arbitre correspondante, (3) le choix de l'arbitre dans la fiche 83. Le composant SREF (27 fonctions)
  sert surtout l'IA et la jauge ; on peut s'en passer pour un arbitre **joué**. Nouvelle estimation :
  **version jouable fidèle à WWE '13 (sans IA de l'arbitre spécial) : 4 à 6 séances**, après l'essai a/b ;
  l'IA (arbitre spécial à l'ordinateur) en plus : 4 à 6 séances.
