# WWE '12 pour iOS : brouillon (rien n'est encore compilé)

Méthode du portage SvR 2011 iOS de Ke619 (<https://github.com/Ke619/WWE-SvR11-IOS>) : le jeu est
déjà recompilé en C++ (`port/generated`), un runner macOS de GitHub Actions le compile pour iPhone
avec le SDK ReXGlue et plume (Vulkan via MoltenVK), et sort un IPA non signé. Pas de JIT : tout est
compilé à l'avance.

**État** : le workflow et les scripts sont écrits, leurs points d'ancrage sont vérifiés sur des copies
de ton SDK et de ton port. Rien n'a été compilé (pas de Mac ici) : la première exécution montrera des
erreurs de compilation, comme pour Ke619 au début. Le workflow compile avec `-k 0` pour les afficher toutes.

## Fichiers

Dans le dépôt PUBLIC (`wwe12-pc`, ajoutés ici, rien de poussé) :

| Fichier | Rôle |
|---|---|
| `docs/ios/ios-port.yml` | le workflow, à copier dans `.github/workflows/` du dépôt privé |
| `docs/ios/ios_sdk_edits.py` | modifications du SDK et de plume pour iOS (ancres vérifiées) |
| `docs/ios/ios_port_edits.py` | modifications du port : CMake, fenêtre, réglages téléphone, pas de codegen |
| `port/cmake/ios.cmake` | cible iOS (équivalent de `android.cmake`) |
| `port/src/ios/wwe12_ios.cpp` | `VirtualQuery` (Mach VM), journal de démarrage, copie de `wwe12.toml` |
| `port/patches/ios/plume_ios.mm` | fenêtre plume en UIKit (de Ke619) |

Dans un dépôt PRIVÉ (jamais public : c'est la traduction du jeu) :

| Fichier | D'où il vient |
|---|---|
| `.github/workflows/ios-port.yml` | `docs/ios/ios-port.yml` |
| `generated/` (≈ 268 Mo, 492 fichiers) | ton `port/generated/` |
| `host_shaders/*.hlsl.spirv.h` (13 fichiers) | déjà préparés dans `work/ios_private/host_shaders/` : les shaders de l'interface compilés par `dxc.exe`, qui ne tourne pas sur macOS |
| `icon/icon.png` | préparé dans `work/ios_private/icon/` (le logo de l'exe) |

Pas de `default.xex`, de `.xexp`, d'ISO ni de `port/assets/` : le workflow s'arrête s'il en trouve.
Les shaders du jeu sont déjà dans `generated/native/`.

## Étapes

1. Créer un dépôt privé GitHub, y mettre les fichiers du second tableau (`git lfs` inutile : aucun
   fichier ne dépasse 100 Mo).
2. Pousser le port public (ton go d'abord) : le workflow clone `iMrBurn/wwe12-pc` et lit les scripts
   dans `docs/ios/`. Avant ça, il peut aussi viser une branche (`port_ref`).
3. Actions → `ios-port` → Run workflow. Premier passage : lire `build.log` / le résumé d'erreurs,
   corriger, relancer (ccache accélère les suivants).
4. Installer l'IPA avec Sideloadly ou AltStore (signature avec un identifiant Apple). Mettre le jeu
   (dossier extrait ou ISO) dans `Documents/game/` par l'app Fichiers (`UIFileSharingEnabled`).
   `Documents/boot.log` dit pourquoi un démarrage échoue.

## Ce qui manque ou risque de coincer

- **Mémoire** : le jeu réserve 4,5 Go d'adresses (fichier creux dans `TMPDIR`). iOS peut tuer l'app
  si elle en utilise trop pour de vrai ; l'option « increased memory limit » de l'outil de sideload
  aidera sans doute. À mesurer.
- **Commandes** : pas de tactile pour l'instant (celui d'Android est en JNI) ; manette Bluetooth.
- **Pas repris** : lanceur UIKit de Ke619, pilotes GPU, musiques perso, en ligne (WWE '12 n'en a pas).
- **Rendu** : la fenêtre passe par `UIWindow` + `CAMetalLayer` (SDL), jamais essayé sur un iPhone.
  Ke619 le note aussi « not yet tested at run time » dans son `plume_ios.mm`.
- Les patchs plume `android-driver` et `android-cache` sont appliqués par fidélité à `setup.ps1` ;
  s'ils gênent, les retirer de la boucle du workflow.
- Le workflow de Ke619 est sous licence MIT (fichier LICENSE de son dépôt) : crédit conservé dans
  chaque fichier repris.

## État au 2026-11-13 (le texte ci-dessus date des premiers essais)

- **Ça tourne sur iPhone 15 Pro** (IPA 17 testé par le propriétaire : textures et HUD corrects, pas de
  plantage). Le jeu se lance depuis un lanceur UIKit (`port/src/ios/wwe12_launcher_ios.mm` : Play,
  Install, DLC, Settings, Controls, Support, Saves, Mods), avec commandes tactiles
  (`wwe12_touch_ios.mm`).
- **Build** : le workflow `ios-port` du dépôt PUBLIC (`.github/workflows/ios-port.yml`, minutes gratuites)
  construit la branche d'où on le lance : `gh workflow run ios-port.yml -R iMrBurn/wwe12-pc --ref <branche>`.
  Les fichiers privés (`generated/`, `host_shaders/`, icône) viennent du dépôt privé
  `iMrBurn/wwe12-ios-private` avec le secret `IOS_PRIVATE_TOKEN` ; l'IPA et les logs vont dans une
  release du dépôt privé (jamais en artefact public). Le jeton gh de la machine n'a pas le droit de
  modifier les fichiers de workflow : tout changement de workflow se fait depuis l'interface GitHub.
- **Textures** : MoltenVK affichait des textures noires ou mélangées avec la table bindless de 65536
  entrées. `gpu/compact_textures.cpp` (actif avec `WWE12_IOS`, ou `-DWWE12_COMPACT_TEXTURES=ON` sur
  l'exe Vulkan PC) remplit avant chaque dessin une petite table de 128 entrées et réécrit les indices
  des constantes partagées ; les emplacements restent un registre côté CPU (`SetBindlessTexture`).
- **Autres réglages iOS** : replay instantané coupé (même gel qu'Android en fin de match), pause du rendu
  en arrière-plan, cache de pipelines Vulkan dans `Library/Caches`, 3 doigts = journal détaillé d'une
  image (F9).
- **Essayés puis abandonnés** : maths exactes de MoltenVK (plante le GPU au chargement d'un match),
  décodage BC sur CPU forcé (aucun gain), re-liaison du set de textures (aucun gain).
- **Signature gratuite** : l'app expire au bout de 7 jours ; après réinstallation, refaire confiance au
  certificat (Réglages > Général > VPN et gestion de l'appareil), VPN coupé.

## Performances au 2026-11-18 (logs 018 et 019, 1.4, iPhone 15 Pro)

Lecture des compteurs `[perf]` du log 019 (2 min 40 de jeu, un match I Quit puis la scène de victoire) :

| Moment | Images | Intervalle | Travail CPU | Dessins (constantes) | Saccades > 25 ms |
|---|---|---|---|---|---|
| Menus | 600 / 10 s | 16,7 ms | 12,2 ms | 0,6 (0,4) ms | 0 |
| Match, 1re minute | 592 / 10 s | 16,9 ms | 11,1 ms | 2,0 (1,3) ms | 5 |
| Match, 2e minute | 523 / 10 s | 19,1 ms | 17,7 ms | 3,7 (2,3) ms | 63 |
| Scène de victoire | 362 / 10 s | 27,6 ms | 24,7 ms | 7,1 (3,2) ms | 162 |

- **Le téléphone baisse ses fréquences.** Entre la 1re et la 2e minute de match, la scène ne change pas
  (0,5 Mo de géométrie hachée par image dans les deux cas) mais *tous* les compteurs CPU montent du
  même facteur ~1,8 (dessins 2,0 → 3,7 ms, textures 0,6 → 0,9, géométrie 0,8 → 1,4). Ce n'est pas le
  jeu qui travaille plus, c'est l'A17 Pro qui ralentit en chauffant. D'où le journal `[thermal]` de
  `wwe12_touch_ios.mm` (dans `boot.log`, à chaque changement) : un prochain test le dira noir sur blanc.
- **La scène de victoire à 60 fps ne passe pas.** Le jeu demande 30 (`[fps] drop to 30 asked`, script
  6161), l'option « 60 fps dans les entrées » le garde à 60, et l'iPhone rend 36 images par seconde
  avec 162 saccades en 10 s. En 30 fps la même scène serait fluide. Le lanceur a maintenant les trois
  modes du menu GRAPHICS du PC (`FrameRateRow`, écrit `wwe12_frame_mode` **et** `wwe12_60fps` : l'ancien
  interrupteur n'écrivait que le second, que `wwe12_frame_mode = "original"` du wwe12.toml livré
  annulait).
- **Gels d'une demi-seconde à chaque nouvelle scène** : `pipelines 3 (0 known, 562 ms)` puis
  `pipelines 1 (0 known, 240 ms)` quand la victoire arrive, avec `pso: render-thread compile (miss,
  not in residual)`. Le log de démarrage disait `0 from the shipped seed` : l'IPA ne livrait pas la
  graine que le zip PC et l'APK livrent. Elle est maintenant dans l'exe (`port/res/pipelines_seed.w12p`
  et `declarations_seed.w12d`, ressources 103 et 104 de `wwe12_resources.cpp`) et écrite au démarrage
  dans Documents, qui est le dossier du jeu sur iOS (`IosInstallSeeds`).
- **Moins de travail par dessin** : `compact_textures.cpp` garde la réponse du dessin précédent
  (mêmes 96 indices, même image, même état du registre) au lieu de refaire le parcours, la recherche
  dans la réserve et les trois liaisons de set.
- **Pas encore regardé** : des relectures GPU → CPU de 1024x512 deux fois par seconde (13 à 31 ms
  chacune d'après les lignes `[load] readbacks`) ; 512 textures BC décodées sur le CPU au fil de la
  partie alors que l'avertissement « le GPU refuse BC » n'apparaît jamais dans le log.
- **Bruit connu, sans effet** : 446 `PhysicalHeap::Alloc` + 446 `BaseHeap::Alloc` au démarrage
  seulement (aussi sur PC), et les études `[iquit] etude arbitre` de la 1.4 (deux lignes par seconde
  pendant un match I Quit, utiles pour lire les logs, à retirer quand le portage WWE '13 sera figé).
