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
  Reste vu sur la planche : points noirs près des yeux (Cena 2010, Justin Roberts), peut-être un défaut de l'outil.
