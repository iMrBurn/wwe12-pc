# Squash matchs : étude (Limit breaking, 2026-10-28, lecture seule)

## Cahier des charges corrigé (demande du joueur, transmise par Main le 2026-10-28)

Le joueur ne demande pas un multiplicateur de dégâts. Il demande :
1. un match SQUASH dans l'Univers, où le jobber **commence avec peu de vie** et le joueur **avec ses prises de
   finition déjà prêtes** ;
2. dans « Edit Superstars » de l'Univers, une **vie de départ par catcheur**, pour qu'un catcheur reste blessé d'un
   match à l'autre (scénarios) ;
3. il signale que WWE '12 a déjà ce réglage : **STARTING HEALTH** dans Create a Story.

**Ce que confirment les fichiers de WWE '12 (lecture seule)** :
- Create a Story a bien **STARTING HEALTH** (texte 22690), avec une barre par zone : HEAD, TORSO, ARMS, LEGS
  (22694–22697), en pourcentage.
- Ses commentaires de match se déclenchent sur **l'état de santé d'une zone** (22929) : 3 niveaux par zone, YELLOW,
  ORANGE et RED pour HEAD, TORSO, ARM et LEG (22419–22430, plus HEALTH STATUS / HEALTH STATE). Le moteur a donc
  **une vie par zone** et sait la régler au départ d'un match.
- WWE '13 a les mêmes notions (`yChar.GetDamageLv`, `GetMostDamageLv`, `yGame.GetHpLevel` dans son script de
  match) : rien de neuf à y prendre pour ce besoin.
- **Pas encore trouvé en lisant le code** : où Create a Story range ces 4 pourcentages pour chaque catcheur
  d'une scène, et la fonction qui les applique aux catcheurs au début du match. Les numéros de textes ne sont
  pas dans le code (les écrans les prennent dans leurs données), et la fiche de création d'un catcheur
  (`sub_82213DB0`, fiche participant : +6 rôle, +12, +20, +72) ne montre pas de champ de vie. Il faut une sonde
  en jeu pour les trouver.
- **Le compteur de prises de finition** n'est pas trouvé non plus : rien à partager pour l'instant avec
  Gameplay. Si la sonde le croise (stock de finishers du catcheur), je le dirai à Main.

**Ce levier est bien plus simple que les dégâts.** On met une valeur de départ, une seule fois, puis le jeu
fait le reste : le jobber, déjà rouge, ne se relève pas des tombés.

**Plan révisé** :

