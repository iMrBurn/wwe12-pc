# Android après la 1.1 : replay, performances, ajouts

## État au 2026-11-06

- **Fait (commits locaux, rien poussé)** : correctif vieux noyau (`rexglue-sdk-zz-android-oldkernel.patch`,
  8b61261) + ligne « Android kernel » dans le journal + rapport VK_EXT_device_fault et 12 derniers
  shaders liés à un DEVICE_LOST (`plume-zz-android-devicefault.patch`) ; version Android 10100 / 1.1.0
  (4351ff6) ; graine `declarations_seed.w12d` dans l'APK (881234f) ; nouveau look du launcher de
  **Yexclsv** intégré (54622e1, crédit fc2426f).
- **Testé** : émulateur x86_64 seulement (démarrage, menu, Match Creator, match jusqu'à l'entrée, 0 erreur ;
  nouveau look : pages PLAY / SETTINGS, le jeu démarre à 60 i/s). Les APK de test sont dans
  `work/android/test11/` (hors dépôt).
- **Pas testé** : le correctif vieux noyau (aucun noyau 4.14 sous la main), le rapport device_fault, le
  nouveau look sur vrai téléphone (onglets DLC / CONTROLS / DRIVER / DEBUG, paysage, éditeur de boutons :
  **Yexclsv doit l'essayer avant publication**), l'Univers, les DLC.
- **Décisions** : pas de 1.0.1, on sort directement une 1.1 (Android = versionCode 10100) ; les testeurs
  des issues #41 / #54 attendent la 1.1 (pas d'APK de test pour eux). Tout ce qui est « 1.2 » attend.
- **Reste pour la 1.1** : refaire l'APK de release signé `v1.1.0` depuis le HEAD final (dès que le PC a joué un
  match 1.1 : `declarations.bin` existe), vérifier que l'asset est dedans, vérifier la mise à jour automatique
  depuis le tag `v1.1.0` sur l'émulateur, relire les notes (`work/android/notes_android_1_1.md`).
