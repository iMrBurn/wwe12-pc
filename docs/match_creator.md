# Match Creator : restrictions (Gameplay, étude du 2026-10-27)

Demande de l'utilisateur pour la 1.0 : enlever les restrictions du MATCH CREATOR. Étape 1 : lecture du code seulement,
sans lancer le jeu. Rien n'est codé.

## Ce que le Match Creator propose (textes du jeu, `string.pac`)

- Pages : **ENVIRONMENT** (41500), **WIN CONDITION** (41501), **RULES** (41502).
- RING TYPE (41514) : STANDARD RING, STEEL CAGE, HELL IN A CELL, ELIMINATION CHAMBER, INFERNO MATCH RING (41520-41524).
- WIN CONDITION : PIN ONLY / SUBMISSION ONLY (41546-41547), ELIMINATION (41532), FALLS COUNT ANYWHERE (41533),
  2 OUT OF 3 FALLS / IRONMAN (41214-41215), FIRST BLOOD.
- RULES : TIME LIMIT (NONE, 5 à 60 MINUTES), décompte à l'extérieur (NO COUNT, 5/10/20 COUNT), REVERSAL DAMAGE,
  BLOOD, READY TITLE, OBJECT SELECT (EXTREME RULES ONLY) avec SLOT 1-4 et les armes (SLEDGEHAMMER, STEEL CHAIR,
  KENDO STICK, GUITAR, MOP), FLAMING TABLE (TABLE MATCH ONLY), CLIMB OUT OF CAGE (CAGE MATCH ONLY),
  NO EXITING / BOTH (sortie de cage).
- Le jeu lui-même annonce donc des options réservées à un type de match : « EXTREME RULES ONLY », « TABLE MATCH
  ONLY », « CAGE MATCH ONLY ».

## Où sont les restrictions (code)

- L'écran (`sub_8255FB68` le prépare) a **trois panneaux d'options**, un par page, à +316, +932 et +1684. Chacun
  est construit par `sub_825935C8(panneau, écran, tables, type de match)` avec **quatre pointeurs de tables** (+8, +12,
  +16, +20) pris dans l'écran (+2440 … +2480), et le type de match de l'écran (+276).
- Classe de base d'un panneau : constructeur `sub_825933F8` (vtable 0x82028164 ; textes des valeurs écrits dans
  l'objet), 6 lignes au plus, nombre de lignes à +176. À l'ouverture, `sub_825910F8` lit pour chaque ligne la
  valeur de départ (`[+8]` : octet groupe × nombre de lignes + ligne) et une deuxième table (`[+12]`, même
  indexation), sans doute « ligne permise / verrouillée ». Ensuite `sub_82593BD0` grise chaque ligne par la méthode
  virtuelle +20 (« cette ligne est-elle permise ? »).
- **Groupes de types de match** : `sub_825936B0(type)` cherche le type (0 … 89) dans une table de 67 groupes
  (0x82027E28, {liste, nombre, drapeau}). Par exemple, le groupe 0 = types 0, 1, 2 ; le groupe 3 = 43, 44, 45.
  Les tables de lignes sont donc indexées **par groupe**.
- **FIRST BLOOD** : grisé si l'option BLOOD est désactivée (octet 0x8322660D ; `sub_82591750`, `sub_825910F8`), et
  impossible pour 6 types de match (`sub_82593790`, table 0x8202814C : 1, 2, 5, 6, 44, 45).
- Pas encore localisés : l'origine des quatre tables (données du jeu ou du code ; à lire en jeu), le nombre de
  participants par type, les arènes ou titres refusés.

## Classement provisoire

- **Probablement sans risque** (ce sont des règles, le moteur de match les sait déjà) : rendre permises les lignes
  grisées par les tables, quand l'option existe déjà dans un autre type de match (limite de temps, décompte,
  REVERSAL DAMAGE, READY TITLE, BLOOD / FIRST BLOOD hors des 6 types interdits).
