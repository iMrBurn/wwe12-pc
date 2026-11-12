# Performances, Steam Deck et jeu plus natif : anciennes parties (archivées le 2026-11-12)

Parties finies ou dépassées de `docs/perf_natif_steam_deck.md`, recopiées telles quelles. L'état actuel est en haut de
ce fichier-là.

Ancienne introduction : « Chantier de l'après-1.1 (1.2), demandé par l'utilisateur le 2026-11-05. Ce document est commun à Rendu et à
Gameplay ; chacun écrit sa section. **Rien n'est codé avant le go de l'utilisateur.** »

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

### Proposition de Rendu (après la 1.1)

1. **Une séance de mesure :** profil 4 fils, entrée et match → tableau des 10 fonctions chaudes.
   Tout de suite après, memcpy / memset du jeu en natif (petit, sûr, mesurable).
2. **Avec Gameplay :** les 2 ou 3 premières fonctions chaudes en natif, une par une.
3. **Point 4** (image en retard) pour les petits PC et le Deck.
4. **Test Deck :** demander un journal à un joueur Deck (plan ci-dessus) dès la sortie de la 1.1.

### Test de performance intégré (Rendu, 2026-11-06, écrit, PAS encore compilé ni testé)

Demande de l'utilisateur (go du 2026-11-06), idée de Jarrod : des chiffres comparables des joueurs (Steam Deck, petits
PC), sans personne à la manette.

- **Lancement :** `tools/dev/lancer_benchmark.bat [exe]`. Il efface puis crée `Documents\wwe12_benchmark` (dossier de
  données neuf : curseurs des menus par défaut, la sauvegarde du joueur n'est jamais touchée) et lance l'exe avec
  `--user_data_root … --wwe12_benchmark=true`. Sur Steam Deck : les mêmes options de lancement dans Steam. Un bouton du
  launcher pourra faire la même chose (à voir avec Launcher).
- **Déroulé (`wwe12_benchmark.cpp`, étapes de la télécommande `wwe12_remote_pad.cpp`) :** A toutes les 2,5 s depuis
  l'écran-titre jusqu'au début d'une entrée (le match d'exhibition par défaut), les entrées, puis
  `wwe12_benchmark_match_s` (30) secondes de match. Passe 1 = caches froids. Le jeu se relance ensuite tout seul
  (`--wwe12_benchmark_pass=2`, même dossier) : passe 2 = caches chauds.
- **Résumé :** `logs\benchmark_<date>_<heure>.txt` dans ce dossier, en anglais : version, processeur, nombre de fils,
  carte, API, Windows ou Proton, sortie, échelle de rendu, réglages ; pour chaque passe, entrées et match : i/s moyen
  et cible, temps d'image moyen / 1 % pires / pire, « work », « gpu wait », CPU par image (fil de rendu, jeu entier),
  pipelines compilés en plein dessin, temps de l'écran-titre à l'entrée et des écrans de chargement ; verdict
  (« OK: 60 fps in matches », « limited by the processor: 45 fps », « limited by the graphics card or the display »).
  Rien n'est envoyé : le joueur colle le fichier. Aucune donnée du jeu (aucun nom).
- **Limites connues :** les catcheurs et l'arène sont ceux du match par défaut du jeu (mêmes pour tous les joueurs sans
  mods, puisque le dossier est neuf) ; des mods qui changent ce choix changent la scène. Imposer catcheurs et arène
  demande de comprendre la préparation d'un match (Gameplay). Le dossier neuf réinstalle les DLC du dossier `dlc` à
  chaque test.
- **À vérifier au premier essai (go de l'utilisateur) :** que « A toutes les 2,5 s » mène bien de l'écran-titre d'un
  dossier neuf jusqu'à un match (premier démarrage : création de la sauvegarde) ; la relance de la passe 2 ; le résumé.

## Mode 30 fps (Rendu, partie PC, 2026-11-06, étude, pour la 1.2)

Demande : issue #57 et décision de l'utilisateur du 2026-11-06. Le mode doit exister aussi sur PC (petits PC,
portables, Steam Deck). Android étudie la partie téléphone ; le code de rendu est le même.

### Ce que le jeu sait déjà faire

Le jeu a **son propre mode 30 fps**. Il l'utilise pour les entrées et les cinématiques : `SetFrameRate`
(`sub_82827918`, r3 = 60/50/30/25), « passer à 30 » (`sub_82827AB8`), « revenir à 60 » (`sub_82827B58`), struct
0x8324CE28 (fps visé, pas de temps de la logique, ms par image ; voir `wwe12_fps.cpp`). Dans ce mode, **la
logique avance de deux fois plus par image**, et le jeu attend 2 vblanks par image : la vitesse reste juste. C'est
déjà ce qui se passe à chaque entrée.

`wwe12_60fps` fait l'inverse (il garde 60 quand le jeu demande 30). Le mode 30 fps imposerait 30 **partout**,
matchs compris : quand le jeu demande 60 (50), on lui donne 30 (25).

