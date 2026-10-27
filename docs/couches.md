# Couches entre le jeu et le PC (cartographie du 2026-10-21)

But : savoir ce qui est déjà natif, ce qui est encore émulé, et ce qu'on peut retirer pour que le
jeu se comporte comme un vrai jeu PC. Établi par lecture du dépôt et des patchs de `port/patches/`
(le SDK ReXGlue, `recomp/`, n'est pas versionné : ce qui touche son intérieur vient de ses patchs).
Rien ici n'a été vérifié en jeu.

## Vue d'ensemble

| Couche | État | Retirer ? |
|---|---|---|
| Fenêtre, entrées, manette | Natives (SDL) | Déjà fait |
| Dessin | Natif (plume / Vulkan, crochets sur les appels D3D du jeu) | Déjà fait |
| Synchro GPU invitée (ring buffer, fences, vblank, interruptions) | Émulée : backend GPU « null » du plugin Xenos | **Oui : cible principale** |
| Attentes du jeu sur le GPU (`sub_82AAAAF8`, `sub_82AA7040`) | Code du jeu, attend le pointeur de lecture du ring buffer | Avec la ligne du dessus |
| Read-backs des cibles de rendu vers la mémoire du jeu (`svr_resources.cpp`) | Émulation de l'EDRAM / du resolve | Réduire, seulement où le jeu relit vraiment |
| Fils et affinités (le jeu suppose 6 cœurs) | Émulés | Adapter, pas supprimer |
| Cadence (horloge vblank, 60 / 30 fps) | Pilotée par la synchro invitée | Découpler (voir plus bas) |
| Sauvegardes (paquets STFS, profil fixe), système de fichiers virtuel | Émulés (XAM, VFS) | Garder, c'est fin |
| Audio XMA | Décodé par le SDK (`xma_context.cpp`) | Garder, ou convertir hors ligne |
| Code du jeu (PowerPC big-endian) | Recompilé statiquement | Impossible sans les sources |

## Le point central : la bibliothèque D3D du jeu tourne encore

- `wwe12_app.h` (`OnPreSetup`) charge le plugin Xenos avec le backend `null`
  (`graphics/null/graphics_system.cpp`, ajouté par `rexglue-sdk-reblue.patch`).
- Ce backend lit le ring buffer écrit par la vraie bibliothèque D3D du jeu (≈ `0x82AA0000`–
  `0x82AC0000`) et fait avancer fences, interruptions et vblank. `IssueDraw`, `IssueCopy` et
  `IssueSwap` ne font rien.
- Les crochets de `src/reblue/gpu/hooks/` (≈ 50 fonctions D3D) dessinent via plume. Beaucoup
  **appellent ensuite le corps d'origine** (`__imp__...`, voir `native/wwe12_d3d_names.h`) : la
  bibliothèque D3D continue donc d'écrire ses paquets PM4 dans le ring buffer.
- En rendu natif, les fonctions `D3DDevice_RingBuffer*`, `BlockUntilIdle`, etc. ne sont **pas**
  remplacées par des stubs : `hooks/device.cpp` ne les stubbe que si `SVR_NATIVE_RENDERER` n'est pas
  défini. Le jeu exécute donc toute la logique d'anneau d'origine.

## Où ça fait mal

- `native/wwe12_cpu.cpp` : `sub_82AAAAF8` attend que le GPU invité ait lu le ring buffer au-delà d'un
  point (pointeur écrit en retour à `device + 11024`) ; chaque tour est `sub_82AA7040` (rotation
  active). Sur les PC à 2-4 fils, cette attente concurrence le fil du processeur de commandes.
  Contournement actuel : on cède le CPU après 2 tours (`wwe12_gpu_wait_yield`).
- `CLAUDE.md` : les gels « le rendu s'arrête, vblank et son continuent » (#4, #11, Nothing) et
  l'interblocage du turbo (affinité 0xF) passent par ces mêmes attentes.
- `native/wwe12_load_turbo.cpp` : le turbo coupe le vsync du SDK pour faire tourner le vblank invité à
  1 kHz. C'est un levier sur l'horloge de l'émulation, pas sur le jeu lui-même.
- `reblue/gpu/present.cpp` : `PaceFrame` / `AbandonFrame` ont leur propre cadence côté hôte, en plus
  de celle de la synchro invitée.

## Plan de découplage

1. **Inventaire** de ce que la bibliothèque D3D du jeu attend du processeur de commandes :
   - `sub_82AA83D8` (`SynchronizeToPresentationInterval`, `WAIT_REG_MEM`, une fois par image) ;
   - `sub_82AAAAF8` / `sub_82AA7040` (attente de lecture du ring buffer) ;
   - fences et `EVENT_WRITE` ; callback de vblank ; interruptions graphiques ;
   - tout écriveur de pointeur de lecture (`device + 11024`).
   À faire : relever les appelants avec `tools/callgraph.py` et le recensement
   (`-DWWE12_D3D_CENSUS=ON`), et lister les imports `Vd*` du code généré.
2. **Remplacer ces attentes** par des crochets natifs, un à un : le pointeur de lecture avance
   virtuellement, la synchro de présentation devient une attente sur un événement hôte. Garder le
   backend « null » en filet jusqu'à la dernière étape. Tester après chaque crochet.
3. **Ne plus appeler le corps d'origine** des fonctions D3D déjà crochetées, pour que la bibliothèque
   n'écrive plus de paquets PM4.
4. **Retirer le backend « null »** et le ring buffer quand plus rien n'en dépend.

## Risques

- Les attentes sont mêlées au temps du jeu : cadence 60 / 30 fps (`wwe12_fps.cpp`), replays (#9),
  chargements et turbo. À tester à chaque étape.
- Plusieurs fonctions D3D ne sont pas encore identifiées (`DrawVertices`, `SetScissorRect`,
  getters de description, voir `docs/d3d_functions.md`) : un paquet qu'on ne connaît pas peut encore
  être attendu par le jeu.
- Il faut compiler et lancer le jeu sous Windows pour chaque essai ; rien ne se valide dans le cloud.
