# WWE '12 PC : consignes pour Claude

Portage PC natif de WWE '12 (Xbox 360) : recompilation statique avec ReXGlue (`recomp/rexglue-sdk`)
et rendu Vulkan natif repris des portages SvR de nefariousjosiah (re:Blue + plume, `port/src/reblue`,
`port/src/native`). Dépôt public : https://github.com/iMrBurn/wwe12-pc (licence GPL-3.0).

L'utilisateur (iMrBurn) est francophone et débutant : réponses en français, simples, sans jargon
inutile ; utiliser « iel/they » si un pronom est nécessaire. Les textes du jeu, du launcher et des
releases sont en anglais (plus FR/DE/ES/IT pour les readmes de `port/dist`).

## Règles absolues

- **Jamais de données du jeu sur GitHub ni dans un zip** : xex/xexp, iso, .pac, .bik, sons, textures,
  `port/assets/`, `port/generated/`, `image.bin`, `work/`, dumps, Title Update, DLC. Pas de lien de
  téléchargement de DLC ou du jeu non plus (risque de DMCA sur tout le dépôt). L'exe (code recompilé)
  est accepté.
- **Rien n'est poussé sur GitHub sans le « go » explicite de l'utilisateur.** Les releases sont créées
  par l'utilisateur sur le site (`gh` n'est pas installé).
- **Toujours prévenir avant de lancer le jeu**, et ne pas le lancer pendant que l'utilisateur regarde
  autre chose sans son « vas-y ». Un seul jeu lancé à la fois.
- **Demander avant tout téléchargement.** Ne jamais toucher aux réglages de sécurité de Windows.
- Intégrer du code d'un tiers (fork, PR) seulement après relecture et accord de l'utilisateur.
- Identité git locale au dépôt : iMrBurn / 267004884+iMrBurn@users.noreply.github.com.

## Travail à plusieurs conversations

Chaque conversation a un sujet : **(Main)** coordonne, versions, GitHub, retours des testeurs ;
**(Rendu)** bugs graphiques ; **(Title Update)** ; **(Launcher)** ; **(Mods)**. Rester dans son sujet ;
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
  ENABLE_VULKAN_RENDERDOC_CAPTURE=1), captures dans `work/rdc`, scripts via `qrenderdoc.exe --python`.
- Release : changer `VERSION`, compiler en release, `tools/package.ps1` → `dist/WWE12-PC-v<ver>.zip`,
  écrire `dist/RELEASE_NOTES_v<ver>.md`. `dist/symbols-v<ver>/` garde `wwe12.map` + `wwe12.exe` de la version (pour traduire les
  hang_trace/crash_trace des joueurs ; la 0.7.0 n'a pas sa carte). Coller à la fin des notes le contenu de `dist/SHA256-v<ver>.txt` (empreintes du zip
  et des deux exe, non signés : Windows Defender les a déjà signalés à tort en 0.6.0 ; faux positif à
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
- Title Update (pour la 1.1) : le codegen ne lit pas les xexp ; il faudra produire un xex patché hors
  ligne, refaire le codegen et retrouver toutes les adresses codées en dur.
- Bugs ouverts : ceinture sur l'épaule à 60 fps (piste VBlank testée le 2026-10-12 et écartée : le
  minuteur est déjà régulier à 60 Hz ; reste l'animation d'objet à 30 Hz, la ceinture de Cena décroche
  de la main quand il court ; mise de côté le 2026-10-12 : ni le VBlank, ni le pas de la physique, ni la
  struct 0x8324CE28 seule n'expliquent le défaut ; piste restante : minuteurs 8246A020/824999F8 + une
  horloge à pas fixe non trouvée ; outils de test dans `#if WWE12_DEV_PROBES`, voir la mémoire) ;
  AMD/Steam Deck (#4 gel en entrée sur RX 560, #7 bloc en haut à droite) en attente de retours 0.5.0 ;
  #7 persiste en 0.5.0 chez DRAVEN0612 sur **Intel UHD 620** (pilote Vulkan 1.3.215, vieux), donc pas
  qu'AMD ; son journal est dans `work/issue7/wwe12_014.log` (rien d'anormal visible : 60 fps, cible
  carrée 1120x1120 fmt 36 non relue, composite peau 1024x512 fmt 15 vidée) ;
  titantrons basse résolution (vidéos Bink 320x320 sur le disque).
