# Vrai 16:10 dans WWE '12 : étude

Rédigé par la conversation Rendu le 2026-10-31, à la demande de Main (le 16:10 passe dans la 1.0,
décision de l'utilisateur). **Étude seulement** : rien n'a été compilé, le jeu n'a pas été lancé.
Base : le port SvR 2011 de Kaiko (clone local `work/kaiko-svr2011`, `native_renderer.cpp`
~869-915 et ~1772-1925, `native_hooks.cpp` ~160), qu'on peut adapter avec crédit (son accord
écrit du 2026-10-26, `THIRD_PARTY.md`).

## Aujourd'hui

Le jeu dessine toujours en 16:9. Sur un écran 16:10 (1280x800, 1920x1200, Steam Deck) :
- **FORMAT = AUTO** (`bd_aspect_ratio` 5) : image 16:9 avec des bandes noires en haut et en bas ;
- **FORMAT = ÉTIRER** (`bd_aspect_ratio` 6) : l'image remplit l'écran, mais tout est étiré de ~11 %
  en hauteur.

Le « vrai 16:10 », c'est : en match, la caméra montre **plus d'image en haut et en bas** (même
largeur de vue qu'en 16:9), sans rien déformer. Le HUD reste au milieu, et les menus restent en 16:9
avec des bandes noires.

## 1. Ce qu'on a trouvé dans WWE '12

### La constante de rapport de la caméra : `0x8200BC60`

Elle vaut 16/9 (`0x3FE38E39`). Juste avant, `0x8200BC5C` vaut 4/3. C'est le même principe que la
constante `0x82007724` de Kaiko dans SvR 2011. Trois fonctions la lisent, et elles touchent toutes
la caméra :

| Fonction | Rôle (lu dans le code recompilé) |
|---|---|
| `sub_82824370` | Mise à jour de la projection d'une caméra. Elle choisit 4:3 ou 16:9 selon un réglage global (`[0x8324CE38 + 4]` : 0 = 4:3, 1 = 16:9). En perspective (`+480` = 0), elle appelle `sub_82823CB0` avec le FOV vertical (`+512`), le rapport, puis near et far (`+500` / `+504`). En orthographique (`+480` = 1), elle appelle `sub_82823BF0`. |
| `sub_823BE870` | Construit le volume de visibilité (*frustum*) de la caméra courante, pour **le tri des objets visibles**. Même choix 4:3 / 16:9, avec le FOV `+512` de la caméra. |
| `sub_826907B0` | Un autre volume construit à partir de la vue (8 coins, `tan(FOV/2) × 16/9`). C'est très probablement **l'ajustement des ombres du soleil** à ce que voit la caméra. |

Conséquence : en changeant la valeur de `0x8200BC60` en mémoire (comme Kaiko), la projection, le
tri des objets et les ombres suivent ensemble. On n'a rien d'autre à chercher pour le rapport.

### Le FOV pour le 16:10 (Kaiko : « tall »)

En 16:10, on veut garder la même largeur de vue et en voir plus en hauteur. Il faut donc :
- un rapport = 16/9 ÷ `tall` ;
- un FOV vertical élargi : `2·atan(tan(fov/2) × tall)`.

Avec `tall` = (16/9) ÷ (16/10) = 1,111.

Kaiko élargit le FOV seulement pendant l'appel de sa fonction de projection (`sub_825818B8`), puis
le remet. Chez nous, le FOV `+512` est lu à deux endroits :
- `sub_82824370`, la projection ;
- `sub_823BE870`, le tri des objets.

Il faut l'élargir aux deux endroits. Sinon, des objets tout en haut ou tout en bas de l'image
risquent de disparaître. `sub_826907B0` (les ombres) lit le FOV à `+104` d'un autre objet. Il
faudra vérifier en jeu que les ombres couvrent bien les bords.

## 2. Ce que ça touche chez nous

Notre rendu applique **une seule échelle** par cible (`SvrSurfaceScale`, la même en largeur et en
hauteur). Le 16:10 demande une échelle différente en hauteur pour la cible de la scène. C'est le
plus gros du travail.

| Partie | Fichier | Ce qu'il faut faire |
|---|---|---|
| Taille des cibles | `native/svr_resources.cpp` (`SvrResolveSurface`, `SvrResolveTarget`) | Seulement les cibles plein écran de la scène (1280x720 ou plus) deviennent `tall` fois plus hautes. Les petites (bloom, flou de profondeur) restent en 16:9 : elles lisent la scène en coordonnées 0-1, comme chez Kaiko. |
| Rendu par tuiles | idem (`g_tiled_extent`, 320x736 → 1280x736) | L'étendue reste en pixels du jeu. Seule la taille côté PC change. À vérifier : les tuiles en 16:10 et le premier match après un menu. |
| Viewport, scissor, résolutions (resolve) | `reblue/gpu/hooks/draw.cpp`, `reblue/gpu/draw.cpp` (`SvrApplyScissor`), `D3DDevice_Resolve_hook` | Une échelle X et une échelle Y au lieu d'un seul `k`. |
| Constantes des shaders | `reblue/gpu/constant_buffers.cpp` (`invRenderScale`, biais de mips) | Garder l'échelle de largeur pour le biais de mips. À vérifier : `invRenderScale` dans les effets en espace écran. |
| HUD 2D | `hooks/draw.cpp` (viewport) | Les sprites du HUD (dessinés à plat, sans profondeur) vont dans la bande 16:9 du milieu, sans être étirés. Les fondus plein écran remplissent tout. C'est la détection `FlatPixels` / `middle` de Kaiko, à refaire chez nous. |
| Écrans 2D, pause, chargement | `reblue/gpu/present.cpp` | Une image sans 3D est affichée en 16:9 avec des bandes. En match, seulement si ça dure 4 images : un plan de caméra sans scène pendant 1 ou 2 images ne doit pas faire clignoter les bandes. C'est le piège noté par Kaiko le 23 octobre. |
| Savoir si on est en match | à créer | Kaiko a `g_match_scene`. Chez nous, rien n'existe encore. Piste : la scène par tuiles (320x736) est dessinée, ou un état du jeu à trouver. Les menus 3D (création de catcheur, sélection) restent en 16:9. |
| Lecture des cibles par le processeur (read-backs) | `SvrAfterResolve` | Aujourd'hui, une image en 16:9 n'est jamais relue par le processeur. Ce test se fait sur la taille côté PC (`dst->width × 9 == dst->height × 16`). Avec une cible 16:10, il ne marcherait plus, et la scène serait relue à chaque image : image noire ou bruitée, et lenteur. **Il faut tester la taille du jeu, pas la taille côté PC.** Même chose pour `screen_like` (`width × 9 == height × 16`). La peau des catcheurs (composites k_4_4_4_4) n'est pas concernée : ce ne sont pas des cibles plein écran. |
| Résolutions 360p / 480p | `SvrRenderScale` | Ça se combine : l'échelle Y = l'échelle × `tall`. En 360p, la cible fait 640x400. Rien de spécial, à part arrondir à un nombre pair. |
| FORMAT AUTO / ÉTIRER | `wwe12_graphics_menu.cpp`, `reblue/gpu/output.cpp` | AUTO deviendrait « vrai 16:10 en match, bandes ailleurs ». ÉTIRER ne change pas. Une valeur « BANDES » (16:9 partout) peut rester disponible par cvar, comme `native_widescreen` chez Kaiko. **À faire décider par l'utilisateur.** Attention : une page d'options a 5 lignes au plus. |
| Vidéos (Bink, entrées) | `present.cpp` (`SofdecPlayer::Playing`) | Elles restent en 16:9 avec des bandes, comme maintenant. |
| DirectX 11 / DirectX 12 | aucun fichier propre | Tout se passe dans le code commun (tailles, viewports, présentation). Le `depthFlip` de DX11 et `d3d11_slots.cpp` ne changent pas. Il faut quand même tester les 3 exe. |
| Écrans plus larges que 16:9 (21:9) | | Hors sujet pour la 1.0. Le même code le permettrait plus tard (`wide` chez Kaiko). |

Choix proposé : la forme (16:9 ou 16:10) est calculée au démarrage d'après la fenêtre. Si la
fenêtre change de forme en cours de partie, la nouvelle forme s'applique quand le jeu recrée ses
cibles, à la scène suivante. Kaiko, lui, attend que la carte graphique ait fini, vide toutes les
cibles et les recrée. On pourra faire pareil si c'est nécessaire.

## 3. Estimation et risques

**Environ 3 séances**, sans compter les tests des joueurs :
1. La constante et le FOV, puis des cibles de scène plus hautes en match. Test dans une fenêtre
   1280x800 : un match, une entrée, une scène coupée. Le HUD sera étiré à cette étape.
2. Le HUD au milieu, les écrans 2D avec bandes, la détection « en match », et les read-backs et
   `screen_like` sur la taille du jeu.
3. Les tests : Vulkan, D3D12 et D3D11 ; 360p, 720p et 1080p ; Steam Deck (16:10) si possible ;
   ralentis et replays, création de catcheur, Road to WrestleMania, ligne FORMAT.

Risques, du plus probable au moins probable :
- **Le HUD** : quelques éléments mal classés, étirés ou décalés (barres de vie, noms, flèches,
  QTE). La détection « 2D à plat » de Kaiko a demandé plusieurs corrections chez lui.
- **Les effets en espace écran** (flou de profondeur, bloom, reflets du soleil). S'ils lisent des
  constantes en pixels 1280x720, ils seront légèrement décalés en haut et en bas.
- **Le tri des objets et les ombres** : du public ou des cordes qui disparaissent tout en haut ou
  tout en bas, ou une ombre coupée au bord.
- **Les entrées** : leurs caméras de cinéma sont cadrées pour du 16:9, donc on verra plus de
  plafond et de sol. C'est voulu (même choix que Kaiko), mais l'utilisateur doit le valider en jeu.
- **Le rendu par tuiles** : un ou deux clignotements noirs au premier match, comme avant le
  correctif `g_learned_extent`.

Si le résultat déçoit, le repli est simple : la cvar coupée = exactement le comportement
d'aujourd'hui (AUTO avec bandes, ÉTIRER).

## Prochaine étape

Il faut le **go de l'utilisateur** avant d'écrire du code, puis son go pour chaque compilation et
chaque lancement du jeu.
