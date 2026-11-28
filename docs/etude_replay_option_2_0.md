# Option « pas de replay d'après-match » (2.0) : étude

Demande d'iMrBurn (via la Main, 2026-11-28) : une option pour couper le replay qui passe après la cloche, dans le
launcher et/ou dans le jeu. Étude en lecture seule (code généré + nos sondes), **rien n'est compilé ni testé**.

## 1. Ce qui existe déjà

- **Android** (`port/src/android/wwe12_replay.cpp`, cvar `wwe12_instant_replay`, défaut false) : met
  l'interrupteur du jeu `0x83174250` à 0 après `sub_822B6D30`. Plus aucun moment fort n'est enregistré, donc pas de
  replay, **et MATCH HIGHLIGHTS est grisé**. Les joueurs Android jouent comme ça depuis l'alpha 5 : les matchs
  finissent bien (victoire, menus, Univers). C'est une preuve que « pas de replay » ne bloque pas la fin de match.
- **PC** : aucun crochet, replay toujours là. Sondes `[replay]` dans `wwe12_game_log.cpp` (enregistrement seulement).

## 2. Où le jeu décide de jouer le replay

`sub_823F4910` (objet en r3, état à +8) est un automate appelé à chaque image en fin de match. États : 0 → 1 →
(5 = replay en cours) → 7 = fini (au-delà de 6 la fonction ne fait plus rien, la suite du jeu prend le relais).

Dans l'état 1 (0x823F4A10 .. 0x823F4D04), il calcule +28 = « jouer le replay ? » [lu dans le code, PROUVÉ] :

| Mode de jeu (G+12460, G = 0x8317C850) | Replay si… |
|---|---|
| 0 = exhibition, Univers (et nos modes 1.4 : I Quit, Special Guest Referee, finishers…) | +24 ≠ 0, ou au moins un moment fort (`sub_821EDEE0`) |
| 2 = Road to WrestleMania | +24 ≠ 0 seulement |
| 3 / 4 = Create a Story (jouée / aperçu) | jamais : `sub_821D49A0` = 1 force +28 = 0 |

(+24 = `sub_8267CFC0`, lit G+321/+322 : sans doute « changement de titre » ; il déclenche aussi un son, 0x188A7.)

Puis : +28 ≠ 0 → `sub_824EEF90` (lance le replay) et état 5 ; **+28 = 0 → état 7 directement**
(0x823F5144). C'est le chemin que le jeu prend lui-même en Create a Story, et en exhibition quand le match n'a aucun
moment fort (le cas Android). La scène de victoire, REMATCH / MATCH HIGHLIGHTS et la sauvegarde Univers viennent
après, hors de cet automate.

Que `sub_824EEF90` soit bien le replay d'après-match : PROBABLE (il mène à `sub_82512CA0` / `sub_82512BB0`, qui
lisent aussi « au moins un moment ? » ; la lecture de `Spectacular.rps` est dans `sub_822EAAB0` ← `sub_8252E848`).
À confirmer par une ligne de journal avant tout test joué (voir §5).

## 3. Comment le sauter proprement (proposition)

**Voie recommandée : forcer le chemin « Create a Story » de l'état 1, rien d'autre.**

- Crochet sur `sub_823F4910` : si l'option est active et que l'état (+8) vaut 1, lever un drapeau (thread_local)
  pendant l'appel à l'original.
- Crochet sur `sub_821D49A0` : si le drapeau est levé, rendre 1 sans appeler l'original.
- Dans l'état 1, `sub_821D49A0` n'est appelée qu'une fois (0x823F4AA4) : les trois autres appels de la fonction
  (états 5 et suivants) ne sont pas touchés, et tous les autres appelants du jeu non plus.
- Résultat attendu : +28 = 0 → état 7, comme en Create a Story. Les moments forts restent enregistrés :
  **MATCH HIGHLIGHTS marche toujours** (différence avec la voie Android).
- Taille : un seul fichier (`port/src/native/wwe12_skip_replay.cpp`), ~40 lignes, aucun code généré à refaire.

