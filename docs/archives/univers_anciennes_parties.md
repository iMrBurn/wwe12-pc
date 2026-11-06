# Mode Univers : anciennes parties (archives)

Parties de `docs/univers.md` déplacées le 2026-11-06 (ménage demandé par l'utilisateur) : premiers plans, listes d'idées, étapes remplacées par des parties plus récentes, étude abandonnée. Gardées telles quelles, avec leur numéro d'origine.

## En-tête d'origine du document (2026-10-16)

Conversation « WWE 12 PC (Univers) », 2026-10-16. **Rien n'a été modifié** dans le jeu ni dans le
code du portage : ce document vient de la lecture des fichiers `.pac`, de l'exe (`image.bin`), du code
traduit (`port/generated`), d'une sauvegarde et du journal `univers.log` (sonde « [uni] » d'une
session précédente, même contenu que les lignes `[game 826D4228]` de `wwe12_game_log`).

Niveaux de certitude : **sûr** (vu dans le code ou les données), **probable** (déduit, très
vraisemblable), **à vérifier** (hypothèse, il faudra une sonde en jeu).

---

## 5. Ce qu'on ne sait pas encore (à faire avant de modifier)

1. **Offsets dans le bloc Univers** : champions de chaque titre, relations entre catcheurs, équipes,
   scénarios en cours, classements. → sonde en jeu : copier le bloc avant/après un événement connu
   (changement de champion, nouvelle rivalité) et comparer.
2. **Où et comment les NEWS sont écrites** : trouver le code qui utilise les ids 27060–27098
   (probablement passés en nombres, pas en texte) et la liste des actualités dans la sauvegarde.
3. **Détail des fiches MTDT/STYT/WCID** (champ par champ), pour pouvoir **ajouter** des matchs de
   qualification et des scénarios.
4. **Lien scénario → numéro de scène** (Act) → archive `Season/RSC/Winning`.
5. **Royal Rumble** : comment la liste des 30 participants est remplie (point d'entrée pour les
   « surprises »).
6. Correspondance mémoire ↔ fichier `SaveData.Dat`.

Ces points demandent de **lancer le jeu** avec des sondes de lecture (sans rien changer au jeu).

## 6. Ce que ça veut dire pour la suite (étapes 1 à 4)

- **Booking (titres, PPV, qualifications, Rumble)** : surtout dans le **code** (sub_826C8690,
  sub_826E3920, sub_826F1028, Rumble). Les modèles de matchs sont dans MTDT (données) : on pourra en
  ajouter si on comprend le format, sinon intercepter la génération de carte.
- **Rivalités** : relations dans la sauvegarde + scénarios STYT (données) + choix en code (826E9338).
- **Cinématiques** : scènes = données (`evt`), choix = code (826DF6D8 / 826ECE78). Réutiliser des scènes
  existantes est réaliste ; en créer de nouvelles est un très gros travail.
- **Actualités** : textes modifiables par `mods` (string.pac) ; de **nouveaux** types (qualifications)
  = code + nouveaux ids de textes.
- Idée des règles de booking dans un fichier réglable : réaliste **si** on remplace/complète la
  génération de carte (826E3920) par notre propre code natif qui lit un fichier.

---

## 8. Tests en jeu (2026-10-18, sauvegarde de l'utilisateur mise de côté puis remise)

- **Univers vidé** (copie où le bloc U est traité comme par `sub_826D5938`) : **plantage** en entrant
  dans l'Univers, juste après le chargement de WWEU/MTDT, STYT et BATL/SAVE. Lecture de 0x0000000C dans
  `sub_826EF988` (méthode virtuelle des scènes/actes, vtable 0x8203E684). Le jeu n'a pas recréé
  l'Univers : un interrupteur « l'Univers existe » est ailleurs. Prochaine étape : comparer une
  sauvegarde neuve avant et après la première entrée dans l'Univers.
- **Ceinture vacante** (titre 1 = **WWE Championship** mis à 512/512 dans la table Univers) : pas de
  plantage ; l'écran RANKINGS affiche le titre **sans nom ni portrait** ; le jeu garde le titre vacant
  et nomme quand même un aspirant n° 1 (Sheamus), mais **ne programme aucun match pour ce titre** (ni
  émission ni PPV). Il faudra donc ajouter nous-mêmes le match pour le titre vacant. L'utilisateur
  signale aussi un Univers « un peu cassé à certains moments » (à préciser).
