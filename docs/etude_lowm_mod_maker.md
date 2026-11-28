# Étude : WWE Legends of WrestleMania (Xbox 360) dans le Mod Maker

Demande de l'utilisateur (2026-11-18, puis relancée par la Main le 2026-11-28 pour la **2.0**) : ajouter des jeux
dans MES DISQUES comme Kaiko, en commençant par Legends of WrestleMania (LOWM, 2009). **Étude seulement, rien n'est
codé.** Pas de téléchargement du jeu ; aucune donnée du jeu dans le dépôt.

**Décision de l'utilisateur (2026-11-28, via la Main) :** pour la 2.0, LOWM = **catcheurs, entrées (chariots
compris) et arènes**. Pas de prises, pas de SmackDown PS2, pas de vidéos All Stars. Rien n'est codé avant son feu
vert et ses disques (ISO Europe et USA).

## 0. État au 2026-11-28 : fait, vérifié hors du jeu

L'utilisateur a donné son ISO **Europe** (`Desktop\WWE ISOS\WWE Legends of WrestleMania (Europe).iso`) et son feu
vert. Pas encore d'ISO USA : la comparaison des régions attend.

- **Disque :** xex titre 54510838, média 59180D5B, zone 00FF0000 (Europe). 398 fichiers, la disposition de Kaiko.
- **Code** (`port/src/modmaker/`) : `game_discs.h` (MES DISQUES), `star_port.h` (`kLowm`, `SvrOld`, nom du jeu),
  `strings2k.h` et `ratings_import.h` (fiche 262 octets, numéro à +44), `new_star_page.h` / `superstar_page.h`
  (choix du jeu, échelle des rendus 1.15 comme 2009), `theme_source.h` (musique PRO +0xE4), `svr_old_entrance.h`
  (règles de Kaiko : figurants → 13902, objet 6 toujours du modèle 2011 ; chariots refusés avec leur raison),
  `svr_scenes.h`, `arena_page.h` (carte « Legends of WrestleMania », `svr_lowm_`, année 2009), `port_svr_arena.h`
  (table `kArenasLowm`, images VS `M<hexa>I`), `moveset_port.h` (prises : refus clair, hors périmètre).
