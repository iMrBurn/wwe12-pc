# SvR 2007 à 2010 dans le Mod Maker : étude (2026-10-31)

Demande de l'utilisateur : porter dans WWE '12 les catcheurs et les arènes des jeux d'avant SvR 2011. Il a les ISO
Xbox 360 de SvR 2007, 2008, 2009 et 2010 (et 2011) dans `Desktop\WWE ISOS`. Étude seulement : rien n'est codé.

## Ce qu'il y a sur les disques

| Jeu | Titre | Catcheurs (`pac/ch`) | Arènes (`pac/bg`) |
|---|---|---|---|
| SvR 2007 | 545107E0 | 81 `chNN.pac` | 8 `bgN.pac` (plusieurs scènes par fichier) |
| SvR 2008 | 5451080B | 74 `chNN.pac` | 45 `bgNN.pac` |
| SvR 2009 | 54510826 | 86 `chNNN.pac` | 45 `bgNN.pac` |
| SvR 2010 | 54510844 | 86 `chNNN.pac` | 52 `bgNN.pac` |

## Ce que font déjà les outils de Kaiko (`Desktop\SVR 2011 KAIKO\Mod Maker Tools`, Python)

Ils portent ces jeux **vers SvR 2011** (son portage PC) :

- **Catcheurs.** `svr10_char.py` : SvR 2010 = même format que 2011 (EPK8 / EMD / PACH / JBOY, même squelette) ;
  seules l'animation du visage (0x64, « JUDE », remplacée par la copie générique de 2011) et la taille du portrait
  (0xc8 / 0xc9) changent. `svr08_char.py` (2008, et 2009 par le même chemin) : EPAC / PACH / JBOY, même squelette et
  mêmes unités, mais anciens shaders et formats de sommets, un seul jeu de textures, pièces de tenue en JBOY à part :
  table de nœuds de 2011, palettes par nom, blocs de shaders de 2011, textures rangées comme 2011, Mk tiré de l'AO,
  pièces fusionnées. `svr07_char.py` : comme 2008, mais une texture par partie du corps (46 pour Steve Austin) cuites
  sur les planches de 2011 (couleur 2048 × 1024, normales 1024 × 1024, Mk 512 × 256).
- **Catcheur SvR 2010 complet** (`svr10_superstar.py`, 988 lignes) : modèle, rendus (DLC_HD.pac SSFB / SSFC), thème
  (music1.yaf, XMA2), vidéo d'entrée (Bink 320 × 320 telle quelle), notes, capacités, prises (`svr10_moves.py`,
  1461 lignes : table des cases 2010 → 2011, prises manquantes portées), entrée de 2011 du même nom.
