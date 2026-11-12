# Prises ajoutées à WWE '12 (SvR, WWE '13, 2K) : état actuel (résumé du 2026-11-12)

But : qu'un catcheur porté garde aussi les prises que WWE '12 n'a pas. Le moveset traduit par le nom est fait pour
tous les jeux (`moveset_port.h` `FromGame` ; SvR 2007-2010 `FromOld`, tables de places de Kaiko dans
`svr_old_slots.h`). Ce document est sur les **prises ajoutées** (animations portées).

## État en bref

- **Coupées par défaut (décision de l'utilisateur, 2026-11-10)** : cvar `wwe12_added_moves` (défaut **false**,
  `wwe12_superstar_mods.cpp`) : les prises ajoutées des mods sont laissées de côté au démarrage et leurs cases gardent
  la prise de la base. Le Mod Maker le dit sous sa case « Prises ajoutées (expérimental) » (décochée par défaut depuis
  le 2026-11-05). Raison : avec les mods, un catcheur envoyé dans les cordes **traverse le ring** (sans mods,
  `--wwe12_no_mods`, il rebondit) ; Gut Kick de tous cassé avec un mod Maria SvR 2009 ; cause exacte pas trouvée
  (l'ajout touche des données partagées : fiches WAZE insérées, lignes m3.pac insérées, situations ZMVM ; tables m3
  0/20, 0/21, 0/22, 1-4 non comprises).
- **Vu en jeu** : Cesaro 2K14 porté, ses 12 prises (finisher Neutralizer, taunts) en match (2026-11-08) ; Batista Bomb
  de SvR 2011 « nickel » après la correction de la position au sol (2026-11-09) ; Hulk Hogan 2K14 refait sur modèles
  (13 prises), plus de gel (2026-11-10).
- **Pas encore vu en jeu** : 2K16 / 2K17, SvR 2010, SvR 2007-2009, situations ZMVM (`zmvm_add.h`), remplaçants
  (`StandIn`).
- Pour plus tard : l'outil de MrSmith (annoncé par l'utilisateur) ; la méthode complète de LA Max (case vide de la
  table des prises, rien d'inséré dans m3.pac, l'animation dans les situations du modèle).

## Comment une prise est ajoutée (paquet WWE12PCMODS)

- **Fiche WAZE** (160 o) insérée par nom dans une copie de `misc.pac` (comme les packs de THQ) ; bits de catégorie
  (16 premiers octets) pris de prises « donneuses » (`FitCategories`).
- **Animations** dans `m_dlcNN.pac`, banque `ZMOT/CA11+` (disque CA01-08, DLC 3-4 CA09-10), **tête de groupe = 177 +
  n° de banque (188 pour CA11)** ; la fiche légère = la complète (compressée BPE ; le jeu ne trouve pas la complète
  d'une prise ajoutée).
- **Paramètres d'animation** : copie de `m3.pac` (MDAT/PRM) avec les lignes de la prise (`m3_params.h`, fichier
  `<numéro>.m3` du mod). Sans eux : gel.
- **Renumérotation par famille** : la centaine du numéro d'animation est le genre de prise ; une prise ne la garde que
  si les catcheurs de WWE '12 l'utilisent dans la même case (taunts 2K14 374xx → 373xx, Neutralizer 505xx → 504xx).
  Deux mods vers le même numéro : `superstar_pack::SpreadNewMoves` donne au second une autre case libre de la centaine.
- **Situations ZMVM** (`zmvm_add.h`, appelé par `superstar_pack::Build`, méthode de LA Max, 2026-11-10) : chaque
  situation (`/ZMVM/STAT`, etc.) qui a la prise modèle (même octets 0x50-0x8F de fiche) est copiée avec l'animation
  ajoutée à côté ; une case vide de 128 octets au numéro de la prise est remplacée ; copies dans le `m_dlcNN.pac` sous
  des noms en minuscules (`ZMVM/stat`), le crochet de `sub_827D98D8` demande d'abord `/ZMVM/stat`. Hors jeu (Maria
  2009) : rien du jeu perdu ; 0,45 s au démarrage. Peut-être pas la correction du Gut Kick.
- **Create a Moveset** : fiches RGRP/MAGG converties au format de WWE '12, bits pris d'une prise de la même case ;
  aucun aperçu de prise ajoutée ne se joue (mis de côté).

## Sources

