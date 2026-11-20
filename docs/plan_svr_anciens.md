# SvR 2007 à 2010 dans le Mod Maker : état actuel (résumé du 2026-11-12)

Demande de l'utilisateur (2026-10-31) : porter dans WWE '12 les catcheurs et les arènes de SvR 2007-2010 (ISO dans
`Desktop\WWE ISOS`). Méthode : **SvR 2007-2010 → format SvR 2011 (méthode des outils Python de Kaiko, reprise en C++
avec crédit, accord de l'utilisateur le 2026-10-31) → WWE '12 (portage SvR 2011 existant)**.

## Le plan est fait

| Étape | Contenu | Où dans le code |
|---|---|---|
| 1 | « Mes disques » reconnaît SvR 2007 (545107E0), 2008 (5451080B), 2009 (54510826), 2010 (54510844) | `game_discs.h` |
| 2 | Catcheurs SvR 2010 (même format que 2011) | `port_svr::PortAttire` ; choix `svr_game_` dans NOUVEAU CATCHEUR |
| 3 | Arènes SvR 2010 (17) | `port_svr_arena::Port`, table `kArenasSvr10`, plaques `SvrEtex` |
| 4 | Catcheurs SvR 2008 / 2009 | `port_svr::OldSvrPictures` (20000 fusionné dans le corps, os k_* suivent leur parent) |
| 5 | Arènes SvR 2007 / 2008 / 2009 | `port_svr_arena::OldSvrArena`, `Svr07Stage` (table `kArenasSvr07`), `Svr07CrowdPlaces`, `OldSvrPlates` |
| 6 | Catcheurs SvR 2007 (textures cuites sur planches 2048 × 1024 / 1024 × 1024) | `port_svr::Svr07Sheets` |
| 7 | Entrées et prises | victoires de SvR 2007 (`evt7.pac`, `svr_old_entrance::Convert`, `VictoryOf` 6000 + id, vérifiées hors jeu) ; prises SvR 2007-2010 faites le 2026-11-09 (`docs/prises_svr11.md`) ; musiques SvR 2007-2010 (`docs/plan_movesets.md`) |

Testé hors du jeu : les 74 / 85 catcheurs de 2008 / 2009 (1re tenue), les 82 fichiers de 2007, les 20 / 19 / 20
arènes de 2008 / 2009 / 2007, planches de visages de tous les catcheurs 2008-2010.

**Vu en jeu** : SvR 2007 (RAW + Edge, Randy Orton sur RAW, Tyson Kidd ; 2026-11-01 : catcheur, écran VS, arène, rampe,
public, titantron 808, plaques) ; arènes SvR 2008 (Judgment Day) et SvR 2009 (SmackDown « nickel ») le 2026-11-11
(détail et règles actuelles des écrans, plaques, tapis, bannières dans `docs/mod_maker.md`, § 7). D'après ce document,
les catcheurs 2008-2010 et les arènes 2010 n'étaient vérifiés que hors du jeu (`docs/plan_movesets.md` note MVP de
SvR 2010 importé et vu en jeu le 2026-11-01).

## Formats utiles

- Noms : 2007 `pac/misc.pac` CHAR/DAT (176 octets, numéro à +26, nom à +28) ; 2008 `pac/misc.pac` (232 octets, numéro à
  +30) ; 2009 `chEtc.pac` (252 octets, +36) ; 2010 `chEtc.pac`.
- Portraits : 2007 `menu/menuHD.pac` MENU/SF2B (SSFA et SSFB) et SF2A (SSFC) ; 2008 `menu/menuHD.pac` ; 2009 / 2010
  `DLC_HD.pac` ; MENU/SSFA-SSFC = PACH { 0 : PACH { numéro : DDS } }.
- Vidéos : 2007 / 2008 `movies/titantron/NN.bik`, 2009 / 2010 `NNN.bik` (Bink 1, 320 × 320).
- Arènes 2007 : jusqu'à dix par fichier, entrée STG<k>/00NN de `pac/bg/bg<k>.pac` (k = NN / 10).
- Plaques : 2007 `pac/evt/evt2dHD.pac` ET2D/00NN ; 2008 ET2D/00NN ; 2009 ETEX/00NN + ATB0 v1 (10000) ; 2010 ETEX comme
  2011 (fichiers `pac/evt/_EvtGdHD.pac` / `_EvtMov.pac`). ATB0 v1 : clé 16 octets {x, y, image, 0}, canal 0 place
  (640 × 448), 1 échelle, 2 couleur RGBA, longueur à +0x2C (60 images/s).
- Transitions : 2007 `Svr07Transition` (36 images à 30/s, 320 × 320 avec alpha) ; 2008 logo W (`pac/movie.pac`
  `MOV /M0we`) ; 2009 logo WWE (2D) ; MOVT n'existe qu'à partir de 2011.
- Rampes au sol (`port13::RampHeight`, modèles « slo… » à tout numéro) : SvR 2007 SmackDown, Velocity, SummerSlam, No
  Mercy, Survivor Series, Armageddon, Royal Rumble, ECW One Night Stand, Taboo Tuesday ; SvR 2008 00, 02, 05-10, 15, 17, 19.
