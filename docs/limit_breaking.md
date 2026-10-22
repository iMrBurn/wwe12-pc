# Limit breaking — idées à tester

Réserve d'idées pour repousser les limites du jeu d'origine (bloc-notes de l'utilisateur,
2026-10-22, plus les idées de départ). On ajoute au fil de l'eau et on note ce qu'on découvre.
Voir aussi `docs/suggestions_joueurs.md` (liste d'un joueur triée par effort) et `docs/univers.md`.

## Contexte

- La 360 n'avait que 512 Mo partagés. La plupart des plafonds (catcheurs à l'écran,
  slots de création, roster) viennent probablement de là, pas du moteur.
- Le PC a beaucoup de marge : ~390 Mo de VRAM par match en 1440p.
- Probable premier mur : les tas mémoire du jeu (déjà des « PhysicalHeap::Alloc unable
  to alloc » au chargement).
- Outils déjà en place : patch de .pac au démarrage servi par l'overlay VFS
  (cf. wwe12_menu_patch.cpp), hooks de fonctions (menu, fps, intros), scanner mémoire F7
  (wwe12_memscan.cpp), journal de débogage du jeu (`wwe12_game_log`).
- Acquis : le 6-man Elimination Chamber tient 60 fps sans crash.
- Limite des 6 catcheurs gravée dans le code : tableaux à 6 cases, budget mémoire Xbox, animations.
- Kaiko a fait un « Limit Breaking » pour son portage SvR 2011 : lui demander jusqu'où il est allé
  (plus de 6 catcheurs ?) et comment. Le Gauntlet vient aussi de SvR 2011.

Colonne « Qui » : la conversation qui s'en charge (Univers, Gameplay, Rendu…) ; « LB » = celle-ci.

## Stipulations à ajouter

| Idée | Piste | Qui | Statut |
|------|-------|-----|--------|
| I Quit | Absent de WWE '12 (aucun texte). Base : victoire « SUBMISSION ONLY » du Match Creator + résultat « GIVEUP » qui existe déjà. Voir notes 2026-10-22 | LB | étudié : version simple **moyen**, vraie (micro, animations 2K14) **très lourd** |
| Special Guest Referee | Le type existe dans la liste interne (n° 83, « SPECIAL REFEREE ») mais aucun menu ne l'offre : reste de SvR 2011, retiré dans '12. Logique encore là ? inconnu | LB | testé 2026-10-22 : type 83 forcé = match jouable, sans plantage, mais encore un match normal ; il manque le rôle d'arbitre (à trouver) |
| Gauntlet (SvR 2011) | **Déjà dans WWE '12** : JOUER > HANDICAP > GAUNTLET (et Match Creator). Même place dans SvR 2011 (Handicap > Gauntlet, apparu dans SvR 2009) : rien à ajouter. Seule piste : plus d'adversaires (voir « plus de catcheurs ») | LB | **existe déjà** |
| Combinaisons (cage en tag, HIAC à plus de 2…) | Mêmes tables de types de match | LB | idée |
| Enlever les restrictions du Match Creator | Menus grisés ; cf. `sub_825910F8` / `sub_82591750` (First Blood grisé) | LB | idée |

## Mode Univers (conversation Univers)

| Idée | Piste | Statut |
|------|-------|--------|
| Titres **utilisés** par une émission rendus vacants | Le jeu ne programmerait plus jamais de match pour eux ; il faut d'abord un tournoi (ligne suivante) | idée (vacant des titres non utilisés : **fait** 2026-10-20) |
| Tournoi pour couronner un titre vacant | Générer une série de matchs sur les cartes d'émission | idée |
| Menu Stat Tracking façon WWE '13 (+ historique des vainqueurs Rumble, MITB, KOTR…) | Le jeu ne garde ni victoires ni historique : à noter nous-mêmes | idée |
| Booking modifié (défenses de titre spontanées en weekly…) | Génération des cartes (docs/univers.md) | reporté à la 1.0 |
| Catcheurs réservés à certains titres (Cena majeurs, Bourne mineurs) | Filtre au moment du booking | idée |
| Backport des cinématiques Univers de WWE '13 / 2K14 | Scènes = données (`pac/evt`), choix = code (`sub_826DF6D8`) ; données d'autres jeux = question de droits | idée |
| Cinématiques de Create a Story déclenchées comme les vraies | Même mécanisme de choix (`sub_826DF6D8` / `sub_826ECE78`) | idée |
| Enlever le minimum de 15 catcheurs par émission (Univers 100 % CAW) | Trouver le test du minimum | idée |

## Modifs de jeu

