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

## 6 quinquies. Étape 3 : médias et images (plan, 2026-10-23, pas commencé)

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

## 7. Questions ouvertes

- Pour Kaiko : ~~code C++~~ (réglé : `port/modmaker/` de son dépôt) ; ~~extension~~ (réglé : `.svrmod` +
  `game=wwe12`) ; un fichier LICENSE dans son dépôt (ou une ligne dans son README) ; la forme de crédit souhaitée.
- ~~Exe séparé ou dans le launcher ?~~ **Décidé (utilisateur, 2026-10-23) : un exe à part, `WWE12 Mod Maker.exe`,
  à côté de l'exe du jeu** (comme `SvR2011 Mod Maker.exe` chez Kaiko), livré dans le même zip ; le launcher garde
  l'installation et la gestion des mods. Plan validé par l'utilisateur le même jour.
- Pour l'utilisateur : le partage en ligne de mods (« Discovery » chez Kaiko) : oui / non / plus tard ?
- Le jeu original sans mods : garder un bouton « Jouer sans mods » (Kaiko a « Start Without Mods ») ?
