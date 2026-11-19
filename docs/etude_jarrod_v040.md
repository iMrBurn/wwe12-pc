# Étude : le rendu de Jarrod v0.3.0 → v0.4.0 (WWE 2K14) comparé à WWE '12

Conversation Rendu, le 2026-11-19, à la demande de Main. **Lecture seule** : notes de version v0.3.0 (2026-11-07), v0.3.3
(2026-11-15) et v0.4.0 (2026-11-19), lues avec `gh api` ; pas de clone, pas de code repris, rien compilé ni lancé.
Projet de Jarrod : `Jarrodfeng/wwe2k14-recomp`, BSD-3, accord verbal pour s'en inspirer avec crédit (on relit ses
méthodes et on les réécrit nous-mêmes ; copier du code = accord de l'utilisateur). Étude précédente :
`docs/optimisation_jarrod.md` (v0.2.x, 2026-10-31).

## À retenir d'abord

- **Son point de départ n'est pas le nôtre.** Jarrod partait du rendu du SDK : le GPU de la Xbox émulé, qui **relisait
  la liste de commandes PM4** du jeu et la rejouait. Son v0.4.0 supprime cette relecture. Nous, nous dessinons déjà
  nous-mêmes depuis le début (re:Blue + plume) : nos crochets sur ~50 fonctions D3D du jeu dessinent directement
  (`port/src/reblue/gpu/hooks/`). Rien n'est rejoué.
- **Ce qui nous reste de « PM4 »** : beaucoup de crochets appellent encore le corps d'origine (`__imp__…`). La
  bibliothèque D3D du jeu écrit donc toujours ses paquets dans un ring buffer, que le backend GPU « null » du SDK lit
  pour faire avancer fences, vblank et interruptions (`docs/couches.md`). Coût mesurable : zone `kGuestD3D` de
  `cpu_zones.h` dans la ligne `[perf]`. C'est la cible de notre « 2.0 », plus petite que la sienne.
- Notre base 1.4.0 (Main, i7-14700HX + RTX 5070, Vulkan 1080p) : fil de rendu **3,7 ms** par image, jeu entier
  14,1 ms, match 60 i/s (travail 11,1 / 16,7 ms), 0 pipeline compilé en dessinant. Sur un gros PC, le fil de rendu
  n'est pas la limite. **Les gains de Jarrod viennent surtout des petites machines** (Surface Pro 6, téléphones) :
  c'est là qu'il faut mesurer chez nous (Steam Deck, iGPU Intel, Android).

## Tableau

Coûts en heures de travail de Claude (lecture + écriture + compile), **sans** le temps de test de l'utilisateur.
Gains : estimation, rien n'est mesuré chez nous.