- **WWE '13 / 2K14-2K17** (`newmoves13.h`, `m3_params::FromOther`) : lignes m3 converties champ par champ ; 2K16 :
  tables 0/0-0/7 dans l'ordre d'octets de la console ; 2K17 : HSPC dont l'enfant 21 = les 0/0-0/11 de 2K16
  (`detail::OtherPrm`). **Depuis le 2026-11-10 (`model_moves.h`)** : chaque prise WWE '13 / 2K est refaite sur un
  **modèle WWE '12** du même genre (fiche +0x72-0x73), mêmes personnes, mêmes segments (x < 90), durée entre la moitié
  et le double ; elle garde ses animations et prend du modèle la fiche (nom et numéro gardés), les commandes YAT2
  (retimées), les paramètres m3, l'aperçu et les fiches Create a Moveset ; numéro dans la centaine du modèle
  (`Renumber`, `ModelHundreds`). **Sans modèle sûr, elle n'est pas ajoutée.** Hors jeu : 1 586 / 1 725 faites (WWE '13
  120/127, 2K14 300/327, 2K15 447/488, 2K16 343/369, 2K17 376/414).
- **SvR 2011** (`svr_moves.h`, branché dans `star_port.h`) : de la vraie prise : attaquant et victime de chaque
  segment, le cap (piste « vector », recalculé depuis le corps converti), ses événements (WAZA/DATA EVT → m3 0/3 + 0/4)
  et deux champs EXH (+10 → 0/1 +6, +18 → 0/2 +4). D'un modèle WWE '12 (prise commune, même catégorie SvR, mêmes
  segments, durée proche, bien recalée) : placement de chaque catcheur, en-tête et commandes YAT2, renvois de contre
  (x 90-92), autres lignes m3, fiche WAZE, aperçu, fiches CAM.
- **SvR 2010** : tables dans misc.pac **BATS/INIT enfants 10 / 11 / 12**, lignes EXH de **32 octets** (36 en 2011).
- **SvR 2007-2009** : fiches WAZE de **144 octets** (numéro à +0x88) refaites en 160 (mêmes octets jusqu'à +0x73, le
  reste 8 octets plus loin) ; banques d'animations dans m.pac **MVMT** (2009 par catégorie ; 2007 / 2008 MOTN) sans
  l'en-tête de 16 octets (rendu) ; 2009 a ses tables dans BATS/INIT 10 / 11 ; 2007 / 2008 n'ont pas de table
  d'événements (événements du modèle, recalés).
- **YMBs** (`svr_ymbs.h`, d'après `ymbs.py` / `ymbs_retarget.py` de Kaiko, avec sa permission) : provocations et
  soumissions ; une personne et trois personnes acceptées si le modèle a le même nombre.
- **Règles pour toutes les SvR** : modèle du même genre, mêmes segments, durée entre 0,5 et 2 fois celle de la prise ;
  placement par le début quand l'animation du modèle diffère de celle de WWE '12.
- **Cases couvertes** (prises manquantes ajoutées) : 2011 96 %, 2010 68 %, 2009 66 %, 2008 48 %, 2007 48 %. Toujours
  refusées : prises avec un objet (y 60/61, la Batista Bomb « Rope » gelait le jeu), sans modèle de leur genre (contres
  « Reversal … », tombés « … Pin »), sans animation trouvée.
- **Remplaçant** (`moveset_port::StandIn`, 2026-11-10) : une prise qui ne peut pas être ajoutée prend la prise de
  WWE '12 que les rosters mettent dans cette case, au nom finissant par le même mot, faite au même endroit, puis avec le
  plus de mots en commun, puis la plus utilisée ; aucune : la base. Hogan 2K14 « Right Punch 2 » → « Cena Punch 2 » ;
  son Running Leg Drop : la base.

## Corrections calculées par le Mod Maker

- **Position au sol de la victime** (aa9cc6d) : ligne 0/6 de la victime (segment 0, personne 1), **+20 u16 = animation
  au sol, 0 sur le dos, 1 sur le ventre**, calculée d'après la dernière pose (axe x du Bip vers le bas → 0, vers le haut
  → 1) ; la commande 81 du jeu (`sub_824AE820`) la lit. `svr_moves::SetDown`, aussi pour les 2K.
- **Image d'impact** : m3 0/6 ligne de l'attaquant +34 = image de la commande 73 ; les jeux 2K n'en ont que l'octet bas :
  reprise de la commande 73 (`svr_moves::SetImpact`).
