# Mod Maker de WWE '12 : état actuel (résumé du 2026-11-12)

Résumé de l'état actuel. Les sections « 1.3 » (2026-11-10 / 11) sont les plus récentes et priment sur les anciennes.
Docs liés : prises ajoutées `docs/prises_svr11.md`, prises / entrées / musiques des autres jeux `docs/plan_movesets.md`,
SvR 2007-2010 `docs/plan_svr_anciens.md`.

## 1. Principe, crédits, programmes

- **Base : le Mod Maker de Kaiko** (portage SvR 2011, dépôt https://github.com/KaikoClanworth1/wwe-svr2011-pc,
  `port/modmaker/`) ; son accord : « feel FREE to use my mod maker as a base… as credit is given ». Crédit obligatoire
  (readme, page AIDE / À propos, launcher, `README`). Son dépôt n'a pas de fichier LICENSE ; des pièces reprises sont
  créditées « MIT » dans ce document (pages MOVES & ENTRANCES, aperçu complet, écran des arènes). Ses outils Python
  (`Desktop\SVR 2011 KAIKO\Mod Maker Tools`) servent de référence. Avant de reprendre du code tiers : relecture et accord
  de l'utilisateur.
- **Deux programmes** : **`WWE12 Mod Maker.exe`** (`port/src/modmaker/modmaker.cpp`, `wwe12_modmaker`) *fabrique* les
  mods ; **le launcher** (`port/src/launcher/mods.h`, page MODS, bouton « Ouvrir le Mod Maker ») les *installe, active,
  désactive, supprime*. `tools/package.ps1` met le Mod Maker dans le zip. Jamais de données du jeu dans le Mod Maker ni
  dans le dépôt : il lit les fichiers du joueur.
- **Lecteurs d'archives** de Kaiko (`port/src/modmaker/svrfmt/` : `archive`, `pac`, `texture`, `jboy`, `xcompress.cpp`
  sur `lzxd.c`) : EPAC/EPK8, PACH, BPE, ZLIB, xcompress, HSPC/SHDC. Aussi compilés dans le jeu (bouchon
  `SVRFMT_NO_XCOMPRESS`). Le Mod Maker est compilé avec `SVRFMT_INFLATE`.

## 2. Format des mods et installation

- **`.svrmod`** = zip (stored ou deflate, CRC vérifiés ; chemins `..`, `:`, absolus, zip64, chiffrés refusés) avec
  `manifest.txt` (clé=valeur UTF-8 : `type=`, `id=` nettoyé `[A-Za-z0-9_.-]`, `name=`, `author=`, `version=`,
  `made_with=`, `needs=`). **`game=wwe12` obligatoire** (un mod de Kaiko est refusé).
