# Version Linux NATIVE (Steam Deck et PC Linux sans Proton) : étude de faisabilité

Étude du 2026-11-06 (session Android), **lecture seule** : rien n'a été compilé ni lancé, aucun fichier du
code modifié. Demande de l'utilisateur, idée d'un joueur Discord (« use an Arch Linux VM and ask Claude to
package for it »). **Pas pour la 1.1 ni la 1.2** : « à étudier ». Ce qui est **vérifié dans le code** est
séparé de ce qui est **supposé** ; aucune promesse de performance.

## 0. Résumé (réponse courte)

- **Faisable, et plus proche que prévu.** Le plus dur de la partie « pas Windows » est déjà fait pour
  Android : le port compile sans `<windows.h>` grâce à un petit remplaçant (`port/src/android/compat`),
  le rendu passe par SDL + plume/Vulkan, le SDK a déjà ses fichiers POSIX (`*_posix.cpp`), ses presets
  `linux-amd64` / `linux-arm64` et une fenêtre SDL. Un exe Linux x86_64 est surtout un **assemblage de
  pièces déjà existantes** (SDK POSIX + code du port compilé comme sur Android + Vulkan sur X11/Wayland).
- **Ce qui manque** : un fichier CMake `linux.cmake` (équivalent de `android.cmake`), des vraies versions
  Linux de quelques fonctions que le remplaçant Android laisse « sans effet » (chemins de données,
  affinité, priorités, minuteurs, bordures), un **launcher** (le nôtre est Win32 + D3D11), l'empaquetage, et
  des **tests sur de vrais Linux / Steam Deck** (aucun sous la main).
- **Ordre de grandeur** : 8 à 12 séances pour une bêta testable (exe qui démarre, joue, archive `.tar.xz`),
  plus des allers-retours avec des joueurs. Le launcher graphique et les paquets « jolis » sont en plus.
- **Gain de performance : inconnu, à mesurer.** Il peut être faible (Proton ajoute peu de CPU côté Vulkan)
  ou non négligeable sur certains points (attentes de fils, exceptions) ; voir la partie 6. Le vrai gain
  est surtout la **simplicité** (pas de Proton à régler, mode jeu du Deck, manette).
- **Lien avec le replay Android** : un exe Linux natif utiliserait **le même code de fils POSIX** que
  Android. Si le gel du replay vient de cette couche (voir `docs/android_apres_1_1.md`), il apparaîtrait
  aussi sur Linux natif ; à régler avant, ou en même temps.

## 1. Ce qui est spécifique à Windows

### 1.1 Dans `port/src` (nombre de fichiers : `reblue` 97, `native` 65, `modmaker` 71, `launcher` 16, `android` 9)

