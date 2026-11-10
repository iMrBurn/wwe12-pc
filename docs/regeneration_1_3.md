# Régénération du code du jeu (1.3) : registres locaux + « volatile »

Étude de Rendu, 2026-11-10, **lecture seule** (rien compilé, rien lancé). Pour décision de l'utilisateur.
Bases : `docs/perf_natif_steam_deck.md` (lignes « Registres du jeu en mémoire » et « Accès mémoire volatile »,
section 4) et `docs/plan_1_2_codegen_tu.md` (plan de Gameplay, écrit quand la TU devait aussi changer le code).
Depuis, la TU passe par nos crochets (voie B) : la régénération ne sert plus **qu'à la vitesse**.

## 1. Ce que ça change

Le jeu Xbox 360 est traduit automatiquement en C++ (`port/generated`, 256 Mo, **régénéré pour la dernière fois
le 2026-10-10**). Deux choses ralentissent ce C++ :

- **Les registres du processeur Xbox sont des champs d'une structure en mémoire** (`ctx.r11`, `ctx.f1`,
  `ctx.cr6`…). Le compilateur du PC doit les relire et les réécrire en mémoire sans arrêt, au lieu de les garder
  dans ses propres registres.
  - Le générateur sait déjà faire mieux : six réglages de `port/wwe12_config.toml`, tous **coupés** aujourd'hui :
    `cr_as_local`, `ctr_as_local`, `xer_as_local`, `reserved_as_local` (risque faible), `non_argument_as_local`,
    `non_volatile_as_local` (le plus gros gain, le plus de risque).
  - Pas de code à écrire : des lignes de réglage, puis tout régénérer.