- **Rangement** : `mods\.svrmods\<Type>\<id>\` (dépaqueté dans `.incoming` puis renommé) ; fichier `disabled` = mod
  désactivé. Les fichiers sous `files/` (chemins du disque) sont posés dans `mods\<chemin>` en liens physiques,
  `mods\.svrmods\deployed.txt` les liste ; les fichiers mis à la main ne sont jamais remplacés ; deux mods sur un même
  fichier : le premier (type puis nom de dossier) gagne, conflit signalé.
- **Types** : `files`, `media`, `textures` (images vers `textures\<id>\`), `arena` (`arena/`), `superstar`
  (`superstar/`), `attire` (`attire/`), `moves` (dossier `Moves`), `signs`, `background`.
- **Dans le jeu** : dossier `mods` à côté de l'exe (patch SDK `rexglue-sdk-overlay.patch`, `MountModsOverlay` dans
  `wwe12_app.h`) ; arènes ajoutées montées avant (`Wwe12MountArenaSlots`) ; catcheurs, tenues, prises, médias dans le
  **pack DLC maison `WWE12PCMODS`** fait au démarrage (`port/src/native/wwe12_superstar_mods.cpp`, recette
  `port/src/modmaker/superstar_pack.h`, après `Wwe12InstallDlc`).
- **Jouer sans mods** (launcher, page JOUER) : `--wwe12_no_mods=true` coupe `wwe12_texture_packs`,
  `wwe12_superstar_mods`, `wwe12_entrance_mods`, ne monte ni `mods` ni les arènes ajoutées (vérifié une fois le
  2026-10-30). Limite : le pack déjà fait reste lu comme un DLC (catcheurs ajoutés figés).
- **Signaler un problème** (launcher, À PROPOS) : zip `WWE12-PC-report-AAAAMMJJ-HHMM.zip` sur le Bureau.

## 3. Réglages (cvars) et fichiers dans Documents\wwe12

| Cvar | Défaut | Rôle |
|---|---|---|
| `wwe12_texture_dump` | false | dump des textures dans `texture_dumps\` |
| `wwe12_texture_packs` | true | remplacements sous `textures\` |
| `wwe12_superstar_mods` | true | pack WWE12PCMODS (catcheurs, tenues, prises, médias) |
| `wwe12_superstar_roster_fix` | | recopie fiche et prises du mod dans la sauvegarde |
| `wwe12_entrance_mods` | | entrées / victoires portées |
| `wwe12_added_moves` | **false** | prises ajoutées d'autres jeux (coupées depuis le 2026-11-10) |
| `wwe12_heap_grow` | « 12:16384,15:4096 » | tas 12 et 15 agrandis (`wwe12_heap_probe.cpp`) |
| `wwe12_no_mods` | | jouer sans mods |
| `wwe12_arena_pages_test` | | 2e page de test des arènes |
| `wwe12_belt_fit`, `wwe12_belt_length` | | ceintures (entrent dans la clé du cache des entrées) |
| `wwe12_dir_log`, `wwe12_entrance_list_log` | | journaux du répertoire / des listes de Créer une entrée |
| sondes `wwe12_roster_probe`, `wwe12_story_probe` (défaut false), `wwe12_moves_probe`, `wwe12_down_probe` | | études |

Fichiers du jeu (`Documents\wwe12`) : `arena_slots\` (+ `cache.txt`), `arena_movies\` (copie d'`evtmov.pac`),
`entrance_slots\` (+ `cache.txt`), `pack_movies\`, `menu_background\`, `added_videos\` + `added_videos.txt`,
`superstar_slots.txt`, `superstar_movies.txt`, `belts.txt`, `moves_mods.txt`, `moves_applied.txt`, `mods_warning.txt`,
`SaveBackups\` (10 copies), `WWE12PCMODS\wwe12pc_stars.txt` et `wwe12pc_stamp.txt`. Mod Maker : `modmaker_discs.txt`
(lignes de disques, `DLC|chemin`), `modmaker_bink2.txt`. Test sans risque : `tools/dev/lancer_isole.bat`
(`--user_data_root %USERPROFILE%\Documents\wwe12_test`).

## 4. Pages du Mod Maker

ACCUEIL (« PREMIERS PAS (mode simple) »), MES DISQUES (`game_discs.h`), ARÈNES (`arena_page.h` : Nouvelle arène,
Retoucher, Autre fichier, Vue 3D, Éditeur 3D), CATCHEURS (`superstar_page.h`), NOUVEAU CATCHEUR (`new_star_page.h`,
projets `.wwe12proj`), PRISES & ENTRÉES / MOVES & ENTRANCES (`moves_page.h`), VIDÉOS (`media.h`), MÉDIA
(`media_page.h`), TEXTURES, PANCARTES (`signs_page.h`), FONDS DU MENU (`backgrounds_page.h`), CEINTURES ET ARMES
(`objects_page.h`), CAW (`caw_page.h`), FICHIERS DU JEU (`pac_page.h`), REGARDER (`look_page.h`), VÉRIFIER LES MODS
(`mod_doctor.h`), AIDE (guides pour toutes les pages, crédits). Aides de test : `WWE12_MM_PAGE=<n>` (9 animations,
6 prises), `WWE12_MM_ANIM`, `WWE12_MM_MOVES`, `WWE12_MM_PORT=<titre hex>,<n°>`, `WWE12_MM_PLAYNEW=1`, `WWE12_MM_WATCH*`.

- **MES DISQUES** : autres jeux du joueur (ISO ou dossier), reconnus par le numéro de titre du `default.xex` (SvR 2011
  5451085D, WWE '13 545108B4, 2K14 545408B2, 2K15 545408B6, 2K16 315A07D2, 2K17 545408BA ; SvR 2007-2010 : voir
  `docs/plan_svr_anciens.md`) ; « Chercher mes disques » (3 niveaux) ; « Dossier des DLC... » (`dlc.h`, `stfs.h` :
  paquets « LIVE » / « CON » ou extraits ; DLC lus comme le disque, `Read` le disque d'abord, `ReadAll` toutes les
  copies). Rien n'est copié.
- **FICHIERS DU JEU** : arbre EPAC / PACH, aperçu, export ; « Remplacer... » puis mod `type=files` ou « Enregistrer
  l'archive sous... ». **Écriture en place** (`pac_page::Browser::InPlace`) : chaque entrée de premier niveau garde son
  début et sa taille (le jeu trouve les entrées par `plist360.arc`) ; une entrée plus grosse est refusée ; BPE
  recompressé par `bpe_like::EncodeLike`. XMemCompress refusé.
- **VÉRIFIER LES MODS** (d'après `mod_check.cpp` de Kaiko) : base absente, modèle illisible, NaN, plus de 20 os par
  maillage, vidéo pas en Bink 1, arène de plus de 12,5 Mo, bannière pas en 256 × 128, deux mods sur le même fichier ou
  la même tenue, numéros gardés pour des mods partis… avec boutons de réparation (dont « Retirer du pack »).

## 5. Textures

- Jeu : `port/src/native/wwe12_texture_packs.{h,cpp}`, branché dans `native_texture_mirror.cpp` (`Build2DMirror`) ;
  empreinte = XXH3 des blocs de l'image de base dépliés (DXT1/3/5, 8:8:8:8, textures 2D). Dump :
  `texture_dumps\<l>x<h>_<dxt1|dxt3|dxt5|rgba>_<empreinte>.png`. Packs : tout `.png`/`.dds` sous `textures\` dont le
  nom finit par `_<empreinte 16 hex>`, à toute taille ; lecture synchrone au premier affichage. Vu en jeu.
- L'empreinte calculée depuis un DDS de svrfmt est la même que celle des captures du jeu (ceintures et armes sans
  capture).

## 6. Vidéos d'entrée et médias

- **`movies\titantron\<nnn>.bik`** (Bink 1 `BIKi`, 320 × 320, 29,97 i/s) : haut 320 × 220 = titantron, bas 320 × 100 =
  minitron ; nnn = numéro de vidéo (celui du catcheur ; vidéos sans fiche listées « Video <nnn> »). Page VIDÉOS
  (`media::EntranceVideos`, `media::MovieEditor`) : titantron et minitron séparés, taille 320 / 640 / 1024 (vu en jeu),
  « Chercher sa vidéo dans mes autres jeux », ruban et mur d'un catcheur du jeu. Encodeur Bink au débit des vidéos du
  jeu (≤ ~24 Ko par image en moyenne, ≤ 100 Ko par image) ; ruban et mur ≥ 180 images et ≤ 1,8 Mo ; musiques en
  48 kHz ; le jeu laisse de côté une vidéo hors normes.
- **Nouvelles vidéos dans Créer une entrée (1.3, 2026-11-11, pas encore vu en jeu)** : page VIDÉOS « Ajouter une
  nouvelle vidéo (Créer une entrée) » + nom → `titantron_add=<NOM>` et `media/titantron_add.bik`. Jeu
  (`MountAddedVideos`) : numéro libre pris de 899 vers le bas, gardé dans `added_videos.txt`, servi depuis
  `added_videos\movies\titantron\NNN.bik`, ajouté à la liste MOVIE (remplie par `sub_829F9F90`, genre 0). Corrigé : un
  catcheur ajouté y était sous le numéro de son entrée (Jey Uso 498), maintenant sous celui de sa vidéo (104).
- **Vidéos des autres jeux** : SvR (Bink 1) copiées ; WWE '13 / 2K par notre lecteur Bink 2 (`docs/plan_movesets.md`),
  RAD Video Tools (`radvideo64.exe BinkConv`, bureau Windows caché) ou ffmpeg de Librempeg en secours, jamais la DLL
  d'un jeu. 2K15-2K17 : une image 16:9 écrasée dans 320 × 320 → convertie en 512 × 288 ; 2K17 minitron `pac\mov\ss_mini`.
- **MÉDIA** : portrait (SSFA/B/C HD et SD, vu en jeu) et musique d'entrée d'un catcheur du jeu (`media/music/NNN.mp3`,
  copiée au démarrage dans `music`, à choisir dans Créer une entrée). Remplacer le thème d'un catcheur du jeu : pas fait.

## 7. Arènes

### Retoucher / remplacer
- Images des paquets 400-402, 32010... (remplacer au format de l'originale, exporter PNG) ; **Couleurs du ring**
  (cordes `ar_rop01`, coins `rin_tb00` / `rin_tb20`, poteaux `rin_pos00` / `rin_pos01` ; vu en jeu). Remplacer garde la
  **taille d'origine** du `bgNN.pac` : `Arena::Fit` retire le mip 0 des plus grandes images (jamais `titan`, `tron`,
  `mov`, `light`, `lgt`).
- **Éditeur 3D** (`arena_edit.h`) : déplacer / tourner / cacher (1 km sous le sol) un objet (ce sont les sommets et
  sphères qui bougent), Dupliquer (`AddCopy`, numéros libres 39-99 puis 300-799 ; 950-999 = ring, pièces 951-980 retirées
  de la liste), « Ajouter un objet 3D (OBJ, FBX)... » (`mesh_import.h`, retiré si l'arène dépasse sa place ; pas encore
  vu en jeu), « Exporter en OBJ ». Vu en jeu : barrière soulevée, copiée.
- **Vue 3D** (`arena_view.h`, D3D11) : maillages WWE '12 de 0xB8 octets ; table 50001 (`r` miroir, `a` additif, `m(n)`
  écarté, `e` / `g`) ; brumes (`z` sans `a`) non dessinées.

### Nouvelles arènes (sans remplacer)
- Mod : `slot=new`, `base=NN`, `arena/arena.pac` (un `bgNN.pac` complet), `arena/banner.dds` (256 × 128 DXT5 sans
  mips, 32 896 octets), facultatifs `card_mb.pac`, `card_mf.pac`, `etex_hd.pac`, `transition.bik`. Jusqu'à 35 (les 35
  `CAR_dummyNN` libres de `menu/MatchHD.pac` MLYE/MUPA). **Place d'une arène ajoutée = 12,5 Mo** (limite : tas 12,
  ≈ 18,7 Mo, taille + 5 Mo libres).
- Jeu (`port/src/native/wwe12_arena_pages.cpp`) : pages après celles du jeu (21 cases, LB / RB, « LB PAGE 1 / 2 RB »,
  nom de l'arène sous le curseur ; vu en jeu) ; numéros fictifs 150 + n ; `sub_825A56C8` rend le numéro de la base ;
  `bgNN.pac` de la base réécrit au choix (`Serve`, `bgNN.pac.current`) ; le répertoire (`sub_827F29F0` accroché) répond
  à `/STG/00NN` (enregistrement STG compté à partir de 0x4000), `/MLYE/MUPA`, `/MLYE/MBnn`, `/MENU/MnnF`, `/ETEX/00NN`,
  `/MOVT/M0NN`.
- **Écran VS d'une arène sur la même base** (corrigé, vu en jeu, 2026-10-26) : numéro de scène oublié (-1) et demande
  `/MLYE/MB<base>` réécrite en `MB21` / `MB22`. **Même base via le menu principal (2026-11-11, pas vérifié en jeu)** :
  +36 du chargeur (`*0x8322DD48`, `sub_8268B960`) remis à -1. Piège : base verrouillée dans la sauvegarde → thème
  « /MLYE/MB-1 », écran de SmackDown.
- **Univers** : arènes ajoutées retirées (décision de l'utilisateur, 2026-10-26 ; indicateur 0x8324E51C = 1,
  `sub_8253A028`). Créer un scénario : mis de côté. Limite : `MatchSD.pac` (4:3) pas fait.
- **Cache au démarrage (1.3, 2026-11-10, pas de build, pas d'essai en jeu)** : seul le début d'`arena.pac` est lu
  (`ReadHostHead`) ; copies gardées si la clé `InputsKey` (code + fichiers de `mods`) est celle de
  `arena_slots\cache.txt`. Journal « [arena pages] nothing changed since the last start... ».
- **Assistant « Nouvelle arène »** (6 étapes) : départ (Mes disques, ISO, dossier, fichier), base (place, rampe,
  « aussi utilisée par » ; proposée : même rampe), nom et bannière, ce qui est repris, aperçu 3D, terminer (**Tester en
  jeu**). `port13::RampHeight` avertit si les rampes ne concordent pas (WWE '12 : 1,80 m, sauf bg10 Bragging Rights et
  bg13 Royal Rumble au sol, Elimination Chamber 2,73 m).

### Portages
- **WWE '13** (`port13.h`, vu en jeu : Capitol Punishment, Vengeance) : décompresser, `EPK8` → `EPAC`, drapeau 0x804
  effacé ; **50010 de WWE '12** ; ring de WWE '12 (951, 952, 953, 956, 960, 964, 973, 974, 980 ; 981 / 982 enlevés) ;
  61000 de la base ; coins des barricades 390-397 déplacés ; collision de la base (995-999) ; images du ring manquantes
  recopiées du paquet 400 de la base ; écran VS `MB<fichier>` / `M<fichier>F` copiés (`port13::Card`, identique en
  jeu) ; plaques ETEX (`port13::Etex`, ZLIB repassé en BPE) ; bannières MLYE/MUAS (table `port13::kArenas13`).
- **SvR 2011** (`port_svr_arena.h`, vu en jeu : WM 26, Breaking Point, Royal Rumble) : `fade_lightmap00/01` copiées de la
  base (en gris pour les arènes SvR), `rin_ep01` = `rin_ep00`, barricades `ar_fence01_a` à `_f` de la base, nœuds
  `ar_fen*` de 50020 remplacés, boîtes 50020 ×100 ; foule propre (non agrandie, places ×10) ; **vrai écran VS**
  (`SvrCard`, case cochée par défaut) ; **plaques et locator** (`SvrEtex` : images clés AG2D 96 / 44 → 108 octets) ;
  projecteurs mobiles (`m(N)`) retirés ; `yReflect` → `yDefault` ; pièces `g` gardées pendant les entrées.
- **SvR 2007-2010** : voir `docs/plan_svr_anciens.md`. **SvR 2008 / 2009 (2026-11-11, vu en jeu : Judgment Day 2008,
  SmackDown 2009 « nickel »)** :
  - écran VS : `CardWearing` ; 2008 : plaque `mh_jud_fr` dans `hell_fr`, pièce `plate` 640 × 96 (lue à la main,
    `BundleRead` refuse `atb`) ; 2009 : traité pareil, plaque étirée entière ; logo et barre de Hell in a Cell cachés ;
  - plaques d'entrée : place de l'animation SvR (écran de 512, échelle 2,5), image entière, sans `fit`, plaque de
    ville vide ; vu en jeu comme Xenia ;
  - écrans : 2008 titantron = 808, minitron = 806 ; 2009 (vraies images `_titan`) : titantron = le plus haut des deux,
    minitron = l'autre, aussi derrière 806 ; walltron = copie de `sd_anim00` 3 cm devant, en 803 ; caches poussés 3 cm,
    copies 801 / 803 (ids 760+) poussées 6 cm, pas de `StageCover` ; objets `<show>_titan_l` / `<show>_titan<chiffres>`
    retirés ;
  - tapis `ar_dwnmt` renommé `ar_dwnmt01` ; `yBumpMap` → `yBG_BumpMap` avec `sampDiffusePrm` / `sampNormalPrm` /
    `sampSpecularPrm`, brillance retirée (-1), 0,25 / 0,5 (`yDefault` plantait) ; barrière : `ar_fence` de l'arène si
    elle n'a pas `ar_fen_wwe` ;
  - bannières : 2008 `menuHD.pac` MENU/STTX (`Svr07Banner`), 2009 `ScrLayHD.pac` SCRL/LSSE 7001 (`Svr09Banner`) ;
    transition 2008 = logo W (`pac/movie.pac` `MOV /M0we`), 2009 logo WWE (2D) ;
  - reste : marches et table des commentateurs (`bgEtc.pac` WPON, communs ; le jeu ne cherche pas WPON par
    l'annuaire, essai d'images dans l'arène sans effet, retiré), teinte orange des entrées. Planche de contrôle :
    `tools/dev/svr_arenas_sheet.py "<iso>" 2008` → `work/svr_check/svr2008_arenas.png`. Il faut reporter l'arène dans
    le Mod Maker après ces changements.
- **2K14-2K17** (recette de `port13.h`) : entrées « ZLIB » remises en BPE (`port13::PortTree`, plantage WM 31 de 2K16
  sinon) ; écrans vidéo renumérotés par rôle (`port13::ScreensTo12` ; branche 2K14 seulement pour les arènes sans image
  `layout_*`) ; `LedPanelsTo12` ; transitions Bink 2 → Bink 1 320 × 320 avec alpha ; bannières `port13::Banner2K`
  (2K16 / 2K17 `Pbg_NN_*`, 2K15 `PPVbg_<fiche>` puis `arena_<nom>`, 2K14 `kBanners2K14` MLYE/OLFF) ; noms d'arènes
  2K15-2K17 dans `pac\wuniData.pac` (tables « udt »), 2K14 liste `strings2k::kArenas2K14`.
  - Plaques 2K = vidéos (2K16 / 2K17 `pac/mov/bg_locator|bg_name`, 2K15 `evtmov_bg_locate|evtmov_bg_name.pac`, 2K14
    `evtmov3|evtmov2.pac`, Bink 1 avec plan alpha) : `PlateMovie2K`, `bink2::Still`, `PlateCut`, `EtexWearing`
    (modèle WM XXVII, flip-book `Ag2dFlipBook` / `Ag2dPlaceLayer`), `PlateOnScreen` ; texte dans la case :
    `PlateTextFor` (1080p, mesuré par base : base 1 RAW, 2 NXT nom 918, 857 ville dès x = 440, 3 Superstars nom 1105,
    802, 5 Fatal 4-Way nom 1115, 925, 9 Hell in a Cell nom (1145, 928) ville (385, 265) ; autres = base 2).
    Plaques 2K17 vues en jeu ; 2K15 corrigé (noms `*_logo`).
  - Écrans VS : 2K14-2K16 `Card2KOwn` (3000 doublé, `dummy` → 3002, police renommée `font000`, dans le MB01 de WWE '12 ;
    pas encore en jeu) ; 2K17 : ATB0 version 8, reste à l'écran habillé, trouvé par nom via `pac/evt/evtlayout.pac`
    (39 arènes sur 46).
- **Écrans vidéo de WWE '12, par numéro** : 800/803/806/808 titantron, 801/807/809 vidéo de l'arène, 804/805 caméra en
  direct ; plus précisément (RAW, aperçu des entrées) : 803 / 808 titantron (808 = haut v 0-0,684), 806 / 807 bande
  dessous (806 = minitron v 0,69-1), 801 / 804 le mur (MVSW), 809 le ruban (MVSB), 800 le petit écran.
- **Éléments 2D** : plaques `pac\evt\evtgdhd.pac` ETEX/00NN (images 0, 10, 20, 40 ; AG2D 10000-10040 ; polices
  20000-20011) ; écran VS `menu/MatchHD.pac` MLYE/MBnn (YXMLBIN VER0003 7000, images 7001, animation 7002) + MENU/MnnF ;
  transition `pac\evt\evtmov.pac` MOVT/M0NN (Bink 1 640 × 640). Piège : la mettre dans la copie de `MatchHD.pac` →
  écran noir ; un `evtgdhd.pac` reconstruit en entier dans `mods` plante.

## 8. Catcheurs

### Ajouter un catcheur (pack DLC maison)
- Mod `type=superstar` : `name=`, `base=NNN`, `superstar\ch.pac` facultatif (sinon `look.txt` = apparence de la base),
  `attributes=12 nombres`, `record=OFF:HEX,...` (jamais 0x2C..0x103), `titantron.bik`, portraits `SSF?_HD|SD.dds`,
  `theme.wem`, `moves/`, `entrance/`, `victory/`, `objects/`.
- Pack `WWE12PCMODS` (numéro = dernier pack + 1, au plus 9) fait des fichiers du joueur, réécrit seulement s'il change
  (cache du jeu vidé alors). **Places** : d'abord une fiche vide du disque (100+), puis DLC 85-99, puis les autres DLC ;
  gardées dans `superstar_slots.txt`. Recette : fiche et prises d'un catcheur de base, numéro (+0x2C, +0xF8, +0x102), nom
  (+0x2E, +0x72, +0xB6), codes de l'emplacement gardés (+0xF0 = `DSBD`), **personne libre (+0x100)** (sinon une seule
  case par personne à l'écran de choix), **drapeau DLC +0x11F = 1** ; ligne de catalogue + entrée `IGST/VOLn` (codes
  « lettre + U » qui se suivent). Portraits cherchés aussi dans les packs (`/SSFx/%04d`).
- **Sauvegarde** : le jeu recopie fiche et prises du mod quand elles diffèrent (`sub_828ADFB8`,
  `wwe12_superstar_roster_fix`, contrôle sur John Cena) ; le pack n'est **jamais** retiré ; un catcheur sans son mod :
  pack laissé tel quel, avertissement `mods_warning.txt`, jusqu'à « Retirer du pack » ; sauvegarde copiée dans
  `SaveBackups` avant chaque changement. Règle pour les joueurs : ne plus utiliser un catcheur ajouté avant d'enlever
  son mod.
- Beaucoup de catcheurs : 50 TEST STAR match joué ; 55 (dont 5 vrais modèles) démarrage OK, match pas testé.
- Les catcheurs ajoutés sont partout (Univers, Créer un scénario).

### Page NOUVEAU CATCHEUR
- Apparence : base, un autre `chNNN.pac`, une tenue d'un autre jeu (Mes disques), ou un FBX (`fbx_star.h`, ufbx 0.23.1,
  `rig_map.cpp` de Kaiko, budget 12 000 sommets ; vu en jeu). Base du même nom proposée d'office. Nom, **attributs**
  (CHAR/DAT +4..+15, recopiés +16..+27), **taille** (+0x28 / +0x2A, 4096 = 1,0 ; vu en jeu), **capacités** (+0x104,
  5 au plus), **marque** (+0x1C..+0x1F : 0 RAW, 1 SmackDown, 6 sans marque, 4 autre ; posée à la première installation
  seulement ; pas encore vue en jeu), portrait (celui de son jeu, « Une image à vous... », ou de la base), vidéo, prises,
  entrée, musique. Bouton « Tester en jeu ».
- Portraits portés : SvR 2011 `DLC_HD.pac` / `DLC_SD.pac` ; 2K14 `menuHD.pac` ; 2K15 / 2K16 `menuHD2.pac` (TGA) ; 2K17
  `menu\SSFaceHD.pac` (HSPC, `faces2k::Match2K17`). Hachage SHDC de 2K17 : MurmurHash64A du numéro en décimal, graine
  0x12345678 (`svrfmt::ShdcHashFor`).

### Tenues
- Mod `type=attire` : `attire/attire.pac` + `superstar=NNN`, `attire=000NNNAK` ; mis dans le pack (son annuaire passe
  avant le disque ; vu en jeu). Plusieurs mods sur un catcheur vont ensemble.
- **Ajouter une tenue (4 au plus)** : « En nouvelle tenue (tenue N, garde l'originale) » ; nombre de tenues relevé dans
  la table **COS** de `misc.pac` (BATS/INIT et BATH/INIT, enfants 5 et 201 ; lignes de 40 o {u32 catcheur, u32 tenues,
  4 × {u32 texte du nom, i32 déblocage}}) ; vu en jeu. Catcheurs des DLC officiels aussi.
- **Render** : `attire/SSFA|SSFB|SSFC_SD|HD.dds`, servis sous « ANNN » (SSFB = CHANGE ATTIRE, SSFA = écran VS ; vus en
  jeu). **Nom** (`attire_name=`) : texte 60000+ dans la ligne COS, crochet de `sub_827DB6D0` / `sub_827DB610`.
- **Tenue d'entrée** (`NNNNNNA4`) : portée avec les tenues de WWE '13 (`port13_star::AddEntranceOutfit`, case « Avec sa
  tenue d'entrée » ; vu en jeu : Miz, Cena 4 tenues) ; 2K16 / 2K17 : « Sa tenue d'entrée (de son fichier) » (vu en jeu,
  Rollins). **1.3 (2026-11-10, syntaxe seulement, pas de build ni d'essai)** : section « Tenue d'entrée de cette tenue »
  (`DrawEntranceOutfit`) pour toute tenue d'un catcheur du jeu ou ajouté ; source : une autre de ses tenues ou une tenue
  d'un autre jeu ; `outfit_only=1` quand la tenue ne change pas (un jeu 1.2.0 ignore la clé).
- **FBX sur une tenue ou comme tenue d'entrée : EXPÉRIMENTAL (1.3, 2026-11-10, syntaxe seulement)** : « Importer sur la
  tenue N » / « Importer comme tenue d'entrée » (`fbx_star::FbxToAttire`), curseur « Sommets au plus » (4 000-30 000,
  défaut 12 000) ; avertissement `FbxWarning` ; l'utilisateur ne l'essaiera pas.
- **Retouche** (page CATCHEURS) : images de la tenue, Matières (brillance × `g_fSpecLev` / `g_fSpecLvBase`, teinte ×
  `g_f4MatDifCol`, en lot ; vu en jeu), panoplies.
- **Panoplies (Superstar Threads)** : `EDIT/SSTL` (fiches de 484 o) ; le pack ajoute `edit_dlc05.pac` avec une fiche par
  catcheur ajouté copiée de sa base (vu en jeu : Mankind).
- **Matériaux et visages des portages** : SvR (`port_svr.h`) : os alignés d'après les articulations, mains `BloodHand`
  recopiées, couleur rendue opaque puis réencodée au format de la base, `Shine` (alpha de `normal` ramené à 0,8 de la
  moyenne de la base), `HairCut` (alpha et bleu de la normal map des cheveux), trois masques Mask1-3, cils 20000 et voile
  des yeux retirés, `blood_hand` / `bloodhand` retirés, `Turns` ; WWE '13 / 2K (`port13_star.h`) : `FullMipChains`.
- **Cheveux qui bougent** : chaînes `kami_bNN` du modèle 10000, entrées 60 (SpringHairSetting) et 61 (collisions) ;
  `KeepChains`, `HairSettings`, `hair_donor.h` ; les os de cheveux n'orientent plus d'os (`ConvertModel`). Vu en jeu
  (CM PUNK '10, Drew McIntyre). FBX : pas de chaînes.

### Prises, entrées et musiques (page MOVES & ENTRANCES et NOUVEAU CATCHEUR)
- Ce qu'on prend d'un catcheur d'un autre jeu : `star_port.h`. Mod `type=moves` : `superstar=<n° WWE '12>`, `moves=`,
  `entrance=N` (autre catcheur du jeu) ou `entrance_own=`, `entrance_champ=<n>`, fichiers `superstar/`. Jeu :
  `superstar_pack.h` (`Moveset`) écrit CHAR / PRO dans le `chEtc.pac` du pack ; copie de la sauvegarde refaite à l'écran
  de choix ; mod retiré : retour une fois (`moves_mods.txt`) ; `moves_applied.txt` (« n°=empreinte:avant ») pour une
  entrée sautée après un gel. Deux mods sur le même catcheur : le dernier dans l'ordre des dossiers gagne.