- **Après la 1.1** : bloc A (replay), bloc B (performances), bloc C (liste du co-mainteneur : le seul envoi vu dans la conversation Android est le zip du nouveau look, déjà intégré ; si une autre liste de fonctions existe, elle est encore à recevoir).
- **Corrections apportées à l'étude du 2026-11-05** : la graine `declarations_seed.w12d` est maintenant copiée
  (ligne du tableau du bloc B) ; l'hypothèse « 5 jeux de descripteurs contre 4 » est **réfutée** (Moto G100 :
  « 4, le jeu en utilise 5 » et 58-60 i/s) ; le vrai blocage des Adreno 6xx avec leur pilote d'origine est
  `descriptorIndexing` absent ; le plantage DEVICE_LOST du Moto vient de Turnip v26.2.0-R5 (v24.1.0 R18 le
  règle) ; le Poco X3 Pro a un noyau 4.14.180 (cohérent avec l'hypothèse MAP_FIXED_NOREPLACE, non prouvé).
- Version d'avant ce ménage : `docs/archives/android_apres_1_1_version_2026-11-05.md`.

Étude en lecture seule du 2026-11-05 (session Android), à la demande de la Main. Rien n'a été
modifié dans le code pour l'écrire. Ce qui est **mesuré** est séparé de ce qui est **supposé**.

Le plan en trois blocs est en bas (A replay, B performances, C ajouts du co-mainteneur).

## 1. Le replay d'après match sur Android : état exact

### Ce que fait le replay dans le jeu (même code que le PC)

- Pendant un match, chaque « moment fort » gardé est écrit dans `cache:\Spectacular.rps`
  (fichier de 150 Mo créé au démarrage par `sub_822B6D30`, 20 cases de 0x594000 octets). Sur
  Android `cache:` = `files/wwe12/xbox_cache/`, sur le stockage du téléphone.
- L'écriture est faite par un **fil de travaux** du jeu, `sub_821D9428` : une boucle « attend un
  travail (`sub_821AE708` → `NtWaitForSingleObjectEx`) → prend la section critique → sort un
  travail de la file → l'exécute (types 0 et 1 : `sub_821D91E8`, `sub_821D9308` ; sinon
  `sub_822B8130` = ouvre `Spectacular.rps`, se place sur la case, flot tamponné `sub_822B7788`
  qui lit d'abord 16 Ko puis écrit ~200 blocs de 16 Ko) → prend la section critique, marque le
  travail fini (`sub_821AEC28`) ». C'est ce que nous avons vu dans les journaux, pas un schéma
  théorique.
- Après la cloche, le jeu enchaîne (journal PC de la Main, `work/android/pc_match_journal_jeu_2026-10-26.log`) :
  `WWE12_Winning` → +4,5 s `/EVP0../EVPA` → `@Entrance_Music-<vainqueur>` → `WWE12_Transition`
  → replay (~40 s, lectures de 3,9 Mo toutes les ~11 s) → `/EVZ*` `/PGEN` `/GMEV` → scène de
  victoire → EXIT / REMATCH / MATCH HIGHLIGHTS.

### Où il est coupé sur Android

- `port/src/android/wwe12_replay.cpp` : le cvar `wwe12_instant_replay` (défaut **false**, Android
  seulement) ; un crochet sur `sub_822B6D30` (celle qui met l'interrupteur du jeu à 1 au démarrage)
  remet `0x83174250` à 0 après l'original. À 0 aucun moment n'est gardé et MATCH HIGHLIGHTS est grisé
  (comme le PC #9). Le PC n'a pas ce crochet : replay toujours actif.
- Launcher : DEBUG > « Instant replay after matches (can freeze the game for now) »
  (`LauncherActivity.java`), plus « Log the replay file's reads and writes » (`replay_io_log`, patch
  SDK `rexglue-sdk-replay-io-log.patch`) et la sonde `[replay]` de `wwe12_game_log`.
- `wwe12_fresh_cache` (tests) vide `EditEx.cac` et `Spectacular.rps` à chaque démarrage.
- Historique : gel vu en alpha 3, détail en alpha 4, coupé par défaut en alpha 5 (#22, #23, #24,
  #29, #31), toujours coupé en 6, 7, 8 et en 1.0.

### Ce que disent les journaux (mesuré)

Sources : `work/android/reports/2026-10-26_replay_freeze_notes.txt`, `2026-10-25_universe_postmatch_stuck_notes.txt`,
journaux `issue29/wwe12_012.log`, `issue31/wwe12_002.log`, `issue23_wwe12_009.log`.

1. **Même motif sur l'émulateur x86_64 et sur un vrai téléphone** (#31 : POCO F6 Pro, 57-60 i/s,
   alpha 4) : après `WWE12_Winning` et `/EVPA` (+4,5 s, comme le PC), plus rien : ni
   `Entrance_Music` ni `WWE12_Transition`. Les catcheurs et la caméra sont figés, la foule bouge :
   le rendu tourne, **la logique du jeu attend**. La lenteur de l'émulateur n'est donc pas la cause.
2. **Au moment fort gardé** : le fil de travaux ouvre `Spectacular.rps` et lit 16 Ko (zéros). Dans
   les bonnes exécutions il écrit tout de suite ~200 blocs de 16 Ko ; dans les exécutions gelées il
   **n'écrit jamais** et attend, au repos, dans `sub_821B04D0` (aucune pile du jeu en dessous).
3. **Aléatoire, ~1 exécution sur 2 sur l'émulateur** (wwe12_060 bon, 061 gelé, cache vidé dans les
   deux : le cache n'est pas la cause, les deux bonnes exécutions étaient de la chance).
4. `sub_822EAD80` (mise en place du replay au début du match) : même paramètre dans les bonnes et les
   mauvaises exécutions. Pas la cause.
5. Pas d'erreur dans le journal, pas de `STUB` (`NtWriteFileGather` jamais appelé).
6. **Un test avec replay coupé a quand même gelé une fois** avant la scène de victoire (wwe12_056,
   après `/ETEX/0000`) : le replay n'est peut-être pas le seul coupable de « bloqué après un match ».
7. Autre famille sur l'émulateur : chargement de match infini après `Comm_0161/Comm_0139` dans 4
   exécutions sur 8, aussi **sans** replay (le jeu attend ~14 s sans lecture dans les bonnes
   exécutions). Probablement la même nature (attente perdue).
8. Moto Edge 50 Fusion (2026-10-25) : « 65 stale host target(s) released » juste avant le blocage : piste
   écartée (le PC fait pareil et continue).
9. Le journal des gels ne dit rien de plus : un gel de ce type n'écrit rien. C'est le défaut des
   attentes perdues.

### La piste de Kaiko (vue avec le commit réel, c43d482 de `wwe-svr2011-pc`)

Son message de commit : « Match-end highlights can no longer lock a won match ». Ce qu'il a trouvé et fait :

- L'étape « moments forts » de fin de match (`sub_82413280` dans SvR 2011) attend, dans son état 1,
  que l'enregistreur de replay (`sub_8219F9F0`) rende le clip suivant ; l'enregistreur ne réussit que
  s'il est au repos (état +0 = 1). Sous Proton sur Steam Deck, il ne revenait jamais au repos : l'étape
  réessayait à l'infini et le match gagné ne finissait jamais.
- **Il n'a pas trouvé la cause : il a mis un chien de garde** (`replays.cpp`) : après 8 s en état 1
  (ou 30 s en état 2), l'étape est marquée finie (offset +788 = 1), **le chemin « pas de moments
  forts » du jeu lui-même** : on passe à la victoire et aux résultats. Il journalise l'état et les
  champs de l'enregistreur quand il saute. Il a aussi un mode de test qui bloque exprès l'enregistreur.
- **Ressemblance avec nous : forte** (logique figée, rendu vivant, enregistreur qui ne finit pas). Nos
  mesures (point 2) donnent une raison : l'enregistreur n'est jamais « repassé au repos » parce que
  le fil qui écrit n'a jamais reçu son travail. Ce n'est encore **qu'une hypothèse** tant que nous
  n'avons pas lu les champs de l'enregistreur de WWE '12 pendant un gel.

## 2. Pour le remettre : cause racine, correctif, tests, risque

### Cause racine (non trouvée, hypothèses classées)

| # | Hypothèse | Pour | Contre |
|---|---|---|---|
| 1 | **Réveil perdu entre le fil qui met le travail en file et le fil de travaux** (la file est sous section critique, l'attente est un événement) | « ~1 sur 2 », fil au repos alors que le travail devrait exister, pas d'erreur | `threading_posix.cpp` (événements, semi-forces) relu : `Signal` pose `signal_` sous le verrou, `Wait` teste le prédicat sous le verrou ; rien de faux vu |
| 2 | **Priorités de fils non appliquées** : `set_priority: permission denied` est dans tous les journaux Android ; sous Windows elles le sont. Le jeu peut dépendre de « le fil de travaux passe avant le fil de jeu » | Cadre avec les gels « moins de 6 fils » du PC (#11) et l'interblocage du turbo : tous des défauts d'ordonnancement | Pas prouvé ; Linux a aussi d'autres différences (affinité) |
| 3 | **Attente côté fil de jeu qui lit un champ que l'autre fil écrit** (chien de garde de Kaiko : l'état de l'enregistreur) | C'est ce que Kaiko a observé | Notre fil de travaux est celui qui ne reçoit rien : l'amont reste à trouver |
| 4 | Lecture/écriture de fichier (flot tamponné `sub_822B7788`) qui ne rend pas la main | La lecture de 16 Ko est la dernière chose vue | Aucune écriture n'est tentée ; la lecture est terminée |

**Sonde décisive à faire (une seule exécution gelée suffit)** : quand le fil de travaux attend depuis
plus de 3 s après l'ouverture, écrire dans le journal (a) le nombre de travaux dans la file et l'état
des travaux (la structure sous la section critique de `sub_821D9428`), (b) l'état signalé ou non de
l'événement de `sub_821AE708` et la date de son dernier `NtSetEvent`, (c) les champs de
l'enregistreur de WWE '12 (équivalent de `sub_8219F9F0`, à retrouver). Lecture :
- file **vide** : le travail n'a jamais été mis en file (amont, hypothèse 3) ;
- file **non vide** mais fil au repos : réveil perdu (hypothèse 1 ou 2) ;
- file **non vide et fil actif** : c'est le travail qui bloque (hypothèse 4).
Le journal des gels (hang_trace) n'aide pas ici : il faut cette sonde, comme la note « sortir les piles
sur demande avant de deviner » de l'équipe.

### Correctif probable, en deux niveaux

1. **Chien de garde à la Kaiko (sûr, rapide, indépendant de la cause)** : accrocher l'étape de fin de
   match qui attend l'enregistreur (à retrouver dans le code de WWE '12 : c'est la fonction qui, après
   `WWE12_Winning`, attend les moments forts ; points de départ connus : `sub_821EE520` ajoute un
   moment, `sub_821EDEE0` = « au moins un moment », `sub_822EAD80`, `sub_822B6D30`). Si l'étape reste
   plus de N secondes dans son état d'attente, la marquer finie par **le chemin « pas de moments
   forts » du jeu**, journaliser, et continuer. Avantage : plus jamais de match bloqué, même si la cause
   n'est pas trouvée ; au pire le replay est sauté. Limite : ne guérit pas « le fil n'écrit pas », donc
   ce moment-là n'aurait pas de replay.
2. **Vraie correction** après la sonde : selon la table ci-dessus (réveil à sécuriser dans le SDK,
   priorités à émuler, ou amont à corriger). Le SDK est partagé avec le PC : tout correctif y passe par
   un patch `rexglue-sdk-*.patch` et une vérification PC.

### Remise en route (ordre conseillé)

1. Sonde (ci-dessus) + enregistrer 6 matchs avec replay **activé** sur l'émulateur : ~1 gel sur 2 donne
   assez de données et teste le correctif (probabilité de ne rien voir en 6 essais si le défaut est à
   50 % : 1,6 %).
2. Chien de garde (niveau 1) + cvar pour le couper ; replay **activé par défaut seulement après** les
   tests ci-dessous.
3. Vraie correction si la sonde l'explique.

### Tests

- Émulateur x86_64 : 10 matchs d'affilée, replay activé, tous types (1 contre 1, cage, 4 catcheurs,
  Univers), compter les gels et le temps de passage vers la victoire. Script `loop_match` (à refaire :
  télécommande `wwe12_remote_pad`, appui long de A par `adb shell input swipe x y x y 200`).
- Émulateur arm64 (traduction) : mêmes essais, pour les différences d'ordre mémoire.
- **Vrais téléphones, indispensable** : Adreno 7xx (#51 Adreno 720, Retroid #29 Adreno 740, POCO F6 #31) :
  un joueur par modèle, journal avec « Game debug messages ». Un défaut d'ordonnancement ne se voit pas
  de la même façon sur 8 cœurs ARM big.LITTLE.
- Contre-épreuve : avec le replay coupé, les matchs finissent-ils toujours (wwe12_056 a gelé une fois) ?
  Si oui, le chien de garde doit aussi couvrir cette étape.

### Risque

- **Moyen** : le code touché est le moteur de fin de match, commun au PC si le correctif est dans le SDK
  ou si le crochet n'est pas protégé par `#if __ANDROID__` / `wwe12_instant_replay`. Garder le chien de
  garde Android seulement au début (comme `wwe12_replay.cpp`), cvar de retour arrière.
- Un chien de garde mal réglé peut sauter un vrai replay lent sur un petit téléphone (le replay dure ~40 s) :
  régler le seuil sur l'**attente sans progrès** (aucune lecture ni écriture), pas sur la durée totale.
- 150 Mo écrits sur le stockage à chaque match : sans effet visible sur les téléphones modernes ;
  vérifier la place libre (déjà signalée correctement par `std::filesystem::space`).

## 3. Plan en trois blocs

### Bloc A : replay d'après match (priorité de l'utilisateur)

1. Retrouver dans le code de WWE '12 l'étape qui attend l'enregistreur (équivalent de `sub_82413280`)
   et le test « enregistreur au repos ». Lecture seule, 1 session.
2. Sonde de gel (file de travaux, événement, champs de l'enregistreur) : 1 session.
3. Chien de garde Android + journal, replay activé par défaut sous cvar ; tests émulateur ×10 : 1 session.
4. Version de test pour 2-3 joueurs (lien de l'utilisateur) : #51, #29, #31 ; recueillir les journaux.
5. Si le défaut reste : vraie correction (SDK, avec la Main).
6. Sortie : replay activé par défaut seulement après N matchs sans gel sur vrais téléphones.

Critère de réussite : 0 gel sur 20 matchs (émulateur) et un retour de joueur par SoC ; en cas de gel,
le chien de garde ramène à la victoire en moins de 10-15 s.

### Bloc B : performances

État mesuré (journal #54, Moto G100, SD 870, Adreno 650, Turnip) : **46 i/s en match, 21,9 ms par
image, fil de rendu 18,2 ms, jeu entier 63,8 ms de CPU par image (~3 cœurs)**, VRAM 519 / 4601 Mo,
50 à 70 « hitches » par 10 s. Le menu tient 58-60 i/s. Le mur est donc le **CPU du fil de rendu**,
pas le GPU (« gpu wait 0,0 ms »).

Les nouveautés PC de la 1.1 sont-elles dans Android ?

| Nouveauté 1.1 | Dans Android ? | À faire |
|---|---|---|
| Pipelines connus préparés au démarrage (`svr_pso_boot_objects`, c442aa3) | Le code est partagé (mêmes sources). **La graine `declarations_seed.w12d` est copiée dans l'APK depuis le 2026-11-05 (881234f)** ; avant, `build.ps1` ne copiait que `pipelines_seed.w12p` | Rafraîchir `pipelines_seed.w12p` à la 1.1 ; vérifier sur le téléphone que le démarrage ne s'allonge pas (compilation en fond : 1 fil par 4 fils matériels, donc 2 sur 8 cœurs, ils peuvent voler du temps aux gros cœurs) ; garder le cache `VkPipelineCache` disque (déjà là) |
| Mémoire des tas du jeu 12 et 15 (3e52573, +16 Mo et +4 Mo) | Oui (code partagé) | Surveiller la mémoire sur les téléphones à 6 Go ; effet attendu faible |
| Tenues blanches (867181b) | Oui | À valider sur Adreno / Mali |
| Petits PC : attentes GPU en veille, relecture des textures sur 32 images (f936b75) | Oui, partagé ; **pas validé sur Android** | Mesurer |
| Correctifs D3D11 / D3D12 | Non concerné (pas Android) | — |

Pistes Android (détail et risques dans `docs/android_optimisation_jarrod.md`) :
1. **Fil de rendu** : afficher le temps CPU par image (déjà dans `[perf]` Android) puis mesurer la
   part « dessin des textures / constantes » (`[perf] render thread split` : draws 4,8 ms dont constantes
   1,5, textures 2,1, géométrie 2,7). Plus gros gain probable à 60 i/s : épingler le fil de rendu et le
   fil du jeu sur les gros cœurs (affinité), puis réduire les hachages de textures (115 textures
   échantillonnées par image).
2. **Cadence 90 / 120 Hz** : aujourd'hui rien ; piste simple (`Surface.setFrameRate`) évitant les images
   perdues sur écrans 120 Hz. Mesure à faire avant.
3. **Résolution dynamique** : baisse automatique du facteur interne si l'image dépasse 25 ms ; le facteur
   existe déjà (résolutions 360p à 1080p). À faire ensuite, à partir de mesures.
4. **Mémoire** : surveiller `largeHeap` et la mémoire du processus sur 6 Go ; limites du jeu non mesurées.
5. **Chauffe** : plafonner à 30 en menu, option « économie » ; à décider avec l'utilisateur.

Critère : 60 i/s stables en match sur Adreno 7xx milieu de gamme (SD 7 Gen / 8 Gen), 30 i/s sur 6xx avec
Turnip. À mesurer sur vrais téléphones (l'émulateur x86 n'est pas représentatif).

### Bloc C : ajouts de la liste du co-mainteneur

La Main indique (2026-11-05) que l'utilisateur a envoyé la liste ; dans la conversation Android, seul le zip du nouveau look (Yexclsv) est arrivé, et il est intégré. Pour une éventuelle autre liste, après la 1.1 PC, marche à suivre :
1. La copier dans ce fichier (section « Liste du co-mainteneur »), chaque ligne triée par effort
   (réglage du launcher / commande tactile / fonction du jeu / SDK) et par partage avec le PC.
2. Ce qui est déjà vu dans les retours des joueurs et qui pourrait en faire partie : icônes PlayStation,
   boutons tactiles déplaçables (déjà possible : éditeur de disposition), Mod Maker sur Android (non :
   seulement le dossier `mods`), 90/120 Hz, sauvegarde et partage des sauvegardes, manette Bluetooth
   (SDL la prend déjà), langue.
3. Rien n'est promis avant le tri avec l'utilisateur.

## 4. Ce qui reste incertain

- La cause racine du gel du replay (hypothèses 1 à 4). Le plus utile maintenant : la sonde de gel.
- Le comportement sur un vrai téléphone moderne avec 6-8 cœurs (un seul rapport #31 avec le niveau de
  détail voulu, alpha 4).
- Les gels « sans replay » (wwe12_056, chargements infinis `Comm_*`) : même famille ou non.
- Je n'ai pas relu le journal #51 en entier ici (non téléchargé) ; les liens de ce document viennent des
  notes de session et des commits.

## 5. Ordre proposé après la sortie de la 1.1

1. Bloc A, étapes 1-3 (replay) : la plus grande valeur pour les joueurs.
2. ~~Bloc B, graine Android~~ : faite (881234f), à vérifier dans l'APK de release.
3. Bloc C dès que la liste arrive.
4. Bloc B pistes 1-3 selon les mesures sur téléphones.

## 6. Mode 30 fps (issue #57, Android, 2026-11-06, étude en lecture seule, pour la 1.2)

Demande : un vrai mode 30 fps pour les téléphones moyens (Redmi Note 13 Pro 5G, Snapdragon 7s Gen 2, « jeu au
ralenti »). Un simple plafond ne marche pas : le jeu avance sa logique d'un pas par image (`wwe12_display.cpp`, « the
game's logic advances once per frame »), donc 30 images = jeu au ralenti de moitié. Partie PC et code commun : voir
`docs/perf_natif_steam_deck.md`, section « Mode 30 fps » (Rendu, ce251b7). **Ma lecture du code arrive à la même
conclusion que Rendu.**

### 1. Comment le jeu tient son rythme

- Le vblank invité est une minuterie de l'hôte à 60 Hz (`graphics_system.cpp`, fil « vsync worker »). Le jeu attend
  ses vblanks, avance sa logique d'un pas par image, puis appelle `D3DDevice_Swap` (crochet `hooks/device.cpp`).
- Le jeu a **son propre mode 30** (struct 0x8324CE28, `SetFrameRate` sub_82827918) : pas de temps de la logique doublé,
  attente de 2 vblanks par image. Il s'en sert pour les entrées. C'est déjà lui qui tourne quand `wwe12_60fps` est
  faux : le chemin est donc éprouvé, y compris sur téléphone.
- `PaceFrame` (présent) plafonne à 60 ; `wwe12_fps_limit` ralentit le jeu en dessous de 60 (confirmé dans le code).

### 2. Où couper

| Voie | Gain sur téléphone | Verdict |
|---|---|---|
| **A : mode 30 du jeu partout** (`SetFrameRate` 60→30, « revenir à 60 » neutralisé) | logique, fil de rendu et GPU divisés par ~2 | **à faire** |
| B1 : ne présenter qu'une image sur deux | quasi rien (le GPU dessine tout, la logique tourne à 60) ; seul effet : l'écran montre 30 images | inutile seule |
| B2 : ne pas dessiner une image sur deux | GPU et un peu du fil de rendu ; **pas** la logique | déconseillée |

Raison de A : le mur sur téléphone est le processeur. Moto G100 en match : 46 fps, fil de rendu 18,2 ms sur 21,9 ms,
jeu entier 63,8 ms de CPU par image (plusieurs fils), attente GPU 0. Les 18,2 ms sont surtout du code du jeu
(recompilé) qui construit la scène, pas des dessins (mesures `draws` : 0,4 à 2,5 ms sur nos autres journaux). B2
n'enlèverait donc presque rien. En A, le téléphone doit tenir 21,9 ms par image sur un budget de 33,3 ms : le Moto
tiendrait 30 stables avec de la marge. Pour #57 (Snapdragon 7s Gen 2, cœurs moyens) on peut espérer le même effet,
**non mesuré**.

### 3. Risques

- **Le jeu n'a probablement jamais tourné en match à 30** (prises, contres et fenêtres d'appui, collisions et objets,
  chronos, IA, replay, moments forts). Seul vrai risque, à tester en jeu : pas faisable à ma place sur l'émulateur.
- **Read-backs, tenues cuites, cibles de rendu : pas touchés** en voie A (le rendu dessine toutes les images que le jeu
  lui donne ; l'image « sautée » n'existe pas côté rendu). C'est l'avantage principal sur B. À noter : les réglages
  comptés en images (`svr_readback_streak` = 32 images, `svr_attire_bake_check`) durent deux fois plus longtemps en
  temps réel à 30 : à regarder si des tenues restent blanches un instant.
- Latence des commandes : +16 ms au plus, un peu plus sensible avec les boutons tactiles. Acceptable, à dire aux joueurs.
- Écrans 90/120 Hz : 30 divise 60, 90 et 120, donc le rythme reste régulier ; rien à faire de plus.

### 4. Ce qu'il faut changer côté Android (en plus du code commun de Rendu)

- `LauncherActivity.java:272` : la case « 60 fps in entrances (heavier) » devient une liste à 3 choix
  (`wwe12_frame_mode` = original / 30 / 60_entrances) ; le préréglage LOW-END PHONE (ligne ~553, qui écrit
  `wwe12_60fps=false`) écrirait `30` **seulement après** les tests en match.
- `wwe12_perf_hint.cpp` : la cible du conseil de performance est fixée à 60 fps (`kTargetNs` = 16,67 ms). En mode 30 il
  faut 33,3 ms, sinon le système croit que le jeu est toujours « en retard ». Petit, mais à ne pas oublier.
- Le compteur de fps du jeu et `GameLoading` (attend la ligne « Android performance hint ») ne changent pas.
- Idée pour plus tard (hors étude) : `Surface.setFrameRate(30)` en mode 30, pour économiser la batterie.

### 5. Estimation, en heures de travail (hors tests en jeu)

| Tâche | Heures |
|---|---|
| Code commun (crochets, clé `wwe12_frame_mode`, ligne FRAME RATE du jeu) : **chez Rendu** | 4,5 - 5,5 |
| Launcher Android : liste à 3 choix, lecture de l'ancienne clé, textes | 1 |
| Cible du conseil de performance selon le mode | 0,5 |
| Préréglage LOW-END PHONE (après les tests) + notes de version | 0,25 |
| Vérif. émulateur (vitesse juste : durée d'une entrée / d'un compte avec la manette distante ; pas de mesure de perf) | 0,75 |
| **Total Android** | **~2,5 h** (+ code commun) |

Tests en jeu (avec un testeur sur un vrai téléphone, par ex. #57 ou #54) : match complet, replay, moments forts,
entrée, Univers. Cible mesurée : « interval » du journal à 33,3 ms stable et « work » sous 25 ms.

Ordre : 1.2, une fois le code commun fait par Rendu. Rien à coder avant son accord sur une seule clé.

## 7. Téléphones bloqués en Vulkan 1.1 (note de Main, 2026-11-06, à regarder après la 1.1)

- Source : notes de Jarrod v0.2.19 (WWE 2K14), https://github.com/Jarrodfeng/wwe2k14-recomp/releases/tag/v0.2.19.
  Pour les GPU limités à Vulkan 1.1 (typique des vieux téléphones), il demande **deux extensions** pour que le jeu accepte la
  disposition des données de shaders : 12 erreurs avant, 0 après. Mesuré sur un profil Mali-G57 **émulé**, pas sur un vrai appareil.
- Utilité pour nous : téléphones qui ne démarrent pas (comparer avec l'erreur « descriptorIndexing » des Snapdragon 845-870, #41/#54,
  qui est peut-être de la même famille). Quelles extensions exactement : **je n'ai pas encore lu le détail** (page non ouverte).
- À faire après la 1.1 : lire la page, repérer les deux extensions, voir si notre plume/Vulkan les demande déjà, puis les ajouter en
  option (patch plume, Android seulement) et faire tester sur un téléphone concerné.
- Licence BSD-3 : méthode seulement (aucun code repris) ; créditer dans THIRD_PARTY.md (ligne Jarrod) si on s'en sert.
