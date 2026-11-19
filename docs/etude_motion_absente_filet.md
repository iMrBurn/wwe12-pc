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

## Écrit le 2026-11-19 (filet A, 1.4.2, coupé par défaut)

`port/src/native/wwe12_motion_guard.cpp` (cvar `wwe12_motion_guard`, faux par défaut) :
- un numéro est noté « absent » quand le lecteur de la file le jette (crochet de `sub_8246EB78`, seulement l'appel de
  0x821D7D00 qui rend 0) ou que le chargeur l'abandonne (état 5, crochet `sub_821D7E78` de `wwe12_motion_trace.cpp`) ;
  la liste est vidée à chaque répertoire refait (`sub_8246E3B0` case 1, chargement d'un match) ;
- chaque image, depuis le crochet de la manette (`sub_8225C4D8`, fil principal) : un acteur dont l'animation est un
  numéro absent depuis 1 s est remis libre (`sub_8247C670`, + 1096 / + 1428 à 0), avec son partenaire de prise s'il
  en a un ('BPMG') ; filet coupé : une ligne dans le journal seulement ;
- aide d'essai `wwe12_motion_guard_test=N` : 10 s après le chargement, quand le joueur 1 est debout depuis 2 s, il
  reçoit la motion N une fois (1852 n'existe dans aucune banque : m.pac + m_dlc). Lanceur sans joueur :
  `tools/dev/lancer_test_filet_motion.bat [on|off]` (le benchmark joue le match seul).
Pas encore essayé en jeu.

**Essai du 2026-11-19 (lancé par la Main, sans joueur, `lancer_test_filet_motion.bat`, exe de 20:45) : RÉUSSI.**
- Le premier essai (20:39 / 20:43) ne s'était pas déclenché : la condition « joueur 1 debout depuis 2 s dans 1350 »
  n'arrive pas dans le match du benchmark. L'essai force maintenant la motion sur l'acteur 0, 12 s après le
  chargement du match, et le filet passe sur les six acteurs à chaque image (`4805645`).
- Filet coupé (20:49) : l'acteur 0 passe de 56159 à 1852, la file jette 1852 (« pas dans le répertoire »), et il reste
  figé dedans (ligne « fige depuis 1 s … (filet coupe) »).
- Filet actif (20:54) : même chose, puis « remis libre (motion 20050) » : `sub_8247C670` lui donne son animation libre
  à lui (20050 pour ce catcheur, pas 1350 : 1350 est celle de l'arbitre). Aucun plantage, résumé du benchmark écrit
  dans les deux passes.
- Au chargement, le jeu demande lui-même 2000 et 2001, que son répertoire n'a pas : le filet les note. Aucun acteur n'y
  est resté 1 s dans ces deux essais. À surveiller : si le filet libérait un jour quelqu'un dans 2000 / 2001, ce serait
  une motion que le jeu d'origine attend aussi en vain.
- Pas encore essayé : la prise à deux ('BPMG', les deux hommes libérés ensemble).

**Prise à deux : NON testée (2026-11-19, 21:04).** Aide d'essai `wwe12_motion_guard_test_pair=1852`
(`lancer_test_filet_motion.bat off prise`, match du benchmark de 90 s). Pendant tout le match, `[acteur + 3404]` est
resté à 0 pour les six acteurs (« aucun objet de prise ») : soit le CPU n'a fait aucune prise au joueur 1 immobile, soit
+3404 n'est pas l'objet de prise que l'on croit. La disposition 'BPMG' (+32 = 2, +36 porteur, +40 pris) a seulement été
**lue dans le code, jamais vue en jeu**. Le filet reste coupé par défaut. Avant de l'activer : un essai joué par
l'utilisateur, une prise pendant le match, avec `wwe12_motion_guard_test_pair=1852`. Sinon, il faut trouver le vrai
objet de prise d'abord. Décision de la Main : on en reste là (option a).
