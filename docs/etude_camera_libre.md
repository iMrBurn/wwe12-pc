# Étude : choix de la caméra de match et caméra libre (Rendu, 2026-11-27)

> **État** : étude seulement, en lisant le code généré et des notes de version publiques. **Rien codé, rien compilé,
> rien lancé.** Aucune promesse aux joueurs. Suite de `docs/camera.md` (Gameplay, 2026-10-27), qui reste valable.

Demande : (1) que le joueur choisisse sa caméra de match (vue, distance, hauteur) ; (2) une caméra libre pendant un match
(souris / clavier / sticks), avec un bouton pour revenir à la caméra du jeu.

## 1. En bref

- **Le jeu n'a aucun réglage de caméra** à part CAMERA CUTS (oui / non). Pas de distance, de hauteur ni d'angle.
- **Ce qu'on sait du code** : un fil `CameraAIDirectorThread` et une « classe caméra animée » (pistes `Camera01` et
  `fov.Camera01`). Mais **la fonction qui place la caméra du match à chaque image n'est pas encore trouvée**. On ne
  peut pas la trouver de façon sûre en lisant seulement le code : il faut une courte sonde en jeu (palier A,
  section 4).
- **Les autres l'ont fait** : Kaiko (SvR 2011) propose 6 caméras de match et une caméra libre. Jarrod (2K14) propose
  un mode photo, une caméra libre avec trajectoires (CineCam) et une caméra « rampe ». C'est donc faisable sur ce
  genre de jeu.
- **Ordre conseillé** : A (trouver la caméra, 1-2 séances) → 0 (distance / hauteur / champ, 2-3 séances) → 1 (caméra
  libre, 3-5 séances) → 2 (vues prêtes comme RAMPE, plus tard).

## 2. (a) Où le jeu choisit et met à jour sa caméra

### Ce qui est établi (lu dans le code)

- **Le réglage CAMERA CUTS** est un octet du bloc de réglages `0x832265C8` (+30, +66 ou +68 ; lequel reste à
  confirmer). Voir `docs/camera.md`.
- **`CameraAIDirectorThread`** (nom à `0x82001708`, fonction `sub_821D8430`, boucle `sub_821D8398`). Relu
  aujourd'hui, ce fil **n'est pas le réalisateur** que `docs/camera.md` supposait :
  - il attend un signal (`[obj+204]`), traite deux listes de demandes (`obj+56+96` et `obj+56+112`), puis signale la
    fin (`[obj+208]`) ;
  - pour chaque demande à l'état 1, `sub_821D8040` lit un fichier et vérifie son en-tête `"BPE "` ou `"ZLIB"`, le
    décompresse (`sub_827E1428` / `sub_827E1248`), puis le range (`sub_82486648`) ;
  - c'est donc un **chargeur en arrière-plan des données de caméra** (plans des prises, des finishers, des scènes),
    et pas la caméra du match elle-même.
- **Une classe « caméra animée »** : son constructeur est `sub_8236D028` (objet de ~4 800 octets, vtable
  `0x8200A010`). Il lie deux pistes d'animation nommées `Camera01` et `fov.Camera01` (textes à `0x8200A0B0` /
  `0x8200A0A0`) ; `sub_8236C6E0` utilise le modèle `Camera%2d`. Elle est construite par `sub_823EDA18`,
  `sub_823FC5A8`, `sub_826D2C88`, `sub_828E7668` et `sub_82DF8968`.
  - Ce sont les caméras « jouées » d'un fichier : entrées, prises, finishers, scènes. Le Mod Maker lit les mêmes
    données dans les entrées (`entrance_preview.h` : œil `#5edfdf5f`, cible `#70d146a5`, champ `#70d304a4`).
- **Notre rendu** ne connaît pas la caméra : il reçoit seulement les matrices déjà calculées par le jeu, dans les
  constantes des shaders.

### Ce qui n'est pas encore trouvé

- **La caméra « de jeu »** (celle qui suit les catcheurs en jeu libre) : où elle garde sa position, sa cible et son
  champ, et quelle fonction les calcule à chaque image.
- **Comment le jeu passe** de la caméra de jeu à un plan animé (prise, finisher), puis revient.
- **Les tables de caméras** par type de match ou par arène. Aucun texte du jeu ne les nomme. Si elles existent, ce
  sont des nombres dans le code ou dans les `.pac` de l'arène.

## 3. (b) Ce que font Kaiko et Jarrod (notes de version publiques, aucun code lu ni copié)

**Kaiko, SvR 2011** (`KaikoClanworth1/wwe-svr2011-pc`)
- v2.1.4 : **caméra libre / mode photo**. F6 pour voler, F5 pour revenir, F7 pour cacher l'interface, F8 pour mettre
  en pause (hors ligne).