- Onglets : Prises / Provocations (`moves_edit.h`), Jouer (`move_preview.h`, animation `m.pac` ZMOT/CAnn par
  `MotionIndex`), Entrée, Depuis un autre jeu (assistant, une case par prise). Prises ajoutées : coupées par défaut,
  voir `docs/prises_svr11.md`.
- **Entrées de champion (1.3, 2026-11-11, pas encore vu en jeu)** : bloc PRO +0x1D4 (répété +0x1E0) : musique, vidéo,
  entrée, puis deux entrées de champion (+0x1DA, +0x1DC) ; WWE '12 600-6xx (Nyujyo9). `star_port` reprend celle que la
  fiche nomme (2K14 +582, 2K15 +586 ; autres jeux pas encore lus), ou une autre de son jeu au choix ; le jeu la met dans
  les deux cases (`PutMoves`) et dans Créer une entrée (« <NOM> CHAMP »).
- **Entrée seule pour Créer une entrée (1.3, 2026-11-12, vérifié hors jeu, pas encore vu en jeu)** : onglet « Ajouter
  une entrée » de MOVES & ENTRANCES (`entrance_add.h`). Jeu, puis une entrée de la liste complète du disque (WWE '13 /
  2K : nom du script, « The_Miz_Champ », 600-6xx = entrées avec ceinture ; SvR 2007-2011 : scripts des Nyujyo*.pac /
  evt1-2.pac, nommés d'après les catcheurs dont c'est l'entrée), un nom. Mod `type=entrance` (dossier `Entrances`) :
  `entrance_own=<n° libre>`, `name=`, `superstar/entrance/`, `superstar/objects/`. Le jeu (`MountEntrances`) la sert
  sous ce numéro et l'ajoute sous son nom aux listes de Créer une entrée ; aucun catcheur modifié. Aide de test :
  `WWE12_MM_PAGE=6 WWE12_MM_MOVES=139,4 WWE12_MM_ADDENT=<titre hex>,<n°>,<rapport>` (2K14 668, SvR 2011 139, SvR 2008
  4012 : convertis en 497).
- **Visages animés (1.3, 2026-11-11, pas encore vu en jeu)** : `face12.h` (via `star_port`, entrée, entrée de champion,
  victoire, tous jeux) : les 34 pistes du visage d'une entrée de WWE '12 (base, sinon Cena 139) copiées octet pour octet
  dans chaque segment du catcheur (`xxxx0000`) qui n'en a pas, coupées à sa durée (61 → 95 pistes sur 2K14).
- **Aperçu complet des entrées (1.3, 2026-11-11, hors jeu)** : bouton « Regarder l'entrée » (`entrance_preview.h`,
  d'après `full_preview.cpp` de Kaiko) : script EVP + banque EVZ, catcheur de WWE '12 (« Montrée sur »), caméras A / B
  (`#5edfdf5f` œil, `#70d146a5` cible, `#70d304a4` champ), arène (RAW par défaut, tapis à y = −120, sol par grille de
  20 cm), écrans aux bons numéros (voir § 7), ring recopié (`View::copies`), musique (vgmstream + XAudio2,
  `audio_out.h`). Vu sur images de test (Orton 2K14) ; titantron et musique à voir par l'utilisateur ; restent les
  cordes animées et l'intro cachée.
- **Musique (1.3, 2026-11-10, vérifié hors jeu, pas de build ni d'essai en jeu)** : `theme_convert::Finish` met le thème
  en stéréo 32 kHz (`To32kStereo`) et l'encode en **Wwise Vorbis comme le disque** (`wwise_vorbis.h`, jusqu'à 10 min,
  ~2,3 Mo pour 3 min 15 ; aoTuV beta6.03 + libogg 1.3.5, BSD-3, `wwe12_vorbisenc`) ; setup identique bit pour bit à
  celui des 125 thèmes stéréo du disque (livres de codes `kGameBooks`, table à 0x8310D1D8). Échec : l'ancien PCM (48 kHz,
  coupé à 100 s ; un thème PCM de plus de ~20 Mo ne démarre jamais). Jeu (`wwise::MusicPackage`) : n'accepte un thème
  Vorbis que si son setup est celui du thème 139 du joueur ; un jeu 1.2.0 ne le prend pas.
