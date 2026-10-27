# Caméra de match : étude (Gameplay, 2026-10-27)

Demande d'un joueur : choisir la caméra de match comme dans WWE 2K25/2K26 (caméra « rampe », autres angles).
Étude courte, en lisant le code seulement (jeu non lancé). Rien n'est codé. Aucune promesse aux joueurs avant un essai.

## 1. Options de caméra existantes

- Le jeu n'a **qu'une** option de caméra : **CAMERA CUTS** (texte 41216), dans GAMEPLAY > IN-GAME OPTIONS. Elle vaut
  oui ou non, et coupe ou garde les changements de plan cinématiques pendant les prises. Il n'y a pas de réglage de
  distance, de hauteur ou d'angle. « CAMERA » n'apparaît dans aucun autre texte du jeu (`string.pac`).
- Stockage : les réglages de cette page sont des octets du bloc de réglages `sub_82684BE8() + 0x57720` (0x832265C8).
  `UpdateInGameOptions` (`sub_82DEC528`, page 0) écrit les octets +30, +68, +66, puis +69 (BLOOD, voir le sang
  des catcheuses dans CLAUDE.md). CAMERA CUTS est l'un des trois premiers. Lequel : à confirmer, par exemple avec
  un piège d'écriture en changeant l'option.

## 2. Où le jeu choisit la caméra

- Le jeu a un **fil dédié** : la table des fils (0x82001A60…) contient « CameraAIDirectorThread », avec la fonction
  `sub_821D8430`. Sa boucle (`sub_821D8398`) attend son signal puis appelle `sub_821D8040` deux fois, une par vue
  (+96 et +112 de l'objet ; sans doute les deux caméras, ou caméra et vue de replay), puis rend la main.
- C'est donc un « réalisateur » automatique qui choisit lui-même ses plans à chaque image. Ses règles (quel plan,
  quand) ne sont pas encore lues : `sub_821D8040` appelle `sub_821D7E78`, `sub_8246E0F0`, `sub_8246E078`,
  `sub_8246E8C8`, `sub_82486648`, `sub_8267D450`, `sub_821FDAF0`…
- `/CART/CAME` (`sub_8264F9F0`) appartient à « Create a Ring Menu Sequence » (séquences de menu), pas au match.
- **Combat sur la rampe** : pas encore établi s'il a sa propre caméra. Il faut regarder le jeu tourner (sonde sur la
  sortie du réalisateur, ou capture RenderDoc pendant un combat sur la rampe).

## 3. Un choix du joueur sans casser le reste ?

Probablement, mais pas prouvé. L'idée : laisser le réalisateur calculer son plan, puis remplacer sa position et sa
visée **seulement** en jeu libre. On ne toucherait pas aux plans des prises, finishers, cinématiques, entrées et
replays. Il faut donc d'abord trouver un indicateur fiable « jeu libre ». CAMERA CUTS en donne une piste : le jeu sait
déjà distinguer les plans cinématiques du plan de jeu. Le risque principal : une caméra mal placée (dans les cordes,
derrière le poteau, rampe hors champ) et des plans cinématiques qui repartent de notre position au lieu de la leur.

## 4. Estimation (séances de travail)

- **(a) Exposer ce qui existe** : CAMERA CUTS est déjà dans le jeu. Il ne reste rien d'autre à exposer tant que les
  réglages internes du réalisateur (distance, hauteur, champ) ne sont pas trouvés : environ 2 à 4 séances pour les
  trouver et en faire 1 à 3 réglages (DISTANCE / HAUTEUR), si ce sont de simples nombres.
- **(b) De vraies caméras sélectionnables** (par exemple « classique », « plus large », « basse », « rampe »), avec
  une page d'options : environ 8 à 15 séances, résultat incertain. Il faut lire le réalisateur, trouver l'état « jeu
  libre », écrire chaque caméra, tester sur ring, rampe, cage, Hell in a Cell, échelles et matchs à plusieurs. Une
  caméra qui suit vraiment l'action sur la rampe est la partie la plus dure.