- v2.1.5 : **MATCH CAMERA** avec 6 vues : NORMAL, FAR, CLOSE, HIGH, LOW et RAMP (une vue large fixe vers la rampe,
  comme au Royal Rumble).
  - Réglage dans GRAPHICS > DISPLAY ou dans le launcher.
  - On peut changer en plein match avec BACK + R3 (F4 au clavier, bouton CAM sur Android).
  - En ligne, chacun choisit sa vue.
- v2.1.9 : il a corrigé « le menu pause ne s'ouvrait plus / l'interface disparaissait » après les touches de caméra
  libre. C'est le piège de l'interface (section 6).
- v2.2.2 : avec la souris et le clavier, la souris reste la caméra pendant le match.

**Jarrod, WWE 2K14** (`Jarrodfeng/wwe2k14-recomp`)
- v0.1.7 : **mode photo**, dans le menu pause. La scène s'arrête presque, et on dirige la caméra : stick gauche pour
  bouger, stick droit pour regarder, gâchettes pour monter et descendre, boutons de tranche pour zoomer, ZQSD/WASD et
  clic droit au clavier. L'interface est cachée et la profondeur de champ allumée.
- v0.1.8 : **CineCam**. Avec F8 en match, on pose des points clés (touche K) sur une ligne de temps, et la caméra
  passe en douceur de l'un à l'autre. Les plans sont enregistrés et peuvent servir pour refilmer un match enregistré.
- v0.4.5 : **RAMP CAMERA**. C'est la caméra du jeu, simplement retournée : même hauteur, même objectif, même cadrage,
  et elle suit toujours les catcheurs.
  - La direction du stick suit la vue.
  - Hors du ring, la vue de bord de ring du jeu reprend la main.
  - Les replays enregistrés se souviennent du réglage.
  - Aussi dans le même numéro : une correction du « saut d'une image » à chaque changement de plan. Son rendu
    dessinait le nouveau plan avec le monde de l'image d'avant. C'est un piège à connaître (section 6).
- **Ce qu'on en retient** : la meilleure façon de commencer, c'est de **modifier la caméra du jeu** (la reculer, la
  monter, la retourner) **plutôt que de la remplacer**. Elle garde alors le suivi des catcheurs, la foule et les
  ombres.

## 4. (c) Plan par paliers

### Palier A : trouver la caméra de jeu (1-2 séances, sonde en jeu, coupée par défaut)

1. **Côté rendu** : à une image en jeu libre, noter la matrice de vue que le jeu envoie (constantes du shader de la
   scène), et en déduire la position de l'œil (x, y, z).
2. **Côté mémoire** : chercher ces 3 floats (big-endian) dans la mémoire du jeu. Il faut 2 ou 3 images où la caméra
   bouge, pour ne garder que l'adresse qui suit.
3. **Piège d'écriture** sur cette adresse, avec la méthode PAGE_GUARD déjà utilisée pour la grille de texte (voir
   `docs/savoir_acquis.md`). On obtient la fonction qui écrit l'œil : c'est la caméra de jeu, avec sa cible et son
   champ juste à côté.
4. Même chose pendant une prise avec changement de plan, pour voir quand la caméra animée prend la main (indicateur
   « jeu libre » / « plan animé »). Recouper avec l'octet de CAMERA CUTS.

Résultat attendu : la structure (œil, cible, champ, haut) et la fonction de mise à jour, notées dans
`docs/savoir_acquis.md`.

### Palier 0 : distance, hauteur, champ par réglage (2-3 séances)

- Un crochet **après** la fonction de mise à jour de la caméra de jeu, seulement en jeu libre :
  - œil' = cible + (œil − cible) × DISTANCE ;
  - + HAUTEUR sur l'axe vertical ;
  - champ × CHAMP.
- **Les plans animés** (prises, finishers, entrées, scènes, replays) ne sont pas touchés.
- **Réglages proposés** : MATCH CAMERA = NORMAL / FAR / CLOSE / HIGH / LOW, dans GRAPHICS (vraie page du jeu, comme
  les autres réglages), plus un raccourci en match (par exemple F4 / BACK + R3).
  - NORMAL = le jeu d'origine, par défaut.
  - Chaque vue est juste un trio de nombres (distance, hauteur, champ).
- **Une 6e vue, RAMP** (retourner la caméra autour de la cible, à la façon de Jarrod), vient plus tard. Elle demande
  aussi de **retourner la direction du stick**, sinon le joueur va à l'envers.
- **Critères avant d'en parler aux joueurs** :
  - ring, rampe, Hell in a Cell, cage, échelles, Royal Rumble, 6 catcheurs ;
  - 30 et 60 images par seconde ;
  - aucune caméra dans les cordes, sous le ring, ni derrière un poteau sans raison.

### Palier 1 : caméra libre en match (3-5 séances)