- Texte des plaques 2007 : `OldSvrPlates` centre le texte dans la fenêtre sombre de la plaque ; base 1 (RAW) mesurée :
  nom vers (1100, 915), ville vers (505, 222) sur 1920 × 1080 (`PlateTextFor`).

## Reste / limites

- Pas repris : pièces de tenue 0x20+ de 2008/2009 (ch23 2304, ch70 7004, ch137 8904) ; écran VS propre de SvR 2010
  (`SvrCard`), ses plaques et sa transition rangées autrement ; boucles d'arène `pac/movie.pac` MOV/M000-M019 de 2007.
- Marches et table des commentateurs (`bgEtc.pac`, communs) : voir `docs/mod_maker.md`.
- ch92 de 2007 vert sur le disque même ; la plaque de SmackDown 2007 (vidéo dans un carré rouge) reste vide dans WWE '12.

## Legends of WrestleMania (Xbox 360) : plan, pas fait

Noté le 2026-11-18 à la demande de l'utilisateur. Rien n'est codé : personne ici n'a le disque pour tester. On ne
télécharge pas le jeu, et on n'ajoute pas un support non testé. À faire le jour où quelqu'un a sa propre copie.

**Source : le port SvR 2011 de Kaiko**, qui gère déjà ce jeu (testé en jeu chez lui en octobre 2026). Sa méthode
peut être reprise avec crédit (son accord du 2026-10-23). Dépôt `KaikoClanworth1/wwe-svr2011-pc` :
- `docs/OTHER_GAMES.md` : sections « Legends of WrestleMania » (arènes) et « Legends of WrestleMania (360) »
  (catcheurs) ;
- `port/docs/OTHER_GAMES_ENTRANCES.md` : section « WWE Legends of WrestleMania (Xbox 360) » ;
- `port/tools/superstar_port.py`, `entrance_port.py` (`low_convert`), `arena_port.py`, `ratings_port.py` (jeu
  `low`) ; `port/modmaker/disc_list.cpp`.

**Ce qu'on sait (d'après Kaiko) :**
- Disque 54510838, XDVDFS comme SvR 2009. Moteur et formats de **SvR 2009**, tous les pac en EPAC.
- **Catcheurs :** `pac/ch/ch100-254.pac` et `chEtc.pac`, 89 catcheurs avec modèle, numéros propres (100 = The Rock).
  - CHAR/DAT 262 octets : celui de 2009, mais 12 octets de stats (deux fois) au lieu de 8. Nom brut à +46, nom
    complet à +114, nom court à +182.
  - CHAR/PRO 504 octets, bloc d'entrée à +0xE4 comme 2009 : musique, vidéo, entrée (4000 + numéro − 100), entrée
    de champion ; tenue 2 à +0xF0.
  - Stats à +4..+15 sur son propre barème (cinq vrais champs : +6 +7 +8 +12 +13). Pas de capacités.
  - Modèle : même lecture que 2008 / 2009. Rendus : `DLC_HD.pac`, groupes SSFA / SSFB / SSFC au premier niveau.
  - Prises : PAS portées chez Kaiko (CHAR/PRO à sa propre disposition, numéros de prises propres).
- **Musiques et vidéos :** `sound/music1.yaf` / `music2.yaf` (numéro de musique × 100), `movies/titantron/<n>.bik`.
- **Entrées :** `pac/evt/Nyujyo0-3.pac`, 157 entrées 4001-4359 (format 2009, ~60 % des pistes identiques à 2009).
  Règles en plus de celles de 2009 : figurants génériques 10902 / 24802 / 24902 → 13902. Entrées en chariot
  « Gondola » (4170-4186, 4230-4233, 4350-4359) : chariot 2552 / 2565 dans `pac/evt/_EvtObj.pac`.
- **Arènes :** `pac/bg/bg201-215` (WrestleMania I à XV), `bg216` (Royal Rumble 2008), `bg219` (copie du Rumble).
  Chemin « 2008 » des arènes : jeux de textures déplacés, lumières renumérotées. Images de chargement / VS : MatchHD
  `M<n° en hexadécimal>I` (`MC9I` = bg201). bg223 / 224 / 225 / 250 ne sont pas des arènes.
- **Ceintures :** sans doute dans `pac/evt/_EvtObj.pac` comme SvR 2008-2010 (à vérifier).

**Travail chez nous :** ajouter 0x54510838 partout où SvR 2009 (0x54510826) est reconnu, puis gérer les écarts
ci-dessus. Endroits trouvés le 2026-11-18 dans `port/src/modmaker/` : `game_discs.h`, `star_port.h` (`SvrOld`, nom
du jeu), `arena_page.h` (`kOldSvr`), `moveset_port.h`, `new_star_page.h` (plusieurs listes, échelle), `ratings_import.h`,
`superstar_page.h`, `svr_old_entrance.h`, `svr_old_slots.h`, `theme_source.h`, `belt_port.h`. Ordre conseillé :
reconnaître le disque, puis catcheurs, arènes, entrées (sans les chariots d'abord), ceintures. Tester chaque étape
en jeu avant la suivante.

Historique complet, essais et détails : docs/archives/plan_svr_anciens_anciennes_parties_2026-11-12.md
