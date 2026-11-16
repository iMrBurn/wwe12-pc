# Special Guest Referee (1.4) : état et savoir acquis

Portage du mode « arbitre spécial » de WWE '13 dans WWE '12. Décision de l'utilisateur (2026-11-16) : la 1.4 sort
quand il est en place (menu, entrée, habillage, jauge). Code : `port/src/native/wwe12_sref.cpp` (+ deux appels dans
`wwe12_match_modes.cpp`, partie I Quit, et deux dans `wwe12_iquit.cpp`). Réglage : `wwe12_sref` (**false par
défaut** tant que tout n'est pas vu en jeu). Lanceur de test : `tools/dev/lancer_test_arbitre3.bat` (journal
`test_arbitre3.log`, lignes `[sref]`). Études détaillées du 2026-10-22/26 : `docs/limit_breaking.md`.

## Ce qui marche (vu en jeu par l'utilisateur, 2026-11-16)

- **Menu** PLAY > SPECIALTY > SPECIAL REFEREE (2e ligne) > ONE ON ONE / TWO ON TWO / TRIPLE THREAT / FATAL 4-WAY >
  stipulations (1v1 : NORMAL MATCH, EXTREME RULES, HELL IN A CELL, IRON MAN, LAST MAN STANDING, SUBMISSION ; les
  autres : NORMAL, EXTREME RULES, HELL IN A CELL). Navigation et retour avec B.
- **Écran de choix** : la dernière place est celle de l'arbitre (vu : 1v1 et triple threat).
- **Match 1v1 NORMAL** joué en entier : le catcheur choisi est l'arbitre (rôle 1), pas d'arbitre normal, joué à la
  manette, tombé compté 1-2-3 avec A, fin de match normale.
- **Pas encore joués** : les matchs à 3 / 4 combattants, par équipes, et les stipulations (menu et écran de choix
  seulement).

## Comment ça marche

1. **Menu** (`Wwe12SrefMenu`, appelé au début du crochet `sub_82DE92F8` de `wwe12_match_modes.cpp`) : la table des
   menus (fiches de 0x74 octets, en-tête 0x28 : +4 total, +8 affichées) reçoit nos fiches. Un sous-menu = copie de
   la fiche FREE BRAWL ; une stipulation = copie de la ligne du jeu du même match (trouvée par son texte et son
   parent), donc elle joue la règle de cette ligne (**fiche +92 = la règle**). ONE ON ONE > NORMAL MATCH joue la
   règle 83 (SPECIAL REFEREE, restée dans misc.pac).
   - **Les id doivent être croissants dans la table** : le jeu retrouve une ligne par son id avec une recherche
     dichotomique (`sub_82DE8718` et copies en ligne). Une ligne ajoutée dans une page reprend donc l'id de la
     ligne d'avant (une ligne de match sans sous-menu) ; nos pages vont en fin de table avec des id au-dessus de
     tous. Les fiches d'une page doivent se suivre (table des pages `sub_82DE8F88`). La ligne I QUIT avait ce
     défaut (B bloqué depuis TWO ON TWO > TORNADO TAG) : corrigé le 2026-11-16, actif par défaut.
   - Champs d'une fiche : +0 texte, +0x16 profondeur, +0x18 page, +0x1C id, +0x38 parent, +0x3C genre (bit 0 :
     s'ouvre, bit 2 = 4 : ligne finale, bit 1 = 2 : dernière de sa page), +0x40 bit 0 du 1er mot : cachée,
     +0x54 lien (0x7D0 = un match), +0x5C règle.
2. **Choix d'une ligne** (`Wwe12SrefDecide`, appelé par `Wwe12MatchDecide`) : note la ligne choisie (règle, nombre
   de combattants).
3. **Écran de choix** (`sub_828AE798`, c'est lui que le menu PLAY appelle, pas `sub_82449898`) : la fiche de la
   règle (`[0x8324E238] + règle × 108`) reçoit une place de plus, de genre 1, sur un côté à elle (`Patch`) :
   +0 nombre de personnes, +24 table des déplacements de la manette entre les places (1 / 3 / 5 / 7 / 8 selon le
   nombre), +52 gabarit de l'écran, +79 = 0 (pas de managers), copiés d'une règle du jeu de cette taille (13, 14,
   38, 5). Remise comme avant au choix d'un autre match (`Restore`).
4. **Validation** (`sub_828AD3A0` → `sub_82A39AC0` puis `sub_82A36150(M, place, genre)` pour chaque place) :
   l'écran de choix remet genre 0 à toutes les places ; notre crochet garde 1 pour la place de l'arbitre. Le jeu ne
   crée alors pas l'arbitre normal (`sub_82A37378`), et le catcheur est créé avec le rôle 1 (acteur +3428).
5. **En match** (`Wwe12SrefTick`, une fois par image depuis le tick du I Quit) : le juge (`[0x83174054]` : +92
   état - 1 tombé, 3 compte de 5, 4 compte à l'extérieur, 5 compte au sol ; +100 compte ; +112 = 2 quand le rôle 1
   est un catcheur) ne compte que par les animations de l'arbitre. WWE '12 a gardé le compte d'un arbitre joué :
   `sub_823C9458(acteur)` le démarre (animation 190 / 191 / 200 / 202), `sub_8233F660` compte par les blocs 1 et 3
   de l'animation 190. Notre A appelle ces fonctions (le bouton natif du jeu dépend d'une table remplie à
   l'exécution).

## Ce qui reste (comme WWE '13)

- Entrée de l'arbitre (WWE '12 : `sub_82A3F0D8` saute le genre 1 ; WWE '13 le fait entrer en premier).
- Habillage de l'écran de choix (mention SPECIAL REFEREE sur sa place).
- Jauge (0,5 au départ ; baisse quand il ne fait pas son travail ou frappe un catcheur ; vide : l'arbitre normal le
  remplace ; pleine : « screw job »).
- L'arbitre qui se bat comme un catcheur (aujourd'hui : frappé, il réagit comme un arbitre bousculé).
- Arbitre joué par l'ordinateur ; DQ prononcée par l'arbitre ; avertissements.

## Pièges rencontrés

- Trois essais perdus pour avoir mis le changement de règle dans une fonction que le menu PLAY n'appelle pas :
  toujours prouver le chemin d'appel réel (journal des appelants) avant de demander un essai.
- `wwe12_kind_probe.cpp` n'est compilé qu'avec `WWE12_DEV_PROBES` : ne rien y mettre d'utile aux joueurs.
- Le tick tourne plusieurs fois par image du jeu : les délais du compte sont en ticks, pas en images.