- **À essayer avec prudence** : options liées à un objet (FLAMING TABLE hors match à tables, CLIMB OUT OF CAGE sans
  cage, OBJECT SELECT hors Extreme Rules), et changer de RING TYPE pour un type de match qui en attend un autre
  (échelles dans une cage, Chamber hors Elimination Chamber). Le match peut chercher un objet qui n'existe pas :
  risque de blocage ou de plantage.
- **Non levable** : plus de 6 catcheurs (tableaux à 6 cases dans tout le moteur, voir `limit_breaking.md`).

## Suite proposée

1. **Sonde (une séance, jeu lancé avec l'accord de l'utilisateur, données isolées)** : à la construction des
   panneaux (`sub_825935C8`), écrire dans le journal les quatre tables et, pour le type choisi, chaque ligne
   permise ou grisée. On parcourt les types de match dans le Match Creator : on obtient la liste exacte des
   restrictions.
2. **Réglage `wwe12_mc_unlock` (1 à 2 séances)** : forcer « permise » pour les lignes classées sans risque, avec une
   liste noire pour les combinaisons dangereuses. Chaque combinaison débloquée est testée par un match.
3. **Combinaisons à risque (2 à 4 séances, plutôt après la 1.0)** : une par une, avec un test de match chacune.

Pour la 1.0 : étapes 1 et 2 seulement, c'est-à-dire ce qui est sûr et testé.

## Sonde, premier passage (2026-10-27, `wwe12_mc_probe`, `tools/dev/lancer_test_mc.bat`, journal `match_creator.log`)

- Un seul panneau est passé par `sub_825910F8` : 11 lignes, vtable 0x82027C7C, ouvert avec le type de match 0
  (groupe 0). Les deux autres panneaux s'ouvrent par une autre fonction (classe dérivée) : à trouver.
- Les quatre tables sont **dans le tas** (0xA3DC5F30…), donc chargées depuis un fichier du jeu et non écrites
  dans le code : on peut les modifier en mémoire après leur chargement.
- `[+8]` (67 groupes × 11) : petits nombres (0, 1, 3), sans doute la **valeur de départ** de chaque ligne.
- `[+12]` (67 × 11) : **50** = le joueur choisit ; **0, 2, 3** = valeur imposée ou ligne fermée, selon le groupe.
  C'est la restriction par type de match.
- `[+20]` : une matrice 11 × 11 avec **255 sur la diagonale**. Ce sont sans doute les **compatibilités entre
  options** (50 = compatibles, 0 = incompatibles, 3 = cas particulier). C'est la restriction entre règles.
- `[+16]` : autre forme, pas encore comprise.
- Reste à faire avant de débloquer : relier chaque ligne à son nom (texte affiché), sonder les deux autres panneaux,
  et confirmer en jeu qu'un 50 forcé dans `[+12]` ou `[+20]` rend bien la ligne choisissable.

## Sonde, deuxième passage (2026-10-27)

- Trois panneaux, tous passés par `sub_82593BD0` : **4 lignes** (vtable 0x820281B4), **11 lignes** (0x82027C7C) et
  **10 lignes** (0x82027CAC ; ses widgets sautent `_07`). Vus avec les types de match 0 (groupe 0), 3 (groupe 14) et
  62 (groupe 32).
- Les widgets des lignes s'appellent `_01` … `_11`. Leur texte n'est pas à `[widget+200]`, et l'indicateur à +180
  est la **surbrillance** (0 sur la ligne du curseur), pas le grisé. Les noms des lignes viennent donc de la mise
  en page (MnLay) : il faut les relier à l'écran avec une capture.
- Textes de valeurs du constructeur, dans l'ordre : OFF/ON, les 5 types de ring, EXTREME RULES (95),
  SLOW/NORMAL/FAST, OFF/BOTH, PIN ONLY/SUBMISSION ONLY, NO EXITING, décomptes, limites de temps, armes.
- Suite : une capture de chaque page (type simple), pour nommer les lignes, puis l'essai `[+12] = 50`.
