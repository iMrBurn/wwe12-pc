# Étude : la souris dans les menus (pour la 2.0)

Date : 2026-11-28. **Étude seulement** : rien n'est codé, compilé ni testé en jeu.

Ce qu'on veut : dans les menus, **survol = sélection**, **clic gauche = A**, **clic droit = B**,
**molette = haut / bas**. Jamais pendant un match. Sans réécrire chaque menu.

Résumé en une ligne : on **n'écrit jamais le curseur du jeu**. La souris **appuie sur les touches
de la manette** (croix, A, B) dans le crochet `XInputGetState` qu'on a déjà. Pour le survol, on lit
la **place des lignes** dans les calques du jeu (les « éléments » nommés), puis on appuie sur
haut / bas jusqu'à ce que le curseur du jeu soit sous la souris. Jarrod (2K14) et Kaiko (SvR 2011)
font pareil.

---

## 1. Comment les menus du jeu savent quelle ligne est choisie

Il n'y a pas **un** menu. Il y a **trois familles**, mais elles partagent beaucoup.

| Famille | Exemples | Curseur / ligne choisie | Preuve |
|---|---|---|---|
| A. Menus de `menu.pac` | menu principal, PLAY, OPTIONS, sous-menus | objet menu M ; fiche choisie = `sub_8253A028(M)` → M+7248 ; index probable M+360 (à vérifier) | `wwe12_quit.cpp:297-368`, `wwe12_menu_patch.cpp:1-15`, code généré `sub_8253A028` (`wwe12_recomp.140.cpp:10016`), `sub_8253F9A0` lit M+360 / M+372 avant d'appeler `sub_8253C3F0` |
| B. Écrans « à calques » | Univers, classements, fiche Superstar, boîtes de choix, sélection d'arène | mot de l'écran (S+28 = ligne ou page, S+20 = sens de la liste) | `docs/univers.md:464-467`, `:593-606`, `:667-669` ; `wwe12_universe.cpp:44-48`, `:2795-2822` |
| C. Pages d'options en Lua | GAMEPLAY > IN-GAME OPTIONS, nos pages GRAPHISMES, JUKEBOX, DÉGÂTS | `yFrame.GetCursorV()` (curseur vertical du cadre) | `graphics_pages/page0.lua:25`, `jukebox.lua:25`, `docs/menus_natifs.md:18-30` |

Ce qui est **commun** (le vrai « moteur ») :

- **Les touches de l'image.** Chaque écran à calques reçoit les touches pressées de l'image en
  **S+64** (A = 0x10, B = 0x20, Y = 0x40, X = 0x80, haut / bas selon la table `0x8201AD80`).
  `sub_824D20A0(S)` les traite **à chaque image** : il parcourt la table `0x8201ADF0`
  ({masque, fonction} : 0x10 → `sub_824D1FB0` = A, 0x20 → `sub_824D2028` = B…). Vu dans le code
  généré (`wwe12_recomp.18.cpp:8176`) et dans l'image (`image.bin` à 0x1ADF0).
- **La mise à jour.** `sub_824D3458` = vtbl[1] de tous les écrans à calques (vtable à `0x8200CA54` :
  `sub_824D3458`, `sub_824D20A0`, …). Un seul crochet voit donc **tous** ces écrans.
- **Les calques (MnLay, YXMLBIN).** Chaque écran dessine des **éléments** de 240 octets :
  +4 x, +8 y, +12 / +16 échelle, +96 **nom**, +176 parent, +180 caché, +184 couleur, +200 texte
  (`wwe12_universe.cpp:2599-2650`, `:3645-3660`). Les lignes ont des noms : `option_line01_01`…,
  le curseur `CorsorSet06_01`… (`page0.lua:8-9`). Dans MATCH CREATOR, +180 marque la **ligne du
  curseur** (`docs/match_creator.md:92`). Le texte de la ligne choisie est **rouge**
  (0.80 / 0.08 / 0.08, `docs/univers.md:654`).
