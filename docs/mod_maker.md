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
  « mods »), comme Kaiko (case EXTRA dans SvR 2011).** Portraits des catcheurs 100+ : seulement dans
  `menu\menuHD.pac` / `menuSD.pac` (`SSFA` 0100…0343, 262 272 o), pas lus dans les DLC.
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

## 7. Questions ouvertes

- Pour Kaiko : ~~code C++~~ (réglé : `port/modmaker/` de son dépôt) ; ~~extension~~ (réglé : `.svrmod` +
  `game=wwe12`) ; un fichier LICENSE dans son dépôt (ou une ligne dans son README) ; la forme de crédit souhaitée.
- ~~Exe séparé ou dans le launcher ?~~ **Décidé (utilisateur, 2026-10-23) : un exe à part, `WWE12 Mod Maker.exe`,
  à côté de l'exe du jeu** (comme `SvR2011 Mod Maker.exe` chez Kaiko), livré dans le même zip ; le launcher garde
  l'installation et la gestion des mods. Plan validé par l'utilisateur le même jour.
- Pour l'utilisateur : le partage en ligne de mods (« Discovery » chez Kaiko) : oui / non / plus tard ?
- Le jeu original sans mods : garder un bouton « Jouer sans mods » (Kaiko a « Start Without Mods ») ?
