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

## Plan de la sonde STARTING HEALTH (préparé par lecture seulement, 2026-10-28 ; rien codé, rien lancé)

À faire seulement avec le go de l'utilisateur dans cette conversation. Pendant la sonde, la conversation ne
compile ni ne lance rien d'autre (une seule compile ou un seul jeu à la fois).

**Outils déjà là, à réutiliser** :
- liste des acteurs du match à **0x8317B770** (9 pointeurs : 0-5 catcheurs, 6 arbitre, 7-8 rôle 4) et le fil qui la
  lit toutes les 100 ms (`wwe12_kind_probe.cpp`, `WatchActors`) ;
- piège d'écriture PAGE_GUARD sur une adresse invitée (`wwe12_blood.cpp`, sonde `wwe12_blood_trap`), qui donne le
  code qui écrit (RIP → `llvm-symbolizer` avec `wwe12.pdb`) ;
- données isolées (`Documents\wwe12_test`, comme `lancer_test_jukebox.bat`) pour ne pas toucher aux sauvegardes.

**Étape 1 : trouver les champs de vie dans l'acteur (un match d'exhibition, 1 contre 1)**
- Nouvelle sonde dev `wwe12_health_probe` : toutes les 100 ms, copie des **0x1200 premiers octets** (l'acteur va au
  moins jusqu'à +4612, vu dans `sub_82213DB0`) des catcheurs 0 et 1. Dans le journal, seulement les mots de
  32 bits qui **changent quand un coup touche** et restent stables sinon.
- Ce que l'utilisateur fait : laisser l'adversaire au sol sans le toucher (5 s), puis frapper **seulement la tête**
  (coups de pied au sol), puis **seulement les jambes** (prises de jambe). Un champ qui ne bouge qu'avec la tête =
  vie de la tête, etc. Attendu : 4 valeurs (flottants ou entiers) qui baissent, plus peut-être un niveau 0-3
  (jaune/orange/rouge : `GetDamageLv` dans WWE '13).
- **Piste prioritaire** (info de Gameplay, `docs/omg_finishers.md` §2, lecture de code non vérifiée en jeu) : la
  jauge de momentum est dans le composant **'CHP '** de `[acteur+3436]` (objet = composant + 24). Un composant nommé
  « CHP » (character HP ?) peut aussi porter la **vie par zone** : copier aussi cet objet (≈ 0x200 octets) en plus de
  l'acteur.
- **Finishers prêts au départ** : déjà trouvés par Gameplay, plus besoin de les chercher ici. C'est la jauge
  (objet CHP +152, flottant 0..1) à 1 au début du match (lecture `sub_82246DE0`, écriture `sub_82246E48`). +180 = mode
  (2 = jauge remise à 1 à chaque image, « infini »). L'étape c du plan révisé se fait donc avec le travail de
  Gameplay.

**Étape 2 : trouver qui écrit la vie de départ (Create a Story)**
- L'utilisateur crée une scène de match dans Create a Story : 1 contre 1, **STARTING HEALTH** du 2e catcheur réglé
  à des valeurs faciles à reconnaître (par exemple HEAD 10 %, TORSO 20 %, ARMS 30 %, LEGS 40 %), puis la joue.
- La sonde pose le piège d'écriture sur les 4 champs de l'étape 1 dès que l'acteur existe (création : crochet
  sur `sub_822CDF78`, comme `wwe12_kind_probe`) et note chaque écrivain jusqu'à la cloche. L'écrivain qui pose
  les valeurs 10/20/30/40 (converties) = **la fonction qui applique STARTING HEALTH**. Ses appelants donnent l'endroit
  où Create a Story garde les 4 pourcentages (on remontera le pointeur source dans le code).
- Contrôle : le même match en exhibition doit passer par l'écrivain « vie pleine » seulement.

**Étape 3 (une fois les deux trouvés, sans lancer le jeu)** : lire la fonction d'application pour savoir si on
peut l'appeler soi-même (option « prochain match : squash ») ou s'il vaut mieux écrire les 4 champs nous-mêmes
juste après elle.

**Risque** : faible. La sonde ne fait que lire, sauf le piège PAGE_GUARD, qui ralentit le jeu pendant qu'il est
posé (seulement jusqu'à la cloche). **Durée** : 1 séance si les champs sont nets, 2 sinon.

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
