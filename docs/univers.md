# Mode Univers : état, savoir et historique

Conversation « WWE 12 PC (Univers) ». Code : `port/src/native/wwe12_universe.cpp` (et `wwe12_jukebox.cpp`, le
jukebox). Les parties périmées (premiers plans, listes d'idées, étapes remplacées, étude abandonnée, plantage corrigé)
sont dans `docs/archives/univers_anciennes_parties.md` et `docs/archives/univers_anciennes_parties_2026-11-12.md` ;
rien n'a été supprimé. Les numéros de partie (§) ne changent pas.

## État au 2026-11-13

**1.4 (commits locaux, non poussés ; liste « Suivi 1.4 » de `docs/feuille_de_route.md`)** :
- **Deux mallettes Money in the Bank** (UNIVERSE OPTIONS > MALLETTES = 2, `wwe12_universe_mitb_briefcases`, défaut 1 =
  le jeu) : testé par l'utilisateur. Au PPV, le match MITB du jeu prend les hommes de RAW et une copie de son fiche
  remplace un match ordinaire (sinon une rivalité, sinon un titre mineur) pour SmackDown (`MakeSecondMitb`, au 1er
  `sub_826EAEB0` de la carte du booker PPV 0x8203EB44). Le 2e détenteur n'existe que dans `universe_stats.txt`
  (« mitb / mitbbrand / mitbclear » : `BriefcaseHolders`). Tout ce que le jeu fait avec SON détenteur (U+51004) marche
  pour les deux : tests « est-il le détenteur ? » (`sub_826CC718` selon l'adresse de retour : 0x826C0BE8 r29,
  0x82DEA6B4 r30, 0x826DB618 [r28+8], 0x826E6278 r31, 0x826D7A94 [r31+8]) ; proposition d'encaissement
  (`sub_82554460`) demandée pour l'autre en le mettant dans U+51004. Choix de qui encaisse : un NON au premier remplace
  la question par celle du second (`sub_825572C8`, état 2 de MATCH CARDS, réponse `sub_8288EEF8` : 1 = OUI, 0 = NON) ;
  deux NON remettent le premier. Mallette bleue pour SmackDown : Mods sert la copie bleue de SOBJ 2557 (et des scènes
  MitB) quand `Wwe12MitbBlue()` le dit ; le match d'encaissement charge « /SOBJ/2557 » directement. Pas d'entrée avec
  la mallette (WWE '12 n'en a pas ; celles de 2K18+ ne sont pas lisibles par nos outils).
- **Blessures** (UNIVERSE OPTIONS > BLESSURES, `wwe12_universe_injuries`) : refus de `sub_826CEA88(vue, id, 1)`. Gardé
  sans preuve en jeu : le jeu ne blesse que dans les matchs JOUÉS (`sub_826C55E0` sort si simulé) ou par une histoire.
  WWE '12 n'a pas d'autre « fin de carrière ».
- **Photo des arènes ajoutées** dans SHOW EDIT (crochet `sub_82586428`, nom donné par `Wwe12ArenaPictureName` de Mods) :
  codée, pas testée (il faut une arène de mod installée).
- **Plus de news** (`wwe12_universe_more_news`, idées et textes de Jay) : encaissement réussi / raté, séries de 5, 10, 15
  victoires et de 5 défaites (« res » par match), mallette gardée plus de 3 mois, Double Gold (titres qui ont déjà
  changé de mains seulement), règnes de 10 / 25 / 52 semaines, Fighting Champion (6 et 10 défenses), nouvelles médicales,
  match #1 contender (à 2 ou 3) et son vainqueur, adversaire mystère (« ??? »). Séries et Double Gold vus dans le
  journal ; #1 contender et mystère pas encore testés. Notre news : type 5, +14 = 0x53, +15 le genre, +10 l'argument
  (titres, nombre), +2 / +4 d'autres catcheurs ; textes composés au dessin. Le jeu vide NEWS à chaque show.
- **Fiche de carte** (104 octets, journal `wwe12_universe_card_probe`) : +0 type, +1 titre (FF aucun), +2 places,
  +3 état, +6 en-tête (1-8 MATCH #n, 9 MAIN EVENT, 10 #1 CONTENDER'S MATCH ; +7 n'est pas l'en-tête) ; places à +16 + 12 × p : +0 catcheur (u16), +3 camp,
  +6 = 1 caché (« ??? »).
- UNIVERSE OPTIONS : 5 lignes avec RETOUR (à 4 lignes le jeu dispose la boîte en 2 colonnes qui se chevauchent).
- Accents des news du draft corrigés (UTF-8 relu en Latin-1 par nos scripts : toujours écrire les textes en \x).

**1.3 (publiée)** : les CAW comptés dans les effectifs (12eb321, avec Gameplay) ; jukebox sans musique du jeu dessous.

**1.2 (publiée le 2026-11-10)** :
- Un catcheur ajouté garde sa marque après un redémarrage, un match et une remise à zéro de l'Univers. La part de Mods
  ne recopie plus la marque du pack. Chez nous, RestoreBrands saute les catcheurs ajoutés (`Wwe12AddedSuperstar`).
- Les news nomment un catcheur ajouté par son propre nom (crochet `sub_82559100`, `wwe12_universe_news_added_names`) :
  le jeu avait des textes fixes pour 12 numéros (115, 131, 135, 165, 182, 261, 272, 278, 330, 331, 338, 340).
- Journal des étapes (`wwe12_universe_flow_log`) : lignes « step id N made » de la fabrique `sub_823DCC38`. Ordre
  après un match joué : 19, 27, 22, 15, 17, « result: », 18, 28, 0. Testé : comportement inchangé.

**1.3 (publiée, détails)** :
- Les CAW comptent dans l'effectif des émissions, comme la TU2 : `wwe12_tu_caw_roster`, chez Gameplay
  (`wwe12_title_update.cpp`). Chez nous, le recomptage d'EDIT BRAND n'exclut plus les CAW, et le compte du départ d'une
  émission (`sub_826D0598` appelé depuis 0x826D3C84) reçoit r5 = 1 : il compte alors depuis le numéro 0 et non plus 100.
  Commit 12eb321.
- Jukebox, testé par l'utilisateur le 2026-11-11 (e587ded, 51e1b5f ; nettoyage 0370c86) : plus de musique du jeu
  sous un thème. Le menu principal (et chaque retour après un écran de chargement) remet [M+32] = 1 et la mise à jour
  relance la piste 0. Tant qu'un thème joue, la garde coupe la piste 0 ou 1 dès qu'elle démarre. Avec aucun thème en
  cours, un écran qui lance la musique (`sub_821D6368`) rend la main au jukebox.

**Fait et testé en jeu** (1.0 et 1.1, poussé) :
- EDIT UNIVERSE (menu en pages) : EDIT SUPERSTARS (marques, tenues), EDIT TITLES (VACANT titre par titre), EDIT
  CALENDAR, MATCHES PER SHOW, UNIVERSE OPTIONS, RESET UNIVERSE (marques et shows d'origine remis) : §11-§13, §16, §27,
  §29, §37.
- SCHEDULE façon 2K14 (Y EDIT DAY : ajouter, déplacer, matchs, retirer ; jours vides ; logos) : §31, §34 ; SHOW EDIT >
  Y MATCHES : §34.
- Cartes : choix du vainqueur à X SIMULATE, SAVE sur un match modifié : §17, §32 ; matchs par show (plantage 5/5 de la
  1.0.0 corrigé) : §29, §39.
- Classements complets et modifiables : §18, §19 ; fiche du Superstar façon 2K14 et STATISTICS (Y dans RANKINGS) :
  §22, §26, §29, §30 ; Money in the Bank noté : §28 ; shows de moins de 15 catcheurs : §20.
- Réglages 1.0 : défenses de titre en weekly (§35), qualifications MITB / Elimination Chamber + news (§36, §36 bis).
- 1.1 (§40) : jukebox arrêté à chaque écran de chargement (NEXT MATCH) ; image du PPV dans les news de qualification ;
  alliés / rivaux vides pour les catcheurs ajoutés (`Wwe12UniverseForgetRelations`, appelée par Mods à l'écran de
  sélection et par nous à l'entrée de l'Univers, une fois par ajouté et par mod).

**Pas encore testé en jeu** : un PPV à une seule marque (SCHEDULE > HOST SHOW > RAW : la carte n'a-t-elle que des
catcheurs de RAW ?) ; essai à l'aveugle le 2026-11-04 raté (écran éteint).

**Abandonné / retiré** : enchaînement automatique après un match (§33, archivé) ; qualifications du Royal Rumble ;
« MOVE » (changer l'ordre des matchs) : malentendu, le joueur voulait des catcheurs animés dans les écrans de choix et
VS (pas l'Univers, « à voir » pour l'utilisateur).

**Reste pour 1.3 / 1.4** (décision de l'utilisateur du 2026-11-07, puis du 2026-11-08 : Univers en 1.4) : deux
mallettes Money in the Bank
(§38, réglage 1 ou 2) ; couper blessures / fins de carrière ; classement des titres par équipe ; rivalités menées au
PPV ; rivalité inter-marques sans match ; titres de SmackDown dans EDIT SUPERSTAR (si c'est le jeu d'origine) ; photo
propre des arènes ajoutées (§41) ; ajout de logos / matchup screens (§41) ; surprises du Royal Rumble ; idées de joueurs
restantes (`docs/suggestions_joueurs.md`).

**Réglages** : `wwe12_universe_edit`, `wwe12_universe_min_roster`, `wwe12_universe_title_defenses`,
`wwe12_universe_qualifiers`, `wwe12_universe_news_ppv_picture`, `wwe12_universe_added_relations`,
`wwe12_universe_news_added_names`, `wwe12_universe_flow_log`, `wwe12_universe_added_arenas`,
`wwe12_universe_draft_news`, `wwe12_universe_probe` (sonde, coupée) ; jukebox : `wwe12_jukebox_stop_on_load`,
`wwe12_jukebox_keep_theme`, `wwe12_jukebox_log_screens`. Fichiers à côté de la sauvegarde : `universe_shows.txt`, `universe_attires.txt`,
`universe_contenders.txt`, `universe_stats.txt` (+ `_before_reset`), `universe_reset_hint.txt` ; `relations_cleared.txt`
(Mods).

**Pièges durables** : un crochet par fonction (une 2e définition casse le lien) ; un « return n » partagé n'a pas
toujours un objet en r3 (vérifier la vtable) ; la liste des news est vidée par le jeu après le 1er match d'un show ;
le jeu ne dessine pas le type de news 6 ni une news sans catcheur ; la table des relations est à la vue +12 (pas +4) ;
tester sur une COPIE de la sauvegarde, dans le seul dossier de test `Documents\wwe12_test`
(`lancer_test_effectif.bat`, `lancer_test_univers.bat`, `lancer_test_jukebox.bat`).

**Marques (page ROSTERS du launcher, 2026-11-10)** : fiche du catcheur = [0x8317FA08] + index × 286, avec
index = u16 [0x8307B5E0 + numéro × 2], soit numéro − 50, sur 323 fiches (`sub_82683878`). Le numéro est écrit dans la
fiche, en u16 big-endian à +0x28 : ne jamais le lire sur le seul octet +41. +24..+27 = marque, mais l'Univers lit et
écrit SEULEMENT +27 (`sub_826CEC60`, `sub_826D3DC0`, transferts `sub_8258A830` / `sub_8258B5F0`, et le résultat de
match `sub_826CEF28`, qui met un catcheur hors RAW / SmackDown dans la plus petite des deux). +24..+26 servent aux
autres modes. Valeurs : 0 RAW, 1 SmackDown, 6 sans marque (Mod Maker), 4 et 2 = autres marques acceptées par le jeu,
sens inconnu. Effectif d'une émission = catcheurs dont la marque +27 est cochée dans la fiche d'émission
(U + 27360 + k × 984, +929 + marque, `sub_826CC0B8`) : il n'y a pas d'autre copie de la marque.

## Sommaire

- 1. En bref
- 2. Les données (`.pac`)
  - 2.1 Format des archives (sûr)
  - 2.2 `pac/wuniData.pac` (546 Ko) : le cœur du mode Univers
  - 2.3 Cinématiques : `pac/evt/` et `pac/evtdsc/` (sûr pour le rangement, probable pour le rôle)
  - 2.4 Textes : `pac/string.pac` (sûr)
- 3. La sauvegarde et la mémoire
  - 3.1 Où est l'état de la partie (sûr)
  - 3.2 Dans le bloc Univers (probable)
  - 3.3 Champions, fiches Univers et format du fichier (2026-10-18, sûr)
- 4. Le code : qui fait quoi
  - 4.1 Les grandes fonctions repérées
  - 4.2 Le déroulement, tel qu'on le voit dans `univers.log`
- 5. Ce qu'on ne sait pas encore (à faire avant de modifier) (archivé)
- 6. Ce que ça veut dire pour la suite (étapes 1 à 4) (archivé)
- 7. Ce que fait WWE '13 (étudié le 2026-10-18, pour s'en inspirer) (archivé)
- 8. Tests en jeu (2026-10-18, sauvegarde de l'utilisateur mise de côté puis remise) (archivé)
- 9. Comment le jeu crée l'Univers (test « sauvegarde neuve », 2026-10-20) (archivé)
- 10. Remise à zéro : ça marche (2026-10-20) (archivé)
- 11. EDIT UNIVERSE > RESET UNIVERSE dans le jeu (2026-10-20, testé en jeu)
- 12. VACANT titre par titre, comme SvR 2011 (2026-10-20, testé en jeu en FR)
- 13. MODIFIER L'UNIVERS façon WWE '13, avec les vrais menus du jeu (2026-10-20, testé en jeu)
- 14. Idées suivantes (suggestions de joueurs, 2026-10-20) (archivé)
- 15. Tenue de chaque Superstar dans l'Univers (2026-10-21, testé en jeu)
- 16. Remise à zéro enregistrée tout de suite (2026-10-21, testé en jeu)
- 17. SAVE sur un match modifié de la carte, comme WWE '13 (2026-10-21, testé en jeu)
- 18. Classement complet pour tous les titres (2026-10-21, testé en jeu)
- 19. Classements changés à la main, comme WWE '13 (2026-10-21, testé en jeu)
- 20. Émissions de moins de 15 catcheurs (2026-10-27, testé en jeu sur une copie)
- 21. Idées venues de WWE 2K14 (captures de l'utilisateur, 2026-10-27) : ce qu'on sait (archivé)
- 22. Fiche du Superstar façon 2K14 : ce qui est fait (2026-10-27)
- 23. Pages ASPIRANT et STATISTIQUES sur la fiche, avec RB / LB (2026-10-27, testé sur la copie) (archivé)
- 24. Page STATISTIQUES en tableau, comme 2K14 (2026-10-27, testé sur la copie) (archivé)
- 25. Résultats des matchs notés pour les statistiques (2026-10-27, compilé, pas encore vu en jeu) (archivé)
- 26. Pages de la fiche façon 2K14, version finale (2026-10-27, vue en jeu par l'utilisateur)
- 27. Émissions d'origine remises à la remise à zéro (2026-10-27)
- 28. Money in the Bank noté (2026-10-27)
- 29. Façon 2K14 : stats générales, calendrier, matchs par show, menu EDIT UNIVERSE (2026-10-27)
- 30. Ceintures dans les statistiques générales (2026-10-27, testé seul à la télécommande)
- 31. SCHEDULE dessine les shows déplacés (2026-10-27, testé seul à la télécommande)
- 32. Choisir le vainqueur (X SIMULATE, comme les 2K récents) et SAVE corrigé (2026-10-27, testé par l'utilisateur)
- 33. Enchaînement automatique après un match : étude (2026-10-27, pas encore de correctif) (archivé)
- 34. SCHEDULE géré comme le PLANNING de 2K14 (2026-10-27, testé par l'utilisateur et à la télécommande)
- 35. Défenses de titre en weekly (1.0, 2026-10-27, testé sur la copie)
- 36. Matchs de qualification Money in the Bank / Elimination Chamber + news (1.0, 2026-10-27)
  - 36 bis. Elimination Chamber, news (suite, 2026-10-27, vu par l'utilisateur)
- 37. UNIVERSE OPTIONS dans EDIT UNIVERSE (1.0, 2026-11-01, vu par l'utilisateur)
- 38. Plan 1.1 : deux mallettes Money in the Bank (demande de l'utilisateur, 2026-11-01)
- 39. Plantage 1.0.0 « 5 matchs RAW + 5 SmackDown » corrigé (2026-11-03, testé sur la copie, candidat 1.0.1) (archivé)
- 40. 1.1 : jukebox, news des PPV, relations des catcheurs ajoutés (2026-11-05, vus par l'utilisateur)
- 41. Arènes ajoutées, logos et matchup screens dans SHOW EDIT (étude puis arènes faites, 2026-11-06)
- 42. Matchs par show : contenders en double et cartes incomplètes (1.1, 2026-11-06)
- 43. News du draft (1.1, 2026-11-06)

---

## 1. En bref

| Quoi | Où c'est | Données ou code ? |
|---|---|---|
| Calendrier, émissions (RAW, SmackDown, Superstars, PPV), mois | code + textes de `string.pac` | règles dans le **code**, noms dans les **données** |
| Modèles de matchs (qui affronte qui, types de matchs) | `pac/wuniData.pac`, table **MTDT** | **données** |
| Scénarios / rivalités scriptées (« StoryTemplate ») | `pac/wuniData.pac`, table **STYT** (136 scénarios) | **données** (le choix se fait dans le code) |
| Table **WCID** (4 listes, 261 fiches) | `pac/wuniData.pac` | **données**, rôle **à vérifier** |
| Place des ceintures sur chaque catcheur | `pac/wuniData.pac`, table **BLTS** | **données** |
| Cinématiques (scènes jouées) | `pac/evt/Season*.pac`, `RSC*.pac`, `Winning*.pac`, `Nyujyo*.pac` (entrées) + `evtdsc/` | **données** ; le **choix** est dans le **code** |
| Textes des actualités (NEWS) | `pac/string.pac`, ids 27059 à 27098 | **données** ; **quand** et **pour qui** : **code** |
| Roster, titres, rivalités, alliances, cartes générées, classements **de la partie en cours** | sauvegarde `SaveData.Dat`, bloc Univers en mémoire à `*(0x8317FA08) + 0x3F394` | état de la partie (ni .pac ni code) |
| Classements, choix des matchs, Draft, Money in the Bank, Royal Rumble, blessures, turns | code, module Univers `0x826BA000`–`0x826F5000` (~1 170 fonctions, ~230 Ko) | **code** |

En une phrase : **les « ingrédients » sont dans les données** (`wuniData.pac` : modèles de matchs et de
scénarios ; `string.pac` : textes ; `evt/*.pac` : scènes), **la « recette » est dans le code** (qui
affronte qui, quel scénario démarre, quelle scène joue, quelle actualité est écrite), et **le résultat
est dans la sauvegarde**.

---

## 2. Les données (`.pac`)

### 2.1 Format des archives (sûr)

Toutes les `.pac` utiles sont des **EPAC** de Yuke's :

- `+0x000` « EPAC » ; à `+0x800` un répertoire d'entrées de 12 octets
  `{nom 4 lettres, début (en secteurs de 0x800 après 0x4000), taille (en unités de 0x100)}`.
  La première entrée est le nom du groupe (ex. `WWEU`), les suivantes les sections.
- Chaque section est un **PACH** : `"PACH", nombre N, puis N × {id, offset, taille}` (u32
  little-endian), les données suivent (offset compté depuis la fin de la table).
- Les blocs peuvent être compressés en **BPE** (`tools/yukes_bpe.py` sait les ouvrir).
- Les données sont en **little-endian** (le jeu les retourne au chargement).

Les scripts de lecture écrits pour cette cartographie (lecture EPAC/PACH, recherche de références dans
l'exe, désassemblage depuis le code généré) sont restés dans le dossier temporaire de la conversation.
On pourra les ajouter à `tools/` à l'étape 1.

### 2.2 `pac/wuniData.pac` (546 Ko) : le cœur du mode Univers

Groupe `WWEU`, chargé par le jeu sous les noms `/WWEU/MTDT`, `/WWEU/STYT`, `/WWEU/WCID`, `/WWEU/BLTS`
(chaînes à 0x8203E813…, une petite classe de chargement par table, vtables vers 0x8203E7D8–0x8203E904).

| Section | Contenu (taille de fiche) | Ce que c'est |
|---|---|---|
| **MTDT** (17 tables « WMT ») | fiches de **508 octets** ; tables 0–12 : 3 fiches chacune ; 13 : 32 ; 14 : 12 ; 15 : 2 ; **16 : 465** | **Modèles de matchs**. Chaque fiche : n° de fiche, type, nombre de participants, places (`0x18 0x00`, `0x18 0x01`…) et règles. La table 16 (465 fiches) est probablement la réserve des matchs d'émission ordinaires ; les 13 petites tables de 3 fiches correspondent sans doute aux **PPV** (12 noms de PPV dans `string.pac` ; à vérifier). |
| **STYT** (« WST », 1 bloc de 197 Ko) | **136 scénarios** (~1 452 octets chacun) | **Scénarios** (« StoryTemplate ») : déclencheur et rôles **CastA à CastF** (le jeu imprime `StoryNo,Trigger,CastA,CastB,CastC,CastD,CastE,CastF`). Contient la suite d'« actes » (scènes) du scénario (probable). |
| **WCID** (4 tables « WCI ») | **564 octets** par fiche : 40 + 55 + 52 + 114 = **261 fiches**, plus un petit bloc « WUE » (52 octets) | **À vérifier.** Valeurs du type 100 (« dddd ») et quelques nombres qui ressemblent à des drapeaux. Hypothèse : réglages des scènes/situations par catégorie. Le chargeur de WCID est voisin du message `StEliminateData file size is over.` |
| **BLTS** (1 PACH de 296 textes) | id = n° du catcheur ; texte `x,y,z,échelle…` | **Position, rotation et échelle de la ceinture de champion** sur chaque catcheur (cinématiques, entrées). |

### 2.3 Cinématiques : `pac/evt/` et `pac/evtdsc/` (sûr pour le rangement, probable pour le rôle)

Chaque archive contient des scènes **numérotées** :

| Archives | Numéros | Rôle (probable) |
|---|---|---|
| `Nyujyo0`–`NyujyoA.pac` (« nyūjō » = entrée en japonais) | 100–1412 | entrées / arrivées |
| `Season0`–`Season6.pac` | 1010–1140, 3500–3840… | scènes de **scénario** Univers (agressions en coulisses, promos…) |
| `RSC0`–`RSC3.pac` | 4000–4068 | autres scènes Univers (sigle inconnu) |
| `Winning0`–`WinningA.pac` | 6100–6342 | scènes **d'après-match / de victoire** |
| `evtdata.pac` (« CSDT ») | groupes `U/E/I/F/G/S` × `000/001/002/006` | données des scènes par **langue** (U = anglais US, E, I, F, G = allemand, S = espagnol) |
| `evtmot.pac`, `evtobj.pac` (531 objets), `evtmov.pac` (vidéos `M000`…) | | mouvements, accessoires, vidéos des scènes |
| `evtdsc/evtdsc0-2.pac` (« DSC0/1/2 », entrées `R000`, `L010`…, `M000`…) | | **descripteurs** des scènes (caméra, déroulé ; à vérifier) |
| `wunimot.pac` (18 Ko, groupe `UNIV` vide) | | presque vide |

Le jeu construit aussi des noms `UNVS/%1d%03d` et `UNVS/%04d` (numéro de scène sur 4 chiffres).

### 2.4 Textes : `pac/string.pac` (sûr)

Tables de textes zlib (voir CLAUDE.md, menu). Ce qui concerne l'Univers (anglais, table `0000ED34`) :

- **Actualités (NEWS)** 27059–27098, un titre et un texte à `%s` :
  New Alliance / New Rivalry / New Team / New Stable / New Champion! / Fan Favorite (face) / Why %s Why?
  (heel) / Rankings / Upset Victory / Royal Rumble Winner / Mr. Money in the Bank! / Bitter Rivals /
  Strong Alliance / Serious Injury / Surprising Return! / Complete Recovery!
  → **il n'existe que ces ~20 modèles** ; on peut changer les textes par le dossier `mods`, mais
  **ajouter un nouveau type d'actualité** (ex. « X s'est qualifié pour le Money in the Bank ») demande
  de toucher au code.
- **Cartes** : MATCH #1–#8, MAIN EVENT, #1 CONTENDER'S MATCH, GRUDGE MATCH, SMACKDOWN VS. RAW, WWE
  DRAFT MATCH… et 4 `RESERVE_03`–`RESERVE_06` vides (places libres pour de nouveaux en-têtes !).
- **Calendrier** : mois, jours, émissions RAW (lundi), SUPERSTARS, SMACKDOWN (vendredi), PPV
  (dimanche) ; les 12 PPV (Royal Rumble → TLC, WrestleMania XXVII).
- **Titres** (7 suivis par l'Univers) : WWE, United States, Women's, World Heavyweight,
  Intercontinental, Divas, Unified Tag Team (+ ECW, WCW, Hardcore, Cruiserweight non suivis).
- Types de matchs 27400+ ; messages (simulation, cash-in du Money in the Bank, formation d'équipe…).

Note : « GetNews », « NewsTicker » dans l'exe concernent les **services en ligne de THQ**, pas l'Univers.

---

## 3. La sauvegarde et la mémoire

### 3.1 Où est l'état de la partie (sûr)

- Toute la sauvegarde est en mémoire à l'adresse contenue dans **`0x8317FA08`**.
- Petites fonctions d'accès (toutes près de 0x82683xxx) :

| Fonction | Renvoie | Contenu |
|---|---|---|
| `sub_82683850` | base | début de la sauvegarde |
| `sub_82684910(id)` | base + id × **286** (catcheurs du jeu), sinon zone des CAW (index × **1 832** à base + 0x1B9CC) | **fiche d'un catcheur** (nom, etc.) |
| `sub_82683898` | base + 0x168DC | ? |
| `sub_82683CD8` / `sub_826838F0` | base + 0x3BAC8 / 0x3BB4C | ? |
| `sub_82683CC0` / `sub_82683908` | base + 0x3F36C / 0x3F388 | ? (juste avant l'Univers) |
| **`sub_82683A30`** | **base + 0x3F394** | **bloc Univers** |

- Sur le disque : `Documents\wwe12\B13EBABEBABEBABE\54510890\00000001\SaveData.dat\SaveData.Dat`
  (5,7 Mo, **non chiffré**, noms des catcheurs lisibles, fiches de 286 octets). La correspondance
  exacte entre une adresse en mémoire et une position dans le fichier reste à vérifier (décalage
  d'en-tête).

### 3.2 Dans le bloc Univers (probable)

`sub_826CDC48` fabrique un petit objet de 428 octets qui « range » le bloc Univers (U = base + 0x3F394) :
un pointeur vers U + 20160, puis des listes `{début, nombre, capacité = 8}` à U + 27360 + k × 984.
**Probable** : ce sont les **cartes pré-générées**, une liste de 8 matchs (MATCH #1–#8) par émission,
984 octets par émission (123 par match). Les classements, les champions, les relations
(rivaux/alliés), les équipes et les scénarios en cours sont aussi dans ce bloc (**offsets à trouver**).

### 3.3 Champions, fiches Univers et format du fichier (2026-10-18, sûr)

**Format de `SaveData.Dat`** : en-tête de 28 octets (7 u32 gros-boutistes)
`{0x4F490, 0x5EE90, 0x38, 0x13, 9, 2, total}`, puis **trois blocs de mémoire copiés tels quels** :

| Bloc | Adresse en mémoire | Taille | Position dans le fichier |
|---|---|---|---|
| 1 : catcheurs, **Univers**… | `0x8317FA14` (`*0x8317FA08`) | 0x4F490 | 28 |
| 2 : réglages du jeu, **titres**… | `0x831CEEA8` (`sub_82684BE8`) | 0x5EE90 | 28 + 0x4F490 |
| 3 | `0x8317F9D0` | 0x38 | 28 + 0x4F490 + 0x5EE90 |

Le dernier mot de l'en-tête est la **somme simple des octets** des trois blocs (0xAE358 octets ;
vérifié sur la sauvegarde de l'utilisateur). Il faut la recalculer après chaque modification. Le reste
du fichier (au-delà de 28 + 0xAE358) n'entre pas dans cette somme.

**Champions** : ils ne sont pas dans le bloc Univers. Il y a deux tables de 23 titres × 2 places
(u16 = n° de catcheur dans le jeu ; **512 = personne, donc ceinture vacante**) :

- hors Univers : bloc 2 + 0x57604 (`0x832264AC`) ;
- **Univers** : bloc 2 + 0x57660 (`0x83226508`), utilisée quand `*0x8324E51C == 1` (drapeau « en
  mode Univers », `sub_82A39ED8` l'écrit et `sub_82A39EE8` le lit).
- Lire : `sub_82A39FC8(titre, place, …)`. **Écrire : `sub_82A3A060(titre, catcheur, place)`** (la
  fonction du jeu elle-même). Remettre les champions par défaut : `sub_82A39E08(titre)` (un titre) ou
  `sub_82A39D98` (tous), à partir d'une table de champions par défaut à `0x8324E3A8` (fiches de
  16 octets).
- Dans la sauvegarde de l'utilisateur, les deux tables sont presque identiques : 1 = Del Rio,
  2 = Orton, 5 = Ezekiel Jackson, 6 = Ziggler, 8 = Layla, 12 = Kelly Kelly, 13 = Otunga & McGillicutty,
  et aussi Hawk & Animal, Ax & Smash, Undertaker, Triple H… (titres des légendes). Le nom de chaque
  numéro de titre reste à confirmer en jeu.
- **Aspirants n° 1** : même format dans le bloc Univers à U + 0xC750 (`sub_826D5088`).
- Points de classement (`sub_826CE180`) : 1000 pour le champion, 999 pour l'aspirant n° 1, sinon la
  fiche Univers du catcheur.

**Numéros de catcheurs** : le jeu utilise des numéros 0–372 (0–49 = catcheurs créés). La table
`0x8307B5E0` (u16[373]) donne la fiche de sauvegarde (base + n × 286, nom court à +178).

**Fiches Univers** : une fiche de **112 octets par catcheur** au début du bloc Univers
(U + index × 112, index = `sub_826D3320(n°)`), environ 180 fiches jusqu'à U + 20160. Viennent ensuite
d'autres données, puis les cartes (U + 27360…). Le bloc Univers s'arrête à la fin du bloc 1
(U + 0x100FC).

**Qui est suivi** : catcheurs créés (n° 0–49, fiche = n°) et **80 superstars** dont la liste est à
`0x83073BA0` (u16[80], fiche = 100 + position) ; les autres ne sont pas suivis (« This Superstar is not
tracked in Universe Mode »).

**Contenu d'une fiche de 112 octets** (lue dans la sauvegarde de l'utilisateur) :

| Octets | Contenu |
|---|---|
| +0 à +45 | 23 × u16 : **points cette semaine** pour chaque titre (0 = pas dans ce classement) |
| +46 à +91 | 23 × u16 : **points la semaine dernière** (écran THIS WEEK / LAST WEEK) |
| +92, +96 | 2 × u32, à 0 partout (même pour The Miz qui a combattu) |
| +100 | u16, 512 = personne (partenaire / allié ? à vérifier) |
| +102 à +111 | octets : +102 variation (ex. −5), +103 = 13 par défaut, +104… |

→ **Le jeu ne garde ni victoires ni défaites** : le « suivi des stats » façon WWE '13 devra être noté
par notre propre code (après chaque match) dans un fichier à nous.

**Vider l'Univers : `sub_826D5938(U)`** (sûr). C'est l'initialisation du bloc Univers, appelée au
démarrage (`sub_82F6DC70`) et à la création de la structure de sauvegarde (`sub_8258FAF0`). Elle remet
les 180 fiches à zéro (points 0, +100 = 512, +103 = 13), la zone U + 20160 à 512, les cartes
(`sub_826D5228` sur U + 27360 + k × 984, 10 + 13 émissions), les aspirants et d'autres listes à
512/0, et garde seulement les bits 0x60 de l'octet U + 0xDE07 (réglages ?), U + 0xDE08 = 50.
**Hypothèse forte** : un Univers dans cet état est recréé par le jeu au prochain passage dans le mode
Univers (texte « BEING CREATED... »). À confirmer en jeu. La remise à zéro = appeler cette fonction,
remettre les champions Univers par défaut (table `0x8324E3A8`), puis laisser le jeu enregistrer.

---

## 4. Le code : qui fait quoi

Le module Univers occupe **0x826BA000 à ~0x826F5000** (ses textes de débogage sont rangés dans le même
ordre, de 0x8203D598 à 0x8203EB00). Yuke's y a laissé des `printf` de débogage, tous imprimés par
**`sub_826D4228`** (que `wwe12_game_log` recopie dans le journal).

### 4.1 Les grandes fonctions repérées

| Fonction | Rôle | Indice | Certitude |
|---|---|---|---|
| `sub_826C8690` (2,3 Ko) | **Avancer le calendrier / mettre à jour l'Univers** : appelle la génération des cartes, le Rumble, etc. | appelée par `826C9050`, `826C9128`, `826C95E0` | probable |
| `sub_826E3FF8` → `sub_826E3920` | **Génère la carte d'une émission** (match par match) | appelée par 826C8690 | probable |
| `sub_826E3310` | calcule **rang et « rate »** de chaque participant (`rank = %2d, rate = %f`) | appelée par 826E3920 | sûr (texte) |
| `sub_826E3738` | imprime la carte : `n: NOM: pts[rang], pts[rang], pts[rang]` | appelée par 826E3920 | sûr (texte) |
| `sub_826D4958(this, sortie, i)` | lit le **i-ème participant** d'un match (id sur 16 bits, 512 = vide) | | probable |
| `sub_826C9018` | **mise à jour des scénarios** : `826C83D8` (démarrer) puis `826C6328` (annuler) | appelée depuis les menus 0x82546700, 0x8255EC88 | probable |
| `sub_826E9338` | **choisit un scénario** parmi les 136 (`StoryTemplate,None` si aucun) | appelée par 826C83D8 | sûr (texte) |
| `sub_826E8228` | liste les scénarios possibles (`StoryTemplateList`, `StoryNo,Trigger,CastA…F`) | | sûr |
| `sub_826E8010` | **démarre** un scénario (`Story Start,StoryNo=%3d`) | | sûr |
| `sub_826E8F60` | **annule** un scénario (`Story Cancel,[%d]`) | | sûr |
| `sub_826E86D0` / `sub_826E7630` | distribue les **rôles** du scénario (`StoryCast Error`) | | probable |
| `sub_826E9ED8` / `sub_826E9C40` | **déroulé d'un scénario** : saut d'acte en acte (`JumpActWarp`) | | probable |
| `sub_826DF6D8` (3 Ko) | **lance une cinématique** au bon moment (`Timing[%d][%2d]:CutScene Start`) | | sûr (texte) |
| `sub_826DF400` → `sub_826DDFF0` → `sub_826ECE78` | choisit **la scène** : `Act=%4d,Sub=%2d,Track%2d` (Act = n° de scène sur 4 chiffres, voir §2.3) | | probable |
| `sub_826C3258` → `sub_826C3090` | **après un match** : Draft (`826C0910`), changements d'attributs / momentum (`826BF2D8`), etc. | appelée par 0x828AD3A0 (fin de match) | probable |
| `sub_826C0910` | **Draft** (`Draft Event Success` / `Cancel`) | | sûr |
| `sub_826CD010(jour)` | « y a-t-il un Draft ce jour-là ? » | | sûr (sonde) |
| `sub_826F1028` | **Money in the Bank** : nombre d'entrants par marque (`MIB Entry:RAW = %d, Smack = %d`) | | sûr (texte) |
| `sub_826CCA20`, `sub_826D22C8`, `sub_826C8520` | **Royal Rumble** : enregistre le résultat (`SetRoyalRumbleResult() Failed`) | 826C8520 appelée par les écrans de fin de match | sûr (texte) |
| `sub_826BE3C0` | réglage « RP » par catcheur (`RP Setup,Status,RosterID,No`, `RP Setup,Disable`) : RP = probablement les points de **relation/rivalité** | | à vérifier |
| `sub_826BF2D8` | `Change Attribute.`, `%s,Momentum,%d,OverAll,%d,%d` (momentum, note globale) | | sûr |
| `sub_826C8120` | retire des matchs des CAW désactivés (`Include disable CAS`, `Match No = %d.Delete`) | | sûr |
| `sub_826C93A0` / `sub_826C98B0` | **intervention** d'un joueur dans un match IA (`PlayableIF…`) | | probable |
| `sub_826BAC88` | **commentateur invité** qui s'en va (`Guest Commentator Get Out`) | | sûr |
| `sub_826EC280`, `826EC4C8`, `826EC678`, `826ED578`, `826EC0A8` | **chargent** MTDT, STYT, WCID, BLTS et `BATL/SAVE` | méthodes virtuelles | sûr |
| `sub_826D4228` | imprime les messages de débogage du module | 46 appels | sûr |

Une table de mots-clés à 0x83073C40 (`C, Win, Los, Ally, Enem, ID, Play, RTop, RTag, RMid, RDiv, STop,
STag, SMid, SDiv, RRW, Btm, P)A, P)E, P)SF, P)DF, M`) sert sans doute à **décrire les rôles** des
scénarios et des scènes : gagnant/perdant, allié/ennemi, joueur, haut de carte/équipe/milieu/Divas de
RAW (R…) ou de SmackDown (S…), Royal Rumble… (**probable**).

### 4.2 Le déroulement, tel qu'on le voit dans `univers.log`

```
Entrée dans l'Univers
  └─ scénarios : StoryTemplate,None (aucun scénario possible ce jour-là)
  └─ génération d'UN MOIS DE CARTES d'un coup (≈ 3 s) :
        pour chaque jour d'émission (0, 3, 4, 7, 10, 11, … 27) :
            test « Draft ce jour-là ? » (sub_826CD010)
            pour chaque match : rang + rate de chaque participant, puis la carte
              « 1: JOHN CENA: 901[ 2], 1[26], 400[-1] »
Après chaque match joué ou simulé
  └─ résultat (Rumble, MITB, titres), Draft, attributs/momentum, relations
  └─ actualités (NEWS) d'après les modèles de string.pac
Avant/après un match
  └─ cinématique choisie par le scénario en cours (Act/Sub/Track) → scène des archives evt
```

Chaque ligne de carte donne 3 couples **points[rang]** (rang −1 = non classé). Ce sont probablement les
classements de division (solo, équipe…). Les catcheurs à `0[0]` partout sont ceux d'une émission
secondaire (ex. Superstars) ou des CAW. **À vérifier.**

Remarque : le « rate » s'affiche `?` dans le journal (nombre à virgule mal recopié par la sonde) ; à
corriger quand on reprendra la sonde.

---

## 5. Ce qu'on ne sait pas encore (à faire avant de modifier)

→ partie déplacée dans `docs/archives/univers_anciennes_parties.md` (2026-11-06).

## 6. Ce que ça veut dire pour la suite (étapes 1 à 4)

→ partie déplacée dans `docs/archives/univers_anciennes_parties.md` (2026-11-06).

## 7. Ce que fait WWE '13 (étudié le 2026-10-18, pour s'en inspirer) (archivé)

Déplacé le 2026-11-12 dans `docs/archives/univers_anciennes_parties_2026-11-12.md`.

## 8. Tests en jeu (2026-10-18, sauvegarde de l'utilisateur mise de côté puis remise)

→ partie déplacée dans `docs/archives/univers_anciennes_parties.md` (2026-11-06).

## 9. Comment le jeu crée l'Univers (test « sauvegarde neuve », 2026-10-20) (archivé)

Déplacé le 2026-11-12 dans `docs/archives/univers_anciennes_parties_2026-11-12.md`.

## 10. Remise à zéro : ça marche (2026-10-20) (archivé)

Déplacé le 2026-11-12 dans `docs/archives/univers_anciennes_parties_2026-11-12.md`.

## 11. EDIT UNIVERSE > RESET UNIVERSE dans le jeu (2026-10-20, testé en jeu)

`wwe12_universe.cpp`, cvar `wwe12_universe_edit` (off par défaut pour l'instant ;
`tools/dev/lancer_test_univers.bat` l'allume). Dans le menu de l'Univers, EDIT SUPERSTAR s'appelle
EDIT UNIVERSE (FR « UNIVERS », DE « WELT », ES « UNIVERSO », IT « UNIVERSE »). Le choisir ouvre la boîte
Oui/Non du jeu « Reset the WWE Universe? Choose No to edit your Superstars. » (curseur sur NON) :
NON → l'éditeur des Superstars, comme avant ; OUI → seconde confirmation (« All WWE Universe
progress will be lost… ») ; OUI → remise à zéro + message « The WWE Universe has been reset. Leave
WWE Universe and come back to see it. ». Vérifié : après être ressorti et rentré, cartes neuves.

Comment le menu a été trouvé (sondes retirées depuis) :
- le menu Univers est un écran « à calques » (vtable 0x82021F98), pas un menu de menu.pac ; tous
  les écrans de l'Univers sont construits d'un coup à l'entrée (`sub_8255EC88`) ;
- le son « Decision » joué à la validation a donné la chaîne : `sub_824D1FB0(écran)` = bouton A des
  menus à calques → vtbl[18] = `sub_825453D8` (action de l'écran Univers : lance la transition,
  +268 = 1) ; l'entrée choisie est le mot +28 (0 MATCH CARDS … 4 EDIT SUPERSTAR, 5 EDIT TEAM) ;
  vtbl[1] = `sub_824D3458`, mise à jour de chaque image (partagée par tous les menus à calques) ;
- boîtes du jeu (objet *0x8324DEF0) : `sub_8254E7E8(id)` question Oui/Non, `sub_8254E680(id)`
  information, `sub_8254E8C8(&réponse)` ≠ 0 une fois fermée (réponse 1 = oui) ;
- textes : `sub_821A4400(this, id)` ; nos questions utilisent les numéros 27026–27028, vides dans
  WWE '12.

Demandes de la Main (faites et testées en jeu le 2026-10-20) : avant le OUI final, copie
automatique de la sauvegarde au format du launcher (`Documents\wwe12\SaveBackups\<date>\`,
`backup.txt` = « auto » / « Before RESET UNIVERSE », 10 copies automatiques gardées ; si la copie
échoue, rien n'est remis à zéro) ; les messages disent qu'une copie est faite et que la remise à
zéro sera enregistrée à la prochaine sauvegarde du jeu (après la prochaine émission).

À faire : déclencher la sauvegarde du jeu juste après la remise à zéro (amélioration) ; vérifier
l'affichage DE/ES/IT (le français est vérifié à l'écran : accents, boîtes qui s'agrandissent) ;
fait aussi (demande de l'utilisateur, pour que les joueurs trouvent l'option) : nom d'entrée plus
clair (FR « MODIFIER L'UNIVERS », DE « WELT BEARBEITEN », ES « EDITAR UNIVERSO », IT « MODIFICA
UNIVERSE ») et message « Nouveau » affiché une seule fois au premier passage dans le menu de
l'Univers (texte 27029, fichier `Documents\wwe12\universe_reset_hint.txt`), vérifiés en jeu en FR ; activer `wwe12_universe_edit` par défaut quand
l'utilisateur valide ; ensuite VACANT.

**2026-10-20 : `wwe12_universe_edit` est activé par défaut** (validé par l'utilisateur). `false` dans
wwe12.toml le coupe. Reste : forcer la sauvegarde du jeu juste après la remise à zéro (amélioration),
puis VACANT pour les titres qu'aucune émission n'utilise.

## 12. VACANT titre par titre, comme SvR 2011 (2026-10-20, testé en jeu en FR)

Fiche d'un Superstar (MODIFIER L'UNIVERS → Non → liste → Superstar) → case du titre → grille des
ceintures (écran vtable 0x82029BB8, action `sub_825B47B0`). En mode Univers, revalider **la
ceinture que le Superstar détient déjà** (le jeu d'origine ne fait alors rien), pour un titre
**qu'aucune émission n'utilise**, demande « Rendre ce titre vacant ? Aucune émission ne l'utilise :
il restera vacant jusqu'à ce que vous le donniez à un Superstar. » ; Oui → les deux places du titre
passent à 512 (`sub_82A3A060`) et « Le titre est maintenant vacant. ». Pas de copie de sauvegarde :
c'est réversible (redonner la ceinture dans la même grille).
- Grille : grid = r3, owner = grid − 28 ; case choisie = owner + (118 + grid[+640]) × 4 (nombre de
  cases owner+656), une case = {titre, place} ; Superstar édité = grid+256 ; détenteur =
  `sub_82A39FC8(titre, place, 1)` ; titre par équipe = `sub_82A39CE8`.
- Titres d'une émission : fiche d'émission U + 27360 + n × 984 (n = 0–9), à +904 un octet par titre
  (0 = pas utilisé, 1–4 = MAJOR / MINOR / TAG / DIVA), lu par `sub_826CC180`.
- Testé : Del Rio (WWE, utilisé par RAW) → pas de question ; Big Show (Hardcore, titre 8,
  inutilisé) → question, OUI → vacant.
- Attention : le curseur de la grille démarre toujours sur la 1re ceinture (WWE), pas sur celle du
  Superstar ; valider une autre ceinture la donne au Superstar (comportement du jeu d'origine).
- Les numéros de titres lus dans `SaveData.Dat` aux §3.3 et §8 sont décalés d'un cran par rapport
  au jeu (le jeu dit titre 8 = Hardcore de Big Show, la lecture du fichier disait 9) : se fier aux
  adresses mémoire du code, pas à ces lectures de fichier.
- Sonde de développement gardée (off) : cvar `wwe12_universe_probe` (écran, vtable, action et mots
  à chaque bouton A des menus à calques + chaîne d'appels des sons Decision/Cancel).
- **Titres d'une émission modifiables en jeu** (testé le 2026-10-20) : CALENDRIER → X sur une émission
  → MODIFIER LE SHOW → CHAMPIONSHIPS (MAJEUR / MINEUR / TAG TEAM / DIVA) → ACCEPTER → SAUVEGARDER.
  Le VACANT suit ces choix (il relit les titres des émissions à chaque fois) : avec le Hardcore en
  MAJEUR de RAW, la ceinture WWE de Del Rio devient « vacantable » (testé : titre 0 libéré, la
  ceinture disparaît de sa fiche).
- **SAUVEGARDER de MODIFIER LE SHOW écrit tout de suite sur le disque** (« SAUVEGARDE… ») et débloque
  le succès n° 46 (« On est à l'antenne ! »). Piste pour enregistrer la remise à zéro tout de suite :
  trouver l'appel d'enregistrement de cet écran et le refaire après le reset.

## 13. MODIFIER L'UNIVERS façon WWE '13, avec les vrais menus du jeu (2026-10-20, testé en jeu)

Demande de l'utilisateur : pas d'écran dessiné par-dessus (comme OPTIONS > GRAPHICS), mais les vrais
éléments du jeu. EDIT UNIVERSE (FR « MODIFIER L'UNIVERS ») ouvre maintenant la **boîte de choix du jeu**
« EDIT UNIVERSE / What do you want to edit? » avec trois lignes :

- **EDIT SUPERSTARS** : l'éditeur des Superstars, comme avant ;
- **EDIT TITLES** : le **vrai écran TITLE MANAGEMENT** d'OPTIONS > TITLE SETTINGS, ouvert depuis
  l'Univers, donc sur **les champions de l'Univers** (Del Rio, pas Cena) ; X = VACATE TITLE ; en
  sortant, « Would you like to save these settings? » (YES enregistre tout de suite sur le disque) ;
- **RESET UNIVERSE** : la confirmation et la remise à zéro de la section 11.

**Boîte de choix** : la boîte du jeu (*0x8324DEF0) a un mode liste (demande de type 4 : texte, +24 =
nombre de choix, +28 = tableau de pointeurs de textes), ouvert par `sub_8260DD80(-, texte, choix, nombre,
0, 0)` (le jeu s'en sert pour « Do Not Review / Remind Me Later ») ; la réponse (`sub_8254E8C8`) est la
ligne choisie, à partir de 0. Pas de retour avec B. Le calque de la boîte n'a que **deux lignes de choix** :
éléments de 240 octets (calque = boîte+1952, tableau à +8, nombre à +24) ; 0xD2/0xD3 = textes (police
font001, y −8/+8), 0xD6/0xD7 = traits rouges (`red_line01`) ; un 3e choix va dans 0xD4 (police font000,
échelle 0,7, posé sur la 1re ligne) et son « trait » est 0xD8 (= le YES des boîtes Oui/Non). Champs : +4 x,
+8 y, +12/+16 échelle, +80 police, +176 parent, +180 caché, +184..+196 couleur RGBA, +200 texte.
Le curseur (`sub_8288F0A0`) rougit le texte 0xD2+i et montre le trait 0xD6+i. Ce que fait le module tant
que le sous-menu est ouvert : 0xD4 devient une copie de 0xD3 un cran plus bas, les trois lignes et leurs
traits remontent de 3/4 de cran (sinon la 3e touche le bas de la boîte), et quand le curseur est sur la
3e ligne le trait 0xD7 descend sous elle (un trait d'un autre groupe, comme 0xDC, reste caché par le jeu) ;
c'est refait après `sub_8288F0A0`, qui passe après le masquage des éléments inutilisés. Les éléments
(0xD2, 0xD3, 0xD4, 0xD6, 0xD7 et 0xD8) sont remis en état à la fermeture (sinon le YES de la boîte
suivante restait pâle). Relevé fait avec `wwe12_universe_probe` (boîte telle que le jeu la dessine,
éléments 0xC8–0xE8 dans le journal à chaque mouvement du curseur).

**TITLE MANAGEMENT depuis l'Univers** : la scène du menu principal (`sub_825477E8`) crée l'objet
(2 560 octets, `sub_821A0000`) avec `sub_82564548(objet, manette)` ; il s'inscrit seul comme écran à
calques (mis à jour et dessiné tout seul) ; fini quand objet+228+16 vaut 5, détruit par
`sub_82565A30(objet, 1)`. Ses lignes (24 octets à objet+376 : détenteur, titre, place…) viennent de
`sub_82A39FC8(titre, place, 1)`, qui lit la table de l'Univers dans l'Univers. Pendant qu'il est ouvert,
le menu de l'Univers ne prend pas les touches (son mot +224 = 0 ; 1 les rend à l'image suivante). En
sortant, la barre des boutons reprend le jeu 9 du menu de l'Univers (`sub_828942C0(9)`, « A SELECT,
B BACK ») ; sinon « X VACATE TITLE » restait affiché. Le X (`sub_82565390`, case = [+28] + [+2520],
titre à +380 + case × 24) garde notre limite dans l'Univers : un titre qu'une émission utilise est
refusé avec un message (texte 27032). Testé : WWE (RAW) refusé, Cruiserweight libéré, NO = rien
d'enregistré.

Textes ajoutés : 27032 (refus du X) et 27033–27035 (les trois lignes), en cinq langues.

## 14. Idées suivantes (suggestions de joueurs, 2026-10-20)

→ partie déplacée dans `docs/archives/univers_anciennes_parties.md` (2026-11-06).

## 15. Tenue de chaque Superstar dans l'Univers (2026-10-21, testé en jeu)

Comme dans WWE 2K14, mais avec les vrais menus du jeu : sur la fiche d'un Superstar dans l'Univers,
**Y** (triangle sur PlayStation) ouvre la boîte de choix du jeu « UNIVERSE ATTIRE / Current: … »
avec ORIGINAL, ses tenues débloquées (noms du jeu), ses tenues **Superstar Threads** (nom donné par le
joueur) et RANDOM ; au-delà de 3 lignes, « MORE... » mène à la page suivante. La barre du bas affiche
« … X DEFAULT  Y ATTIRE ». Choix gardés dans `Documents\wwe12\universe_attires.txt` (« id tenue
emplacement Threads », -1 -1 = aléatoire), pas dans la sauvegarde. Testé : Kelly Kelly en tenue Threads
« BLANC » à l'entrée d'un match de la carte.

- Un match de l'Univers repart toujours en tenue 0 : `sub_826BE900(this, M, match, 1)` remplit l'objet
  de sélection M (*0x8324DEFC) depuis la carte, et `sub_828A9E50` remet la tenue à 0. Places : M +
  [M+15960] × 1596 + 124 + i × 244 (+0 id, +16 tenue, +20 emplacement Threads ou -1, +44 manette). Après
  ce chargement, le crochet applique le choix comme l'écran des participants (`sub_825AE158`) :
  `sub_828AA050(M, place, tenue)` et `sub_828AA070(M, place, emplacement)`. Le joueur peut encore
  changer dans MODIFIER LES PARTICIPANTS.
- Liste des tenues comme l'écran des participants (`sub_825ADD48`) : ORIGINAL (texte 6571) ; table T =
  *0x8324E524 ([T+3200] fiches de 40 octets {id, nombre, 4 × {nom, déblocage}} ; `sub_82A41D50` nombre,
  `sub_82A41C70` fiche de la tenue k, `sub_82A3FE18` débloquée) ; Superstar Threads : 3 fiches de 108
  octets à `sub_82685CD8(id)` + 744 (+0 tenue de départ ou -1, +40 nom). Catcheurs créés (id < 50) : pas
  gérés (leurs tenues sont ailleurs).
- L'octet +4 d'une place de la carte enregistrée (§14) est la **manette** (0xFF = CPU), pas la tenue.
- Fiche du Superstar : écran vtable 0x82028C88 (propriétaire 0x82028D10), Superstar = `sub_82DEA3A8()`.
  Boutons des écrans à calques : table à 0x8201ADF0 {masque, fonction} ; A = 0x10, B = 0x20, **Y = 0x40**
  (`sub_824D1AF0` : emplacement 21 « permis ? » puis 8 « action »), **X = 0x80** (`sub_824D1B68`,
  emplacements 22 / 9 ; X = DEFAULT sur la fiche).
- Barre des boutons : données *0x8324DF18, fiches de 20 octets à [+48] (nombre [+52]) : trois places
  {bouton, libellé} en u16 + l'id du jeu à +12 ; la fiche affiche le jeu 505 ({45000 SELECT}, {45001
  BACK}, {45003 DEFAULT}), sans place libre. Le libellé DEFAULT (45087) devient le nôtre, « DEFAULT
  [U+E007] ATTIRE » : les icônes de boutons (caractères privés U+E0xx) peuvent être dans un texte.
- Calque des écrans de l'Univers : `sub_8254E920()` (UMSI de MnLayHD.pac, YXMLBIN) ; relevé possible
  avec `wwe12_universe_probe` (`Documents\wwe12\logs\umsi_layout.txt`). Une vraie ligne ATTIRE comme
  2K14 a été écartée : pas d'élément de rechange pour le fond et le cadre d'une ligne.
- B / rond ferme les deux boîtes de choix (MODIFIER L'UNIVERS et tenues) sans rien changer : demande
  +12 = 1 (`sub_8260DD80(…, 1, 0)`), comme la boîte du jeu de `sub_82623608`. Fermée par B, la boîte
  renvoie quand même la ligne sous le curseur : c'est son mot +20 qui les distingue (1 après A,
  `sub_824D1A20` ; 0 après B, `sub_824D1A50`). Testé en jeu le 2026-10-21.

## 16. Remise à zéro enregistrée tout de suite (2026-10-21, testé en jeu)

Après le OUI final (copie automatique puis remise à zéro), on lance la sauvegarde du jeu comme le fait
TITLE MANAGEMENT quand on le quitte avec des changements (`sub_82564DD0`) : si le profil peut
enregistrer (`sub_82A27208(sub_8267CD10())`) et que la sauvegarde existe (`[sub_82684BE8()]` ≠ 0),
on crée l'objet « enregistreur » du jeu (33 720 octets, `sub_828955A0`) dans le tas du jeu
(`sub_827D1C68/1F18/28D0/2950/1F98`), on met à 1 le drapeau +0x576BC du bloc 2, puis
`sub_825391F0(enregistreur)` (ou `sub_82539130(enregistreur, 0)` si l'octet +0x57667 du bloc 2 vaut 0).
Il affiche ses propres messages (« SAVING... ») ; quand son mot +32 devient non nul, on le détruit
(`sub_82895740(enregistreur, 1)`) et on montre le texte 27040 (« remise à zéro et enregistrée »).
Sans profil ou sans sauvegarde : ancien message (enregistré à la prochaine sauvegarde du jeu).
Journal : `[univers] save: started` / `save: done`. Testé : la sauvegarde sur le disque change tout
de suite.

## 17. SAVE sur un match modifié de la carte, comme WWE '13 (2026-10-21, testé en jeu)

EDIT sur un match de la carte ouvre l'écran du match (2 976 octets, `sub_82575688(drapeaux, curseur)`,
état 2 de la machine des cartes `sub_8254A778` = `sub_82548F80`). Son menu (`sub_82574BD8`) a une ligne
par bit des drapeaux : 1 PLAY (texte 284), 2 MATCH CREATOR (286), 4 SELECT ARENA (282), 8 CHANGE
PARTICIPANTS (514), 16 SIMULATE (487). Ligne j : écran+276+4j = son bit (le résultat, lu au curseur
écran+72 par `sub_82571F88`), écran+296+4j = active, `[écran+2964]`+20+4j = son texte. Dans l'Univers,
4 et 8 ne sont jamais tous les deux là : le libre devient **SAVE** (texte 238 du jeu, déjà traduit) avec
notre résultat 32. Vu : drapeaux 0x17, SAVE en 4e ligne, avant SIMULATE.

Avec SAVE : le match est écrit dans la carte comme SIMULATE le fait avant de simuler
(`sub_826C63D8(-, match, M)` : l'objet de sélection M → la fiche du match ; n° du match = octet +32 du
contexte `*0x83231548`, ou `sub_826BAF28` si son +20 = 1), puis le A de l'écran devient un B
(`[écran+20]` = 0) : le jeu revient à la carte par son propre chemin, rien n'est joué. La fermeture
fait comme SELECT ARENA (pas `sub_82893B30(0)`, réservé à PLAY). Le jeu a enregistré la sauvegarde sur
le disque juste après (4 s) de lui-même. Journal : `[univers] match edit: SAVE, card match N written`.

## 18. Classement complet pour tous les titres (2026-10-21, testé en jeu)

Avant : seul le premier titre d'une émission (WWE, World) avait sa liste complète (5, 6, 7…) ; les
titres mineurs, par équipes et Divas s'arrêtaient au champion + 4 challengers. Le jeu calcule pourtant
le classement de chaque titre (`sub_825530A0` → `sub_826D2508`, jusqu'à 180 : `sub_826D0000` en solo,
`sub_826D18E8` en équipes ; filtre = émissions qui portent le titre + sexe via `sub_82A39EF8`). C'est
la mise en place de l'écran (`sub_825535E0`) qui coupe le défilement : à l'entrée, +56 = place du titre
dans l'émission (0 majeur, 1 mineur, 2 équipes, 3 Divas), puis +56..+68 = {page, pages, 0, défilement},
pages = 2 + (n − 6) / 12 si n > 5, et défilement = 0 avec les flèches 900/901 cachées si la place ≠ 0.
Crochet : dans l'Univers, défilement = 1 et flèche du bas (901) montrée s'il y a plus d'une page.
Vu : US 29 classés (3 pages), équipes 6, Divas 13. Rien n'est écrit dans la sauvegarde.

## 19. Classements changés à la main, comme WWE '13 (2026-10-21, testé en jeu)

Sur un classement de titre en solo (majeur, mineur, Divas), **X EDIT** (ajouté à la barre du bas, set
515 = {B BACK} + notre {45003 X, 45409 EDIT}) lance la modification : un curseur rouge (le rouge des
menus du jeu, 0.80/0.08/0.08) sur un Superstar, haut/bas pour le déplacer (les pages suivent), **A** le
prend (doré) et haut/bas le monte ou le descend d'une place, A le repose, B ou X termine. Le champion ne
bouge pas.

- Ordre affiché = points de la semaine, du plus haut au plus bas (tri `sub_82552A00` sur +24, à
  égalité par numéro) ; `sub_826CE180` donne 1000 au champion, 999 à l'aspirant n° 1 (table U+0xC750,
  écrite par `sub_826D5118(table, titre, Superstar, 0)`), sinon les points de la fiche Univers
  (U + `sub_826D3320(id)` × 112 + titre × 2, u16).
- Un déplacement réécrit l'aspirant n° 1 et ces points, strictement décroissants dans le nouvel ordre
  (mêmes valeurs, montées au besoin, 998 au plus), puis `sub_825530A0(R)` refait la liste. Copie
  automatique de la sauvegarde avant le premier changement (« Before RANKINGS EDIT »). Le jeu enregistre
  ensuite à sa prochaine sauvegarde.
- Écran : S (vtable 0x82023240), R = S − 28. Touches de l'image en S+64 (16 A, 32 B, 128 X ; haut/bas
  selon la direction de la liste S+20 et la table 0x8201AD80 : ici 3 → 0x10000 / 0x40000), lues par
  `sub_824D20A0` ; pendant la modification elles sont à nous. Page = S+28 ; changer de page = S+28 puis
  `sub_823D35B8(S, 1|2)` et `sub_82552CE8(S)` (dessin des lignes, aussi après chaque échange).
- Noms surlignés par numéro d'élément : page 0, #1..#4 = 948..951 ; pages de liste, lignes 1020..1031
  (chercher par texte trouvait parfois le même nom sur l'écran des titres, caché).
- Pas pour les titres par équipes (la liste classe des équipes).

## 20. Émissions de moins de 15 catcheurs (2026-10-27, testé en jeu sur une copie)

Réglage `wwe12_universe_min_roster` (wwe12.toml, **15 par défaut = jeu d'origine**, de 1 à 15). Le jeu
teste « 15 » en dur à trois endroits ; chacun a son crochet :

- `sub_826D3B58(id)` « peut quitter son émission » (transfert vers une autre marque, retour par défaut :
  textes 51274 / 51275 ; aussi `sub_8258A088`) : effectif `sub_826D0598(émission, 0, 0, 1)` > 15. Le crochet
  de `sub_826D0598` ajoute 15 − minimum, seulement pour l'appel fait là (adresse de retour 0x826D3C84).
- `sub_82599DB0`, contrat (texte 51411) : compte les catcheurs de l'émission avec `sub_826CC0B8` sur les 373,
  il en faut 15. Pendant cet appel (adresse de retour 0x8259A028), jusqu'à 15 − minimum réponses « non »
  deviennent « oui ».
- `sub_82583B78`, marques choisies dans EDIT BRAND (texte 51413) : compte les catcheurs (hommes) des marques
  cochées (S + (marque + 66) × 4 = 1) ; sous 15, S+428 = {0, 51413, 1}. Après un refus, notre propre compte ;
  s'il atteint le minimum, le refus est défait et on fait la fin « acceptée » du jeu (`sub_826CC0E0` pour les
  13 émissions, drapeau de fin [S+444] = 1, `sub_82550538(S − 28)`).

Copie automatique de la sauvegarde la première fois qu'un effectif sous 15 passe (« Before a show under 15
Superstars »). Le jeu prévient lui-même (texte 51303) que sous le minimum les matchs risquent d'être mal
formés. **Test** (lanceur `tools/dev/lancer_test_effectif.bat`, copie dans `Documents\wwe12_test_effectif`,
minimum 8) : SmackDown à 8 catcheurs ; la carte se forme (2 matchs, un match d'aspirant n° 1 et un main event,
parfois « ??? » = adversaire mystère du jeu), toujours 2 matchs trois semaines plus tard, aucun plantage.
Le contrat (51411) n'a pas été essayé à part.

Suite du test (2026-10-27) : avec SmackDown à 8, le PPV Royal Rumble passe. **Une émission vide (0 catcheur)
est refusée** (minimum réglable de 1 à 15) : essayée avec un minimum de 0, le jeu accepte SmackDown sans
marque (carte vide, X ADVANCE pour la passer), mais **plante au Royal Rumble** en fabriquant la carte du PPV
(`sub_826F0290`, appel de `sub_826F0228`, écriture hors limites). D'où l'abandon de « un seul show majeur »
(décision de l'utilisateur) : il faudrait réécrire le booking des PPV.

## 21. Idées venues de WWE 2K14 (captures de l'utilisateur, 2026-10-27) : ce qu'on sait

→ partie déplacée dans `docs/archives/univers_anciennes_parties.md` (2026-11-06).

## 22. Fiche du Superstar façon 2K14 : ce qui est fait (2026-10-27)

Dans 2K14, la fiche a 4 pages (LB/RB) : OPTIONS, PARAMÈTRES ASPIRANT, RELATIONS, STATISTIQUES.

- **RELATIONS** : la fiche de WWE '12 montre déjà ALLIES et RIVALS (et le MOMENTUM) : rien à ajouter.
- **PARAMÈTRES ASPIRANT** et **STATISTIQUES** : dans la boîte Y de la fiche (vraie boîte du jeu), lignes
  « STATISTICS... » et « CONTENDER SETTINGS... » (§21), après les tenues.
- **Statistiques** : chaque changement de tenant d'un titre de l'Univers (`sub_82A3A060`, résultats de match,
  TITLE MANAGEMENT…) est écrit dans `universe_stats.txt` (à côté du dossier de sauvegarde) : « title <titre>
  <place> <nouveau> <ancien> <jour> ». Le jour = compteur de l'Univers **U + 0xDDC4** (mois = jour / 28 % 12,
  `sub_826D2890` ; semaine = jour / 7, `sub_826D2860`). La boîte « TITLE HISTORY » donne, par titre, le
  nombre de règnes et les semaines détenues (nom du titre : `sub_826D2C30(titre)`). Testé sur la copie :
  WWE Championship donné à Alex Riley dans TITLE MANAGEMENT → « WWE Championship: 1 reign(s), 0 week(s) ».
  Une remise à zéro de l'Univers met le fichier de côté (`universe_stats_before_reset.txt`). Pas encore
  notés : Royal Rumble, Money in the Bank, WrestleMania (§21 pour les pistes).
- **Pages LB/RB** : RB arrive bien sur la fiche (masque 0x200, la fiche n'en fait rien), mais montrer
  l'écran d'accueil de RANKINGS depuis la fiche n'a pas marché : la fiche et RANKINGS partagent un grand
  affichage (celui de l'objet RANKINGS, R+296 ; R = menu Univers +264, menu = r3 de `sub_82545A70`, qui crée
  aussi +260 MATCH CARDS, +268, +272, +276, +280…), où seuls les éléments 3/5 (et 826/829) changent de
  « caché », et dans l'affichage du cadre (`sub_8254E920`) les éléments 8/9, 40/46, 59/72 ; basculer ces
  drapeaux ne change rien à l'image : le jeu passe d'un écran à l'autre autrement (à trouver : l'écran actif
  du menu Univers). Sonde : `wwe12_universe_probe` + LB/RB → `logs/umsi_*.txt`, `logs/rlay_*.txt`.
- **Blessures** : un réglage pour les couper (`wwe12_universe_injuries`, refus de `sub_826CEA88(vue, id, 1)`)
  a été codé puis **retiré à la demande de l'utilisateur** (2026-10-27) : les blessures restent comme dans le jeu.

## 23. Pages ASPIRANT et STATISTIQUES sur la fiche, avec RB / LB (2026-10-27, testé sur la copie)

→ partie déplacée dans `docs/archives/univers_anciennes_parties.md` (2026-11-06).

## 24. Page STATISTIQUES en tableau, comme 2K14 (2026-10-27, testé sur la copie)

→ partie déplacée dans `docs/archives/univers_anciennes_parties.md` (2026-11-06).

## 25. Résultats des matchs notés pour les statistiques (2026-10-27, compilé, pas encore vu en jeu)

→ partie déplacée dans `docs/archives/univers_anciennes_parties.md` (2026-11-06).

## 26. Pages de la fiche façon 2K14, version finale (2026-10-27, vue en jeu par l'utilisateur)

- Haut des deux pages : icônes LB / RB (U+E00B / U+E00E ; U+E00D est le stick gauche) + nom de la page.
  CHAMPIONSHIPS (ex-CONTENDER SETTINGS, sans le nom du catcheur) : A ou X met le titre de la ligne sur OFF / ON.
- STATISTICS : bandeau du champion (éléments 907-921) caché, nom du catcheur en haut (897), « LB RB
  STATISTICS » en dessous (898, x 96 y 55), 12 lignes espacées (bandes 996/1008 + k à y = 92 + 17k, libellés
  1020 + k à y - 18 échelle 0,8, valeurs dans la colonne 1032 + k déplacée à x 392, blanche ; 1044 + k cachée).
  Élément : +4 x, +8 y, +12/+16 échelle, +180 caché. Valeurs d'origine gardées et remises à la fermeture du
  tableau (`RestoreTableLayout`), l'écran RANKINGS du jeu est inchangé.
- Boîte Y : tenues seulement (les lignes STATISTICS... et titres sont retirées, la fiche a ses pages RB / LB).
- Main event de WrestleMania : la carte a 8 fiches, les vides ont 0 place (fiche +2) ; l'entrée +8 de
  `sub_826CCC38` est un pointeur, pas un nombre.
- À faire : la remise à zéro de l'Univers ne remet pas les effectifs des émissions (l'émission d'un Superstar
  est dans SA fiche : `sub_826D2B50(id)` = `sub_82A341A0(id, 3)` = octet +27 de `sub_82684910(id)`).

## 27. Émissions d'origine remises à la remise à zéro (2026-10-27)

L'émission d'un catcheur est dans SA fiche (286 octets, `sub_82684910(id)`, octets +24..+27 identiques,
l'Univers lit +27 par `sub_826D2B50`), pas dans le bloc Univers. Les fiches d'origine viennent de misc.pac,
groupe /CHAR/DAT (classe à 0x820ECC78), lu à chaque lancement par `sub_82DCAA58(this, données, taille)` :
place k < 323 → catcheur `sub_82A33440(k)`, entrée de 4 + 286 octets ; avec this+48 non nul, il écrit dans ce
tampon au lieu des fiches. On garde les données lues, et la remise à zéro les relit dans un tampon à nous puis
remet +24..+27 de chaque fiche (catcheurs créés, ids < 50, gardés). Valeurs : 0 RAW, 1 SmackDown, 4 et 6
autres marques (6 = 164 catcheurs, sans doute FreeAgent). Testé sur la copie : les fiches étaient déjà à
l'origine (0 octet changé) et SmackDown montre ses 20 catcheurs débloqués (27 dans les données).
Le « 8 à SmackDown » vu avant n'a pas été reproduit. Sonde : `wwe12_universe_probe` écrit logs/brands.txt et
les lignes « [univers] brands: » à la remise à zéro.

## 28. Money in the Bank noté (2026-10-27)

Détenteur de la mallette : U + 51004 (u16, 512 = personne ; vainqueur du Rumble en U + 51000), lu par
`sub_826CC718(vue)`, posé par `sub_826CCA90(vue, id, drapeau)` juste après un match Money in the Bank (type 76,
`sub_82A2F998`) pour son gagnant, effacé par `sub_826CCAB8(vue)` au cash-in. Vu en jeu : le match du cash-in est
ajouté à la fin de la carte du PPV et la mallette est effacée **après** son résultat (autre chemin dans
`sub_826C95E0` : effacée avant le match). Notre crochet de résultat garde les catcheurs du dernier match ; à
l'effacement, si le détenteur y était, « cashin <id> <gagné> <jour> », sinon on attend son prochain résultat.
« mitb <id> <jour> » à la pose. Testé : Mark Henry gagne la mallette (WINS 1) puis encaisse et gagne le titre ;
la première version (drapeau +5020 de l'objet 0x83231548) ne voyait pas ce cash-in, corrigé ensuite.
L'option qui coupait les blessures a été retirée (choix de l'utilisateur).

## 29. Façon 2K14 : stats générales, calendrier, matchs par show, menu EDIT UNIVERSE (2026-10-27)

Testé seul sur la copie, à la télécommande.

- **Statistiques générales** : Y sur l'écran d'accueil de RANKINGS ; même tableau que la fiche (page d'un classement),
  LB/RB = TITLES (plus de règnes, plus de semaines par titre), CHAMPIONS (champion + 5 anciens, semaines), MONEY IN
  THE BANK (détenteur U + 51004, taux après cash-in, anciens vainqueurs et résultat), ROYAL RUMBLE (vainqueurs et leur
  match de WrestleMania suivant) ; haut/bas = pages, B = retour (R+56 remis). Vu en jeu : OK. La barre du bas montre
  encore « X EDIT » du jeu (X ne fait rien ici).
- **Menu EDIT UNIVERSE** : boîte à choix du jeu en pages de 2 lignes + MORE... (`Choose`, texte 27051 pour la question,
  toujours sur 2 lignes sinon la boîte la pose sur la 1re option) : EDIT SUPERSTARS, EDIT TITLES, EDIT CALENDAR,
  MATCHES PER SHOW, RESET UNIVERSE.
- **Calendrier** (étude : table des jours U + 0xDE00, 7 octets, lundi d'abord, 0 RAW / 1 SmackDown / 2 Superstars /
  0xFF ; le dimanche n'y est jamais lu ; table des PPV U + 0xDDD0, 12 mois × 4 semaines, PPV 10-22 ; fiches de show
  U + 27360 + k × 984, +832 nom, +929/+930 marques RAW/SmackDown ; seuls les shows 0-2 ont un booker, `sub_826EB218`) :
  déplacer / retirer un show de la semaine (lundi-samedi), déplacer (mois + semaine, échange avec le PPV déjà là) /
  retirer un PPV, choisir son organisateur. Vu en jeu : SmackDown mis au mardi passe bien juste après RAW.
  L'écran SCHEDULE ne dessinait pas le logo d'un show dans un jour qui n'était pas le sien : corrigé (§31). Ligne de journal « calendar:
  weekdays … | PPVs by month … » à chaque ouverture du menu Univers. Retirer RAW/SmackDown et l'organisateur d'un
  PPV : pas encore essayés en jeu.
- **Matchs par show** (étude : chaque type de show a une classe de booker, son slot virtuel 3 = nombre de matchs :
  RAW/SmackDown `sub_826F19E0` 4, Superstars `sub_82E05C90` 3 (fonction partagée, reconnue par la vtable 0x8203EB30),
  PPV `sub_826F1EF8` 6) : notre réglage (`universe_shows.txt`, « matches <0/1/2/10> <n> ») le remplace. **Plantage vu**
  avec RAW à 6 : 4 modèles de match seulement (`sub_826DC6E8(table, groupe, i)`, table +4 groupes, groupe à table + 8 +
  g × 508, +2 = modèles), la case sans modèle était lue par une règle de la carte. Corrigé : pour l'appel du booker
  (retour 0x826F1690), le dernier match prend le dernier modèle (main event / titre) et les autres tournent sur les
  modèles normaux. Vu en jeu : RAW à 6 = MATCH #1 à #5 + MAIN EVENT, pas de plantage. PPV : 6 au plus.

## 30. Ceintures dans les statistiques générales (2026-10-27, testé seul à la télécommande)

TITLES et CHAMPIONS : un titre par page du tableau (haut/bas = titre suivant). Pour un titre solo, le bandeau du
jeu reste visible : `sub_82552910(R)` dessine la ceinture de R+8848 (éléments 908/917), et `sub_82552240(R)` la photo
du premier Superstar du haut, lu en R+8900 (puis les 4 aspirants tous les 8 octets). On y met le titre et son champion,
puis on remet R+8848 / R+8900 de RANKINGS à la fermeture. Un titre par équipes a une autre mise en page (grands
portraits) : sa page est dessinée comme un titre solo, sans bandeau, avec le titre et ses champions en premières
lignes. Les lignes du tableau sont dessinées pour le premier titre solo de la liste. Chaque élément touché
(position, taille, caché, texte +200, drapeaux +228, couleur +184) est gardé (`Keep`) et remis à la fermeture :
le vrai classement retrouve THIS WEEK / LAST WEEK.

## 31. SCHEDULE dessine les shows déplacés (2026-10-27, testé seul à la télécommande)

SCHEDULE (`sub_8255C408`) : 28 cases (4 semaines × 7 jours), chacune une entrée de 16 octets (table 0x82023790… :
+4 groupe du layout 0x29-0x2C = la semaine, +8 premier élément de la case (8 par case), +12 l'image « jour choisi »
(3 éléments par jour à partir de 56)). `sub_8255BFD8(écran, jour, case)` prend le show du jour
(`sub_826CCCD8(écran+29708, jour)`, journal : bonnes données) et donne son logo à l'objet image de la case
(écran + (case + 2) × 336). Mais la mise en page n'a de vraie image (élément de type 0xBCA, sprite « PPV_raw ») que
dans les cases des jours d'origine (lundi, jeudi, vendredi) ; les autres ont un « dummy » (type 0x1027) qui ne dessine
rien. Correctif : avant `sub_8255BFD8`, pour un show de la semaine (fiche +927 < 10, pas le dimanche : ses cases
dessinent les PPV autrement), chaque élément dummy de la case (+0 image, +3 2e image, +6 cadre, +7 cadre « aujourd'hui »,
+12 image du jour choisi) reçoit une copie de l'élément au même rang d'une vraie case, en gardant sa place (+4/+8),
son nom (+96) et **son objet image (+204)** (sinon il montre le logo du modèle). Vu : SmackDown au mardi, Superstars
au samedi, case du jour en grand, mois suivants, dimanche (SummerSlam) intact.

## 32. Choisir le vainqueur (X SIMULATE, comme les 2K récents) et SAVE corrigé (2026-10-27, testé par l'utilisateur)

Demande de joueurs (Discord). MATCH CARDS (panneau C = menu Univers + 260, écran S = C + 28, vtable 0x82023358) : X =
`sub_82556500(S)`, qui simule le match sous le curseur (C+552 la ligne, C+4100 + 4 × ligne le match) par
`sub_82554048(C, 1)` → `sub_826C9050(owner, match, mode 0, -1, -1)`. La simulation a un mode « résultat imposé » :
r5 = 2, r6 = place du gagnant, r7 = place du perdant (-1 : la 1re place d'une autre équipe) → `sub_826C4358`, qui écrit
le résultat comme le jeu (titres, momentum, news, nos stats). Notre X ouvre d'abord « Choose the winner of this
match » : une ligne par équipe (ses catcheurs joints par « & »), ou par catcheur au Royal Rumble, puis RANDOM (le
tirage du jeu) ; la réponse arme le mode imposé et appelle le X du jeu. Vu : le catcheur choisi gagne (WON / LOST).

**Boîte à plus de 3 lignes** : la boîte du jeu n'est faite que pour 2 lignes (textes D2/D3, copies petite police D4/D5
aux mêmes places, soulignements D6/D7) ; son code de curseur (`sub_8288F0A0`, vtable) parcourt count lignes sur
0xD2 + i et colore 0xD6 + i ; sa liste cachée de 7 lignes (E1-E8) n'est jamais dessinée. Un élément texte dessine
« 
 » comme un retour à la ligne et centre son bloc sur son y : on écrit deux colonnes (D2 blanc / D3 rouge à
gauche, D4 / D5 à droite, la colonne de droite complétée de lignes vides pour aligner les rangées), la ligne du
curseur seulement dans le bloc rouge. Gauche / droite (bits 4 / 8 de l'écran du cadre, `sub_824D20A0`) changent de
colonne (box + 56 = curseur). Jusqu'à 7 lignes (6 catcheurs + RANDOM) ; le Royal Rumble garde des pages (MORE...).
Le menu EDIT UNIVERSE et le calendrier profitent aussi des 7 lignes.

**SAVE d'un match modifié** : le match écrit était « le prochain à jouer » quand le jeu joue la carte dans l'ordre
(+20 = 1), d'où « modifier le match 3 change le match 1 ». Corrigé : RB EDIT (`sub_82554270(C)`) note le match sous le
curseur, SAVE l'écrit. Vu : le match 3 change, le match 1 reste.

## 33. Enchaînement automatique après un match : étude (2026-10-27, pas encore de correctif)

→ partie déplacée dans `docs/archives/univers_anciennes_parties.md` (2026-11-06).

## 34. SCHEDULE géré comme le PLANNING de 2K14 (2026-10-27, testé par l'utilisateur et à la télécommande)

Écran SCHEDULE : S (vtable 0x82023988, propriétaire S−28), S+284 le jour du curseur, S+29680 la vue ;
`sub_8255C408(S−28)` redessine les 28 cases ; X = `sub_8255CB08` (ouvre le SHOW EDIT du jeu, objet de 1332 octets
`sub_82589958`). **Y EDIT DAY** (Y libre sur cet écran) ouvre notre boîte pour le jour du curseur : jour de semaine vide
→ y mettre RAW / SMACKDOWN / SUPERSTARS (retiré de son ancien jour) ; jour avec un show → MOVE / MATCHES / REMOVE ;
dimanche vide → y mettre un PPV ; dimanche avec un PPV → MOVE / HOST SHOW / REMOVE (comme EDIT CALENDAR).
- Le curseur du jeu (`sub_8255C770`) saute les jours vides (`sub_826CE6C0(vue, jour)` = 255) : appelé depuis là, un
  jour vide répond 254, le curseur s'y pose. Sur un jour vide, la grande image du jour choisi (élément +12 de la case)
  est cachée et les cadres (+6, +7) sont donnés à la case, sinon le curseur est invisible ; une case à qui on avait
  donné une image retrouve son « dummy » quand son jour devient vide (sinon le logo d'un autre mois reste).
- Barres du bas (données d'aide 0x8324DF18, fiches de 20 octets, 3 paires {bouton, texte}, le jeu en +12) : 522
  (B BACK, X EDIT) reçoit Y ; 518 (A SELECT, B BACK, X EDIT, pleine) et 6 (B BACK, partagée avec d'autres écrans)
  ont leur dernier texte remplacé par 27053 / 27054 = le texte du jeu (45409 EDIT / 45040 BACK) suivi de
  « Y EDIT DAY » quand SCHEDULE est affiché (menu Univers +288 == +276), le texte seul ailleurs. Bouton Y = 45002.
- Matchs par show : le modèle n° (défaut − 1) du jeu est son main event (le 4e des 4 de RAW) : seul notre dernier
  match le prend, les autres tournent sur 0 .. défaut − 2 (vu : SmackDown à 5 = 4 matchs + 1 MAIN EVENT).
- **SHOW EDIT** (X EDIT sur SCHEDULE : propriétaire O de 1332 octets `sub_82589958`, écran O+28 vtable 0x82027188,
  O+320 sa copie de la fiche du show, +832 le nom, +927 le show) : écran plein (une vraie ligne MATCHES demanderait
  de déplacer UNIVERSE DRAFT, SAVE, DEFAULT et d'apprendre une ligne à son curseur). **Y MATCHES** ouvre le nombre de
  matchs de ce show ; sa barre (jeu 4 : A SELECT, B CANCEL, partagé) montre CANCEL = 27055 (« CANCEL   Y MATCHES »
  tant que SHOW EDIT est ouvert, posé avant que sa barre soit faite).

## 35. Défenses de titre en weekly (1.0, 2026-10-27, testé sur la copie)

Sonde : le jeu ne réserve jamais de match de titre dans les shows hebdo (drapeau à 0 sur toutes les cartes vues).
Un match devient un match de titre par un drapeau du jeu : à l'écriture de la carte (`sub_826EAEB0(booker, entrée, i)`,
fiche i = booker + 20 + i × 1320 → entrée de carte de 104 octets), entrée +4 bit 0 (`sub_826D4DD8(entrée, 1)`, lu par
`sub_826D4DC8`) et octet +1 = le titre. Le reste suit : le titre change de main avec le résultat (`sub_826D4030`), nos
stats. Fiche : +0 type, +4 titre, +12 drapeau titre, +16 places, +24 build WrestleMania, +28 rivalité, +40 + k × 32 les
places (+0 catcheur, +4 équipe) ; booker +4..+16 = les 4 titres du show.
Réglage `wwe12_universe_title_defenses` (pourcentage, **0 par défaut = le jeu d'origine**) : un match d'un show hebdo où
est déjà un champion du show (les deux champions dans une même équipe contre une seule autre pour un titre par équipes)
devient la défense de ce titre, avec cette chance ; une par carte au plus ; jamais un Royal Rumble, MITB, Elimination
Chamber, build WrestleMania ou match de rivalité ; personne n'est déplacé. Sur MATCH CARDS, le jeu ne montre un match
de titre que par l'image du show derrière les portraits (élément 23) : `sub_82554B68` est suivi d'un en-tête (élément 30)
= le nom du titre. Vu : US Evan Bourne vs Mason Ryan → Mason Ryan champion ; « WWE CHAMPIONSHIP » sur Mark Henry vs
Triple H.

## 36. Matchs de qualification Money in the Bank / Elimination Chamber + news (1.0, 2026-10-27)

Réglage `wwe12_universe_qualifiers` (**false par défaut = le jeu d'origine**). Pas de qualifs pour le Royal Rumble
(choix de l'utilisateur). Dans les 2 semaines avant le PPV (MITB = show 16, juillet semaine 3 ; EC = show 11, février
semaine 3 ; là où le calendrier met le PPV), chaque carte hebdo a **un match de qualification** : un 1 contre 1 de deux
hommes (pas de titre, pas de match spécial, pas déjà qualifiés), 6 au plus par événement et par année (année = jour /
336). Marqué à l'écriture de la carte (`sub_826EAEB0`, avant la défense de titre, qui ne s'y ajoute pas). Le gagnant
est qualifié (crochet de résultat `sub_826CEF28`). À l'écriture de la carte du PPV (booker vtable 0x8203EB44), les
qualifiés prennent les places du match MITB (type 76, `sub_82A2F998`) / EC (`sub_82A2FA98`) dans la fiche du booker,
avant qu'elle soit écrite, sauf s'ils sont déjà ailleurs sur la carte.
- En-tête de la carte : « MONEY IN THE BANK QUALIFYING MATCH » (élément 30, remis au nom du type de match du jeu,
  27400 + type, quand le curseur passe sur une autre ligne).
- News (écrivain du jeu `sub_826D1630(vue, fiche de 16 octets)`, liste de 240 à U + 51116) : types 5 et 6, non
  utilisés par le jeu et aux textes vides (27068-27071) ; +4 = 0x51000000 + événement ; nos textes (5 langues) remplacent
  27068-27071 pendant que cette news est dessinée (`sub_82559468` corps, `sub_82559A08` titre, la news en r4). Type 6 =
  « … qualifying matches begin this week … », type 5 = « X PLACE SECURED! / X has qualified … ». La liste ne garde que
  les news écrites avec les résultats : les deux news partent avec le résultat du 1er match de qualif.
- Fichier `universe_stats.txt` : « qualbegin / qualnews <év> <année> », « qualifier <év> <année> <a> <b> <jour> »,
  « qualdone … », « qualified <év> <année> <id> <jour> ».
Vu à la télécommande (MITB, copie) : en-tête QUALIFYING MATCH (Sin Cara vs Tyson Kidd), news « MONEY IN THE BANK PLACE
SECURED! — Sin Cara has qualified for the Money in the Bank Ladder Match! », 5 qualifiés sur 2 semaines, 4 placés dans
le match MITB du PPV (le 5e était déjà sur la carte). Elimination Chamber : même code, **pas encore vu en jeu**
(février).

### 36 bis. Elimination Chamber, news (suite, 2026-10-27, vu par l'utilisateur)

- Chamber par marque : la carte du PPV a une Chamber par titre (RAW, SmackDown). 5 qualifiés par marque (matchs de qualif
  entre deux catcheurs de la même marque, jusqu'à 3 par carte pendant les 2 semaines), placés dans la Chamber du titre
  dont le champion est de leur marque ; le champion garde sa place ; un qualifié déjà ailleurs sur la carte est échangé
  avec celui qu'il remplace. Money in the Bank : 6 en tout, 1 match par carte.
- **News : le jeu vide la liste** (`sub_826CD138`) dans `sub_826C8690(owner, match, 1)` après le 1er match joué d'un show
  (`sub_826C4778`), puis écrit les siennes : nos news (file d'attente) sont écrites après `sub_826C8690`, et la news
  « … begin this week » quand la 1re carte de qualif est préparée (RAW de février / juillet semaine 1) : elle est dans
  NEWS en arrivant sur ce RAW, avec celles du show d'avant (Royal Rumble). Le jeu ne dessine pas le type 6 : les deux
  news sont du type 5, +15 bit 7 = « begin ». Une news sans catcheur n'est pas affichée : « begin » prend la photo du
  champion WWE. Les marqueurs « news déjà écrite » restent en mémoire (une sauvegarde rechargée la réécrit) ; les lignes
  du fichier d'un jour plus tard que le jour courant sont ignorées (sauvegarde plus ancienne rechargée).
Vu : « Elimination Chamber qualifying matches begin this week… » au RAW de février semaine 1, puis « ELIMINATION CHAMBER
PLACE SECURED! — Kofi Kingston has qualified… » avec une seule photo.

## 37. UNIVERSE OPTIONS dans EDIT UNIVERSE (1.0, 2026-11-01, vu par l'utilisateur)

EDIT UNIVERSE > UNIVERSE OPTIONS : TITLE DEFENSES (OFF / 10 / 25 / 50 / 100 %) et QUALIFYING MATCHES (OFF / ON),
textes en 5 langues, message de confirmation. Le choix change le réglage tout de suite (`SetFlagByName`) et l'écrit
dans `wwe12.toml` à côté de l'exe (même fichier que le launcher, RÉGLAGES > AVANCÉ). Vu : « Title defenses on weekly
shows: 25 %. », et `wwe12_universe_title_defenses = 25` / `wwe12_universe_qualifiers = true` écrits dans le toml.

## 38. Plan 1.1 : deux mallettes Money in the Bank (demande de l'utilisateur, 2026-11-01)

Comme en 2010/2011 : un match MITB par marque au PPV (show 16) et **deux détenteurs** en même temps.
- Carte du PPV : deux matchs MITB (RAW / SmackDown), placés comme les deux Chambers (§36 bis) ; qualifs par marque.
- Le jeu n'a qu'un détenteur (U + 51004, u16, `sub_826CCA90` pose / `sub_826CC718` lit / `sub_826CCAB8` efface). La
  1re mallette reste celle du jeu (son cash-in d'origine) ; la 2e est à nous : détenteur noté dans `universe_stats.txt`
  (« mitb2 <id> <jour> »), gardé au reset/rechargement comme les autres lignes.
- Le cash-in de la 2e mallette est à faire nous-mêmes : étudier d'abord comment le jeu décide et ajoute le sien (match
  ajouté à la fin de la carte du PPV, chemin de `sub_826C95E0`), puis le rejouer pour notre détenteur (match de titre
  contre le champion de sa marque, changement de champion `sub_826D4030`, effacement + news).
- Affichage : fiche du Superstar, STATISTICS (page MITB), news de la victoire et du cash-in.
- Ordre : 1) étude du cash-in du jeu (sans rien modifier) ; 2) deux matchs au PPV ; 3) 2e détenteur ; 4) cash-in ;
  5) affichage. Réglage prévu dans UNIVERSE OPTIONS, validé par l'utilisateur : MONEY IN THE BANK = 1 ou 2
  (1 = un match et une mallette, le jeu d'origine, défaut ; 2 = deux matchs au PPV et deux mallettes).

## 39. Plantage 1.0.0 « 5 matchs RAW + 5 SmackDown » corrigé (2026-11-03, testé sur la copie, candidat 1.0.1) (archivé)

Déplacé le 2026-11-12 dans `docs/archives/univers_anciennes_parties_2026-11-12.md`.

## 40. 1.1 : jukebox, news des PPV, relations des catcheurs ajoutés (2026-11-05, vus par l'utilisateur)

- **Jukebox + NEXT MATCH** (bug Discord) : le thème ne s'arrêtait que quand le jeu coupait lui-même la musique des
  menus ; NEXT MATCH (pause de l'Univers) n'y passe pas. Il s'arrête maintenant à chaque écran de chargement (le temps
  de `Wwe12MsSinceLoadScreen()` redescend), dans le crochet de `sub_8271BFD0`. Réglage `wwe12_jukebox_stop_on_load`.
- **Image des news de qualification** : `sub_82558FC8(S, news)` nomme les 6 images de news (« newsBG_01..06 »,
  éléments 390-395, S + 1040 + i × 336 par `sub_82550B68` + `sub_828366A8`) et montre celle du type (table
  0x820236A8, type 5 = newsBG_03 = globe). Pour nos news, la n° 3 devient PPVbg_MinB / PPVbg_EC (les fonds du
  calendrier) : vu, fond Elimination Chamber derrière la photo. Réglage `wwe12_universe_news_ppv_picture`.
- **Alliés / rivaux** : table de la vue +12 (180 fiches de 40 octets, copiée par `sub_826CBCD0(vue + 8)`) ; fiche d'un
  catcheur `sub_826D4F98(table, id)` : +0 5 alliés, +20 5 rivaux (u32, 512 = personne ; non symétriques). Un catcheur
  ajouté gardait celles de l'ancien occupant de son numéro (Sandow à 116 : Eve et 4 Divas). `Wwe12UniverseForgetRelations`
  les vide et le retire des listes des autres, une fois par ajouté et par mod (`relations_cleared.txt` de Mods), à
  l'écran de sélection et à l'entrée de l'Univers ; copie de la sauvegarde avant. Réglage `wwe12_universe_added_relations`.
  Piège : la vue +4 est une AUTRE table (première version fausse, vue sur la copie seulement).

## 41. Arènes ajoutées, logos et matchup screens dans SHOW EDIT (étude, 2026-11-06)

Demande de l'utilisateur (par Main) : arènes du Mod Maker jouables dans l'Univers, CUSTOM LOGO, matchup screens et
photo d'arène. Étude en lecture seule du code recompilé (agent + Mods pour les images) ; rien n'est vérifié en jeu.

**Fiche d'un show** (984 octets, U + 27360 + k × 984 ; copiée par `sub_82588EA0`, dessinée par `sub_825871F8`) :
- +896 u8 logo : 0..39 = table 0x82022CB8 (40 × {PPV_x 256×128, PPV_x2}, dont Add_01..15 = 24..38, 39 = PPV_WWE) ;
  40 = logo perso, alors +952 u32 = case perso 0..19.
- +897 u8 matchup screen : < 31, table 0x82022DF8 (PPVbg_*, 512×128 ; onglet 1 = 0..20, onglet 2 = PPVbg_origin01..10).
- +898 u16 scène (`sub_826CBFB8` lit, `sub_826CBFD0` écrit ; 300 par défaut, `sub_826D5228`).
- +928 bit 0x40 = « réglé par SHOW EDIT » (`sub_826CBE70` à SAVE, `sub_826CBE50`) ; sans lui le match prend la scène 1.
- Vérif `sub_826CD8D0` : logo < 41, matchup < 31, scène < 300.
- Images : `MnLayHD.pac` MLYE/USEI (199 DDS), noms écrits dans l'image.

**Grilles** : SHOW LOGO `sub_82588788` (objet de 14224 octets, 41 cases fixes ; mode +14216 : 1 = 40 logos de show,
0 = 9 logos de marque 0x82022E74, liste pleine). Onglet CUSTOM (+14212 = 1) = 20 cases, lues par `sub_8254FD88` dans
l'objet `*(0x8317C5C4)+14204` (`sub_82587388`, chargé par `sub_82586160`, demande 55 au gestionnaire de sauvegarde
`sub_828955A0`, 20 fiches de 0x50468 octets) : ce sont les logos du Paint Tool / Create-A-Logo (`%02dPaintTool.pt`,
probable). « EMPTY » = aucun logo créé dans la sauvegarde. MATCH-UP SCREEN : `sub_82588B90`, onglets `sub_82586858`,
pas d'onglet perso.

**Arène** : choix = l'écran partagé `sub_825A67A0` (une seule page dans l'Univers, `wwe12_arena_pages.cpp`) ; liste du
jeu = registre des lieux (nombre 0x8324E250, fiches de 36 octets `*0x8324E24C`, scène u8 à +4). Photo de la case
ARENA : `sub_82586428`, table de 21 {scène, Uni_arena_*} (256×64), 200..249 = Uni_arena_CAR, sinon rien. Match de
l'Univers : `sub_826BBE30` (depuis `sub_82548818`, `sub_826BE900`) → `sub_828AC150(setup, scène, 1)`, qui n'écrit la
scène (+88) que si elle est dans le registre (`sub_828A98A8`), sinon garde l'ancienne. Autres tables à 21 :
`sub_826BB500`, `sub_82742D50`.

**Conclusion** : un numéro de scène fictif (150 + n) ne peut pas aller dans la fiche (pas de photo, refusé au match,
reste dans la sauvegarde si le mod est retiré). Voie propre : la fiche garde le numéro de la BASE, et un fichier à côté
de la sauvegarde (`universe_arenas.txt` : show → id du mod) dit quelle arène ajoutée joue ; avant chaque match de
l'Univers on sert l'arène du show (ou la base). Mod retiré = la base joue, rien de cassé.

**Fait (1.1, 2026-11-06, vu en jeu par l'utilisateur sur une copie : Breaking Point de SvR 2011 sur RAW, OK)** :
`wwe12_universe.cpp` (crochets `sub_826CBFD0` à 0x82584054 = choix de SHOW EDIT, `sub_826CBE70` à 0x825893C4 = SAVE,
`sub_828AC150` depuis 0x826BBE88 = match de l'Univers ; r31 y est le PORTEUR de la fiche, fiche = [r31]) et
`wwe12_arena_pages.cpp` (`Wwe12ArenaChosen/Id/Find/Play`, pages gardées dans l'Univers seulement sur la ligne ARENA de
SHOW EDIT). Réglage `wwe12_universe_added_arenas` (true). **Piège** : ne jamais laisser `g_reload_vs` (« écran VS à
refaire », `sub_828A9898` remet la scène de la préparation à -1) dans l'Univers : rien n'y remet la scène ensuite, le
match chargeait la scène -1 (plantage avant l'entrée dans `sub_822EE080`, lecture de 0x50, ou écran blanc).
`Wwe12ArenaPlay` le remet à false. Remarque de l'utilisateur (pour Mods) : sur Breaking Point, la foule gêne quand la
caméra s'éloigne.

## 42. Matchs par show : contenders en double et cartes incomplètes (1.1, 2026-11-06, vu sur la copie)

Liste d'un joueur (points 2, 4, 9) + capture de Bartók (2 « #1 contender » Divas, Natalya deux fois, SmackDown d'après
TLC) ; l'utilisateur : « des fois 2, des fois 3, des fois 4 » avec 6 demandés.
- **Modèles** (wuniData MTDT, lus en petit-boutiste dans le fichier, 84 octets, +4 = genre ; `sub_826EA830` donne le
  titre visé) : tables 0-12 = PPV (6 modèles), 13 = RAW / SmackDown (32 groupes de 4 ; genres 1, 2, 3, 8, 19 ; le 4e
  toujours genre 4 = main event), 14 = Superstars (12 × 3), 15 = shows d'après un PPV (`sub_826CD010` : 4 modèles,
  tous genre 8 = #1 contender, un par titre). Nos matchs en plus reprenaient ces modèles : contender en double.
  Correctif : les matchs en plus prennent les modèles de la semaine autres que le genre 8 ; après un PPV ils gardent
  leur modèle de la semaine (`t_regular_template`, crochet `sub_826DC6E8` à 0x826F16C8).
- **Cartes incomplètes** : avant une carte (`sub_826EB510`), le jeu met au repos tout catcheur d'une carte des 7
  derniers jours jusqu'au dernier PPV (`sub_826E2340(ensemble, id)` depuis 0x826EB8B4). Avec plus de matchs, la moitié
  de la marque se repose, une place reste « personne » (512), le match est invalide (`sub_826EAAF8`) et la réparation
  du jeu (règle 22, `sub_826F09D0`, un simple entre deux catcheurs libres du classement) ne trouve personne → match
  abandonné. Correctif : pour un match sur le point d'être abandonné seulement, les catcheurs au repos qui ne sont pas
  sur la carte redeviennent libres le temps de la réparation. Ligne de journal par carte : « card of …: x of n valid ».
  Vu : RAW et SmackDown à 6 sur 6 pendant plusieurs semaines, « match 1 had nobody free, 18 rested … booked ».

Note : la table 15 (`sub_826CD010` = « draft ce jour-là ») est celle des **matchs du draft** (en-tête « WWE DRAFT
MATCH », vu le 2026-11-06) ; le genre 8 s'affiche « #1 CONTENDER'S MATCH » dans une semaine normale (table 13).

## 43. News du draft (1.1, 2026-11-06, vu par l'utilisateur sur une copie)

Demande de l'utilisateur : en simulant le draft d'après WrestleMania, on ne savait pas qui changeait de marque. Le draft
se joue au 1er RAW et au 1er SmackDown d'après WrestleMania (matchs « WWE DRAFT MATCH ») ; un catcheur change de marque
(+27 de sa fiche, `sub_82684910(id)`) juste après son match (rien ne change dans `sub_826C0910`). Les marques de tout
l'effectif (`sub_82A33440(k)`, k < 323) sont gardées et comparées après chaque résultat (`sub_826C8690`) et à
l'ouverture du menu Univers, seulement un jour de draft (`sub_826CD010(vue, sub_826CC618(vue))`) ou juste après : chaque
catcheur passé à RAW / SmackDown reçoit une news de type 5 (+14 = 0x52, +15 la marque), textes en 5 langues
(« DRAFTED TO RAW! » / « %s has been drafted to RAW! »). Réglage `wwe12_universe_draft_news` (true). Vu : Cody Rhodes
→ RAW, Triple H → SmackDown dans NEWS (le jeu vide la liste à chaque show : chaque show du draft montre les siens).
