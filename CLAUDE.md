# WWE '12 PC : consignes pour Claude

Portage PC natif de WWE '12 (Xbox 360) : recompilation statique avec ReXGlue (`recomp/rexglue-sdk`)
et rendu natif repris des portages SvR de nefariousjosiah (re:Blue + plume, `port/src/reblue`,
`port/src/native`) : trois exe, `wwe12.exe` (Vulkan), `wwe12_d3d12.exe` (DirectX 12) et `wwe12_d3d11.exe`
(DirectX 11, adapté du portage SvR 2011 de Kaiko, avec crédit). Dépôt public : https://github.com/iMrBurn/wwe12-pc (licence GPL-3.0).

L'utilisateur (iMrBurn) est francophone et débutant : réponses en français, simples, sans jargon
inutile ; utiliser « iel/they » si un pronom est nécessaire. Les textes du jeu, du launcher et des
releases sont en anglais (plus FR/DE/ES/IT pour les readmes de `port/dist`).

## Règles absolues

- **Jamais de données du jeu sur GitHub ni dans un zip** : xex/xexp, iso, .pac, .bik, sons, textures,
  `port/assets/`, `port/generated/`, `image.bin`, `work/`, dumps, Title Update, DLC. Pas de lien de
  téléchargement de DLC ou du jeu non plus (risque de DMCA sur tout le dépôt). L'exe (code recompilé)
  est accepté. **Seules exceptions, décidées et assumées par l'utilisateur** : l'icône de l'exe (logo du
  jeu) et les icônes de boutons de la version PS3 (`port/res/ps3_buttons.bin`, dans le dépôt et dans
  l'exe, décision du 2026-10-17 en connaissant le risque DMCA). Aucune autre donnée sans son accord
  explicite.
- **Rien n'est poussé sur GitHub sans le « go » explicite de l'utilisateur.** Les releases sont créées
  par l'utilisateur sur le site ; `gh` est installé et sert à lire / répondre aux issues (réponses simples
déléguées à la Main par l'utilisateur), jamais à publier une release sans son go.
- **Toujours prévenir avant de lancer le jeu**, et ne pas le lancer pendant que l'utilisateur regarde
  autre chose sans son « vas-y ». Un seul jeu lancé à la fois.
- **Demander avant tout téléchargement.** Ne jamais toucher aux réglages de sécurité de Windows.
- Intégrer du code d'un tiers (fork, PR) seulement après relecture et accord de l'utilisateur.
- Identité git locale au dépôt : iMrBurn / 267004884+iMrBurn@users.noreply.github.com.

## Travail à plusieurs conversations

Chaque conversation a un sujet : **(Main)** coordonne, versions, GitHub, Discord, retours des testeurs ;
**(Rendu)** graphismes, performances, Steam Deck ; **(Gameplay)** code du jeu, mises à jour de THQ, natif ;
**(Univers)** mode Univers et jukebox ; **(Mods)** Mod Maker ; **(Launcher)** launcher PC ;
**(Android)** version Android ; (Limit breaking, iOS : en pause). Rester dans son sujet ;
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
  ENABLE_VULKAN_RENDERDOC_CAPTURE=1), captures dans `work/rdc` (dossier créé par les lanceurs d'autocapture ; les anciennes captures ont été supprimées le
  2026-11-06, scripts conservés dans `work/scripts_rdc`), scripts via `qrenderdoc.exe --python`.
- Release : changer `VERSION`, compiler en release, `tools/package.ps1` → `dist/WWE12-PC-v<ver>.zip`,
  écrire `dist/RELEASE_NOTES_v<ver>.md`. `dist/symbols-v<ver>/` garde `wwe12.map` + `wwe12.exe` de la version (pour traduire les
  hang_trace/crash_trace des joueurs ; la 0.7.0 n'a pas sa carte). Coller à la fin des notes le contenu de `dist/SHA256-v<ver>.txt` (empreintes du zip
  et des exe, non signés : Windows Defender les a déjà signalés à tort en 0.6.0 ; faux positif à
  soumettre à Microsoft par l'utilisateur ; signature de code à envisager pour la 1.0). Le tag GitHub doit être `v<VERSION>` : le launcher s'en sert
  pour les mises à jour automatiques. Mise à jour automatique vérifiée en vrai par l'utilisateur (0.5.0 → 0.5.1,
  2026-10-12) : elle garde `wwe12.toml`, `game/` et `mods/`.

## Où est quoi