### Où couper sur PC : deux voies

| Voie | Ce qu'elle fait | Gain sur un petit PC | Risques |
|---|---|---|---|
| **A (conseillée) : le mode 30 du jeu, partout** | Crochet de `SetFrameRate` : 60 → 30 (50 → 25) ; « revenir à 60 » ne fait plus rien ; au démarrage, un appel à `SetFrameRate(30)` si le jeu ne passe pas par là | **CPU ET GPU ÷ 2 environ** : le jeu calcule et dessine 30 images au lieu de 60. C'est le bon levier, puisque nos mesures montrent que le code du jeu limite (`cpu per frame`). | Le jeu n'a sans doute jamais tourné en match à 30 sur Xbox : à vérifier en jeu (prises, contres et fenêtres de pression, physique des objets, replays, sauvegarde des moments forts, chronos, IA). Le rendu ne change pas : chaque image est dessinée en entier, donc read-backs, cuisson des tenues (`svr_attire_bake_check`) et cibles de travail restent justes. |
| B : la logique à 60, une image sur deux non dessinée | Les dessins d'une image sur deux sautés, présent tous les 2 vblanks | **GPU ÷ 2, CPU presque inchangé** : le fil de rendu ne prend que ~1 ms, c'est la logique à 60 qui coûte. Peu utile sur nos petits PC limités par le processeur. | **Élevés** : sauter une image casse ce que le jeu dessine pour lui-même (cuisson des tenues, peau, ombres, read-backs relus par le CPU) ; il faudrait trier « scène » et « cibles de travail » à chaque dessin. |

**Conseil : voie A.** C'est aussi celle qui répond à « jeu au ralenti » sur Android (#57), où le processeur limite.
B ne servirait qu'aux PC limités par la carte graphique, où la résolution interne (540p / 480p / 360p) fait déjà le
travail.

### Réglage et interface

- **wwe12.toml :** une seule clé pour la cadence, plutôt que deux interrupteurs qui se contredisent :
  `wwe12_frame_mode = "original" | "30" | "60_entrances"` (`wwe12_60fps = true` lu comme `"60_entrances"` pour les
  anciens fichiers). `"30"` = voie A.
- **OPTIONS > GRAPHICS > ADVANCED** (page pleine : 5 lignes, voir `rendu-pitfalls`) : la ligne « 60 FPS ENTRANCES »
  (2 valeurs) devient « FRAME RATE » à 3 valeurs : `30 (LOW-END)`, `ORIGINAL`, `60 ENTRANCES`. Aucune ligne de plus.
  Il faut 2 textes nouveaux dans la table du menu (comme « GRAPHICS »), puis régénérer les pages avec
  `tools/graphics_pages.py` (`TABS`, ADVANCED : `[4, 3, 3, 2, 2]`).
- **Launcher :** la même liste à 3 choix (conversation Launcher), et `"30"` dans le profil Steam Deck / petit PC
  (`machine_profile.h`), si les tests en match sont bons.
- **Test de performance intégré :** le résumé note la cadence (cible 30 ou 60) et compare à la bonne cible. Une
  passe en mode 30 se lance avec `--wwe12_frame_mode=30` ajouté à `lancer_benchmark.bat` (sans autre changement).

### Coordination avec Android

Même code (`wwe12_fps.cpp` est commun) : la voie A marche telle quelle sur téléphone. Android garde ses propres
mesures, son profil LOW-END PHONE et l'interface de son launcher.

### Estimation (heures de travail, tests en jeu à part)

| Tâche | Heures |
|---|---|
| Crochets de cadence (voie A), cvar `wwe12_frame_mode` + lecture de l'ancienne `wwe12_60fps` | 1,5 - 2 |
| Ligne FRAME RATE à 3 valeurs (2 textes, régénération des pages, enregistrement dans wwe12.toml) | 1,5 - 2 |
| Test de performance : cible selon la cadence | 0,5 |
| Launcher : liste à 3 choix + profil Steam Deck (conversation Launcher) | 1 |
| **Total** | **4,5 - 5,5 h** |

Tests en jeu nécessaires (avec l'utilisateur) : un match complet à 30 (prises, contres, finisher, sortie du ring,
objets), un replay et les moments forts, une entrée, un match d'Univers, un Royal Rumble. Puis un test de performance
sur un petit PC ou en 4 fils (attendu : le double de marge CPU par image).

## Test de performance « à la 2K » : un seul match imposé (Rendu, 2026-11-06, étude, pour la 1.2)