| Étape | Séances | Risque |
|---|---|---|
| a. Sonde en jeu (accord de l'utilisateur, données isolées) : une scène Create a Story avec STARTING HEALTH réglé, puis on cherche les 4 valeurs du catcheur en mémoire et qui les écrit au début du match. On repère au passage le compteur de finishers | 1 à 2 | faible (on lit seulement) |
| b. Option « prochain match : squash » en exhibition / Univers : le jobber à 15–25 % par zone, appliqué par la même fonction que Create a Story | 1 | faible : c'est un réglage que le jeu sait déjà faire |
| c. Finishers prêts au départ pour le dominant (à partager avec le travail de Gameplay sur le stock de finishers) | 0 à 1 | faible |
| d. Univers : case « squash » sur un match de la carte, et vie de départ par catcheur dans Edit Superstars, gardée d'un match à l'autre (il faut la stocker nous-mêmes, à côté de la sauvegarde ; c'est le plus gros morceau) | 2 à 4 | moyen : touche aux sauvegardes de l'Univers |

- **Squash léger (a + b + c) : 2 à 4 séances**, au lieu de 3 à 5 avec les dégâts.
- **Avec l'Univers complet (d) : 4 à 8 séances.**
- **Proposition** : a + b + c pour la **1.0** si la place le permet (option « prochain match : squash »), puis d
  (Univers, blessures qui durent) et l'attaque après le tombé en **1.1**.
- La partie « multiplicateur de dégâts » plus bas est **abandonnée** au profit de la vie de départ. Elle reste
  notée comme solution de secours si la vie de départ ne peut pas être réglée hors de Create a Story.

## Première version de l'étude (dégâts), remplacée par ce qui précède

Demande de l'utilisateur, transmise par Main, pour la roadmap : un match court où un catcheur domine l'autre,
façon WWE '13/2K, avec une fin rapide et parfois une attaque après le tombé. Étude du code et des fichiers seulement :
je n'ai ni lancé le jeu ni compilé.

## Ce qui existe

- **Aucun « squash » dans WWE '12 ni dans WWE '13.** Le mot n'apparaît ni dans les textes (`string.pac`), ni dans le
  script de match en Lua de WWE '13 (`yase/InGameLua.pac`). Pour 2K14 à 2K17, je n'ai que les ISO ou des exécutables
  sans texte lisible : rien n'est vérifié, et je n'ai rien extrait.
- **Ce n'est donc pas un type de match** (rien dans la table `RUL` de `misc.pac`, ni dans les menus) : c'est un
  **comportement**. Il fait jouer un match normal avec un dominant et un perdant.
- Ce que WWE '12 sait déjà faire :
  - l'Univers décrit déjà les victoires écrasantes après un match (textes 27700–27705 : « Dominating his opponent
    from start to finish », « scored a lopsided victory »…), ce qui laisse supposer une note de domination calculée
    à la fin du match ;
  - l'Univers sait imposer le vainqueur d'un match simulé (`docs/univers.md` §32, `sub_826C9050` en mode 2) ;
  - les réglages par match : MOMENTUM BAR CHARGE SPEED (SLOW/NORMAL/FAST), REVERSAL DAMAGE, TIME LIMIT, DIFFICULTY
    (difficulté de l'ordinateur, réglage du match dans l'Univers, texte 22736), INTERFERENCE ;
  - par catcheur : attributs Strike Power, Grapple Power, Recovery et Strike/Grapple Defense, qui règlent les dégâts
    et la facilité des contres (textes 24864–24896), et la santé par zone (tête, bras, jambes, corps).

## Où seraient les réglages

| Levier pour un squash | Où dans WWE '12 | Connu ? |
|---|---|---|
| Le perdant prend plus de dégâts | calcul des dégâts d'une prise sur la santé par zone | **à trouver** (sonde) |
| Le dominant a vite sa prise de signature et sa prise de finition | jauge de momentum de chaque acteur pendant le match (pas le momentum de l'Univers) | **à trouver** |
| Le perdant contre peu | difficulté de l'ordinateur pour le perdant / fenêtre de contre | réglage du match connu, par catcheur à trouver |
| Le perdant ne se relève pas (pas de « kick out ») | dépend de sa santé : suit les dégâts | — |
| Fin rapide | conséquence des trois leviers du haut ; sinon TIME LIMIT court | oui |
| Attaque après le tombé | l'Univers a des scènes après match (attaques, interventions) ; l'exhibition n'a rien | **non**, chantier à part |

- **Table `RUL` / menus** : rien à y mettre. Pour une entrée « SQUASH » dans les menus, on passerait par le même
  crochet que pour les stipulations, `sub_82449898` (page × ligne → règle) : la ligne jouerait la règle 0 (match
  simple) et notre code marquerait le match « squash ».
- **Match Creator** : ses trois pages ont un nombre de lignes fixe (4, 11 et 10), avec des mises en page du jeu
  (`docs/match_creator.md`). On ne peut pas y ajouter une ligne sans refaire une mise en page : c'est déconseillé. Une
  option « SQUASH » y est donc difficile, mais le Match Creator peut produire le match de base.
- Le plus simple pour choisir le dominant : une **boîte de choix du jeu** avant le match (« Qui domine ? »), la même
  que celle de l'Univers (§32, jusqu'à 7 lignes), ouverte depuis la ligne SQUASH en exhibition ou depuis la carte
  de l'Univers.

## Version proposée « SQUASH léger »

1. Une ligne « SQUASH » en ONE ON ONE (crochet `sub_82449898` → règle 0), puis « Qui domine ? ». Dans l'Univers :
   une case « squash » sur un match de la carte, plus tard.
2. Pendant le match : dégâts reçus par le perdant ×3 environ, dégâts reçus par le dominant ÷2, jauge du dominant
   plus rapide, perdant à l'ordinateur en difficulté basse (moins de contres).
3. Fin : normale (tombé). La note de domination de l'Univers devrait sortir d'elle-même.
4. Pas d'attaque après le tombé dans cette version.

## Estimation

| Étape | Séances | Risque |
|---|---|---|
| a. Sonde : trouver la santé par zone, le calcul des dégâts et la jauge de momentum (jeu lancé **avec l'accord** de l'utilisateur, données isolées) | 1 à 2 | faible (lecture seulement) |
| b. Multiplicateurs de dégâts et de jauge + choix du dominant (boîte du jeu) | 1 à 2 | faible à moyen : on ne change que des nombres ; il faut éviter de toucher aux autres matchs |
| c. Ligne SQUASH dans le menu (crochet `sub_82449898`, mise en commun avec I Quit / Special Referee) | 1 | moyen : l'insertion de lignes a déjà cassé la navigation du menu principal ; renommer une ligne est plus sûr |
| d. Attaque après le tombé (scène d'après-match de l'Univers, ou IA qui continue d'attaquer après la cloche) | 3 à 5 | élevé : rien de tel n'existe en exhibition |

- **Squash léger (a + b + c) : 3 à 5 séances.**
- **Avec l'attaque après le tombé : 6 à 10 séances.**
- Plantage : peu probable pour a et b, puisque ce sont des nombres déjà utilisés par le jeu. Le risque réel est un
  match déséquilibré ailleurs si le marquage « squash » reste actif : il doit être remis à zéro à la fin de chaque match.

## Proposition

- **1.0** : seulement si la place le permet. Faire la sonde (a) puis le squash léger (b) avec le choix du
  dominant ; l'entrée la plus sûre au départ est une **option** (cvar + launcher, « prochain match en squash ») plutôt
  qu'une nouvelle ligne de menu.
- **1.1** : la ligne SQUASH dans le menu, en même temps que les autres stipulations (même crochet `sub_82449898`), la
  case squash de l'Univers, et l'attaque après le tombé.