- **Vérifié hors du jeu avec le code du Mod Maker** (outil de test dans le dossier temporaire, rien de copié) :
  - noms : 101 fiches nommées de 100 à 254 (100 The Rock, 254 Ultimate Warrior) ;
  - catcheurs : les 99 modèles `ch100-254` portés sur une base WWE '12 sans erreur ;
  - entrées : 157, dont 124 converties ; 29 chariots refusés (pas encore portés) ; 4 échecs propres (4030, 4200,
    4311, 4312 : « a key value out of its field » / une table d'événements) ;
  - arènes : les 16 (WrestleMania I à XV, Royal Rumble) converties ;
  - ceintures : 12 familles lues (dont Million Dollar, WCW classique, Attitude).
- **Pas fait :** test en jeu (à l'utilisateur), stats (pas de table d'équivalence : celles de la base), chariots,
  les 4 entrées en échec, comparaison Europe / USA.

## 1. Où on en est (étude du 2026-11-28, avant le code)

- **Chez nous** (`port/src/modmaker/game_discs.h`) : SvR 2007, 2008, 2009, 2010, 2011, WWE '13, 2K14, 2K15, 2K16,
  2K17. LOWM manque. Pas de disque LOWM sur ce PC (ni dans `Desktop\WWE ISOS`, ni dans `modmaker_discs.txt`).
- **Chez Kaiko** (port SvR 2011, `port/modmaker/disc_list.cpp`) : les mêmes jeux Xbox 360 **plus LOWM**, et les 9
  SmackDown PS2 (Just Bring It, Shut Your Mouth, Here Comes the Pain, SvR, SvR 2006, PS2 SvR 2007-2011). Il lit aussi
  les vidéos des DLC de WWE All Stars (jumbotrons Bink 1). Ce sont les autres jeux qu'on pourrait ajouter ensuite.
- **LOWM chez Kaiko** : catcheurs, entrées (y compris les chariots « Gondola »), arènes et notes testés en jeu
  (octobre 2026). **Prises pas portées.** Sa méthode peut être reprise avec crédit (son accord du 2026-10-23).

Sources (dépôt `KaikoClanworth1/wwe-svr2011-pc`, lu en lecture seule) :
- `docs/OTHER_GAMES.md` : sections « Legends of WrestleMania » (arènes) et « Legends of WrestleMania (360) »
  (catcheurs) ;
- `port/docs/OTHER_GAMES_ENTRANCES.md` : section « WWE Legends of WrestleMania (Xbox 360) » ;
- `port/tools/superstar_port.py`, `entrance_port.py` (`low_convert`), `arena_port.py`, `ratings_port.py` (jeu
  `low`).

## 2. Le jeu

- Numéro de titre : **54510838** (le `default.xex`). Disque XDVDFS, partition à 0xFD90000 comme SvR 2008-2010,
  409 fichiers, dont 236 pac.
- Moteur et formats de **SvR 2009** : tous les pac en EPAC, une PACH par fichier. Nos lecteurs (EPAC, PACH, BPE,
  JBOY, YMKs) les ouvrent tels quels.

### Europe et USA

- **Reconnaissance :** le Mod Maker reconnaît un jeu au numéro de titre de son `default.xex` (infos d'exécution,
  `game_discs.h` `TitleOf`). Chez THQ, ce numéro est le même pour toutes les régions : seuls la zone et l'identifiant
  de média du xex changent. Preuve chez nous : notre ISO **Europe** de SvR 2008 est reconnue avec 5451080B, le même
  numéro que Kaiko donne sans parler de région. Donc 54510838 devrait reconnaître les deux ISO de LOWM. **À
  confirmer** sur les deux disques en lisant leur xex (titre, zone, identifiant de média) avant de coder.
- **Une seule ISO à la fois :** MES DISQUES garde un disque par numéro de titre. Ajouter l'ISO USA remplace l'ISO
  Europe dans la liste, et inversement. Ça suffit : les deux doivent donner les mêmes catcheurs, entrées et arènes.
- **Pas de texte d'une langue dans le code :** on lit les catcheurs par leur numéro (fiches CHAR/DAT de `chEtc`, noms
  à +46 / +114 / +182), les arènes par leur numéro de fichier (`bg201`…) et les entrées par leur numéro (4001-4359).
  Les noms d'arènes viennent de `string.pac` par **numéro d'entrée** (SDB 0100, entrée 674 + n − 200), jamais en
  cherchant un texte. Si une version a plusieurs langues dans `string.pac`, on prend la table anglaise par sa place,
  pas par son contenu.
- **Ce qui peut différer entre les deux** (à vérifier) : `string.pac` (langues), les sons de commentaires et de menus
  (`sound/`), les vidéos avec texte (`movies/`), et `default.xex`. Les modèles (`pac/ch`), les entrées
  (`pac/evt/Nyujyo*`) et les arènes (`pac/bg`) devraient être identiques. Quand l'utilisateur donne ses deux ISO, une
  comparaison fichier par fichier (taille et empreinte, hors jeu, rien de copié) dit exactement lesquels diffèrent ;
  on lui donne la liste avant de coder.

## 3. Ce qui se lit déjà, ce qui manque