- Confirmé : titre 1 = WWE Championship (RAW, MAJOR), 6 = United States, 12 = Divas, 13 = Unified Tag.
- **Décision de l'utilisateur (2026-10-18)** : pas d'option « ceinture vacante » (WWE '13 ne la
  propose pas dans l'Univers, et le jeu ne programme pas de match pour un titre vacant). On garde la
  remise à zéro de l'Univers et le suivi des stats.
- **Décision révisée (2026-10-18)**, après une suggestion de joueur (dans SvR 2011 on pouvait rendre
  vacants les titres non utilisés par l'émission) : on ajoute **VACANT seulement pour les titres
  qu'aucune émission n'utilise** (titres des légendes, etc.). Les titres des émissions restent
  comme avant (le jeu ne programme pas de match pour un titre vacant). WWE '12 n'a ni texte
  « VACANT » ni bouton : à ajouter (EDIT SUPERSTAR ou notre écran). Reste à trouver où le jeu
  range « quel titre appartient à quelle émission ».

## 14. Idées suivantes (suggestions de joueurs, 2026-10-20)

Dans l'ordre décidé : 1) tenue de chaque catcheur dans l'Univers (une précise ou Random, comme 2K14) ;
2) SAVE sur une carte de match modifiée avec MATCH CREATOR (WWE '13) au lieu de la lancer tout de suite ;
3) moins de matchs par émission (8 cases par émission dans la sauvegarde, donc 8 au plus) ;
4) catcheurs non suivis (« Vous ne pourrez pas suivre la progression de cette Superstar… », ex.
Christian) : l'Univers n'a que 180 fiches, toutes prises (50 créés + 130) : échanger un catcheur suivi
contre un non suivi ; 5) weekly n'importe quel jour et plusieurs PPV par mois (calendrier à cartographier).

Comme WWE '13 (captures de l'utilisateur, 2026-10-21) : CALENDRIER où l'on peut modifier ou supprimer une
émission ou un PPV existant (X MODIFIER, Y SUPPRIMER) et ajouter une émission majeure/mineure ou un PPV
n'importe quel jour, dimanche compris (X AJOUTER sur une case vide) ; CLASSEMENTS dont on change l'ordre
à la main (X MODIFIER). Calendrier = booking : 1.0 sauf si l'étude montre que c'est simple (décision de
l'utilisateur). Classements à la main : prévu pour la 0.9.

## 21. Idées venues de WWE 2K14 (captures de l'utilisateur, 2026-10-27) : ce qu'on sait

- **Paramètres d'aspirant (MAJEUR / MINEUR / TAG / DIVA par Superstar)** : WWE '12 n'en a pas dans sa
  sauvegarde. Le jeu décide « peut-il prétendre à ce titre » par `sub_82A39EF8(id, titre, 1)` (seulement
  homme/femme), appelé par les classements de l'Univers (`sub_826CFDA8` solo, retour 0x826CFF0C ;
  `sub_826D18E8` équipes, 0x826D1AE0) et par un filtre du booking (`sub_826E1380`, vtable 0x8203E3B4 slot 1,
  titre à +8, retour 0x826E13A8), mais aussi par TITLE MANAGEMENT, MATCH CREATOR et le match (non touchés).
  Le genre d'un titre = sa place dans les 4 titres de son émission (`sub_826CC180(émission, out[4])`,
  émission k = `sub_826CCB88(vue, k)`, vue de 428 octets `sub_826CDC48` / `sub_826CE098`). **Codé et testé le
  2026-10-27** (copie de la sauvegarde : MINOR = OFF pour un catcheur → il disparaît du classement US, reste au WWE) : dernière ligne de la boîte des tenues (Y sur la fiche) →
  « CONTENDER SETTINGS... » → MAJOR / MINOR / MORE..., TAG / DIVA, chaque ligne ON/OFF (textes du jeu 27606-27609,
  27613/27614 ; question = notre texte 27048) ; fichier `universe_contenders.txt` (« id masque »).
- **Options scénaristiques** (rivalités auto, tag teams, foule, blessures) : WWE '12 n'a aucune option de ce
  genre (aucun texte). Pistes dans le code : la mise hors service d'un Superstar (blessure) est
  `sub_826CEA88(vue, id, 1)` (bit 31 de la fiche Univers +92, semaine en +96, et il perd sa place d'aspirant
  n° 1) ; `sub_826E7A10` = blesser, `sub_826E7A70` = guérir (actions de scénario), autres appelants
  `sub_826C3388`, `sub_826C55E0`. Une option « BLESSURES OFF » = refuser l'appel avec 1 (la NEWS « Serious
  Injury », texte 27093-27094, s'écrit peut-être ailleurs : à vérifier). Rivalités et tag teams : non étudiés.