| Idée | Piste | Qui | Statut |
|------|-------|-----|--------|
| Sang pour les catcheuses | `wwe12_blood.cpp`, voir « Savoir acquis » de CLAUDE.md | Gameplay | reporté à la 1.0 |
| 5 contre 5, plus de 6 catcheurs sur le ring | Tableaux à 6 cases, slots manettes/IA, spawn, mémoire | LB | idée |
| Royal Rumble : plus de catcheurs en même temps sur le ring | Même plafond | LB | idée |
| Touches du clavier affichées à la place des boutons Xbox/Sony | Même méthode que les icônes PS3 (planche d'icônes remplacée selon l'appareil) ; icônes à dessiner nous-mêmes | Gameplay | idée |
| Jukebox dans les menus | Lecteur de musique des menus (on sait déjà jouer des musiques perso) | Gameplay | idée |
| Moments OMG de WWE '13 | Nouvelles animations + déclencheurs : très lourd | LB | idée |
| 50 → 100/200 slots CAW | Zone des CAW : `sub_82684910`, fiche de 1 832 octets ; taille des sauvegardes | LB | idée |
| 20 → 100/200 logos perso | Idem, côté logos | LB | idée |
| Plus d'arènes créées, roster plus grand | Idem | LB | idée |
| Foule plus dense, distance d'affichage, LOD au max | Paramètres de la foule et des LOD | Rendu | idée |

## Gros chantiers

| Chantier | Notes | Statut |
|----------|-------|--------|
| Mod Maker pour la 1.0 | En parler avec Kaiko | prévu 1.0 |
| Jeu 100 % natif, sans émulation | Remplacer les fonctions recompilées par du C++ lisible, système par système (voir CLAUDE.md) ; en parler avec Jarrod (WWE 2K14) | long terme |
| D3D12 en plus de Vulkan (et peut-être D3D11) | plume gère Vulkan et D3D12, pas D3D11 ; aujourd'hui compilé Vulkan seul (`native_renderer.cmake`) | après la 1.0 |

## Notes de recherche

### Stipulations : I Quit, Special Referee, Gauntlet (2026-10-22, lecture seule)

Étude sans compiler ni lancer le jeu : textes de `string.pac`, arbre de `menu/menu.pac`, code recompilé.
Outils (lisent les données du joueur, ne les copient pas) : `tools/dev/strpac.py` (toutes les tables de
textes → TSV) et `tools/dev/menutree.py` (arbre des menus avec les noms).

**La table `[gest+556]` n'est PAS le catalogue des types de match.** `sub_82782AD8` la construit avec
`+560 = 10` entrées de 52 octets seulement (remplies depuis l'objet `[+548]`) ; `sub_827829B0` l'indexe
par `+72` (borné par `+76`) × 10 + `+56` (< 10). Ce sont des fiches par place/participant (d'où le drapeau
« pas de sang » à +32), utilisées par un automate d'états (`sub_82783xxx`, champs +564/+568/+572).

**Catalogue des types de match** : textes **27400–27520** (121 types internes), indexés par l'octet 0 d'une
fiche de match de 104 octets (`sub_82554B68`, fiches à objet+3248 : `27400 + octet`). Extraits :
54 IRON MAN, 59 LAST MAN STANDING, 67 SUBMISSION, 76 MONEY IN THE BANK, 81 INFERNO, **82 GAUNTLET**,
**83 SPECIAL REFEREE**, 84–90 ARENA BRAWL. **Pas de I QUIT.** D'autres listes de noms existent (textes
85–135 et 40700–40786, avec LUMBERJACK, THE DIRT SHEET BRAWL, GM OFFICE… = Road to WrestleMania /
coulisses) : SPECIAL REFEREE y figure aussi (96, 40775), LUMBERJACK (82, 40780).

**Menus d'exhibition** (`menu.pac`, 2e copie à 0x154800 ; même chose sous MATCH CREATOR, pages 28–43) :
- HANDICAP (page 11) = ONE ON TWO, ONE ON THREE, TWO ON THREE TAG, **GAUNTLET** (fiche 74, lien 0x7D0).
- SPECIALTY (page 12) = CHAMPIONSHIP SCRAMBLE, FREE BRAWL, ROYAL RUMBLE, MATCH CREATOR.
- Aucune entrée SPECIAL REFEREE, LUMBERJACK ni I QUIT.

**Fin de match** : résultats possibles (« BY WAY OF », textes 2942–2950) = PINFALL, COUNT OUT, TIME OUT,
**GIVEUP**, K.O., DQ, REFEREE STOP, FIRST BLOOD, SUBMISSION. Match Creator > WIN CONDITION (3356–3366) =
PIN AND GIVE UP, 2 OUT OF 3 FALLS, IRONMAN, LAST MAN STANDING, FINISHER MATCH, FIRST BLOOD, FLAMING TABLE,
CLIMB OUT OF CAGE, ELIMINATION, FALLS COUNT ANYWHERE ; RULES : PIN ONLY / **SUBMISSION ONLY** / NO EXITING.
Les conditions sont donc des réglages combinables, pas du code par type : bonne base pour de nouvelles
stipulations « assemblées ».

