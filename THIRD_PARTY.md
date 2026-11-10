# Composants tiers

Le code propre au projet est sous licence **GPL-3.0** (voir le README). Les composants
ci-dessous gardent leur propre licence. Aucune donnée du jeu (XEX, fichiers du disque,
shaders extraits, code généré) n'est incluse dans ce dépôt ni dans ses versions publiées.

| Composant | Utilisation | Licence |
|---|---|---|
| [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) | recompilation statique, runtime | BSD 3 clauses (dérivé de [Xenia](https://xenia.jp)) |
| [XenosRecomp](https://github.com/hedge-dev/XenosRecomp) | conversion des shaders Xbox 360 en HLSL | MIT |
| [re:Blue](https://github.com/zolaware/reblue) | cœur du rendu natif (`port/src/reblue`) ; pour `wwe12_d3d12.exe`, `gpu/shaders/shader_linker.*` et `dxc_link.*` repris tels quels du dépôt (zolaware, commit 85aae68, 2026-10-26) | BSD 3 clauses (`port/src/reblue/LICENSE.reblue`) |
| [plume](https://github.com/zolaware/plume) | couche Vulkan / D3D12 du rendu natif | MIT |
| [DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler) | compilation HLSL → DXIL / SPIR-V ; `dxcompiler.dll` et `dxil.dll` livrés à côté de `wwe12_d3d12.exe` (liaison des shaders à l'exécution) | MIT (LLVM : Apache 2.0 avec exception LLVM) |
| [miniz](https://github.com/richgel999/miniz) | `wwe12_d3d12.exe` : décompression du cache de shaders préliés de re:Blue | domaine public (unlicense) |
| [aoTuV](https://github.com/AO-Yumi/vorbis_aotuv) beta6.03-2020 (libvorbis de Xiph + réglages d'Aoyumi) + [libogg](https://xiph.org/ogg/) 1.3.5 | Mod Maker : encodeur Vorbis des thèmes portés, écrits comme les thèmes Wwise Vorbis du disque (`port/src/modmaker/third_party/vorbis_aotuv`, `libogg`, fichiers de l'encodeur seulement, non modifiés ; accord de l'utilisateur le 2026-11-10) | BSD 3 clauses (Aoyumi, Xiph.org ; `COPYING` de chaque dossier) |
| [vgmstream](https://github.com/vgmstream/vgmstream) | Mod Maker : lecture des musiques des autres jeux ; `vgmstream-cli.exe` et ses DLL, release Windows officielle r2168 non modifiée, livrées dans `vgmstream\` avec son `COPYING` (pas dans le dépôt) | ISC (vgmstream) ; DLL des codecs : FFmpeg et mpg123 LGPL 2.1, Vorbis et Speex BSD, CELT BSD |
| [libadrenotools](https://github.com/bylaws/libadrenotools) + [liblinkernsbypass](https://github.com/bylaws/liblinkernsbypass) | version Android : chargement d'un pilote Vulkan personnalisé (Turnip) | BSD 2 clauses (Billy Laws) |

Modifications locales appliquées par `setup.ps1` :

- `port/patches/rexglue-sdk-wwe12.patch` : diagnostic DRED, marqueurs et historique des
  draws D3D12, option `d3d12_dred`.
- `port/patches/xenosrecomp-wwe12.patch` : constantes de spécialisation fixées à la
  compilation, interpolateurs de WWE '12, cube maps.

Projets dont ce portage s'inspire :

- [wwe-svr2011-pc](https://github.com/KaikoClanworth1/wwe-svr2011-pc) (portage PC de SvR 2011,
  KaikoClanworth1) : pas de licence publiée ; **avec son accord écrit du 2026-10-26**, son backend
  Direct3D 11 pour plume (`plume_d3d11.h/.cpp`, de son `plume-svr2011.patch`, dépôt au commit b2521fe)
  est repris et adapté dans `port/patches/plume-d3d11.patch` (version DirectX 11, en cours), crédité
  « SvR 2011 PC port (KaikoClanworth1) ». Son « native_widescreen » (constante de rapport de la
  caméra patchée, champ de vision vertical élargi en 16:10, cibles plein écran élargies ou plus
  hautes) est adapté, avec le même accord, dans `port/src/native/wwe12_widescreen.cpp` et
  `svr_resources.cpp` (cvar `wwe12_widescreen`). Le dépôt a depuis une licence MIT (vue le 2026-11-04,
  commit 5fcae1a) : l'étiquette de page de l'écran des arènes (« LB  PAGE 1 / 2  RB », son
  `arena_mods.cpp` ArenaPageLabel) est adaptée dans `port/src/native/wwe12_arena_pages.cpp`. Le reste :
  méthode uniquement. Android (README du dépôt, 2026-11-07) : la variable `FD_DEV_FEATURES=enable_tp_ubwc_flag_hint=1`
  pour les pilotes Turnip sous HyperOS (case facultative du launcher, `DriverManager.java`) et l'idée du retour au
  pilote du téléphone quand un pilote personnalisé échoue.
  Benchmark (1.3) : sa méthode du match de test (un trajet fixe dans les menus, puis une vérification du
  match obtenu, `test_match.cpp`) est reprise en méthode seulement dans `port/src/native/wwe12_benchmark.cpp`
  (scénario `hiac6`, trajet enregistré, vérification au départ du match), ainsi que l'idée de sa condition
  « faible » (`opt_bench.ps1` : cœurs limités et un fil occupé à côté).
- [wwe2k14-recomp](https://github.com/Jarrodfeng/wwe2k14-recomp) (recomp de WWE 2K14, Jarrodfeng) :
  licence BSD 3 clauses (vérifiée sur le dépôt le 2026-10-31) ; accord oral de l'auteur (Discord, 2026-10-31) pour s'inspirer de ses mises à
  jour. Idées Android étudiées dans `docs/android_optimisation_jarrod.md` (cadence 90/120 Hz, résolution
  dynamique, SGSR, cœurs rapides, copies GPU), correction de la ceinture à 60 fps (méthode) et, pour Android
  (2026-11-07), le constat de ses notes android-v0.2.4 : les réveils d'événements fusionnés bloquent l'enregistreur
  de moments forts, d'où le réveil forcé de `port/src/android/wwe12_jobwatch.cpp` (code écrit par nous). Aucun
  code repris pour l'instant ; tout code repris plus tard sera ajouté ici avec son fichier.
- **Yexclsv**, co-mainteneur de la version Android (travail donné pour le projet, crédité dans les notes
  de version) : gestionnaire de pilotes GPU (`DriverManager.java`), vérification du téléphone
  (`DeviceCheck.java`), température et avertissement de chauffe, import de sauvegardes (idée de
  `SaveImporter`), réglages et préréglages en un appui, et le **nouveau look du launcher** (jaune et noir,
  écran de chargement : `Ui.java`, `GameLoading.java`, `android/game/UI_REDESIGN.md`, 1.1).
- Portages SvR 2008/2009/2010 de [nefariousjosiah](https://github.com/nefariousjosiah)
  (GPL-3.0) : leur rendu natif (src/native, adaptations de src/reblue) est la base du rendu
  natif de WWE '12.

La liste complète des bibliothèques incluses dans l'exe et les DLL, avec leurs licences, est
dans `port/dist/THIRD_PARTY_NOTICES.txt` (livré avec chaque version, textes dans `licenses/`).

- Icône de `wwe12.exe` (`port/res/wwe12.ico`) : composée à partir du logo de WWE '12, marque de WWE/THQ, utilisée par ce projet de fans sans affiliation.
- Icônes de boutons PlayStation (`port/res/ps3_buttons.bin`, intégrées à `wwe12.exe`) : extraites de la version PS3 de WWE '12 (THQ/Yuke's ; symboles des manettes PlayStation, marques de Sony Interactive Entertainment), utilisées par ce projet de fans sans affiliation, à la demande de l'auteur du projet.
- Netteté de l'image agrandie (1.3, `svr_upscale_sharpness`, `port/src/reblue/gpu/shaders/hlsl/gamma_correction_ps.hlsl`) :
  idée du mode 720p du recomp WWE 2K14 de Jarrodfeng ; formule de netteté adaptative au contraste décrite par AMD
  FidelityFX CAS (licence MIT), réécrite par nous (aucun code copié).
