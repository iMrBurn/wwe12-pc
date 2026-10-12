# WWE '12 PC : consignes pour Claude

Portage PC natif de WWE '12 (Xbox 360) : recompilation statique avec ReXGlue (`recomp/rexglue-sdk`)
et rendu Vulkan natif repris des portages SvR de nefariousjosiah (re:Blue + plume, `port/src/reblue`,
`port/src/native`). Dépôt public : https://github.com/iMrBurn/wwe12-pc (licence GPL-3.0).

L'utilisateur (iMrBurn) est francophone et débutant : réponses en français, simples, sans jargon
inutile ; utiliser « iel/they » si un pronom est nécessaire. Les textes du jeu, du launcher et des
releases sont en anglais (plus FR/DE/ES/IT pour les readmes de `port/dist`).

## Règles absolues

- **Jamais de données du jeu sur GitHub ni dans un zip** : xex/xexp, iso, .pac, .bik, sons, textures,
  `port/assets/`, `port/generated/`, `image.bin`, `work/`, dumps, Title Update, DLC. Pas de lien de
  téléchargement de DLC ou du jeu non plus (risque de DMCA sur tout le dépôt). L'exe (code recompilé)
  est accepté.
- **Rien n'est poussé sur GitHub sans le « go » explicite de l'utilisateur.** Les releases sont créées
  par l'utilisateur sur le site (`gh` n'est pas installé).
- **Toujours prévenir avant de lancer le jeu**, et ne pas le lancer pendant que l'utilisateur regarde
  autre chose sans son « vas-y ». Un seul jeu lancé à la fois.
- **Demander avant tout téléchargement.** Ne jamais toucher aux réglages de sécurité de Windows.
- Intégrer du code d'un tiers (fork, PR) seulement après relecture et accord de l'utilisateur.
- Identité git locale au dépôt : iMrBurn / 267004884+iMrBurn@users.noreply.github.com.

## Travail à plusieurs conversations

Chaque conversation a un sujet : **(Main)** coordonne, versions, GitHub, retours des testeurs ;
**(Rendu)** bugs graphiques ; **(Title Update)** ; **(Launcher)** ; **(Mods)**. Rester dans son sujet ;
noter ce qui concerne les autres pour la Main.

Les worktrees git ne marchent PAS bien ici : `recomp/`, `port/generated/`, `port/assets/` et `work/`
ne sont pas versionnés (énormes ou données du jeu). Toutes les conversations travaillent donc dans ce
dossier ; **une seule compile ou lance le jeu à la fois**, et on ne modifie pas un fichier qu'une
autre conversation est en train de modifier. Commits locaux par sujet, la Main pousse.

Ce qu'on découvre d'important sur le moteur va dans la section « Savoir acquis » ci-dessous.

## Compiler, tester, empaqueter

- Toujours compiler via `build.ps1` (il charge `tools/env.ps1`) :
  `powershell -ExecutionPolicy Bypass -File build.ps1 -Config release` (ou `relwithdebinfo`).
  `-Codegen` relance la traduction PPC → C++ (`port/wwe12_manifest.toml`, `port/wwe12_config.toml`).
- Ne jamais lancer un `cmake -S . -B` nu : il met en cache des CMAKE_CXX_FLAGS vides (build non
  optimisé). Réparer avec `cmake --preset X -U "CMAKE_CXX_FLAGS*"` après `. tools\env.ps1`.
- Le SDK est modifié par les patchs de `port/patches/`. Après un changement du SDK :
  `cmake --build out/build/win-amd64 --config Release --target install` (et `RelWithDebInfo`) dans
  `recomp/rexglue-sdk`, puis recopier `rexruntimerd.dll` à côté de l'exe RelWithDebInfo. Régénérer le
  patch correspondant.
- Fermer le jeu et le launcher avant de recompiler (exe verrouillé).
- Lanceurs de test : `tools/dev/lancer_*.bat` (diagnostic, dump de textures, 720p, iGPU Intel…).
  Journaux et traces de plantage : `Documents\wwe12\logs`. F9 en jeu = log détaillé d'une image.
- RenderDoc 1.46 portable : `C:\Users\melfo\Documents\WWE12-PC\renderdoc`, couche Vulkan par variables
  d'environnement (VK_ADD_LAYER_PATH, VK_INSTANCE_LAYERS=VK_LAYER_RENDERDOC_Capture,
  ENABLE_VULKAN_RENDERDOC_CAPTURE=1), captures dans `work/rdc`, scripts via `qrenderdoc.exe --python`.
- Release : changer `VERSION`, compiler en release, `tools/package.ps1` → `dist/WWE12-PC-v<ver>.zip`,
  écrire `dist/RELEASE_NOTES_v<ver>.md`. Le tag GitHub doit être `v<VERSION>` : le launcher s'en sert
  pour les mises à jour automatiques.

## Où est quoi