- **Le principe** : une touche (F6 / BACK + L3) remplace l'œil et la cible de la caméra de jeu par les nôtres, à
  chaque image, après le calcul du jeu. Le jeu continue de calculer la sienne (on reprend la main sur la même
  structure), et la même touche la lui rend.
- **Les commandes** :
  - souris pour regarder, ZQSD/WASD pour bouger, A/E ou Q/E pour monter et descendre, molette pour zoomer ;
  - à la manette : stick gauche pour bouger, stick droit pour regarder, gâchettes pour monter et descendre, boutons
    de tranche pour zoomer.
  - Pendant la caméra libre, ces entrées **ne vont pas au catcheur**. Le plus simple, comme Jarrod et Kaiko, est de
    mettre le match en pause pendant la caméra libre : c'est un « mode photo ». La caméra libre sans pause, où l'on
    joue en même temps, est plus difficile et passe après.
- **Options** : cacher l'interface (une touche), vitesse lente.
- **Hors ligne seulement.**

### Palier 2 (plus tard) : vues prêtes et trajectoires

- RAMP, vue « Rumble » fixe, et éventuellement des points clés à la CineCam. Seulement si les paliers 0 et 1 sont
  propres.

## 5. Estimation

| Palier | Séances | Risque | Ce qui peut bloquer |
|---|---|---|---|
| A : trouver la caméra | 1-2 | faible (sonde) | la caméra est recalculée à partir de plusieurs structures (alors 1 séance de plus) |
| 0 : distance / hauteur / champ | 2-3 | moyen | la foule ou des objets disparaissent quand on recule (section 6) |
| 1 : caméra libre (mode photo) | 3-5 | moyen | l'interface, les entrées manette, le replay |
| 2 : RAMP, trajectoires | 3-6 | élevé | le stick à retourner, les combats hors du ring |

## 6. (d) Les pièges

- **L'interface (HUD)** : la cacher en caméra libre (Kaiko a eu « le menu pause ne s'ouvre plus / l'interface
  disparaît » après ses touches de caméra). Toujours la remettre en revenant, et ne jamais toucher à la caméra
  pendant les menus.
- **La foule et les objets coupés** : le jeu décide ce qu'il dessine d'après **sa** caméra et son champ. Si on recule
  ou si on vole loin, des gradins, des parties de foule ou des objets peuvent manquer. Kaiko a eu ce cas sur des
  arènes portées : « tapis, barrières et gradins visibles seulement sous certains angles ». La parade : modifier la
  caméra **avant** que le jeu choisisse quoi dessiner. C'est une raison de plus de passer par la structure du jeu, et
  pas par la matrice dans notre rendu.
- **Les ombres** : leur zone suit la caméra (réglage SHADOW DISTANCE dans nos textes). Une caméra éloignée peut
  montrer le bord de la zone d'ombre : à vérifier au palier 0.
- **Les entrées en 60 images par seconde** : notre mode 60 dédouble des images. Toute caméra libre ou modifiée doit
  être lissée sur notre temps, et pas sur l'image du jeu, sinon elle tremble. Ne pas toucher aux entrées au palier 0.
- **Le changement de plan** (piège de Jarrod) : au moment d'une coupe, notre rendu ne doit pas mélanger la caméra
  d'une image avec le monde d'une autre. Comme on modifie la caméra du jeu avant son calcul de l'image, ce risque est
  faible, mais il faut le vérifier.
- **Le replay** : il rejoue ses propres plans. Ne pas toucher à la caméra pendant un replay au palier 0. Plus tard,
  la caméra libre en replay serait un plus (c'est la CineCam de Jarrod).
- **L'arbitre et les cinématiques** (compte, prises, finishers, I Quit, arbitre spécial) : ce sont des plans
  animés, non touchés. Il faut un indicateur fiable « jeu libre » (palier A, étape 4) avant d'appliquer quoi que ce
  soit.
- **La manette en caméra RAMP** : il faut retourner la direction du stick, sinon le catcheur va à l'envers. Jarrod
  l'a fait.
- **Le mode en ligne** : pas concerné. Le jeu est hors ligne chez nous.
- **Le benchmark** : il doit garder la caméra NORMAL, pour que les mesures restent comparables.

## 7. Sources

- Code généré lu : `sub_821D8398`, `sub_821D8040`, `sub_8236D028` (vtable `0x8200A010`), références des textes
  `Camera01` / `fov.Camera01` / `Camera%2d`.
- `docs/camera.md` (Gameplay, 2026-10-27), `docs/etude_autres_recomps_2026-11-21.md`.
- Notes de version publiques lues avec `gh api` : Jarrod v0.1.7, v0.1.8, v0.4.5 ; Kaiko v2.1.4, v2.1.5, v2.1.9,
  v2.2.2.