- **Cache des entrées au démarrage (1.3, 2026-11-10, pas de build ni d'essai)** : clé FNV-1a 64 (code, `wwe12_belt_fit`,
  `wwe12_belt_length`, `superstar_slots.txt`, `superstar_movies.txt`, `belts.txt`, fichiers de `mods`) comparée à
  `entrance_slots\cache.txt` ; pas de cache quand une arène ajoutée sert ses vidéos. Journal « [entrances] nothing
  changed... used again » ou « made again ».
- Gel avant l'entrée avec une arène ajoutée : réglé (rubans / murs des catcheurs ajoutés mis dans la copie
  d'`evtmov.pac` de l'arène ; vu en jeu).

## 9. Autres types

- **Pancartes** (`type=signs`) : `pac\audience.pac` AUDE/BORD (393 lots « no01 » 128 × 64 DXT1 ; catcheur × 10 + 1..4,
  générales 4001-4621 et 5001-5151) ; `signs/NNN_K.dds` ou `signs/<numéro>.dds` ; servies dans
  `pac/audience_dlcNN.pac` ; les catcheurs ajoutés reçoivent celles de leur base. Vu en jeu.
- **Fonds du menu** (`type=background`, `backgrounds/NN/menu.dds`, `title.dds`) : `MnLayHD.pac` MLYE/EM00 (EL00 selon
  la région), `ortontex02` / `ortontex01` remplacées dans une copie servie depuis `menu_background`, un fond tiré au
  sort à chaque lancement. Vu en jeu. Piège : un EM01 dans le pack → écran noir.