| Partie | Où | Écart avec SvR 2009 | Effort chez nous |
|---|---|---|---|
| Reconnaître le disque | `default.xex` | aucun | ~1 h : ajouter 54510838 là où 0x54510826 est reconnu |
| Catcheurs (89 avec modèle, ids 100-254) | `pac/ch/ch100-254.pac`, `chEtc.pac` | CHAR/DAT 262 octets : 12 octets de stats (deux fois) au lieu de 8 ; nom brut +46, complet +114, court +182. CHAR/PRO 504 octets, bloc d'entrée +0xE4 comme 2009 (tenue 2 à +0xF0). Rendus dans `DLC_HD.pac`, groupes SSFA / SSFB / SSFC au premier niveau (2009 les range sous MENU). Ids à eux (100 = The Rock ; leur Ted DiBiase 136 est le père) | ~½ journée |
| Stats | CHAR/DAT +4..+15 | propre barème : cinq vrais champs (+6 +7 +8 +12 +13), le reste à 0x01 ; pas de capacités | ~2 h : table d'équivalence comme Kaiko (approximative) |
| Musiques, titantrons | `sound/music1.yaf` / `music2.yaf`, `movies/titantron/<n>.bik` | aucun (numéro de musique × 100, Bink 1) | inclus dans les catcheurs |
| Arènes | `pac/bg/bg201-215` (WrestleMania I à XV), `bg216` (Royal Rumble 2008), `bg219` (sa copie pour le match Rumble) | chemin « 2008 » : jeux de textures déplacés, lumières renumérotées. Images de chargement / VS dans MatchHD `M<n° en hexa>I` (`MC9I` = bg201 ; 2008 / 2009 les nomment en décimal). bg223 / 224 / 225 / 250 ne sont pas des arènes | ~½ journée |
| Entrées (157, n° 4001-4359) | `pac/evt/Nyujyo0-3.pac` | format 2009 (~60 % des pistes identiques). Figurants génériques 10902 / 24802 / 24902 → 13902 avant les règles 2009. 30 accessoires sur 43 existent ailleurs | ~1 journée sans les chariots |
| Entrées en chariot « Gondola » (29) | 4170-4186, 4230-4233, 4350-4359 | chariot 2552 / 2565 dans `pac/evt/_EvtObj.pac`, sur sa propre piste ; leurs scripts commencent au segment 1 | ~1 journée de plus, à faire après |
| Ceintures | sans doute `pac/evt/_EvtObj.pac` | à vérifier sur le disque (lecteur SvR 2008-2010 déjà fait) | ~1 h si c'est bien le même format |
| Prises | CHAR/PRO | **inconnu** : disposition et numéros de prises propres, non faits chez Kaiko | étude à part, plusieurs jours |

Code à toucher (relevé le 2026-11-18 dans `port/src/modmaker/`) : `game_discs.h`, `star_port.h` (`SvrOld`, nom du
jeu), `arena_page.h` (`kOldSvr`), `moveset_port.h`, `new_star_page.h` (plusieurs listes, échelle), `ratings_import.h`,
`superstar_page.h`, `svr_old_entrance.h`, `svr_old_slots.h`, `theme_source.h`, `belt_port.h`.

## 4. Effort et ordre conseillé

Périmètre décidé (sans les prises) : **environ 3 jours** de travail avec les disques, tests en jeu compris.

0. Comparer les ISO Europe et USA (liste des fichiers qui diffèrent, xex des deux) ; donner la liste à l'utilisateur.
1. Reconnaître le disque dans MES DISQUES (les deux régions).
2. Catcheurs (modèle, noms, stats, musique, titantron, rendus) ; test en jeu.
3. Arènes ; test en jeu.
4. Entrées sans chariot ; test en jeu. Puis les chariots.
5. Ceintures (petit bonus si c'est le format de SvR 2008-2010).

Hors périmètre (décision du 2026-11-28) : prises, SmackDown PS2, vidéos All Stars.

## 5. Ce qu'il faut de l'utilisateur

- **Ses propres ISO** (Europe et USA), et son feu vert, seulement au moment de l'étape 0.
  On ne la télécharge pas pour lui.
- Rien d'autre tant qu'on n'en est pas aux tests en jeu (qu'il fait ou autorise, comme d'habitude).

Risque principal : tant qu'on n'a pas le disque, tout ce qui précède vient des notes de Kaiko. Les écarts réels se
vérifient sur le disque avant de coder (outils hors jeu d'abord, comme pour SvR 2007-2010).