- **Chaque accès à la mémoire du jeu est marqué `volatile`** (~1,1 million d'accès). Ce mot interdit au
  compilateur de garder une valeur déjà lue, de fusionner deux lectures ou d'enlever une écriture inutile.
  - Le retirer = un patch du SDK (modèle `pch_h.inja`, dans `port/patches/`), en le gardant pour la mémoire
    matérielle et pour les boucles d'attente entre fils du jeu.
  - Risque : une boucle du jeu qui attend qu'un autre fil écrive une valeur, sans barrière. Sans `volatile`, elle
    peut ne plus jamais voir la nouvelle valeur (gel). Il faut les repérer dans le code généré.

**Gain mesuré par Jarrod** (WWE 2K14, même générateur, v0.2.4) : fil principal du jeu **−15 %**, exe −13 Mo.
**Gain espéré chez nous** : du même ordre sur le code du jeu, soit 10 à 20 %.
- Sur le i7 du PC : le jeu prend ~17-18 ms de CPU par image en match (benchmark du 2026-11-10). Il en resterait
  ~14-16 ms. Pas de différence visible : le i7 tient déjà 60.
- Sur le Ryzen 3 5300U de l'issue #53 : ~36-43 ms de CPU par image dans les matchs à 4-6. Il en resterait
  ~31-37 ms. **Ça aide mais ne suffit pas seul pour 60 partout** ; c'est le seul gain « partout à la fois » qui
  reste.

## 2. Nos crochets à revoir

Un crochet remplace une fonction du jeu et peut lire les registres (`ctx`) avant et après avoir appelé l'original.
Avec les registres locaux, les registres « non-arguments » que l'original modifie ne sont plus recopiés dans `ctx`.
Un crochet qui les lit **après** l'original pourrait donc lire une ancienne valeur.

Comptage automatique dans `port/src` (2026-11-10) :
- 39 fichiers de crochets, environ 1 900 fonctions crochetées (beaucoup sont générées par des macros : D3D, fps).
- **12 crochets seulement** lisent après l'original un registre autre que le résultat (`r3` / `f1`), dans
  9 fichiers : `wwe12_arena_pages.cpp`, `wwe12_kind_probe.cpp`, `wwe12_move_events.cpp`, `wwe12_native_mem.cpp`,
  `wwe12_screen_probe.cpp`, `wwe12_superstar_mods.cpp`, `wwe12_title_update.cpp`, `wwe12_universe.cpp`,
  `wwe12_wait_probe.cpp`.
  - Registres lus : `r4` (8 fois), `lr` (6), `r5` (5), `r6`, `f30`, `r25`, `r28`, `r31` (1 chacun).
  - **Un seul** lit un registre « non volatile » (`r25`, `r28`, `r31` : ceux que `non_volatile_as_local` rend
    locaux). C'est le cas le plus sûr à casser : à réécrire pour lire la valeur **avant** l'appel.
  - Les lectures de `r4`-`r6` après l'appel lisent déjà, sur la console, une valeur que l'original a pu écraser :
    à vérifier une par une (souvent elles veulent l'argument d'entrée, à garder avant l'appel).
- Aucun crochet au milieu d'une fonction (« midasm ») dans `wwe12_config.toml` : rien à revoir de ce côté.
- **Crochets qui reconnaissent leur appelant par `ctx.lr`** (contrôle demandé par Gameplay, 2026-11-10) : correctifs
  THQ et Univers (`sub_82A330D8`, `sub_826D0598`, `sub_826CC0B8`) et `wwe12_move_events.cpp`. Aujourd'hui, le
  code généré écrit `ctx.lr = 0x…;` juste avant chaque appel. Aucun des six réglages ne concerne `lr`, mais il faut le
  **vérifier sur une fonction régénérée** (lire le C++ produit) avant de compiler. Si `lr` n'était plus écrit, ces
  correctifs cesseraient de s'appliquer **sans le dire**. Au gros test : les lignes de journal `[tu]` attendues
  doivent apparaître comme avant la régénération (comparer un même parcours avant / après).
- **Sonde de Gameplay sur `sub_822769C8`** : elle lit `r25` / `r28` / `r31` de l'appelant dans `ctx`. Elle est
  coupée par défaut (pas de risque pour le jeu), mais elle ne donnerait plus les bonnes valeurs avec
  `non_volatile_as_local` : à refaire si on en a encore besoin.
- Les sondes (`*_probe.cpp`) sont coupées par défaut : à revoir quand même, mais elles ne touchent pas les joueurs.

## 3. Plan de test et retour en arrière

**Pas de réglage dans le jeu pour revenir en arrière** : le code généré est dans l'exe. Retour en arrière =
remettre l'ancien `port/generated` (copie de sauvegarde de 256 Mo avant la régénération, hors dépôt) et
recompiler, ou garder la dernière version publiée. Les joueurs, eux, peuvent toujours reprendre la version
précédente sur GitHub.

Paliers internes (aucun n'est publié) : chaque palier = régénération + compilation + benchmark.
1. `cr_as_local`, `ctr_as_local`, `xer_as_local`, `reserved_as_local`.
2. + `non_argument_as_local`.
3. + `non_volatile_as_local`.
4. + patch `volatile`.

Si un palier casse, on s'arrête au précédent.

Mesures avant / après (le benchmark est maintenant automatique) :
- benchmark 1 contre 1 avec entrées, 3 fois sur chaque config : iGPU Intel, NVIDIA, 4 fils (`wwe12_dev_affinity F`) ;
- comparer `cpu_game_ms`, `cpu_render_ms`, `match_p1_ms`, `match_frames_over_51ms` du bloc `[results]` ;
- taille de l'exe.

**Le gros test final de l'utilisateur** (~1 h 30 à 2 h de jeu, une seule fois, sur la version du dernier palier
qui tient) :
- menus, options, OPTIONS > GRAPHICS ;
- 1 contre 1, match à 6 (HIAC), match par équipe, avec entrées et replay d'après-match ;
- Univers : une semaine complète avec émissions, EDIT BRAND avec CAW (TU de Gameplay) ;
- Create a Superstar (moveset, tenue Superstar Threads), Create an Entrance ;
- catcheurs et arènes ajoutés par le Mod Maker, DLC ;
- les 3 exe (Vulkan, D3D12, D3D11), et le benchmark en 4 fils (petit PC) ;
- Android : compilation au moins (Main / Android).
- Ce qu'on cherche : gel, plantage, prise ou animation fausse, chargement infini.

## 4. Ce qui doit être posé AVANT

- **La vague THQ de Gameplay finie et testée en jeu** : ses crochets entrent dans la liste du point 2, et un bug
  après la régénération doit pouvoir être attribué à l'une ou l'autre.
- Les autres changements de la 1.3 compilés et testés (chargements, démarrage, benchmark, netteté : faits).
- Une version de référence publiée ou gardée de côté (pour comparer, et pour revenir en arrière).
- L'accord de Gameplay (son plan d'origine) et celui de l'utilisateur.
- Facultatif : lire les patchs du SDK de Jarrod (registres, volatile), pour savoir s'il a fait plus que ces
  réglages. Téléchargement = accord de l'utilisateur ; reprise de code = accord de Jarrod.

## 5. Estimation et risque

| Étape | Heures (hors temps machine) |
|---|---|
| Revue des 12 crochets + lecture du code généré (comment les registres sont rendus aux appels) | 2-3 |
| Patch `volatile` + repérage des boucles d'attente du jeu | 3-4 |
| 4 paliers : régénération, compilation (~1 h de machine chacun, à confirmer), benchmark | 3-4 |
| Gros test de l'utilisateur + analyse des journaux | 2-3 |
| **Total** | **10-14 h**, en 2-3 séances |

**Risque : élevé** sur tout le jeu (le code de toutes les fonctions change), mais **contrôlé** par les paliers,
le benchmark avant / après et la copie de sauvegarde. Le palier 1 est presque sans risque. Les paliers 3 et 4
sont ceux qui peuvent faire geler ; on peut s'arrêter avant.

**Recommandation** : faire au moins les paliers 1 et 2 en fin de 1.3, après la vague THQ. Ne garder les paliers 3-4
que si le benchmark montre un vrai gain et que le gros test passe.