**Voie de repli : celle d'Android** (`0x83174250` = 0). Déjà prouvée par des centaines de matchs, mais elle
grise MATCH HIGHLIGHTS ; bonus : plus d'écriture de `Spectacular.rps` (150 Mo) pendant le match.

Ce qui saute aussi avec la voie recommandée (comme en Create a Story) : le travail de l'état 5 (sons 13 / 14,
`sub_8270C4E8(7/12/44/47)`, sans doute des messages ou une transition après le replay). Le chemin « aucun moment
fort » l'évite déjà sans dégât connu. Seul point à surveiller : un **match pour un titre en Road to WrestleMania**
(+24 ≠ 0 et +28 = 0 n'arrive jamais tout seul dans ce mode ; le bloc RTWM 0x823F4B94 tourne alors).

## 4. Clé et réglage (à valider par iMrBurn, aucune clé créée)

- Clé proposée : `wwe12_skip_replay` (bool, **défaut false** = comportement d'origine), dans `wwe12.toml`.
- Launcher : une case dans GAMEPLAY, « Skip the instant replay after matches » (pour la session Launcher).
- Menu OPTIONS du jeu : possible plus tard, comme GRAPHICS / JUKEBOX (vrai menu Lua, `wwe12_graphics_menu.cpp`),
  mais plus long ; je propose le launcher d'abord.
- Android : `wwe12_instant_replay` peut rester tel quel ; la nouvelle clé pourrait plus tard le remplacer
  (replay coupé mais MATCH HIGHLIGHTS gardé), si le gel Android vient bien de la lecture et non de l'écriture : pas sûr
  (le gel est dans le fil qui ÉCRIT le fichier), donc ne rien changer côté Android pour l'instant.

## 5. Plan

1. Écrire le crochet + une ligne `[replay] skip` (mode, +24, état) ; compiler (en prévenant la Main / Rendu).
2. Vérifier dans le journal, sans jouer de vrai test, que l'état passe 1 → 7.
3. Test joué (iMrBurn) : exhibition 1 contre 1 (tombé), match pour un titre en exhibition, match Univers + sauvegarde,
   match RTWM pour un titre si possible, histoire Create a Story, puis MATCH HIGHLIGHTS et REMATCH. Avec l'option
   coupée : rien ne change.

Estimation : 1 session courte pour le code et le journal ; le test joué en plus.

## 6. Premier essai (2026-11-28) et nouvelle voie

- **Essai 1, raté** (exe de 16:25, `test_replay.log`) : le replay est passé quand même, aucune ligne `[replay] skip`.
  Le journal montre le replay **avant** la scène de victoire (script 6139 à 16:29:46). `sub_823F4910` tourne après
  cette scène : ce n'est pas lui qui décide du replay d'après-match. Le crochet a été retiré. (§2-3 restent justes pour
  cet automate, mais il ne s'agit pas de ce replay-là.)
- **Voie 2 (piste d'iMrBurn : MATCH CREATOR > ENVIRONMENT > POST MATCH REPLAY)** : le Match Creator garde une fiche
  de 28 octets par type de match (119) dans le bloc de réglages `sub_82684BE8()` (0x831CEEA8) + 0x57786 + type × 28 :
  octet 0 type de ring, 1 ENTRANCE, 2 POST MATCH REPLAY, 3 MOMENTUM (lu ainsi par le panneau, `sub_82593F88`). Les
  mises en place de match en recopient 25 octets dans leurs règles avec `sub_82A3DEF0(dst, src)` (`sub_825493B0`,
  `sub_828A99D0`, `sub_826EAAF8`…). Le crochet met l'octet 2 à 0 dans la copie (jamais dans le bloc lui-même, donc la
  sauvegarde n'est pas changée), avec une ligne `[replay] skip … POST MATCH REPLAY x -> 0`.
- Non prouvé : que 0 = OFF (ordre des textes OFF/ON du panneau) et que l'exhibition lise bien cet octet. L'essai 2 le dira.