| Zone | Windows ? | Déjà abstrait par Android ? | Pour Linux x86_64 |
|---|---|---|---|
| `src/reblue/` (rendu natif re:Blue) | Seulement 1 fichier avec `<windows.h>` ; **les branches `#else` existent** : fenêtre `native_window.cpp` (SDL, « suit le pilote vidéo choisi par SDL »), boîtes de dialogue `fatal_dialog.cpp` (SDL), `host_resources.cpp` | Oui (Android : `__ANDROID__`) | Peu de choses : vérifier que la branche `#else` (Linux) compile et marche avec X11 et Wayland. Ni D3D12 ni D3D11 : **Vulkan seulement** |
| `src/native/` (65 fichiers) | **22 fichiers incluent `<windows.h>`** (affinité, minuteurs, `GetAsyncKeyState`, `VirtualQuery/Protect`, fenêtres, `SHGetKnownFolderPath` pour `Documents`, taille d'écran) ; `crash_trace` (dbghelp), `memscan`, `autocap` (RenderDoc), `d3d_census` : outils PC | Oui : `port/src/android/compat/windows.h` (185 lignes) remplace ces appels ; `android.cmake` retire 4 fichiers PC | Réutiliser et **généraliser** ce remplaçant (le renommer en `port/src/posix_compat`) ; remettre de vraies versions là où Linux sait faire (voir 1.2) |
| `src/launcher/` (16 fichiers) | **Tout** : fenêtre Win32, D3D11, ImGui Win32, `winhttp` (mises à jour), WIC (images), `SHGetKnownFolderPath`, `CreateProcess`, `ShellExecute` | Non (Android a un autre launcher en Java) | Voir partie 3 |
| `src/modmaker/` (71 fichiers) | Win32 + D3D11 + WIC + lecture vidéo Windows ; outil de bureau | Non | **Reste Windows** (le dossier `mods` est des données portables ; le Mod Maker se lance sous Wine/Proton si besoin) |
| `src/prelink/` (outil), exe `wwe12_d3d12`, `wwe12_d3d11` | Windows | Sans objet | **Non repris** (Vulkan natif : RADV, Mesa, NVIDIA) |
| Racine : `main.cpp`, `wwe12_app.h` (démarrage), `wwe12_dlc.cpp`, `wwe12_saves.cpp`, `wwe12_disc.cpp` | Chemins `Documents\wwe12` (par le SDK), sinon portable (`std::filesystem`) | Oui (`REX_APP_DIR` sur Android) | Chemin utilisateur Linux (voir 1.2) |
| `CMakeLists.txt` | Le `else()` de `add_executable(wwe12 ...)` **existe déjà** (ni WIN32 ni ANDROID) ; icône/`.rc` seulement sous `if(WIN32)` | Oui (`cmake/android.cmake`) | Un `cmake/linux.cmake` (voir plus bas) |

Preuve de portabilité : les 22 fichiers `native` qui incluent `<windows.h>` **compilent déjà pour Android**
(arm64 et x86_64) avec le remplaçant. Le travail Linux est donc surtout de **lui donner de vraies
implémentations** plutôt que « rien ne se passe ».

### 1.2 Ce que le remplaçant Android laisse « sans effet » et que Linux peut faire pour de vrai

| Fonction Windows | Sur Android | Sur Linux |
|---|---|---|
| Affinité (`wwe12_dev_affinity.cpp` = **outil de développement** du port ; l'affinité des fils du jeu est faite par le SDK, `rex::thread::EnableAffinityConfiguration`) | rien | `sched_setaffinity` : utile (Deck 4 cœurs / 8 fils) ; **à vérifier** si la branche POSIX du SDK l'applique déjà |
| Priorité des fils | rien (« Android gère seul ») | `setpriority` négatif refusé sans `CAP_SYS_NICE` (même « permission denied » que sur Android) : rtkit / `nice` seulement ; **à garder en tête pour le replay** |
| Minuteur haute résolution / limiteur d'images (`wwe12_display.cpp`) | `sleep_for` | `clock_nanosleep` (précis) |
| `GetAsyncKeyState(F9)`, F2, etc. | rien | SDL (événements clavier) |
| Fenêtre sans bordure / plein écran (`borderless` du SDK, patch `rexglue-sdk-borderless`) | rien | SDL (`SDL_SetWindowFullscreen`, bordures) |
| Taille d'écran (liste du menu GRAPHICS) | rien | `SDL_GetDisplayBounds` |
| Dossier des données (`Documents\wwe12`) | `REX_APP_DIR` | `$XDG_DATA_HOME/wwe12` (`~/.local/share/wwe12`) ou à côté de l'exe ; `GetUserFolder()` existe déjà dans `filesystem_posix.cpp` |
| `crash_trace` (dbghelp) | retiré | handler POSIX du SDK (`exception_handler_posix.cpp`) + `backtrace()` |
| `VirtualQuery / VirtualProtect` (sondes) | lit `/proc/self/maps` | idem (même code) |

## 2. Le SDK ReXGlue pour Linux, et ce qui diffère d'Android

**Vérifié** : `CMakePresets.json` du SDK contient `linux-base` (Ninja Multi-Config, **clang / clang++**,
`-march=x86-64-v2`, C++23) et les presets `linux-amd64` / `linux-arm64` (debug, release, relwithdebinfo) ;
le README montre des **workflows de CI GitHub pour `linux-amd64` et `linux-aarch64`**, et remercie des
contributeurs Linux / SDL. Donc le SDK **se compile pour Linux PC** (clang + CMake + Ninja), comme il le
fait déjà ici pour Windows et pour l'Android NDK. Non vérifié ici : une compilation Linux de **notre** version
patchée (nos patchs ont été validés sur Windows et Android seulement).

| Sujet | Android (déjà fait) | Linux PC (à faire / à vérifier) |
|---|---|---|
| libc | **Bionic** : pas de mutex robustes (`#if REX_PLATFORM_LINUX && !REX_PLATFORM_ANDROID` dans `threading_posix.cpp`), `ashmem`/`ASharedMemory` pour la mémoire partagée (`memory_posix.cpp`) | **glibc** : mutex robustes disponibles (le code les utilise déjà hors Android) ; mémoire partagée par `shm_open` (branche POSIX existante du même fichier, vérifiée) |
| Mémoire invité | arène fixe (4 Go virtuels) par `mmap(... MAP_FIXED_NOREPLACE)` ; **le piège des noyaux < 4.17** (notre correctif `rexglue-sdk-zz-android-oldkernel`) | Noyau du Steam Deck (SteamOS 3, 6.x) et des distributions récentes : **`MAP_FIXED_NOREPLACE` connu**, mais le correctif sert quand même (noyaux anciens / conteneurs). À vérifier : collision de l'arène avec ASLR / PIE (le SDK le fait déjà en CI Linux) |
| Sections du binaire | « grand modèle mémoire x86_64 = 70 000 sections » refusé par l'**éditeur de liens d'Android** | Probablement accepté par `lld` / ELF (numérotation étendue) : **à vérifier tôt** (c'est un bloqueur possible) |
| Fenêtre / surface | `ANativeWindow` (activité SDL) | SDL3 (X11 **ou** Wayland, choisi par SDL) ; plume : `PLUME_SDL_VULKAN_ENABLED` (option pour Linux, présente) ou chemin Xlib. Sous gamescope (mode jeu du Deck) SDL + Vulkan marche en pratique, mais **non vérifié** pour nous |
| Audio | SDL3 (AAudio) | SDL3 (PipeWire / PulseAudio / ALSA : choisi par SDL) : **le même pilote SDL** que Android et Windows |
| Entrées | manette SDL virtuelle (tactile) | Manettes SDL (Xbox, DualSense, **Steam Input** au Deck) ; clavier / souris par `mnk` (déjà SDL) |
| Pilote GPU | Vulkan du téléphone ou Turnip (libadrenotools) | Mesa **RADV** (Deck, AMD), ANV (Intel), NVIDIA propriétaire ; Vulkan par `libvulkan.so.1` (chargé par volk) : pas de pilote à embarquer. Fonctions exigées : `descriptorIndexing`, `bufferDeviceAddress` : présentes sur les pilotes de bureau récents |
| Codegen | tourne sur le PC Windows (même C++ généré) | Idem : **on réutilise `port/generated`** produit sur Windows ; pas besoin du codegen sous Linux (il compile ensuite `port/generated` tel quel) |
| Compilation | 241 unités générées, 362 étapes, ~650 Mo par configuration | Même ordre de grandeur (mesure à faire). Un clang récent (20+, comme le NDK 30) est nécessaire |

## 3. Un launcher Linux

Le nôtre est **Win32 + Direct3D 11 + ImGui** : fenêtre Win32, D3D11, WIC (images), `winhttp` (mises à jour),
`CreateProcess` (lancer le jeu), `SHGetKnownFolderPath`. Ce qui est **portable** (std C++ / aucune API
système) : lecture / écriture du `wwe12.toml` (`toml_file.h`), vérification de paquets STFS (`stfs_check.h`),
inflate, sauvegardes et mods (`std::filesystem`), journaux (en partie). Ce qui est à refaire : la fenêtre, le
dessin, le téléchargement et le lancement.

| Option | Idée | Effort | Remarques |
|---|---|---|---|
| **A. Aucun launcher au début** (recommandé) | Le jeu a déjà **OPTIONS > GRAPHICS** (13 réglages) **dans le jeu**, à la manette, qui écrit le `wwe12.toml`. Le chemin de l'ISO / du dossier du jeu vient d'une clé du `wwe12.toml` (ou d'un argument) ; un petit script `wwe12.sh` d'aide | 1 séance | **Bon pour le mode jeu du Deck** (pas de souris). Perd : choix d'ISO graphique, mises à jour, sauvegardes, mods, DLC |
| B. Launcher ImGui sur **SDL3 + Vulkan** (réutilise l'ImGui du SDK et notre `launcher.cpp` en retirant Win32/D3D11) | Même écrans, même code | 3 à 5 séances | Clavier / souris / manette ; à tester sur Deck. Portage de `updater.h` : `libcurl` (dépendance) ou appel de `curl` |
| C. Launcher web local ou console | Mini serveur local (ou TUI) pour les réglages | 2 à 3 séances | Peu naturel pour les joueurs ; ne sert pas au Deck en mode jeu |
| D. Fichiers de bureau (`.desktop`) + éditer `wwe12.toml` à la main | Le strict minimum | 0,5 séance | À donner avec A |

## 4. Empaquetage

Règle absolue du projet : **aucune donnée du jeu** dans une archive ou un paquet (ni ISO, ni `.pac`, ni DLC).
L'exe (code recompilé) est accepté, comme sous Windows ; la graine `pipelines_seed.w12p` /
`declarations_seed.w12d` (aucune donnée du jeu) peut être livrée.

| Format | Ce que ça demande | Pour / contre |
|---|---|---|
| **Archive `.tar.xz`** (`wwe12`, `wwe12.toml`, graines, licences, readme, `wwe12.sh`) | Rien de spécial ; compiler dans le conteneur **Steam Linux Runtime « sniper »** (SDK de Valve) pour viser une glibc ancienne | Le plus simple ; marche au Deck en mode bureau ; mises à jour à la main |
| **AppImage** | `linuxdeploy` / `appimagetool`, FUSE sur la machine du joueur | Un seul fichier, « double-clic » ; bibliothèques Vulkan / Mesa **non** embarquées (elles viennent du système) |
| **Paquet Arch / AUR** (`wwe12-pc-bin`) | Un `PKGBUILD` qui télécharge notre archive de la release GitHub et vérifie la somme de contrôle. **Pas de version « source »** : `port/generated` n'est pas public | L'AUR est communautaire (modération) ; paquet « bin » acceptable ; **à décider avec l'utilisateur** |
| **Flatpak** | Un manifeste, runtime freedesktop, accès au dossier du jeu (`--filesystem`), GPU par l'extension standard | Plus lourd ; publier sur **Flathub** : règles et revue de leur équipe, **risque juridique** (projet qui exige l'ISO d'un jeu commercial) : à étudier avant de promettre |
| Steam (jeu non-Steam) | L'utilisateur ajoute l'exécutable ; pas de Proton (« Linux natif ») | Aucun paquet à faire ; documentation FAQ |

Les mises à jour automatiques du launcher PC (tags `v<ver>`, zip) ne se transposent pas telles quelles : pour
Linux, release GitHub avec une archive `wwe12-pc-v<ver>-linux-x86_64.tar.xz` (même tag), et au début
**pas de mise à jour automatique**.

## 5. Tests sans vrai Steam Deck

- **VM Arch** : disque **50 à 60 Go** conseillés (clang, SDK ~700 Mo par configuration, `port/generated`
  268 Mo, objets du jeu ~650 Mo, plus l'image du système ; l'ISO du jeu ~7-8 Go si on veut le lancer),
  RAM 12 Go ou plus pour compiler, 6 à 8 cœurs. Une VM **n'a pas de GPU** : le rendu se fait par **lavapipe**
  (Vulkan logiciel) : on peut vérifier **le démarrage, l'édition de liens, le chargement de l'ISO, SDL, les
  chemins et les journaux**, très lentement ; on **ne peut pas juger les performances ni les bugs de pilote**.
- **WSL2 sur ce PC** (Arch ou Ubuntu) : compile du Linux natif et peut afficher une fenêtre (WSLg) ; Vulkan
  accéléré par la couche Mesa « dzn » (sur D3D12) : à essayer, c'est la voie la plus rapide pour valider que
  l'exe démarre et dessine, **pas pour juger Deck / RADV**.
- **Conteneur Steam Linux Runtime (sniper)** : pour compiler avec la bonne glibc.
- **Ce qui reste aux joueurs** : Steam Deck (mode jeu : gamescope, Steam Input, suspendre / reprendre),
  AMD / NVIDIA / Intel sur Linux, Wayland et X11, manettes, audio PipeWire, performances et chaleur.
  Comme pour Android, la liste de ce qu'on a vérifié ou non doit être dite dans les notes.

## 6. Gain de performance attendu : ne rien promettre

- **Ce que dit déjà le dossier Steam Deck** (`docs/perf_natif_steam_deck.md`) : `wwe12.exe` (Vulkan) parle
  presque directement au pilote AMD sous Proton ; D3D12 / D3D11 ajoutent une couche (vkd3d, DXVK). Donc,
  côté **GPU**, le natif Linux ne gagne **presque rien** sur le Vulkan sous Proton ; il supprime les couches
  pour ceux qui utilisent D3D12 / D3D11 (qu'on déconseille déjà).
- **Où le natif pourrait gagner (supposé, non mesuré)** : (1) **attentes de fils** : sous Proton le jeu utilise
  le chemin Windows du SDK (événements, `WaitMultiple`) que Wine émule (esync / fsync) ; natif utiliserait
  `pthread` / condition (déjà utilisé sur Android : les mesures Android ont montré des boucles d'attente
  coûteuses corrigées par `rexglue-sdk-android2-cpu.patch`, donc **à surveiller aussi sur Linux**) ;
  (2) **exceptions** (violations d'accès gérées par le SDK) : Wine les traduit depuis les signaux, plus cher ;
  fréquence chez nous inconnue ; (3) chemins fichiers / lecture de l'ISO ; (4) cache des shaders
  (Mesa, direct).
- **Où le natif peut perdre** : (1) le SDK en POSIX a eu moins de tests sur ce jeu (Android a trouvé des
  gels, replay et chargements infinis, **pas expliqués** : ils viennent peut-être de cette couche) ;
  (2) pas de D3D11 / D3D12 de secours (Vulkan seulement) ; (3) priorités de fils non appliquées.
- **Le goulot actuel** (mesures PC et Android) est le **CPU du fil de rendu** et le code du jeu lui-même :
  aucune des deux voies ne le change. **Le vrai gain annoncé doit rester « plus simple à lancer »**, pas
  « plus rapide ». Protocole : même Deck, mêmes réglages, Proton (Vulkan) contre natif, i/s moyens en entrées
  et en match, 3 passes chacun.

## 7. Estimation, risques, premiers pas

### Séances (une séance = une session de travail de la conversation, ordre de grandeur)

| Étape | Séances |
|---|---|
| 1. Environnement : WSL2 / VM Arch, clang récent, SDK `linux-amd64-release` compilé **avec nos patchs** | 1-2 |
| 2. `cmake/linux.cmake` + remplaçant POSIX généralisé : compile du jeu (`port/generated` copié), éditions de liens | 2-3 |
| 3. Démarrage jusqu'au menu (lavapipe / WSLg), chemins XDG, ISO par clé du `wwe12.toml`, journaux | 1-2 |
| 4. Vraies versions Linux (affinité, minuteurs, plein écran, taille d'écran, touches F2 / F9, crash) | 1-2 |
| 5. Archive `.tar.xz` + `wwe12.sh` + `.desktop` + FAQ Linux, compilation dans le conteneur sniper | 1 |
| 6. Bêta pour testeurs (Deck et PC) et corrections | 3 et plus |
| Total pour une bêta testable | **environ 8 à 12** ; **+ 3 à 5** pour un launcher ImGui (option B) ; + AppImage / AUR / Flatpak si voulu |

### Risques

1. **Éditeur de liens et très grand nombre de sections** (bloqué sur Android x86_64) : à tester en premier.
2. **Fils et attentes** : gels déjà vus sur Android ; en Linux natif, mêmes sources de risque (priorités,
   réveils perdus). Le travail sur le replay Android profite directement au Linux natif.
3. **Fixed `mmap` de l'arène invitée** et la mémoire du Deck (16 Go partagés avec le GPU) : à mesurer.
4. **Pilotes Vulkan** : fonctions exigées présentes sur Mesa récent ; vieux Mesa / pilotes libres anciens : non.
5. **Maintenance** : un troisième système (Windows, Android, Linux) à tester à chaque version ; pas de
   machine Linux ici. Le build Linux ne peut pas se faire dans un CI public (le C++ généré n'est pas public) :
   **construit en local**, comme le zip Windows.
6. **Juridique** : mêmes règles que Windows (code recompilé accepté, jamais de données du jeu) ; un paquet
   AUR ou Flathub expose le projet à une modération externe : décision de l'utilisateur.
7. **Image du projet** : pas de promesse « plus rapide » ; pas de Mod Maker Linux ; pas de D3D12 / D3D11.

### Premiers pas sans risque (rien à publier, rien à pousser)

1. **Installer WSL2 + une distribution** (ou une VM Arch) et vérifier qu'un clang 20+ y tourne : demande
   du téléchargement à l'utilisateur d'abord (taille de l'image : plusieurs Go).
2. **Compiler le SDK `linux-amd64-release`** (patchs inclus) et lancer ses tests : prouve que notre SDK
   patché sort sur Linux, sans toucher au jeu.
3. **Copier `port/generated` et compiler `wwe12` avec un `linux.cmake` brouillon** dans un dossier de test
   (hors dépôt), pour compter les erreurs (listes d'inclusions Windows restantes). Rien n'est versionné.
4. **Mesurer le nombre de sections** du binaire Linux (point de risque n° 1).
5. **Documenter** : liste de ce qu'on ne sait pas, à vérifier sur un vrai Deck (FAQ « Steam Deck » existante).
6. Décision de l'utilisateur ensuite : bêta Linux oui / non, quelle distribution (archive d'abord), qui teste.

## 8. Ce qui reste incertain

- Si le SDK patché **compile et démarre** sur Linux (aucune compilation Linux faite ici).
- Le **nombre de sections** et la taille du binaire en ELF.
- Le comportement sous **gamescope** (mode jeu du Deck) et Wayland.
- Les **performances** réelles, natif contre Proton.
- Les **exceptions** invitées : à quelle fréquence le jeu les utilise (important pour Wine comme pour le natif).