- **Arènes** (`arena_port.py`) : `svr2010_convert` (le fichier presque tel quel) et `svr2008_convert` pour 2007,
  2008, 2009 (2007 : une scène d'un fichier à plusieurs scènes) ; elles prennent dans SvR 2011 la foule, les images
  de table, l'écran VS et l'éclairage (d'où le besoin du disque SvR 2011). Statut chez Kaiko : « ready », RAW testé.
- **Entrées** : `svr08_entrance.py` (EVP / EVT de 2008 → 2011, 674 lignes), `svr10_entrance.py`.

## Le chemin pour WWE '12

Le Mod Maker porte déjà **SvR 2011 → WWE '12** : catcheurs (`port_svr.h`, vu en jeu : CM Punk) et arènes
(`port_svr_arena.h`, vu en jeu : WM 26, Breaking Point). Donc :

**SvR 2007-2010 → format SvR 2011 (méthode de Kaiko) → WWE '12 (notre portage existant).**

Il n'y a rien à refaire pour la seconde moitié. La première moitié doit passer en C++ dans le Mod Maker (les joueurs
n'ont pas Python), comme on l'a fait pour `yanm_decode.py` et `rig_map` : **code tiers, il faut l'accord de
l'utilisateur et le crédit à Kaiko** (sa permission connue couvre son travail D3D11 ; pour ces outils, lui
redemander ou s'en tenir aux idées).

## Ordre proposé et effort

| Étape | Contenu | Effort |
|---|---|---|
| 1 | Page « Mes disques » : reconnaître SvR 2007-2010 (numéros de titre ci-dessus) | court |
| 2 | Catcheurs SvR 2010 → WWE '12 (visage et portrait seulement) | 1 séance |
| 3 | Arènes SvR 2010 → WWE '12 | 1 séance |
| 4 | Catcheurs SvR 2008 / 2009 (shaders, textures, pièces de tenue) | 2 séances |
| 5 | Arènes SvR 2007 / 2008 / 2009 | 2 séances |
| 6 | Catcheurs SvR 2007 (textures cuites sur planches) | 1-2 séances |
| 7 | Entrées et prises (2008, 2010) | plus tard, gros |

Chaque étape : vérifiée hors du jeu, puis un essai en jeu par l'utilisateur.

## Décision et avancement

- **2026-10-31 : l'utilisateur accepte** de reprendre en C++ la méthode des outils de Kaiko, avec crédit.
- **Étape 1 faite** : « Mes disques » reconnaît SvR 2007 à 2010 (`game_discs.h`, « bientôt » sauf ce qui marche).
- **Étape 2 faite (vérifiée hors du jeu, pas encore en jeu)** : les catcheurs de SvR 2010 passent **tels quels** par
  le portage SvR 2011 (`port_svr::PortAttire` : ch104 Jericho, ch100, ch125, ch139 Cena et ses 4 tenues) ; leurs noms
  (chEtc.pac, 334 fiches), portraits (DLC_HD.pac SSFA/SSFB/SSFC) et vidéos (movies/titantron/NNN.bik) sont rangés
  comme dans 2011. Page NOUVEAU CATCHEUR : apparence « SvR » puis SvR 2011 ou SvR 2010 (`svr_game_`, gardé dans les
  projets). Prises de 2010 pas encore lues (ses cases ne sont pas celles de 2011 : table de `svr10_moves.py`).
- **Étape 3 faite (vérifiée hors du jeu, pas encore en jeu)** : les 17 arènes de SvR 2010 passent **telles quelles**
  par `port_svr_arena::Port` (comme Kaiko les joue dans SvR 2011) : SmackDown, RAW, ECW, SummerSlam, Unforgiven, No
  Mercy, Survivor Series, Armageddon, Royal Rumble, No Way Out, WM 25, Backlash, Judgment Day, Night of Champions, Great
  American Bash, Cyber Sunday, Extreme Rules (table `kArenasSvr10` d'après `SVR2010` de Kaiko). Assistant : carte
  « SvR 2010 » (`svr_year_`). Repris : bannière (MASI ; Extreme Rules n'en a pas) et écran VS de WWE '12 habillé
  (`Card`). Pas repris : son propre écran VS (`SvrCard`), ses plaques (`SvrEtex`), sa transition (rangés autrement) :
  ceux de la base.
- **Étapes 4 et 5 (2008 / 2009) faites (2026-10-31, vérifiées hors du jeu, pas encore en jeu)**, méthode de Kaiko
  (`svr08_char.py`, `arena_port.py` `svr2008_convert`) reprise en C++ avec crédit :
  - **Catcheurs** (`port_svr::OldSvrPictures` puis le portage 2011) : la couleur (`<id>color`, rangée avec les normales
    dans l'entrée 10) passe dans une entrée 12 « color », la carte de normales devient « normal », les modèles les
    nomment ainsi ; une image que l'attire n'a pas (2009 Jericho : « 6102color ») = sa couleur. Le 20000 de 2008/2009
    n'est pas celui de 2011 (sourcils de Rey, bracelets de Torrie, cils + yeux de 2009) : **fusionné dans le corps**
    (sur ses os), comme les morceaux du modèle des cheveux qui ne tiennent à aucun os de la tête (ch127 2009 : une
    pièce sur la main et les hanches). Squelette : les os d'aide k_* suivent leur parent. Testé : les 74 fichiers de
    2008 et les 85 de 2009 passent (1re tenue), images de face propres (Jeff Hardy, Torrie, Jericho, ch127).
    Noms : 2008 dans `pac/misc.pac` (fiches de 232 octets, numéro à +30), 2009 dans `chEtc.pac` (252 octets, +36).
    Portraits : 2009 `DLC_HD.pac`, 2008 `menu/menuHD.pac`, MENU/SSFA-SSFC = PACH { 0 : PACH { numéro : DDS } },
    remis aux tailles de WWE '12. 2008 : fichiers `chNN.pac`, vidéos `NN.bik`. Pas repris : les pièces de tenue
    0x20+ (3 tenues seulement : ch23 2304, ch70 7004, ch137 8904), les prises.
  - **Arènes** (`port_svr_arena::OldSvrArena` avant `Port`) : images 200/201 → 400/401 ; foule (20000-20010) et
    mouvements de lumières 31010 d'ancien format retirés (foule de la base) ; banque de lumières 30000 sans en-tête
    « YMKs » (ajouté), jeux de lumières que le jeu cherche lui-même pris dans la base, drapeaux l(n) vers un jeu
    existant ; arbre de visibilité 50020 fabriqué (une boîte par modèle) ; 402 et 61000 de la base. Les 20 arènes de
    2008 et les 19 de 2009 passent ; bannière = la plus grande image de leur écran VS (MENU/MnnI). Leur propre écran
    VS, leurs plaques et leur transition ne sont pas repris (formats plus anciens) : écran VS de WWE '12 habillé.
  - À voir en jeu : tout (catcheurs, sourcils / cils fusionnés en peau opaque, arènes, lumières).
- **Plaques d'entrée et transitions (2026-10-31, demande de l'utilisateur, vérifié hors du jeu)** :
  - Fichiers : 2008-2010 `pac/evt/_EvtGdHD.pac` / `_EvtMov.pac` (2011 : `evtgdhd.pac`, `evtmov.pac`). Les `MOV` de
    2009/2010 sont les boucles du logo pour les écrans de l'arène (256 × 256), **pas** des transitions : la vidéo de
    transition (MOVT) n'existe qu'à partir de 2011.
  - **2010** : ETEX/00NN comme 2011 (AG2D) : `SvrEtex` les prend telles quelles. Pas de transition propre.
  - **2009** : ETEX/00NN = images (entrée 0) + animation ATB0 version 1 (10000, la plaque de nom) ; 10010 = le logo WWE
    traversé de traînées (60 images, le même partout) = **sa transition**. **2008** : ET2D/00NN, plaque de nom seule.
  - ATB0 v1 lu et dessiné image par image (`port_svr_arena::OldSvrPlates`) : clé 16 octets {x, y, image, 0}, canal 0
    place (centre du calque, sur 640 × 448, parents ajoutés), 1 échelle, 2 couleur RGBA (les calques s'allument une
    image : arrivée en flip-book), longueur à +0x2C (60 images/s). La plaque tenue + 20 images d'arrivée vont sur les
    plaques de WWE '12 (`EtexWearing`, comme les 2K ; la ville reçoit la même plaque), barre du nom à 60 % de la
    largeur sur le texte de WWE '12 (`PlateTextFor`). Transition 2009 : 30 images (une sur deux), 320 × 320, Bink 1
    avec alpha.
- **Planche des visages (2026-10-31, hors du jeu, outil `face08` du dossier temporaire)** : les 74 catcheurs de 2008, 85
  de 2009 et 85 de 2010 portés puis rendus de face. Corrigé grâce à elle :
  - cheveux tombés sur les yeux (Regal, Maria, Stephanie, Kelly Kelly, Lilian, Melina...) : le modèle des cheveux n'a
    pas de visage sous la tête, sa tête prenait un autre angle que celle du corps (~27°) ; les cheveux et les pièces
    20000 prennent maintenant les angles du corps (`ConvertModel` `turns_in` / `turns_out`). Touche aussi 2010 / 2011 ;
  - commentateurs (Lawler, Tazz, JR, Joey Styles, JBL 2008) au visage noir : images nommées « 5306all » / « 5306allN »
    ou « 5506 » / « 5506_N », reconnues par leur fin (`OldSvrPictures`) ;
  - cils d'Orton 2008 en anneaux noirs : les pièces 20000 sur la tête restent dans 20000 (transparentes).
  - cheveux courts en taches (Chris Masters, MVP, Finlay, Kane...) : ces catcheurs n'ont pas de modèle de cheveux (peints
    sur la tête), les cheveux de la base restaient et prenaient l'image de SvR à leurs places : la base perd alors son
    10000 et sa physique (60, 61), comme WWE '12 pour Cena ou Orton. Les points noirs près des yeux (Cena 2010) partent
    avec.
- **Étapes 5 et 6 pour SvR 2007 (2026-11-01, vérifiées hors du jeu, pas encore en jeu)**, méthode de Kaiko
  (`svr07_char.py`, `arena_port.py` `SVR2007`) reprise en C++ avec crédit :
  - **Catcheurs** (`port_svr::Svr07Sheets`, avant `OldSvrPictures`) : 2007 texture chaque partie du corps à part (Steve
    Austin : 40 images dans l'entrée 10, « 0402Body », « 0402Body_N », « 0402Hand »… ; chaque maillage a son texDiffuse
    / texNormal, `g_texDiffuse` pour les cils de Kane). Chaque image de couleur reçoit une case d'une planche couleur de
    2048 × 1024 (rangement par rectangles libres, la plus grande d'abord, même échelle pour toutes, la plus grande qui
    tient : ~79 % le plus souvent ; les étagères de Kaiko laissaient la moitié de la planche de Kane vide), sa carte de
    normales la même case d'une planche de 1024 × 1024 (aucune ou « dummy_N » : plate), les UV de chaque maillage
    passent dans sa case. L'entrée 10 garde alors « color » et « normal » (plus le sang…), puis le chemin 2008. Fichiers
    `chNN.pac`, noms dans `pac/misc.pac` CHAR/DAT (176 octets, numéro à +26, nom à +28), portraits `menu/menuHD.pac`
    MENU/SF2B (512, le corps : SSFA et SSFB) et SF2A (256, le buste : SSFC), vidéos `movies/titantron/NN.bik` (Bink 1,
    320 × 320). Planche des visages : les 82 fichiers passent, visages propres ; ch92 est vert sur le disque même (modèle
    de remplacement, sans nom). Rendu en pied propre (Austin, Kane, Trish, Hogan). Pas repris : les prises.
  - **Arènes** (`port_svr_arena::Svr07Stage`, puis le chemin 2008) : 2007 range jusqu'à dix arènes par fichier, entrée
    STG<k>/00NN de `pac/bg/bg<k>.pac` (k = NN / 10 ; bg0.pac a aussi les armes, la cage, les échelles) : l'arène est
    sortie seule dans un EPAC « STG » comme ceux de 2008, sa foule (modèles et fichiers de places 20000-21247,
    40000-40191) laissée (foule de la base). Table `kArenasSvr07` (20 arènes ; noms et ordre de l'écran de choix d'arène du jeu, vus par l'utilisateur le
    2026-11-01 : 2 = Velocity et 3 = HEAT, pas ECW / copie comme chez Kaiko ; 17 = ECW One Night Stand, 18 = Taboo
    Tuesday ; 20+ = coulisses, parking, bar du mode histoire). Les 20 passent (4 jeux de lumières pris dans la base,
    arbre de visibilité fabriqué). Bannière (`Svr07Banner`) : son image de l'écran de choix d'arène, `menu/menuHD.pac`
    MENU/STTX / 0 / n (512 × 512, dans l'ordre des arènes), une bande 2:1 un peu au-dessus du milieu. **Écran VS**
    (`Svr07CardPictures`, vu sur les captures du jeu de l'utilisateur) : `menuHD.pac` MACH/<émission> (cadre et logo)
    et MACH/<fond> (1024 × 512), PACH { 0 : PACH { i : DDS } } ; fond + logo (la plus petite image d'au moins 100 de
    côté de l'émission ; HEAT, Royal Rumble, Judgment Day, GAB et NYR n'ont qu'une bande décorative : la bannière)
    sur l'écran VS de WWE '12 habillé (`CardWearing`, modèle MB09). **Écrans vidéo** (2007 et 2008, `OldSvrArena`) :
    leurs images étaient des bouche-trous que l'ancien jeu remplissait (« titan_mov » sur 801/803, « titan » ou un nom
    sans image, « sd_titan », sur 800), restés rouges : renommées « titantron_movie » (800, 803) et « arena_movie »
    (801) comme WWE '12 les attend, 800 ramené sur la partie titantron de la vidéo de WWE '12 (v 0-0,684). Pas de
    plaques d'entrée (pas de
    `_EvtGdHD.pac`). Ring blanc et tache blanche sur le tablier dans la vue 3D : pareil avec SvR 2008 (vu par
    l'utilisateur), c'est l'aperçu.
  - À voir en jeu : tout.
- **Hauteur des rampes (2026-11-01, remarque de l'utilisateur : « les arènes proposées sont souvent fausses »)** :
  `port13::RampHeight` ne regardait que les modèles 200-299 nommés « slo » (WWE '12, SvR 2010-2011, 2K). SvR 2007-2009
  numérotent leurs rampes autrement (2007 / 2008 « raw_ent_slope_0 » 11-16, « wm23_slo01 », « sd_slo0 » ; 2009
  « slo_body » 60-65) : toutes mesurées à 0, le Mod Maker leur proposait Royal Rumble ou Bragging Rights comme base. Un
  modèle nommé « slo… » ou « …_slo… » compte maintenant à tout numéro. Mesures : WWE '12 inchangé (seuls bg10 Bragging
  Rights et bg13 Royal Rumble au sol, Elimination Chamber 2,73 m, les autres 1,80 m) ; SvR 2007 au sol : SmackDown,
  Velocity, SummerSlam, No Mercy, Survivor Series, Armageddon, Royal Rumble, ECW One Night Stand, Taboo Tuesday (sol à 0,
  allée de 18 cm ; vérifié modèle par modèle), les 11 autres à 1,80 m ; SvR 2008 au sol : 00, 02, 05-10, 15, 17, 19.
- **Premier essai en jeu (2026-11-01, RAW de SvR 2007 + Edge de SvR 2007, par l'utilisateur)** : Edge bon (visage,
  cheveux, tenue), écran VS de 2007 bon, arène, rampe, public, éclairage bons, petits écrans avec la vidéo. Corrigé
  ensuite (pas encore revu en jeu) :
  - **titantron muet** : les images que l'ancien jeu posait sur le grand écran hors vidéo (« titan_logo_01 », le
    défilant « titan_logo_02 », « titan_kari_01 » : ciel étoilé, art « ENTER ») sont sur son plan et WWE '12 les
    dessine toujours, par-dessus : retirées (avant l'arbre de visibilité). 800 et 803 (souvent le même quad) prennent la
    partie titantron de la vidéo, UV pris sur tout l'écran ; les textures de la scène qui clignotaient en venaient
    sans doute (plans confondus) ;
  - **escaliers du ring blancs** : objets du jeu qui cherchent leur reflet dans l'entrée 401 de l'arène sous les noms
    « cube_00 » / « cube_01 » (WWE '12) ; les arènes de SvR 2007-2011 ont « cube00 », « ar_ref » : ceux de la base
    ajoutés (`Port`, toutes les arènes SvR) ;
  - **pas de plaques de nom ni de locator, pas de transition** (une arène ajoutée n'a rien sinon) : plaques de son
    arène de base telles quelles (`port13::Etex` sur l'evtgdhd.pac de WWE '12) quand le jeu n'en a pas (2007 ; celles
    de WrestleMania XXVII au logo de l'émission, premier essai, n'ont pas plu à l'utilisateur), transition de
    l'arène de base quand il n'en a pas (2007, 2008).
- **Deuxième et troisième essais en jeu (2026-11-01)** : la scène de 2007 est bien chargée (journal : « /STG/0001: the
  added arena »), les petits écrans (801) passent la vidéo de l'arène, mais le grand écran restait sombre en match :
  WWE '12 ne donne aucune vidéo au numéro 800 (déjà vu sur une scène 2K) ; 800 devient 808 quand l'arène n'en a pas.
  **SvR 2007 a bien ses plaques** : `pac/evt/evt2dHD.pac` ET2D/00NN (ET2D/0000 = la plaque bleue de SmackDown, et
  chaque arène la sienne : RAW mécanique avec étincelles, ville de WrestleMania 22, « A » d'Armageddon), animations ATB0
  v1 comme 2008 : `OldSvrPlates` les lit (année ≤ 2008 → ET2D) ; une image rouge unie (place que le jeu remplissait,
  2007 SmackDown) est laissée transparente. Les 20 plaques dessinées hors du jeu. CSFT/00NN du même fichier = polices ;
  `pac/movie.pac` MOV/M000-M019 = boucles d'arène 256 × 256 (Bink 1, ~7 s ; pas repris : celle de la base passe). Les
  plaques de la base ne servent plus que si l'arène n'en a pas. Transition : sur l'enregistrement de l'utilisateur, en
  quittant l'écran VS, SvR 2007 lance une gerbe de feu autour du logo de l'émission, puis du noir : un effet que le jeu
  dessine lui-même (les entrées ATB0 10000 de MACH sont des mises en page fixes, longueur 0). Une transition est faite
  pareille (`Svr07Transition`) : fond de l'écran VS (carré du milieu), logo recadré sur sa partie visible qui grandit,
  éclair blanc au tiers, fondus ; 36 images à 30/s, 320 × 320 avec alpha (Bink 1, comme celle de 2009). La plaque de
  SmackDown 2007 avait la vidéo d'entrée du catcheur dans son carré rouge : une plaque de WWE '12 est une image fixe,
  le cadre reste vide.
- **Quatrième essai en jeu (2026-11-01, Randy Orton sur RAW 2007)** : titantron bon (la vidéo d'Orton sur le grand
  écran, 808), plaques de 2007 bonnes (plaque mécanique RAW, titre et nom écrits dedans). La plaque du locator sortait à
  moitié de l'écran en haut à gauche (WWE '12 écrit la ville près du coin ; la grande plaque de 2007 y est posée pour que
  sa barre tombe sur le texte) : réduite autour du texte quand elle sortirait de l'écran (marge 16 px). Page Home, guide
  et NOUVEAU CATCHEUR du Mod Maker mis à jour : SvR 2007 à 2011, DLC, écran VS / plaques / transition des arènes.
- **Texte dans la fenêtre des plaques (2026-11-01, demande de l'utilisateur)** : le texte de WWE '12 (nom, ville) tombait
  sur la barre du bas de la plaque RAW 2007 (point fixe à 60 % × 50 %). `OldSvrPlates` cherche maintenant la fenêtre
  sombre de la plaque (plus longue suite de lignes aux 2/5 sombres et opaques, puis de colonnes, loin des bords) et y
  centre le texte ; sinon l'ancien point. Vérifié hors du jeu sur les plaques 2007-2009 : la fenêtre trouvée pour RAW
  2007, Taboo Tuesday, HEAT, No Mercy 2008 / 2009, No Way Out 2009, Unforgiven 2009, Tribute to the Troops ; les autres
  gardent l'ancien point (No Mercy 2007, sans fenêtre : sous les lames).
  Essai en jeu (2026-11-01) : la plaque bougeait bien, mais le texte de WWE '12 n'est pas là où `PlateTextFor` le
  croyait pour la base RAW (valeurs mesurées sur 2K17 / SmackDown) : mesuré sur les captures de l'utilisateur, nom et
  titre centrés vers (1100, 915), ville vers (505, 222) sur 1920 × 1080 : ajouté pour la base 1. Les deux plaques
  (ville et nom) sont réduites autour de leur texte quand elles sortiraient de l'écran, jusqu'à 70 % au plus.