- Menu OPTIONS > GRAPHICS (`wwe12_graphics_menu.cpp`) : l'entrée du manuel (texte 41184) renommée
  en mémoire ; écran ImGui au style des pages d'options (police Impact penchée), entrées du jeu
  bloquées pendant qu'il est ouvert. Page 100 % native = projet à part : les pages d'options sont
  des listes de numéros de textes en mémoire, et `wwe12_graphics_link_test=1304` ouvre déjà la vraie
  page IN-GAME OPTIONS depuis GRAPHICS (enregistrement menu.pac : +0x54 type 5, +0x6C lien).
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
- Choix du rendu : impossible aujourd'hui. Le rendu natif est compilé Vulkan seul (plume D3D12 coupé dans
  `native_renderer.cmake`, `REBLUE_D3D12` = build D3D12 seul) et le cvar `gpu_backend` de `wwe12.toml`
  est ignoré (`wwe12_app.h` quitte avant de le lire avec `SVR_NATIVE_RENDERER`).

## Feuille de route (décidée avec l'utilisateur le 2026-10-16)

Retours : issues GitHub + salon « bugs » du Discord de Kaiko (l'utilisateur y a sa catégorie).
Note : en match à plus de 2, le nom affiché au-dessus d'un catcheur est celui de la cible : normal.

**1.0 = stabilité, tous les bugs connus** (dans cet ordre) :
0. ~~Fuite de mémoire vidéo~~ : **corrigée** (2026-10-16, pas encore publiée). Trois fuites dans
   `svr_resources.cpp` : cibles « périmées » jamais libérées (`RetireHost` les rend maintenant comme
   un Release du jeu : registre des surfaces, ou destruction après la fence), surfaces que le jeu
   abandonne à chaque scène (libérées après 30 s sans usage), copies de textures et cibles des
   matchs précédents (60 s / 2 min). Le plantage d'origine venait de `DestroyResourceNow`, qui lisait
   une poignée HostResourceHeap comme une adresse (corrigé dans `graveyard.cpp`). cvar
   `svr_release_stale_targets` (false = comportement 0.6.0). Mesuré en 1440p, 3 matchs Univers :
   ~390 Mo stables par match au lieu de +120 à +250 Mo par match. La ligne `[perf]` donne la
   mémoire vidéo (`vram X / Y MB`), « SvR native: … presents/s » le nombre de surfaces, cibles et
   textures en vie. Probablement liés : image figée / ralenti / 0,25x après quelques matchs (Discord
   bugs 1, 4, 6, 7), #10, #8, cel-shading Steam Deck : à confirmer par les joueurs.
   Remarqué (pas gênant) : micro-écran noir < 0,1 s sur la moitié droite juste avant la cloche.
1. **RÉGRESSION 0.7.0 (2026-10-16)** : #11 (JimminyBuccilli, Intel HD 520, 2 cœurs) gèle maintenant en
   entrée ET en match, ce qui n'arrivait pas en 0.6.0 (pareil dans les vieilles versions du SvR 2011
   de Kaiko : lui demander ce que c'était). Journaux `work/issue11/` : dans wwe12_013.log, juste avant le
   gel, « 1 stale host target(s) released » puis les surfaces 1280x736 recréées aux mêmes adresses ;
   wwe12_008.log gèle sans libération juste avant. Test demandé au joueur : `svr_release_stale_targets
   = false`. Autre point : sur les PC à moins de 6 threads, `XThread::SetActiveCpu` (SDK, xthread.cpp)
   écrit « Too few processor cores » à chaque appel (des dizaines de milliers de lignes par minute) :
   à n'écrire qu'une fois (coût CPU/disque sur les petits PC).
1. Plantages / gels : #4 (gel en entrée, RX 560, journaux 0.6.0 joints), #8 (gel).
2. Graphismes AMD/Intel/Steam Deck : #7 et #11 (pavés pendant les entrées, toujours là en 0.6.0),
   #12 (yeux blancs sur AMD ; pas corrigé en 0.7.0 ; test demandé au joueur le 2026-10-16 :
   `svr_preserve_inf_nan = false`, même famille Inf/NaN que les yeux noirs Intel ?), #13 (pavés à droite
   en entrée + gel en match, Intel 6e gén. HD 520, pilote 30.0.101.3111 : même bug que #7/#11).
