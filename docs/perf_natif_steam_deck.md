# Performances, Steam Deck et jeu plus natif

Chantier de l'après-1.1 (1.2), demandé par l'utilisateur le 2026-11-05. Ce document est commun à Rendu et à
Gameplay ; chacun écrit sa section. **Rien n'est codé avant le go de l'utilisateur.**

## Natif / code du jeu (Gameplay, 2026-11-05, lecture seule)

Ce qui suit vient de la lecture du code généré (`port/generated`), du SDK (`recomp/rexglue-sdk`) et d'anciens
échantillons de profil. Le jeu n'a pas été lancé pour cette étude.

### 1. Les couches qui restent entre le code du jeu et le PC

Notre exe exécute le code PowerPC du jeu traduit en C++ (ReXGlue). Chaque fonction du jeu devient une fonction
C++ qui travaille sur une structure `ctx` (les registres du PowerPC) et sur la mémoire de la Xbox (`base` +
adresse). Voici les couches, de la plus coûteuse (probablement) à la moins coûteuse.

| Couche | Ce qu'elle fait aujourd'hui | Coût estimé | Idée |
|---|---|---|---|
| **Registres du jeu en mémoire** (`ctx.r11`, `ctx.f1`, `ctx.cr6`…) | Toutes les options du codegen qui gardent les registres dans de vraies variables locales sont **désactivées** chez nous : `non_volatile_as_local`, `non_argument_as_local`, `cr_as_local`, `ctr_as_local`, `xer_as_local`, `reserved_as_local` (absentes de `port/wwe12_config.toml`, défaut false dans `src/codegen/config.cpp`). Chaque registre est donc un champ de `ctx`. Comme les accès mémoire du jeu peuvent « toucher » `ctx` (voir la ligne suivante), le compilateur relit et réécrit ces champs très souvent. | **Le plus gros**, à mesurer. C'est très probablement ce que Jarrod a gagné en v0.2.4 (fil principal −15 %, exe −13 Mo). | Activer ces options dans `wwe12_config.toml`, une à une, puis refaire le codegen et une compilation complète. Pas de code à écrire, mais tout le jeu change : test complet obligatoire (setjmp/longjmp, nos ~280 crochets qui lisent `ctx` à l'entrée et à la sortie). |
| **Accès mémoire `volatile`** | `REX_LOAD_U32(x)` = `__builtin_bswap32(*(volatile u32*)(base + x + REX_PHYS_HOST_OFFSET(x)))`. Il y a environ **1,1 million** d'accès de ce genre dans le code généré. `volatile` interdit au compilateur de garder une valeur, de fusionner deux lectures ou de supprimer une écriture inutile. | Moyen à gros (il empêche aussi les optimisations de la ligne précédente). | Enlever `volatile` pour la mémoire ordinaire et le garder pour la mémoire matérielle (MMIO, déjà traitée à part par `REX_MMIO_*`). À faire dans le modèle `resources/templates/codegen/pch_h.inja` : c'est un changement du SDK, donc un patch dans `port/patches/`. Risque : du code du jeu qui « attend » une valeur écrite par un autre fil sans barrière (boucles d'attente). Il faudra les repérer. |
| **Décalage « mémoire physique »** | `REX_PHYS_HOST_OFFSET(x)` = un test `x >= 0xE0000000` à **chaque** accès (Windows et macOS ARM). | Petit : une comparaison et un choix par accès (souvent effacés quand l'adresse est connue). | Le garder seulement là où l'adresse peut être physique, ce que le codegen ne sait pas toujours. Gain modeste, à garder pour après. |
| **Inversion d'octets** (`bswap`) | Le PowerPC est gros-boutiste. | Très petit (une instruction, souvent `movbe`). | Rien à faire : on ne peut pas l'enlever sans changer les données. |
| **Mode des nombres à virgule** (`ctx.fpscr.disableFlushMode()`) | Avant les calculs à virgule, le mode « flush » du processeur est changé si besoin (`setcsr`), et remis pour les calculs vectoriels. L'appel est mis en cache, donc il ne coûte que quand on alterne vecteurs et nombres simples. | Petit à moyen dans les calculs d'animation (vecteurs et nombres simples mélangés). | À mesurer avant tout. Si ça coûte : une variante « sans changement de mode » pour les fonctions natives réécrites. |
| **Noyau émulé** (threads, sections critiques, attentes : `xboxkrnl`, `xam`) | Fonctions C++ du SDK (ex. `RtlEnterCriticalSection_entry`, avec attente active puis attente du système). | Faible : dans les anciens échantillons, environ **2 %** du temps actif du fil principal est dans `rexruntime.dll`. | Rien de prioritaire. Les attentes déjà corrigées : turbo des chargements, `wwe12_gpu_wait_yield`, minuterie du SDK. |
| **Audio XMA / XAudio** | Décodage XMA par le SDK sur ses propres fils. | Non mesuré ici ; hors du fil principal. | Pour Rendu ou le SDK, plus tard. |
| **Fichiers** | VFS du SDK, lecture par blocs de 256 Ko. | Faible (32 Mo lus en 12 ms, mesuré le 2026-10-16). | Rien. |
| **Minuteries / cadence** | Le jeu avance au rythme des images (barrière `sub_827DEA20`, vblank invité). | Ce n'est pas un coût de calcul : c'est de l'attente voulue. | Rien. |

### 2. Trouver les fonctions les plus chaudes du fil principal

**Méthode** :
1. Lancer avec `SVR_SAMPLE_STACKS=<ms>` (profil de `crash_trace.cpp`, écrit dans `Documents\wwe12\logs\samples.txt`).
   Pour plus de points, prendre un intervalle de 5 ms, sur un match lourd (6 catcheurs, Univers), 60 s au moins.
   Lanceur existant : `tools/dev/lancer_test_4fils.bat`.
2. Effacer `samples.txt` avant : le fichier s'**ajoute** à chaque lancement (celui du 28/10 contient plusieurs
   lancements mêlés).
3. Lire les adresses avec la **carte de la même compilation** (`wwe12.map`). Constat utile : les fonctions du jeu
   (`sub_8…`) gardent la même place d'une compilation à l'autre si le codegen n'a pas changé. Les échantillons du
   28/10 se lisent ainsi avec `dist/symbols-v1.0.0/wwe12.map` (contrôle : la chaîne `sub_82880D68` <
   `sub_82819828` < `sub_82826A38` du fil de rendu retombe bien). Le code du portage, lui, bouge.
4. Le fil principal du jeu est celui dont la racine est `sub_821AD2B8` → `sub_827D5380` (boucle principale) ; le
   fil de rendu du jeu, `sub_82826A38`.
5. Script : compter, pour chaque échantillon « actif » (fonction la plus profonde hors `ntdll`/`KERNELBASE`), la
   première fonction `sub_` (temps propre) et toutes les `sub_` de la pile (temps inclus).

**Ce que disent les anciens échantillons (28/10, intervalle ~40 ms, peu de points : indicatif seulement)** :
- Le fil principal n'est **actif que 6 à 14 %** du temps, le reste en attente de la barrière d'image : environ
  1 à 2,5 ms de travail par image dans ce que montrait cette capture (menus puis un peu de jeu).
- Le fil de rendu du jeu est actif environ 17 à 25 % du temps. Sa fonction la plus chaude est `sub_82872620` (tiers
  de ses points), puis `sub_82824DF0`, `sub_82880D68` (côté bibliothèque D3D du jeu).
- Fil principal, temps propre (246 points actifs) : `sub_8285E080` (29), `sub_82823598` (16), `sub_82822A48`
  (11), `sub_827D42F0` (10), `sub_821ADE30` (10), `sub_821B6460` (6), `sub_8280E160` (6), `sub_82823790` (6),
  `sub_82A76060` (5, memcpy), `sub_8285C908` (4).
- Fil principal, temps inclus : `sub_827D3A60` / `sub_827DE278` (150), `sub_82871FF0` (118), `sub_8285E080` (115),
  `sub_82209C48` / `sub_822FFD28` (69), `sub_823001C0` (54), `sub_822132F8` (40), `sub_825BAC70` (32).
- Les fonctions d'animation des os (`sub_8248CE10`, `sub_8248D528`, `sub_8248B898`, `sub_8248B458`) n'apparaissent
  **dans aucun** échantillon de cette capture.

**Ce top 10 n'est pas fiable pour décider** : trop peu de points, pas de match lourd identifié. Il faut **une
mesure neuve** (un lancement, avec le go de l'utilisateur) : match à 6, 60 s, 5 ms, `samples.txt` effacé avant,
même compilation que la carte.

### 3. Réécrire des fonctions en natif (comme Jarrod pour la déformation des catcheurs)

Méthode de vérification commune : garder l'original recompilé. Une cvar choisit natif / original. Un mode
« comparaison » appelle les deux sur une copie de l'état et compare les octets écrits (mémoire du jeu et `ctx` de
sortie), image par image, en journalisant le premier écart. Identique bit à bit veut dire : même ordre des calculs
à virgule (pas de `-ffast-math`, pas de FMA si le jeu n'en fait pas), mêmes arrondis, même mode flush.

| Candidat | Ce qu'on sait | Difficulté | Gain probable |
|---|---|---|---|
| Échantillonneurs d'animation `sub_8248B898` (clés 16 bits en deltas), `sub_8248B458` (8 bits), rotation `sub_8248CE10` | Déjà bien compris (correctif de ceinture). De petites fonctions de calcul pur, appelées pour chaque os de chaque catcheur à chaque image. | **Moyenne** : peu d'appels sortants, entrées / sorties claires (fiche d'os de 80 octets). Le plus délicat : refaire exactement les calculs à virgule. | **Inconnu** : ces fonctions n'apparaissent pas dans les anciens échantillons. À vérifier par la nouvelle mesure avant d'écrire une ligne. |
| Déformation (« skinning ») des catcheurs sur le CPU, la piste de Jarrod | Sur la Xbox, la déformation est en grande partie faite par le GPU (vertex shaders). Une partie CPU existe peut-être (vêtements, cheveux, physique). À trouver dans la nouvelle mesure. | Inconnue tant que la fonction n'est pas trouvée. | Chez Jarrod, −40 % sur ce fil. Chez nous, à mesurer. |
| `sub_82872620` et voisines (fil de rendu du jeu, bibliothèque D3D de la Xbox) | La plus chaude des anciens échantillons. Elle construit les commandes pour le GPU de la Xbox, que le SDK relit ensuite. | **Difficile** : elle touche aux formats de commandes du GPU Xenos. | Possible, mais c'est la frontière avec le travail de Rendu (remplacer le chemin D3D invité par des appels natifs). À décider avec Rendu. |

Recommandation : d'abord le point 4 (registres locaux, sans réécrire le jeu à la main), puis la nouvelle mesure,
puis 1 ou 2 réécritures natives choisies sur cette mesure.

### 4. Le code généré : registres et taille de l'exe

- Notre SDK (ReXGlue v0.10.0, c94f5eb + nos patchs) **sait déjà** mettre les registres en variables locales
  (`src/codegen/builders/context.cpp` : `r()`, `f()`, `v()`, `cr()`, `ctr()`, `xer()`). Il suffit des options dans
  `wwe12_config.toml`. Une limite du SDK : pas de locaux pour une fonction qui partage ses registres
  (`sharesRegisters()`, gestionnaires d'exception).
- Ce qu'il faut lire chez Jarrod (WWE 2K14, v0.2.4) pour savoir s'il a fait plus que ces options : ses patchs du SDK
  (codegen et modèle `pch_h.inja`). En particulier : `volatile` retiré ou non, ses réglages `*_as_local`, et s'il a
  fusionné la vérification `REX_PHYS_HOST_OFFSET`. **Télécharger ses fichiers = go de l'utilisateur ; reprendre son
  code = demander à Jarrod** (pas de licence connue : voir la mémoire « Jarrod permission »).
- Plan d'essai proposé (après go) :
  1. Une compilation avec `cr_as_local`, `ctr_as_local`, `xer_as_local`, `reserved_as_local` (risque faible) ;
     mesurer.
  2. Ajouter `non_argument_as_local` puis `non_volatile_as_local` ; mesurer à chaque pas. Vérifier nos crochets :
     un crochet qui lit `ctx.r31` après l'original pourrait ne plus voir la bonne valeur si l'original la gardait
     en local.
  3. Le patch du SDK sans `volatile`.
  - Mesures : taille de l'exe, `cpu per frame` de Rendu, temps actif du fil principal (échantillons).

## Steam Deck et performances (Rendu, 2026-11-05, lecture seule)

Rien n'a été lancé pour cette étude. Seul ajout : l'outil `tools/samples_top.py`, qui lit un profil
sans rien lancer.

### Ce que disent nos mesures récentes

- **Ligne `[perf] cpu per frame`** (1.1, commit ff941e5) : dans une scène chargée, le fil de rendu
  prend **1,1 ms** sur 6,2 ms de « work ». Le reste est l'attente de l'image du jeu. **Le goulot
  est le code du jeu recompilé, pas notre rendu.**
- **#7** (Intel UHD 620, portable 15 W) : entrées à ~27 i/s pour 30 visées, match à ~50 i/s pour
  60. « gpu wait » vaut ~0 : c'est le CPU.
- **Shaders préparés en plein match** : réglé en 1.1 (c442aa3). Sur RTX 5070, même match :
  65 → 0 pipelines compilés sur le fil de rendu.
- Sur un portable 15 W, le CPU et le GPU partagent la même puissance. Chaque milliseconde de CPU
  gagnée laisse aussi plus de marge au GPU, et l'inverse.

### Steam Deck : le matériel et le chemin du jeu

- **Puce :** AMD « Van Gogh ». CPU Zen 2, 4 cœurs / 8 fils, 2,4 à 3,5 GHz. GPU RDNA 2, 8 unités.
  Enveloppe partagée de 4 à 15 W, 16 Go partagés.
- **Écran :** 1280x800, en **16:10**.
- **Système :** SteamOS (Linux). Notre exe Windows passe par **Proton** (Wine) :
  - `wwe12.exe` (Vulkan) parle presque directement au pilote AMD de Linux (RADV) : **c'est le
    meilleur choix sur Deck** ;
  - `wwe12_d3d12.exe` passe par vkd3d-proton, une couche de plus ;
  - `wwe12_d3d11.exe` passe par DXVK, une couche de plus.
- Sous Windows (Deck réinstallé), c'est un PC AMD normal.

### Steam Deck : ce qu'on sait déjà

- **Aucune issue GitHub ouverte ne parle du Deck** (vérifié dans `work/issues_open.json`). La FAQ
  le cite seulement pour les pavés de couleur (réglés en 0.9, 2026-10-20).
- **Anciens retours AMD :**
  - **#4** : gel pendant une entrée sur RX 560 (0.6) ;
  - « cel-shading » sur Deck, probablement la fuite de mémoire vidéo réglée en 0.7 : **à faire
    confirmer** par un joueur Deck en 1.0 ou 1.1.
- **Les pièges que Kaiko a rencontrés sur Deck avec son SvR 2011** (même famille de rendu), et
  notre état :

| Chez Kaiko | Chez nous |
|---|---|
| Shaders traduits avec des `switch` à cas enchaînés : sous Proton, le pilote du Deck refusait leurs pipelines (1a4d023) | Nos 726 shaders HLSL ne contiennent ni `switch` ni `while (true)` (vérifié dans `work/shaders/d3d11/hlsl`) : sans doute pas concernés. Dans un journal Deck, ce serait des lignes `PSO build failed` ou `draw dropped`. |
| Zip avec des `\` : le gestionnaire d'archives du Deck (Ark) en faisait des noms de fichiers (cada307) | Notre zip 1.0 utilise des `/` (vérifié : 0 nom avec `\` sur 46) : OK. |
| Match gagné qui ne finit jamais : l'étape des moments forts attend l'enregistreur de replay (c43d482) | À regarder par Gameplay : notre **#51** (Android, Adreno 720 : bloqué après un match 1 contre 1) ressemble beaucoup à ce cas. |
| Vidéos du titantron sans le processeur vidéo de Windows sous Proton (1c464b5) | Pour le Mod Maker (Mods) seulement : le jeu lit les Bink lui-même. |

### Steam Deck : ce qu'un joueur a probablement encore

1. **Performances :** 4 cœurs Zen 2 à 15 W, c'est la classe du portable de #7. Il faut donc
   s'attendre à des entrées un peu sous 30 i/s et des matchs autour de 45 à 55 i/s, limités par le
   CPU. Voir « Performances » ci-dessous et la section Natif.
2. **Saccades :** réglées en 1.1 pour les pipelines connus (c442aa3). Avec 8 fils, la préparation
   utilise 2 fils en arrière-plan. Le cache du pilote (sous Proton, `steamapps/shadercache`) aide
   encore à partir de la 2e partie.
3. **Batterie :** notre attente du GPU dort au lieu de tourner en boucle depuis le 2026-10-31
   (`wwe12_gpu_wait_sleep_us`, −12 % de CPU). Une limite à 30 i/s n'est pas possible : le jeu est
   cadencé sur sa propre horloge à 60 et ralentirait.
4. **Image 16:10 :** `wwe12_widescreen` remplit le haut et le bas. Elle n'est pas encore
   entièrement vérifiée (notes de la 1.1). La résolution interne 720p suffit à 800p.
5. **Contrôles :** Steam Input présente une manette Xbox : icônes Xbox, A/B/X/Y aux bons endroits.
   Rien à faire.
6. **Chargements :** une ISO sur carte microSD se lit plus lentement. Nos lignes `[load] … read`
   le montreront.

### Steam Deck : un test sans avoir l'appareil

Demander à un joueur Deck (Discord) :
1. Exe Vulkan (`gpu_backend = "vulkan"`), 720p, réglages par défaut, version de Proton notée.
2. Un match d'exhibition 1 contre 1 avec entrées, puis le même match une 2e fois.
3. Renvoyer `Documents\wwe12\logs\wwe12.log` (sous Proton :
   `steamapps/compatdata/<id>/pfx/drive_c/users/steamuser/Documents/wwe12/logs`).

Ce qu'on y lit :
- `GPU caps:` : le nom du pilote (RADV VANGOGH attendu) ;
- `[perf] … work / gpu wait` : CPU ou GPU ;
- `[perf] cpu per frame` : notre rendu ou le jeu ;
- `[load] … pipelines N` : saccades restantes ;
- `pso boot:` : préparation au démarrage ;
- `PSO build failed` : pipelines refusés par le pilote.

Idée reprise de Jarrod : un **test de performance intégré** (deux entrées et un début de match,
deux fois) qui écrit un résumé chiffré. Les joueurs auraient alors des chiffres comparables.

### Performances : les points restants de `optimisation_jarrod.md`

Faits : 1 (read-back 32 images), 2 (attente du GPU qui dort), 3 (pipelines au démarrage,
c442aa3). Classement des autres pour un **portable 15 W limité par le CPU** :

| Rang | Point | Gain sur petit PC | Risque | Séances | Pourquoi ce rang |
|---|---|---|---|---|---|
| 1 | **Nouveau : fonctions de copie de mémoire du jeu en natif** (`sub_82A76060` memcpy, `sub_82A76590` memset) | À mesurer, sans doute réel (fonctions appelées partout, code PowerPC traduit instruction par instruction) | **Faible** : même résultat octet pour octet | 1 | C'est le cas le plus simple du point 10. Aujourd'hui, `d3d_census.cpp` ne fait que passer l'appel à l'original. |
| 2 | **10. Fonctions chaudes du jeu réécrites en natif** (déformation des catcheurs chez Jarrod, −40 % sur ce fil) | Grand si on vise juste | Élevé : chaque fonction doit donner exactement le même résultat | 2+ par fonction | Le goulot est le code du jeu. Voir la section Natif. |
| 3 | **6. Code recompilé qui garde plus de registres** (Jarrod : fil principal −15 %) | Grand, partout | **Élevé** : touche tout le code du jeu, tout à retester | 3-4 | Le plus gros gain général. Voir la section Natif, partie 4. |
| 4 | **4. Image en retard affichée tout de suite** | Moyen : une image de 18 ms n'attend plus jusqu'à 33 ms | Moyen (déchirures si mal réglé) | 1-2 | Utile exactement là où le Deck et #7 sont : juste au-dessus de 16,7 ms. |
| 5 | **8. Shader de post-effets réécrit** | Moyen sur iGPU faible (HD 4000, Deck en 4 W) | Moyen | 2-3 | Seulement quand le GPU limite. À mesurer d'abord avec RenderDoc. |
| 6 | **7. Foule dessinée par groupes** | **Faible aujourd'hui** | Moyen | 2-3 | Il allège notre fil de rendu, qui ne prend que ~1 ms. |
| 7 | **5. Copies et redessins inutiles sautés** | Faible (GPU, quelques %) | Moyen | 1-2 | Idem : pas notre goulot. |
| 8 | **9. Constantes en mémoire GPU sans ReBAR** | Faible, vieilles cartes | Faible | 1 | |

### Outil : `tools/samples_top.py`

Il lit le `samples.txt` de `SVR_SAMPLE_STACKS` (`crash_trace.cpp`, lanceur
`tools/dev/lancer_profil.bat`) avec la carte de l'exe. Il donne, fil par fil, les fonctions où
tombent les échantillons (« self ») et celles présentes dans la pile (« total »), avec des fenêtres
de temps :

    python -I tools/samples_top.py samples.txt dist/symbols-v1.0.0/wwe12.map --top 10
    python -I tools/samples_top.py samples.txt port/out/build/win-amd64-release/wwe12.map --thread <nom> --from 60000 --to 120000

Les fonctions recompilées gardent leur adresse Xbox 360 dans leur nom (`sub_82AA7040`). Testé sur un
faux profil ; un vrai profil demande un lancement, donc le go de l'utilisateur.

### Proposition de Rendu (après la 1.1)

1. **Une séance de mesure :** profil 4 fils, entrée et match → tableau des 10 fonctions chaudes.
   Tout de suite après, memcpy / memset du jeu en natif (petit, sûr, mesurable).
2. **Avec Gameplay :** les 2 ou 3 premières fonctions chaudes en natif, une par une.
3. **Point 4** (image en retard) pour les petits PC et le Deck.
4. **Test Deck :** demander un journal à un joueur Deck (plan ci-dessus) dès la sortie de la 1.1.