- `port/src/wwe12_app.h` : démarrage (overlay mods, menu patché, DLC, partition cache).
- `port/src/native/wwe12_fps.cpp` : 60 fps en entrées (cvar `wwe12_60fps`, défaut false).
- `port/src/native/wwe12_menu_patch.cpp`, `wwe12_quit.cpp` : SHOP → EXIT, Xbox LIVE → ONLINE.
- `port/src/native/svr_resources.cpp` : cibles de rendu, read-backs vers la mémoire du jeu.
- `port/src/wwe12_dlc.cpp` : installation des DLC du dossier `dlc` (contribution de Banshee).
- `port/src/launcher/` : « WWE12 Launcher.exe » (ImGui/D3D11, mise à jour GitHub dans `updater.h`).
- `port/config/wwe12.toml` : réglages par défaut livrés. `port/dist/` : readmes et licences livrés.
- **Compiler sans redemander** (décision de l'utilisateur, 2026-11-11) : compiler quand un changement doit être testé, en prévenant Rendu avant et après ; une seule compile / un seul jeu à la fois ; ne jamais lancer le jeu sans prévenir ; pousser reste soumis au « go ».

## Savoir essentiel (le détail est dans `docs/savoir_acquis.md`)

- Hooks : `extern "C" REX_FUNC(sub_X)` + `REX_EXTERN(__imp__sub_X)` ; cvars : `REXCVAR_DEFINE_*` ; en ligne de commande `--nom=false`.
- **Ne jamais soumettre / rouvrir la command list au milieu d'une image** (saccades, yeux noirs) : les attentes GPU vont en fin d'image (read-backs différés).
- Fréquence d'images : struct 0x8324CE28 ; SetFrameRate sub_82827918 ; passage à 30 sub_82827AB8 ; retour à 60 sub_82827B58. 3 modes fixes (Xbox / 60 entrées+victoires / 30), jamais de bascule « intelligente » ; chaque scène demande son script /GMEV/%04d (sub_825B85B8) juste avant : entrée = n° du catcheur, RTWM 1010/1020/1810/1820, Create a Story 4000, victoire 6000+gagnant.
- Début de match réel : sub_822B51A0 (pas sub_821D5430). Boucle d'image : barrière sub_827DEA20.
- Menus : OPTIONS > GRAPHICS / JUKEBOX sont de vrais menus du jeu (scripts Lua compilés, `wwe12_graphics_menu.cpp`, `wwe12_jukebox.cpp`).
- Sauvegardes : profil fixe `B13EBABEBABEBABE`, paquets STFS sous `Documents\wwe12\B13EBABEBABEBABE\54510890\00000001\`.
- Title Update : voie B (nos propres crochets, `docs/title_update_corrections.md`) ; la régénération du code est dans « Plus tard », sans version.
- Trois exe (`wwe12.exe` Vulkan, `wwe12_d3d12.exe`, `wwe12_d3d11.exe`) ; aucune DLL Microsoft dans le zip (shaders prélinkés) ; à exclure du zip : dxcompiler.dll, dxil.dll, wwe12_prelink.exe, fxc, d3dcompiler.
- Mod Maker (`port/src/modmaker`) : détails dans `docs/mod_maker.md` ; prises ajoutées d'autres jeux coupées par défaut (`wwe12_added_moves`, 2026-11-11).
- Dossier de test unique : `Documents\wwe12_test` (lanceurs `tools/dev/lancer_*.bat`) ; le vrai jeu et sa sauvegarde sont dans `Documents\wwe12`, à ne pas toucher en test.

## Plans et feuilles de route

- Feuille de route actuelle : `docs/feuille_de_route.md` (1.1.0 publiée le 2026-11-07, 1.2.0 « Moveset update » publiée le 2026-11-10 (prises SvR / 2K dans le Mod Maker, page Moves & Entrances, gamma, 30 fps expérimental, corrections ; Android 1.2.0 avec), pas de 1.1.1 (décision du 2026-11-08),
  1.3 = corrections THQ par nos crochets + performances / code natif + benchmark + page ROSTERS du launcher (bloc-notes de l'utilisateur du 2026-11-07), 1.4 = modes de match + Univers ; décision du 2026-11-08). Idées de joueurs : `docs/suggestions_joueurs.md`.
- Anciens plans 0.8 / 0.9 / 1.0 et anciennes listes : `docs/archives/claude_md_anciens_plans_2026-11-06.md`.