3. Performances : #10 (chutes de fps en milieu et fin de match).
4. Jeu : portes transparentes en backstage. ~~#9 (replay d'après-match)~~ : **corrigé** (2026-10-16,
   vérifié en jeu, pas encore publié ; il faut un SDK recompilé avec `rexglue-sdk-freespace.patch`).
   Le jeu (sub_822B6D30) veut 150 Mo libres sur `cache:` et y crée `Spectacular.rps` (150 Mo) pour
   les replays des moments forts, sinon il coupe tout (pas de replay, MATCH HIGHLIGHTS grisé). Le SDK
   annonçait 64 Mo fixes pour les dossiers de l'ordinateur ; il donne maintenant la vraie place.
   ~~Matchs WWE Draft du mode
   Univers~~ : **ça marche** (vérifié le 2026-10-16) : il faut choisir « DRAFT EVENT » à la fin du
   match, puis le tirage au sort et l'entrée de la superstar draftée se jouent (Kane → RAW).
   Messages de débogage cachés du jeu : `tools/dev/lancer_journal.bat` (cvar `wwe12_game_log`,
   `native/wwe12_game_log.cpp`, lignes « [game XXXXXXXX] » dans `journal_jeu.log` ; 826D4228 = mode
   Univers : cartes des émissions, classements, Draft).
5. Autres retours Discord (2026-10-16) : écran noir sur Intel HD 5500 (**message clair ajouté** 2026-10-16, `native/wwe12_vulkan_error.cpp` ; `work/discord/bug02.log` :
   « Plume CreateVulkanInterface failed », ce GPU Broadwell n'a pas de pilote Vulkan sous Windows ;
   au minimum un message clair au lieu d'un écran noir figé, idéalement le rendu D3D12) ; tenue
   Superstar Threads avec des zones blanches (Daniel Bryan). ~~Vibrations de manette absentes en mode
   Univers~~ (déjà le cas sur Xbox 360) : **corrigé** (2026-10-16, vérifié en jeu, pas encore publié) :
   `native/wwe12_rumble.cpp`, cvar `wwe12_universe_rumble` (défaut true, à ajouter aux readmes).
   Le catcheur du joueur a son indicateur « vibration » (catcheur+3452) à 0 en Univers
   (sub_82761630), à 1 en exhibition ; le crochet de sub_824729A8 le prête à 1 le temps de l'appel.
6. Option « tout débloquer ». ~~Gestion des sauvegardes~~ : **faite** (page SAUVEGARDES du launcher ;
   import Xbox 360 testé en jeu par l'utilisateur avec ses propres sauvegardes le 2026-10-16 : OK).

**Chargements, suite (prévu pour la 0.8, avec la ceinture à 60 fps)** : recette de l'auteur du recomp de WWE 2K14 (même famille Yuke's,
résumé du 2026-10-16). Son chargement d'avant-entrée : 5,5 s → 0,9 s. Dans l'ordre de ses gains :
(1) le chargeur fait une étape puis attend la prochaine image (chaque fichier = 4 images) : enchaîner
les étapes dans la même image (chez nous : `wwe12_loading.cpp`, déjà x2) ; (2) **« turbo » pendant les
écrans de chargement seulement** : faire tourner l'horloge vblank du jeu très vite (vsync coupée,
~400 images/s) car le jeu fait ~70 images de préparation cadencées par l'image ; le plus dur = début
et fin exacts de la fenêtre (sinon spinner trop rapide, début de match accéléré) : brider les
animations 2D à 1/60 s réel, finir au premier vrai mouvement des catcheurs ; (3) profiler nos propres
crochets (chez lui 8 VirtualQuery par image = 1,4 ms) ; (4) vblank sur minuteur précis ; (5) zlib natif
à la place du zlib 1.2.3 traduit du jeu, seulement quand l'en-tête est standard (231 → 72 ms), idem
BPE de Yuke's (`tools/yukes_bpe.py`) et LZX. Sans effet chez lui : sauter le dessin (perd un rendu
unique par chargement), vblank plus rapide avec vsync active. Attention : le turbo change la
synchronisation image/temps (replays, #9) : à couper là où c'est comparé. Mesurer avant chaque étape.
**Fait (2026-10-16, étapes 0 et 1, vérifié en jeu, cvar `wwe12_load_turbo` encore à false par
défaut)** : `native/wwe12_load_turbo.cpp`. Mesure : les threads du jeu avancent au rythme des images
(barrière sub_827DEA20 : prêt → attend le feu vert), 60 images de jeu par image affichée ; sur ~4 s
d'écran de chargement, le chargeur ne travaille que ~1 s. Écrans de chargement = 14 classes
(`/LOAD/IG%02d`, GN, CV…, vtable slot 13) ; méthodes communes **pas appelées à chaque image** :
slot 14 sub_826AF0F0 à l'ouverture (~10 images), slots 16/22/23 (sub_826A83B0, sub_826A6B90,
sub_826B3060) à la fermeture, quand l'entrée démarre. Turbo de l'ouverture à la fermeture (20 s max) :
cvar SDK `vsync` coupé par son nom (`rex::cvar::SetFlagByName`, vblank invité à 1 kHz) + une image sur
`wwe12_load_turbo_frames` (6) affichée (`present.cpp` demande `Wwe12LoadTurboSkipPresent`) ; ≈ 340
images de jeu/s. Résultat : chargement de match ≈ 4,0 → 2,8 s, fin des entrées → cloche ≈ 5,9 → 5,0 s
(un écran du même type s'ouvre aussi en fin d'entrées, avant `WWE12_Transition` : turbo 1,9 s, rien de
visible) ; replay et MATCH HIGHLIGHTS OK. L'interrupteur 0x8317B7BC vaut 1 en menus/chargement/match,
5 en entrées : inutilisable comme « match en cours ». Sonde : `wwe12_load_probe` (lignes `[loadprobe]`),
lanceurs `tools/dev/lancer_mesure_chargement.bat` / `lancer_test_turbo.bat`. Un plantage (corruption
du tas, 0xc0000374) au 1er essai ne s'est pas reproduit en 5 lancements : deux jeux tournaient en même
temps. Reste : étape 2 (fin des entrées → cloche : transition ~2 s puis marche des catcheurs ~2,5 s,
vrai jeu) et le gros chargement CPU (~1 s, 7 000 appels du chargeur).