- **Piste « vector »** : cap de l'axe z de Bip - 90° après le demi-tour du placement, moyenné sur ±8 images ; sa
  composante y laissée à 0.
- 2K14 pilote mollets et avant-bras par IK : `yanm_ik.h` les cuit en rotations ; événements de genres inconnus retirés.

## Formats utiles

- **Fiche WAZE WWE '12** (`misc.pac` MOVS/WAZE, 160 o) : nom +0x10, numéro u16 +0x90 ; numéro d'animation = numéro de la
  prise aux octets inversés (`newmoves13::MotionOf`). SvR 2011 : numéro u16 petit-boutiste +0x90.
- **Fiche d'animation WWE '12** (`m.pac` ZMOT/CA01..CA08, PACH : un id de tête, puis enfants par paires, pair = légère,
  impair = complète) : en-tête de 0xC0 octets (gros-boutiste ; +0x48 nombre d'entrées, +0x4C le numéro en texte, puis
  0x28 o par entrée avec x et y en texte), puis un bloc YANM + POF0 + YAT2 par entrée. Entrées : y « 0000 » attaquant,
  « 0001 » victime ; x « 0090 », « 0091 », « 0092 » = branches de contre. **Le u16 à +2 de chaque entrée = la taille du
  bloc en unités de 32 octets** (pas le nombre d'images). WWE '12 compte environ deux fois plus d'images que SvR 2011.
- **Placement** : l'attaquant de WWE '12 est rangé tourné d'un demi-tour et souvent 150 cm plus loin en -z (règle de
  Kaiko) ; mesuré sur 770 prises communes : la victime garde son repère, l'écart attaquant - victime dépend de la prise
  (catégorie SvR : octet +0x91 de la fiche SvR, puis +0x73).
- **m3.pac** (MDAT/PRM) : tableaux triés par numéro d'animation (u16 petit-boutiste en tête de fiche) : 0/1 (28 o),
  0/2 (20 o), 0/3 (8 o {animation, partie, début u16 et nombre u16 dans 0/4}), 0/4 paires {événement u16, image u16}
  (60 Hz), 0/6 (36 o), 0/7 (20 o), 0/8 listes {genre, image} finies par {0, 0} (non comprise), 5 (« SMB », 20 o).
  WWE '13 / 2K14 : 0/1 ↔ 0/11, 0/2 ↔ 0/12, 0/3 ↔ 0/14 (12 o), 0/6 ↔ 0/10 (40 o), 0/7 ↔ 0/16 (32 o).
- **SvR** : animations en banques YMKs (`svr11_entrance::detail::ReadBank`), clés (id, x, y) : y0 attaquant, y1
  victime, y50 / y51 / y57 caméra et objets, x90 / x91 contres ; MOT/BM00/n (le jeu complet). Données de prise dans
  `misc.pac WAZA/DATA` (EVT 16 o + événements 4 o à 30 images/s ; EXH 36 o).
- Banques de situations : m.pac a 23 entrées ZMVM (128 Mo), PACH de groupes entiers copiés de ZMOT/CA01-CA08 ; le jeu
  prend la première archive qui a le nom (`sub_827F29F0`, appelé par `sub_827D98D8`).

## Pièges et limites

- Prise « contre les cordes » SvR (31074, anime y60 / y61) : gèle, refusée.
- Les vieux SvR ont moins de bons modèles (à moins de 15 cm : 2011 / 2010 / 2009 / 2008 / 2007 = 80 / 40 / 23 / 12 / 7 %) ;
  2007 / 2008 les moins sûres.
- AJ Styles 2K17 : prises ajoutées finissant à 1-2 m de leur modèle (Fireman's Carry Neckbreaker, modèle Brain Chop 1).
- **Bug du jeu, pas du Mod Maker** : la Beast Bomb du Batista DLC (case 0x4E) plante sans aucun mod (sub_824A5458 ←
  sub_82481C78, lecture 0x5C) ; transmis à Gameplay le 2026-11-09.
- Aides : journal `--wwe12_dir_log=true` ; sondes `wwe12_moves_probe` (sub_827D8E30), `wwe12_down_probe`,
  `tools/dev/lancer_test_au_sol_copie.bat` ; outils d'étude hors dépôt dans `tools_local\moves_study`.

Historique complet, essais et détails : docs/archives/prises_svr11_anciennes_parties_2026-11-12.md
