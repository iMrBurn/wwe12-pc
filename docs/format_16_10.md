# Vrai 16:10 dans WWE '12 : étude

## État au 2026-11-12 (Rendu)

Le vrai 16:10 est fait (`wwe12_widescreen`, commits 1ec0082 et a11e428, livré depuis la 1.0) : en match, plus d'image
en haut et en bas ; HUD dans la bande 16:9 du milieu. **Reste à confirmer** (feuille de route) : menu pause en match,
retour aux menus, 21:9 avec HUD, D3D11 / D3D12, 360p / 480p, et un vrai Steam Deck. La section 1 (constante caméra
`0x8200BC60`, FOV) reste la référence.

Parties finies archivées dans `docs/archives/format_16_10_anciennes_parties_2026-11-12.md`.

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

## État (2026-10-31)

Go de l'utilisateur pour le code ; tests faits par Main (fenêtre 1280x800, données isolées).
- **Étape 1** (commit 1ec0082) : `native/wwe12_widescreen.cpp` (constante caméra, FOV vertical
  élargi dans les 3 fonctions ; la page de la constante est en lecture seule, elle est rendue
  inscriptible une fois, sinon plantage), cibles de scène en échelles X/Y (`svr_resources.cpp`),
  présentation à la forme de la cible (`present.cpp`). Entrée et match remplissent la fenêtre.
- **Étape 2** (commit a11e428) : sprites du HUD dans la bande 16:9 du milieu (`hooks/draw.cpp`,
  `SvrHudSprite`, d'après une capture F9 : quads UP en pixels 1280x720, viewport actif, sans
  profondeur). Signal « scène » : surface par tuiles OU dessins 3D plein écran (certains plans de
  caméra n'utilisent pas les tuiles ; sans ça l'image passait en 16:9 à chaque plan).
- **Reste à vérifier** : menu pause en match, retour aux menus (doivent repasser en 16:9), 21:9 avec
  HUD, `--wwe12_widescreen=false`, barres de vie / noms / invites, D3D11 et D3D12, 360p/480p,
  ligne FORMAT (AUTO = vrai 16:10, à confirmer par l'utilisateur), readmes et launcher (nouvelle cvar).

Règle : le go de l'utilisateur pour chaque compilation et chaque lancement du jeu.