Idée de l'utilisateur (via Launcher et Main) : comme les benchmarks des WWE 2K récents, pas d'entrée, UN match imposé
(souvent un Hell in a Cell à 6 : s'il tient 60 là, il tient partout), rien à choisir, ~1 min 30, des résultats
comparables d'un PC à l'autre. Aujourd'hui (85441d9), le joueur choisit lui-même le match.

### Comment imposer le match : deux voies

| Voie | Principe | Heures (hors tests) | Risques |
|---|---|---|---|
| **1 (conseillée d'abord) : un trajet fixe dans les menus** | Sur un dossier de données neuf, les curseurs des menus sont toujours au même endroit. L'utilisateur a trouvé le début : `START, LEFT, A, A, A, B` → menu principal. On écrit la suite une fois (EXHIBITION → HELL IN A CELL → 6 catcheurs → 6 cases de la grille → arène → valider), avec les mots déjà en place (`until<état>`, attentes) et des captures `shot` pour la vérifier écran par écran, sans personne devant. | 3 - 4 | Les **mods** changent la grille des catcheurs et la liste des arènes : le test les coupe pour sa durée (`wwe12_superstar_mods`, `wwe12_entrance_mods`, et l'overlay du dossier `mods` à couper aussi, +0,5 - 1 h). Les DLC installés aussi : le dossier neuf n'a que ceux du dossier `dlc` du joueur, qui peuvent différer ; à vérifier (les DLC ajoutent des cases à la fin de la grille ?). Un patch de menu du jeu qui déplace une ligne casse le trajet. |
| 2 : préparer le match sans menu | Remplir la configuration du match (catcheurs, arène, type, nombre) puis appeler le chargement, comme les modes du jeu le font (`sub_82783808` prépare un match d'un autre mode, `sub_827829B0` = règles du match ; l'Univers remplit ses cartes : `wwe12_universe.cpp`). | 8 - 15, incertain | C'est le travail de Gameplay : trouver la structure de la configuration et la fonction qui lance le chargement. Une fois faite, elle sert aussi ailleurs (lancer un match de test directement). |

**Conseil :** voie 1 pour la 1.2, avec la voie 2 étudiée en parallèle par Gameplay, qui la remplacera quand elle
existera. Même résumé dans les deux cas.

### Scénario et mesures

- **Scénario :** dossier neuf, mods coupés, trajet fixe jusqu'à un Hell in a Cell à 6 dans une arène du jeu. Les
  entrées sont passées (START pendant l'entrée, ou option des entrées du jeu si elle existe). Puis 60 s de match
  mesurées sans toucher à la manette. **Une seule passe** (~1 min 30 de mesure, plus le chargement) ; la 2e passe
  (caches chauds) devient une option (`wwe12_benchmark_pass2`).
- **Mesures** (inchangées) : i/s moyen, 1 % pires, pire image, work, gpu wait, CPU par image (fil de rendu, jeu
  entier), shaders compilés au dessin, temps de chargement, verdict.

### Format du résumé pour le launcher

Le texte lisible reste en haut. Un bloc à la fin se lit facilement par le launcher (une clé par ligne, pas de
texte libre à analyser) :

    [results]
    version=1.2.0
    scenario=hiac6            (ou "player_match" pour le test actuel)
    cpu=Intel(R) Core(TM) i7-14700HX
    threads=28
    gpu=NVIDIA GeForce RTX 5070 Laptop GPU
    api=Vulkan                (Vulkan | D3D12 | D3D11)
    platform=Windows          (ou "Wine / Proton 9.0")
    output=1920x1080
    render_scale=auto         (ou 360 / 480 / 540 / 720 / …)
    frame_mode=original       (original | 30 | 60_entrances)
    match_fps=60.0
    match_target_fps=60
    match_p1_ms=18.0
    match_worst_ms=18.7
    match_work_ms=3.9
    match_gpu_wait_ms=0.0
    cpu_render_ms=3.8
    cpu_game_ms=13.3
    pipelines_while_drawing=0
    load_s=12.2
    verdict=ok                (ok | cpu | gpu | failed)

Le bouton « réglages conseillés » du launcher peut s'appuyer sur `verdict`, `match_fps` et `render_scale` : `cpu` →
mode 30 fps (quand il existera) et foule réduite ; `gpu` → résolution interne plus basse ; `ok` → rien.

### Estimation (heures, tests en jeu à part)

| Tâche | Heures |
|---|---|
| Trajet fixe jusqu'au HIAC à 6 (avec captures de contrôle) | 3 - 4 |
| Mods coupés pendant le test | 0,5 - 1 |
| Une seule passe, entrées passées, 60 s de match | 1 |
| Bloc `[results]` dans le résumé | 0,5 |
| **Total voie 1** | **5 - 6,5 h** |
| Voie 2 (Gameplay, préparation du match sans menu) | 8 - 15 h, à part |

### Match imposé du benchmark : préparation du match (Gameplay, 2026-11-06, lecture seule, pour la 1.2)

Rien codé, rien lancé. Faits lus dans `misc.pac`, `string.pac` (textes) et le code généré.

**1. Le HIAC à 6 existe déjà dans le jeu**
- Table des règles `misc.pac` (`RUL\0`, 119 fiches de 108 octets, voir `limit_breaking.md`). Les fiches 49 à 53 ont
  le nom 40447 HELL IN A CELL :
  - 49 = 1 contre 1 ;
  - 50 = par équipes ;
  - 51 = Triple Threat ;
  - 52 = Fatal 4-Way ;
  - **53 = 6 places, chacun pour soi** (« ARMAGEDDON HELL IN A CELL », textes 130 et 40744 ; « HELL IN A CELL 6-MAN »,
    21276).
- C'est un match du jeu, **pas une combinaison du Match Creator**. Les règles de l'écran de préparation
  (`match_creator.md`, tables `[+12]`/`[+20]`, `wwe12_mc_unlock` limité aux types 0-26) ne s'appliquent donc pas.
- `sub_82449898` (ligne de menu → numéros de règle) a une page dont les lignes rendent 46, 47, 48, 49, 52, 53 :
  c'est sans doute la page HELL IN A CELL de l'exhibition. **À confirmer par une capture** : le texte de la ligne et
  sa place.

**2. Ce que le jeu remplit (connu / à trouver)**
- **Connu.** Fiche du match en cours : 104 octets à **0x8317C850** (`sub_8267CF38` la rend, la plupart du code de
  match la lit). `sub_826D4888(fiche, type)` la remet à neuf :
  - +0 = type (règle) ;
  - +1 = 255 ;
  - +2..+14 = réglages remis à 0 ;
  - **+16 : 6 places de 12 octets** (u16 0x200, octet 14, deux octets 0xFF, le reste à 0) ; le numéro du catcheur
    est sans doute là, à confirmer.
  - Pour les types < 119, `sub_82A2FA18` choisit une autre mise à zéro (`sub_826D4838`).
- **Connu.** Écrivains du type en exhibition : `sub_82449428`, `sub_826F8D08`, `sub_823E3220`.
  `sub_82783808` / `sub_827829B0` : règles du match d'un autre mode (52 octets par type et sous-type).
  Le match travaille ensuite sur **une copie** (vu pour BLOOD). Il faut donc écrire **avant** cette copie.
- **À trouver.**
  - Le champ de l'arène.
  - Les champs des 6 catcheurs (numéro, tenue, manette ou ordinateur).
  - La fonction qui passe de l'écran d'avant-match au chargement.
  - **Méthode :** une sonde, en une séance, sur données isolées : piège d'écriture sur 0x8317C850..+104 et sur
    l'objet qui la contient, pendant un HIAC à 6 choisi à la main. On note qui écrit quoi et dans quel ordre, puis la
    pile au début du chargement (`[remote]` state loading).

**3. Voie la plus sûre : mixte**
Le trajet fixe de Rendu va jusqu'au HIAC à 6 et valide la grille avec n'importe quels catcheurs. Ensuite, **un
crochet de notre code réécrit les 6 catcheurs et l'arène** dans la fiche, au moment où le jeu la termine (avant la
copie et le chargement). Le jeu lance lui-même son chargement.
- **Avantages :**
  - pas de dépendance à la grille des catcheurs, donc ni les mods ni les DLC ne gênent ;
  - pas d'appel direct au chargement.
- **Simuler seulement les touches** (voie 1 seule) : fragile, à cause de la grille et des arènes.
- **Appeler nous-mêmes le chargement sans menu** (voie 2 complète) : la plus propre à la fin, mais la plus risquée.
  Le jeu fait beaucoup de choses entre les menus et le match : manettes (`sub_826E09D8`/`sub_826E0B18`),
  libération des menus, tenues, sons, sauvegarde du dernier match. À garder pour plus tard, si un « lancer un match
  de test » sert ailleurs.

**4. Pièges**
- **Dossier de données neuf.** L'arène Hell in a Cell est à débloquer (texte 41428), et certains catcheurs aussi.
  Il faut imposer **une arène et 6 catcheurs du jeu de base disponibles dès le départ** (numéros fixes, jamais un
  CAW ni un DLC). Le crochet doit vérifier qu'ils existent, sinon le test échoue proprement avec
  `verdict=failed`.
- **Validité des catcheurs.** Le Match Creator n'est pas en cause, mais l'écran de choix vérifie peut-être les
  catcheurs (manager, genre : Diva interdite en HIAC ?). Comme on écrit après cet écran, on ne passe pas ses
  contrôles : il faut prendre 6 hommes « normaux ».
- **Entrées.** À 6 elles sont longues. Les passer (START), comme le prévoit Rendu.
- **Plus de 6 catcheurs.** Impossible (tableaux à 6 cases partout). Le HIAC à 6 est déjà le maximum.

**5. Estimation (heures de travail, tests en jeu à part)**

| Tâche | Heures |
|---|---|
| Sonde : fiche du match, arène, catcheurs, moment du chargement (code + lecture des journaux) | 2 - 3 |
| Crochet « match imposé » (6 catcheurs + arène fixes, contrôles, `verdict=failed` si absent) | 2 - 3 |
| Lien avec le trajet de Rendu (fin du trajet = la page HIAC, plus de grille à parcourir) | 0,5 - 1 |
| **Total voie mixte (Gameplay)** | **4,5 - 7 h** |
| Voie 2 complète (lancement sans aucun menu), en plus | 6 - 12 h, incertain |

Tests en jeu nécessaires (avec l'utilisateur) :
- une séance de sonde ;
- puis 2 ou 3 benchmarks complets sur un dossier neuf (avec et sans mods installés) ;
- un match HIAC à 6 normal ensuite, pour vérifier que rien ne reste imposé hors du test.

### Ajouts au test de performance d'après le benchmark de Kaiko (Rendu, 2026-11-06, écrit, pas encore compilé)

Idées reprises du benchmark scripté du portage SvR 2011 de Kaiko (`port/tools/opt_bench.ps1`, `port/docs/PERF_LOG.md`),
réécrites chez nous avec crédit. Le code attend dans `work/wip/rendu_benchmark_results_fixed.patch` (pas dans l'arbre
tant qu'il n'est pas compilé) :
- **condition « faible »** : `--wwe12_benchmark_weak_mask=5` garde le jeu sur 2 cœurs physiques (masque remis toutes
  les 3 s, le runtime rend tous les cœurs), avec un fil occupé de priorité normale à côté : un petit PC simulé ;
- **mesures** : médiane, 1 % pires, pire image, images > 20 ms et > 51 ms (3 vblanks : saccade visible), dans le texte
  et dans `[results]` (`match_median_ms`, `match_frames_over_20ms`, `match_frames_over_51ms`, `condition=`) ;
- **historique** : une ligne par essai dans `benchmark_history.csv` à côté de l'exe, pour comparer avant / après ;
- **une passe** (`--wwe12_benchmark_passes=1`), **scénario imposé** (`--wwe12_benchmark_scenario=hiac6`, points
  d'entrée pour Gameplay : `Wwe12BenchmarkWantsFixedMatch`, `Wwe12BenchmarkFixedMatchStarted`,
  `Wwe12BenchmarkFixedMatchFailed`), et `verdict=failed` au lieu de mesurer les menus quand aucun match n'arrive.

### Match imposé : voie 2 (Gameplay, 2026-11-06, lecture seule, pour la 1.2)

Choix de l'utilisateur (via Main) : la **voie 2**, un match préparé **sans passer par les menus**. Cette section
complète la précédente (« préparation du match »). Rien codé, rien lancé.

**1. Ce qui est établi (lecture du code généré et des fichiers du jeu)**
- **Type de match = numéro de règle.** Fiche 53 de `misc.pac` = « ARMAGEDDON HELL IN A CELL » (texte
  27400 + 53, idem 40744), 6 places, chacun pour soi. Le type est le même numéro dans la fiche du match en cours.
- **Fiche du match en cours :** 104 octets à **0x8317C850** (`sub_8267CF38`), dans un objet plus grand :
  - le même getter sert à écrire **+12456** et **+336** ;
  - `sub_826D4888(fiche, type)` remet tout à neuf : +0 type, 6 places de 12 octets à +16.
- **Une fonction du jeu lance déjà un match sans écran de choix : `sub_82449428`**, appelée par la machine à états
  `sub_82449638` (état +64 = 3), dans le module des menus d'exhibition (à côté de `sub_82449898`). Dans l'ordre :
  1. elle vide la fiche (`memset` 64 octets) ;
  2. elle écrit **le type 49 (HELL IN A CELL 1 contre 1)**, puis +19 = 1, +20 = 1, +336 = 0 et objet+12456 = 3 ;
  3. elle appelle `sub_82449110` et `sub_8246DBE8(2, -1)` ;
  4. elle construit une demande de scène (`sub_822AF500`, `sub_827D2548(40)` ou `sub_827D3600(3)`,
     `sub_822B5D88`) : le passage au chargement.
  - Ce qu'elle lance (match de démonstration du menu, déblocage, tutoriel…) : à voir en jeu. C'est **le modèle
    exact de la voie 2** : la même séquence avec le type 53, nos 6 catcheurs et notre arène.
- **Création des personnages au chargement :**
  - `sub_822D23A0` → `sub_822D0618` → `sub_822CDF78` : un personnage par fiche de participant (+6 rôle, +9, +16,
    +20) ;
  - liste finale à 0x8317B770 (9 cases : 6 catcheurs, arbitre, 2 hors ring).
  - Les fiches de participants viennent d'une liste encore plus haut : à trouver.
- **Comparaison avec Kaiko (SvR 2011, `work/kaiko-svr2011/port/src/test_match.cpp`, lu seulement)** : son test
  **ne se passe pas des menus**.
  - Un trajet fixe (« route match » : menu principal → PLAY → ONE ON ONE → NORMAL MATCH → écran de choix) amène le
    jeu à l'écran de choix.
  - Ensuite deux crochets réécrivent :
    - les catcheurs, au moment où le jeu construit les personnes du match à partir des choix (SvR `sub_828BC5E8`,
      places de 2116 octets) ;
    - l'arène, sur l'objet de l'écran de match, dès qu'il existe.
  - La règle vient de son crochet « ligne de menu → règle ».
  - Sa leçon : l'arène écrite directement dans la fiche du match chargé laissait des parties du chargement avec
    l'ancienne (pas de cage, un gel sur téléphone). Il faut la poser **à la source**, avant que le chargement la lise.
  - Ses limites : un CAW plante (~2 s), car la préparation des CAW de l'écran de choix n'est pas faite.
  - Pour nous : **même famille de moteur, adresses différentes** (aucune taille 2116 retrouvée telle quelle dans
    WWE '12). On reprend la méthode, pas le code (accord de Kaiko pour ce qui s'inspire de lui, **crédit
    obligatoire**).

**2. Ce qui reste à trouver (une séance de sonde, données isolées, go de l'utilisateur)**
- **(a) `sub_82449428` : ce qu'elle lance.** On la journalise, puis on va au menu, à l'exhibition, et on lance un
  HIAC. Est-ce le match de fond du menu ? Un écran ? D'où viennent les catcheurs et l'arène quand elle n'en écrit
  pas ?
- **(b) Un HIAC à 6 choisi à la main**, avec un piège d'écriture sur 0x8317C850..+104 et sur +12456/+336 de l'objet,
  plus un journal de `sub_822D23A0` (fiches de participants).
  - **But :** trouver les champs des catcheurs (numéro, tenue, manette ou ordinateur, équipe) et le champ de l'arène,
    avec leur écrivain.
- **(c) La pile au moment où le chargement démarre** (état `loading` de la télécommande). But : la fonction qui
  remplace PLAY, et ce que l'écran de choix prépare en plus : manettes (`sub_826E09D8`/`sub_826E0B18`), tenues,
  musiques d'entrée, objets.

**3. Plan en étapes (critère final « vu en jeu » : un HIAC à 6 chargé et joué 60 s, sans aucun menu après le
menu principal)**

| Étape | Contenu | Heures (hors tests) | Vu en jeu |
|---|---|---|---|
| 0 | Sonde (a)(b)(c) ci-dessus, code + lecture des journaux | 3 - 4 | la carte des champs et du déclencheur |
| 1 | **Appel direct** depuis le menu principal (fil du jeu, `Wwe12BenchmarkWantsFixedMatch()`) : même séquence que `sub_82449428` avec le type 53, 6 catcheurs et une arène fixes du jeu de base, puis `Wwe12BenchmarkFixedMatchStarted()` | 3 - 5 | le chargement part et le match s'ouvre |
| 2 | Ce que l'écran de choix faisait : manettes (tous à l'ordinateur), tenues, entrées passées, mods coupés, contrôle « catcheur / arène présents » sinon `Wwe12BenchmarkFixedMatchFailed()` | 2 - 4 | 60 s de HIAC à 6 jouées par l'ordinateur, pas de plantage |
| 3 | Sortie propre : fin du test (`quit` du benchmark) ou retour au menu sans sauvegarde touchée | 1 - 2 | le dossier de données neuf est intact ou jeté |
| **Total** | | **9 - 15 h** | |

**4. Risques**
- **États non initialisés.**
  - Ce que l'écran de choix met en place en passant (profil du joueur, affectation des manettes, chargement des
    tenues et des musiques, CAW) manque si on l'évite.
  - Kaiko le voit déjà avec les CAW. D'où **jamais de CAW, ni de DLC, ni de catcheur moddé** dans le test.
- **Arène :** la poser à la source (comme Kaiko), sinon la cage peut manquer ou le jeu geler.
- **Sauvegarde :** un match lancé hors menu peut écrire des statistiques ou un « dernier match ». Le test tourne sur
  un dossier neuf (déjà le cas), donc la sauvegarde du joueur n'est jamais touchée.
- **Mods :** `wwe12_superstar_mods`, `wwe12_entrance_mods` et l'overlay du dossier `mods` sont coupés pendant le
  test.
- **Dossier neuf :** catcheurs et arènes verrouillés au départ (arène HIAC : texte 41428). On prend des numéros du
  jeu de base disponibles dès le départ ; la cage du HIAC vient du type de match, pas de l'arène.
- **4 fils / Android :** l'appel se fait sur le fil du jeu, jamais sur un fil hôte.

**5. Point de repli vers la voie 1 de Rendu**
- **Basculer après l'étape 0** (environ 4 h) si l'un de ces cas se présente :
  - `sub_82449428` ne lance pas un vrai match ;
  - le déclencheur du chargement dépend d'objets d'écran introuvables ;
  - les champs des catcheurs ou de l'arène restent introuvables.
- **Basculer aussi après l'étape 2**, si le match lancé sans écran plante ou gèle encore après 2 essais
  corrigés.
- **Repli conseillé, plus solide que la voie 1 seule :** la méthode de Kaiko.
  - Le trajet de Rendu va jusqu'à la page HIAC.
  - Les crochets de l'étape 0 réécrivent les catcheurs et l'arène à la source.
  - Les heures de l'étape 0 ne sont pas perdues.

**6. Effet de l'étude de Kaiko sur l'estimation de 8 à 15 h**
- **Elle ne raccourcit pas la voie 2 pure** : Kaiko ne l'a pas faite. Il garde le trajet dans les menus et réécrit
  au passage.
- **Elle confirme la méthode de repli et ses pièges** : arène à la source, pas de CAW.
- La voie 2 pure reste à **9 - 15 h**, grâce au modèle `sub_82449428` trouvé dans WWE '12 (sans lui, plutôt
  12 - 20 h).
- La méthode de Kaiko (trajet + réécriture) : **6 - 9 h** (Gameplay), plus le trajet de Rendu.

**7. Étape 0 faite (Rendu, 2026-11-10, sonde `wwe12_bench_probe`, HIAC Armageddon à 6 lancé à la main, iGPU Intel)**
- `sub_82449428` n'est **pas** sur le chemin normal de PLAY : jamais appelée.
- **Passage au match** : `sub_822B51A0` met l'objet de la fiche (`sub_8267CF38`) +12456 = 2, vide le mot global
  0x8318B7E0, alloue une tâche de 208 octets (`sub_827D2548(208)` ou `sub_827D3600(13)`) et la construit avec
  `sub_821D5430` = constructeur de la tâche **`CGameMainSequence`** (nom à 0x82002EB0, vtable posée à +0 ; elle
  fait elle-même la demande de scène 40 / 3 et objet+12456). `sub_822B51A0` est appelée en 0x822B51FC (chemin
  vu au test) ; `sub_822B5210` la précède d'une copie de 2 064 octets (`sub_827D3808`) : sans appelant direct
  (pointeur de fonction), sans doute « refaire le match ».
- **Fiche du match** (0x8317C850) au moment du passage : déjà remplie, +0 = 53.
- **Catcheurs choisis** : tableau de places de **48 octets au début du heap 0** (heap 0 = 0xAEDDFE00 ; place 0 à
  0xAEDDFEA4, numéro x 100 + tenue à +16 ; place 1 à +48…), copie identique à 0xAEDE06B4, 3e structure à
  0xAEDE1D58 (pas 0x44). La liste de chargement (0xAEECBC20, 80 octets : +0 numéro x 100 + tenue, +5 place,
  +6 rôle ; arbitre et extras aux places 7 et 8) en est tirée.
- **Stage** de l'objet de mise en place (`sub_828A9898()` +88) : 0 pour ce match.
- **Reste pour l'étape 1** : l'adresse du tableau est-elle la même à chaque lancement (heaps créés dans le même
  ordre : probable, à vérifier) ; qui remplit ces places et la fiche (piège d'écriture) ; appeler
  `sub_822B51A0` depuis le menu principal laisse-t-il la séquence des menus en place (elle doit se terminer comme
  à la sortie de l'écran de choix) ; le stage et l'arène.

## Idées de Jarrod v0.2.19 (Rendu, 2026-11-06, lecture seule, pour la 1.2)

Source : les notes de version de wwe2k14-recomp v0.2.15 à v0.2.19 (Jarrodfeng, BSD-3, accord oral
pour s'inspirer). On reprend les **méthodes** seulement, on réécrit nous-mêmes. Avant de copier du vrai
code : redemander à l'utilisateur et l'ajouter dans `THIRD_PARTY.md`. Rien de tout ça n'entre dans la 1.1.

**Le cas Intel de l'utilisateur** (`wwe12_044.log`, iGPU RaptorLake-S, Vulkan, 720p automatique)
tournait encore avec la **1.0** : 44 compilations de pipelines au début du match, dont une image de
170 ms, et le petit écran noir dans la même seconde. Une fois le match lancé, il tient 60 fps : environ
10 ms de travail par image, et l'attente GPU est presque nulle. Ses soucis venaient donc des
compilations (corrigées par c442aa3 en 1.1), pas de la vitesse du GPU. La plupart des idées
ci-dessous ne changent presque rien pour lui. Elles visent les iGPU plus faibles (UHD 620, Vega 3/6).

| # | Idée | Chez nous aujourd'hui | Faisable ? Où | Gain sur le cas Intel | Risque | Estimation |
|---|---|---|---|---|---|---|
| 1 | **AUTO sur GPU intégré** : 720p + agrandissement avec netteté (type FSR 1 / RCAS), foule à 70 % | 720p sur iGPU, c'est déjà fait (`svr_resources.cpp`, auto → 720p pour `INTEGRATED` et les cartes de moins de 3,5 Go). L'agrandissement à l'écran est un simple filtre linéaire. La foule n'a que deux choix, oui ou non (`wwe12_crowd`, `hooks/draw.cpp`). | **Netteté** : une passe de plus au moment de l'affichage (`present.cpp`), un shader dans les 3 exe et sur Android. RCAS d'AMD est sous licence MIT, à créditer si on reprend le shader. **Foule à 70 %** : réduire le nombre d'instances des quelques dessins de foule (repérés par shader, comme `wwe12_crowd`). Il faut une coupure stable pour éviter l'effet « des spectateurs qui apparaissent et disparaissent », que Jarrod a corrigé en 0.2.19. | Image plus nette en 720p sur un écran 1080p. Vitesse : la netteté coûte un peu, la foule allégée rapporte un peu. Jarrod n'a pas mesuré non plus. | Faible pour la netteté (un réglage, désactivable). Moyen pour la foule (trous visibles, instances mal comptées). | Netteté 3-4 h ; foule 3-5 h + RenderDoc |
| 2 | **Résolution dynamique** de la scène 3D seulement (menus et HUD nets) | Absent. `svr_render_scale` est fixe pour toute la partie. Les cibles de la scène sont créées à la taille choisie (`svr_resources.cpp`). | Possible mais lourd : dessiner dans une partie de la cible (viewport et scissor réduits) au lieu de recréer les cibles, puis corriger toutes les passes qui lisent la scène (flou de profondeur, bloom, copies) pour qu'elles ne lisent que cette partie. Android a déjà étudié la même idée (`android_optimisation_jarrod.md`). | Rien tant que le match tient 60 fps. Utile seulement sur un iGPU qui tombe sous 60. | **Élevé** : bords flous, effets décalés, plus de pipelines | 8-12 h + beaucoup de tests |
| 3 | **Réduire puis restaurer la fenêtre** : recréer la surface (image figée chez lui) | En lisant le code, on n'a **pas** son bug. Quand la fenêtre est réduite, `resize()` de plume remet la taille à 0 et échoue (Vulkan et D3D12). À la restauration, `needsResize()` voit une taille différente et `RebuildSwapChain` (`present.cpp`) refait tout. Jarrod, lui, ne recréait qu'au changement de taille. | Rien à coder. À vérifier une fois en jeu (réduire puis restaurer, en Vulkan, D3D12 et D3D11). L'exe D3D11 n'a pas été relu. | — | — | Test de 5 min |
| 4 | **Moins de dessins** : foule en gros lots, objets hors écran ignorés, dessins répétés réutilisés (+10 à +40 % sur une 4090 en 720p sans limite) | Nous dessinons les commandes du jeu (natif SvR), sans tri ni regroupement. WWE '12 dessine déjà sa foule en peu d'appels. Ses gains viennent de TRUE NATIVE, son propre moteur de rendu, et sont mesurés **sans limite d'images**. Lui-même s'attend à moins avec la limite à 60. | Il faudrait savoir quand deux dessins sont identiques (mêmes shaders, constantes, géométrie) et tester la visibilité des objets avec leurs boîtes : c'est en partie refaire le moteur de rendu. Point de départ : `reblue/gpu/hooks/draw.cpp`, `draw.cpp`. Il faut d'abord une capture RenderDoc d'un match pour compter les dessins vraiment répétés. | Faible avec 60 fps (le CPU de rendu n'est pas le goulot chez lui). Peut aider les petits CPU et le Steam Deck. | **Élevé** : objets qui disparaissent, image fausse | Mesure 2 h, puis 10 h et plus |
| 5 | **GPU à fenêtre d'adressage de 128 Mo** (contournement) | Chez lui, ça cassait des formes dans TRUE NATIVE, qui lit de gros tampons. Nous n'avons pas ce mode, et aucun rapport de ce genre chez nous. | Seulement si un joueur le signale : vérifier les tailles de tampons liés (géométrie, constantes) avec les limites du GPU (`maxStorageBufferRange` et autres). | Aucun (l'Intel Raptor Lake n'est pas concerné). | — | 1 h de vérification si besoin |
| 6 | **Affichage « mailbox »** contre la déchirure de l'image | Vulkan : avec la synchro verticale on utilise FIFO, sans elle IMMEDIATE (déchirure possible). plume détecte déjà si MAILBOX existe (`mailboxPresentModeSupported`) mais ne s'en sert pas. D3D12 : FLIP_DISCARD, la déchirure seulement si la synchro est coupée. | Facile : un choix de plus (synchro OUI / NON / SANS DÉCHIRURE). En Vulkan, MAILBOX si le GPU le propose, sinon FIFO. En D3D12, un intervalle de 0 sans le drapeau de déchirure fait la même chose. Code dans `recomp/plume/plume_vulkan.cpp` (`setVsyncEnabled`), `present.cpp`, et une ligne du menu GRAPHICS. | Aucun en vitesse. Plus de déchirure avec la synchro coupée. | Faible (le GPU travaille plus, plus de chaleur sur portable) | 2-3 h |

**Ordre proposé pour la 1.2 :** d'abord tester l'idée 3 (5 min, pour s'assurer qu'on n'a pas le bug),
puis 6 (petit et utile), puis la netteté de l'idée 1. La foule à 70 % et l'idée 4 seulement après une
capture RenderDoc qui montre un gain réel. L'idée 2 reste plus tard, à faire avec Android.
L'idée 5 attend qu'un joueur signale le problème.

Prochain test Intel : refaire le même match 1v1 avec la 1.1 (c442aa3). Si le petit écran noir reste
alors qu'il n'y a plus aucune compilation, regarder les cibles créées vides au changement de scène
(`seeded false` dans le journal).
