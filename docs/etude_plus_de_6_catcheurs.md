# Plus de 6 catcheurs sur le ring : état des lieux (étude, 2026-11-21)

Demande de Main (rêve d'iMrBurn), pour **après la 1.5**. Étude en lecture seule : rien compilé, rien lancé.
Sources : le code recompilé (`port/generated`), `image.bin`, la table des règles de `misc.pac` (lue sur la copie
locale du jeu, rien copié), nos notes (`docs/limit_breaking.md`, `docs/arbitre_special.md`, `docs/squash.md`) et une
recherche sur le web (rien téléchargé).

## En bref

- Le « 6 » n'est pas un seul réglage. Il est dans **trois endroits solides** : la liste des personnages du match
  (9 cases), la table des règles (6 places par fiche) et des centaines de boucles et de tests écrits pour 6.
- La mémoire n'est sans doute **pas** le premier mur sur PC : on sait déjà agrandir les tas du jeu.
- Personne n'a fait plus de 6 dans un jeu de la série SvR 2008 → WWE '13 (rien trouvé de public). Le seul exemple
  prouvé est un patch de **Here Comes the Pain** (PS2) qui monte à **8**, en prenant la place de l'arbitre.
- Première piste réaliste : **8 catcheurs** en réutilisant les **cases 7 et 8** de la liste (aujourd'hui deux
  personnages hors du ring), sans agrandir la liste. Premier test petit et mesurable : voir plus bas.

## 1. Où est gravé le « 6 »

| Endroit | Ce qu'on a vu | Ce que ça veut dire |
|---|---|---|
| **Liste des personnages du match** `0x8317B770` | 9 pointeurs : cases 0-5 = catcheurs, 6 = arbitre (rôle 1), 7-8 = deux personnages de rôle 4 (hors du ring). La variable suivante du jeu commence juste après (`0x8317B794`) | **969 fonctions** lisent cette liste. 169 la parcourent jusqu'à 9, 125 comparent avec 6. On ne peut pas l'allonger sur place |
| **Table des règles** (`misc.pac`, bloc `RUL`, 119 fiches de 108 octets, lecteur `sub_82A30590`) | Nombre de personnes à +0, puis **6 places de 3 octets** {numéro, équipe, genre}. 21 règles ont 6 personnes, aucune n'en a plus | Le format lui-même s'arrête à 6. Pour 8, il faut une fiche plus longue (lue par notre code), pas juste un chiffre changé |
| **Écran de sélection** | Les déplacements des manettes entre les places viennent de la fiche de règles + 24 (tables pour 1/3/5/7/8 personnes), la mise en page de + 52, les managers de + 79 (essais de l'arbitre spécial) | Il faudra des cases en plus à l'écran, ou choisir les catcheurs 7-8 ailleurs (notre menu, réglage) |
| **Manettes** | L'affectation des manettes (`sub_826E09D8`, `sub_826E0B18`) saute les rôles 1 et 3. La vibration connaît 8 manettes (`0x83248BD8[pad]`, pad < 8) | 4 manettes de joueurs suffisent. Les catcheurs en plus seraient à l'ordinateur |
| **Royal Rumble** | 30 entrants, mais le ring utilise les mêmes cases : un éliminé est remplacé par le suivant (entrée `'RREn'`, `sub_823317D8`, depuis `sub_82386168`, qui parcourt les 9 cases) | Le Rumble est donc lui aussi à **6 en même temps**. Il montera avec le reste, pas avant |
| **Arbitre** | Le travail d'arbitre (compter les tombés, DQ) va à **un seul** arbitre, la case 6 (vu avec l'arbitre spécial) | Ne pas toucher à la case 6 : notre version garde l'arbitre, contrairement à HCTP |
| **Caméra, IA, collisions, barres de vie, fin de match** | Pas encore regardé un par un : ce sont les 125 fonctions qui comparent avec 6 et les 169 boucles jusqu'à 9 | C'est le gros du travail : chaque test « < 6 » oublie les catcheurs en plus |

Petit outil d'étude (scratchpad, pas dans le dépôt) : un script qui liste toutes les fonctions qui utilisent la
liste `0x8317B770` et leurs tests avec 6 ou 9. Il se refait en quelques minutes si besoin.

## 2. Ce qu'il faudrait agrandir

1. **Rien à agrandir pour 8, si on prend les cases 7 et 8.** Les boucles « jusqu'à 9 » (collisions, positions,
   mises à jour, Royal Rumble) voient déjà ces cases. Il faut seulement que ces deux personnages soient des
   catcheurs (rôle 0, modèle, prises, IA) au lieu de leur rôle 4.
