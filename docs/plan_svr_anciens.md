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

Historique complet, essais et détails : docs/archives/plan_svr_anciens_anciennes_parties_2026-11-12.md
