# Étude : un filet de sécurité pour les animations absentes (2026-11-19)

Étude en lecture seule (session « stipulation modding », à la demande de la Main, suite du bug #79). Rien n'est écrit
dans le code. Proposition pour la **1.4.2 au plus tôt**, **coupée par défaut**.

## L'idée vient de Kaiko

Portage SvR 2011 de Kaiko (KaikoClanworth1/wwe-svr2011-pc, accord : s'inspirer, avec crédit). Deux commits lus,
**rien copié** :

- `c529dfe` : un catcheur resté **1 s** dans une animation qu'aucune banque n'a est relancé sur l'animation « au sol »
  (446), d'où il se relève comme d'habitude. Seulement s'il est seul dans cette animation : relancer **un seul des deux**
  catcheurs d'une prise a fait planter le jeu chez lui. Dans ce cas, il se contente d'écrire une ligne dans le journal.
- `4d17d7b` : quand une prise d'un mod manque, le jeu la remplace par une prise « de secours » de la même catégorie.
  Kaiko choisit la première prise de secours **dont l'animation est chargée** pour ce match (la première de la liste
  ne l'était pas : les deux catcheurs restaient figés).

Le principe commun : **ne jamais laisser un catcheur attendre une animation qui n'arrivera pas** ; le remettre dans un
état que le jeu sait gérer.

## Ce que fait WWE '12 aujourd'hui

Une animation jouée par numéro passe par plusieurs étapes :

1. `sub_8247AD18(acteur, numéro, …)` → `sub_82478AE8`, puis la **file de chargement** `sub_821D7480` (32 demandes au
   plus).
2. Le lecteur de la file (`sub_821D7C80` → `sub_8246EB78`) **jette** un numéro absent du **répertoire des motions**
   `[0x8317B870]` (cases 1 et 11-22). L'acteur attend alors une animation qui ne viendra jamais : **il reste figé**.
   Vu le 2026-11-14 : l'arbitre du I Quit restait immobile.
3. Le chargeur (`sub_821D7E78`) décompresse l'enregistrement (BPE ou ZLIB) dans la place de la file. Si ce n'est pas
   le bon format, la demande passe à l'**état 5** (abandonnée) : même résultat, l'acteur reste figé.
4. Au moment de jouer, `sub_821D7150` retrouve l'animation chargée, puis `sub_8246DF78` lit deux blocs dedans
   (`sub_827E6BB8`) **sans vérifier** qu'ils existent. S'il en manque un : lecture à l'adresse 2, **plantage**. C'est
   le plantage d'Android du bug #79, et la même famille que « 250NN not found » des entrées 2K16.

## Le filet proposé (deux parties)

### A. Le catcheur figé (le plus utile, risque faible)

- **Où le brancher :** le crochet de la manette `sub_8225C4D8` (`wwe12_iquit.cpp`). Il passe une fois par image pour
  chaque acteur, sur le **fil principal**, et sert déjà à l'arbitre spécial.
- **Comment savoir qu'une animation manque :**
  - on note les numéros que le lecteur jette (crochet de `sub_821D7C80` / `sub_8246EB78`) et les demandes passées à
    l'état 5 (`sub_821D7E78` est déjà crocheté dans `wwe12_motion_trace.cpp`) ;
  - on peut aussi vérifier le répertoire directement, comme le fait déjà la fonction `Directory()` de
    `wwe12_motion_trace.cpp`.
- **Quand agir :** l'animation de l'acteur (`Anim(acteur)`) fait partie de ces numéros manquants depuis **1 s**.
- **Ce qu'on fait :** `sub_8247C670(acteur)`, qui le **remet dans son animation libre** (1350, debout ; son cerveau
  agit à partir de là). C'est la fonction `Free()` de `wwe12_sref.cpp`, déjà vue marcher en jeu avec l'arbitre spécial.
- **Prise à deux** (objet 'BPMG' à acteur +3404 : +36 celui qui porte, +40 celui qui est pris) : on **libère les deux
  ensemble ou aucun**. Leçon de Kaiko : n'en libérer qu'un seul a fait planter le jeu.
- **Cas couverts :**
  - entrées converties dont une animation manque (2K16) ;
  - prise d'un mod dont l'animation n'est pas chargée ;
  - animations de mods « motions » mal rangées ;
  - entrants du Royal Rumble figés, si la cause est une animation absente.
- **Risque :** faible. Le jeu d'origine a toutes ses animations, donc le filet ne devrait jamais se déclencher dans un
  match normal. À vérifier avec une ligne de journal qui compte les déclenchements.
- **Estimation :** environ 1 jour de code, plus 2 essais en jeu. Pour l'essai, une aide coupée par défaut : une touche
  fait demander au joueur 1 un numéro qui n'existe pas. Il doit se relever seul en 1 s.

### B. Le plantage dans `sub_8246DF78` (plus délicat, risque moyen)

- **Où le brancher :** un crochet de `sub_8246DF78`. On refait ses deux recherches comme le jeu. Si la seconde ne
  trouve rien, on rend un **bloc vide** (zéro élément, préparé une fois dans la mémoire du jeu) au lieu de lire
  l'adresse 2. L'animation ne bouge pas, et la partie A remet l'acteur libre.
- **Risque :** moyen.
  - `sub_827E6BB8` **décompresse en place** certains blocs (types 1 à 8) : ne jamais l'appeler deux fois pour la même
    recherche.
  - Les deux appelants (`sub_821D7620`, `sub_8246F950`) et la suite lisent d'autres champs : il faut les lire en
    entier avant d'écrire quoi que ce soit.
- **Estimation :** 2 à 3 jours, avec un fichier volontairement abîmé, **dans le dossier de test seulement**.

## Ce que le filet ne répare pas

Une **mémoire écrasée**, comme le Royal Rumble de la 1.4.0 (un élément de liste qui valait 0x1800010A au lieu d'un
pointeur). Là, il faut corriger la cause, ce qui est fait en 1.4.1 (`ef67296`, animations de WWE '13 seulement en
I Quit et avec l'arbitre spécial). Un filet cacherait le problème sans le régler.

## Réglage proposé

`wwe12_motion_guard = false` par défaut. Une ligne de journal à chaque déclenchement : quel acteur, quel numéro
manquant, libéré seul ou avec son partenaire. À mettre « vrai » par défaut seulement après des essais en jeu
(entrées 2K16, Royal Rumble, une prise de mod). Crédit à Kaiko dans les notes de version et dans le code.