**Ce qu'il faudrait faire :**
- **Gauntlet** : rien à ajouter, il existe. Demander à l'utilisateur ce que la version SvR 2011 (de Kaiko ?)
  a de plus : plus d'adversaires, format « le gagnant reste »… Ça relèverait alors de « plus de catcheurs ».
- **Special Referee** : 1) test en jeu (avec accord) : forcer le type 83 sur une fiche de match (crochet sur
  la mise en place) pour voir si la logique d'arbitre jouable existe encore ; 2) si oui, l'offrir dans un
  menu. Ajouter une entrée à menu.pac casse la navigation (voir CLAUDE.md, 8e entrée du menu principal) :
  plutôt une entrée existante détournée ou une option dans notre écran. Si la logique n'existe plus :
  comptes, arbitre contrôlé, IA, animations = très lourd (reprise de WWE '13/2K14 en local seulement).
- **I Quit** : version simple = match « SUBMISSION ONLY » renommé, sans DQ ni décompte, fin par GIVEUP,
  avec un texte/son à la place du micro (moyen ; il faut trouver où le type fixe ses règles). Vraie version
  avec l'arbitre qui tend le micro et les animations : très lourd (animations et logique à reprendre de
  WWE '13/2K14, extraites en local depuis la copie du joueur, jamais publiées).
**Test en jeu Special Referee (2026-10-22, accord de l'utilisateur, 720p)** : sonde
`port/src/native/wwe12_kind_probe.cpp` (dev, `WWE12_DEV_PROBES`), lanceur `tools/dev/lancer_test_arbitre.bat`.
- 1er essai, crochet sur `sub_826D4888` : sans effet. Cette fonction ne sert qu'aux fiches d'Univers et des menus
  (type 119 = vide, 40 fiches au démarrage) ; l'exhibition écrit le type ailleurs (plusieurs écrivains :
  `sub_82449428`, `sub_826F8D08`, `sub_823E3220`).
- 2e essai, crochet sur le lecteur `sub_8267CF38` : l'exhibition TRIPLE THREAT NORMAL = **type 13** (1 contre 1 =
  type 0). Remplacé par 83 au lancement du match : **pas de plantage, match jouable à 60 fps, mais rien de
  visible** : 3 catcheurs qui se battent et l'arbitre habituel (capture de l'utilisateur, ring Nitro).
- Lecture : le type seul ne suffit pas. Le rôle « arbitre » d'un catcheur se décide sans doute à la mise en place
  des participants (avant notre remplacement, ou dans les fiches de place à +16). Indices : `sub_8227EEC0` teste
  la place `r27 == 3` + l'octet 33 de la fiche, `sub_82328E28`/`sub_823292C8` mettent une valeur 4 dans un tableau
  de 4 rôles quand le type est 83, `sub_821D0530` lit l'acteur+3428.
- **Rôles des participants (3e essai, même jour)** : `sub_822CDF78(type, fiche, …)` crée les personnages du match ;
  type 1 = humain (classe `sub_82213DB0`, ou `sub_823A3310` si fiche+6 == 4). `sub_82213DB0` recopie l'octet
  **fiche+6 = rôle** dans **acteur+3428**. Journal d'un TRIPLE THREAT : n° 0-2 = les 3 catcheurs (rôle 0),
  **n° 3 = l'arbitre normal (rôle 1)**, n° 4-5 = rôle 4 (hors ring, commentateurs/chronométreur ?).
  **Rôle 1 = arbitre.** En type 83, le jeu traite le rôle 1 à part (`sub_824EFA78` : vrai pour 0/2/3, et pour 1
  seulement en type 83 ; `sub_821D0530` saute un traitement quand rôle 1 et type 83).
- Donner le rôle 1 au 3e catcheur à sa création (sonde `wwe12_ref_slot`/`wwe12_ref_role`) : écrit (journal
  « n 2 role 0 -> 1 ») mais **il se bat normalement** (l'utilisateur jouait le 1er) ; il y a alors 2 arbitres.
- Pistes suivantes : `sub_82386168` (qui teste aussi le Gauntlet) remet acteur+3428 à une valeur (r26) à
  3 endroits, sans doute au début du match → réécrire le rôle **après** ; le comportement « arbitre » (IA,
  compte des tombés) dépend peut-être aussi de la classe ou d'autres champs ; l'arbitre normal devrait ne pas
  être créé en type 83 (`sub_82328E28` retire l'entrée 102 de sa liste de 4 en type 83 : modèle de l'arbitre ?).
- Suite possible : remplacer le type plus tôt (dès l'écran de choix des catcheurs) et lire ces fonctions pour
  trouver le champ « rôle » ; sinon chercher la logique dans WWE '13/2K14 (l'utilisateur les a, en local
  seulement).

- **Risque** : faible tant qu'on ne touche qu'aux réglages d'un match ; moyen dès qu'on force un type
  que les menus ne proposent plus (code mort possible, plantage en match) ; Univers et sauvegardes
  non concernés.
