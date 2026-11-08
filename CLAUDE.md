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
- Highlights / replay d'après-match : moments ajoutés par sub_821EE520 dans 20 cases de 20 octets
  à (*0x8317C84C)+128+6200 ; sub_821EDEE0 = « au moins un moment » (sinon MATCH HIGHLIGHTS grisé).
  Interrupteur 0x83174250, mis par sub_822B6D30 (place libre sur `cache:` + `Spectacular.rps`).
  Sonde dans `wwe12_game_log.cpp` (lignes « [replay] »).
- Title Update (décision 2026-11-05 : voie B, nos propres crochets en 1.3 (ex-1.2), plan `docs/plan_1_2_codegen_tu.md`, corrections THQ
  listées dans `docs/title_update_corrections.md` : TU1 = v0.0.1.4, TU2 = v0.0.2.4) : le SDK applique déjà un `default.xexp` posé
  à côté du xex (codegen compris). **Mesure** (image 0.0.2.4 vs `image.bin` 0.0.0.4, même taille 20 316 160 o) : 82 %
  des mots du code diffèrent ; sur 47 240 fonctions, 54 seulement restent à leur adresse, 46 834 (99,1 %) sont
  identiques mais DÉCALÉES (de ~1 à 12 Ko, décalage qui dérive), 352 (0,7 %) changées ou supprimées, +77 nouvelles.
  Adresses utilisées : 1 sur ~990 (config + port/src livré) valide telle quelle. Le décalage est mécanique (recalage
  par signature), mais tout reste à retester et le jeu de base sans TU ne marcherait plus.
