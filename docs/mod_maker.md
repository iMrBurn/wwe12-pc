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

## 7. Questions ouvertes

- Pour Kaiko : ~~code C++~~ (réglé : `port/modmaker/` de son dépôt) ; ~~extension~~ (réglé : `.svrmod` +
  `game=wwe12`) ; un fichier LICENSE dans son dépôt (ou une ligne dans son README) ; la forme de crédit souhaitée.
- ~~Exe séparé ou dans le launcher ?~~ **Décidé (utilisateur, 2026-10-23) : un exe à part, `WWE12 Mod Maker.exe`,
  à côté de l'exe du jeu** (comme `SvR2011 Mod Maker.exe` chez Kaiko), livré dans le même zip ; le launcher garde
  l'installation et la gestion des mods. Plan validé par l'utilisateur le même jour.
- Pour l'utilisateur : le partage en ligne de mods (« Discovery » chez Kaiko) : oui / non / plus tard ?
- Le jeu original sans mods : garder un bouton « Jouer sans mods » (Kaiko a « Start Without Mods ») ?
