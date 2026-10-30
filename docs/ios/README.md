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