- **Ceintures et armes** : `evtobj.pac` SOBJ/NNNN (30 ceintures), `bgEtc.pac` STG/WPON… ; image remplacée = pack de
  textures. Pas encore vu en jeu sur une ceinture.
- **CAW** : liste, image des listes, export `.wwe12caw`, import ; sommes de contrôle (`.cas` +12, +0x14B254, fin ; fiche
  dans `SaveData.dat` à 0x1B9E8 + place × 0x6E4). **Supposé, à vérifier en jeu** : 0x6E4 et 50 places.
- **Types de match** : recherche seulement (`pac/misc.pac` `RUL ` à 0x9630, 119 fiches de 108 octets ; en mémoire
  `*(0x8324E238) + id × 108`) ; code de jeu, à décider avec l'utilisateur.

## 10. Formats et offsets utiles

- **`pac/ch/chEtc.pac`** : CHAR/DAT 290 o (BPE) : +4..+15 attributs (12, ordre STRIKE POWER, GRAPPLE POWER, SUBMISSION,
  STRIKE DEFENSE, GRAPPLE DEFENSE, SPEED, AGILITY, ADRENALINE, RECOVERY, DURABILITY, CHARISMA, TAG TEAM), +0x1C..+0x1F
  marque, +0x28 / +0x2A taille, +0x2C numéro, +0x2E / +0x72 / +0xB6 nom, +0xEF nombre de tenues, +0xF0 4 codes
  (= `DSBD`), +0xF8 et +0x102 numéro, +0x100 personne, +0x104 capacités (N → texte 40616 + N, table à 0x8206AD30),
  +0x11F drapeau DLC. CHAR/PRO 1 072 o (prises ; entrée +0x1D4). Numéros 0-49 créés, 50-99 DLC, 100-372 disque
  (réserves 344-372).
