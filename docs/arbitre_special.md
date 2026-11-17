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

## Écrit le 2026-11-16, PAS encore vu en jeu

- **Entrée de l'arbitre** (`wwe12_sref_entrance`) : crochet de `sub_82A3F828(ordre)` ; les six premiers mots de la
  fiche d'ordre sont le rang de chaque côté ; le côté de l'arbitre reçoit le rang 0 (il entre en premier, comme
  `sub_82292268` de WWE '13) et son genre vaut 0 le temps de l'appel (`sub_82A3F0D8` saute les genres autres que
  0, 2, 7). À la cloche le rôle 1 se place à la place de départ de l'arbitre de la règle (`sub_823B83B8`).
- **Écran de choix** : les deux fonctions du panneau d'une place (`sub_82596848` choisi, `sub_825962D0` en cours ;
  r3 = panneau, +284 = la place) demandent le texte « COM » / « 1P » (40355, 40357 + manette) ; pour la place de
  l'arbitre on rend le texte du jeu 40775 « SPECIAL REFEREE » (drapeau lu par `Wwe12MatchModesGameText`).
- **Jauge** (`wwe12_sref_gauge`, arbitre joué seulement) : règles et nombres lus dans le composant 'SREF' de
  WWE '13. 0,5 au départ ; devoir non fait (le juge attend un compte et l'arbitre n'est pas dans l'animation du
  compte) : délai de grâce 3 s (tombé, 190) ou 5 s (191 / 200), puis -0,02 par seconde ; compte de tombé abandonné
  (190 bloc 2 pendant le tombé) -0,2 ; son coup touche un catcheur -0,1 (crochet `sub_822A7600` appelé depuis
  0x822A8E14, objet d'attaque +256 attaquant, +272 victime, +288 = 0) ; remontée +1/150 par seconde quand rien ne
  la fait baisser et qu'il ne porte rien. Dessin : barre en bas de l'écran (ImGui, notre dessin, pas d'image de
  WWE '13).
- **Jauge vide ou 30 fautes** (version simple, pas celle de WWE '13) : l'acteur perd le rôle 1 (acteur +3428,
  participant +8, fiche de création `[0x8322DD44] + 528 + 80 × n + 6`), reçoit +998 = 4 (hors des tombés) ; sans
  rôle 1 le juge compte seul (`sub_82265710`). Une fois hors du ring (zone +996 = 1 ou 16) il s'en va
  (`sub_82371C78`, comportement natif).
- **Screw job** : jauge pleine + Y pendant une prise ('BPMG' à [catcheur +3404], +32 = 2, +36 porteur, +40 pris) :
  `sub_82261320(pris, 2, numéro du porteur)`, puis les six mots de demande à 0x8317B7A4 à zéro.

## Ce qui reste (comme WWE '13)

- L'arbitre normal qui entre remplacer l'arbitre spécial (WWE '13 : arbitre créé garé au rôle 6, objet 'SRLV',
  animations 2043 / 2050 / 2051) ; les avertissements (2020-2028), la DQ prononcée (2040-2042), l'animation du
  screw job (2029) : **les animations 2012-2060 n'existent pas dans WWE '12** (m.pac), il faudrait les données de
  WWE '13 (accord explicite de l'utilisateur).
- L'arbitre frappé / qui frappe comme un catcheur : écrire le rôle 0 dans son acteur le temps des appels de
  `sub_8228C610`, `sub_8228BE40`, `sub_821D3F58`, `sub_8224B790` (ils testent le rôle 1 de la victime).

## Ancienne liste

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

## Cahier des charges de WWE '13 (lu dans son code et son script le 2026-11-16)

Animations de WWE '13 embarquées (`port/res/iquit13.bin`, accord de l'utilisateur du 2026-11-16) : 20NN de
WWE '13 = 13115 + (NN - 20) dans WWE '12 (2020 → 13115 … 2051 → 13146). Extraction : outil `m13` (scratchpad
`sref13motions`, sources WWE '13 `pac\mot00.pac` / `mot01.pac`, ZMOT/CA06 groupe 109 ; `.high` = le plus grand des
deux enregistrements passé par `yanm::ik::BakeLegIk`, `.m3` = `m3_params::FromOther` + `Write` sans retirer les
événements inconnus, comme les fichiers du I Quit qu'il refabrique à l'identique).
Durées (images à 60 Hz) : 2020 = 183 ; 2021 = 191 ; 2022 = 261 ; 2023 = 241 ; 2024 = 287 ; 2025 = 391 ; 2026 = 152 ;
2027 = 201 (2 corps) ; 2028 = 138 (2 corps) ; 2029 = 306 ; 2037 blocs 0 / 1 / 2 / 10 = 36 / 461 / 36 / 261 ;
2038 = 113 / 318 / 141 / 246 ; 2040 = 231 ; 2041 = 621 ; 2042 = 536 ; 2043 = 621 (2 corps + objets) ; 2049 = 151 ;
2050 = 251 ; 2051 = 331. Pas encore importées : 2045-2048 (ses provocations, qui remontent la jauge), 2030-2035.

- **Bouton de prise** (toujours une action d'arbitre, jamais une prise libre). Juge au repos : avertissement si
  jauge ≥ 0,1 et un catcheur à portée, sinon « rien à faire » (2049). Avertissement : l'arbitre joue 2020 ; à
  l'image 60, si le catcheur est libre : il lâche son arme, reçoit un « avertissement » (3 par catcheur, 60 s
  chacun) ; au 3e il conteste (2024 / 2025, puis l'arbitre répond 2026 à l'image 60 : -0,5, compteurs remis à
  zéro) ; sinon il obéit (2021 arbitre derrière lui, sinon 2022 / 2023) et ses adversaires au sol se relèvent ;
  -0,1 si le catcheur était dans son droit. De près (≤ 200 cm) : d'abord une bousculade à deux (2027 de face,
  2028 de dos), puis 2020.