- `port/src/wwe12_app.h` : démarrage (overlay mods, menu patché, DLC, partition cache).
- `port/src/native/wwe12_fps.cpp` : 60 fps en entrées (cvar `wwe12_60fps`, défaut false).
- `port/src/native/wwe12_menu_patch.cpp`, `wwe12_quit.cpp` : SHOP → EXIT, Xbox LIVE → ONLINE.
- `port/src/native/svr_resources.cpp` : cibles de rendu, read-backs vers la mémoire du jeu.
- `port/src/wwe12_dlc.cpp` : installation des DLC du dossier `dlc` (contribution de Banshee).
- `port/src/launcher/` : « WWE12 Launcher.exe » (ImGui/D3D11, mise à jour GitHub dans `updater.h`).
- `port/config/wwe12.toml` : réglages par défaut livrés. `port/dist/` : readmes et licences livrés.

## Savoir acquis (à compléter)

- Hooks : `extern "C" REX_FUNC(sub_X)` + `REX_EXTERN(__imp__sub_X)`. Cvars : REXCVAR_DEFINE_*.
  En ligne de commande les booléens s'écrivent `--nom=false`.
- **Ne jamais soumettre/rouvrir la command list au milieu d'une image** (cause des saccades et des
  yeux/dents noirs) : les attentes GPU vont en fin d'image (read-backs différés).
- Read-backs : cibles 1 niveau immédiates (limitées par série), multi-niveaux différées en fin
  d'image avec mips refaits sur CPU, cibles 16:9 jamais. Texels écrits en ordre BGRA.
- Fréquence d'images : struct 0x8324CE28 ; SetFrameRate sub_82827918 (r3 = fps), passage à 30
  sub_82827AB8, retour à 60 sub_82827B58.
- Menu : menu.pac patché au démarrage (label 41035 SHOP → 45061 EXIT) servi par l'overlay VFS
  (`Documents\wwe12\patched`) ; hook « decide » sub_8253C3F0. La vraie 8e entrée a échoué (une
  insertion casse la navigation B).
- VFS overlay (patch SDK) : fichiers seulement, ouvertures en lecture ; dossier `mods` à côté de l'exe.
- Title Update (pour la 1.1) : le codegen ne lit pas les xexp ; il faudra produire un xex patché hors
  ligne, refaire le codegen et retrouver toutes les adresses codées en dur.
- Bugs ouverts : ceinture sur l'épaule à 60 fps (piste VBlank testée le 2026-10-12 et écartée : le
  minuteur est déjà régulier à 60 Hz ; reste l'animation d'objet à 30 Hz, la ceinture de Cena décroche
  de la main quand il court ; mise de côté le 2026-10-12 : ni le VBlank, ni le pas de la physique, ni la
  struct 0x8324CE28 seule n'expliquent le défaut ; piste restante : minuteurs 8246A020/824999F8 + une
  horloge à pas fixe non trouvée ; outils de test dans `#if WWE12_DEV_PROBES`, voir la mémoire) ;
  AMD/Steam Deck (#4 gel en entrée sur RX 560, #7 bloc en haut à droite) en attente de retours 0.5.0 ;
  #7 persiste en 0.5.0 chez DRAVEN0612 sur **Intel UHD 620** (pilote Vulkan 1.3.215, vieux), donc pas
  qu'AMD ; son journal est dans `work/issue7/wwe12_014.log` (rien d'anormal visible : 60 fps, cible
  carrée 1120x1120 fmt 36 non relue, composite peau 1024x512 fmt 15 vidée) ;
  titantrons basse résolution (vidéos Bink 320x320 sur le disque).
- Menu OPTIONS > GRAPHICS (`wwe12_graphics_menu.cpp`) : l'entrée du manuel (texte 41184) renommée
  en mémoire ; écran ImGui au style des pages d'options (police Impact penchée), entrées du jeu
  bloquées pendant qu'il est ouvert. Page 100 % native = projet à part : les pages d'options sont
  des listes de numéros de textes en mémoire, et `wwe12_graphics_link_test=1304` ouvre déjà la vraie
  page IN-GAME OPTIONS depuis GRAPHICS (enregistrement menu.pac : +0x54 type 5, +0x6C lien).
- Voitures des entrées (Del Rio, Eddie Guerrero) : le jeu dessine la carrosserie en mélange alpha avec
  l'alpha d'une texture BC3 (~0,63), donc translucide ; la Xbox l'affiche opaque (cause réelle inconnue).
  Contournement : cvar `wwe12_car_opaque` (défaut true, `pipeline_cache.cpp`) coupe le mélange pour le
  pixel shader 7EE1904EE7E52F05 (vitres opaques aussi ; ce shader sert peut-être aux ceintures).

## Idées et demandes des joueurs (à faire)

- **Option « tout débloquer »** (comme le portage SvR 2011) : un joueur signale que le DLC « All
  Unlockables » ne débloque que les persos des DLC, pas le reste. Piste : cvar `wwe12_unlock_all`
  (case dans le launcher) qui force les drapeaux de déblocage, sans toucher aux sauvegardes du joueur.
  Il faut d'abord trouver dans le code où le jeu teste si un élément est débloqué.