- Bugs ouverts (2026-11-06) : voir les issues GitHub ouvertes et `docs/feuille_de_route.md`. Réglés depuis octobre : ceinture
  à 60 fps (échantillonnage de rotation aux images paires), blocs de couleur #7 (Intel / AMD), D3D12 transparent sur Intel
  UHD 620 (viewport inversé remis dans l'ordre), tenues Threads blanches, peau GT 430, gels d'arènes + catcheurs ajoutés.
  Reste : titantrons basse résolution (vidéos Bink 320x320 sur le disque), aucune issue Steam Deck ouverte.
- Menu OPTIONS > GRAPHICS (`wwe12_graphics_menu.cpp`) : **vrai menu du jeu** (2026-10-20). Le sous-menu du
  manuel (TUTORIAL/ABILITIES/ATTRIBUTES) devient DISPLAY/GRAPHICS/ADVANCED et ouvre de vraies pages d'options
  du jeu (scripts Lua 5.1 compilés dans MScript.pac /MENS/0014, nos 3 pages compilées à l'avance par
  `tools/graphics_pages.py` avec un luac 5.1.5 hors dépôt, intégrées à l'exe dans
  `wwe12_graphics_pages.inc`). 13 réglages, mêmes cvars/clés que le launcher, enregistrés dans wwe12.toml
  à la fermeture de la page. L'ancien écran ImGui est retiré ; `wwe12_graphics_menu = false` rend le
  manuel d'origine. Détails : `docs/menus_natifs.md`. Le patch SDK `rexglue-sdk-menu-input.patch` (retiré le 2026-10-22)
  (GetStateIgnoringActive) ne sert plus au menu ; `rexglue-sdk-pads-list.patch` sert aux icônes PS.
- Voitures des entrées (Del Rio, Eddie Guerrero) : le jeu dessine la carrosserie en mélange alpha avec
  l'alpha d'une texture BC3 (~0,63), donc translucide ; la Xbox l'affiche opaque (cause réelle inconnue).
  Contournement : cvar `wwe12_car_opaque` (défaut true, `pipeline_cache.cpp`) coupe le mélange pour le
  pixel shader 7EE1904EE7E52F05 (vitres opaques aussi ; ce shader sert peut-être aux ceintures).
- Sauvegardes (2026-10-16) : le profil local est fixe (`B13EBABEBABEBABE`, user_profile.cpp). Une
  « sauvegarde » = un dossier de paquets STFS de type 1 (jeu 54510890) sous
  `Documents\wwe12\B13EBABEBABEBABE\54510890\00000001\<nom du paquet>\` + `Headers\00000001\<nom>.header` :
  `SaveData.dat` (partie principale) et, sur la Xbox, `NNEncodeDat.enc` (entrées, titantrons...) et
  `NNCreateSuperStar.cas` (catcheurs créés), jusqu'à ~95 paquets. Succès : `achievements\54510890.toml`.
  Les DLC sont à part (`0000000000000000\54510890\00000002`). Page SAUVEGARDES du launcher (`saves.h`) :
  copies (sauvegarde + succès seulement), restaurer/supprimer, repartir de zéro, copie automatique avant
  tout changement (10 gardées). **Import Xbox 360** : le launcher vérifie les paquets (type à 0x344, jeu à
  0x360, en gros-boutiste) puis écrit `saves_import.txt` à côté de l'exe ; au démarrage
  `wwe12_saves.cpp` (appelé après les DLC dans `wwe12_app.h`) extrait chaque paquet (même nom = remplacé),
  supprime la liste et écrit `saves_import_result.txt`. Signatures non vérifiées (pas de re-signature).
  Testé en vrai avec « A Promoter's Dream » de Petchy (95 paquets, CAW visibles dans le jeu).
- **Choix du rendu (1.0, mis à jour le 2026-11-01)** : TROIS exe : `wwe12.exe` (Vulkan, défaut), `wwe12_d3d12.exe`
  (cible CMake `WWE12_D3D12_EXE`) et `wwe12_d3d11.exe` (cible `WWE12_D3D11_EXE`, ON pour la release ; backend D3D11 de
  plume repris de Kaiko avec sa permission et crédit « SvR 2011 PC port (KaikoClanworth1) », voir `docs/dx11.md`) ;
  ils réutilisent `wwe12_recomp`, une carte `.map` chacun. Le launcher lance l'un ou l'autre selon `gpu_backend` de
  `wwe12.toml` (`any` = wwe12.exe qui enchaîne Vulkan → D3D12 → D3D11 si le device échoue, `vulkan`, `d3d12`,
  `d3d11`) ; OPTIONS > GRAPHICS > ADVANCED a la ligne « GRAPHICS API » à 4 valeurs (« (NOT INSTALLED) » si l'exe
  manque). **Aucune DLL Microsoft n'est livrée** : depuis le 26/10 (commit 71bd3f3) l'outil `wwe12_prelink` lie
  à la compilation les 1 996 variantes de shaders et les intègre à `wwe12_d3d12.exe` (`linked_shader_cache`, miniz) ;
  `dxcompiler.dll` n'est chargé en différé que s'il est présent (dossier de dev) ; le D3D11 embarque ses shaders
  DXBC. À EXCLURE du zip : `dxcompiler.dll`, `dxil.dll`, `wwe12_prelink.exe` (outil de compilation), fxc,
  d3dcompiler. **Pièges D3D12** : 1) garder la profondeur inversée (MinZ > MaxZ), sinon catcheurs et foule
  disparaissent ; 2) le choix de carte se fait par fabricant (noms DXGI ≠ noms Vulkan) ; 3) correctifs plume dans
  `port/patches/plume-d3d12.patch` (read-backs, choix de carte) et `plume-d3d11.patch`, appliqués par `setup.ps1` ;
  4) les fichiers de re:Blue (`shader_linker`, `dxc_link`, BSD-3, commit 85aae68) sont crédités dans
  `THIRD_PARTY.md`. D3D11 : reflets du soleil non dessinés. La chaîne AUTO sur un vrai échec de Vulkan a été vue
  (Vulkan → D3D12) ; le dernier pas (D3D12 → D3D11) n'est pas testé. Idée plus tard : fusion en un seul exe avec
  choix à l'exécution (comme Kaiko : `gpu.h` + `backend_*.cpp`) ; `g_vulkan` est constexpr à ~50 endroits.
- Clavier (2026-10-16) : le jeu n'accepte que des noms de touches exacts et sensibles à la casse (table de
  `rexglue-sdk/src/ui/keybinds.cpp`, recopiée dans `launcher/keybinds.h`) ; un nom inconnu ou un modificateur
  seul (`Shift`) est ignoré sans message. Le launcher corrige/valide les noms et propose « Définir ».
  **Clavier = joueur 2** : cvar `mnk_player` (1 = partage le joueur 1 avec la manette 1, défaut ; 2-4 = manette
  séparée), patch SDK `port/patches/rexglue-sdk-keyboard-player.patch` (`DeviceInfo::slot`, `SlotAssignment`,
  `MnkInputDriver`). Testé en vrai : Xbox One S = joueur 1, clavier = joueur 2.
  **Shift / Ctrl / Alt seuls sur un bouton (issue #18, corrigé pour la 1.0 le 2026-10-26, vérifié en jeu : Shift = A,
  Ctrl = B)** : le pilote mnk exigeait le masque exact des modificateurs (« Shift » seul attend 0, mais tenir Shift met
  le bit). Patch SDK `rexglue-sdk-mnk-modifiers.patch` : un modificateur lié seul (`Shift`, `Ctrl`/`Control`, `Alt`)
  devient une touche normale et sort du masque de toutes les liaisons (les « Shift+… » ne marchent plus alors) ;
  gauche/droite toujours non distinguées (`sdl_virtual_key.cpp`). Launcher (`keybinds.h`) : les accepte, les capture
  (modificateur appuyé puis relâché seul), avertit des combinaisons alors mortes ; noms valides dans la FAQ. Capture
  du launcher pas encore essayée à la main. Lanceur `tools/dev/lancer_test_touches.bat`.
  **Profil et joueurs (2026-10-18, issue #17)** : le runtime n'a qu'un profil (user 0, XUID fixe
  B13EBABEBABEBABE) ; avant, START pressé depuis un autre slot (clavier en joueur 2, 2e manette, appareil
  arrivé en slot 1) donnait « You are not currently signed in with a gamer profile… unable to save ».
  Reproduit (manette + clavier `mnk_player=2`, Entrée à l'écran-titre) puis corrigé : patch SDK
  `port/patches/rexglue-sdk-signin-slot.patch` (`ProfileSlot()` dans `xam_user.cpp` : un slot qui a un
  appareil connecté est connecté avec le même profil pour les `XamUser*`). Les sauvegardes
  (`xeXamContentCreate`) utilisaient déjà le profil unique quel que soit le slot. Prévu pour la 0.9.
- **Saisie de texte au clavier (2026-10-19, `wwe12_kbd.cpp`)** : le jeu n'utilise pas XamShowKeyboardUI
  pour Create a Story mais **sa propre grille de lettres**. Les lignes de la grille sont des widgets
  `_write21`, `_write22`… (nom à +0x60), dessinés chaque image par `sub_824C9228(écran r3, widget r4)`.
  Leur texte (`[widget+200]`) n'est qu'une **copie d'affichage** : y écrire montre les lettres mais le
  compteur LEFT ne bouge pas, et la validation reprend le vrai texte (case vide). Le vrai texte est dans
  l'objet « éditeur » de la grille, qui est à **écran − 0x740** (vu : éditeur A3DEEE50, écran A3DEF590 ;
  vérifs : `[éditeur+44]` ≠ 0, ligne courante `[éditeur+3676]` < 8). Fonctions de la grille :
  `sub_828A47D0(éditeur, const char* s, 1, 0)` insère `s` (≤ 3 caractères) au curseur (les cases lettres
  via `sub_828A7A98`, SPACE via `sub_828A62F0` avec " " ; gère la limite de 84, la ligne suivante et le
  mode SHIFT de la grille) ; `sub_828A6158(éditeur)` = BACK. Le module rejoue les touches (OnKeyChar de la
  fenêtre : AZERTY, Maj, répétition ; accents → lettre simple) par ces fonctions, sur le fil du jeu, dans le
  crochet ; pendant la grille, les touches de frappe ne vont plus à `mnk_mode` (écouteur de fenêtre à z
  1000, `set_handled`). Trouvé avec un piège PAGE_GUARD sur la page de la ligne (RIP → `llvm-symbolizer`
  avec `wwe12.pdb`) : la ligne est refaite par memset `sub_82A76590` + memcpy `sub_82A76060` appelés depuis
  `sub_828A47D0`. Fausses pistes : `[[écran+268]+8]` (0 ici), `sub_82BFD248` (jamais appelé), auto-pointeur
  `[texte−0x50+0x30]` (vrai seulement sur une ligne neuve). Sonde : `wwe12_kbd_log` ; lanceur
  `tools/dev/lancer_test_clavier.bat`. Le module de départ d'OpenCode (commits 36099f4/9c21077,
  `docs/wwe12_text_entry_analysis.md`) écrivait dans la copie d'affichage.
- **Sang des catcheuses (2026-10-21/22, reporté à la 1.0, `wwe12_blood.cpp`)** : l'option BLOOD est l'octet
  **0x8322660D** = bloc de réglages `sub_82684BE8()` (= 0x831CEEA8) + 0x57720 + 69 (1 = activé), écrit par
  UpdateInGameOptions (`sub_82DEC528`, ligne 3 de sa première page). Seul le MATCH CREATOR le lit
  (`sub_825910F8`, `sub_82591750` : First Blood grisé si BLOOD = 0). `sub_82783808` (mise en place de match d'un
  autre mode, pas l'exhibition) le recopie dans l'entrée de règles du match (52 octets, `sub_827829B0` =
  [gest+556] + (type × 10 + sous-type) × 52) : +32 = « pas de sang », sauf si [gest+647] force le sang.
  **Pendant un match First Blood où un catcheur a saigné, l'octet n'est jamais lu** : le match travaille sur une
  copie, et le test homme/femme est dans le code du saignement, pas encore trouvé. Piste suivante : repérer
  l'octet « saigne » d'un catcheur en match, puis un piège d'écriture dessus. Attention : `sub_8267CF38` renvoie
  un objet global (0x8317C850), pas une fiche de catcheur. Sonde `wwe12_blood_trap` (piège PAGE_GUARD sur
  l'octet, ralentit le jeu), lanceur `tools/dev/lancer_test_sang_piege.bat`. Astuce de l'utilisateur pour faire
  saigner vite : Extreme Rules + MATCH CREATOR > WIN CONDITION > FIRST BLOOD = ON. **Plantage vu 3 fois (même
  sans la sonde) au chargement qui suit la fin d'un match First Blood** : lecture de guest 0 dans
  `sub_82488DE0` (lr 82488DE8), build du 2026-10-22 avec des changements de Rendu non commités ; confié à Rendu.
- **Jukebox (1.0, 2026-10-26, `wwe12_jukebox.cpp`)** : OPTIONS > CREDITS devient JUKEBOX (`wwe12_jukebox_menu`,
  choix de l'utilisateur), page d'options native (4 lignes : JUKEBOX, ORDRE, THÈME, MUSIQUE DES MENUS DU JEU AUSSI ;
  `wwe12_jukebox` désactivé par défaut). Numéro d'un thème d'entrée = numéro du catcheur (Cena 139) ; nom pour le
  moteur de son `sub_8271F080` = « @Entrance_Music-MUS_%04d_0_0 ». La musique des menus est un conteneur du moteur
  (« @System-Menu_Music ») qui choisit seul : sa liste de 19 numéros (0x83074908) ne décide rien. Lecture d'un thème
  comme l'écoute de Créer une entrée (`sub_829EBBE8`) : couper les pistes 0-2 du gestionnaire de musique
  (`sub_8271BB60`), charger sur la piste 2 (`sub_8271BC90`), attendre prêt (`sub_8271BF58`), jouer (`sub_8271BD28`).
  Vérifié en jeu : thème choisi, arrêt au match, reprise au menu. Pas encore vérifiés en jeu : thème d'un catcheur DLC
  (82 entrées listées) et le tour « musique des menus du jeu ». Lanceur `tools/dev/lancer_test_jukebox.bat` (données
  isolées `Documents\wwe12_test`).
- **Interrupteur « jeu d'origine » (2026-10-22, reporté à la 1.0 : PAS dans l'arbre, sauvé dans
  `work/original-game-1.0.patch`, hors dépôt ; `git apply` pour le reprendre)** : `wwe12_original.h` (`Wwe12OriginalGame()`) +
  `wwe12_original.cpp` (cvar `wwe12_original_game`, défaut false ; le launcher n'écrira que cette clé). À true,
  il coupe les AJOUTS, jamais les correctifs. Chaque ajout le teste à côté de son cvar :
  `if (Wwe12OriginalGame() || !REXCVAR_GET(x))`. Dans le patch : clavier, vibrations Univers, intros sautées
  (Gameplay) ; à brancher par Univers : EDIT UNIVERSE, menu GRAPHICS, QUITTER ; à décider : icônes PS, textes
  « Online », musiques perso.

- **Outils de test sans l'utilisateur (Rendu, 2026-10-20)** : télécommande `wwe12_remote_pad` (mots de `remote_pad.txt`
  à côté de l'exe : A B X Y START BACK UP DOWN LEFT RIGHT LB RB, `wait<ms>`) + captures d'écran PowerShell ; parcours
  menu → match : `START A A A A A` puis `RIGHT A A A A A A` (attentes 2-3 s). Validation Vulkan :
  `tools/dev/lancer_validation.bat` (SDK Vulkan installé, synchro comprise). Profil CPU 4 fils :
  `lancer_test_4fils.bat` (piles) ; cadence sans perturbation : `lancer_test_4fils_sans_mesure.bat`.
- **Correctifs 0.9 (Rendu, 2026-10-20, non vérifiés sur Intel/AMD)** : pavés #7/#11/#13/#16 = cible peau 1024x512 à
  3 mips attachée avec tous ses niveaux (vue niveau 0 seul, `draw_framebuffer.cpp`) ; filler BLENDINDICES en
  R32_UINT. CPU : patch SDK `rexglue-sdk-timer-sleep.patch` (fil TimerQueue 20 % → 0 %), scan des textes moins
  fréquent, hachage échantillonné textures/géométrie (ligne `[perf] hashed per frame`). Reste : attente active
  du fil de rendu sur le processeur de commandes (`sub_82AA7040`, ~17 % de son temps actif).

## Nouvelles pistes (2026-10-19)

- **Ceinture d'Orton à 60 fps : CORRIGÉE le 2026-10-21** (commit 0ac5ae7, validé en jeu par l'utilisateur) : la
  ceinture se retournait d'environ 180° une image sur deux sur son épaule (captures RenderDoc consécutives,
  `tools/dev/lancer_autocap_orton.bat`). Cause : rotation lue entre deux images paires. `sub_8248CE10` (rotation
  d'un os → quaternion à fiche+0, avant l'adaptation à la taille du catcheur dans `sub_8248D528`) est maintenant
  aussi lue aux deux images paires, puis mélangée par le chemin court (même cvar `wwe12_title_fix`). Fiche d'os
  ([motion+92] + i*80) : +0 rotation, +16 translation, +32 autre rotation (`sub_82488DE0`), +48 copie de la
  translation, +64 drapeaux.
- **Ceinture à 60 fps : CORRIGÉE le 2026-10-19** (`wwe12_title_fix.cpp`, cvar `wwe12_title_fix` activé par défaut ;
  validé en jeu par l'utilisateur sur l'entrée de Cena). Cause : clés d'animation impaires fausses (la console
  à 30 fps ne lit que les paires). Échantillonneurs de translation `sub_8248B898` (clés 16 bits en deltas) et
  `sub_8248B458` (8 bits) ; correction seulement quand le jeu demande 30 et tourne à 60 (`Wwe12EntranceAt60`,
  `wwe12_fps.cpp`). Mesure : écart au trajet p90 8,2 → 0,6 cm ; ~0,3 % des positions corrigées. **Décision de
  l'utilisateur (2026-10-19) : `wwe12_60fps` reste désactivé par défaut** (réglage AVANCÉ pour ceux qui le
  veulent) ; `wwe12_title_fix` reste à true (sans effet tant que le 60 fps est coupé). Ancienne note :
  l'auteur du recomp de WWE 2K14 l'a corrigée chez lui (entrées et scènes de
  victoire). Son explication : les ceintures ont leur propre animation, séparée des catcheurs ; comme le
  mouvement peut passer d'un catcheur à un autre de taille différente, le jeu estime la position, et
  cette estimation part en vrille au-dessus de 30 fps. L'utilisateur lui demande quelle fonction / quel
  calcul il a changé (et sa licence avant de reprendre du code). But : entrées à 60 fps par défaut.
  Rendu.
- **Version Android** : beaucoup de joueurs la demandent ; conversation dédiée « Android » (Opus). Étape
  1 = état des lieux sans casser le PC (ARM64 / NDK, presets linux-arm64 du SDK, Vulkan via plume, SDL ;
  parties propres à Windows ; accès à l'ISO ; tactile ; performances). Il faut un téléphone de test.
  Kaiko a fait une version Android de SvR 2011 (conversation « Android (Mali) ») : lui demander.
  **État (2026-10-22, conversation « WWE 12 PC (Android) »)** : outils sur `D:\Android` (SDK, NDK 30 = clang 21,
  émulateur WHPX ; Android Studio sur C:). Pas de téléphone : testeurs + émulateur x86_64 (il utilise le GPU
  du PC, donc textures BC OK). Fait : appli de diagnostic `android/diag` (APK sans Gradle, `build.ps1`, clé de
  signature hors dépôt dans `D:\Android\keys`, release GitHub à part avec l'APK seul, sans .zip : le launcher
  ne regarde que les releases avec un .zip) ; SDK compilé pour Android x86_64 + arm64
  (`port/patches/rexglue-sdk-android.patch`, `android/build_sdk.ps1`, sorties sur D:). Le SDK PC reste
  identique (code Android derrière `REX_PLATFORM_ANDROID` / `if(ANDROID)`).
  **2026-10-23 : le jeu tourne sur l'émulateur jusqu'au MENU PRINCIPAL** (démo 3D ~40 fps, sauvegarde créée ;
  commit 2bea735). Jeu = `libwwe12.so` (`port/cmake/android.cmake`), APK par `android/game/build.ps1` (sans
  données du jeu). Sur le téléphone, `Android/data/io.github.imrburn.wwe12/files` joue le rôle du dossier de l'exe
  (variables `REX_APP_DIR` / `REX_NATIVE_LIB_DIR` posées par `WWE12Activity`) : `game/` (ISO ou fichiers),
  `wwe12.toml`, `logs/`, `mods/`, `dlc/`, `wwe12/` (sauvegardes). Les fichiers du portage qui incluent
  `<windows.h>` reçoivent `port/src/android/compat/windows.h` (aucune modif côté PC). **Fichiers partagés
  touchés (Rendu)** : `reblue/gpu/backend.h` (`kSwapChainFormat` : RGBA8 sur Android, BGRA8 inchangé sur PC),
  `device.cpp`, `device_pipelines.cpp`, `imgui_overlay_drawer.cpp`, `platform/native_window.cpp` ; patch
  `plume-android.patch` (appliqué par `setup.ps1`). Pièges : fichiers poussés par `adb` = propriétaire « shell »,
  invisibles pour l'appli (chown en root sur l'émulateur) ; grand modèle mémoire x86_64 = 70 000 sections
  refusées par Android. Test sans manette : `wwe12_remote_pad = true` + `remote_pad.txt` dans `files/`.
  **2026-10-24 : alpha publique 1 prête** (`D:\Android\release\WWE12-PC-Android-alpha1.apk`, ARM64, notes
  `android/game/RELEASE_NOTES_alpha1.md`, tag prévu `android-alpha-1` en pre-release, APK seul). Launcher Android
  (`LauncherActivity` : ISO par le sélecteur d'Android lue via /proc/self/fd, réglages dans wwe12.toml, partage du
  journal), jeu dans son propre processus `:game`, boutons tactiles = manette SDL virtuelle (`TouchControls.java` +
  `port/src/android/wwe12_touch.cpp`). **Clé de signature `D:\Android\keys\wwe12-android.jks` : à sauvegarder**
  (sans elle, plus de mise à jour possible sans désinstaller, donc perte des sauvegardes des joueurs).
  Reste : optimisations (mesures des testeurs), QUITTER du menu, icônes PS, boutons déplaçables, traces de plantage.
  **2026-10-24/25 (alpha 2 publiée, alpha 3 en préparation)** : ARMv8.2, boucles à vide corrigées
  (`rexglue-sdk-android2-cpu.patch`), écran noir après arrière-plan / Retroid #21 (`present.cpp` garde une
  référence sur la fenêtre Android et refait la swap chain), stderr dans le journal, pilotes Turnip
  (libadrenotools dans `recomp/libadrenotools`, `plume-android-driver.patch`, `wwe12_driver.cpp`), cache
  `VkPipelineCache` sur disque (`plume-android-cache.patch`). **Pour la Main / le PC** : nouveau patch SDK
  `rexglue-sdk-thread-name.patch` (course sur le nom d'un thread au démarrage, plantage possible aussi sur
  PC) : recompiler le SDK PC avant la prochaine version PC. Testeurs : `work/android/reports/`.

- **Version vraiment native de WWE '12 (souhait de l'utilisateur, 2026-10-21, long terme, après la 1.1)** :
  aujourd'hui le jeu est recompilé automatiquement (le C++ de `port/generated` est illisible et ne se
  publie pas). Piste : décompilation **progressive** (comme déjà fait à la main pour les deux ouvriers de
  jobs, `wwe12_actor_jobs.cpp`) : remplacer les fonctions du jeu par du C++ lisible, système par système
  (jobs, chargement, entrées, menus, animation), en gardant le reste recompilé ; chaque fonction
  réécrite se vérifie contre l'original. À discuter avec Jarrod (WWE 2K14) pour sa méthode.
  Règle inchangée : le code dérivé du jeu et les données ne se publient pas sans décision de l'utilisateur.

- **Suggestions d'un joueur (2026-10-21)** : longue liste sur l'Univers (titres, classements, calendrier, tournois,
  qualifs, stats, tenues, CAW 100-200, saignement des catcheuses, types de match de 2K14…). Triée par effort
  dans `docs/suggestions_joueurs.md` ; rien promis. Déjà fait : titre vacant, tenues, reset.

- **Title Update de WWE '12 (1.1), piste du recomp de WWE '13** (2026-10-22, étude de la conversation Limit breaking) :
  PromethenDev/wwe13-recomp (BSD-3) et son SDK (fork HollywoodAkeem/rexglue-sdk-yukes + 99 patchs, cloné dans
  `work/wwe13`, jamais poussé). Leur méthode : le codegen passe par le chargeur XEX normal, qui applique le
  `default.xexp` posé à côté du `default.xex` (cvar `xex_apply_patches`), donc le C++ généré est celui du jeu
  patché ; clé `patched_file_path` dans la config de codegen (absente de notre SDK c94f5eb) ; variable
  `REX_DUMP_PATCHED_IMAGE` pour recaler les adresses du manifest ; le jeu exige la TU exacte (2.0.1.0 chez eux).
  Pour nous : TU v4 = 0.0.0.4 → 0.0.2.4. Code tiers : revue de licence et accord de l'utilisateur d'abord.

## Plans et feuilles de route

- Feuille de route actuelle : `docs/feuille_de_route.md` (1.1.0 publiée le 2026-11-07, pas de 1.1.1 (décision du 2026-11-08), 1.2 = « Moveset update » (prises SvR / 2K dans le Mod Maker + page Moves & Entrances + gamma + corrections),
  1.3 = corrections THQ par nos crochets + performances / code natif, 1.4 = modes de match + page ROSTERS ; décision du 2026-11-08). Idées de joueurs : `docs/suggestions_joueurs.md`.
- Anciens plans 0.8 / 0.9 / 1.0 et anciennes listes : `docs/archives/claude_md_anciens_plans_2026-11-06.md`.

## Optimisation (2026-10-12)

- Chargements : l'horloge Windows 1 ms (`wwe12_precise_timer`) ne change rien (~12 s de chargement
  de match avant/après) ; le jeu attend volontairement. Laissée désactivée.
- **Chargements x2** : `src/native/wwe12_loading.cpp` relève le budget du chargeur (sub_82819258,
  float à this+196 comparé au temps depuis la dernière image, même design que SvR 2009/2010 de
  nefarious) à `wwe12_load_slice_ms` = 12 ms. Match : ~6 s au lieu de ~12 s avant l'entrée. Le passage
  entrée → match (~5 s) ne bouge pas : autre attente, à étudier.
- Saccades 1er lancement : `pipelines_seed.w12p` livré à côté de l'exe (copie de
  `Documents\wwe12\cache\pipelines.bin` par `tools/package.ps1`), préparé en arrière-plan au démarrage.
- Petits PC : `svr_render_scale = 540` (0,75x) existe, image correcte ; le voile blanc des entrées en 540p a disparu avec la correction Inf/NaN (vérifié sur l'iGPU Intel) :
  540p est proposé dans le launcher et OPTIONS > GRAPHICS. Puce Intel RaptorLake à 720p :
  ~6 ms de travail par image en match, 12 ms dans les menus.