- **Le clic « décider ».** Tous les choix de `menu.pac` passent par `sub_8253C3F0` (déjà crocheté).

Les grilles ont leur propre code :

- Sélection d'arène : `sub_825A3AB8(écran, ligne, colonne)` pose le curseur ; ligne à +72,
  colonne à +88 ; grille 4 × 6 de cases à `kRecords + 176` ; `sub_825A6A98` dessine à chaque image ;
  `sub_825A56C8` = l'arène choisie (`wwe12_arena_pages.cpp:2000-2132`).
- Calendrier de l'Univers : `sub_8255C770` (saute les jours vides, `docs/univers.md:873`).
- Boîte de choix : `sub_8288F0A0` (lignes 0xD2 + i, `docs/univers.md:849`).
- Grille de lettres : `sub_824C9228(écran, widget)` dessine chaque ligne de texte
  (`wwe12_kbd.cpp:1-18`, `:190`).

**Réponse** : un moteur de calques commun (dessin, touches, éléments nommés), mais **chaque écran
garde son curseur à sa place**. Donc : pas un seul index à écrire. D'où le choix d'**appuyer sur
des touches** plutôt que d'écrire un index.

---

## 2. La manière la plus simple

### 2.1 Les touches (sûr, déjà prouvé)

Le crochet `sub_82A8C070` (`XInputGetState`, `wwe12_intros.cpp:69-99`) ajoute déjà des touches au
joueur 1 : Start pour les vidéos, `Wwe12RemotePadButtons()` pour la manette scriptée
(`wwe12_remote_pad.cpp`). Il crée même une manette « vide » quand aucune n'est branchée (l.91-95).
La souris peut faire **exactement pareil** : une fonction `Wwe12MousePadButtons()` à côté.

| Souris | Touche | Remarque |
|---|---|---|
| Clic gauche | A (0x1000) | seulement si la souris est dans l'image |
| Clic droit | B (0x2000) | partout hors match |
| Molette haut / bas | croix haut 0x0001 / bas 0x0002 | un cran = un appui |
| (option) Molette + Maj | LB / RB (pages, onglets) | utile pour la grille d'arènes |

Durée d'un appui : **1 à 2 images enfoncé, puis 1 relâché**. Jarrod a mesuré qu'« 1 enfoncé,
1 relâché » prend chaque appui (son `kHoldFrames`). Notre manette scriptée prend 120 ms, sûr mais
lent. Trop long = la répétition automatique du menu part (double ligne).

### 2.2 Le survol : d'où viennent les rectangles ?

| Piste | Idée | Pour | Contre |
|---|---|---|---|
| (a) Les dessins 2D | `SvrHudSprite` (`reblue/gpu/hooks/draw.cpp:116-140`) voit chaque quad en pixels 1280 × 720 | marche pour tout | il faut **reconnaître** la barre de surbrillance (texture, couleur) écran par écran ; fil de rendu ≠ fil du jeu ; textes et barres mélangés |
| (b) Les calques du jeu | les éléments nommés (`option_line01_0N`, `CorsorSet…`), leur x / y / échelle + ceux des parents | précis, stable, comme Jarrod et Kaiko | il faut d'abord **prouver** le calcul de la position à l'écran (parents, échelle du groupe) |
| (c) Relatif | molette = haut / bas, clic = A, clic droit = B ; survol seulement là où on a les rectangles | marche **tout de suite partout** | pas de survol au début |

**Recommandation : (c) tout de suite, puis (b) famille par famille.** (a) seulement pour vérifier.

Comment marche le survol avec (b) (méthode de Jarrod, `mouse_menu.h`, et de Kaiko, `menu_touch.cpp`) :

