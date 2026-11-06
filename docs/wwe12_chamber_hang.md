# Hang en cours de match de WWE'12 PC (v0.8.0-beta) — analyse du trace de blocage

Analyse hors-ligne du trace capturé (`work\discord\hang_trace_940mx.txt`), appuyée uniquement sur le trace
et le code recompilé. Aucun lancement de jeu. Chaque affirmation est citée (fichier:ligne, offset du trace,
ou script de restitution dans `work\rdc\`).

---

## 1. Synthèse

- Le **XThread principal (15336)** n'est **pas en défaut** : il est bloqué (park) sur l'event nommé
  **`GameMainThreadEvent`** (chaîne vérifiée dans l'image invitée à `0x82001AC0`), qui est l'état de repos
  normal du `main()` XAPI pendant toute la partie. S'il sortait de ce park, il passerait directement à
  `DbgPrint("[XAPI RETURN VALUE] %d\n")` puis `XamLoaderTerminateTitle` (séquence d'arrêt du titre) —
  donc un réveil du main serait un arrêt/crash, pas un hang.
- Le **vrai point de blocage** est le **thread moteur maître** (`XThread192C`, id 6444) : il est figé dans
  l'attente *INFINITE* du handle de complétion d'un **slot de job** (`sub_82475210`, attente à
  wwe12_recomp.54.cpp:6799-6807), au milieu de la passe de décharge des slots (`sub_821E2B98`) appelée par
  l'interlock de quantum `sub_821E2CE8` (wwe12_recomp.113.cpp:395-640).
- Autour de lui, **~20 threads invités** dont les rôles proviennent de la table statique de threads du
  moteur (CGameWorkerThread, CSkinModelUtil, CParallelDraw1, HavokClothThread, CMovieControlXenon, AIThread,
  etc. — §4) sont tous en attente : sur leurs events individuels (groupe A) ou en sommeil périodique
  (groupes B/C). Les threads GPU/Kernel/VSync côté hôte sont correctement inactifs → aucun rendu depuis
  10 s, cohérent avec un pipeline CPU-jeu figé en amont, pas un bug GPU.
- **Hypothèse n°1 (racine la plus probable)** : stall du pipeline jobs/events — un job posté dans le
  tableau, scanné par `sub_821E2B98`, dont l'event de complétion (champ `+7` du slot) n'est **jamais**
  signalé. Le maître attend pour toujours ; les workers attendent leur réveil ; le rendu est figé.
  Classement complet en section 9.

---

## 2. Contexte et méthode

- Source primaire : `work\discord\hang_trace_940mx.txt` — stacks d'environ 90 threads au moment du hang
  (10 s sans frame présentée).
- Décodage image hôte : les `wwe12.exe+0xNNN` sont des offsets d'image ; VA absolu = offset + `0x140000000`
  ; résolution par `dist\symbols-v0.8.0-beta\wwe12.map` (nouvel outil `work\rdc\map_lookup.py`, transcription
  Python du `map_lookup.ps1` existant, avec vérification « INSIDE » par le symbole suivant).
- Données statiques invitées : le `wwe12.exe` distribué ne contient pas de magie XEX2 ; l'image utile est
  `port\assets\default.xex` (18 595 840 octets, compression « basic », clé retail). Le loader XEX2 existant
  `tools\find_fptrs.py` (`load_xex`, lignes 61-106) est réutilisé par les nouveaux scripts
  `work\rdc\xex_static.py` et `work\rdc\xex_thread_table.py` pour extraire les données statiques utilisées
  par `xstart`/`sub_821AD1E8` (base invitée `0x82000000`, taille `0x1360000`).
- Primitives invitées identifiées par lecture directe des sources recompilées (`port\generated\default\*.cpp`)
  ; le bas de chaque stack de thread invité est le trampoline d'entrée `sub_821B3F58`
  (wwe12_recomp.236.cpp:73-129 ; `bctrl` du thread main à :110 puis `ExTerminateThread`), présent sous tous
  les threads invités du trace.
- Conversion des `lis` invités : base = immédiat signé 16 bits étendu en haut 16 bits, vérifiée contre les
  valeurs décimales générées par le C recompilé (ex. `lis r11,-31983` → `-2096037888` = `0x83110000` ;
  wwe12_recomp.113.cpp:407-408). Les notes de la session précédente contenaient plusieurs conversions
  fautives (columns corrigées en §12).

---

## 3. Les primitives invitées (couches « noyau » recompilées)

| Wrapper invité | Sens | Preuve |
|---|---|---|
| `sub_821B04D0` | `KeWaitForSingleObject(handle)` → `NtWaitForSingleObjectEx` en boucle sur timeout | wwe12_recomp.172.cpp:57-120 (appel `__imp__NtWaitForSingleObjectEx` :92-93) |
| `sub_821AE708` | helper tail-call de `sub_821B04D0` (1 ligne `b`), pour les attentes INFINITE | wwe12_recomp.71.cpp:47-54 |
| `sub_821AE710` | `NtCreateEvent` (nom dans r6 → `RtlInitAnsiString` + `OBJECT_ATTRIBUTES` via `sub_821B0538`, puis `__imp__NtCreateEvent` :95-97) | wwe12_recomp.72.cpp:47-139 |
| `sub_821B0590` / `sub_821AEAC0` | `ExCreateThread` : handle, plus le trampoline d'entrée `0x821B3F58` en r6 et la routine `startRoutine` en r7 | wwe12_recomp.104.cpp:55-121 (`__imp__ExCreateThread` :95-97, r6=`0x821B0000+16216` :85-92) et wwe12_recomp.130.cpp:47-56 |
| `sub_821AE7F0` | `KeSetBasePriorityThread` (clamp ±15→±16 ; appel noyau :98) | wwe12_recomp.2.cpp:48-124 |
| `sub_821AE980` | `KeSetAffinityThread` (`1 << cœur`, cœur < 6 ; `ObReferenceObjectByHandle` :74, `KeSetAffinityThread` :90) | wwe12_recomp.174.cpp:45-148 |
| `sub_827DE6B8` | `NtResumeThread(struct[4])` — **correction** §12 : ce n'est pas un enregistreur d'event | wwe12_recomp.121.cpp:15113-15120 → wwe12_recomp.185.cpp:62-66 |
| `sub_821AEC28` | `NtSetEvent` (r4=0 ; erreur → `sub_821B0478`) — le signal générique du moteur (~40 sites d'appel) | wwe12_recomp.159.cpp:54-93 (appel :67-69) |
| `sub_821B07E8` | `XSleep`/`KeDelayExecutionThread` (ms→100 ns ; attente :113) | wwe12_recomp.192.cpp:62-148 |
| `sub_82A89BE0` | reset/clear d'event juste après une attente | appelé après les waits : wwe12_recomp.113.cpp:487-491 et 629-633 |
| `sub_827D38C8` | condition-wait : `RtlLeaveCriticalSection(obj+12)` → wait INFINITE sur `obj[104]` → reset → `RtlEnterCriticalSection(obj+12)` | wwe12_recomp.37.cpp:16432-16489 |

Trampolines `rexruntime.dll` observés dans les stacks (pas de .map rex disponible, sens inféré par usage) :
`0xB0320` ↔ attente simple ; `0x2C3801/0x4A0979` ↔ sommeil ; `0xB04FA/0x2CD6D1` ↔ variante multi-object ;
`0x2C2274/0x2C57D1/0xB0BD5` = trampoline de fond de chaque thread invité.

---

## 4. La table statique des threads du moteur (découverte clé)

`work\rdc\xex_thread_table.py` (données de `default.xex`, base `0x82000000`) restitue une table
d'enregistrements à pas `0x14` à partir de `0x820018C8` : `{nomPtr, taillePile, main, fX, fY}`
(fX/fY = champs d'annotation, sémantique exacte non établie — probablement cœur/instance) :

| Enreg. | Nom (chaîne invitée) | Pile | Main | fX | fY |
|---|---|---|---|---|---|
| 0x820018C8 | `CGameMainThread` | 0x20000 | `0x821AD068` | 4 | 0 |
| 0x820018DC | `CGameWorkerThread` | 0x10000 | `0x8281A2C8` | 1 | 1 |
| 0x820018F0 | `CRenderParticleManager` | 0x20000 | — (main enregistré à l'exécution) | 3 | -1 |
| 0x82001904 | `XHV Thread` | 0x8000 | `0x82827698` | 5 | 0 |
| 0x82001918 | `CSkinModelUtil` | 0x20000 | `0x82826A38` | 3 | 0 |
| 0x8200192C | `CRenderCmdManager` | 0x10000 | `0x827DE400` | 0 | 0 |
| 0x82001940 | `CParallelDraw0` | 0x10000 | `0x827DE400` | 1 | 0 |
| 0x82001954 | `CParallelDraw1` | 0x20000 | `0x821E4D58` | 2 | -1 |
| 0x82001968 | `HavokBackThread` | 0x20000 | — | 2 | 0 |
| 0x8200197C | `HavokClothThread` | 0x20000 | `0x827DDC68` | 3 | 1 |
| 0x82001990 | `CFileThreadManager` | 0x18000 | `0x826801D8` | 4 | 1 |
| 0x820019A4 | `CMovieControlXenon` | 0x10000 | `0x8288C558` | 4 | 1 |
| 0x820019B8 | `CSaveLoadXenon` | 0x8000 | `0x821E0BC8` | 2 | 1 |
| 0x820019CC | `BaneHairThread0` | 0x8000 | `0x821E0BC8` | 3 | 1 |
| 0x820019E0 | `BaneHairThread1` | 0x8000 | `0x821D9A20` | 5 | 1 |
| 0x820019F4 | `BaneRopeThread` | 0x8000 | `0x821D9918` | 1 | 1 |
| 0x82001A08 | `CommentatorThread` | 0x8000 | `0x821D9590` | 4 | -2 |
| 0x82001A1C | `Highlightreel` | 0x8000 | `0x82B00368` | 4 | -1 |
| 0x82001A30 | `HighlightMovieControlXenon` | 0x8000 | `0x821D8AE8` | 5 | 1 |
| 0x82001A44 | `GameReplayRecordThread` | 0x8000 | `0x829597D8` | 3 | -1 |
| 0x82001A58 | `AIThread` | 0x8000 | `0x821D8A18` | 2 | 1 |
| 0x82001A6C | `StageRingThread` | 0x8000 | `0x821D8720` | 3 | -1 |
| 0x82001A80 | `CameraAIDirectorThread` | 0x8000 | `0x821D8430` | 1 | 1 |
| 0x82001A94 | `MotionDecompressionThread` | 0x8000 | `0x824752A0` | 1 | 1 |

Chaînes voisines (restitution complète dans `work\rdc\xex_static.py`) : `HavokClothThread` (0x820017FC),
`CameraAIDirectorThread` (0x820016EC), `ParalellCDraw %d` (0x82001AF4), `MainThread` (0x82001B08),
`GameMainThreadEvent` (0x82001AC0), `GameWorkerThread` (0x82001AD8), `D:\` (0x82001AEC, graine de buffer).

**Créateur de thread XAPI** : `sub_827DE618` (wwe12_recomp.205.cpp:15924-16010) reçoit un descripteur, et
: stocke `desc[0]` (taille de pile) dans out[12] et la décrémente du compteur global de mémoire libre
`0x83242B90` (base `lis r10,-31964` → `0x83240000` + 11152, :15943-15964) ; appelle `sub_821AEAC0` =
`ExCreateThread(...)` (:15969-15973) ; pose la priorité via `sub_821AE7F0` (:15976-15978) ; l'affinité via
`sub_821AE980` (:15983-15985) ; crée l'event individuel du thread nommé `desc[4]` (:15986-15998).
Descripteur de bootstrap à `0x820018B8` = `{pile 0x20000, main 0x821AD2B8, cœur 0, priorité 0,
nomEvent → 'CGameMainThread' (0x8200189C)}`.

---

## 5. Le XThread principal (15336) est en park **normal** — preuve

Trace (`hang_trace_940mx.txt:210-224`) : `ntdll+0x9D634 / KERNELBASE+0x1B6AE / rexruntime 0xB0320… /
wwe12.exe+0x3A056CA` (= `sub_821B04D0+0xea`, wrapper d'attente) / `wwe12.exe+0x3915553` (= `sub_821AD1E8
+0x2d3`) / `wwe12.exe+0x302EC53` (= `xstart+0x833`).

- `xstart` (invité `main()`) se termine inconditionnellement par `sub_821AD1E8` (wwe12_recomp.142.cpp:281-286),
  puis `DbgPrint("[XAPI RETURN VALUE] %d\n")` (format à `0x82002810`, appel :298-300) et
  `XamLoaderTerminateTitle` (:307-309) : le park est le dernier acte du main.
- `sub_821AD1E8` (wwe12_recomp.169.cpp:34-155) : prépare ses structures (init de critique-section globale
  `0x82002B68` via `sub_827DD540` :72-74) ; crée **le thread moteur** depuis le descripteur `0x820018B8`
  (`sub_827DEB18` :89-91 → `sub_827DE618` : main = `0x821AD2B8`, cœur 0, prio 0, pile 0x20000) ; crée
  l'event **`GameMainThreadEvent`** (r6 = `0x82001AC0`, appel `sub_821AE710` :114-116) ; **reprend** le
  thread moteur (`NtResumeThread` via `sub_827DE6B8` :119-123, le thread est créé suspendu par
  `ExCreateThread`) ; **park INFINITE** dessus (:124-130, wait via `sub_821AE708`, puis vérif
  `sub_821AE6C0` :133-135 et nettoyage `sub_827DE5C0` :140-142).
- Donc pendant une partie : main parké = comportement nominal. S'il se réveillait, ce serait la fin du
  titre — ce park ne peut pas expliquer un hang de match en cours.

---

## 6. `XThread192C` (6444) = thread moteur maître — le vrai point de blocage

Trace (`hang_trace_940mx.txt:236-256`), chaîne hôte intégralement décodée (toutes les frames « INSIDE » des
spans de symbole, via `work\rdc\map_lookup.py`) :

```
#6  wwe12.exe+0x3A056CA -> sub_821B04D0+0xea   (wrapper KeWaitForSingleObject)
#7  wwe12.exe+0x13544CD -> sub_82475210+0x11d  (attente du handle de complétion du slot de job)
#8  wwe12.exe+0x1AE3461 -> sub_821E2B98+0x1f1  (passe de décharge des slots)
#9  wwe12.exe+0x26A5A69 -> sub_821E2CE8+0x7a9  (interlock de quantum)
#10 wwe12.exe+0x4AE58D7 -> sub_821D4FC8+0x407
#11 wwe12.exe+0x2D5C2E4 -> sub_827D42F0+0x464
#12 wwe12.exe+0x14652A9 -> sub_827D5380+0x199
#13 wwe12.exe+0x3867F65 -> sub_821AD2B8+0x2dc5 (thread main = celui démarré par le main XAPI — §5)
#14 wwe12.exe+0x4F2B750 -> trampoline d'entrée (cf. §2)
```

Détail de la chaîne :

1. **`sub_821E2CE8`** (wwe12_recomp.113.cpp:395-640), l'interlock de quantum :
   - lit le contexte du quantum depuis le global `0x83113890` (:407-410 ; base `lis r11,-31983` =
     `0x83110000` + 14496) ;
   - si `ctx[332] != 0` : **attente INFINITE sur `ctx[348]`** (:480-486, via `sub_821AE708`), puis reset par
     `sub_82A89BE0` (:487-491) ;
   - contrôles de sous-champs du contexte (pairs d'index 38/18/20 puis 24/26/28 ; global `0x83113884` etc.),
     :503-579 ;
   - **décharge des slots de jobs** : appel `sub_821E2B98` (:580-582) ;
   - **signal** `NtSetEvent` sur `ctx[352]` et `ctx[356]` (:612-621 via `sub_821AEC28`) ;
   - **attente INFINITE sur `ctx[368]`** (:622-628), puis reset (:629-633).
   → C'est le verrou de rendez-vous de trame : le maître attend les events de basses heures (348/368),
   décharge les slots de jobs finis, puis publie les events 352/356 pour le reste du moteur.
2. **`sub_821E2B98`** (wwe12_recomp.77.cpp:371) : boucle sur les enregistrements du tableau de jobs, appel
   `sub_82475210` par slot (:416-418).
3. **`sub_82475210(r3=tableau, r4=i)`** (wwe12_recomp.54.cpp:6771-6854) :
   - si `*(tableau + (i+16)*4) != 0` (drapeau « travail en attente » posé) :6791-6796 ;
   - **attente INFINITE sur `*(tableau + (i+7)*4)`** — le handle de complétion du slot (:6799-6807, via
     `sub_821AE708`) ;
   - registre de diagnostic `0x8317B770 + i*4` (:6808-6815 ; base `lis r9,-31976` = `0x83180000` − 18576),
     avec code d'erreur éventuel via `sub_827D5C28`/`sub_8236BE08` (:6829-6844) ;
   - puis **efface le drapeau du slot** `*(tableau + (i+16)*4) = 0` (:6846-6849).

Le maître est donc arrêté sur l'événement **de complétion d'un job précis**, jamais signalé : blocage en
attente sur un producteur disparu (ou jamais démarré) — pas un busy-spin.

---

## 7. Catalogue des threads du trace (groupes et rôles identifiés)

Conventions de groupes (primitives citées §3) :

- **Groupe A** — attente simple INFINITE (`ntdll+0x9D634 / KERNELBASE+0x1B6AE / rexruntime 0xB0320 →
  0x2CD442 → 0x495348 → 0x4A158E`, wrapper invité `sub_821B04D0`).
- **Groupe B** — sommeil (`ntdll+0x9DC34 / KERNELBASE+0x462DE / rexruntime 0x2C3801+0x4A0979 →
  wwe12.exe+0x40B4E70` = `sub_821B07E8+0x1a0`, vérifié par `work\rdc\map_lookup.py`) : boucle alive
  typique des threads à horloge.
- **Groupe C** — variante multi-object (`ntdll+0x9E104 / KERNELBASE+0x50D10 / rexruntime 0xB04FA+0x2CD6D1` ;
  wrappers invités vérifiés : `wwe12.exe+0x345EB81` = `sub_82DFA260+0x1b1` (wwe12_recomp.154.cpp) ;
  `wwe12.exe+0x3E0EC46` = `sub_82EDDCB0+0x346` (wwe12_recomp.183.cpp)).

### 7.1 Threads invités nommés par la table (§4) — tous en attente

| Trace id (nom trace) | Rôle (table §4) | Groupe | Site d'attente (lecture source recompilée) | Trace |
|---|---|---|---|---|
| 6444 `XThread192C` | thread moteur maître (bootstrap §5, main `0x821AD2B8`) | A | attente slot de job : `sub_82475210`:6805-6807 ← `sub_821E2B98` ← `sub_821E2CE8` (§6) | :236-256 |
| 15336 `Main XThread` | main XAPI (bootstrap) | A | park `GameMainThreadEvent` : `sub_821AD1E8`:124-130 ← `xstart`:284-286 | :210-224 |
| 12712 `XThread31A8` | `CGameWorkerThread` | A | `sub_8281A2C8+0x156` (wwe12_recomp.83.cpp) ← `sub_827D75B8+0x403` (wwe12_recomp.87.cpp) — worker en attente dans sa boucle de prise de travail | :395-410 |
| 12304 | `CSkinModelUtil` | A | `sub_82826A38` ← `sub_827DE8A0` | — |
| 19908 | `XHV Thread` | A | `sub_82827698` ← `sub_827D75B8` | — |
| 7028 | `CRenderCmdManager` **ou** `CParallelDraw0` (même main `0x827DE400`) | A | attente dans le main `sub_827DE400` | :552+ |
| 20268 | `CParallelDraw1` | A | `sub_821E4D58` | — |
| 8816 `XThread2270` | `HavokClothThread` | B | sommeil `sub_821B07E8` ← `sub_827DDC68` ← `sub_821A1AD8` (idle périodique) | :273-287 |
| 5008 | `CMovieControlXenon` | B | `sub_8288C558` | — |
| 8672 | `BaneHairThread1` | A | `sub_821D9A20` ← `sub_821D99B0` | — |
| 19028 | `BaneRopeThread` | A | `sub_821D9918` ← `sub_821D98A8` | — |
| 17584 | `CommentatorThread` | A | `sub_821D9590` ← `sub_821D9428` | — |
| 19244 | `HighlightMovieControlXenon` | A | `sub_821D8AE8` | — |
| 19160 | `GameReplayRecordThread` | A | `sub_829597D8` ← `sub_829596F0` | — |
| 19384 | `AIThread` | A | `sub_821D8A18` ← `sub_821D8960` | — |
| 13784 | `StageRingThread` | A | `sub_821D8720` ← `sub_821D8688` | — |
| 9500 | `CameraAIDirectorThread` | A | `sub_821D8430` ← `sub_821D8398` | — |
| 8180, 18124 | famille `CSaveLoadXenon`/`BaneHairThread0` (main table `0x821E0BC8`) | A | `sub_821E0AD0+0x50a` (même sous-famille, à −0xF8 du main de table) | — |
| 18380 `XThread47CC` | main de table `CGameMainThread` = `0x821AD068` | A | condition-wait `sub_827D38C8+0x111` (wwe12_recomp.37.cpp) ← `sub_821AD068+0x406` (wwe12_recomp.157.cpp) | :532-547 |

Threads invités hors table (créés par un sous-système, pas par la table statique) :
`16140` (`sub_82E75F20`), `14008` HavokWorker (`sub_82BACA50`), `20468` (`sub_82A2C430` ← `sub_82A2CD18`),
plus les divers Groupe B (`19664 sub_826A4960`, `17068 sub_827F1DF0`, `4280 sub_827F1690`,
`2260 sub_82DF6DF0`) et Groupe C (`13972`, `13056` via `sub_82DFA260`/`sub_82EA3A98`, `14288` via
`sub_82E17380` ← `sub_82E18AF8` ← `sub_82E172C0`).
Threads terminés sans stack : `20360` (`XThread4F88`) et `17188` (`XThread4324`, trace `hang_trace_940mx.txt:549-551`,
seule frame `ntdll+0x4CC70`).

### 7.2 Threads hôte / autres (sains pendant le hang)

- `17828 GPU Commands` (attente du prochain paquet GPU, `hang_trace_940mx.txt:63-73` — stack intégralement
  rex/rexgpu en attente : sa file est vide → le CPU en amont n'alimente plus).
- `4976 GPU VSync` (sommeil vsync, :75-83).
- 9608 (hooks D3DDevice/state.cpp + pompe de messages), 12108 (SDL joystick), 19796 (WASAPI), 12380
  (audio), 4212 (CoreMessaging), 11040 (Sleep detour), 2852/15676 (hook OBS), threads NVIDIA,
  12588/11244/18060/8948 (ntdll waits), 19296 (hook D3D/ucrtbase), 15968 (watchdog/sampler hôte du
  wwe12.exe, state.cpp), 15128 (Kernel Dispatch), 8912 (XMA decoder), 10016 (Audio Worker).
  → aucun marqueur d'interblocage côté hôte.

---

## 8. Chemins de signalisation et verrou de trame (animation nominale)

Cycle nominal d'un quantum (reconstitué depuis le code, pas le trace) :

1. Le maître (`XThread192C`) parcourt son quantum via `sub_821E2CE8` : il attend d'abord `ctx[348]`
   (:480-486), décharge les slots finis, **signale** `ctx[352]` et `ctx[356]` (:612-621), puis attend
   `ctx[368]` (:622-628).
2. Les slots de jobs sont donc déchargés dans la même passe : `sub_821E2B98` → `sub_82475210` — pour chaque
   slot à drapeau posé, le maître attend l'event de complétion puis binne le drapeau (§6.3).
3. Les workers (CGameWorkerThread et familles) sont réveillés par des `NtSetEvent` (`sub_821AEC28`,
   ~40 sites) sur leurs events individuels créés à l'init (`sub_827DE618`, §3), exécutent leur travail, puis
   posent l'event de complétion du slot — l'event que le maître attend dans `sub_82475210` (:6805).
4. Le main XAPI patiente sur `GameMainThreadEvent` pendant tout ça (§5) — il ne participe pas au quantum.

Dans le trace, **le maillon 3 est rompu** : le maître attend un slot précis (empêtré dans le noyau dans
`sub_82475210`), tous les workers restent parkés sur leurs events d'éveil — l'event de complétion attendu
n'a jamais été posé (ou a été détruit puis re-créé avant la consommation).

---

## 9. Hypothèses de cause racine (classées)

**H1 (principale) — Slot de job « orphelin » dans le tableau de jobs (stall du pipeline).**
Le maître est arrêté sur l'event de complétion d'un job du slot `i` (wwe12_recomp.54.cpp:6805), jamais
signalé. Étiologies compatibles avec la photo de stacks :
 a. le worker qui a pris le job s'est re-parké sans le signaler (chemin de reprise tombé dans une attente) —
    signal perdu ou double-consommation de l'event ;
 b. le job a été posté mais jamais pris, et le maître attend le handle du slot plutôt qu'un event
    « file non vide » (erreur/état non traité) ;
 c. une exception/chemin d'erreur a court-circuité le signal — le canal de diagnostic `0x8317B770 + i*4`
    plus `sub_827D5C28`/`sub_8236BE08` (:6829-6844) prêt à être activé pour le vérifier.
*Ce qui la fait pencher* : cohérence du trace (maître accroché 8 frames sous les contrôles du slot, workers
tous parkés), et le fait que ni les GPU threads ni le main XAPI ne bougent — le rendu en amont est figé,
pas un problème GPU/Present.

**H2 (secondaire) — interlock de quantum : les events 348/368 de `sub_821E2CE8`.**
Même famille que H1 mais sur les events d'interlock plutôt qu'un slot de job : un participant attendu (par
ex. l'instance 18380 sur main de table `CGameMainThread`, ou un sous-système groupe B/C) n'a pas répondu à
son quantum. **Cependant** le trace montre le maître *déjà dans* la décharge (`sub_82475210` est sous
`sub_821E2B98`, donc l'attente 348 est franchie) : le stall est en aval, pas sur 348/368. H2 reste le cadre
général, H1 en est l'instanciation précise.

**H3 (démystification) — le park `GameMainThreadEvent` n'est PAS la cause.**
État inactif normal (§5). Inclus pour trancher la question « pourquoi le XThread principal attend sans fin ? »

**H4 (écartée) — deadlock du hook D3D / Present.**
GPU Commands simplement en attente de paquet (`hang_trace_940mx.txt:63-73`), VSync en sommeil vivant
(:75-83), hooks D3D intacts, aucune entrée suspensive anormale côté hôte.

**Note croisée** : le dossier connu « turbo à 4 fils / interblocage » (worktree `recomp\`) est une panne
différente et indépendante — ne pas le mélanger avec ce trace.

---

## 10. Prochaines pistes d'instrumentation (pour confirmer H1)

1. Instrumenter `sub_82475210` : dump périodique (script hôte ou breakpoint conditionnel) des champs
   `tableau + (i+7)*4` (handle attendu), `tableau + (i+16)*4` (drapeau) et `0x8317B770 + i*4` (diagnostic).
2. Vérifier le mode du wait du slot (retour 257 vs signal paramétré) via la boucle de `sub_821B04D0`
   (wwe12_recomp.172.cpp:93) — un compteur d'appels noyau suffit.
3. Compter les `NtSetEvent` sur `ctx[352]/[356]` (points d'appel de `sub_821AEC28`) : si le signal du
   maître n'arrive pas, le vrai blocage est en amont, au niveau d'un worker parké en prise de travail
   (comme le worker `12712` dans `sub_8281A2C8+0x156`).

---

## 11. Outils produits

- `work\rdc\map_lookup.py` : offset image wwe12.exe → symbole (101 674 symboles, grep libre), transcription
  Python du `map_lookup.ps1` (bloqué par l'execution policy PowerShell).
- `work\rdc\xex_static.py` / `work\rdc\xex_thread_table.py` : XEX2 → dump des données statiques invitées
  (chaînes, descripteur de bootstrap, table des threads), réutilisant le loader de `tools\find_fptrs.py`.
- `work\rdc\lookup_many.ps1` (intermédiaire abandonné, remplacé par `map_lookup.py`).

## 12. Corrections par rapport aux notes de la session précédente

- `sub_827DE6B8` n'est **pas** un enregistreur d'event du park : c'est `NtResumeThread(struct[4])`
  (wwe12_recomp.121.cpp:15113-15120 → wwe12_recomp.185.cpp:62-66). En conséquence, `sub_821AD1E8`
  **reprend** le thread moteur issu du descripteur, il ne l'enregistre pas.
- Le descripteur de bootstrap est à **`0x820018B8`** (r4 = `0x82000000+6328`), pas `0x820018C8` : ses 5 mots
  sont `{pile 0x20000, main 0x821AD2B8, cœur 0, priorité 0, nomEvent → 'CGameMainThread'}` ; le
  **main = 0x821AD2B8** correspond exactement au bas de stack de XThread192C (§6, frame #13).
- Adresses globales corrigées par la table des `lis` (conversions du checkpoint fausses) :
  - contexte du quantum dans `sub_821E2CE8` : **`0x83113890`** (base `lis -31983` = `0x83110000`), pas
    `0x82213890` ; les sous-globals de contrôle sont `0x83113884`/`0x83113888` (`:503-545`) ;
  - registre de diagnostic de `sub_82475210` : **`0x8317B770`** (base `lis -31976` = `0x83180000` − 18576),
    pas `0x831BB770` ;
  - compteur de mémoire libre de `sub_827DE618` : **`0x83242B90`** (base `lis -31964` = `0x83240000`),
    pas `0x83202B90` ;
  - `ExCreateThread` reçoit bien le trampoline **`0x821B3F58`** en r6 (base `lis -32229` = `0x821B0000` +
    16216, wwe12_recomp.104.cpp:85-92) — cohérent avec le trampoline d'entrée du §2.
- `0x82001AEC` contient la chaîne `D:\` (graine de buffer pour `sub_827DFA20`), pas un nom d'event : le nom
  d'event du park est `GameMainThreadEvent` à `0x82001AC0`, et l'event individuel du thread moteur est
  nommé `CGameMainThread` (recopie de l'entrée de table).
- Les seuls RVAs/symboles hérités du checkpoint à être restés valides sont ceux retrouvés dans le trace et
  la map ; le pair `RVAs 0x000300F0/0x000406A0/0x000407B0` en tête de checkpoint reste abandonné (halluciné,
  cf. décision précédente).