- **Vérification de soumission** (pendant une prise) : bouton de prise → 2037 (prise debout) ou 2038 (au sol),
  bloc 0 puis 1 en boucle ; l'abandon ne compte QUE s'il vérifie (bloc 10 + fin du match) ; rupture de corde
  seulement s'il appuie ; sans vérification après 3 s la jauge baisse. Fin de la prise : bloc 2.
- **Screw job** : pendant une prise, jauge pleine, bouton de finisher → 2029 ; à l'image 100 celui qui porte la
  prise gagne par soumission.
- **Prononcer la DQ** : une faute (un catcheur frappe l'arbitre ; un homme frappe une femme ; un non-légal attaque)
  reste « en attente » 120 images puis ouvre 180 images pendant lesquelles le bouton de finisher la prononce
  (2040 debout, 2041 sur le dos, 2042 sur le ventre) : fin du match par DQ. Sinon elle est oubliée, sans coût.
- **Jauge** : dessin sous les pieds (le y du monde pointe vers le bas : « le plus haut y » = les pieds) ; à 720p :
  fond texture 48 (68 × 6), cadre 47 (88 × 16), remplissage 125 teinté (10, 80, 190) ancré à gauche (-34, -3),
  barre de perte rouge (234, 10, 6), maillot 122 (20 × 20) à +46 ; pleine : clignote bleu / blanc ; ≤ 0,25 : rouge /
  blanc. Textures : `game2dHD.pac` G?2D/BASE enfant 0, sous-dossier 90. Invites (images, enfant 38 : vérifier la
  soumission, compter, décision en attente, prononcer la DQ, screw job) : pas encore extraites.
- **Expulsion** (jauge vide ou 30 fautes) : l'arbitre spécial joue 2050 ; 180 images plus tard l'arbitre normal
  (garé depuis le début au rôle 6) apparaît à (0, -120, 0) et joue 2043 (animation à deux corps avec un second
  figurant et l'objet 142) pendant que l'arbitre spécial joue 2051 ; à la fin de 2043 : l'arbitre normal prend le
  rôle 1, l'ancien le rôle 0 avec « doit quitter le ring », puis il remonte la rampe (comportement 'RRTR').
- **Arbitre à l'ordinateur** : son IA appuie sur les mêmes boutons ; jauge et expulsion pareilles.

## Arbitre normal garé dans WWE '12 (étude du 2026-11-16, pas encore écrit)

1. Création : crochet de `sub_82A37378` : pour l'appel, genre de la place de l'arbitre spécial (M+684+2244×i) = 0
   et M+45 = 1, puis remis. Le participant 40 est alors créé (rôle 1, ordinateur) : il lui FAUT le rôle 1 à la
   création, sinon son IA (composant `CMIF`) n'est pas construite (`sub_822143D0` : pas pour les rôles 4 / 6).
   **Piège (2026-11-16)** : l'IA n'est PAS à acteur +3384 (écrit par d'autres fonctions, resté à 0 alors que l'IA
   existait : relais refusé à chaque test). Le bon test = chercher le composant `'CMIF'` (0x434D4946) dans la liste
   des composants de l'acteur à +3436 (comme `sub_827D5C28`) ; l'IA est à son +24. Fonction `Brain()` de
   `wwe12_sref.cpp`.
2. Le garer avant le premier `sub_823B5D18` (remise en place du match) : acteur[6] +3428 = 6, participant 40 +8 = 6,
   fiche de création `[0x8322DD44] + 528 + 80 × 6 + 6` = 6 ; chaque tick : `sub_822CF850(mgr, 6, 0)` (dessin),
   `sub_822CE3F0(mgr, 6, 0, 0)` (mise à jour), mgr = `[0x8317434C]`, drapeau +0 = 0 sur acteur +1360 et +1028,
   +998 = 1, position (0, y, 3200) (+752 / 756 / 760 et `[acteur+768]`).
3. Échange : l'arbitre normal : rôle 1 aux trois endroits, `sub_82212140(acteur, 1)`, 'CMIF' +40 = 1, +3140 = 1,
   +2536 = -1, 'CASH' mot +10 = 1 ; l'ancien : rôle 0 aux trois endroits, +2568 = 1, +3140 = 0, +1780 = 0,0.
4. Sortie : `sub_82371C78(acteur)` ('RRTR') une fois hors du ring (zone +996 = 1 ou 16) ; il ne franchit pas les
   cordes tout seul. Bord du ring ±313, tapis y = -120, rampe vers +z.
Correspondance WWE '13 → WWE '12 : +3336 → +3428 (rôle), +500 → +998, +160 → +996 (zone), +1172 → +1780,
+1268 → +2536 (objet tenu), +2564 → +3140, +1408 → +2568, drapeaux +808 / +532 → +1360 / +1028.

## Écrit le 2026-11-16 après-midi, PAS encore vu en jeu

- `wwe12_sref_actions` : `RefActions` (A hors compte : vérification de soumission 2037 / 2038, avertissement 2020
  avec réponse du catcheur 2021 / 2022 / 2024 et réplique 2026, sinon 2049 ; Y : DQ prononcée 2040-2042 dans les 5 s
  après qu'un catcheur a frappé l'arbitre, screw job 2029). Les animations sont demandées à la file du jeu avant
  d'être jouées (`Wwe12MotionReady`), puis tenues leur durée (`Step` / `Start`).
- `wwe12_sref_replacement` : crochets `sub_82A37378` (l'arbitre du jeu gardé), `sub_823B5D18` et `sub_823B8F38`
  (garé au rôle 6, caché), `HandOver` (jauge vide : 2050, 3 s, puis l'arbitre du jeu montré au milieu du ring avec
  2043 pendant que l'arbitre spécial joue 2051, puis échange des rôles ; l'ancien arbitre a +998 = 4 et s'en va
  une fois hors du ring).
- Jauge : pièces de WWE '13 (cadre 47, fond 48, remplissage 125 teinté, maillot 122), sous les pieds.
- Pas fait : bousculade avant l'avertissement de près (2027 / 2028), arme lâchée, ruptures de corde accordées par
  l'arbitre, abandon qui ne compte que s'il vérifie, ses provocations (2045-2048), images des invites (enfant 38),
  coups reçus comme un catcheur, arbitre spécial joué par l'ordinateur.

## Pièges trouvés au test du 2026-11-16 (17:45)

- Le cerveau d'un acteur (`CMIF`, mise à jour `sub_8232E3B0`) n'agit que si l'acteur est dans sa **motion libre 1350**
  (`[acteur+680]` → n° de motion). Après une motion à nous (2051, idle d'arbitre 20150, 446…) il reste figé :
  `sub_8247C670(acteur)` le remet dans sa motion libre (fonction `Free()`).
- `sub_822143D0` (mise en place d'un acteur) le replace en (0, 33, 0), sous le ring : garder sa place et son cap autour.
- Sortie d'un homme hors du match : `+2568` ≠ 0 et but du cerveau `+2140` = 0 → but 7 → `sub_8234FD30`, qui finit
  par `sub_82371C78` (remplace son composant de commande `+3444` par la marche de départ).
- Caméra de 2043 : ses hauteurs sont sous le sol (plan A -59.5) ; WWE '13 soulève toute la scène de 180 cm (la scène) :
  origine de la caméra à y = -300 comme les corps.
- `sub_822143D0` recopie d'abord le rôle de la fiche du participant (`sub_8267CD30` +8) dans l'acteur +3428 :
  pour que l'arbitre garé (rôle 6) garde son cerveau à la dernière mise en place de la cloche, il faut le rôle 1 dans
  les **trois** fiches (`SetRole`) le temps de l'appel. **Ne jamais rappeler `sub_822143D0` en plein match** : le
  test de 17:58 a planté juste après (appel virtuel pur dans la mise à jour des composants, `sub_827D5380`).
- Le tick (`Wwe12SrefTick`, depuis `sub_82246DE0`) tourne sur **deux fils** : le fil principal (celui de la lecture
  des manettes `sub_8225C4D8` et de la mise à jour des composants `sub_827D5380`) et un fil de travail, au milieu de
  la mise à jour d'un cerveau. Sortir un objet de sa réserve (`sub_82254518`, le micro) depuis le fil de travail a
  bloqué le jeu (18:03 : les deux fils s'attendaient). Le relais (scène, objets, motions) ne se fait que sur le fil
  principal (`OnMainThread()`). **À vérifier aussi pour le micro du I Quit** (`RefPlay`, même appel).
- Vérifié au test de 18:03 : l'arbitre garé garde son cerveau après la mise en place de la cloche (rôle 1 dans les
  trois fiches).
- Descente de la rampe comme WWE '13 (à faire) : depuis la scène, le cerveau du vrai arbitre ne le fait pas marcher
  (test 18:08, 30 s immobile à z 2477). Le jeu a une entrée « Royal Rumble » (`'RREn'`, créée par `sub_823317D8(acteur)`
  depuis `sub_82386168`), mais elle joue des motions de catcheur 8000-8015 et lit la manette : pas pour un arbitre.
  Piste retenue : une marche scriptée (position et cap à chaque image, motion de marche de l'arbitre) ; le journal
  « l'arbitre du jeu : motion … bloc … » relève les motions qu'il joue en montant dans le ring.
- Étude du 2026-11-16 (18:38 match normal / 18:40 après le relais) : tout est pareil entre l'arbitre d'un match
  normal (qui marche) et le nôtre (immobile), sauf le drapeau **acteur +1096** (+68 du bloc +1028, copie +1428) :
  0 (parfois 1 / 2 un instant) sur l'arbitre normal, **1 en permanence** sur le nôtre et sur l'ancien arbitre, tous
  deux après des motions à nous (`sub_8247AD18` avec r8 = 1). `Free()` le remet à 0. Si ça suffit, l'arbitre
  descend seul de la scène (comme WWE '13) ; sinon aides à 12 s (pied de la rampe) et 20 s (dans le ring).
- Test du 2026-11-16 19:57 : l'ancien arbitre sort seul du ring (motion 717, entre les cordes) et remonte la rampe
  quand on lui donne la marche 'RRTR' dès le relais, dans le ring. Cette marche pilote l'acteur par une **manette
  virtuelle** (`sub_82371D10` → `sub_8225C4D8(pad, mot)`), comme la marche de l'arbitre du I Quit (`wwe12_iquit.cpp`,
  vue marcher en jeu le 2026-11-15). Le relevé en match normal : l'arbitre marche dans la motion 20152 (bloc 0), +1096
  y passe à 1 de temps en temps (ce n'est donc pas un blocage). L'arbitre du jeu descend maintenant de la scène par
  cette manette (`Wwe12SrefWalkWord`) : vers le pied de la rampe (0, 430), puis vers le ring (0, 120) ; aides : pas de
  progrès 5 s → pied de la rampe, 30 s → dans le ring.
- Test du 2026-11-16 20:04 : l'arbitre descend bien la rampe à pied par la manette virtuelle (13 s jusqu'au pied
  de la rampe), mais un arbitre ne monte pas dans le ring par le stick (arrêté au bord, z 363). Au bord du ring il
  devient catcheur (rôle 0, +998 = 0) le temps de monter (le jeu choisit la motion, comme la sortie 717 d'Orton), puis
  arbitre à nouveau. Micro : tenu par le manager général (objet +96 = lui). Jauge cachée pendant la scène.
  À faire : la course (WWE '13 : l'arbitre court) - le bouton de course de la manette virtuelle n'est pas encore trouvé.
- Étude du 2026-11-16 20:17 (manette de l'ordinateur) : mot = stick dans le 3e octet (0x80 | direction << 4),
  boutons dans les 16 bits bas. Remonter dans le ring : au bord, stick vers le ring + boutons **0x1400**, le jeu joue
  716 (entre les cordes). La motion de marche 20150 sert aussi aux catcheurs. L'arbitre au bord du ring : rôle 0 et
  +998 = 4 (pas de décompte ni de tombé sur lui) le temps de monter, boutons pressés une image sur deux ; rôle 1 à
  nouveau une fois dans le ring et la motion 7xx finie ; sinon un petit pas derrière les cordes au bout de 3 s.
  La course n'est pas trouvée (aucune course dans ce relevé).
- Tests du 2026-11-16 20:27-20:47 : entrée dans le ring = chaîne du jeu 712 (sur le tablier, 2 s) → 750 (3,4 s) →
  7010 → marche ; la laisser finir (12 s max). Dans le ring l'arbitre reste planté, +1428/+1096 remis à 1 par le jeu
  aussitôt effacés (l'effacer ne sert à rien). Essai : le remettre en place comme le jeu le fait pour chaque homme à
  la mise en place d'un match (`sub_823B5C58` : poids des deux couches de motion `[+3396]+68/+76` remis, motion
  libre sur les canaux 0 et 2 (`sub_82479730`, canaux à +216 + 48 × n), navigation `oNav` (`sub_8232E200`)) :
  `PutInPlace()`, sur le fil principal, après sa montée dans le ring.
- Tests du 2026-11-17 (sondes) : notre arbitre passe les premières portes de l'état libre du cerveau comme un arbitre
  normal, mais sa manette ne reçoit **aucune entrée** (mots à 0 ; l'arbitre normal : le stick). Le comportement
  d'arbitre du jeu `sub_823C97B0` n'est appelé que si sa manette demande « l'action 2 » (`[+3376]` virtuelle +36 =
  `sub_8225B340(pad, 2, 0)`), jamais pour lui ; appelé d'ici (800 fois) il n'a écrit aucun mot non plus. Donc son
  travail dans le ring est fait d'ici : stick de la manette virtuelle (près des catcheurs, à 170 cm de leur milieu ;
  vers un tombé en courant) et compte par les fonctions du jeu (`sub_823C9458` le commence, puis motion 190 bloc 1 /
  bloc 3 toutes les 0,8 s ; 5 / extérieur / au sol : un appel par compte, 1 s). Course : bouton **0x400** (celui
  qu'écrit la marche « aller à un point » du jeu, `sub_8228F5B8`) - à confirmer en jeu. Le juge seul : 5 s.
- L'ancien arbitre au milieu du ring sortait parfois par le côté opposé à la rampe et se coinçait près des marches
  (Mason Ryan, 08:21) : il marche d'abord vers le côté rampe du ring (z > 150, 4 s au plus), puis 'RRTR'.