- **Statistiques** (titres : plus de règnes / de semaines ; émissions : anciens champions ; MITB : vainqueurs,
  cash-in ; Rumble : vainqueurs, WrestleMania) : à noter nous-mêmes. Changement de champion = toujours
  `sub_82A3A060(titre, catcheur, place)` (appelants : TITLE MANAGEMENT `sub_82564DD0`, résultats
  `sub_826C6530`, `sub_825396F8`, `sub_82684C00`, `sub_826D4030`, `sub_82DEA418`) ; semaine en cours =
  [vue+4] + 56772 (U + 0xDDC4) ; Rumble = `sub_826C8520` ; MITB = `sub_826F1028`. Le plus long : l'écran.
- **Calendrier (ajouter/supprimer une émission, PPV et organisateur)** : non (booking ; une émission vide fait
  planter les PPV, §20).
- **Nombre de matchs par émission** (MATCHES 5 de 2K14) : cartes de 8 cases au plus (§3.2), génération
  `sub_826C8690` non étudiée.

## 23. Pages ASPIRANT et STATISTIQUES sur la fiche, avec RB / LB (2026-10-27, testé sur la copie)

Sur la fiche d'un Superstar dans l'Univers, **RB** (barre du bas : « RB MORE ») passe de la fiche à
**CONTENDER SETTINGS** puis **STATISTICS** puis de nouveau la fiche ; **LB** dans l'autre sens ; **B** revient
à la fiche. Les pages réutilisent le vrai écran d'accueil de RANKINGS (4 ceintures de l'émission du
Superstar), comme les pages de 2K14 :
- en-tête (élément 830) : « NOM - CONTENDER SETTINGS / STATISTICS » ;
- ligne du champion de chaque titre (842/854/866/881 et leurs copies visibles 843/855/867/882) :
  ASPIRANT = sa place dans le classement (`sub_826D2508`), CHAMPION, OFF s'il est écarté, « - » si le titre
  n'est pas pour lui (`sub_82A39EF8`, ex. Divas pour un homme) ; haut/bas choisit, X active/désactive
  (rouge = ligne choisie) ; STATISTICS = « HELD n - n WEEK(S) » (`universe_stats.txt`).

Comment (écran actif du menu Univers M, r3 de `sub_82545A70`) : M+288 = panneau affiché, choisi dans la liste
de gauche par `sub_82544EE8` (0 MATCH CARDS +260, 1 SCHEDULE +276, 2 RANKINGS +264, 3 NEWS +280, 4 EDIT
UNIVERSE +268, 5 EDIT TEAM +272, 6 EDIT BRAND +284) ; changer = cacher l'ancien (`sub_82545070`, vtable +92
avec sa chaîne +256), M+288 = le nouveau, le montrer (`sub_82545008`, vtable +88). Pour nos pages : afficher
le panneau RANKINGS (R = M+264, R+2352 = émission 0/1, rafraîchi comme ses LB/RB : `sub_82552768`,
`sub_82551CB8`, `sub_82551F48`, `sub_82552048`), la fiche garde les touches (son +224 = 2, celui du menu
M+28+224 = 0). Au retour il faut remettre ce que le réaffichage d'EDIT UNIVERSE efface :
- **M+296** (2 = « l'écran d'un panneau est ouvert », sinon le menu ne reprend plus la main après la liste) ;
- la liste des Superstars (écran vtable 0x82027970 = panneau +412) : son défileur à +1760 ({+0 ligne,
  +4 première ligne affichée, +8 lignes visibles, +12 lignes, +16 boucle}, `sub_8254DE48`), puis sa fonction
  « ligne changée » `sub_8258D240` (avec +88 = 0) qui remet le Superstar courant (`sub_82DEA390`) et l'en-tête ;
- la fiche : `sub_825ACEC0` (son affichage) et `sub_825AAD38` (sa barre de boutons) ;
- les +224 de la fiche et du menu tels qu'avant.
Testé (télécommande `wwe12_remote_pad`, sur la copie) : RB → aspirant (CHAMPION / RANK 8), bas + X → OFF
(enregistré), RB → statistiques (HELD 1), RB → fiche, B → liste (curseur gardé), B → menu, RANKINGS normal.

## 24. Page STATISTIQUES en tableau, comme 2K14 (2026-10-27, testé sur la copie)