- **Sauvegarde** (`SaveData.Dat`) : CHAR/DAT sans ses 4 premiers octets à 0x1C (286 o par catcheur), CHAR/PRO 1 068 o à
  324792 ; fiche de sauvegarde [0x8317FA08] + (numéro − 50) × 286 (octet 247 débloqué, 283 DLC).
- **Pack DLC** : `info/catalog.dlc` (« DLCC », lignes de 0x40 : numéro du catcheur +0x10, nom +0x20 ; fin = liste des
  fichiers), `info/catalog_hd.arc` / `_sd.arc` (annuaire), `DLC.pac` (`IGST/VOLn`, `RGRP/MAGG`) ; le jeu lit
  `/IGST/VOL0` à `VOL9`, monte chaque pack sur `dlc%d:`.
- **Tenues** : `pac/ch/chNNN.pac` EPK8, entrée EMD `000NNNAA` (A = tenue, sorte 2 tenue, 4 tenue d'entrée) ; PACH :
  0 corps (107 os Biped), 10 textures (`normal`), 12 `color` (DXT1 2048 × 1024), 13-15 Mask1-3, 10000 cheveux, 20000
  yeux / cils, 40, 50, 60, 61, 80, 100. Le jeu cherche `/EMD/%06d%02d`. Une tenue reste dans sa taille d'origine.
- **Animations** : YANM gros-boutiste (`yanm.h`, d'après `yanm_decode.py` de Kaiko) ; `pac/m.pac` ZMVM / ZMOT / ZMBS /
  ZCAF / CAV ; numéro d'animation = numéro de la prise (MOVS/WAZE +0x90 en petit-boutiste) ; blocs « PPPPWWWW » ;
  pistes hachées `h = rotl(h, 7) ^ c` ; 60 Hz.
- **Arène** : `bgNN.pac` EPAC + section `STG ` ; taille dans `plist360.arc` (`sub_823E02E0`, `sub_827D99E0`) ;
  bannières `menu/MatchHD.pac` MLYE/MUPA entrée 7001 ; écran de choix `sub_825A67A0`, données à 0x82028560.
- **Vidéo du catcheur** : numéro du script d'entrée (`sub_828F2410`, 999 = vidéo propre) ; `sub_82A316A0(n)` fait le
  chemin (packs DLC d'abord) ; crochet : numéro × 100 + tenue identifie le catcheur qui entre. Ruban / mur :
  `sub_82A31578(numéro, sorte)` (2 mur, 1 ruban).

## 11. Pièges connus

- Un fichier servi plus gros que sa taille dans `plist360.arc` est lu tronqué → plantage ; les entrées compressées se
  décompressent sur place : jamais une taille plus petite que celle du jeu.
- Notre BPE est ~10 % moins bon que celui de Yuke's : `Fit` réduit une image au besoin (un fichier trop gros fait
  planter `sub_827E1428`).
- Un `string.pac` modifié servi par-dessus le disque fait planter (« call to guest 0 ») ; celui du pack DLC n'est jamais
  lu.
- Toutes les versions du jeu partagent `Documents\wwe12` : une version lancée sans mods ne retire plus le pack, mais
  tester avec `lancer_isole.bat`.
- La vidéo ne marchait pas servie depuis un autre fichier ; le jeu lit parfois plus que la taille donnée.
- Écran VS SvR 2011 mis dans le `MLYE/MBnn` d'une base sans conversion → plantage (`sub_82571BA8`, élément 30).

## 12. Pas fait / ouvert

- Ajouter des types de match ; remplacer le thème d'un catcheur du jeu ; petites icônes de la grille (MENU/SSFD
  d'après Kaiko) ; nom annoncé par le speaker, style de combat, catégorie de poids (`sub_8292C538`) ; walltrons 2K ;
  partage en ligne de mods ; agrandir une entrée dans FICHIERS DU JEU (corriger `plist360.arc`).
- Pour plus tard (noté le 2026-11-04) : personnes des entrées 2K visibles, écrans VS et plaques exacts des arènes
  portées, vidéos de l'arène NXT 2K14 sur les bons écrans, au-delà de 55 catcheurs.
- Le disque du rond central de WM 31 caché par le mur vidéo (laissé de côté) ; Mojo Rawley et Tye Dillinger (DLC 2K17)
  verts dans leurs propres fichiers.

Historique complet, essais et détails : docs/archives/mod_maker_anciennes_parties_2026-11-12.md