**Mode Univers enrichi (voulu par l'utilisateur, 2026-10-16)**, sur le modèle de « DarkMatches » (Discord,
recomp de WWE '13, même mode Univers) : matchs de qualification (Money in the Bank, Elimination Chamber)
avec leurs actualités (NEWS) et la place gagnée ; participants surprise au Royal Rumble (0 à 3 légendes
de la division Legends, affichés « ??? » avec le portrait vide dans la liste). Base de travail : journal
de débogage du jeu (`wwe12_game_log`, 826D4228 = mode Univers : cartes, classements, Draft). Commencer
par cartographier comment le jeu génère les cartes d'émission et les actualités. Demander à DarkMatches
comment il s'y prend (et sa licence avant de reprendre du code).
L'utilisateur veut aller plus loin : refonte du mode Univers (booking, rivalités/scénarios,
cinématiques, actualités), dans une conversation dédiée « Univers ». Plan : 0) cartographie sans rien
modifier → `docs/univers.md` (stockage des émissions/roster/titres/rivalités, fonctions de génération
des cartes, choix des cinématiques, écriture des NEWS ; ce qui est en données .pac vs dans le code) ;
1) booking (titres, PPV, qualifications, Rumble surprises) ; 2) rivalités ; 3) cinématiques ;
4) actualités. Idée : à terme, des règles de booking dans un fichier réglable par les joueurs.
La 1.0 (stabilité) passe avant ; la cartographie peut se faire en parallèle.

**1.1** : Title Update (dernière mise à jour du jeu).
**Après** : choix du rendu (Vulkan / D3D12 ; plume gère les deux, pas D3D11), mods poussés, musiques
perso, textures HD (remplacement de textures par le dossier mods), minitrons ajoutés (contenu de mods),
vidéos HD, nouvelles stipulations et mode Univers enrichi (très difficile : logique du jeu), online.

## Idées et demandes des joueurs (à faire)

- **Option « tout débloquer »** (comme le portage SvR 2011) : un joueur signale que le DLC « All
  Unlockables » ne débloque que les persos des DLC, pas le reste. Piste : cvar `wwe12_unlock_all`
  (case dans le launcher) qui force les drapeaux de déblocage, sans toucher aux sauvegardes du joueur.
  Il faut d'abord trouver dans le code où le jeu teste si un élément est débloqué.
- ~~Yeux/dents noirs et cheveux de l'arbitre sur Intel iGPU~~ : **corrigé** (2026-10-12). Le pilote
  Intel supprimait les bornes ±FLT_MAX des shaders (Vulkan l'autorise si le module ne demande pas de
  garder Inf/NaN), d'où Inf × 0 = NaN dans la couche du visage. `guest_shaders.cpp` ajoute
  `SignedZeroInfNanPreserve` à chaque shader (cvar `svr_preserve_inf_nan`). À vérifier : l'issue #7 et le
  voile blanc en 540p pourraient avoir la même cause.

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