LB depuis la fiche (ou RB depuis la page aspirant) : la page détaillée d'un classement (RANKINGS puis A)
sert de tableau. Ouverture `sub_82553770(R+28)` (R+56 = place du titre, ici 0), fermeture `sub_82552D40(R+28)`
(son B) ; pages de liste R+56 = 1, 2 (R+60 = nombre de pages), 12 lignes : 1020 + k (libellé), 1032 + k
(colonne du rang, vidée), 1044 + k (valeur ; texte en +200, drapeau 0x10 de +228 effacé). Le jeu redessine
les lignes (`sub_82552A78`) et le haut (`sub_82552240`) : nos crochets remettent nos textes juste après. Haut :
897 = « STATISTICS », 898/899 vidés, 911 vidé, 912 = le nom du Superstar, 913-915 (portrait du champion) cachés.
Lignes : TITLES (DÉTENU - SEMAINES par titre de son émission, « -- » si pas pour lui, TOTAL), ROYAL RUMBLE
(victoires, participations), WRESTLEMANIA (matchs, main events, victoires, taux), MONEY IN THE BANK
(victoires, taux après cash-in) ; sections en rouge ; haut/bas = page 1/2 du tableau. Les blocs Rumble,
WrestleMania et MITB lisent des lignes « rumble / mania / mitb / cashin » de `universe_stats.txt` qui ne sont
**pas encore écrites** (prochaine étape : le résultat des matchs de l'Univers).

## 25. Résultats des matchs notés pour les statistiques (2026-10-27, compilé, pas encore vu en jeu)

Tous les résultats de l'Univers (match joué ou simulé) passent par `sub_826CEF28(vue, émission, -, match,
place du gagnant, place du perdant, 1)`. Émission : 0-9 les shows de la semaine, 10-22 les PPV dans l'ordre du
jeu (10 Royal Rumble, 11 Elimination Chamber, **12 WrestleMania**…). Matchs de l'émission : `sub_826CCC38(vue,
émission)` +4 = fiches de 104 octets, +8 = nombre (même lecture que `sub_826D0B90`). Fiche : +0 type de match
(`sub_82A2FA18(type) == 1` = Royal Rumble), +2 nombre de places (`sub_826D4F90`), +3 = 2 une fois joué. Place :
`sub_826D4958(fiche, sortie, place)`, sortie +0 Superstar (u16), +3 son équipe (Rumble : sa place, +5 = gagnant) ;
gagnant 6 (40 au Rumble) = pas de gagnant. Le Rumble garde son vainqueur à U + 51000 (`sub_826CCA20`).
Notre crochet lit tout ça avant l'original et ajoute à `universe_stats.txt` : « rumble <id> <gagné> <jour> »
pour chaque participant du Royal Rumble, « mania <id> <main event> <gagné> <jour> » pour chaque participant
d'un match de WrestleMania (main event = dernier match de la carte ; gagné = même équipe que le gagnant).
Journal : ligne « [univers] result: show … ». Reste : Money in the Bank (gagnant de la mallette, cash-in) pas
encore trouvé.

## 33. Enchaînement automatique après un match : étude (2026-10-27, pas encore de correctif)

Demande de joueurs (Discord) : après un match de l'Univers, le jeu lance tout seul le match suivant de la carte.
Sondes (`wwe12_universe_probe`, lignes « flow: ») sur 3 matchs joués :
- Le jeu enchaîne des étapes créées par la fabrique `sub_823DCC38(-, id 1..34)` (table de sauts à 0x823DCC70 +
  4 × (id − 1)), pilotées par un « propriétaire » (vtable 0x8200A8D0, `sub_823894C0` : +32 l'étape en cours, finie quand
  son +4 = 5 ; créateur vtable +56 → +40 objet, vtable +4). Après le résultat : ids 17, 18, 28, puis l'après-match
  (vtable 0x8200D310, `sub_823DDBB0` qui note le match suivant U+5068 via `sub_826BF4C0` / `sub_826BAF28`), puis id 0 = fin.
- Un nouveau mode démarre alors depuis `sub_822B3420` (gestionnaire de modes : this+8 objet, vtable 0 appelé avec
  this+12 = mode demandé) → `sub_822B5598` → `sub_82389E20` → id 1, puis 24, 20, 2, 25, 3, 5, 6, 7, 23, 8, 9, 10, 11
  (11 = préparation du match suivant, vtable 0x8200DF18, `sub_823F2E60` → `sub_826C8638`). L'écran MATCH CARDS ne
  tourne pas entre les deux (`sub_82547EE0` muet).
- Reste à trouver qui remplit this+12 (le mode demandé) à la fin de l'après-match : c'est là qu'il faudrait demander
  le mode « menu Univers » au lieu de « match suivant » quand l'option est active. Piste suivante : sonde sur les
  écritures de this+12 de `sub_822B3420` (ou sur `sub_822B5598` avec la valeur demandée et la pile).
