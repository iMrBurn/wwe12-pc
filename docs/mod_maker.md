# Mod Maker de WWE '12 : plan (conversation Mods, 2026-10-23)

Plan seulement : rien n'est codé. À valider avec l'utilisateur avant de commencer. Prévu pour la **1.0**, qui
passe d'abord par la stabilité (voir la feuille de route de `CLAUDE.md`).

## 1. Point de départ et décisions

- **Base : le Mod Maker de Kaiko** (portage SvR 2011). Son avis (2026-10-23) : un seul Mod Maker pour tous les
  portages serait trop de travail, chaque jeu doit avoir le sien ; il nous autorise à partir du sien, *« as credit
  is given »*. Description complète de son format : `SVRMOD_FORMAT.md` (reçu de lui, hors dépôt).
- **Crédit obligatoire** : readme livré, page « À propos » du Mod Maker et du launcher, `README` du dépôt.
- **Ce qu'on a de lui** (`Desktop\SVR 2011 KAIKO`, version publiée de son portage, regardée le 2026-10-23, rien
  exécuté) : `SvR2011 Mod Maker.exe` **compilé, sans son code C++** ; mais **ses outils Python sont livrés en
  clair** (`Mod Maker Tools\`, ~70 scripts : `svrfmt.py`, `jboy.py`, `arena_port.py` (arènes de WWE '12 à 2K17
  vers 2011), `w13_char.py` (modèles WWE '12-2K17 vers 2011), `yanm_decode.py`, `w13_retarget.py`,
  `wwe13_entrance.py`, `superstar_port.py`, `ymks.py`, `wwise.py`…) : ils lisent déjà les formats de WWE '12, dans
  le sens inverse du nôtre. Plus `svrmod.exe`, un guide HTML illustré (`Mod Maker Guide\index.html`, v2.1.0) et
  41 mods livrés (`Bundled Mods\*.svrmod`). **Aucune licence pour son propre code** (ses « Third-party notices »
  ne couvrent que FSR et DLSS) : lui demander d'écrire noir sur blanc les conditions (ex. « GPL-3.0 ou MIT, avec
  crédit »).
- **Son guide** donne le modèle d'organisation : un projet (`.svrproj`) → « Save as mod » → `.svrmod` → le launcher
  l'installe dans `Mods\<Type>\<id>\` → le jeu le superpose au démarrage, sans jamais modifier ses propres
  fichiers ; bouton « Install into game » direct ; le Mod Maker s'ouvre depuis l'onglet Mods du launcher ;
  Python + numpy + Pillow + ffmpeg installés à part (onglet Setup du launcher) pour les conversions ; un « mod
  doctor » vérifie les mods installés ; un explorateur « Look inside the game » (fichiers, animations, images).
- **Son code source** (réponse de Kaiko, 2026-10-23) : dans son dépôt public
  https://github.com/KaikoClanworth1/wwe-svr2011-pc, dossier **`port/modmaker/`** : 61 fichiers C++ (`app.cpp`,
  une page par type : `arena_page.cpp`, `star_page.cpp`, `attire_page.cpp`, `media_page.cpp`, `signs_page.cpp`,
  `moves_page.cpp`, `match_page.cpp`, `backgrounds_page.cpp`, `installed_page.cpp`… ; `pac_editor.cpp`,
  `mod_check.cpp` (le « mod doctor »), `svrmod_cli.cpp`, `editor.cpp` (3D), `video_mf.cpp`…), sous-dossiers
  `svrfmt/` (formats) et `third_party/ufbx/`. Pas encore lu en détail.
- **Licence** : le dépôt n'a **aucun fichier LICENSE** (vu le 2026-10-23). Son accord écrit (« feel FREE to use my
  mod maker as a base… as credit is given ») vaut autorisation ; lui proposer d'ajouter un LICENSE ou une ligne dans
  son README (ex. GPL-3.0 ou MIT) pour que ce soit clair pour tous. Ses dépendances ont leurs propres licences
  (ufbx, plume MIT, décodeur Bink dérivé de FFmpeg en LGPL) : à reprendre avec leurs avis.
- **Avant de reprendre une ligne de son code** (règles du dépôt) : relire le code, puis accord de l'utilisateur.
- **Jamais de données du jeu** dans le Mod Maker, son dépôt ou ses releases (règle absolue du dépôt). Il lit les
  fichiers du jeu de l'utilisateur sur son disque ; les mods que les joueurs fabriquent et échangent les regardent.
- **Deux programmes, comme chez Kaiko** :
  - **WWE12 Mod Maker.exe** : *fabrique* les mods (pages par type, éditeur d'archives, conversions) ;
  - **le launcher** (`port/src/launcher`, page MODS déjà là) : *installe, active, désactive, met à jour* les mods.

## 2. Ce qu'on reprend de Kaiko tel quel (ou presque)

| Élément | Chez Kaiko | Pour WWE '12 |
|---|---|---|
| Conteneur | `.svrmod` = zip sans compression, `manifest.txt` à la racine | **`.svrmod` aussi** (décision de Kaiko, 2026-10-23 : pas d'extension à part) |
| Manifest | `clé=valeur` UTF-8, clés inconnues ignorées ; `type=`, `id=`, `name=`, `author=`, `version=`, `made_with=`, `needs=` | Identique, **plus `game=wwe12`** : c'est cette clé qui dit à quel jeu est le mod. Notre launcher refuse (ou convertit) un mod sans `game=wwe12` ; le sien pourra refuser `game=wwe12` |
| Installation | `Mods\<Type>\<id>\`, remplacement atomique (`.incoming` / `.old`), fichier `disabled` | Identique, côté launcher |
| Versions | comparaison `1.2.3`, `needs=` = version minimale du jeu | Identique (notre `VERSION`) |
| Fichiers « point » | état local (`.mm_project.txt`, `.applied`…) | Identique |
| Mods livrés avec le jeu | `Bundled Mods\` + `.bundled_state` | Plus tard, si on livre des mods |
| Lecteur zip | stored + deflate, chemins vérifiés (`..`, `:`, `\`) | Identique (sécurité : jamais d'écriture hors du dossier du mod) |
| Interface | C++ / ImGui | Notre launcher est déjà en ImGui (D3D11) : même outil |

## 3. Ce qui change : type par type

Chez Kaiko, tout le contenu « lourd » (modèles, numéros de catcheurs, mouvements, arènes) est propre à SvR 2011.
Son document (section 7) le dit : un WWE '12 doit le convertir ou le refuser.

| Type | Kaiko (SvR 2011) | WWE '12 : ce qu'on sait | Difficulté |
|---|---|---|---|
| `media` | musiques, vidéos d'entrée, rendus, sons | musiques perso **déjà faites** (dossier `music`) ; vidéos Bink remplaçables par le dossier `mods` | facile |
| `Background` | images du menu principal | à trouver dans les `.pac` de menus | facile à moyen |
| `signs` | pancartes de la foule | à trouver (`audience` ?) | moyen |
| textures | packs par empreinte (rendu) | à voir avec la conversation Rendu (remplacement par hachage) | moyen |
| `arena` | `arena.pac` + `base=` + bannière | **remplacement d'un `bgNN.pac`** : format connu (EPAC/PACH/BPE/JBOY/textures), **taille ≤ celle de l'arène remplacée**, outils `tools/dev/arena_convert.py` | moyen (WWE '12/'13), dur (SvR) |
| `matchtype` | interrupteurs | nos cvars existants (pas de mod nécessaire au début) | — |
| `superstar` / `attire` | `ch.pac` EPK8 + fiches CHAR/DAT et CHAR/PRO | modèles « style WWE '13 » (fiches de maillage de 184 octets, comme nos arènes), profils de 1 072 octets avec l'entrée à +0x1D4 (d'après Kaiko, **à vérifier**) ; ajout de catcheurs : piste des DLC (`wwe12_dlc.cpp`) | dur |
| `moves` | banques YMKs + MOVS/WAZE | animations ZMOT/YANM, pas YMKs (d'après Kaiko) | très dur |
| `backstage` | `bg78.pac` | `bg78.pac` existe aussi (coulisses) | moyen à dur |

## 4. Comment le jeu chargera les mods

Aujourd'hui : **dossier `mods` à côté de l'exe** (patch SDK `rexglue-sdk-overlay.patch`, `MountModsOverlay` dans
`wwe12_app.h`). Un fichier posé au même chemin que sur le disque remplace celui du jeu. Limites : fichiers entiers
seulement, en lecture, un seul fichier gagnant par chemin.

Il faudra donc, comme le « PacOverlay » de Kaiko, une **étape de fusion** :
- le launcher (ou le jeu au démarrage) construit les fichiers finaux à partir des mods activés
  (ex. deux mods qui touchent chacun une entrée de `misc.pac` → un seul `misc.pac` fusionné), dans un dossier
  généré (ex. `mods\.merged\`) ;
- recompression BPE avec `yukes_bpe.compress_like()` (garde les blocs de Yuke's, fichiers aussi petits que
  possible) ;
- tampon « déjà fusionné » (tailles + dates des mods) pour ne refaire la fusion que si quelque chose change.

Arènes : au début, **une arène modée remplace une arène du jeu** (même numéro `bgNN`). Des pages d'arènes en plus
(comme Kaiko) demandent de toucher au menu de choix d'arène : plus tard.

## 5. Formats WWE '12 : connus / à étudier

Connus (outils dans `tools/`) : EPAC + PACH (`arena_convert.py`), BPE de Yuke's (`yukes_bpe.py` : décompression,
compression, `compress_like`), JBOY (`tools/dev/jboy.py` : maillages de 184 octets, POF0, `scale`, shaders),
paquets de textures 400-402, banques de lumières YMKs, drapeaux d'objets 50001, règle de taille des arènes,
menus Lua compilés (`docs/menus_natifs.md`). Détails : `docs/limit_breaking.md`, sections « Arènes ».

À étudier : fichiers de catcheurs (`ch`, EPK8 ?), fiches CHAR/DAT et CHAR/PRO de WWE '12, table des chaînes
(`string.pac`), images de menus, pancartes, animations YANM.

## 6. Étapes proposées (une à la fois, testée avant la suivante)

1. **Code de Kaiko** : obtenir le source + la licence ; relecture ; liste de ce qu'on garde, adapte ou retire.
   *Fini quand* : l'utilisateur a validé la liste.
2. **Format `.svrmod` (avec `game=wwe12`) + gestionnaire dans le launcher** : installer un zip, liste des mods, activer/désactiver,
   mettre à jour, désinstaller ; les fichiers d'un mod vont dans le dossier `mods`. *Fini quand* : un mod
   d'essai (une vidéo de titantron) s'installe, s'active, se désactive, et le jeu le montre.
3. **Premiers types « simples »** : `media` (vidéos d'entrée, musiques), images de menus.
4. **Mod Maker, première version** : fenêtre ImGui, page « média », écriture du `.svrmod` ; éditeur d'archives
   en lecture (`.pac`).
5. **Arènes** : page « arène » (remplacer une arène par un `bgNN.pac` de WWE '12 ou WWE '13, contrôle de taille,
   réduction des textures si besoin comme le `FitFile` de Kaiko) ; SvR 2011 avec `arena_convert.py` en option.
6. **Fusion de `.pac`** (plusieurs mods sur un même fichier).
7. **Plus tard** : catcheurs et tenues, puis mouvements (gros chantiers d'étude).

## 6 bis. Revue du code de Kaiko (étape 1, 2026-10-23)

Dépôt cloné en lecture seule dans `work/kaiko-svr2011` (hors dépôt, rien exécuté), commit `b2521fe`. Son Mod Maker :
`port/modmaker/` ≈ 36 700 lignes (pages) + `svrfmt/` ≈ 8 400 (formats) ; programme `svr2011_modmaker` dans son
`CMakeLists.txt` (l. 347-383), ImGui du SDK + D3D11, plus des fichiers de son launcher (`unzip.c`, `bink_decode.c`,
`movie_maker.c`, `setup_tab.c`, `bg_map.h`, `caw_import.c`) et `third_party/ufbx`.

**Architecture** (`app.h`) : une fenêtre, un rail de pages à gauche, chaque page = un namespace avec `Draw()`,
`Problems()`, `Reset()` et des crochets de projet ; tout ce qui est lent passe par `RunInBackground` ; toute erreur
possible devient un « Problem » (une erreur bloque Save/Install, avec des boutons « Fix it »).

**Formats (`svrfmt/`)** : `archive.h` ouvre et réenregistre à l'identique toutes les archives Yuke's de SvR 2007 à
2K17 (EPAC/EPK8 12/16/20 octets, PACH, BPE, ZLIB, xcompress, HSPC/SHDC) ; `pac.h` (BPE, PACH, EPAC) ; `texture.h`
(paquets de textures, DDS DXT1/3/5 lecture et écriture) ; `jboy.h` (modèles 0xB4) ; `arena.h` (`FitBudget`/`FitFile`
= réduire les textures pour tenir dans la taille d'origine ; plafond de géométrie ≈ 65 450 sommets / 110 522 index) ;
**`wwe13.h` convertit déjà WWE '13 → SvR 2011** et décrit nos différences (maillages 0xB8 avec une adresse en plus à
+0x74, cm contre 10 cm, shaders `yBG_*`, collision HMD, arbre de visibilité, **fichiers de places de la foule dans le
paquet de la foule**, banque de lumières).

**Tri des fichiers** (garder / adapter / plus tard) :

| Verdict | Fichiers |
|---|---|
| **Garder** | `session_log` (journal, « signaler un problème »), `tool_run` (Python/ffmpeg), `json.h`, `simple_steps` (assistant pas à pas), `help_page` (manuel intégré, textes à remplacer), `audio_out`, `video_mf`, `loudness`, `disc_list`, `pac_editor`, `svrmod_cli` (outil en ligne de commande, utile aux tests), tout `svrfmt/` |
| **Adapter** | `app` (cœur : `svr2011.exe` l. 1227-1245, dossier de réglages l. 1746, roster 100-321 l. 806), `mod_check` (le « médecin » : routage `KindOfType` l. 911, clés communes l. 328), `installed_page`, `assets_page` (fiches de 184 octets), `icons_page`, `signs_page`, `backgrounds_page`, `media_page`, `attire_page`, `pac_attires`, `model_materials`/`material_panel`, **`arena_page`/`simple_arena`/`editor`** (bonne première cible ; éditeur 3D en « 1 unité = 10 cm », à passer en cm), `rig_map` (plus tard), `star_page` (plus tard, adresses `sub_8…` de SvR) |
| **Plus tard / à refaire** | `char_preview`, `hair_sim`, `anims_page` (YMKs), `full_preview`, `threads_page`, `moves_page`, `match_page`, `caw_page`, `simple_star`, `port_page`/`simple_port`/`other_page` (conversions d'autres jeux) |

**Changements transverses** : un seul assistant pour le dossier des mods (aujourd'hui 67 constructions de
`<jeu>\Mods\<type>` ; chez nous `mods`) ; `game=wwe12` à ajouter aux clés communes (`mod_check.cpp:328`), à écrire
dans chaque manifest et à vérifier dans `CheckModFolder` (rien ne lit `game=` aujourd'hui) ; noms du jeu, de l'exe,
du dossier de réglages ; les numéros propres à SvR (roster, arènes, adresses).

## 6 ter. Étape 2 : installation par le launcher (2026-10-23, **testée par l'utilisateur : OK**)

Test du 2026-10-23 : installation du mod d'essai (rangé dans `.svrmods\Arenas\wm26_test`, `mods\pac\bg\bg15.pac`
= lien physique identique à la v22, absent de la liste « fichiers du joueur »), WM 26 en jeu, désactivation →
WM XXVII revient (le lien est retiré, `deployed.txt` vide), mod de Kaiko (`raw_is_war.svrmod`) refusé avec le
message `game=wwe12`.

`port/src/launcher/mods.h` (module dans le style de `saves.h`, textes EN/FR), affiché en haut de la page MODS.
Le jeu n'est **pas** modifié : il lit toujours le dossier `mods` comme avant.
- **Format** : `.svrmod` = zip (stored ou deflate, CRC vérifiés ; chemins `..`, `:`, absolus, zip64 et zips
  chiffrés refusés) avec `manifest.txt` à la racine. **`game=wwe12` obligatoire** (sinon : « ce mod n'est pas pour
  WWE '12 ») ; `type=` parmi ceux de Kaiko (dossier par type, comme son `KindOfType`) ; `id=` nettoyé
  (`[A-Za-z0-9_.-]`). **Convention WWE '12** : les fichiers donnés au jeu sont sous `files/`, aux chemins du disque
  (`files/pac/bg/bg15.pac`, `files/movies/titantron/100.bik`).
- **Rangement** : `mods\.svrmods\<Type>\<id>\` (dépaqueté dans `.incoming` puis renommé ; un mod de même id est
  remplacé, son état désactivé gardé) ; fichier `disabled` = mod désactivé.
- **Pour le jeu** : les fichiers des mods actifs sont posés dans `mods\<chemin du disque>` en **liens physiques**
  (copie si impossible) ; `mods\.svrmods\deployed.txt` liste ce que le launcher a posé (chemin, mod, taille). Les
  fichiers mis à la main par le joueur ne sont **jamais** remplacés (conflit signalé) ; deux mods sur un même
  fichier : le premier (type puis nom de dossier) gagne, conflit signalé. Resynchronisé au démarrage du launcher
  (sauf jeu lancé), à chaque installation, activation, désactivation, suppression.
- **Page MODS** : « Installer un mod… », tableau Actif / Nom (nombre de fichiers) / Version / Auteur / Type /
  Supprimer (avec confirmation) ; boutons grisés tant que le jeu tourne ; la liste « fichiers du dossier mods »
  n'affiche plus que les fichiers du joueur.
- **Test** : launcher compilé seul (`cmake --build --preset win-amd64-release --target wwe12_launcher`), copié en
  `WWE12 Launcher (test mods).exe` dans le jeu installé ; mod d'essai `work/mods_test/wm26_test.svrmod` (la WM 26
  v22 en `type=arena`, `game=wwe12`) ; refus attendu pour un mod de Kaiko (`Bundled Mods\raw_is_war.svrmod`).

## 6 quater. Textures : dump, packs, mods `type=textures` (2026-10-23, **testés par l'utilisateur : OK**)

Test (jeu de développement, release) : 26 PNG dumpés sur l'écran-titre en 45 s, couleurs et transparence justes ;
le « '12 » rouge du logo (`256x128_dxt5_2aa11b16ac15a795`) recoloré en bleu et agrandi ×4 (1024×512) dans
`textures\test\` → affiché bleu et net au lancement suivant. Mod `type=textures` par le launcher : mod d'essai
`work/mods_test/logo_vert.svrmod` (même texture en vert, `textures/logo/titre_2aa11b16ac15a795.png` dans le zip),
installé depuis la page MODS du launcher de développement → « '12 » vert à l'écran-titre.


Demande de l'utilisateur. Repris des Texture Packs du portage SvR 2011 de Kaiko (`port/src/native/texture_packs.*` :
encodeur PNG, règle « nom finissant par `_<empreinte 16 hex>` », lecteurs PNG/DDS ; crédit), branché sur le miroir de
textures de re:Blue au lieu de son cache.
- **Jeu** : `port/src/native/wwe12_texture_packs.{h,cpp}` ; branchement dans
  `port/src/reblue/gpu/native_texture_mirror.cpp` (`Build2DMirror`, juste après `UntileFace`) : empreinte = XXH3 des
  blocs de l'image de base, dépliés et serrés (DXT1/3/5, 8:8:8:8, textures 2D) ; si un remplacement existe, la
  texture est faite à partir de lui (`BuildReplacementMirror`, sa taille, ses niveaux, lignes alignées sur 256) et le
  réarrangement de couleurs du jeu (swizzle) ne lui est pas appliqué (`t_replaced`). stb_image (PNG) pris dans
  `recomp/rexglue-sdk/thirdparty/stb` (une ligne dans `port/cmake/native_renderer.cmake`).
- **Dump** : cvar `wwe12_texture_dump` (false) → `texture_dumps\<l>x<h>_<dxt1|dxt3|dxt5|rgba>_<empreinte>.png` à côté
  de l'exe, une fois par empreinte (aussi d'une partie à l'autre), sur un fil de basse priorité, swizzle appliqué.
- **Packs** : cvar `wwe12_texture_packs` (true) ; tout `.png`/`.dds` sous `textures\` (sous-dossiers compris) dont
  le nom finit par `_<empreinte>` remplace la texture, à n'importe quelle taille (PNG : mips refaits ; DDS : ses mips).
  Lecture au premier affichage (synchrone pour l'instant : petite saccade possible la première fois).
- **Launcher** : page MODS, case « Dump des textures » (`wwe12_texture_dump` dans `wwe12.toml`), boutons « Ouvrir
  texture_dumps » / « Ouvrir textures » ; type de mod **`textures`** (le nôtre) : les images sous `textures/` dans le
  zip vont dans `textures\<id>\` à côté de l'exe (`deployed.txt` : `textures:<id>/…`).
- Pas encore : nom de la texture du jeu dans le nom du dump (Kaiko indexe les `.pac` pour ça), lecture en arrière-plan.

## 6 quinquies. Étape 3 : médias et images (2026-10-23, vidéos d'entrée **testées par l'utilisateur : OK**)

**Fait : vidéos d'entrée.** Page MODS du launcher en 3 onglets (Mods installés / Vidéos d'entrée / Textures).
Onglet « Vidéos d'entrée » (`port/src/modmaker/media.h` depuis l'étape 4) : catcheur (liste avec recherche), vidéo ou image,
cadrage (image entière / remplir / étirer), bandeau de rampe du jeu gardé ou non, durée max., aperçu avant/après,
puis « Créer le mod et l'installer » → `.svrmod` (`type=media`, `id=entrance_video_<nnn>`,
`files/movies/titantron/<nnn>.bik`, zip non compressé écrit par `mods::Manager::WriteZip`) installé aussitôt.
- **`movies/titantron/<nnn>.bik` = numéro du catcheur** : `pac/ch/chEtc.pac`, PACH à 0x4000 = fiches CHAR/DAT de
  290 octets (BPE), id u16 à +44, nom (Latin-1) à +46 (ex. 100 THE ROCK, 139 JOHN CENA, 161 RANDY ORTON). Les
  vidéos 401-427, 500-520, 616-725, 900-999 n'ont pas de fiche (vidéos génériques / créer une entrée ?) : listées
  « Video <nnn> ».
- `port/src/launcher/game_disc.h` : lecture des fichiers du jeu par le launcher (dossier `assets`/`game`, ou
  **l'ISO** : XDVDFS, partition à 0 / 0x2080000 / 0xFD90000 / 0x18300000), décodeur BPE en C++, noms des catcheurs.
- `port/src/modmaker/video/` : code de Kaiko (b2521fe, crédit en tête de chaque fichier) ; Media Foundation en
  chargement différé (éditions « N » de Windows). Le projet CMake compile maintenant aussi du C.
- Test : John Cena + un MP4 de l'utilisateur → vidéo vue en jeu pendant l'entrée.

**Fait : images (guide) et crédits** (vérifiés par l'utilisateur le 2026-10-23). Onglet Textures : guide en 5 étapes
(dump → `texture_dumps` → copie retouchée dans `textures` en gardant `_<empreinte>` → relancer → partager) et bouton
« Créer un mod de textures... » (`media::TextureMods` : un dossier → `.svrmod` `type=textures`, seuls les
`.png`/`.dds` nommés `..._<16 hex>` ; les autres sont comptés et signalés). Crédits Kaiko (vidéos, packs de textures)
et FFmpeg (lecteur Bink, LGPL 2.1+) dans la page À propos et `port/dist/THIRD_PARTY_NOTICES.txt` (+ stb_image).

Plan d'origine :

Constats :
- **Vidéos d'entrée** : `movies\titantron\<nnn>.bik` (217 fichiers) = **même format que SvR 2011** (Bink 1 `BIKi`,
  320×320, 29,97 i/s, sans son ; le haut = grand écran, le bas = bandeau de la rampe). `logo.bik` et `wweopen.bik`
  sont en 1024×512. Un mod vidéo n'a besoin d'**aucun code dans le jeu** : `files/movies/titantron/<nnn>.bik`
  passe déjà par l'installation de l'étape 2.
- Kaiko a un **fabricant de vidéos** (`port/launcher/movie_maker.c`, 751 lignes : MP4/image → Bink via Media
  Foundation/WIC) avec **son propre encodeur Bink** (`bink_encode.c`, 300 lignes) et un **lecteur Bink**
  (`bink_decode.c`, 817 lignes, porté de FFmpeg, **LGPL 2.1+**, compatible avec notre GPL-3.0). Réutilisables
  presque tels quels (même format), avec crédit.
- **Images de menus / renders** : déjà faisables avec les mods `type=textures` (dump → retouche → mod).
- **Musiques d'entrée des catcheurs existants et sons du jeu** : chez Kaiko, crochets dans le moteur de son du
  jeu (`media_mods.cpp`, évènements « Play_<nom> » remplacés). Chez nous, gros travail d'étude (sons en `.pck`) :
  plus tard.

Plan proposé :
1. Reprendre `bink_encode` + `bink_decode` + `movie_maker` (crédit Kaiko ; LGPL notée pour le lecteur).
2. Trouver quel numéro de `titantron\<nnn>.bik` correspond à quel catcheur (table, par l'étude du roster).
3. Page « Médias » (dans le launcher en attendant le Mod Maker.exe) : choisir un catcheur + une vidéo MP4 →
   aperçu → `.svrmod` `type=media` (`files/movies/titantron/<nnn>.bik`).
4. Aide pour les images : un petit guide « dump → retouche → mod de textures ».

## 6 sexies. Étape 4 : `WWE12 Mod Maker.exe`, première version (2026-10-23, **testée par l'utilisateur : OK**)

Décision de l'utilisateur : **le Mod Maker fabrique, le launcher installe**. Le launcher garde « Mods installés »
et le dump de textures, avec un bouton « Ouvrir le Mod Maker » en haut de la page MODS ; la fabrication (vidéos
d'entrée, guide et mods de textures) est passée dans le Mod Maker.
- Programme `wwe12_modmaker` → `WWE12 Mod Maker.exe` à côté du jeu (`port/src/modmaker/modmaker.cpp`, ressources
  `port/res/modmaker.rc.in`) ; même style que le launcher (`port/src/launcher/ui_style.h`, partagé) ; langue du jeu
  (`user_language` de `wwe12.toml`) ; installe ce qu'il fabrique par le même `mods::Manager`.
- Pages : Accueil, Vidéos (`media::EntranceVideos`), Textures (dump + `media::TextureMods`), **Fichiers du jeu**
  (`pac_page.h` : dossier `pac/...` du jeu ou fichier quelconque → arbre EPAC / PACH, BPE défait, aperçu des DDS et
  des paquets de textures sur damier, textes, export ; lecture seule), À propos (crédits).
- Lecteurs d'archives de Kaiko repris : `port/src/modmaker/svrfmt/` (`bytes.h`, `pac`, `archive`, `texture`), avec
  deux cales : `unzip.h` (zlib par `launcher/inflate.h`) et `xcompress.h` (LZX de WWE '13+ non repris : refusé
  avec un message). `game_disc.h` sait aussi lister les sous-dossiers (`ListDirs`).
- `tools/package.ps1` copie `WWE12 Mod Maker.exe` dans le zip et ajoute son empreinte SHA-256.
- Test : ouverture depuis le launcher, toutes les pages, vidéo d'entrée et mod de textures refaits, archives
  `.pac` parcourues avec aperçu.
- À vérifier par la Main avant la release : la mise à jour automatique du launcher doit aussi remplacer
  `WWE12 Mod Maker.exe` ; readmes à compléter (Mod Maker, dossiers `textures` / `texture_dumps`).

## 6 septies. Étape 5 : page Arènes (2026-10-23, **testée en jeu par l'utilisateur : OK**)

`port/src/modmaker/arena_page.h` (page ARÈNES), `arena_edit.h`, `bpe_like.h`, `wic_image.h`.
- **Retoucher une arène** : arènes bg00-bg20 par leur nom (table de `limit_breaking.md`) ; toutes les images des
  paquets de textures de l'arène (ids 400-402, 32010... : 132 pour bg15) en vignettes avec recherche ; remplacer
  (image mise à la taille et au format DXT/ARGB de l'originale, mips gardés), exporter en PNG (WIC), remettre
  l'originale.
- **Autre fichier d'arène** : un `.pac` d'arène (vérifié : EPAC + section `STG `) mis dans un emplacement ; son
  numéro de scène (nom de l'entrée STG, `%04d`) est mis à celui de l'emplacement.
- **Taille** : le fichier ne doit jamais dépasser le `bgNN.pac` d'origine de l'emplacement. Reconstruction par
  l'archive de Kaiko (sans changement : fichier identique à l'octet) ; paquets modifiés recompressés en gardant les
  blocs BPE de Yuke's, aussi quand les données ont été décalées (`bpe_like::EncodeLike` : Kaiko seul = +2,5 %, soit
  +118 Ko sur bg15). Si ça dépasse encore : `Arena::Fit` retire le niveau le plus détaillé (mip 0) des plus grandes
  images non remplacées, sans réencodage (jamais les écrans vidéo ni les éclairages : `titan`, `tron`, `mov`,
  `light`, `lgt`), et dit lesquelles. Mesures sur bg15 : une petite image remplacée par un dégradé dépassait de
  2 Ko (l'arène est pleine) ; 3 grandes images en bruit → 3 images réduites, 10 061 824 / 10 063 872 octets.
- Test de l'utilisateur : tapis de WM XXVII (`rin_mat00`) remplacé par une image, `ar_dm` réduite pour tenir ;
  match à WrestleMania XXVII, l'arène charge et le tapis montre l'image.
- `.svrmod` `type=arena`, `id=arena_bgNN_<nom>`, `files/pac/bg/bgNN.pac`, installé aussitôt.

**Vue 3D des arènes** (2026-10-23, vue par l'utilisateur : OK) : page ARÈNES, « Vue 3D » (`arena_view.h`, D3D11
dans une image affichée par ImGui ; clic gauche tourner, clic droit déplacer, molette zoom). Modèles JBOY lus par
le `svrfmt/jboy` de Kaiko, **adapté aux maillages WWE '12** (0xB8 octets, champs à partir de +0x74 décalés de 4 ;
reconnus par le nom du shader à +0x7C) : 143 modèles de bg15 lus sans erreur. Les modèles d'arène sont déjà en
place (cm, y vers le bas) ; table 50001 : `r` dessiné aussi en miroir (x → -x), `a` additif, `m(n)` écarté (placé
par animation), `e` / `g` selon la case « Objets des entrées ». Image : `sampDiffusePrm` (case de la liste de
textures du modèle). **Lumière** : la couleur de chaque sommet est la lumière précalculée de l'arène (sol et tribunes sombres, accessoires blancs) : image × couleur du sommet (+ léger ombrage), fond presque noir ; écrans vidéo (`titantron_movie`, `minitron_movie`, `arena_movie` : pas d'image dans l'arène) en noir bleuté. Ni lightmaps séparées (UV non trouvées : une seule série d'UV par sommet), ni public, ni cordes (posées par le jeu). bg15 : 117
objets, 54 427 triangles. Vérifié d'abord hors écran (rendu dans des PNG).

## 6 octies. Page Catcheurs (2026-10-23, **testée en jeu par l'utilisateur : OK**)

`port/src/modmaker/superstar_page.h`. `pac/ch/chNNN.pac` (NNN = id du catcheur, noms de `chEtc.pac`) = EPK8, une
entrée « EMD » par tenue (`000NNNAA`). Chaque tenue = PACH : modèle du corps (id 0, ~9 400 sommets, 107 os, pose
de référence en T), textures 10 (`normal`, détails `MF..`, rides `WM_..`), 12 (`color` = la texture principale,
DXT1 2048×1024), 13-15 (`Mask1-3` : masques de couleur Superstar Threads, **16 bits non compressés** : pas encore
lus par `svrfmt/texture`, vignettes vides), yeux (20000).
- Page : catcheur (recherche) → tenue → images (remplacer / exporter PNG / remettre) + vue 3D du catcheur
  (`arena_view` : section = la tenue ; texture `g_mDiffusePrm` ; noms sans tenir compte des majuscules ; shaders
  `yCh_*` éclairés d'en haut au lieu de la lumière des sommets ; caméra de face si le modèle fait < 10 m).
- Mod `type=attire`, `files/pac/ch/chNNN.pac` ; la tenue ne dépasse jamais sa taille d'origine (`Fit` mesuré sur la
  section, prudence : limite réelle non vérifiée).
- Test de l'utilisateur : Alex Riley, tenue 1, `color` retouchée (short vert « RILEY ») → vu en match.

Suite possible : lire les masques 16 bits, couleurs plus justes (lightmaps), public et cordes dans la vue 3D, conversion d'arènes WWE '13 (même format que WWE '12) ou SvR 2011
(`arena_convert.py`), sons, catcheurs et tenues.

## 6 nonies. Nouveaux emplacements d'arènes (étapes 1 à 3 testées en jeu : OK)

Voulu par l'utilisateur : ajouter des arènes (portées de SvR 2011, WWE '13, 2K14, 2K16 : il a les disques) sans
remplacer celles du jeu. Méthode de Kaiko (SvR 2011) : des **pages** en plus dans le choix d'arène ; chaque arène
ajoutée a une **base** (une arène du jeu dont elle prend le numéro, l'éclairage, le thème VS et la mémoire) et
c'est son fichier qui est servi à la place de celui de la base quand on la choisit.

**Ce que fait le jeu** (étude en lecture seule du code recompilé ; adresses vérifiées en jeu sauf mention) :
- **Écran de choix d'arène** (exhibition) : classe native, constructeur **`sub_825A67A0`** (12 272 octets).
  Données à **0x82028560** (section en lecture seule de l'image : la page hôte est rendue modifiable par
  `VirtualProtect` avant d'y écrire, sinon violation d'accès) : +0 = 22 × {numéro de scène, drapeau} (0..20 puis
  300 = au hasard), +176 = grille 4 × 6 de cases {fiche, haut/bas/gauche/droite}, +368 = 21 × {scène, nom de la
  bannière « arenaNN »} (`sub_825A3CD8` lie les bannières, cherchées par numéro). Arène choisie : `sub_825A56C8`
  (case → fiche → scène), rangée par la préparation du match en u16 à matchSettings+898 (`sub_826CBFD0`).
- **Onglets** (this+3260) : LB / RB = `sub_825A5C88(this, 0/1)` (emplacements 10 / 13 de la vtable 0x820287E0),
  mais en exhibition un seul onglet est actif et **le jeu ne transmet même pas LB / RB** : les pages lisent la
  manette elles-mêmes pendant le dessin de l'écran (`sub_825A6A98`, chaque image : `sub_82A8C070` =
  XInputGetState, LB 0x0100 / RB 0x0200, clavier compris).
- Changement de page = les numéros des 21 fiches réécrits, puis ce que fait un changement d'onglet
  (`sub_824C3000`(+3444), `sub_825A3CD8`, `sub_825A3AB8`(0,0), widget +44 vtable+16 = `sub_828BF398`,
  `sub_824C3220`(+3368), `sub_824D19E8`).
- Fausse piste : une classe jumelle (`sub_826361A8`, table 0x83071ED0) n'est pas cet écran.
- **Bannières** : `menu/MatchHD.pac`, groupe MLYE/MUPA (entrée 7001, 71 DDS) : `arena00-03` 256×128 (grandes
  cases), `arena04-20` plus petites ; **35 `CAR_dummyNN` de 256×128 inutilisées** (place pour les bannières des
  arènes ajoutées). Non encore vérifié en jeu.
- **Taille d'une arène** : chaque fichier a sa taille dans **`plist360.arc`** (et `_4x3`) ; le chargement
  (`sub_823E02E0`) lit cette taille (`sub_827D99E0`), un fichier plus gros est lu tronqué → plantage (cause des
  plantages de WM 26). Mémoire : tas 12 (≈ 18,7 Mo, table {tas, Ko} à 0x83073260) doit avoir taille + 5 Mo libres.
  Non encore vérifié en jeu.

**Étape 1 (faite)** : `port/src/native/wwe12_arena_pages.cpp`, cvar `wwe12_arena_pages_test` (une 2e page = les
21 arènes à l'envers), lanceur `tools/dev/lancer_test_pages_arenes.bat`. Testé : O / U (RB / LB) changent de page.

**Étape 2 (faite, testée en jeu le 2026-10-24 : OK)** : arène ajoutée de test (WM XXVII au tapis en damier,
base 15) sur sa propre page, bannière en damier, match chargé avec ce tapis ; WM XXVII d'origine intacte.
- **Format d'une arène ajoutée** : mod installé `mods\.svrmods\Arenas\<id>\` (sans fichier `disabled`) dont le
  `manifest.txt` dit `slot=new` et `base=NN` (l'arène du jeu dont elle prend le numéro, l'éclairage, l'écran VS et la
  mémoire), avec `arena\arena.pac` (un `bgNN.pac` complet) et `arena\banner.dds` (256×128 DXT5 sans mips, 32 896
  octets). Jusqu'à 35 (les 35 `CAR_dummyNN` libres).
- **Pages** : après celles du jeu, 21 cases par page ; numéros de scène fictifs (150 + n) dans les fiches, la table
  des bannières (0x82028560+368) réécrite pour ces pages ; cases vides = 149, verrouillées (`sub_825A3318` accroché :
  disponible pour les ajoutées, pas pour 149). `sub_825A56C8` accroché : rend le numéro de la base et retient
  l'arène choisie.
- **Fichiers** (au démarrage, `Wwe12MountArenaSlots`, appelé dans `wwe12_app.h` **avant** le dossier mods, donc
  prioritaire) dans `Documents\wwe12\arena_slots` :
  - `pac\menu\MatchHD.pac` = celui du jeu (ou de mods) + le paquet MLYE/MUPA reconstruit (bannières dans les
    `CAR_dummyNN`, recompressé en gardant les blocs de Yuke's) **ajouté à la fin** ; le répertoire répond à
    `/MLYE/MUPA` (fichier n° 234) par ce paquet (`sub_827F29F0` accroché : +0 = début en secteurs de 2 Ko, +8 =
    taille en 256 octets). Marche.
  - `pac\bg\bgNN.pac` de chaque base = le fichier entier de l'arène qui joue là (celle du jeu, gardée dans
    `game\`, ou l'ajoutée), complété par des zéros jusqu'à la plus grande : **réécrit au choix de l'arène**
    (`Serve`). Le répertoire répond à `/STG/00NN` par le début de l'arène ajoutée (son enregistrement STG à 0x810 se
    compte **à partir de 0x4000** : + 8 secteurs) et une taille **jamais plus petite que celle du jeu** (les entrées
    compressées sont décompressées sur place dans cette place : une taille exacte plus petite plante).
  - Échecs notés : données de l'arène ajoutées à la fin du `bgNN.pac` de la base (plantage sur `lightmap00`, même
    cause de taille), adresse de départ comptée depuis 0 (lu l'en-tête).
- Les lecteurs de pac de Kaiko (`svrfmt/archive`, `pac`, `texture`) sont compilés aussi dans le jeu
  (`port/CMakeLists.txt`) ; `svrfmt/unzip.h` inclut `launcher/inflate.h` par un chemin relatif.
- Limites connues : l'arène choisie reste servie pour sa base jusqu'au prochain choix (un match de l'Univers dans la
  base juste après jouerait l'ajoutée) ; `MatchSD.pac` (4:3) pas encore fait ; le launcher n'installe pas encore ces
  mods (dossier `arena\`).

**Étape 3 (faite, testée en jeu par l'utilisateur le 2026-10-24 : OK)** : page ARÈNES du Mod Maker, choix
« Ajouter comme nouvelle arène (garde l'originale) » (par défaut) ou « Remplacer l'arène » ; bannière 256×128 en
aperçu : celle de l'arène de base (lue dans `MatchHD.pac`, `arenaNN`) ou une image du joueur (recadrée au centre en
2:1). Le mod : `type=arena`, `id=newarena_bgNN_<nom>`, `slot=new`, `base=NN`, `arena/arena.pac`,
`arena/banner.dds`. Le launcher (`mods.h`) l'accepte (dossier `arena/`) et le garde dans son dossier (rien dans
`mods\`) ; le jeu le lit au démarrage.

**Étapes suivantes** : 4) arènes plus grosses que leur base (taille du répertoire déjà prise en charge, mémoire du
tas 12 à vérifier) ; 5) portages (WWE '13, SvR 2011, 2K14, 2K16).

## 6 decies. Portage d'arènes de WWE '13 (2026-10-24 : Capitol Punishment et Vengeance testées en jeu : OK)

Fichiers du joueur : `Desktop\WWE13-Recomp-x64\WWE 13` (lus seulement, rien copié dans le dépôt). Les `bgNN.pac` de
WWE '13 sont presque tous XMemCompress (« 0x0FF512ED », LZX) : lecteur repris de Kaiko
(`port/src/modmaker/svrfmt/xcompress.cpp`, sur `lzxd.c` de libmspack du SDK ; le jeu, lui, a un bouchon :
`SVRFMT_NO_XCOMPRESS`). Sigle des objets → arène : bg05 = **cp (Capitol Punishment)**, bg03 wss, bg04 otl, bg06 mib,
bg07 ss, bg08 noc, bg09 hic, bg10 vg, bg11 sv, bg12 tlc, bg13 rr, bg14 elc, bg15 wm, bg16 er, bg28-49 = arènes
« légendes » (sd99, sv97, ono, ss97, wm14, oe98, kr, ss98, JD, sv98, un98, bd, rb, stv, WM15, snh...), bg00-02 non compressés.

**Format** : décompressé, une arène de WWE '13 = celle de WWE '12 à deux octets près (« EPK8 » au lieu de « EPAC », et
le mot du groupe STG à 0x804 porte un drapeau 0x0010 dans ses 16 bits hauts). Mêmes modèles (0xB8, cm, yBG), mêmes
paquets de textures.

**Recette (Capitol Punishment, base = 5)**, trouvée par essais (une variante par case, A à I) :
- décompresser, `EPK8` → `EPAC`, drapeau 0x804 effacé ;
- **50010 = celui de WWE '12** (le même dans les 21 arènes de WWE '12 : liste XML de 151 textures avec le niveau de
  mip à charger, 16 en pleine résolution ; celui de WWE '13 en met 129 en pleine résolution → le jeu attend sans fin
  de la mémoire : allocateur `sub_827D3600`/`sub_827D3438` appelé par `sub_8220D018`, depuis le chargement de la scène
  `sub_823BEC88` ; le hang_trace l'a montré). Les lumières, la foule, 50003, 50020, 30003 de WWE '13 marchent ;
- **ring de WWE '12** (951, 952, 953, 956, 960, 964, 973, 974, 980) : celui de WWE '13 est trop haut ; les images du
  tablier restent celles de WWE '13 (mêmes noms). 981 / 982 (poteau et couvercle en plus de WWE '13) enlevés ;
- **61000** (DDS 256×128, absente de WWE '13) prise dans la base ;
- **coins des barricades** 390-397 (`ar_fence01_c/f/g/h`, `ar_fence02_c/f/g/h`, pièces cassables des « OMG moments »,
  modélisées à l'origine et placées par le code de WWE '13) déplacés dans le modèle (sommets, sphères des maillages
  et des nœuds) : `_c` (+561, −559), `_f` (−561, −559), `_g` (+533, +560), `_h` (−533, +560) (x, z en cm) ;
- bannière : `arena05` absente du `MatchHD.pac` de WWE '13 sous ce nom (à chercher), tuile grise pour l'instant.

**Dans le Mod Maker (fait, testé par l'utilisateur le 2026-10-24 : Vengeance OK en match)** : page ARÈNES, mode
« Porter de WWE '13 » (`port/src/modmaker/port13.h` = la recette ; `arena_page.h`) : dossier de WWE '13, liste de ses
arènes (17 principales + 22 de l'époque Attitude, noms d'après les sigles et les bannières ; pas les coulisses ni les
structures), base = l'arène choisie en haut, « Voir en 3D », puis le mod (nouvelle arène par défaut). Résultat identique
octet pour octet à la variante I testée. **Bannières de WWE '13** : `MatchHD.pac` MLYE/**MUAS** (pas MUPA), entrée 7001 ;
`arenaNN` dans l'ordre de l'écran de choix de WWE '13 (SmackDown, RAW, NXT, Superstars, SummerSlam, Survivor Series,
Royal Rumble, WrestleMania XXVIII, Over the Limit, Capitol Punishment, Money in the Bank, Night of Champions, Hell in a
Cell, Vengeance, TLC, Elimination Chamber, Extreme Rules), **pas** celui des fichiers ; `AEM01-22` = bg28-49 dans
l'ordre. Table `port13::kArenas13`.

**Entrées (2026-10-24)** : les catcheurs flottaient pendant les entrées de Vengeance (base SmackDown). Cause : la
hauteur de la **rampe de la base**. Les arènes de WWE '12 ont une rampe à 1,80 m (`slo_floor` 203, y −180), sauf
Bragging Rights (bg10) et Royal Rumble (bg13), au sol ; les arènes de WWE '13 vues (Vengeance, Capitol Punishment)
l'ont au sol. Avec la base Royal Rumble **et la collision de la base** (995, 996, 997, 999), plus de flottement (testé
en jeu). La recette prend donc la collision de la base, et `port13::RampHeight` mesure les deux rampes : le Mod Maker
avertit (en jaune) et conseille Royal Rumble / Bragging Rights si elles ne concordent pas. **Poteaux** : sur la base
Royal Rumble, ils montraient le titantron : le poteau de la base (951) cherche `rin_pos00` et `rin_hone00`, absents
de Vengeance (qui a `rin_pos00_slv`) ; le jeu prend alors une autre image. La recette recopie maintenant du paquet 400
de la base les images du ring qui manquent. **Testé en jeu le 2026-10-24 (Orton / Cena) : entrées au sol, poteaux normaux.**

**Écran VS (« match card », 2026-10-24)** : `menu/MatchHD.pac` a pour chaque arène `MLYE/MBnn` (mise en page YXMLBIN
7000, images 7001, animation 7002) et `MENU/MnnF` (sa police : `font_royal` pour Royal Rumble, `font_venge` pour
Vengeance ; la mise en page la nomme), nn = numéro de l'arène (= numéro de fichier bgNN, dans les deux jeux). Même
format dans WWE '12 et WWE '13 (PACH non compressés ; MB70-84 identiques). Le Mod Maker copie `MB<fichier>` et
`M<fichier>F` de WWE '13 tels quels (`port13::Card`) dans `arena/card_mb.pac` et `arena/card_mf.pac` ; le jeu les
ajoute à la fin de son `MatchHD.pac` servi et, quand l'arène ajoutée est choisie, l'annuaire répond avec eux à
`/MLYE/MBnn` et `/MENU/MnnF` de la base (`wwe12_arena_pages.cpp`). **Testé en jeu le 2026-10-24 : identique à WWE '13** (Big Show / Kane), police comprise.

Reste : SvR 2011, 2K14, 2K16.

## 6 undecies. Portage de catcheurs de SvR 2011 (étude du 2026-10-24, Orton `ch161.pac` donné à l'utilisateur)

Fichier tiers sur le Bureau (`Desktop\ch161.pac`, 19 Mo, jamais copié dans le dépôt). Même contenant que WWE '12 (EPK8,
une entrée « EMD » par tenue `000NNNAA`, chacune un PACH) mais :
- **4 tenues** (02, 12, 22, 32) contre 2 (02, 12) dans le `ch161.pac` de WWE '12 (6,4 Mo) ; tenue SvR ≈ 4,3-5,5 Mo
  rangée contre ≈ 3,2 Mo : il faudra réduire des images (le jeu charge une tenue dans une place faite pour elle) ;
- entrées : SvR a 0 modèle, 10/11/12/13/14 textures (11 = CUBEMAP en plus), 40, 50, 80 (1 648 o, autre format), 100,
  200/201 (DDS), 20000 (cils), 30000/30001 (sang) ; WWE '12 a 0, 10, 12, 13, 14, 40, 50, 80 (2 816 o), 100, 20000,
  40000/40010/40040 (« DUMY »). Pour un catcheur déjà dans WWE '12, garder les entrées 40-100 et 4xxxx de WWE '12 ;
- **squelette différent** : SvR 102 os aux noms japonais (`root`, `koshi` bassin, `mune` poitrine, `onaka` ventre,
  `momo`/`momo_x` cuisse, `sune` tibia, `ashi` pied, `tsumasaki` orteils, `sakotsu` clavicule, `ninoude`/`_x`/`_m`
  bras, `kote`/`_x`/`_x2`/`_m` avant-bras et poignet, `te` main, `yubiNK` doigts, `kata_m` épaule, `mune_dummy`,
  `sakotsu_m`, `kubi`/`kubi_x` cou, `atama` tête) ; WWE '12 107 os « Biped » (`Bip`, `Bip Pelvis`, `Bip L Thigh`,
  `L_Thigh_x`, `Bip L Calf`, `Bip L Foot`, `Bip L Toe0`, `Bip Spine`, `Bip Spine1`, `L_Chest_Dummy`, `L_Clavicle_m`,
  `Bip Neck`, `Neck_x`, `Bip Head`, `Bip L Clavicle`, `L_Shoulder_m`, `Bip L UpperArm`, `L_UpperArm_x`/`_m`,
  `l_Elbow_j`, `L_Elbow_m`, `Bip L Forearm`, `L_Wrist_x`/`x2`, `Bip L Hand`, `Bip L FingerN[1|2]`, Finger0 = pouce) ;
  **os du visage aux mêmes noms** (`mayu`, `mabuta`, `eye`, `meziri`, `hoho`, `hana`, `kuchi`, `miken`) ;
- **échelle** : SvR = WWE '12 / 10, centré sur le bassin (cuisse 2,2 / 21,7 ; tibia 4,3 / 43,5 ; bras 2,0 / 20,0 ;
  avant-bras 1,4 / 14,1 ; main 0,9 / 9,3 : même squelette d'Orton) ; WWE '12 en cm depuis le sol, `Bip` à y −96,6.
  Sommets : pos × 10 + (0, −96,6, 0) (y vers le bas). Les sommets sont en position du corps (la vue 3D les montre
  debout sans squelette) ; palettes de maillage = index d'os + 1 → à renuméroter par nom ;
- **matériaux différents** : SvR `yBumpMapSS` / `yReflect` (37-39 réglages : texDiffuse, texNormal, texOcclusion,
  Wrinkle/Muscle…) ; WWE '12 `yCh_skin`, `yCh_skin_flex`, `yCh_gear`, `yCh_Reflect` (19-65 réglages : g_mDiffusePrm,
  g_mCubeLPrm, g_mNormalFxPrm, g_mNoiseTilePrm, g_mSweatPrm, g_mMaskPrm, g_mMusclePrm, IdMap…) ; les réglages
  « texture » (type 15) sont des index dans la liste de textures du modèle. `vfmt` : 0/3/6/8/9/12 (SvR) contre 0-3
  (WWE '12), sens inconnu. Mêmes nombres de sommets pour certains maillages (200, 357, 590, 738, 764), mais pas les
  mêmes sommets ni le même ordre ;
- format des maillages : fiche de 0xB8 octets dans WWE '12 (un mot de plus, pointeur) contre 0xB4 ; `JboyWrite`
  n'écrit que le format SvR ; `tools/dev/jboy.py svr_to_w12()` sait insérer le mot (arènes SvR).

**Plan** : 1) convertisseur (Mod Maker) : squelette de la tenue WWE '12 du même catcheur, sommets mis à l'échelle,
palettes renumérotées par nom, matériau de chaque maillage pris sur le maillage WWE '12 le plus proche (mêmes os,
même zone), textures `color` / `normal` / `Mask1` / `Mask2` de SvR à la place de celles de WWE '12 ; écriture au format
0xB8 ; 2) essai en jeu en remplaçant la tenue 1 de l'Orton de WWE '12 ; 3) catcheurs absents de WWE '12 (base = un
catcheur de même gabarit).

**Premier essai en jeu (2026-10-24, tenue 1, `port_svr.h`, fichier déposé à la main dans `mods\pac\ch\ch161.pac`)** :
**ça marche** : Orton de SvR 2011 s'affiche et s'anime (écran de choix, entrée avec la ceinture, match contre Cena),
tatouages, visage, short, genouillères, bottes corrects. Défauts vus : **doigts sombres en forme de griffes** (à
comprendre), image de couleur réduite de moitié pour tenir dans la place (1024×512). Méthode retenue : les deux jeux
n'orientent pas les repères des os pareil (tête et pieds de WWE '12 tournés d'un demi-tour) : chaque os est aligné
d'après la place des articulations (os → enfant le plus loin), pas d'après les rotations.
**Corrigé et revu en jeu le même jour** : les mains de SvR ont leurs propres images (`BloodHand0_L/R`, relief
`BloodHand_n_L/R` ; elles saignent en match), pas l'image du corps : elles sont recopiées dans le paquet 10 et le
matériau WWE '12 (`g_mDiffusePrm`, `g_mNormalFxPrm`) pointe dessus (sinon griffes noires). Image de couleur réencodée
au format de la base (DXT1) au lieu d'être réduite, **rendue opaque avant** (l'alpha de SvR sert à autre chose ; DXT1
faisait de tout pixel sous la moitié un pixel transparent noir : corps entièrement noir). Reste réduite : `normal`
(1024 → 512). Résultat (2e essai) : Orton de SvR 2011 propre en entrée et en match.
**Brillance** (remarque de l'utilisateur : « Orton brille un peu trop par rapport à Cena ») : WWE '12 lit l'intensité des
reflets dans l'alpha de l'image `normal` (poitrine : Orton WWE '12 16, Cena 30, Orton SvR 45). `port_svr::Shine` ramène
l'alpha de l'image de SvR à la moyenne de celle de la base (poitrine 45 → 30), réencodée au format de la base.
**Dans le Mod Maker** (page CATCHEURS, section repliable « Porter une tenue de SvR 2011 ») : choisir le catcheur et la
tenue de WWE '12 à remplacer (de préférence le même catcheur), le `chNNN.pac` de SvR 2011, une de ses tenues,
« Porter sur la tenue N » ; le résultat s'affiche (images, 3D) puis « Créer le mod » (type=attire, files/pac/ch/chNNN.pac,
tenu dans la taille d'origine). Le fichier de test déposé à la main a été retiré.

## 6 duodecies. Ajouter des catcheurs (étude commencée le 2026-10-24)

Demande de l'utilisateur : ajouter des catcheurs au jeu (sans en remplacer), portés d'autres jeux ou faits par les
moddeurs. Le jeu le fait déjà avec ses **DLC** : c'est la piste (un « DLC maison » par mod).

Un pack DLC installé (`Documents\wwe12\0000000000000000\54510890\00000002\<paquet>\`, ex. pack 4) contient :
`info/catalog.dlc`, `pac/ch/chNN_dlc04.pac` (modèle, NN = numéro du catcheur), `pac/DLC.pac`, `DLC_HD/SD_dlc04.pac`
(portraits ?), `pac/string.pac` (textes, copie complète), `edit_dlc04.pac`, `evtmov_dlc04.pac` (entrées), `m_dlc04.pac`
(prises ?), `gm_dlc04.pac`, `menu/sas_dlc04.pac`, `prtcls_dlc04.pac`, `sound_dlc_04.pac`, `sound/DLC_04.pck`
(musiques, voix), `movies/titantron/NNN.bik`, et des copies complètes de `audience.pac`, `m3.pac`, `misc.pac`,
`chEtc.pac`, `menu/mvMot.pac`, `evt/NyujyoC.pac`, `evt/WinningC.pac`. Les packs sont cumulatifs (le dernier porte
les copies les plus récentes).
- **`catalog.dlc`** : « DLCC », u16 2, u16 0x18 (taille d'en-tête), u32 numéro du pack, u16 nombre de lignes, u16 ?,
  8 octets (date) ; puis des lignes de 0x40 octets : u16 0x40, u16 rang, u16 type (1, 0x0B…), u16 pack, u16 ?, u16 ?,
  u32 0, **u32 numéro du catcheur** (0x46 = 70 Batista), …, **nom** à +0x20 (32 octets : « BATISTA », « DLC_CH_061 »).
- **`DLC.pac`** (EPAC, cumulatif : 94 208 o au pack 3, 96 256 au pack 4 ; le disque en a un aussi) : `ENEX/LSD`,
  `ENEX/BIN`, `ENEX/PRP`, **`IGST/VOLn`** (une par pack : u32 nombre, puis par catcheur : u32 type, u32 ?, u32 numéro,
  u32 0, u32 rang (0x0E…), 2 codes de 2 lettres (`MU`/`NU`, textes ?), u32 1, u32 2, clé de 16 caractères
  « WWE12BATISTA1234 ») et **`RGRP/MAGG`** (0 : table « HMT » ; 10 / 11 : noms des prises ajoutées, 104 octets
  chacune : u32 id 0x80EA…, « Scissors kick »…).
- Packs 1 et 2 installés chez l'utilisateur : seulement `catalog.dlc` (contenu absent).
- **Fin de `catalog.dlc` = liste des fichiers du pack** (nombre en en-tête +14) : par fichier u8 taille de la ligne
  (7 + nom), u8 0, u16 rang dans l'annuaire HD, u16 rang dans l'annuaire SD (0xFFFF : absent), u8 longueur, chemin
  (`pac\ch\ch70_dlc04.pac`, `movies\titantron\063.bik`, `sound\DLC_04.pck`). Ligne de catcheur : +0 u16 0x40,
  +2 rang, +4 type (1, 0x0B), +6 pack, +0x10 u32 numéro, +0x20 nom.
- **`info/catalog_hd.arc` / `_sd.arc` = annuaire du pack**, comme `plist360.arc` : pour chaque fichier, un repère (`FF 00 00 00`
  pour le 0, puis `FF FF` + u16 rang), puis **la table des groupes de son en-tête EPAC (0x800) recopiée en gros-boutiste**
  (groupe : type, u16 mots << 4, u16 « noms longs » << 12, u32 0 ; entrées : nom, u32 début, u32 taille) ; fin `FF FF FF FF`.
  Le jeu monte chaque pack sur `dlc%d:` (`dlc%d:\%s`) et lit `/IGST/VOL%d`.
- **Les 373 fiches de catcheurs (`pac\ch\chEtc.pac`, `CHAR/DAT` 290 o, `CHAR/PRO` 1 072 o = prises, `CHAR/DSBD` 8 o)
  existent toutes sur le disque, DLC compris** (les packs n'apportent qu'une copie mise à jour) : 0-49 catcheurs créés
  (« Edit 01-50 »), **50-99 emplacements DLC** (libres : 56-60, 67-69, 71, 73, 76-79, **85-99**, nommés « DLC 7 »…),
  100-372 disque (beaucoup de fiches vides « 0 », réserves 344-372). Fiche `CHAR/DAT` : u16 1, u16 0x122, 24 codes
  (deux fois 12), 4 octets par tenue, +0x2C **numéro**, +0x2E / +0x72 / +0xB6 **nom** (3 fois), +0xEF nombre de tenues,
  +0xF0 4 codes (= `DSBD`), +0xF8 et +0x102 **numéro**, +0x110 codes (musique, titantron ?).
- **Kaiko (SvR 2011, `port/docs/SUPERSTAR_MODS.md` de son dépôt, `work/kaiko-svr2011`)** ajoute des catcheurs dans les
  emplacements vides du jeu (137 numéros), fiche et profil copiés d'un catcheur de base (`base=`), modèle renommé au
  numéro, portraits générés, `slots.txt` pour garder les numéros ; mode simple en 10 étapes dans son Mod Maker.
- **Essai 1 (2026-10-24)** : pack maison n° 5 `WWE12PCTESTPACK05` (outil `mkdlc`, à partir du pack 4 de
  l'utilisateur) : catcheur 85 « TEST STAR » = fiche, prises et modèle de Batista (70), portraits de Batista en 0085,
  `IGST/VOL5` ajouté à `DLC.pac`. Cache `xbox_cache` mis de côté et sauvegardes copiées avant l'essai.
  1er lancement : erreur disque puis plantage, chemins de la liste sans « \ » (`DLC4:\PACCHCHETC.PAC` : faute de
  l'outil). **2e lancement : ça marche** : TEST STAR à l'écran de choix (portrait de Batista, « DOWNLOADABLE CONTENT »,
  OVERALL 90, RAW), entrée et match contre Cena normaux. Les packs sont montés dans l'ordre (le nôtre = `DLC4:`), et
  l'annuaire du dernier pack l'emporte. **Méthode validée : un catcheur ajouté = un emplacement DLC libre rempli par
  un pack maison.** Suite : le pack fabriqué au démarrage par le jeu (comme les arènes ajoutées) à partir des mods
  (modèle, nom, catcheur de base, portraits), sans recopier de données dans les mods ; page « Nouveau catcheur ».
- **Essai 2 (2026-10-24)** : même pack avec deux catcheurs, 85 « TEST STAR » et **104 « TEST DISC » (fiche vide du
  disque)** : **TEST DISC apparaît et joue (entrée, match)** : les fiches vides du disque marchent aussi par un pack
  (~220 places : 29 DLC + 164 fiches « 0 » + 29 réserves 344-372). Défauts : grand portrait absent à l'écran de choix
  pour TEST DISC (sans doute le portrait vide du disque, trouvé avant le nôtre), TEST STAR plus vu (à vérifier : case
  partagée haut / bas ? codes `WA`/`WB` de `VOL5` changés depuis l'essai 1 ?).
- **Essai 3 (codes « lettre + U » qui se suivent, comme les packs : `WU`/`XU`, `YU`/`ZU`)** : toujours seulement TEST
  DISC, et **Batista disparaît**. Liste de la case DLC (haut / bas, relevée par l'utilisateur) : Cole, Lawler, J.R.,
  Shawn Michaels, Edge, Christian, TEST DISC, Brodus Clay, Savage, Foley, Kane, The Rock = **12**. Avant nos essais elle
  avait déjà 12 catcheurs (avec Batista) : **la case DLC de l'écran de choix est limitée à 12** ; chaque ajout en chasse
  un (les divas ont leur case). Le jeu lit `/IGST/VOL0` à `VOL9` (boucle de 10 vers 0x8257AB78, chaîne 0x8202657C) :
  10 packs. **Suite : trouver dans le code de l'écran de choix la limite de 12 et la contourner (case agrandie ou case
  « mods »), comme Kaiko (case EXTRA dans SvR 2011).** Portraits des catcheurs 100+ : sur le disque seulement dans
  `menu\menuHD.pac` / `menuSD.pac` (`SSFA` 0100…0343, 262 272 o), **mais le jeu les cherche aussi dans les packs**
  (2026-10-26 : `sub_828B1428` / `sub_828AFE00` font `/SSFx/%04d` (table 0x83079B44), et `/SSFx/9000` (vide) si
  l'entrée manque ; même chemin pour tous les numéros ≥ 50) : **Jericho placé au n° 104 a ses portraits (écran de
  choix, écran VS), vérifié en jeu**. Pas de limite à 29 mods.
- **Cause réelle (2026-10-24, sonde `wwe12_roster_probe`, `port/src/native/wwe12_roster_probe.cpp`)** : pas de limite de 12.
  L'écran de choix (constructeur `sub_82570BC8`, panneau par joueur `sub_825A1200`, grille `/MEFP/POSI` de
  `menu.pac` lue par `sub_828B69D8`, case DLC = type 3 testée par `sub_82567068`) prend ses candidats dans
  `sub_828ADFB8` : numéros 50..372 (table 0x8307B8D0), gardés s'ils sont « débloqués » (octet 247 de la fiche de
  sauvegarde, 286 o, `[0x8317FA08]` + (numéro − 50) × 286) et, pour les DLC (octet 283 de la fiche), « possédés »
  (liste `[0x8322E238]` + 164, fiches {1, numéro, 0, état}, état 2 ; construite par `sub_826B13D0` depuis les lignes
  des catalogues). Puis **une seule case par « personne » : champ +0x100 de `CHAR/DAT`** (valeurs 0x0001-0x0433 ; les
  costumes alternatifs 80, 81, 83, 84 partagent celle de leur catcheur). Mes copies gardaient celle de Batista
  (0x042E) : seul le plus grand numéro restait. **Correctif : un numéro de personne libre par nouveau catcheur**
  (0x042F pour 85, 0x042D pour 104) : **Batista, TEST STAR et TEST DISC apparaissent tous les trois** (vérifié en jeu).
  Recette d'un catcheur ajouté : fiche, prises (`PRO`) d'un catcheur de base, numéro (+0x2C, +0xF8, +0x102), nom
  (+0x2E, +0x72, +0xB6), **codes de l'emplacement gardés (+0xF0 = `DSBD`)**, **personne libre (+0x100)** ; ligne de
  catalogue + entrée `VOL` (codes « lettre + U » qui se suivent).
- **Étape 2 faite (2026-10-24, testée en jeu)** : **le jeu fabrique le pack tout seul au démarrage**
  (`port/src/native/wwe12_superstar_mods.cpp`, appelé après `Wwe12InstallDlc` dans `wwe12_app.h` ; cvar
  `wwe12_superstar_mods`, défaut true ; recette dans `port/src/modmaker/superstar_pack.h`, partagée avec le Mod Maker).
  Mod = `mods\.svrmods\Superstars\<id>\manifest.txt` (`type=superstar`, `name=`, `base=NNN`) + `superstar\ch.pac`
  facultatif (sinon le modèle du catcheur de base) ; le launcher accepte `superstar/`. Pack `WWE12PCMODS` (numéro =
  dernier pack + 1, au plus 9), fait des fichiers du joueur (chEtc / DLC.pac / portraits du dernier pack installé, sinon
  du disque ; portraits des bases dans `DLC_HD/SD*.pac` et `menu\menuHD/SD.pac`), réécrit seulement s'il change (empreinte
  `wwe12pc_stamp.txt`, cache du jeu vidé alors), retiré sans mod. Numéros gardés dans
  `Documents\wwe12\superstar_slots.txt`. Emplacements : DLC 85-99, puis les autres DLC, puis les fiches vides du disque
  (222 chez l'utilisateur). Piège : **drapeau DLC +0x11F** à mettre à 1 (une copie d'un catcheur du disque, Orton, avait 0
  et n'apparaissait nulle part). Essai : « TEST STAR » (base Batista, sans modèle) et **« RANDY ORTON 2011 » (base Orton,
  modèle converti de SvR 2011) : nouveaux catcheurs à l'écran de choix, écran VS, entrée et match** (Batista toujours là).
- **Page « NOUVEAU CATCHEUR » du Mod Maker** (`port/src/modmaker/new_star_page.h`, 2026-10-24) : nom, catcheur de base
  (catcheurs du disque), apparence (celle de la base, un autre `chNNN.pac`, ou une tenue de SvR 2011 portée sur la
  tenue 1 de la base par `port_svr.h`, mise à la taille d'une tenue), vue 3D, puis `.svrmod` `type=superstar`
  (`superstar/ch.pac`, ou `superstar/look.txt` quand c'est l'apparence de la base). Au passage : accents de
  `modmaker.cpp` réparés (fichier ré-encodé deux fois : « CrÃ©ez »). **Essayée par l'utilisateur** : « orton svr11 » (base Orton, tenue
  1 de SvR 2011 portée) créé dans la page, installé, ajouté par le jeu au n° 87 (les n° 85 et 86 gardés) : **ORTON SVR11
  à l'écran de choix (portrait d'Orton, OVERALL 94) et en entrée**.

## 6 quindecies. Arènes de SvR 2011 dans le Mod Maker, depuis l'ISO (2026-10-25, WM 26 validée en jeu)

`port_svr_arena.h` (C++ de `tools/dev/arena_convert.py`, à la manière de `port13.h`), page ARÈNES, mode « Porter de
SvR 2011 » : l'ISO (ou le dossier) de SvR 2011 (`GameDisc::OpenPath`), ses arènes par nom (ordre de son `string.pac` :
bg11 = WrestleMania XXVI ; l'arène « bg15 » reçue d'un tiers était bg11 renommée), sa bannière (`MatchHD.pac`
MENU/MASI 7001, 256 × 128), en nouvelle arène. Vérifié contre la v22 : 225 entrées sur 226 identiques. Corrections
trouvées ensuite, une par essai :
- **foule sombre** : le code de WWE '12 cherche par leur nom `fade_lightmap00` / `fade_lightmap01` (aucun modèle ne les
  nomme ; SvR a `aud_lightmap`) → copiées de la base ;
- **tablier de WM XXVII sur un côté** : le tablier de la base cherche `rin_ep00` et `rin_ep01`, SvR n'a que `rin_ep00`
  → `rin_ep01` = le `rin_ep00` de l'arène ;
- **barricades traversées** : le code cherche les barricades **par nom** (`ar_fence01_a` à `_f`) ; celles de SvR ont
  d'autres noms → les `ar_fen*` de l'arène partent, celles de la base arrivent (numéros libres, lignes de 50001 avec la
  lumière des barricades d'origine), elles prennent les images de l'arène par leur nom (logos de WM 26) ;
- **logo qui disparaît selon la caméra** : 50020 (visibilité : nœuds par **nom**, boîte, nombre d'enfants) gardait la
  petite boîte de l'ancien `ar_fence02` de WM 26 → nœuds `ar_fen*` remplacés par ceux de la base sous la racine, enfants
  recomptés ;
- collisions (995-999) de la base (sans effet sur les barricades, gardé comme pour WWE '13) ; taille : le Mod Maker
  réduit `rin_mat00` (et une autre) pour tenir dans la place de WM XXVII.
Restent : **marches blanches** (objets « arme » de `bgEtc.pac` WPON 0x20-0x22, leurs propres images : sans doute
l'éclairage de WM 26), foule = celle de WM XXVII (la vraie foule de WM 26 : corps sans agrandissement, fichiers de
places ×10, d'après le code de Kaiko).

### Éléments 2D des arènes (2026-10-25)

- **Plaques d'entrée** (locator « ATLANTA, GA », plaques de nom) : `pac\evt\evtgdhd.pac` (index 190 de l'annuaire ;
  `evtgdsd.pac` en SD), un groupe **`ETEX/00NN` par arène** : images (0, 10, 20, 40), animations 2D `AG2D`
  (10000-10040), polices (20000-20011). **Même organisation dans WWE '12, WWE '13 et SvR 2011.** Le jeu sert celui d'une
  arène ajoutée (`arena/etex_hd.pac`, ajouté à la fin d'une copie servie, `/ETEX/00NN` de la base redirigé) :
  **WWE '13 validé en jeu (Vengeance : écran VS, locator, plaque de nom)** — ses entrées sont en `ZLIB`, décompressées
  et repassées en BPE (`port13::Etex` ; le Mod Maker est maintenant compilé avec `SVRFMT_INFLATE`). **SvR 2011 : pas de
  plantage, sa police s'affiche, mais les images des plaques sont mal placées** (ses animations 2D diffèrent) → non
  pris. Piège : un `evtgdhd.pac` reconstruit en entier dans `mods` a planté (l'annuaire garde la place de chaque
  entrée).
- **Écran VS de SvR 2011** : `MENU/MnnL` (mise en page YXMLBIN **VER0002**, lue par WWE '12) + `MENU/MnnI` (7001 / 7002)
  + `MENU/MnnF` ; mis dans le `MLYE/MBnn` de la base → **plantage** : le code de l'écran VS prend les éléments par
  numéro (`sub_82571BA8`, élément 30). Retiré ; les arènes SvR gardent l'écran VS de leur base. Piste : réhabiller
  l'écran VS et les plaques de WWE '12 avec les images de l'arène SvR.
- **Vidéo de transition** (2026-10-25, validée en jeu : Vengeance, WM 26) : `pac\evt\evtmov.pac` (index 192),
  `MOVT/M0NN`, Bink 1 640 × 640, même groupe dans WWE '12, WWE '13 et SvR 2011 → `arena/transition.bik`. Le jeu la
  met à la fin d'une copie d'`evtmov.pac` gardée dans `Documents\wwe12\arena_movies` (273 Mo, refaite seulement quand
  les vidéos changent) et redirige `/MOVT/M0NN` de la base. Piège : la mettre dans la copie de `MatchHD.pac` (numéro
  de fichier changé à +12) → écran noir, le lecteur vidéo attend sans fin.
- **Écran VS des arènes de SvR 2011** (2026-10-25, Royal Rumble validée en jeu) : un écran VS de WWE '12 habillé des
  images de l'arène (`port_svr_arena::Card`) : l'écran de WWE '12 aux images de mêmes noms et proportions s'il existe
  (WWE '12 a repris ceux de Hell in a Cell, RAW, Night of Champions, Superstars de SvR), sinon celui de Hell in a Cell
  (`MB09`) avec la plus grande image en fond, le logo (sinon la bannière), la plaque ; feu et grille cachés.
- **Foule de SvR 2011** (2026-10-25, WM 26 validée en jeu) : la sienne, personnes **non agrandies**, fichiers de places
  ×10 (sièges : les 4 nombres entre parenthèses ; zones `; R` / `; NR` : tous les nombres) ; celle de la base si un
  modèle ne se convertit pas.
- **Plaques d'entrée de SvR 2011** : toujours non prises (essai : 2e mise en page copiée dans 10000 / 10020, sans effet).
- **Écran VS et bases partagées** : le jeu ne recharge l'écran VS (`sub_82574E50` → `sub_828916B8`, partie 16) que
  si le **numéro** d'arène change (l'écran du match reste à son étape 11 sinon). Deux arènes ajoutées sur la même base
  (ou une et sa base) : en passant directement de l'une à l'autre, l'écran VS reste celui de la première. 4 essais
  ratés (renommer `MB%02d`, forcer la partie 16, repartir de l'étape 1 dans `sub_82576998`) : à reprendre.
- **Aperçu 3D** corrigé : les modèles de WWE '13 écrivent le type de bande `0x06000000` (`Strip::IsStrip`).
- Reste : marches blanches (objets de `bgEtc.pac`, ni les jeux de lumières 300-306 ni l'entrée 30001) ; locator des
  arènes de WWE '13 un peu bas (WWE '12 écrit la ville à une hauteur fixe).
- **Plaques d'entrée des arènes 2K14 à 2K17 (2026-10-30, vérifié hors du jeu, pas encore en jeu)** : leur `ETEX` n'a que la
  plaque copyright (20), une animation (10020) et des polices ; le **locator et la plaque de nom sont des vidéos**, une
  par arène : 2K16 / 2K17 `pac/mov/bg_locator|bg_name/NNNNNN.pac`, 2K15 `pac/evt/evtmov_bg_locate|evtmov_bg_name.pac`
  `MOVL|MOVN/MNNNNNN` (Bink 2, 1280 × 280-400, plaque + projecteurs sur fond noir : l'alpha n'est pas rendu par les outils),
  2K14 `pac/evt/evtmov3|evtmov2.pac` `MOVL|MOVN/MNNN` (**Bink 1 avec un plan alpha**, lu par notre décodeur : plan alpha
  ajouté à `video/bink_decode.c` comme FFmpeg, avant Y/V/U et suivi de 32 bits). `port13::PlateMovie2K` les trouve,
  `bink2::Still` prend l'image la plus remplie entre 30 et 80 % de la vidéo, `port_svr_arena::PlateCut` découpe la plaque
  (lignes où un trait lumineux court sur 1/5 de la largeur, + 1/3 de leur hauteur ; noir rendu transparent, ou la vraie
  transparence en 2K14). `EtexWearing` (modèle WrestleMania XXVII) la met dans la **barre (image 21)** des entrées 10
  (locator) et 0 (plaque de nom ; vu en jeu le 2026-10-30, d'abord inversées) et rend transparentes leurs autres images
  (logo, chevrons, traits). Le voile de l'alpha de 2K14 (< 24) est effacé (il faisait un rectangle sombre). Le texte reste
  écrit par WWE '12 à la place prévue pour la barre de WM XXVII : 1er essai en jeu (RAW de 2K14), nom du catcheur sur la
  plaque, ville au-dessus à gauche. 2e essai (inversion corrigée) : bonnes plaques, texte sur le logo.
  **Position (3e version)** : le texte de WWE '12 est placé par le code (les entrées 20001 / 20011 ne sont que la police),
  le calque `base` (image 21) des animations 10000 (nom) / 10010 (ville) est donc déplacé et agrandi
  (`detail::Ag2dPlace`) : AG2D gros-boutiste, calques (w, h, image, piste, nom), pistes (bit 0 = clés avec x, y ; clés
  = images × champs : image, précédente, suivante, x, y…), repos (+4 x, +8 y, +16 échelle) ; écran 1080p = 3 × place
  + 29 en y (mesuré). La plaque garde la taille et la place qu'elle a dans 2K (vidéo × 1,5, locator en haut, nom en
  bas) : la case du nom tombe sous le texte de WWE '12 ; le locator est recentré sur x = 605 (ville de WWE '12). Dans
  les 2K la case du texte change d'une arène à l'autre (2K17 : `evt/evtlayout.pac`, YXML par plaque, non lu).
  L'animation de la vidéo est perdue (image fixe). Sans outil Bink 2 (RAD Video Tools / Librempeg) : 2K15-2K17 gardent
  l'ancien habillage (logo de l'arène).

### Mes disques de jeu et assistant « Nouvelle arène » (2026-10-25, `docs/plan_page_arenes.md`)

- **MES DISQUES** (`game_discs.h`) : les autres jeux du joueur, ajoutés une fois (ISO ou dossier), reconnus par le
  numéro de titre de leur `default.xex` (SvR 2011 5451085D, WWE '13 545108B4, 2K14 545408B2, 2K15 545408B6, 2K16
  315A07D2, 2K17 545408BA), gardés dans `modmaker_discs.txt` à côté du Mod Maker ; « Chercher mes disques » fouille
  un dossier sur 3 niveaux. Rien n'est copié.
- **ARÈNES** en onglets : Nouvelle arène (l'assistant), Retoucher une arène du jeu, Autre fichier d'arène, Vue 3D.
  L'assistant (6 étapes, Retour / Suivant, problèmes en bas en rouge / jaune) : point de départ (SvR 2011 ou WWE '13
  depuis Mes disques, ISO ou dossier, ou un fichier d'arène SvR), base (tableau des 21 avec place, rampe, « aussi
  utilisée par » ; base proposée : même rampe, puis libre, puis assez de place ; le portage se fait en la choisissant),
  nom et bannière, ce qui est repris (cases), aperçu 3D, terminer (créer, installer, **Tester en jeu**).
- **WWE 2K14 à 2K17** (2026-10-25, vérifié hors du jeu, **pas encore en jeu**) : leurs arènes sont du genre de WWE '13
  (modèles de WWE '12 ; 2K14 / 2K15 EPAC, 2K16 EPAC xcompressé à entrées de 20 octets, 2K17 HSPC dont la liste est un
  SHDC) → la recette de `port13.h` (2K17 : ses entrées remises dans une copie de l'EPAC de la base, sa foule et ses
  fichiers de données manquants (50003) ceux de la base). bg00 des quatre jeux converti ; bg66 et plus (structures)
  écartés. Pas de noms d'arènes (ailleurs que dans l'ordre des fichiers) : l'étape 1 montre le **tablier** de l'arène
  choisie (`rin_ep00`). Catcheurs : `port13_star` marche aussi (Jericho ch104 de 2K14 et 2K17 sur Ziggler) ; leurs
  noms ne sont pas dans `chEtc.pac` (enregistrements de 208 / 216 / 220 octets) mais dans leurs textes : la fiche
  CHAR/DAT a le numéro à +4 et des numéros de textes « numéro + constante » ; nom complet = texte n° numéro + 64000
  (2K14) ou numéro + 6000 (2K15 à 2K17 ; +1000 / +3000 = noms courts, +9000 = nom Twitter), dans `pac/string.pac`
  (2K17 : `string_360.pac`, HSPC), tables SDB « u32 0, u32 nombre, nombre × {u32 position, u32 longueur, u32 n°} »,
  anglais = groupes `SDB /XX00` (2026-10-25 : 113 / 105 / 182 / 175 noms lus). Les numéros sans nom gardent ceux de
  WWE '13 / '12 au même numéro, marqués « ? ». **Noms des arènes 2K** (2026-10-25, vus à l'écran pour 2K16) : `pac\wuniData.pac` (données de l'Univers), tables « udt » (u32 `udt\2`, nombre, 52, … ; lignes dès +32) de {nom interne[40] (`PPV_RAW`, `PPV_WM31`…), u32 n° du texte, u32 **numéro d'arène**, u32 rang} : bg00 SMACKDOWN, bg01 RAW, bg04 WWE PAYBACK '14, bg15 WRESTLEMANIA 31… (2K15 à 2K17 ; n° < 1000 = emplacements d'arènes créées, sans nom). 2K14 n'a pas ces tables : aperçu du tablier seulement. Lecture commune : `strings2k.h`. Vérifié en
  pilotant le Mod Maker (Royal Rumble de SvR → Bragging Rights ; Capitol Punishment de WWE '13) ; Breaking Point
  (SvR) faite par l'utilisateur et chargée en jeu.

## 6 quaterdecies. Vidéo d'entrée des catcheurs ajoutés ; titantron et minitron séparés (2026-10-24)

- **Minitron** = l'écran de la scène sous le grand écran ; c'est le **bas (320 × 100)** de la même vidéo
  `movies\titantron\NNN.bik` que le titantron (le haut, 320 × 220). Un seul fichier par catcheur.
- Le **numéro de vidéo vient du script d'entrée** (commande vidéo, `sub_828F2410` lit commande + 172, venu de
  `sub_828D83B8` ; 999 = la vidéo propre du catcheur, pour les CAW, `sub_828C83F0`) ; `sub_82A316A0(n)` fait le chemin
  (`%03d`, hors 0-999 : `255.bik`), les packs DLC d'abord (`GAME:\` sinon). Un catcheur ajouté reprend l'entrée de sa
  base, donc sa vidéo.
- **Fait et testé en jeu** : un mod de catcheur peut avoir `superstar/titantron.bik` ; le pack du jeu l'ajoute en
  `movies\titantron\NNN.bik` (numéro libre : celui de l'emplacement s'il n'existe ni sur le disque ni dans un pack,
  sinon 373-399, 428-499, 551-615, 726-899 ; listé dans `catalog.dlc` sans annuaire, comme les packs officiels), et le
  crochet de `sub_82A316A0` remplace le numéro quand le catcheur qui entre (`sub_828CEF58(sub_828C21A8())` → place +16,
  `sub_8267CD30(sub_823F8270(place))` + 16 = **numéro × 100 + tenue**, ex. 8702) est ce catcheur. Essai : ORTON SVR11
  avec la vidéo de Cena, le vrai Orton garde la sienne.
- **Mod Maker** (compilé, pas encore essayé par l'utilisateur) : `media::MovieEditor` (titantron : celui du jeu / le
  vôtre ; minitron : celui du jeu / comme le titantron / le vôtre ; aperçu avant / après), utilisé par la page VIDÉOS et
  par la section « 3. Vidéo d'entrée » de la page NOUVEAU (vidéo de la base au départ).
- Idées notées : base facultative (une base par défaut) ; choisir les pièces une à une (prises, entrée, stats) ; deux
  mods pour le même catcheur (l'un titantron, l'autre minitron) assemblés par le jeu ; catcheurs de WWE '13 ; page
  « Bibliothèque » qui liste ce qu'on peut importer des jeux du joueur (jamais de données distribuées).

- **Vidéo d'entrée de SvR 2011 reprise (2026-10-25, compilé, pas encore essayé en jeu)** : à l'étape 4 de l'assistant
  NOUVEAU, un catcheur venu de SvR 2011 propose « La sienne dans SvR 2011 (NNN.bik) » : `movies/titantron/NNN.bik` de
  son jeu (disque de MES DISQUES, ou dossier du fichier choisi), même format que WWE '12 (Bink 1 `BIKi` 320 × 320),
  copiée telle quelle en `superstar/titantron.bik`. Numéro de vidéo = numéro du catcheur (85 vidéos pour 94 catcheurs
  dans SvR 2011 : les autres n'en ont pas à eux). WWE '13 et les 2K : vidéos en Bink 2 (`.bik2`), pas encore lues.

- **Portraits de SvR 2011 (2026-10-25, compilé, vérifié hors du jeu)** : un catcheur porté de SvR 2011 emporte ses
  portraits : `pac\DLC_HD.pac` / `DLC_SD.pac` de SvR (pas `menu\menuHD.pac`, qui n'a que des `RSCP/4NNN` 256 × 256),
  `SSFA` / `SSFB` / `SSFC` « NNNN », DXT5 sans mips comme ceux de WWE '12. HD et `SSFC` repris tels quels ; `SSFA` /
  `SSFB` SD refaits depuis les HD (SvR les range en 256 × 512, WWE '12 en 512 × 256 : l'image HD écrasée en hauteur).
  Cadrage un peu plus large que WWE '12 (jusqu'aux genoux). Les six ou aucun (sinon ceux de la base).

- **Portraits des catcheurs 2K (2026-10-25, compilé, vérifié hors du jeu)** : 2K14 a les `SSFA-C` de WWE '12 dans
  `menu\menuHD.pac` / `menuSD.pac` (déjà lus comme WWE '13). 2K15 et 2K16 : `menu\menuHD2.pac` (~70 Mo, gardé ouvert),
  `SFAG` / `SFBG` / `SFCG` « NNNN » en **TGA** non compressé 32 bits (512 × 512, 512 × 512, 256 × 256, même cadrage que
  WWE '12 ; 2K16 sans `SFBG` : `SFAG` à la place), convertis en DDS DXT5 sans mips (SD `SSFA` / `SSFB` 512 × 256).
  **2K17 (2026-10-25, vu à l'écran : AJ Styles)** : `menu\SSFaceHD.pac` (HSPC, noms hachés par une recette
  inconnue) : 352 TGA 512 × 512 = deux séries de 176 dans le même ordre (cadrages façon SSFA / SSFB), rangées dans
  le fichier (`order`) **par numéro**, parfois plusieurs par catcheur ; un SHDC dans un SHDC donne 161 icônes 64 × 64
  (DDS) **par numéro** (positions de l'en-tête en blocs de 16 octets : +0x0C = 16 ; lu et réécrit à l'identique par
  `svrfmt` maintenant). `faces2k::Match2K17` : chaque icône comparée au visage de chaque portrait (vers x 168, y 88,
  144 × 144), puis meilleure attribution **dans l'ordre** (programmation dynamique) : 99 sûres (toutes justes à l'œil),
  159 avec un second passage plus large (quelques erreurs, ex. The Rock → Stone Cold) : l'étape 1 montre le portrait
  trouvé, avec « Garder ce portrait (sinon celui de la base) ». SSFC fait depuis le premier.
- **Hachage des SHDC de 2K17 trouvé (2026-10-25)** : index d'un enfant = **MurmurHash64A du numéro en décimal
  (« 161 »), graine 0x12345678** (les 454 paires vues collent). `svrfmt::ShdcHashFor` le calcule : on peut ajouter
  n'importe quel numéro à une liste SHDC. Les noms des HSPC restent inconnus (essayés : noms de groupes, numéros,
  langues, « numéro + tenue » : rien).
- **Fichiers de catcheurs de 2K17 acceptés** (HSPC de SHDC, une par tenue ; « Tenue N (2K17) ») : AJ Styles porté et vu
  en 3D. **Noms des arènes de 2K14** : pas de table dans le jeu (logos) : liste écrite d'après les tabliers
  (`strings2k::kArenas2K14` : WrestleMania 1 à XIX = bg28-46, XX à XXVIII = bg47-55, XXX = bg56, 29 = bg15).

- **Base du même nom (2026-10-25, vu à l'écran)** : quand on choisit un catcheur d'un autre jeu (MES DISQUES) qui
  existe aussi dans WWE '12 (même nom, lettres et chiffres seulement), et que le joueur n'a pas choisi de base, ce
  catcheur de WWE '12 devient la base : le nouveau a ses **prises, stats et entrée** de WWE '12, et l'apparence de
  l'autre jeu (ex. Big Show de 2K16 sur Big Show de WWE '12). Changeable à l'étape 2 (le choix du joueur prime ensuite).

- **Page AIDE et « Tester en jeu » (2026-10-25, vus à l'écran)** : À PROPOS devient AIDE / HELP (et une ligne de
  l'accueil) : guides pas à pas repliables (vos autres jeux, nouvelle arène, nouveau catcheur, vidéos et textures,
  vos mods, ce qui n'est pas encore possible), comme le manuel intégré de Kaiko, puis les crédits. L'assistant
  NOUVEAU a, comme celui des arènes, un bouton « Tester en jeu » une fois le mod installé (`arena_page::LaunchGame`).

- **Vidéos Bink 2 des 2K (2026-10-25, vu à l'écran : AJ Styles de 2K17)**, comme le fait Kaiko (idée seulement,
  code à nous, `bink2.h`) : le Mod Maker utilise un outil **installé par le joueur** : RAD Video Tools (gratuit,
  `radvideo64.exe BinkConv <in> <out.avi>`, lancé sur un **bureau Windows à lui, jamais affiché** : ses fenêtres
  n'arrivent pas sur l'écran du joueur ; « Done » fermé, « Error opening » = refus) ou le ffmpeg de Librempeg (images
  brutes par un tuyau ; un ffmpeg ne compte que si `-decoders` liste bink2). Jamais la `bink2w64.dll` d'un jeu. AVI
  brut relu par nous, images réencodées par notre encodeur Bink 1. Outil trouvé seul (dossiers habituels, liste de
  désinstallation, PATH) ou choisi dans MES DISQUES (`modmaker_bink2.txt`). Lisibles : **2K15-2K17 (KB2i)** ; WWE '13
  (KB2d) et 2K14 (KB2f) : refusés par RAD Video Tools 2024. Mise en page des 2K : `movies\titantron\ent_NNNN_<tenue>_<n>.bk2`,
  320 × 320 = **une image 16:9 écrasée, sans bandeau** : convertie en 512 × 288 puis passée au fabricant de vidéos
  (titantron « le vôtre », remplir ; minitron « comme le titantron »). Étape 4 de NOUVEAU : « La sienne dans WWE 2K17
  (ent_0610_0_0.bk2) » ; la page VIDÉOS accepte aussi les .bk2 / .bik2. Conversion d'Orton (835 images) : 3,7 s.

- **Minitrons, walltrons et autres vidéos de WWE 2K17 (2026-10-25, vu à l'écran : AJ Styles)** : pas dans
  `movies\titantron` (que des titantrons plein écran : `ent_` catcheurs, `tag_` équipes, `bel_` ceintures 880 × 720,
  `cre_` créés, `uni_` Univers ; `ent_NNNN_<a>_<b>` = plusieurs versions, ex. Undertaker : croix, lune…) mais dans
  **`pac\mov`** (remarque de l'utilisateur) : `ss_mini` (112, minitrons des catcheurs, Bink 2 KB2i 256 × 128),
  `ss_wall` (96, walltrons 256 × 256), `bg_mini` / `bg_wall` / `bg_name` / `bg_locator` (arènes), `transition` (43) ;
  un `.pac` (HSPC) = une vidéo, nommé `NNNNvv` (numéro + version ; 1000xx / 2900xx : autres). Étape 4 de NOUVEAU :
  « La sienne dans WWE 2K17 » prend le titantron (version au choix) **et son minitron** (`ss_mini`, au choix, ou
  « comme le titantron ») : deux conversions à la suite (titantron 512 × 288, minitron à sa taille). Pas encore
  repris : walltrons (pas d'écran de ce genre dans WWE '12), vidéos des arènes 2K, transitions 2K.

- **Page VIDÉOS : la vidéo du même catcheur dans les autres jeux (2026-10-25, vu à l'écran : Randy Orton)** : bouton
  « Chercher sa vidéo dans mes autres jeux » : même nom (lettres et chiffres) dans SvR 2011 (`movies\titantron\NNN.bik`,
  pris tel quel : haut et bas) et WWE 2K15-2K17 (chaque `ent_NNNN_*.bk2` + le premier `pac\mov\ss_mini\NNNN*`,
  convertis avec l'outil du joueur) ; un clic remplit le titantron et le minitron du fabricant de vidéos. Orton : 5
  vidéos (SvR 2011, 2K15 ×2, 2K16, 2K17 ; 2K15 sans `ss_mini`). Noms des catcheurs lus en un seul endroit
  (`strings2k::SuperstarNames`), une fois par jeu.

- **Attributs des catcheurs ajoutés (2026-10-25 ; Mod Maker vu à l'écran, côté jeu compilé mais pas encore essayé en
  jeu : il faut l'exe refait)** : fiche `CHAR/DAT` (290 octets) **+4..+15 = les 12 attributs** (0-100), recopiés à
  +16..+27 ; ordre = celui des textes du jeu (`string.pac`, SDB 0000) : STRIKE POWER, GRAPPLE POWER, SUBMISSION, STRIKE
  DEFENSE, GRAPPLE DEFENSE, SPEED, AGILITY, ADRENALINE, RECOVERY, DURABILITY, CHARISMA, TAG TEAM (ex. Big Show 100 100
  85 75 80 45 40 65 75 100 90 95 ; Evan Bourne : Speed / Agility / Adrenaline 100). Le nom est en +0x2E, +0x72 et +0xB6
  (toujours le même : pas de nom court). Mod Maker : étape 3 « Nom et attributs » de NOUVEAU (ceux de la base, ou
  « ses propres attributs » au curseur) → manifest `attributes=12 nombres` ; jeu (`wwe12_superstar_mods.cpp` →
  `superstar_pack::Star::attributes`) : écrits dans la fiche du catcheur ajouté. La note générale est calculée par le
  jeu. Pas encore : nom annoncé par le speaker, style de combat, capacités (+28..+35 ?), musique, prises.

- **Portrait perso (2026-10-25, vu à l'écran ; pas encore essayé en jeu)** : étape 1 de NOUVEAU, section « Son
  portrait » pour toute apparence (même la base) : celui de son jeu (case pour le garder), **« Une image à vous... »**
  (PNG détouré de préférence) ou celui de la base. L'image est mise **entière** dans le carré 512 × 512 (transparent
  autour, posée sur le bord du bas), puis les 6 portraits du jeu : HD SSFA / SSFB 512 × 512, SSFC 256 × 256 ; SD
  SSFA / SSFB le carré écrasé en 512 × 256, SSFC 256 × 256 ; DXT5 sans mips (`superstar/SSF?_HD|SD.dds` du mod, comme
  ceux portés).

- **Test en jeu du 2026-10-26 (triple menace : CM Punk de SvR, AJ Styles de 2K17, Big Show de 2K16 + Orton)** :
  écran VS avec les portraits portés, AJ Styles et Big Show parfaits, vidéos de SvR et de 2K17 jouées, match normal.
  Défauts : (1) **plantage juste après le chargement de l'arène WrestleMania 31 de 2K16** (veille) : 207 de ses 227
  entrées gardaient l'emballage « ZLIB » de 2K16, que WWE '12 ne lit pas dans une arène ; `port13::PortTree` les
  remet en BPE (sous-paquets compris ; 2K14 à 2K17) ; à refaire et retester ; (2) **CM Punk de SvR : yeux blancs,
  visage raté** : le portage ne prenait que `Mask1` / `Mask2` (13, 14) de SvR et laissait le **`Mask3` (15) de la
  base**, dessiné pour l'image de couleur de la base ; maintenant les trois (SvR et WWE '13 / 2K), et le `Mask3` de
  la base est retiré quand le jeu d'origine n'en a pas (AJ Styles n'en a pas et s'affiche bien) ; à retester ;
  (3) un rectangle noir au milieu de l'écran VS (pas encore regardé).

- **Ports de SvR 2011 : visage, yeux, mains (2026-10-26, vérifié en rendu hors du jeu, pas encore en jeu)** :
  (1) **cheveux** : le portage ne convertissait que le corps (0) ; la pièce 10000 (cheveux) de la base restait et
  prenait ses couleurs dans l'image de SvR (CM Punk : cheveux de travers) → les cheveux de SvR sont convertis aussi
  (matériau des cheveux de la base) ; (2) **cils** : la pièce 20000 de la base (cils) dessinait des anneaux sombres
  sur l'image de SvR, et celle de SvR (4 morceaux autour des yeux) couvrait les yeux → **retirée** (des tenues de
  WWE '12 n'en ont pas : Michael Cole, Lawler, l'arbitre, Sin Cara) ; (3) **yeux** : SvR a deux morceaux (globes +
  voile plat brillant, ajouté par-dessus en SvR) ; le voile, avec le matériau opaque de l'œil de WWE '12, couvrait les
  yeux → retiré ; les yeux suivent la tête (pas leurs propres articulations) ; (4) **mains** : CM Punk a une pièce
  `blood_hand` (calque de sang rouge, montré en SvR seulement quand il saigne) → retirée (rendue telle quelle :
  mains rouges aux doigts pointus) ; les vraies mains d'Orton utilisent aussi les images BloodHand : gardées ;
  (5) les trois masques (Mask1-3). Les 93 catcheurs de SvR 2011 passent le portage sans erreur. Reste à voir en jeu ;
  les yeux de CM Punk paraissent encore clairs dans l'aperçu (Orton correct). Le `Mask` (512 × 512, paquet 10) de la
  base est gardé : SvR a un `Mk` aux canaux différents (essayé puis retiré).

- **CM Punk de SvR validé en jeu (2026-10-26)** : cheveux longs, barbe, yeux, portrait. Les mèches étaient
  dessinées mais toutes rejetées : le shader des cheveux de WWE '12 découpe avec **l'alpha de la normal map**
  (`Kill` si normal.a × alpha du sommet < seuil ; RenderDoc), et le **bleu** de la normal map y vaut à peu près la
  luminosité des cheveux (CM Punk de base : 15) ; SvR a sa brillance dans l'alpha et un bleu plein (cheveux
  invisibles, puis blonds et tachés). `port_svr::HairCut` : sous les UV des cheveux (10000), alpha = alpha de la
  couleur de SvR, bleu = luminosité de la couleur. Portraits SvR : 1,45 en hauteur, 0,92 en largeur.
- **Arènes 2K (2026-10-26, pas encore vu en jeu)** : (1) **place d'une arène ajoutée = 12,5 Mo** (au lieu de la
  taille de sa base) : le jeu la sert avec sa propre taille (`wwe12_arena_pages.cpp`), la limite est le tas 12
  (≈ 18,7 Mo, taille + 5 Mo libres) ; WrestleMania 31 de 2K16 (12,2 Mo une fois en BPE) n'a plus d'images réduites ;
  remplacer une arène garde la taille d'origine ; (2) **bannière du jeu d'origine** (`port13::Banner2K`) : 2K16 /
  2K17 `Pbg_NN_*` (512 × 256, NN = le fichier, `MnLayHD.pac` / `MnLayHD_WWEUnivese.pac`) ; 2K15 `arena_<nom>`
  (256 × 128, `MnLayHD.pac` MLYE/USCH) d'après les fiches `PPV_<nom>` de `wuniData.pac` (noms raccourcis : même
  début, même numéro ; PPV_ER = arena_ex) ; 2K14 table `kBanners2K14`. Vérifié hors du jeu sur 28 arènes.
- **Arènes 2K, suite (2026-10-26, vérifié en jeu sur WrestleMania 31 de 2K16)** : (3) **écrans vidéo** : WWE '12
  choisit la vidéo d'un écran **par son numéro** (RenderDoc : 800/803/806/808 titantron, 801/807/809 vidéo de l'arène,
  804/805 caméra en direct = effet « miroir ») ; les 2K les dessinent depuis une image `layout_*` (quart haut = rubans,
  reste = titantron) ; `port13::ScreensTo12` renumérote par rôle (801 mur → 808, 808 bandeau → 800, 805 bande LED →
  803, 800 → 807 ; doublons 802-804/807/809 retirés ; caches `titan_off` retirés) et recale les UV sur la vidéo
  320 × 320 de WWE '12 ; `LedPanelsTo12` : panneaux `yBG_UVScroll` à plusieurs images → `yBG_Default` ; (4)
  **transition** : 2K15-2K17 en Bink 2 (2K16/17 `pac/mov/transition/0000NN.pac`, 2K15 `evtmov_transition.pac`)
  → Bink 1 320 × 320 avec alpha (encodeur : plan alpha avant Y, chaque partie précédée de sa taille ; RAD Video Tools
  ne rend pas l'alpha : fondu de 8 images) ; 2K14 déjà en Bink 1 ; (5) **écran VS** : la mise en page 2K a 14
  éléments contre 15 (pris par numéro : noms absents, rectangle noir) et une police version 104 ; un écran VS de
  WWE '12 porte les images de 2K (`port_svr_arena::CardWearing`, commun avec SvR 2011) ; leur logo est rangé tassé
  (256 × 256) et affiché 3,4 fois plus large que haut par 2K16 : remis à ces proportions ; (6) **plaques de nom /
  locator** : les ETEX des 2K n'ont que le bandeau © (20) et des polices v104 (leurs plaques sont des vidéos,
  `pac/mov/bg_name`, `bg_locator`) : l'ETEX/0015 de WWE '12 (WrestleMania XXVII ; logo = images 0 et 6-20 des
  entrées 0 et 10) porte le logo de l'arène et son bandeau © (`EtexWearing`) ; jugé « bof » par l'utilisateur, à
  revoir plus tard (et reprendre davantage des jeux récents). Reste : le disque du rond central de WM 31 caché par le
  mur vidéo (laissé de côté).
- **Écran VS d'une arène ajoutée, défaut de mémoire (corrigé et vérifié en jeu, 2026-10-26, `wwe12_arena_pages.cpp`)**
  : choisie après son arène de base (même numéro de scène), elle montrait l'écran VS de la base. Deux causes : (a) le
  réglage du match (`sub_82576120`) ne refait la scène et son écran VS (`sub_828AC150`) que si le numéro change (+88
  de l'objet de `sub_828A9898`) : après un tel choix, ce numéro est oublié (-1) ; (b) le jeu garde ce qu'il a lu d'une
  entrée du répertoire : la demande `/MLYE/MB<base>` est réécrite en `MB21` / `MB22` (entrées qu'aucune arène
  n'affiche) et y répond avec la carte ajoutée. Fausses pistes : un nom propre `MB150` (numéro utilisé ailleurs,
  affichage cassé), le calque 16 de l'écran (jamais le chemin pris).

## 6 sexdecies. Import FBX des catcheurs (2026-10-26, vu en jeu)

- **Mod Maker** : NOUVEAU CATCHEUR, étape 1, « Un modèle 3D (.fbx) » (`fbx_star.h`). Lecture par ufbx 0.23.1 (MIT,
  `third_party/ufbx`), squelette reconnu par le nom des os (`rig_map.cpp` de Kaiko : Mixamo, Unity / VRChat, Rigify,
  3ds Max Biped, Unreal, SvR, WWE / 2K), pose et taille sur le squelette WWE '12 de la base (code de pose de Kaiko,
  `char_fbx.cpp`), chaque sommet recollé sur les articulations de la base, matériaux de la base (peau `yCh_skin` sans
  muscles ni rides, `yCh_gear`, `yCh_Reflect`), 20 os par maillage, textures en une image à la taille de la couleur de la
  base, détail réduit au-delà d'un budget (12 000 par défaut), masques Superstar Threads vides, 10000 / 20000 retirés.
- **Essai** : l'Orton de SvR 2011 exporté en FBX (export de Kaiko, hors dépôt) importé sur CM Punk : **en jeu, en match
  et en entrée (celle de CM Punk)**. Défaut vu : une tache claire sur le front (sans doute une décalcomanie de rides de
  SvR, transparente chez lui, opaque ici). Pas encore essayé : un FBX venu d'ailleurs (Mixamo…).
- **Places des catcheurs ajoutés (trouvé en chemin)** : la sauvegarde garde sa copie de chaque fiche (`SaveData.Dat` :
  CHAR/DAT sans ses 4 premiers octets à 0x1C, 286 o par catcheur, 13 nombres 16 bits retournés ; CHAR/PRO = prises et
  entrée, sans ses 4 octets, 1068 o, nombres 16 bits retournés jusqu'à l'octet 486, à 324792 ; même ordre, index = table
  u16 0x8307B5E0). Le jeu ne la prépare pour une place DLC (85-99) qu'en découvrant un pack : une place ajoutée plus tard à
  notre pack restait verrouillée (cachée), puis « DLC 37 » / OVERALL 0, écran noir au match. **Les nouveaux catcheurs
  prennent maintenant d'abord une fiche vide du disque (100+)**, et le jeu recopie la fiche et les prises du mod dans la
  sauvegarde quand elles diffèrent (`sub_828ADFB8`, cvar `wwe12_superstar_roster_fix`, contrôle sur John Cena avant
  d'écrire les prises). Sans les prises, pas d'entrée.

## 6 septdecies. Mod doctor et tenues comme mod à part (2026-10-26)

- **VÉRIFIER LES MODS** (`mod_doctor.h`, d'après le `mod_check.cpp` de Kaiko) : lit chaque mod installé et dit en clair
  ce qui ira mal (base absente, modèle illisible, NaN, plus de 20 os par maillage, vidéo pas en Bink 1, arène de plus de
  12,5 Mo, bannière pas en 256 × 128, fichier que le disque n'a pas, deux mods sur le même fichier ou la même tenue,
  numéros gardés pour des mods partis…), avec un bouton quand c'est réparable (désactiver, retirer la vidéo, oublier les
  numéros).
- **Tenues** (`attire_mod.h`) : un mod `type=attire` ne contient plus que sa tenue (`attire/attire.pac` + `superstar=NNN`,
  `attire=000NNNAK`) ; le jeu la met dans notre pack DLC (`superstar_pack.h`), dont l'annuaire passe avant celui du disque :
  **remplacer une tenue marche (vu en jeu : Orton, tenue 2 remplacée par un modèle importé, entrée et match)**. Plusieurs
  mods sur un catcheur vont ensemble. Faits par la page CATCHEURS (retouche) et par NOUVEAU CATCHEUR (« Une tenue de
  <base> » : toute source, SvR, WWE '13 / 2K, FBX).
- **Ajouter une tenue (5ᵉ… ou 3ᵉ d'Orton) : pas encore**. Le jeu la trouve (`sub_82A342B0` / `sub_82A335C0` cherchent
  `/EMD/%06d%02d` dans l'annuaire, code = (catcheur × 10 + tenue) × 10 + sorte), et les lignes de DLC de sorte 2
  (VOL / catalog.dlc : catcheur, indice de tenue, comme Miz, Hawk, Animal) sont écrites, mais l'écran des tenues ne la
  propose pas : sa liste vient d'ailleurs (pas de `DSBD` = numéros des noms ? pas les octets +0x116 de la fiche). Option
  retirée du Mod Maker en attendant.
- Pièges : un fichier de catcheur refait et servi entier garde l'annuaire du disque (tenue plus grosse = suivantes
  décalées) ; d'où le pack.

## 6 octodecies. Pack média (2026-10-26)

- Page **MÉDIA** (`media_page.h`) : pour les catcheurs du jeu, un **portrait** (une image → SSFA/B/C HD et SD, comme le
  portrait perso de NOUVEAU CATCHEUR) et une **musique d'entrée** ; un `.svrmod` `type=media` (`media/portraits/NNN/`,
  `media/music/NNN.mp3`).
- **Portraits : vus en jeu** (Randy Orton avec le portrait de Jericho à l'écran de choix) : le jeu les met dans les pacs
  DLC_HD / DLC_SD de notre pack sous le numéro du catcheur ; l'annuaire du pack passe avant `menuHD.pac`. Reste : les
  petites icônes de la grille (ailleurs : MENU/SSFD d'après Kaiko).
- **Musiques** : copiées au démarrage dans le dossier `music` (« <pack> <numéro>.mp3 ») ; on les choisit dans Créer une
  entrée (PLAYLIST UTILISATEUR), volume réglé comme les musiques du jeu. Remplacer le thème d'un catcheur sans passer
  par là demanderait d'écrire ses entrées dans la sauvegarde : pas fait.
- Vidéos d'entrée : page VIDÉOS ; autres images et sons : page TEXTURES et mods de fichiers (`files/`).

## 6 novodecies. Pancartes du public (2026-10-26, vues en jeu)

- `pacudience.pac` AUDE/BORD : un PACH (enfant 0) de 393 lots d'une image « no01 » 128 × 64 DXT1 (mips) ; numéro =
  catcheur × 10 + 1..4 (1611-1614 : Randy Orton), plus les pancartes générales du public 4001-4621 et 5001-5151.
- Mod `type=signs` : `signs/NNN_K.dds` (catcheur NNN, pancarte K) ou `signs/<numéro>.dds` (n'importe laquelle, les
  générales aussi). Le jeu remet AUDE/BORD (et CTRL) dans notre pack (`pac/audience_dlcNN.pac`), pancartes remplacées
  ou ajoutées ; **les catcheurs ajoutés reçoivent celles de leur base**. Vu en jeu : les pancartes générales
  remplacées par des magenta.
- Page **PANCARTES** du Mod Maker (`signs_page.h`, d'après celle de Kaiko) : un catcheur, quatre images (posées sur un
  carton blanc).

## 6 vicies. Fonds du menu principal (2026-10-26, vu en jeu)

- Le menu principal affiche `MnLayHD.pac` MLYE/EM00 (EL00 selon la région) : `ortontex02` (1024 × 1024, 32 bits dans EM00,
  DXT5 dans EL00) = Orton à côté du menu, `ortontex01` (512 × 1024) = l'écran-titre. Le jeu nomme `/MLYE/EM%02d` (n° au
  champ +268 de l'objet du menu, `sub_82543700`), mais ces mises en page sont lues d'un bloc au démarrage : un EM01 dans
  notre pack donnait un écran noir.
- Mod `type=background` (`backgrounds/NN/menu.dds`, `title.dds` facultatif) : à chaque lancement, un fond est tiré au sort
  (ceux des mods et celui du jeu) ; les images d'EM00 / EL00 sont remplacées dans une copie de MnLayHD / MnLaySD.pac, au
  même format et à la même taille (chaque entrée garde sa place), servie depuis `Documents\wwe12\menu_background`.
  **Vu en jeu : Jericho au menu principal.** Page **FONDS DU MENU** du Mod Maker (`backgrounds_page.h`).

## 6 unvicies. Éditeur 3D d'arène, visionneuses, projets, manuel (2026-10-26)

- **Éditeur 3D** (page ARÈNES, onglet « Éditeur 3D », `arena_edit.h` Move, d'après l'Arena Editor de Kaiko) : un objet
  de l'arène (modèle de la section STG) déplacé (nœuds racines, cm, y vers le bas), tourné autour de la verticale, ou
  caché (envoyé 1 km sous le sol : le retirer pourrait laisser le jeu le chercher) ; le mod se fait comme une retouche.
  **Vu en jeu** (2026-10-26, bg13 Royal Rumble : la barrière `ar_fence02` soulevée d'un mètre). Le jeu dessine
  les sommets où ils sont stockés : déplacer le nœud racine ne fait rien, ce sont les sommets (et les sphères
  englobantes) qui bougent, la rotation se fait autour du centre de l'objet. Les pièces du ring (ids 951-980 :
  poteaux, cordes, tapis) sont placées par le jeu lui-même : retirées de la liste. Le modèle recompressé est un peu
  plus gros (notre BPE ~10 % moins bon que celui de Yuke's) : `Fit` réduit au besoin une image pour tenir dans la
  taille d'origine (un fichier trop gros fait planter le décompresseur du jeu, `sub_827E1428`).
- **Ajouter un objet** (bouton Dupliquer, `arena_edit.h` `AddCopy`) : une copie d'un objet de l'arène sous un numéro
  libre (39-99 puis 300-799 ; 950-999 sont au ring), avec la ligne de son modèle recopiée dans la table 50001
  (`<n> l(121) p`...), puis déplacée comme les autres. **Vu en jeu** (2026-10-26, bg13 : la barrière `ar_fence02`
  copiée 110 cm au-dessus de l'originale, les deux dessinées). Une copie se retire (Supprimer la copie).
- **REGARDER** (`look_page.h`) : les images du jeu par usage (portraits, bannières, écrans de chargement, pancartes,
  catcheur du menu, ceintures, logos des shows ; enregistrables en PNG) et les mods installés en 3D (catcheurs, tenues,
  arènes), et les animations (section suivante).
- **Projets** (NOUVEAU CATCHEUR) : `.wwe12proj` (lignes clé=valeur : base, apparence et ses fichiers, nom, attributs,
  portrait, tenue) enregistré et rouvert.
- **Manuel** : la page AIDE a des guides pour toutes les pages (FBX, tenues, média, pancartes, fonds, vérification).
- **Modes simples** : NOUVEAU CATCHEUR et Nouvelle arène sont des assistants pas à pas.

## 6 duovicies. Animations jouées sur un catcheur (2026-10-26, vu dans le Mod Maker)

- **Format** : les animations de WWE '12 ne sont pas les YMBs/YMKs de SvR 2011 mais des **YANM** (Yuke's, gros-boutiste),
  le même format que WWE '13 : le décodeur de Kaiko (`port/tools/yanm_decode.py` de son portage SvR 2011, rétro-ingénierie
  de l'exe de WWE '13) les lit tels quels. Porté en C++ : `port/src/modmaker/yanm.h` (crédit Kaiko).
- **Où** : `pac/m.pac` (270 Mo), groupes `ZMVM` (les banques du move-set : STAT postures, BASE provocations, FINI
  finitions, SPE1/2, STN1/2, GRND, CORN, CETB, ROPE, APRO/APR2, DIVI, RUN, TEAM, WEAP, ANAS/ANA2, ABIL, RUMB, LADR,
  CELL), `ZMOT` (CA01-CA92, WE00-04, UN00, RES0), `ZMBS`, `ZCAF`, `CAV` : 50 503 enregistrements en tout. Chaque entrée est
  un arbre PACH dont les feuilles sont les enregistrements ; l'id de la feuille = le numéro de l'animation
  (ex. ZMVM/FINI 33002 = « Scissors kick », la prise DLC de Booker T : le numéro d'une animation est celui de sa
  prise, MOVS/WAZE +0x90 en petit-boutiste ; corrigé le 2026-10-27, on l'avait pris pour l'Attitude Adjustment).
- **Enregistrement** : en-tête {taille, total, 0x20, premier bloc ; +0x20 u16 sorte ; +0x48 nombre de blocs ; +0x4C le
  numéro en texte}, table des blocs à 0x70 (0x28 octets : +4 nom sur 8 caractères, +0x24 décalage). Nom du bloc
  « PPPPWWWW » : P la partie, W la personne (0000 celui qui fait la prise, 0001 l'adversaire, 0002...). Un bloc = un
  morceau YANM : pistes nommées par un hachage du nom d'os (`h = rotl(h, 7) ^ c`), 24 pistes (« Bip », « Bip Pelvis »,
  jambes, colonne, bras, « vector »), ou un gabarit (en-tête mode ≠ 0) ; clés en angles d'Euler 8/16 bits, charnières à
  un axe, positions en deltas ; images à 60 Hz. Les rotations remplacent celle de l'os au repos, les positions
  s'ajoutent à sa translation ; les os hors piste (aides `_x`, `_m`, visage, doigts) gardent leur pose de repos.
- **Vue** (`arena_view.h`, `keep_skin` + `SetPose`) : chaque modèle d'une tenue a son squelette (table des nœuds, local
  = T(t) Rz Ry Rx) ; ses sommets sont dans la pose de repos ; palette du maillage → os = palette[emplacement] − 1 ;
  déformation sur le CPU (≈ 15 000 sommets), tampon dynamique. Un 2e catcheur (`Add`, personne 1) joue la piste de
  l'adversaire, face à lui (les positions des deux pistes sont dans le même repère).
- **Page** : REGARDER > Animations : catcheur et tenue, adversaire, catégorie, numéro, parties ; lecture, image,
  vitesse, boucle, caméra qui suit les hanches. Aides de test : `WWE12_MM_PAGE=9` et
  `WWE12_MM_ANIM=<catégorie>,<numéro>[,<partie>,<image>]`.
- **Pas encore** : les noms des prises (table des prises du jeu), la cinématique inverse des pieds (pistes 0x0001, non
  vues dans les banques ZMVM), les doigts, les animations d'entrée (autres fichiers), les cheveux et vêtements qui
  bougent.

## 6 tervicies. Cheveux qui bougent des catcheurs portés de SvR 2011 (2026-10-26, vu en jeu)

- **Le jeu** : comme SvR 2011, les cheveux longs de WWE '12 sont des chaînes d'os `kami_bNN` (racine sous « Bip Head »,
  puis `kami_bNN_00`, `_01`... alignés sur l'axe x local, 6 à 7 cm) dans le modèle des cheveux (10000), animées par la
  physique du jeu. Réglages dans la tenue : **entrée 60** (« ; SpringHairSetting », `Version2`, puis `Setting1`, `2`,
  `3`, `5`, chacun avec deux nombres d'en-tête et une ligne par chaîne `kami_bNN rayon viscosité gravité ressort
  amortissement A corrLongueur flexion corrMouvement`, fin `*` ; puis `ReferenceHair`, cinq liens par chaîne) et
  **entrée 61** (« ; SpringHairCollision », capsules `os x0 y0 z0 r0 x1 y1 z1 r1 k` sur les os Biped). 25 catcheurs du
  disque les ont (ch102, ch117...). SvR 2011 : même 60 sans en-têtes (une ligne par chaîne, unités de SvR ≈ 11 cm).
- **Portage** (`port_svr.h` `KeepChains`, `HairSettings`) : les os de cheveux de SvR sont gardés dans le modèle 10000
  converti, déplacés comme la tête (même déplacement que les mèches), rattachés à « Bip Head » ; les sommets restent
  pondérés sur eux (avant : collés à la tête et au haut du torse, `HangHair`, gardé quand SvR n'a pas de 60). Les chaînes
  de la base sont retirées. Entrée 60 écrite depuis celle de SvR (rayon et gravité x10, quatre réglages comme les
  catcheurs du jeu, aucun lien) ; 61 : celle de la base, sinon celle d'un catcheur aux cheveux longs lue sur le disque
  du joueur (`hair_donor.h`). Vérifié hors du jeu (SvR ch102 sur la base ch137 : 53 os, chaque maillon à 0,3-6 cm de ses
  mèches).
- **Vu en jeu** (2026-10-26, validé par l'utilisateur) : CM PUNK '10 (SvR ch131 sur la base ch131), 24 os de
  cheveux, entrée et match contre Big Show : les cheveux bougent, rien ne s'étire.
- **Pas encore** : les modèles FBX (os de cheveux à reconnaître et à changer en chaînes) ; les vêtements
  (les `R_Bone*` du corps de certains catcheurs : autre mécanisme, non étudié).
- **Mèches devant le front, corrigé (2026-10-27)** : DREW McINTYRE '10 (SvR ch278) avait dans WWE '12 des mèches
  raides devant le front (traits noirs), absentes dans SvR (capture du SvR 2011 de Kaiko : cheveux plaqués). Dans SvR
  ces mèches (tenues par la seule tête) sont glissées sous la peau du crâne du corps. Cause : le sens de la tête du
  modèle de cheveux se calculait vers son enfant le plus loin, la racine de chaîne `kami_b00`, tournée sur le
  `kami_b00` de la base (le Drew de WWE '12 a sa propre coiffure à chaînes) : toute la tête des cheveux pivotait, le
  crâne rond ne le montrait pas, les mèches sortaient du front. Les os de cheveux ne servent plus à orienter un os
  (`ConvertModel`). Vérifié hors jeu : Drew propre ; CM Punk (ch131), ch117 et Undertaker (ch103) inchangés.
  **Vu en jeu** (2026-10-27, validé par l'utilisateur) : Drew refait, front propre à l'entrée.
  Fausses pistes : transparence, envers des faces (cheveux de SvR dessinés des deux côtés : drapeaux de rendu du
  maillage à +0x88 = 0x114, sans 0x2, d'après `svr11_build.py` de Kaiko), retirer les faces tournées vers la tête.
- **Brillance** (2026-10-27) : l'alpha de la carte des normales de SvR est ramené à 0,8 de la moyenne de la base (plus
  contrasté que celui de WWE '12 : à moyenne égale, ses zones vives brillaient trop).

## 6 quinvicies. Ajouter une tenue : recherche (2026-10-26, pas trouvé)

- L'écran CHANGE ATTIRE ne vérifie pas l'existence des tenues quand on les fait défiler : une sonde sur
  `sub_827D98D8` (le jeu demande si une entrée existe : « /SSFB/0161 », « /MLYE/... ») n'a vu **aucune** demande
  « /EMD/... » ; seulement le portrait de la tenue suivante (/SSFB/1161, absent → celui de base). Le nombre de tenues
  est donc connu d'avance.
- Il n'est pas dans la fiche CHAR/DAT de `chEtc.pac` : ni octet, ni masque, ni moitié d'octet ne suit le nombre de
  tenues des 72 catcheurs (1 : 49, 2 : 14, 3 : 2, 4 : 2). Les 4 nombres à +0xF0 (id × 10 + 1..4) sont là pour tous.
- Les noms des tenues (« ORIGINAL ATTIRE », « Civilian Clothes », « Wrestling Gear », « Suit »...) sont une petite
  table de textes de l'écran de choix (`string.pac` SDB/0000 enfant 3, entrées de 16 octets {décalage, longueur, id, 0},
  ids 9 à 19) : le jeu a donc quelque part une table catcheur → tenues → nom, pas encore trouvée (ni dans les tables de
  `image.bin` qui listent les catcheurs par numéro).
- Suite possible : tracer en jeu la fonction de l'écran CHANGE ATTIRE (DROITE / GAUCHE) jusqu'à son maximum.

## 6 quatervicies. Types de match : état de la recherche (2026-10-26, rien de fait dans le jeu)

- **Chez Kaiko (SvR 2011)** : `docs/MATCH_TYPES_RESEARCH.md` et `port/src/match_types.cpp` de son portage. Chaque
  match = une fiche de règles (119 dans `misc.pac` `RUL`) ; 36 fiches sans ligne de menu, dont des matchs complets
  rendus jouables en ajoutant seulement une ligne de menu (Falls Count Anywhere, cage « pin & give up », Rumble à 15 et
  25...) ; d'autres types écrits dans son code natif (Lumberjack, Three Stages of Hell...).
- **WWE '12** : même table, `pac/misc.pac` à 0x9630 (copie à 0x46C630) : `RUL `, 119 fiches (0x77) de **108 octets**
  (SvR : 100) ; mêmes champs : +0 nombre de personnes, +2 six places de 3 octets, +28 / +32 / +44 / +48 textes
  (catégorie, sous-nom, famille, libellé), +36 mode (1 normal, 4 spécial), +40 arène imposée (Rumble 22, Chamber 21,
  coulisses 78), +56 nom complet, +60 nom interne, +68 drapeaux. En mémoire : `*(0x8324E238) + id × 108` (lecteurs :
  `mulli r,r3,108` dans une vingtaine de fonctions). Noms trouvés : Falls Count Anywhere (0x2B-0x30, dont « SINGLE
  w/MANAGER »), Special Referee (0x53), Championship Scramble (0x26), 40-Man Royal Rumble (0x17), Armageddon Hell in
  a Cell (0x35), Brawl-4-All / coulisses (0x18-0x21, 0x68-0x76).
- **Différence avec SvR** : le choix du type de match n'a pas de lignes dans `menu.pac` (seules « NORMAL MATCH »,
  lien 0x7D0, règle 0 en +0x5C) ni dans les scripts Lua de `MScript.pac` : la liste est faite par le code, à partir des
  fiches. Falls Count Anywhere 1 contre 1 (0x2B) a **exactement les drapeaux** du match normal (0x00) : son
  comportement est dans le code (tests par numéro), comme dans SvR.
- **Suite** : trouver la fonction qui construit la liste des types (appelants du lecteur de fiches), voir ce qui
  cache une fiche, puis l'essayer en jeu sur Falls Count Anywhere. C'est du code de jeu (conversation Gameplay ou
  Limit breaking) plus qu'un mod : à décider avec l'utilisateur.

## 6 sexvicies. Protéger les sauvegardes qui utilisent des catcheurs ajoutés (2026-10-26)

- **Le piège** : toutes les versions du jeu sur un PC partagent `Documents\wwe12` (sauvegarde, DLC installés, pack
  WWE12PCMODS). Une version de test lancée avec un dossier `mods` vide a retiré le pack, et la 0.9 de l'utilisateur a
  refusé sa sauvegarde (« le contenu téléchargeable semble manquant ou endommagé ») : la sauvegarde garde les fiches
  des catcheurs ajoutés. Réparé en relançant une fois la version qui avait les mods (pack refait, mêmes numéros).
- **Désormais** (`wwe12_superstar_mods.cpp`) : le pack n'est **jamais** retiré ; il note ses catcheurs
  (`WWE12PCMODS\wwe12pc_stars.txt`, « mod=numéro ») ; si l'un d'eux n'a plus son mod, le pack reste **tel quel**
  (avertissement dans le journal et `Documents\wwe12\mods_warning.txt`, affiché par la page MODS du launcher ; les
  mods de catcheurs ajoutés depuis n'entrent pas) jusqu'à ce que le Mod Maker l'en retire (VÉRIFIER > Retirer du
  pack). Avant chaque changement du pack, la sauvegarde est copiée dans `SaveBackups` (même format que les copies
  automatiques du launcher, 10 gardées).
- **Essais sans risque** : `tools/dev/lancer_isole.bat` lance la version de test avec `--user_data_root
  %USERPROFILE%\Documents\wwe12_test` (réglage du SDK) : sauvegarde, DLC et pack à part.
- **Règle pour les joueurs** (FAQ) : avant d'enlever un mod de catcheur, ne plus l'utiliser dans sa sauvegarde.

## 6 terdecies. Les ajouts dans les autres modes (2026-10-24, vérifié en jeu)

- **Catcheurs ajoutés** : présents partout sans rien de plus (le jeu les traite comme des DLC) : Univers (effectif,
  divisions, classements), Créer un scénario (CAST).
- **Arènes ajoutées : retirées de l'Univers (décision de l'utilisateur, 2026-10-26, « c'est bugué »)** : l'écran
  des arènes d'un show n'a plus que la page du jeu (indicateur 0x8324E51C = 1), et choisir un mode au menu principal
  oublie l'arène ajoutée choisie avant (`sub_8253A028`). Avant : l'écran des arènes d'un show de l'Univers est le même que celui des matchs
  (`sub_825A67A0`) : les pages LB / RB y marchent déjà (TEST ARENA en page 2).
- **Créer un scénario, liste LOCATION des scènes : mis de côté (décision de l'utilisateur)**, peu utile pour le
  risque (une histoire enregistrée avec une arène moddée, puis le mod retiré). Ce qu'on sait, pour reprendre :
  - éditeur de scène (vtable 0x82044408, constructeur `sub_8278B918`, destructeur `sub_8278A9E0` qui libère les
    tableaux par `sub_827D2EA0(p, 2)`) ; 3 onglets à éditeur + 432 + 12 × onglet = {n°, entrées, nombre}, remplis par
    `sub_8278AB58` (pile du constructeur) : ARENA = 21 entrées fixes {lieu 0-20, 0xB, bit 0, u16 déblocage
    (0xFFFF = toujours)} + lieux 50-59 si `sub_82A40680(n)` ; BACKSTAGE = lieux 67-79 (78 = 7 variantes) ; liste
    construite par `sub_8278B4C0`, nom par `sub_82789A40` → `sub_82A31A60(lieu)` = texte 40002 + n° d'arène ;
  - **table des lieux** du jeu : `[0x8324E24C]` = fiches de 36 octets (n° du lieu = octet +4), `[0x8324E250]` =
    nombre, cherchée par `sub_82A31780` (accesseurs `sub_82A31AA8` … `sub_82A31F30`) : y ajouter une fiche par arène
    ajoutée (copie de sa base) la ferait connaître de tout le jeu ;
  - reste inconnu : comment une scène passe de son lieu à l'arène chargée (le champ +898 des réglages de match,
    `sub_826CBFB8`, n'est écrit que par l'exhibition) ;
  - sonde `wwe12_story_probe` (`port/src/native/wwe12_story_probe.cpp`, défaut false, lignes « [story] »).

## 7. Questions ouvertes

- Pour Kaiko : ~~code C++~~ (réglé : `port/modmaker/` de son dépôt) ; ~~extension~~ (réglé : `.svrmod` +
  `game=wwe12`) ; un fichier LICENSE dans son dépôt (ou une ligne dans son README) ; la forme de crédit souhaitée.
- ~~Exe séparé ou dans le launcher ?~~ **Décidé (utilisateur, 2026-10-23) : un exe à part, `WWE12 Mod Maker.exe`,
  à côté de l'exe du jeu** (comme `SvR2011 Mod Maker.exe` chez Kaiko), livré dans le même zip ; le launcher garde
  l'installation et la gestion des mods. Plan validé par l'utilisateur le même jour.
- Pour l'utilisateur : le partage en ligne de mods (« Discovery » chez Kaiko) : oui / non / plus tard ?
- Le jeu original sans mods : garder un bouton « Jouer sans mods » (Kaiko a « Start Without Mods ») ?

## Écrans VS des arènes SvR (2026-10-28, notes de Kaiko reçues)

Kaiko a envoyé à l'utilisateur `VS_STYLES.md` et `tools/vs_port.py` (son SvR 2011 : thèmes VS de SvR 2008-2010 et
d'arènes mods ; gardés dans Downloads, hors dépôt ; crédit à mettre si on adapte). Comparaison faite hors jeu :
- **Même système dans WWE '12** : thème = mise en page YXML + images (`0x1B59`, DXT5) + animations ATB (`0x1B5A`) + police.
  SvR 2011 : `MENU/MnnL` (YXML **VER0002**, PACH enfant 0) + `MENU/MnnI` (0x1B59, 0x1B5A) + `MENU/MnnF`. WWE '12 :
  `MLYE/MBnn` = **un seul** PACH (0x1B58 YXML **VER0003**, 0x1B59, 0x1B5A) + `MENU/MnnF` (police, même forme).
- Mêmes 7 groupes (racine, 4 groupes de texte, `beta`, fond), mêmes ressources (`RAW_fr`, `RAW_bg`, `font000`,
  `nearbg01-05`, `farbg`) ; WWE '12 ajoute `matchup_BG01/02` et `dummy` (type 3 : `_paintHD/SD`, `_Normal`, `_SDSD`,
  `_SDHD`). Nœud : **0x90 o en VER0002, 0x84 en VER0003** (même début : id de ressource, x, y, échelles, largeur,
  hauteur ; noms raccourcis en VER0003 : « font000 » + « _lname » au lieu de « font000_lname »).
- → Un vrai écran VS de SvR 2011 (et 2008-2010 via le `reshape` de Kaiko) pour nos arènes portées de SvR : convertir le
  YXML VER0002 → VER0003, fusionner L + I en un MBnn, reprendre les nœuds propres à WWE '12 (`dummy`, `matchup_BG`) du
  thème WWE '12 de la base, garder la police. Pas fait.
- **2026-10-29 : vrai écran VS de SvR 2011 VU EN JEU** (`port_svr_arena::SvrCard`, case cochée par défaut dans l'assistant,
  étape « Ce qui est repris ») : Breaking Point (base Fatal 4-Way) avec son fond, son cadre, son logo, ses animations
  (ATB0 version 1 de SvR lue telle quelle par WWE '12) et sa police. **Piège** : si l'arène de base est verrouillée dans
  la sauvegarde du joueur, le jeu cherche le thème « /MLYE/MB-1 » (absent) et montre celui de SmackDown (profil de test
  neuf, 2026-10-28). Royal Rumble : son deuxième cadre `rr_fr02` n'a pas de place (laissé).
- **2026-10-29 : entrées dans les arènes de SvR 2011, corrigées et VUES EN JEU (Breaking Point)** :
  - les **projecteurs mobiles** (objets dont la ligne de 50001 porte un mouvement `m(N)` : appareils `bp_lig*` et
    faisceaux `bp_glow_mo*` de 10 m, plus 167-182) sont retirés : leurs mouvements les posaient sur le ring (appareils
    sombres sur le tapis et le tablier, bouts de faisceaux bleus) ;
  - les surfaces **`yReflect`** de SvR deviennent `yDefault` (sol de la rampe à moitié transparent : reflet à 0,5) ;
  - les pièces de sol **« en match seulement »** (`g` dans 50001, entre le sol et la hauteur de la scène : `sd_tile_core`,
    `sd_tile0`) restent visibles pendant les entrées (SvR y mettait son propre plancher d'entrée : trou dans la rampe).
  - Reste : **plaques de nom et locator** de SvR (`ETEX/00NN` : même format, mais WWE '12 a deux versions de chaque
    `AG2D` et compte les positions autrement : « copylight » à (106, 64) contre (500, 308) chez SvR) ; ceux de la base.

- **Arènes SvR 2011, suite (2026-10-29, vérifié en jeu sur Breaking Point)** : ses **plaques d'entrée et son locator**
  sont pris (`port_svr_arena::SvrEtex`) : les images clés de ses animations 2D (AG2D) font 96 octets (ou 44 sans le
  drapeau 0x1000000), celles de WWE '12 108 (3 mots de plus avant les coordonnées de texture : 0, 0, 1.0) ; toutes
  passent au format de WWE '12. Les **boîtes de visibilité (50020)** de SvR sont en mètres : ×100, pas ×10 (sinon
  tribunes, scène, rampe et sol disparaissaient selon la caméra pendant les entrées).

## Ajouts du 2026-10-30 (d'après le tableau comparatif avec le Mod Maker de Kaiko)

Hors jeu, sauf la vérification de « Jouer sans mods » notée plus bas.
- **Lanceur, « Jouer sans mods »** (page JOUER, sous le grand bouton) : lance le jeu avec `--wwe12_no_mods=true`
  (cvar de `main.cpp`, lue dans `wwe12_app.h` : coupe `wwe12_texture_packs`, `wwe12_superstar_mods`,
  `wwe12_entrance_mods`, ne monte ni le dossier `mods` ni les arènes ajoutées ; le QUITTER du menu reste). Rien n'est
  supprimé. **Limite** : le pack des catcheurs ajoutés déjà fait reste dans les DLC (le jeu le lit comme un DLC) : les
  catcheurs ajoutés restent donc, figés tels qu'au dernier lancement normal.
  **Vérifié une fois (2026-10-30, données de test, sans le lanceur : `--wwe12_no_mods=true`)** : écran-titre d'origine,
  journal sans aucune ligne « Mods : les fichiers… », « [superstar mods] » ni « Textures : » (12 au lancement d'avant).
- **Lanceur, « Signaler un problème »** (page À PROPOS) : un zip sur le Bureau (`WWE12-PC-report-AAAAMMJJ-HHMM.zip`) :
  `diagnostics.txt` (le texte de « Copier le diagnostic »), `mods.txt` (mods installés, activés ou non), les 3
  derniers `logs\wwe12_*.log` d'à côté de l'exe, `crash_trace.txt` / `hang_trace.txt` / `stderr.txt` de
  `Documents\wwe12\logs`. L'Explorateur s'ouvre sur le zip. Les journaux peuvent contenir le nom d'utilisateur Windows
  (chemins) : dit dans l'info-bulle.
- **Mod Maker, FICHIERS DU JEU : « Remplacer... »** (une entrée feuille) puis « Créer le mod et l'installer... »
  (archive du jeu : `.svrmod` `type=files`, `files/<chemin du jeu>`, nouveau type `files` → dossier `Files` dans le
  lanceur) ou « Enregistrer l'archive sous... ». **Écriture en place** (`pac_page::Browser::InPlace`) : le jeu trouve
  chaque entrée d'une archive par sa place (`plist360.arc`), donc chaque entrée de premier niveau garde son début et
  sa taille dans la table ; une entrée changée est écrite dans sa place (des zéros après) et ne doit être ni plus
  grosse ni (BPE) plus grosse une fois décompressée ; une entrée BPE est recompressée en gardant les blocs de Yuke's
  (`bpe_like::EncodeLike`). Archives XMemCompress (WWE '13 et après) refusées. Test hors jeu
  (`inplace_test`, scratchpad) : `menu.pac`, une image de 512 Ko de `MENU/ASPC` remplacée → fichier de même taille,
  1 entrée sur 15 changée, aucune déplacée, image relue identique ; une image deux fois plus grosse refusée.
  `MatchHD.pac` : la même image décalée (tout change) dépasse de 0,7 Ko sa place (entrées serrées à 256 octets) et
  est refusée : remplacer une image d'un paquet compressé ne marchera que si elle se compresse aussi bien. Plus tard :
  agrandir la place en corrigeant aussi `plist360.arc`, ou réduire d'autres images comme `Arena::Fit`.
- **Réglages du catcheur (piste n° 2 du tableau) : taille et capacités, faits.** Fiche `CHAR/DAT` de 290 octets
  (`chEtc.pac`). Le mod écrit des octets de sa fiche par `record=OFF:HEX,...` (offset en hexa dans la fiche, octets en
  hexa ; jamais 0x2C..0x103 : numéro, noms, personne), appliqué par `superstar_pack.h` après le reste.
  - **Taille = +0x28 et +0x2A**, deux échelles où 4096 = 1,0 (Rey 3449, Cena 3988, Big Show 4591, Khali 4764 ; chez
    certaines divas la 2e est plus grande, Trish 3513 / 4096). **Vu en jeu le 2026-10-30** : Mankind avec 4764 sur les
    deux dépasse Cena d'une tête. Rôle exact de la 2e non vérifié : le Mod Maker garde le rapport de la base.
  - **Capacités = +0x104**, jusqu'à 5 numéros puis 0. Numéro N = N-ième nom de la table du jeu à 0x8206AD30
    (`image.bin` 0x6AD30 : 0 → 40677 « NONE », N → texte 40616 + N, `pac/string.pac` `SDB /0000` enfant 0, entrées
    de **16 octets** {début, longueur, id, 0}). Utilisés par le jeu : 1 Dirty Pin, 7 Move Thief, 9 Hammer Throw,
    11 Resiliency, 14 Comeback, 19 Outside Dives, 20 Springboard Dives, 22 Leverage Pin, 24 Ring Escape (+ 23 Fired Up,
    qui a sa page d'aide). L'écran de choix (SUPERSTAR INFO) ne montre que prises de finition et attributs : effet en
    match pas encore vu.
  - Catégorie de poids : le jeu la lit sur un octet 1-5 (`sub_8292C538`, +235 d'une autre structure, textes 40678-40683),
    pas trouvée dans la fiche : laissée.
  - Mod Maker, NOUVEAU CATCHEUR, étape 3 : « Taille et capacités » (curseur + catcheur de même taille ; 10 cases, 5 au
    plus), gardés dans le projet (`own_size`, `size`, `own_abilities`, `abilities`).
- **Mode simple (piste n° 1)** : page ACCUEIL du Mod Maker, « PREMIERS PAS (mode simple) » ouvert par défaut : trois buts
  (catcheur d'un autre jeu, tenue retouchée, vidéo d'entrée), une étape numérotée par ligne avec « Ouvrir » (la page de
  l'étape) et [OK] quand le Mod Maker le voit (jeu trouvé, disque ajouté, au moins un mod installé). Vu dans le Mod Maker.
- **Couleurs du ring (piste n° 9, « kit du ring »)** : page ARÈNES, « Retoucher une arène », section « Couleurs du ring » :
  une couleur pour les cordes (`ar_rop01`), les protections des coins (`rin_tb00`, `rin_tb20`) ou les poteaux
  (`rin_pos00`, `rin_pos01`), mêmes noms dans toutes les arènes ; l'image du jeu est teintée (luminosité × couleur,
  transparence gardée), à sa taille et son format ; « Remettre » rend celle du jeu. **Vu en jeu le 2026-10-30** :
  SummerSlam avec cordes rouges et coins noirs (cordes blanches d'origine). Pas les cartes de normales (`_n`).
- **Taille des vidéos d'entrée (piste n° 15)** : page VIDÉOS, « Taille de la vidéo » 320 (comme le jeu) / 640 (HD) /
  1024 (HD+) : le fabricant de Kaiko les savait faire (même disposition), le choix manquait dans la page. **Vu en jeu le
  2026-10-30** : une vidéo 640×640 à la place de celle d'Orton (161) s'affiche sur le titantron de son entrée.
- **Pistes restantes : comment Kaiko les a faites dans SvR 2011** (lu dans `work/kaiko-svr2011`, résumé, pas de code
  repris ; à vérifier pour WWE '12 avant d'écrire quoi que ce soit) :
  - *Panoplies (Superstar Threads)* : liste `edit\edit.pac` EDIT/SSTL (fiches de 484 o : id, 48 noms de parties,
    caméra, couleur par défaut) ; partie p = canal (p-1)%4 du Mask((p-1)/4+1) (enfants 0x0D-0x0F de l'EMD) ; il sert
    un SSTL agrandi d'une fiche par mod (`threads.pac`, recherche `/EDIT/SSTL` redirigée). Testé chez lui, « expérimental ».
  - *Tenue d'entrée* : entrées EMD `NNNNNNA4` (tenue A, sorte 4) ; dans WWE '12 aussi (`00013904`, `00010304`...).
    Un mod qui fournit une `...04` marche tel quel chez lui. → pour nous : porter aussi la tenue d'entrée d'un catcheur
    de WWE '13 (même nom), le pack renomme déjà toutes les entrées.
  - *Matériaux* : paramètres de shader par maillage (`g_fSpecularLev`, `g_iSpecularPow`, `g_f4MatDifCol`), drapeaux de
    rendu u32 à maillage+0x88 (0x110 découpe alpha, 0x2 face cachée).
  - *Ajouter une tenue* : crochets du nombre de tenues et de leurs noms (`sub_828C4140`, `sub_828C4060` chez lui),
    fichier `ch<id>_x<N>.pac` en plus ; 4 au plus ; échoue chez lui pour les catcheurs à tenue d'entrée.
  - *CAW* : `.cas` de SvR 1 347 532 o (rendus DXT5 par tenue, sommes de contrôle), fiche par case dans `SaveData.dat`
    liée par la somme. Notre `.cas` WWE '12 fait 1 357 028 o : format à refaire.
  - *Ceintures / armes* : chez lui seulement des retextures ; *types de match* : pas lus par son jeu non plus.
- **Tenue d'entrée des catcheurs portés de WWE '13 (piste n° 5)** : `port13_star::AddEntranceOutfit` porte l'entrée
  « ...A4 » (même nom que la tenue avec la sorte 4, comme `00013904` dans WWE '12) sur une copie de la tenue de la base
  et l'ajoute au `ch.pac` du mod (le pack renomme déjà toutes les entrées au numéro du catcheur), réduite comme une
  tenue (`Arena::Fit` sur sa section). Case « Avec sa tenue d'entrée » (cochée par défaut) sous « Tenue à porter ».
  ~30 catcheurs de WWE '13 en ont une (Rock, Cena, Miz, R-Truth, Del Rio, Cody...) ; Mankind non. **Vu en jeu le
  2026-10-30** (programme de test `outfit_test` qui fait le même portage) : Miz de WWE '13 sur la base 218 entre avec son
  manteau, puis combat en tenue de ring.
- **Matières (piste n° 6)** : page CATCHEURS, section « Matières (brillance, teinte) » : par groupe de maillages du
  modèle du corps (enfant 0 de la tenue) : « Peau » (shaders `yCh_skin*`, sauf dents) et « Tenue : <partie> »
  (`yCh_gear`, par partie du modèle : boots, shorts, kneepad...). Brillance = × `g_fSpecLev` et `g_fSpecLvBase` ; teinte
  = × la couleur de `g_f4MatDifCol` (`arena_edit::Material`, écrit au Build). **Vu en jeu le 2026-10-30** :
  Orton à la peau teintée en vert et brillance ×3 (programme `mat_test`, même code). Une teinte sur une partie noire ne
  se voit pas (multiplication).
- **Plusieurs tenues pour un catcheur ajouté (piste n° 4, pour les portés de WWE '13)** : le nombre de tenues vient de
  la table **COS** de `misc.pac` (BATS/INIT et BATH/INIT, enfants 5 et 201 : « COS\0 », u32 nombre, puis lignes de
  40 o {u32 catcheur, u32 tenues, 4 × {u32 texte du nom, i32 déblocage (-1 : débloquée)}}, triées par numéro ; seulement
  les catcheurs du jeu, les DLC officiels ne la changent pas). Le pack (`superstar_pack.h`, étape 4c) y ajoute une ligne
  par catcheur ajouté dont le modèle a plusieurs tenues « 000NNNA2 » et sert sa copie de `misc.pac`. Noms : le jeu n'a pas
  de texte « TENUE 2 » ; pris ORIGINAL ATTIRE, Wrestling Gear, T-Shirt, Civilian Clothes (textes 9, 11, 13, 10).
  Mod Maker : case « Toutes ses tenues » (cochée par défaut) : les autres tenues de WWE '13 (4 au plus) portées sur des
  copies de la tenue de la base (`port13_star::AddPortedAttire`), chacune avec sa tenue d'entrée, chacune réduite à la
  place d'une tenue. **Vu en jeu le 2026-10-30** : Cena de WWE '13 (4 tenues) sur la base 261 : ADVANCED à l'écran de
  choix propose 4 tenues ; la 3e entre avec son t-shirt « Rise Above Hate » (sa tenue d'entrée).

## Panoplies, CAW, ceintures et armes (2026-10-30)

- **Panoplies (Superstar Threads) des catcheurs ajoutés** : la liste des panoplies est `EDIT/SSTL` (`edit\edit.pac` sur
  le disque ; chaque DLC en apporte une copie entière, `pac\edit\edit_dlcNN.pac`, qui remplace la précédente) : u16
  nombre, puis des fiches de 484 octets triées par numéro {u32 catcheur, 48 × u32 nom de partie (4 tenues × 12, -1 :
  aucune), zones de couleur, couleurs par défaut}. Notre pack ajoute `edit_dlc05.pac` (la table la plus récente + une
  fiche par catcheur ajouté, copiée de sa base) : `superstar_pack.h` étape 4d. **Vu en jeu par l'utilisateur
  (2026-10-30)** : Mankind (WWE '13) dans SUPERSTAR THREADS, avec les parties de sa base ; sa cravate rentre dans son
  corps (défaut du modèle porté, pas des panoplies). Les noms de parties de WWE '13 sont d'autres numéros de textes : pas repris.
- **Page CAW** (`caw_page.h`, d'après Kaiko) : liste les CAW de la sauvegarde, change l'image des listes (rendu
  256 × 512 DXT5 à 755 044 + 0x220 + tenue × 149 528, vignette 128 × 128 à +0x20414 ; décodage vérifié), exporte en
  `.wwe12caw` (zip : `caw.cas`, `record.bin`, `header.bin`), importe dans la première place libre. Sommes de contrôle :
  `.cas` +12, +0x14B254 et à la fin (la somme bouge de la différence des octets des images, méthode de Kaiko) ; fiche
  du CAW dans `SaveData.dat` à 0x1B9E8 + place × 0x6E4 (nom à +0x2A, copie de la somme du `.cas` à +0x684) ; somme de
  `SaveData.dat` (+0x18) = octets de 0x1C à 0x1C + les deux tailles en tête (vérifié). **Supposé, à vérifier en jeu** :
  taille d'une fiche (0x6E4) et 50 places (une seule fiche vue : un 2e CAW le confirmera). Copie de la sauvegarde
  avant chaque écriture (`SaveBackups`).
- **Page CEINTURES ET ARMES** (`objects_page.h`) : objets de `evtobj.pac` (SOBJ/NNNN, nommés par SOBJ/LIST : fiches de
  64 octets {u16 objet, nom}) dont 30 ceintures (dépliée / pliée / roulée), armes de `bgEtc.pac` (STG/WPON, STG3/WPON,
  STG3/WPO2, STGR/WPON). Une image remplacée devient un pack de textures (`textures/<nom>_<empreinte>.dds`) : l'empreinte
  du jeu (XXH3 du niveau 0 tel que le DDS de svrfmt le tient) est **la même** que celle des captures du jeu (vérifié sur
  les 26 captures du menu), donc pas besoin de capturer en jeu. Pas encore vu en jeu sur une ceinture.
- **Matières en lot** (2026-10-30, d'après Kaiko) : page CATCHEURS, section Matières, cases à cocher par partie,
  « Tout cocher » / « Tout décocher » : un réglage changé sur une partie cochée va à toutes les parties cochées.
- **Objets d'arène depuis un fichier 3D** (2026-10-30, d'après `model_objects.cpp` de Kaiko) : éditeur 3D des
  arènes, « Ajouter un objet 3D (OBJ, FBX)... » (`mesh_import.h` : OBJ en mètres, FBX par ufbx, y vers le haut tourné
  d'un demi-tour autour de x = l'espace du jeu en cm, y vers le bas) : une copie de l'objet choisi (`AddCopy` : sa ligne
  de drapeaux, ses nœuds) dont les maillages deviennent ceux du fichier (matière du 1er maillage de l'objet, un os
  fixe, couleur de sommet moyenne de l'objet, bandes jointes par triangles vides, 65 535 sommets par maillage), posée
  au centre de l'objet, sur son sol ; l'image du fichier (MTL map_Kd / matériau FBX) ajoutée au 1er paquet d'images
  de l'arène (puissance de 2, 1024 au plus, DXT5). « Exporter en OBJ... » écrit l'objet (OBJ + MTL + PNG). Vérifié
  hors jeu (un cube ajouté à bg01, relu : 12 triangles, son image) ; **pas encore vu en jeu**. Le volume des musiques,
  autre nouveauté de Kaiko, existait déjà chez nous (`xmp_music_level_db`).
- **Objet 3D ajouté, suite (2026-10-30)** : réglages avant l'import (taille, décalage X/Y/Z, rotation : `SetMove` dès
  l'ajout) ; contrôle de place juste après (le même `Fit` que « Créer le mod », images réduites si besoin, puis
  `Unreduce`) : si l'arène dépasse sa place, l'objet et son image sont retirés (`RemoveMeshObject`) avec un message
  (triangles, taille obtenue / place) ; sinon un message avec la taille. Vérifié hors jeu : 720 000 triangles refusés
  sur bg01 (17,8 Mo pour 10,2 Mo), images de l'arène identiques après retrait, 200 triangles acceptés. La vue 3D reste
  sur l'objet ajouté (et sur l'objet choisi avec « Voir cet objet ») jusqu'à « Recentrer ».
