# Chargements : où partent les secondes, et ce qu'on peut encore gagner (1.2)

Étude de Gameplay, 2026-11-06, à la demande de Main. **Lecture seule** : rien compilé, rien lancé,
rien dans la 1.1. Sources : notre code (`port/src/native/wwe12_loading.cpp`, `wwe12_load_turbo.cpp`,
`wwe12_wait_probe.cpp`), nos anciennes mesures (`docs/archives/claude_md_anciens_plans_2026-11-06.md`,
section « Chargements, suite »), le journal `ecran_scene.log` du 2026-11-06 (build actuelle, turbo coupé)
et la doc publique du recomp WWE 2K14 de Jarrod (README, `docs/release/`, noms des fichiers).

Point de départ (l'utilisateur, via Main) : chez Jarrod on voit à peine « now loading ». Chez nous :
~6 s de chargement de match avant l'entrée, puis ~5 s entre la fin des entrées et le match.

## 1. Où partent les secondes aujourd'hui

**Résumé : presque tout le temps est de l'attente d'images, pas du travail.** Le PC lit, décompresse et
prépare tout en bien moins d'une seconde ; le jeu, lui, avance d'un pas par image (1/60 s), comme sur
la console.

| Poste | Mesure | Part du temps |
|---|---|---|
| **Lectures disque** (VFS, `sub_827EA330`, blocs de 256 Ko) | 32 Mo lus en 12 ms (sonde du 2026-10-16). Journal du 2026-11-06 : chaque seconde de chargement lit 0 à 5 Mo en 0 à 3 ms. | Négligeable |
| **Décompression** (zlib `sub_827F57A8`, BPE, LZX) | inflate ~25 ms par match ; BPE invisible dans les piles. | Négligeable |
| **Textures et pipelines** | Journal : 95 textures en 54 ms au pire ; pipelines déjà préparés (`pipelines_seed.w12p`). | Faible (< 0,1 s) |
| **Attentes volontaires du jeu** (Sleep / attente avec limite) | Sonde `wwe12_wait_probe` : aucune autre attente cachée. Le seul frein trouvé (500 ms après chaque bloc de 256 Ko autour des entrées) est **coupé depuis le 2026-11-02** (`wwe12_read_throttle=false`, commit 71bcf03). | Plus rien de connu |
| **Le chargeur cadencé par l'image** (`sub_82819258`) | Le jeu lui donne 1 ms par image ; nous 12 ms (`wwe12_load_slice_ms`). Avant : ~12 s, après : ~6 s. | Déjà réduit de moitié |
| **Les fils du jeu cadencés par l'image** (barrière `sub_827DEA20`) | Chaque fil fait sa part puis attend l'image suivante. Le jeu fait **des centaines d'images de préparation** ; à 60 images/s cela fait des secondes, quoi que fasse le PC. Sur ~4 s d'écran de chargement, le chargeur ne travaille que ~1 s. | **L'essentiel** |
| **Fin des entrées → cloche** (~5 s) | Écran de chargement de fin d'entrées (~4,5 s sans turbo, ~2 s avec) puis la transition `WWE12_Transition` (~2 s, animation visible) et la marche des catcheurs vers leur place (~2,5 s, du vrai jeu). | Moitié attente d'images, moitié vrai jeu |

Ce qui accélère vraiment, c'est donc de **faire tourner le jeu plus vite que 60 images/s pendant les
écrans de chargement**. C'est le « turbo » (`wwe12_load_turbo`). Il est écrit et marche, mais il est
**coupé par défaut** (décision de l'utilisateur du 2026-10-17, après un chargement infini sur 4 fils).

Mesures avec le turbo (2026-10-16, avant la coupure du frein de lecture) :
- chargement de match ≈ 4,0 → 2,8 s ;
- écran de fin d'entrées ≈ 4,5 → 2 s ;
- dans les ~2,7 s restantes : ~1 s de vrai travail du chargeur, puis **une lecture d'1 s pour
  750 Ko** juste après les voix du speaker. 750 Ko = 3 blocs de 256 Ko, avec 500 ms de sommeil entre
  les blocs : c'est très probablement le frein de lecture, **déjà coupé depuis**. À confirmer par une
  mesure ; si c'est bien lui, le chargement turbo tombe vers ~1,7 s.

Note : le « ~6 s » de Main et le « 4,0 s » ci-dessus ne mesurent pas le même intervalle (le premier
part du choix du match, le second de l'ouverture de l'écran de chargement). La mesure du point 4 les
remettra sur la même règle.

## 2. Attentes encore retirables

| # | Quoi | Réglage | Gain estimé | Risque |
|---|---|---|---|---|
| 1 | **Turbo par défaut** sur les écrans de chargement | `wwe12_load_turbo = true` (existe déjà ; dans `wwe12.toml` seulement, ni launcher ni menu) | Match : ~1,2 s, et jusqu'à ~2 s si le point 3 se confirme. Fin d'entrées : ~2,5 s. **Le plus gros gain.** | Un chargement infini vu une fois sur 4 fils (2026-10-17), plus revu depuis `wwe12_gpu_wait_yield` (7 chargements turbo d'affilée OK sur 4 fils, 2026-10-20) ; contre-épreuve jamais faite. Le petit rond de chargement tourne plus vite (visible). **Décision de l'utilisateur à reprendre.** |
| 2 | Proposer le turbo dans le launcher et OPTIONS > GRAPHICS > ADVANCED | ligne « FAST LOADING » | Le gain du 1 pour ceux qui le veulent, sans changer le défaut | Faible |
| 3 | Vérifier que la « lecture d'1 s pour 750 Ko » a disparu | aucun (`wwe12_read_throttle` déjà à false) | ~1 s si elle est encore là | Aucun (mesure) |
| 4 | Budget du chargeur au maximum | `wwe12_load_slice_ms` 12 → 15 | Petit (le chargeur a déjà ~12 ms sur 16,7) ; sans effet avec le turbo | Image du chargement plus saccadée sur petit PC |
| 5 | Turbo aussi pendant `WWE12_Transition` (fin des entrées) | nouveau cvar, ex. `wwe12_load_turbo_transition` | ~1-1,5 s | **Visible** : l'animation de transition passe en accéléré ; l'enregistreur du replay tourne déjà (le turbo change le lien images/temps). À voir avec l'utilisateur. |
| 6 | Écourter la marche des catcheurs avant la cloche | nouveau cvar | ~2 s | **Change le jeu** (c'est du vrai jeu, pas un chargement) ; replay et caméra. Déconseillé. |
| 7 | zlib / BPE / LZX en natif | — | < 0,03 s | Inutile chez nous (mesuré) |
| 8 | Horloge Windows 1 ms | `wwe12_precise_timer` | 0 (mesuré 2026-10-12) | — |

## 3. Ce que fait Jarrod (WWE 2K14, BSD-3), d'après sa doc publique

Lu seulement : README, `docs/release/README.md`, `docs/release/CHANGELOG.md`, et les **noms** des
fichiers de `patches/rexglue-sdk/` et `src/`. Aucun code lu, rien téléchargé.

- **Sa recette** (son explication à l'utilisateur, notée le 2026-10-16) : chargement d'avant-entrée
  5,5 → 0,9 s, dans l'ordre de ses gains :
  1. le chargeur enchaîne plusieurs étapes par image (chez nous : `wwe12_load_slice_ms`, fait) ;
  2. **turbo pendant les écrans de chargement seulement** : l'horloge vblank du jeu tourne très vite
     (vsync coupée, ~400 images/s). Le plus dur : le début et la fin exacts de la fenêtre. Il bride
     les animations 2D à 1/60 s réel (le rond ne s'emballe pas) et arrête au premier vrai mouvement
     des catcheurs (chez nous : turbo fait, coupé par défaut ; la fin de fenêtre est faite, commit
     028823d ; **le bridage des animations 2D n'est pas fait**) ;
  3. profiler ses propres crochets ; 4. vblank sur minuteur précis ; 5. zlib natif (gain réel chez
     lui, pas chez nous : voir le point 7).
- **Noms de fichiers qui le confirment** : patchs SDK `0007-session14-vblank-rate-override`,
  `0008-session15-vblank-unthrottled`, `0009-session15-skip-frame-draws` (= notre saut d'images),
  `0010-session15-det-frames` ; dans `src/` : `load_turbo.h`, `load_pipeline_wait.h`, `load_probe.h`,
  `barrier_probe.h`, `native_bpe.h`, `lzx_probe.h`, `background_speed.h`.
- **Son journal des versions** ne chiffre pas les chargements. Il en parle ainsi :
  - v0.1.0 annonce « Fast loads » ;
  - v0.2.2 : un décompresseur plus rapide, gardé pour les chargements de match seulement (dans les
    menus, il causait une course) ;
  - v0.2.17 / v0.2.18 : des chargements infinis venus de deux caches du jeu activés à tort, puis
    retirés ;
  - aucune option « turbo » n'est montrée aux joueurs : chez lui, c'est actif par défaut.
- **La différence principale avec nous : son turbo est actif pour tout le monde, le nôtre est coupé.**
  Le reste (chargeur par tranches, lectures, décompression) est au même niveau ou sans effet chez nous.
- Reprise de code : rien à reprendre pour l'instant ; notre turbo est déjà écrit. Si on veut son
  bridage des animations 2D, on lit son `load_turbo.h` **après accord de l'utilisateur**, on réécrit,
  et on crédite dans `THIRD_PARTY.md` (Jarrod y est déjà).

## 4. Proposition pour la 1.2

1. **Mesure (une séance, go de l'utilisateur pour lancer)** : `tools/dev/lancer_mesure_chargement.bat`
   (`wwe12_load_probe`), un même match 3 fois sans turbo puis 3 fois avec, sur tous les cœurs puis
   sur 4 fils (`lancer_test_turbo4.bat`). On chronomètre : choix du match → écran de chargement →
   entrée → fin des entrées → cloche. On vérifie aussi que la lecture de 750 Ko a disparu (point 3).
   ~1 h.
2. **Bridage des animations 2D** pendant le turbo, pour que le rond tourne à vitesse normale (idée de
   Jarrod, réécrite) : 2-3 h. C'est ce qui rend le turbo « invisible ».
3. **Contre-épreuve de l'interblocage sur 4 fils** (`wwe12_gpu_wait_yield=false`, turbo, 4 fils) :
   comprendre plutôt que croire qu'il a disparu. 1-2 h.
4. Puis **demander à l'utilisateur** : turbo actif par défaut ? (sinon une ligne « FAST LOADING »
   dans le launcher et le menu, point 2).
5. Le point 5 (transition en turbo) seulement si l'utilisateur accepte l'accéléré visible.

Gain attendu après 1 à 4 : chargement de match ~1,7-2,8 s au lieu de ~4 s (chronométrage à refaire),
fin des entrées → cloche ~2,5 s plus court. La marche des catcheurs (~2,5 s) reste : c'est le jeu.