1. Chaque image, un crochet du dessin note les rectangles des lignes visibles (et l'image du jeu).
2. La souris passe sur **une autre** ligne → elle devient la **cible**. Une souris immobile ne fait
   rien : elle ne gêne jamais la manette.
3. Chaque image, on compare la ligne du curseur du jeu à la cible : on appuie sur haut ou bas, une
   fois, puis on attend que le jeu ait bougé. Le jeu joue son son, sa surbrillance, son aperçu.
4. Clic gauche sur la ligne : A **une fois le curseur arrivé**.
5. Garde-fous : 8 à 40 appuis au plus, 4 s au plus, on s'arrête si le curseur ne bouge plus.

Pour savoir « où est le curseur du jeu », on a deux moyens :
- le mot du curseur de l'écran (S+28, M+360, `GetCursorV`, grille +72 / +88) ;
- ou, sans le connaître, l'élément **en surbrillance** (+180 ou texte rouge).

---

## 3. De la fenêtre aux coordonnées du jeu

- **Les clics.** Le SDK passe par SDL : `WindowSDL::HandleMouseEvent`
  (`rexglue-sdk/src/ui/window_sdl.cpp:571-606`) envoie `OnMouseDown / Up / Move / Wheel` aux
  écouteurs, en **pixels physiques** (× densité). On s'abonne comme le clavier :
  `window->AddInputListener(&listener, 1000)` (`wwe12_kbd.cpp:185-188`).
- **Le mode « mnk ».** `mnk_mode = true` par défaut (`port/config/wwe12.toml:122`). Le pilote
  (`input/mnk/mnk_input_driver.cpp`) sait lier LMB / RMB / MMB à des touches (`ui/keybinds.cpp:141`),
  mais **pas la molette** (Kaiko le note aussi). `mnk_mouse` (souris = stick droit) est **éteint** :
  il cache et capture le curseur (l.474-499). Règle : si `mnk_mouse` est allumé ou si une touche est
  liée à LMB / RMB, notre souris de menu se tait (comme Jarrod, `MouseTaken`).
- **Le curseur Windows.** Personne ne le cache dans notre code (aucun `SetCursorVisibility` dans
  `port/src`). Le SDK a un mode « caché après un temps » (`window_sdl.cpp:95-101`, `kAutoHidden`).
  Plan : curseur **Windows** visible dans les menus, caché en match (pas de dessin ImGui au début).
- **L'image dans la fenêtre.** `Output::ComputeFit` (`reblue/gpu/output.cpp:181-206`) donne
  l'image (largeur, hauteur, décalage) ; `present.cpp:360-381` l'utilise et la note dans le journal
  (`[present] … picture WxH at X,Y`). Il suffit de **garder ces 4 nombres** pour la souris.
  Calcul : `x_jeu = (x_souris − décalage_x) × 1280 / largeur`, pareil en y avec 720.
- **16:9 dans les menus.** Les cibles ne prennent la forme de l'écran **que pendant une scène 3D**
  (`svr_resources.cpp:211-221`) : les menus restent 16:9, avec des bandes noires sur 16:10.
- **Le menu pause en match.** Là, l'image a la forme de l'écran, et l'ATH est dessiné dans le
  **milieu 16:9** (`draw.cpp:333-345`). Il faut alors prendre ce milieu 16:9 de l'image.
- **STRETCH** (`bd_aspect_ratio`) : l'image remplit tout, sans bandes ; même calcul.
- **4:3** (drapeau `0x8324CE3C`, sous 1280 × 720) : le jeu prend ses calques SD. Les éléments ont
  peut-être d'autres places. À tester à part.
- **Résolution interne** (`svr_render_scale`) : sans effet, les éléments sont en pixels du jeu.
- **Caméra libre.** Elle lit la souris seule (`GetCursorPos` + bouton droit,
  `wwe12_match_camera.cpp:340`) : en match seulement, donc pas de conflit si on se tait en match.

---

## 4. Savoir qu'on est dans un menu

Ce qu'on a :

| Signal | Fichier | Limite |
|---|---|---|
| `Wwe12LoadingScreenUp()` | `wwe12_load_turbo.cpp:177` | vrai pendant un chargement : se taire |
| `Wwe12SceneMode()` | `wwe12_fps.cpp:112` | entrée / victoire / scène : se taire |
| `Wwe12MatchSeen()` | `wwe12_fps.cpp:220` | « un match depuis le lancement » : **reste vrai** dans les menus d'après |
| `Wwe12InMatchNow()` | `wwe12_match_camera.cpp:158`, `:563` | = MatchSeen et pas scène ni chargement : **aussi vrai dans les menus après un match** |

Donc `Wwe12InMatchNow()` ne suffit pas. Proposition : **« un écran de menu a tourné il y a moins
de 150 ms »** (idée de Jarrod, `ListUp()`), vu par nos crochets :
`sub_824D20A0` (écrans à calques), `sub_8253F9A0` / `sub_8253C3F0` (menus de `menu.pac`, à vérifier
qu'ils tournent à chaque image), `sub_825A6A98` (arènes). Et toujours : pas de chargement, pas de
scène, pas de caméra libre, pas de benchmark, fenêtre au premier plan.

Le **menu pause** : c'est aussi un écran du jeu. Il faut une sonde pour savoir s'il passe par
`sub_824D20A0`. Si oui, la règle ci-dessus le couvre seule ; le match lui-même ne fait tourner
aucun écran de menu, donc la souris s'y tait.

---

## 5. Ce que font les autres (méthodes seulement, pas de code repris)

- **Jarrod, WWE 2K14** (`src/mouse_menu.h`, 1 700 lignes, « Session 67 ») : il crochète la fonction
  qui remplit chaque ligne de liste à chaque image, lit la matrice de l'élément texte (centre et
  taille de la case, en 1280 × 720), et le curseur du « SmartMenu » (+352) et son défilement. Survol :
  le curseur va **à côté** de la cible, puis **un** appui haut / bas laisse le jeu finir le geste
  (son, surbrillance). Clic gauche = A une fois arrivé ; molette = haut / bas ; clic droit = B
  partout hors match. Muet si `mnk_mouse`, si un script joue, si une boîte de message est ouverte,
  et **jamais en match**. Image 16:9 centrée. Même chose, à part, pour la grille des catcheurs, les
  places des joueurs, les tuiles d'arène, les listes de règles et la barre d'aide (clic sur « A »).
  `src/menu_focus.h` choisit « le menu qui a le focus » parmi ceux qui ont tourné à l'image.
- **Kaiko, SvR 2011** (`port/src/menu_touch.cpp`, pour le tactile du téléphone) : crochets du dessin
  des calques (groupe, élément) ; place d'un élément = sa position × l'échelle du groupe + le
  décalage du groupe. Un toucher sur une ligne : curseur à côté, un appui de croix, puis A. Boîtes
  OUI / NON : la réponse allumée se reconnaît à la **couleur rouge** de son marqueur. La grille des
  catcheurs est une table fixe. Sa page CONTROLS (`port/docs/CONTROLS.md`) lie LMB / RMB / MMB à des
  touches ; pas de molette.
- **re:Blue** (Blue Dragon, `src/engine/d2anime/anime_mouse.cpp`, `engine/mouse_cursor.cpp`) :
  test de clic sur sa couche 2D, puis appel direct de `setSelectedIndex` du jeu (fonctions nommées,
  ce qu'on n'a pas) ; dessine son propre curseur avec les textures du jeu.
- **Notre SDK** : rien pour les menus ; seulement le mode mnk (souris = stick droit).

Les trois arrivent à la même règle : **la souris pousse le jeu par ses propres touches**.

---

## 6. Plan par étapes

### Étape 0 — sonde (dev, sans rien changer au jeu) — ½ à 1 séance
- Journal : quels crochets tournent à chaque image dans le menu principal, OPTIONS, nos pages Lua,
  le menu pause (`sub_824D20A0`, `sub_824D3458`, `sub_8253F9A0`) ; la valeur de M+360 à chaque
  haut / bas ; le nom, x, y, échelle et parent des éléments dessinés (comme `DumpPageLayout`).
- Une capture (`shot`) de la manette scriptée pour comparer : centre calculé de chaque ligne =
  centre vu sur l'image, à 2 px près.
- Risque : faible (dev seulement).

### Étape 1 — relatif partout + survol dans le menu principal et OPTIONS — 2 à 3 séances
- Écouteur souris sur la fenêtre (comme `wwe12_kbd.cpp`) ; appuis versés dans `sub_82A8C070`.
- Clic gauche = A (dans l'image), clic droit = B, molette = haut / bas, partout hors match.
- Survol : menus de `menu.pac` (menu principal, OPTIONS, sous-menus GRAPHICS / JUKEBOX) et nos
  pages Lua (lignes `option_line01_0N`, curseur `GetCursorV`).
- Option `wwe12_mouse_menus = true` (et `false` pour l'éteindre), dans le launcher plus tard.
- Test (le tien) : survoler chaque ligne, cliquer, revenir avec clic droit, molette, en 16:9,
  16:10 et fenêtré ; vérifier qu'en match la souris ne fait rien.

### Étape 2 — sélection des catcheurs et grille d'arènes — 2 séances
- Arènes : rectangles des 21 tuiles (4 × 6) ; cible (ligne, colonne) ; appuis de croix jusqu'à
  +72 / +88 = cible ; molette + Maj ou clic sur l'étiquette de page = LB / RB (`wwe12_arena_pages.cpp`).
- Catcheurs : grille de portraits (code à trouver) ; plusieurs joueurs → seulement le joueur 1.

### Étape 3 — Univers — 2 séances
- Écrans à calques (S+28, S+64) : menu Univers, cartes, classements, calendrier (saute les jours
  vides : **ne pas compter** les appuis, regarder le curseur), boîtes de choix à 2 colonnes.

### Étape 4 — modes Création, menu pause — 2 à 4 séances
- Création : beaucoup d'écrans à part (curseurs 3D, glissières) ; d'abord le relatif seul.
- Menu pause : milieu 16:9 de l'image ; rien tant que la sonde n'a pas prouvé qu'on est en pause.

### Risques (pour toutes les étapes)

| Risque | Parade |
|---|---|
| Mauvaise ligne (rectangle faux) | sonde + capture avant ; ne survoler que les écrans prouvés, sinon relatif |
| Double appui (répétition du menu, deux lecteurs de la manette dans la même image) | 1-2 images enfoncé, 1 relâché ; appui compté par image du jeu, pas par appel |
| Menu qui boucle (du bas au haut) | pas de comptage : on regarde le curseur après chaque appui ; plafond d'appuis |
| Lignes grisées que le curseur saute | même parade ; on s'arrête si le curseur ne bouge plus |
| 16:10 / 4:3 / STRETCH / menu pause en 16:10 | rectangle de l'image gardé à la présentation ; milieu 16:9 en match ; 4:3 testé à part |
| Souris immobile qui « reprend » la manette | seul un **changement** de ligne survolée fait bouger |
| Conflit avec mnk (LMB / RMB liés, `mnk_mouse`) | se taire dans ces cas |
| Curseur Windows ou ImGui | Windows d'abord (rien à dessiner) ; caché en match / scène |
| Fils : souris sur le fil de l'interface, appuis lus par le fil du jeu | valeurs atomiques, comme `g_press_from` (`wwe12_intros.cpp:40`) |
| Android (tactile) | même moteur plus tard (c'est le but de Kaiko) ; pas dans la 2.0 PC |

### Questions ouvertes
1. M+360 est-il bien l'index de ligne du menu principal ? Le menu principal passe-t-il par les
   écrans à calques (`sub_824D20A0`) ?
2. Le menu pause est-il un écran à calques ? Et les menus d'avant le menu principal (titre) ?
3. Position à l'écran d'un élément : x / y propres + parents + échelle du groupe : la bonne formule ?
4. 4:3 : mêmes noms d'éléments, autres places ?
5. Faut-il garder le curseur Windows visible en plein écran, ou le cacher après 3 s sans bouger ?