| # | Sa piste | Ce qu'on a déjà | Ce qui manque | Coût | Risque | Gain attendu |
|---|---|---|---|---|---|---|
| 1 | **Toutes les images dessinées arrivent à l'écran** (v0.3.0 ; sur GPU faible, ~¼ étaient jetées : entrée de Bryan 34 → 44 i/s montrées) | Pas de rejet d'image en jeu normal : `present.cpp` n'abandonne une image (`AbandonFrame`) qu'en turbo de chargement (voulu), sans cible de rendu, ou si l'image d'échange échoue. V-sync FIFO ; V-sync adaptative coupée (1.3.1, #63/#70) ; `svr_hold_new_scene_frame` (écran noir avant la cloche). | Une **preuve chiffrée** : compter « images dessinées / montrées / abandonnées » dans la ligne `[perf]` (et sur Android). Aujourd'hui on suppose, on ne mesure pas. | 1-2 h | Faible (journal seul) | Diagnostic ; un gain seulement si on découvre des rejets |
| 2 | **Blocage caché à chaque image** (v0.3.0 : le format de l'écran n'était jamais retenu, donc chaque image attendait la précédente et refaisait sa dernière étape ; pire moment 24 → 33 i/s) | Pipelines de présentation créés **une fois** au démarrage avec un format fixe (`kSwapChainFormat`, `device_pipelines.cpp`) ; base 1.4.0 : 0 pipeline compilé en dessinant. Le piège exact de Jarrod n'existe donc pas chez nous. | Vérifier l'attente par image `AdvanceAndWaitReused` (`present.cpp`) : attend-on la fin de l'image **précédente** (1 image d'avance seulement) ? Une mesure Tracy ou un chrono suffit. | 1-2 h | Faible | Inconnu ; utile sur petit GPU si l'attente est trop tôt |
| 3 | **Particules fusionnées** (v0.3.0 : ~2 700 petits dessins de pyro par image → quelques longs) | Chaque `DrawVerticesUP` / `BeginVertices`-`EndVertices` devient un dessin à part (`hooks/draw.cpp`). | Compter les dessins dans une entrée avec pyro (F9 = journal détaillé d'une image), puis fusionner les dessins consécutifs de même état (même shader, textures, mélange). | Mesure 1 h ; fusion 6-10 h | Moyen (ordre de dessin, transparence) | Fort **dans les entrées avec pyro** sur petits PC / téléphones ; nul sinon |
| 4 | **Catcheurs déformés une fois par image** au lieu d'une fois par passe (ombre, image, reflets) | Déformation faite par les vertex shaders du jeu traduits, refaite dans chaque passe. | Repérer les shaders de déformation, déformer une fois (compute) et réutiliser. Gros chantier de rendu. | 20-40 h | Élevé | Moyen à fort sur GPU faible ; peu sur gros PC |
| 5 | **Envois groupés** (v0.3.0 : moins de commandes GPU pour les envois de données) | Envois par dessin (constantes, géométrie, textures) ; zones `kConstants` / `kGeometry` / `kTextures` déjà mesurées. | Lire ces zones dans un journal de petit PC, puis grouper les envois d'une image dans un seul tampon. | Mesure 1 h ; travail 4-8 h | Moyen | Petit sur PC (3,7 ms au total) ; moyen sur téléphone |
| 6 | **Plus de test d'adresse à chaque accès mémoire** sous Windows (v0.3.0 : code du jeu −25 %, grâce à une fonction mémoire de Windows 10 1803+) | Notre code généré fait sous Windows `REX_PHYS_HOST_OFFSET(addr)` = `addr >= 0xE0000000 ? 0x1000 : 0` **à chaque lecture / écriture** (`codegen_pch_h.inja`). Sous Linux / Android c'est déjà 0. | Projeter la mémoire physique du jeu avec les « placeholders » de Windows 10 (`VirtualAlloc2` / `MapViewOfFile3`), pour que le décalage de 0x1000 ne soit plus nécessaire, puis régénérer tout le code avec la macro à 0. **Va avec la régénération (registres locaux + `volatile`) mise par l'utilisateur dans « Plus tard ».** | 6-10 h + régénération complète | Élevé (tout le code du jeu ; Windows < 1803 refusé) | **Le plus gros gain CPU** : fils du jeu, tous les PC, surtout les petits |
| 7 | **Liste de shaders préparés construite sur un parcours de tout le jeu** (v0.3.0 : liste presque doublée) | `pipelines_seed.w12p` + `bd_pso_precache`, préparés en arrière-plan au démarrage ; le benchmark (1 contre 1) ne compile plus rien. | Une liste qui couvre **tous les modes** : une longue partie de l'utilisateur avec enregistrement des nouveaux pipelines (Royal Rumble, Cage, Hell in a Cell, Univers, Create, arènes portées), puis fusion dans la graine livrée. | 2-3 h + une session de jeu | Faible | Moins d'à-coups la première fois, surtout iGPU / Steam Deck |
| 8a | **Vrais vertex buffers sur téléphone** (plus de limite 128 Mio) | Déjà fait : `physical_buffers.cpp` crée de vrais tampons hôte pour la géométrie. | Vérifier côté Android qu'aucun chemin ne passe encore par un grand tampon unique (session Android). | 1 h (lecture) | Faible | À vérifier |
| 8b | **Décompression BC au chargement sur Mali** | Déjà fait : `texture_upload.cpp` (`g_decode_bc`, #34 / #36). | Rien. | — | — | — |
| 8c | **Crash handler qui nomme les plantages** | `crash_trace.cpp` (PC), gestionnaires Android / iOS ; `dist/symbols-v<ver>/` garde la carte. | Garder la carte de **chaque** exe de test aussi (le dump 0xc0000409 du 16/11 n'a pas pu être nommé, voir `perf_natif_steam_deck.md`). | 1 h | Faible | Diagnostic |
| 9 | **Supprimer la couche PM4** (v0.4.0 : 2 300 commits en 8 jours, rendu 100 % à lui) | Dessin déjà natif ; plan de découplage écrit dans `docs/couches.md` (4 étapes). | Voir la section suivante. | 40-80 h | Élevé (gels, cadence, replays) | Moitié du CPU de rendu chez lui ; chez nous : la part `kGuestD3D` + les attentes `sub_82AAAAF8` / `sub_82AA7040` |

## Point 9 : la couche PM4, pour WWE '12

**Chez lui** (d'après ses notes) : il a d'abord fait dessiner par son rendu ce qu'il comprenait, en gardant la relecture
des commandes comme filet. Il a ensuite rendu ce chemin « par défaut » (v0.2.19 → v0.3.0), comblé chaque manque type par
type (catcheurs, arènes, foule, vidéos, effets, menus…, ~180 enquêtes écrites) et enfin coupé la liste de commandes
(v0.4.0). Il a gardé à chaque étape une option pour revenir en arrière (`--wwe2k14_s101_promotions=false`,
`--gpu_native_owned_frame=false`).

**Chez nous, la moitié du chemin est déjà faite** : on dessine tout nous-mêmes. Il reste à faire taire la bibliothèque
D3D du jeu sans casser ce qu'elle synchronise. Étapes (reprises de `couches.md`, dans l'ordre sûr) :

1. **Mesurer** (sans risque) : part de `kGuestD3D` et des attentes `sub_82AAAAF8` / `sub_82AA7040` dans la ligne
   `[perf]`, sur un gros PC et sur l'iGPU Intel. Si c'est < 0,5 ms par image, la 2.0 ne vaut pas son risque pour la
   vitesse.
2. **Inventaire** de ce que le jeu attend du GPU invité : `sub_82AA83D8` (SynchronizeToPresentationInterval), fences,
   `EVENT_WRITE`, callback vblank, pointeur de lecture `device + 11024` (`tools/callgraph.py`, recensement
   `-DWWE12_D3D_CENSUS=ON`).
3. **Une attente à la fois** remplacée par un crochet natif, derrière une cvar **coupée par défaut**, en commençant par
   SynchronizeToPresentationInterval (une fois par image, facile à tester).
4. **Ne plus appeler `__imp__`** des fonctions de dessin déjà crochetées, une famille à la fois (états, constantes,
   puis dessins), avec la même cvar de retour.
5. **Retirer le backend « null »** quand plus rien n'en dépend.

Estimation : étapes 1-2 ≈ 4-6 h (sûres) ; 3 ≈ 8-12 h ; 4-5 ≈ 30-60 h et beaucoup de tests (cadence 60 / 30, replays,
turbo, gels déjà connus). Les trois exe (Vulkan, D3D12, D3D11) et Android partagent les crochets : un seul chantier.

## Classement

**Sûr pour une 1.4.x / 1.5** (petit, mesurable, sans toucher au code du jeu) :
- 1 (compteur d'images montrées / abandonnées) ; 2 (vérifier l'attente par image) ; 8c (garder les cartes des exe de
  test) ; point 9 étapes 1-2 (mesure + inventaire).
- 7 (liste de pipelines de tout le jeu), si l'utilisateur veut faire une longue partie d'enregistrement.
- 3 (particules), **seulement si** le comptage F9 montre des milliers de petits dessins dans les entrées.

**Pour la 2.0** : 6 (mémoire sans test + régénération, déjà dans « Plus tard »), 4 (déformation une fois par image),
5 (envois groupés), 9 étapes 3-5.

Rien de tout ça ne demande de copier du code de Jarrod. Si une idée est reprise, on le crédite dans `THIRD_PARTY.md`.