2. **Les tests « < 6 »** (125 fonctions) : il faut les trouver un par un et leur faire voir les cases 7-8. Avec un
   crochet par fonction, ou mieux, par un petit changement au moment de la génération du code.
3. **Plus de 8** (10 et plus) : déplacer la liste ailleurs dans la mémoire (une nouvelle liste plus longue) et
   changer ses 1 098 adresses dans le code. C'est faisable mécaniquement dans un portage recompilé (le code est du
   C++ généré), mais tous les « case 6 = arbitre » et « cases 7-8 » codés en dur seraient alors à revoir. C'est un
   chantier long, pour une 2.x.
4. **Fiche de règles** : une fiche « 8 personnes » à nous, lue par notre crochet (comme pour l'arbitre spécial).
5. **Écran de sélection et barres de vie** : des cases en plus. Dans HCTP, les plaques des barres de vie n'avaient
   que 6 places : l'auteur a dû leur ajouter une zone à part.

## 3. Mémoire

- La 360 n'avait que 512 Mo, d'où les plafonds. Sur PC, on agrandit déjà les tas du jeu (`wwe12_heap_grow`, tas
  12 de +16 Mo et 15 de +4 Mo, pour 50 catcheurs ajoutés) et on sait voir ce qui manque (`wwe12_heap_probe` écrit
  « PhysicalHeap::Alloc / heap N failed » ; `wwe12_heap_watermark` montre le remplissage).
- Ce que coûte un catcheur de plus sur le ring (modèle, textures, motions) n'est **pas mesuré**. Ce sera la première
  mesure : remplissage des tas avec 6 catcheurs, puis avec 8.
- Le répertoire des motions (`sub_8246E3B0`) est rempli pour les catcheurs du match. Plus de catcheurs = plus de
  motions chargées. Ce bloc a déjà cassé une fois (crash du Royal Rumble, 1.4.1) : à surveiller.

## 4. Ce que d'autres ont fait (web, rien téléchargé)

- **SvR 2008 PS2 avec 8 ou 10 catcheurs : rien trouvé** (Smacktalks, YouTube, GameFAQs, forums PCSX2, codes
  pnach). Le jeu d'origine a un Royal Rumble « 10 hommes » (10 entrants, 6 sur le ring au plus) : c'est peut-être
  de là que vient le « 10 ». Une seule question sans réponse sur Smacktalks (« 2 catcheurs de plus, pour en avoir
  8 ? »). Compte X du moddeur donné par l'utilisateur : `https://x.com/a050651`. **Illisible sans être connecté**
  (X demande un compte, aucun miroir ni moteur de recherche ne le montre) : rien de confirmé. Pour avancer :
  l'utilisateur ouvre le compte connecté et copie les messages ou vidéos utiles, ou demande au moddeur quelle
  version du jeu et quelle méthode (patch de l'exécutable, codes pnach, fichiers modifiés).
- **Le seul exemple prouvé : Here Comes the Pain (PS2), 8 catcheurs**, projet « smackdown-hctp-8player » de
  TahirrMirza sur GitHub (licence MIT, octobre-novembre 2026, d'après ses propres documents) :
  - le jeu avait déjà 8 cases de personnages et de manettes ; le plafond était dans les menus et dans environ
    **80 limites 5 / 6 / 7 écrites en dur** ;
  - il a réutilisé deux fiches de match inutilisées (bataille royale à 8, échelle à 4 équipes de 2) et monté les
    boucles de 6-7 à 8 (chargement des personnages, mémoire, textures) ;
  - ce qui a cassé : une case vide dans un tableau de 7 (lecture de 192 Ko par-dessus la mémoire du système), un
    numéro de catcheur invalide (boucle infinie), la boucle des finishers, les revanches (objets d'affichage
    périmés), les plaques des barres de vie (6 seulement) ;
  - ce qui reste : **plus d'arbitre** (sa case est devenue un catcheur), seulement 2 modes à 8 (Royal Rumble, tag,
    Elimination Chamber restent à 6), zoom de la caméra à revoir ;
  - la mémoire n'était pas la limite (environ 17 Mo de libres).
- SvR 2009 / 2010 / 2011, WWE '12, WWE '13 : aucun mod à plus de 6 trouvé. Le 8 officiel n'arrive qu'avec WWE 2K18.

## 5. Plan par paliers

**Palier 0, premier test petit et mesurable (1 séance, rien de visible pour les joueurs).**
Un réglage de développement : dans un match à 6 (6-man tag ou Elimination Chamber), donner aux cases 7 et 8 deux
catcheurs du roster au lieu de leurs personnages de rôle 4. On mesure :
1. le jeu tient-il (pas de plantage au chargement ni dans la première minute) ;
2. les deux catcheurs en plus sont-ils dessinés, bougent-ils, peuvent-ils être frappés ;
3. le remplissage des tas (`wwe12_heap_watermark`) et les images par seconde (benchmark), avec 6 puis 8.

Avant de demander ce test : lire `sub_822CDF78` (création des personnages) et ce que deviennent les cases 7-8
(qui sont-elles, qui les crée, qui les remet à leur rôle), pour que le test ait une vraie chance de marcher.

**Palier 0 : préparé le 2026-11-22 (ordre de la 2.0 validé par iMrBurn), pas encore compilé ni lancé.**
- Ce qu'on a lu dans le code :
  - `sub_822D23A0(chargeur, case, fiche)` demande chacun des 9 personnages du match. La fiche fait 80 octets :
    - +0 = catcheur × 100 + tenue ;
    - +4 = la personne (0-5 les catcheurs, 40 l'arbitre, 41-42 les deux en plus) ;
    - +5 = la case ;
    - +6 = le rôle (0 catcheur, 1 arbitre, 3 arrivée en cours de match, 4 en plus) ;
    - +8 = 1 pour les catcheurs et l'arbitre, 0 pour les deux en plus ;
    - +16 = la classe (1 = humain pour les 9) ;
    - +20 = le tas mémoire (30 + case) ;
    - +32 à +63 = la position de départ.
  - `sub_822D0618` fabrique le personnage. Avec le rôle 4, il laisse de côté presque toutes les pièces d'un
    lutteur (prises, IA, dégâts…). Avec le rôle 0, il les met toutes. La classe et la taille restent les mêmes.
  - `sub_822CE4A0` remplit la liste `0x8317B770` à partir des mêmes fiches.
  - Les tas mémoire (table `0x83073260`) : 30-35 = 1 152 Ko (cases 0-5), 36 = 544 Ko (arbitre), 37-38 = 492 Ko
    (cases 7-8). Ce ne sont que les tas propres à chaque case. Les tas 18-23 (6 × 1 088 Ko) et 27-29 servent peut-être
    aussi aux personnages : à surveiller dans les « [heaps] ».
  - Les cases 6-8 sont créées au premier match puis gardées. Le test doit donc être le **premier match** après
    le lancement.
- Le réglage : `wwe12_dev_eight_wrestlers` (coupé par défaut), dans `port/src/native/wwe12_dev_eight_wrestlers.cpp`,
  appelé par une ligne du crochet de `sub_822D23A0` (`wwe12_discord.cpp`). Il ne touche que les fiches des cases
  7-8 encore au rôle 4 :
  - +0 = un catcheur de `wwe12_dev_eight_ids` (« 12302,13902 » par défaut) ;
  - +6 = 0 ;
  - +8 et +9 recopiés de la case 1.
- Lanceur : `tools/dev/lancer_test_8catcheurs.bat` (données `wwe12_test`, journal `test_8catcheurs.log`). Il
  agrandit les tas 37 et 38 de 660 Ko et suit le remplissage des tas. Faire un 6-MAN TAG ou un ELIMINATION
  CHAMBER en premier match, jouer 2 minutes, quitter.
- Ce qu'on lit dans le journal :
  - `[eight] slot N` : chaque fiche avant / après ;
  - `[eight] list` toutes les 10 s : les 9 personnages, leur rôle (+3428) et leur position ;
  - `[eight] heaps` : le tas de chaque case ;
  - `[perf]` toutes les 10 s : les images par seconde ;
  - `[heaps] … failed` : la mémoire qui manque.
- Ce qui peut arriver, et ce qu'on en apprend :
  - plantage au chargement : il manque de la mémoire ou une case dans un tableau à 6 (voir `[heaps]` et la trace
    de plantage) ;
  - les deux catcheurs restent hors du ring ou figés : leur position de départ et leur comportement « en plus »
    viennent d'ailleurs (la personne 41-42, le rôle remis à 4 : voir `[eight] list`) ;
  - ils se battent : le palier 1 peut commencer.

**Test 1 du palier 0 (2026-11-22, joué par iMrBurn, 3 contre 3 en premier match, 2 min) : rien de visible.**
- Journal : les fiches 7-8 ont bien été changées quand le jeu les a demandées. Elles se suivent bien, à 80 octets
  l'une de l'autre : la 7 est 480 octets (6 × 80) après la case 1. Les tas 37-38 ont été agrandis à 1 152 Ko.
- Mais les deux personnages sont restés au **rôle 4**, sur le sol hors du ring (y 0, z -880, les catcheurs sont à
  y -120), et ne bougent pas.
- Les 446 lignes « PhysicalHeap::Alloc unable to alloc » sont **normales** : elles viennent toutes du démarrage
  (10:31:26, 80 s avant le match), et on trouve le même nombre (446 ou 892) dans les autres journaux de test.
- Images : environ 600 présentées en 10 s (60 / s), comme d'habitude (seulement 6 catcheurs à l'écran).
- Ce que dit le code :
  - Avec le rôle 4, le jeu fabrique une variante du lutteur (`sub_823A3310` appelle le constructeur
    `sub_82213DB0`, puis change sa table de fonctions). Avec le rôle 0, il fabrique le lutteur normal, et
    +3428 = rôle de la fiche = 0.
  - `sub_826DD2C0` cache les personnages au rôle 4 (+1736 / +1752 = 1) et leur redonne le rôle de la table des
    participants du jeu (`[0x8322DD44]` + 528 + case × 80). Mais il ne s'occupe que de ceux qui sont **déjà** au
    rôle 4.
  - Donc nos deux personnages ont été fabriqués au rôle 4 : la fiche lue au moment de la fabrication n'était plus
    la nôtre, ou la fabrication s'est faite à partir d'une autre copie.
- Pas suivant (prêt à compiler) :
  - un crochet sur `sub_822D0618` (la fabrication elle-même) remet la fiche des cases 7-8 au rôle 0 juste avant ;
  - la même chose dans l'entrée de la table des participants, si c'est une autre fiche ;
  - le journal dit ce qui a été fabriqué : `[eight] making / made`, avec la classe, le rôle et « caché » ;
  - réglage `wwe12_dev_eight_place` (activé) : les deux catcheurs partent de la place des cases 2 / 3, décalés de
    60, au lieu du sol hors du ring.

**Test 2 (2026-11-22, même match) : toujours rien de visible, mais on avance.**
- Les cases 7-8 sont **fabriquées au rôle 0**, avec la classe des catcheurs (82004220, comme les cases 0-5).
- 10 s plus tard, elles sont **de nouveau au rôle 4**, à leur place hors du ring. Et c'est bien le même personnage :
  la classe est restée celle d'un catcheur.
- La fiche est l'entrée de la table des participants du jeu (même adresse). Le jeu remet son octet +6 à 4 après
  la fabrication, probablement en recopiant la table d'un bloc. Puis `sub_826DDC30` / `sub_826DD2C0` recopient ce
  rôle dans le personnage (+3428), et le code du rôle 4 le replace hors du ring. Aucune écriture d'un seul octet ne
  le fait (cherché dans le code) : il faut donc prendre le coupable sur le fait.
- « +1736 / +1752 = 1 » ne veut pas dire « caché » : l'arbitre, bien visible, a la même valeur.

**Essai 3 (prêt à compiler)** :
- toutes les 100 ms, la fiche 7-8 et le personnage sont remis au rôle 0 s'ils sont repassés à 4 (`[eight] force`) ;
- le piège matériel `wwe12_pin_trap` (registre DR2, sur `wwe12_watch_addr`, réglé tout seul sur la fiche 7)
  donne le code du jeu qui réécrit la fiche (`[compte] adresse surveillee … wwe12.exe+X`) ;
- la liste des 10 s dit si le personnage de la case est bien celui qu'on a fabriqué.

**Essai 3 (2026-11-22, capture d'iMrBurn) : les deux personnages sont dans le ring !**
- Ce sont **les deux commentateurs** (la case 7-8 = la table des commentateurs). Remis au rôle 0 toutes les
  100 ms, ils sont restés dans le ring à la place prévue, mais avec leur apparence de commentateurs et sans bouger.
  La table des commentateurs est restée vide.
- Le piège DR2 a trouvé le coupable : `sub_828C38E0`, une étape de mise en place du match (appelée par
  `sub_828E5C08` / `sub_828E5DA0`). Environ 150 ms après la fabrication des personnages, elle vide l'entrée
  (`sub_824C0F90`) puis la remplit à nouveau à partir des personnes 41-42 : numéro 15006 / 15306, rôle 4.
- Images : 60 / s, comme d'habitude.

**Essai 4 (prêt à compiler)** :
- crochet sur `sub_8268B380`, la fonction par laquelle le jeu lit une entrée de la table (84 appels). Chaque
  lecture des entrées 7-8 les retrouve en catcheurs (numéro, rôle 0, place). Le modèle et ce qui suit devraient
  donc suivre ;
- réglage `wwe12_dev_eight_persons` (vide par défaut) : changer la « personne » des cases 7-8 sans recompiler, si
  l'IA dépend de la personne (les commentateurs n'ont sans doute pas d'IA de lutteur).

**Essai 4 (2026-11-22) : gel au chargement du match.**
- Le « premier match » était le **match de démonstration de l'écran-titre** (un 1 contre 1 : cases 0, 1, 6, 7, 8).
  Il a été modifié lui aussi : les cases 7-8 y ont été refabriquées 3 fois de suite.
- Pendant la préparation du vrai match (règle 4), le crochet de lecture a de nouveau changé les entrées 7-8 avec
  les fiches de la démo. Juste après, le chargement s'est figé : plus d'image pendant 1 minute, puis la fenêtre a
  été fermée (`hang_trace.txt`).

**Essai 5 (prêt à compiler)** :
- seulement dans un match où les cases 2 à 5 sont demandées aussi (6 catcheurs) : la démo et les 1 contre 1 ne
  sont pas touchés ;
- tout est oublié quand un match demande la case 0 (nouveau match) ;
- le crochet de lecture, la refabrication et la remise à 0 n'agissent que pour ce match, et la lecture seulement
  dans les 10 s qui suivent la fabrication des cases 7-8 (le temps des reconstructions de la mise en place).

**Essai 5 (2026-11-22) : pas de gel, mais toujours les deux commentateurs, immobiles.**
- Le numéro de catcheur dans la fiche ne suffit pas. L'apparence et le comportement viennent de la **personne** du
  match.
- La table des personnes (`sub_8267CD30` → `sub_82686058` : `[0x8317C84C]` + 132 + n × 48) a **43 places**. Une
  personne contient : +16 = catcheur × 100 + tenue (51200 = vide), +8 = la case, puis +21 / +22 / +23 / +45.
- `sub_828C38E0` reconstruit les fiches à partir des personnes.
- La correspondance case → personne vient de `sub_8268B218` (15 appelants) : cases 0-5 = personnes 0-5, case 6 =
  40 (arbitre), 7-8 = 41-42 (les commentateurs). Les personnes 6 à 39 servent aux entrants du Royal Rumble.

**Essai 6 (prêt à compiler)** :
- crochet sur `sub_8268B218` : cases 7-8 → personnes 6-7 (réglage `wwe12_dev_eight_persons_map`, activé) ;
- les personnes 6-7 sont remplies en copiant les personnes 2 et 5 (une de chaque côté : un 3 contre 3 devient un
  4 contre 4), avec les catcheurs de `wwe12_dev_eight_ids` et leur propre case (+8).

**Essai 6 (2026-11-22) : plantage au chargement du match.**
- Les personnes 6-7 ont bien été remplies (copies des personnes 2 et 5, catcheurs 123 / 139), et les cases 7-8
  pointaient bien vers elles (`sub_8268B218`, appelée depuis `sub_82A38C94`, puis `sub_823DEB04`).
- Puis, pendant le chargement : « PhysicalHeap::Alloc unable to alloc » et une lecture à l'adresse 0x18
  (`crash_trace.txt`).
- La fonction en cause est `sub_82837EC8`, dans la chaîne `sub_823BD9A8` → `sub_823BD730` → `sub_823BD528` →
  `sub_823BBFA8` → `sub_82839848` → `sub_82837EC8`. C'est le chargement du modèle 3D d'un personnage. Elle demande
  de la mémoire physique (`sub_82A9C000`) pour une taille = nombre × pas, lue dans une table de fiches de 184 octets
  par index (r25 = 9 au moment du plantage). La demande est refusée et le résultat vide est utilisé.
- Lecture : c'est le mur attendu, une table du chargeur de modèles prévue pour les personnes habituelles (6 catcheurs,
  l'arbitre, les 2 commentateurs). Pour les personnes 6-7, l'entrée n'existe pas ou est vide, ou alors la mémoire est
  trop juste. À noter aussi : le lanceur de test prend 20 Mo de plus pour les tas 12 et 15 (`--wwe12_heap_grow_from=0`),
  ce qui ne sert à rien ici.
- **Prochaine étape : une étude sans test** de ce chargeur (`sub_823BD528` / `sub_823BBFA8` / `sub_82839848` : qui
  remplit la table de 184 octets, combien d'entrées, à partir de quoi). Le but est de trouver comment le Royal Rumble
  charge ses entrants (personnes 6-39) et de faire pareil. Plus d'essai à l'aveugle avant cela.

**Idée d'iMrBurn (2026-11-22) : de vrais types de match à 8** (4 contre 4, combat à 8, etc.). C'est le palier 1, à
faire dès que l'essai 6 montre deux vrais catcheurs qui bougent :
- 8-MAN TAG (4 contre 4) = la règle du 6-man tag (3 contre 3) + 2 personnes de plus, une par équipe (ce que fait
  l'essai 6) ;
- FATAL 8-WAY = la règle du 6-pack challenge + 2 personnes de plus, chacun dans sa propre équipe ;
- menu : de nouvelles lignes, ajoutées comme celles d'I QUIT et de SPECIAL REFEREE (`wwe12_match_modes.cpp`) ;
- choix des 2 catcheurs en plus : l'écran de sélection n'a que 6 places (sa mise en page). Première version : ils
  se choisissent ailleurs (réglage, notre page, ou au hasard). Ensuite, voir si l'écran peut avoir 8 places.

**Plan des risques (pour le palier 1)**

| Partie | Risque | Ce qu'on fera |
|---|---|---|
| Arbitre | Faible : on garde la case 6. Mais il ne regarde peut-être que les cases 0-5 (tombés, soumissions, DQ, sorties du ring) | Tester un tombé sur un catcheur des cases 7-8 ; si l'arbitre ne compte pas, trouver ses boucles « < 6 » |
| Caméra | Moyen : le cadrage suit les catcheurs. Elle peut ignorer les cases 7-8 (catcheurs hors de l'image) ou mal calculer son zoom (HCTP a dû revoir le sien) | Regarder les captures ; chercher les lecteurs de la liste dans le code de la caméra |
| IA | Moyen : les catcheurs à l'ordinateur choisissent une cible parmi les cases 0-5. Les deux en plus ne seraient alors jamais visés | Journal des cibles ; corriger les boucles de choix de cible |
| Barres de vie / noms | Moyen : l'affichage a sans doute 6 places, comme dans HCTP (6 plaques) | Pas de barre pour les cases 7-8 au début. Ensuite, ajouter des places |
| Fin du match / résultat | Élevé : le gagnant et l'écran de résultat ne connaissent sans doute que 6 places | Ne pas faire gagner les cases 7-8 au palier 1, puis corriger |
| Mémoire | Moyen : d'autres tas que 37-38 peuvent manquer | `[heaps]` ; agrandir au besoin (le PC a de la marge) |
| Entrées / sorties | Faible : les cases 7-8 n'ont pas d'entrée, elles sont déjà là | Rien au palier 0 ; plus tard, une entrée ou une arrivée en cours de match (rôle 3) |
| Univers, sauvegardes | Nul : rien n'est écrit | — |

**Palier 1, « match libre à 8 » (3 à 6 séances).** Fiche de règles à 8 lue par notre crochet, choix des catcheurs
7-8 (réglage ou notre menu avant le match), IA des catcheurs en plus, puis les tests « < 6 » qui comptent pour un
match libre : cible de l'IA, fin du match (tombé, soumission), barres de vie, caméra. Ce qui peut casser : la fin du
match qui ignore les cases 7-8, l'écran de résultat, les prises à plusieurs, les entrées (le jeu ne fait entrer que
6 catcheurs), la mémoire des motions.

**Palier 2, Royal Rumble à 8 en même temps (2 à 4 séances, après le palier 1).** `sub_82386168` et l'entrée
`'RREn'` parcourent déjà les 9 cases. Il faut que le remplaçant d'un éliminé puisse aussi aller en case 7-8, et que
le décompte des entrants les connaisse.

**Palier 3, plus de 8 (long, 2.x).** Liste déplacée et agrandie (1 098 adresses), « case 6 = arbitre » revu
partout, écran de sélection refait. Seulement si le palier 1 tient bien.

**Risques.** Élevé pour la stabilité tant que les tests « < 6 » ne sont pas tous vus : prévoir un réglage coupé
par défaut, comme pour l'arbitre spécial. Univers et sauvegardes non concernés tant qu'on ne touche qu'au match.
