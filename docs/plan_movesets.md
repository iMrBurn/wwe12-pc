# Prises et entrées des catcheurs portés : état actuel (résumé du 2026-11-12)

But (plan du 2026-10-27) : un catcheur porté d'un autre jeu (SvR 2007-2011, WWE '13, 2K14-2K17) garde **son moveset,
son entrée, sa scène de victoire, sa musique et ses vidéos**, comme le Mod Maker de SvR 2011 de Kaiko (autorisation de
lire et d'adapter son code, avec crédit).

## Le plan est fait

| Étape | Code | État |
|---|---|---|
| 1. Moveset traduit par le nom des prises | `port/src/modmaker/moveset_port.h` ; ligne `moves=OFF:NUMÉRO,...` ; jeu : `superstar_pack.h` | fait, vu en jeu |
| 2. Entrée d'un catcheur de WWE '12 | ligne `entrance=NUMÉRO` | fait |
| 3. Prises absentes de WWE '12 (animations portées) | `newmoves13.h`, `svr_moves.h`, `model_moves.h`, `m3_params.h`, `zmvm_add.h` | fait, **coupé par défaut** (`wwe12_added_moves`) ; état dans `docs/prises_svr11.md` |
| 4. Entrées portées | `entrance13.h`, `yanm_ik.h` ; jeu : `MountEntrances` (`wwe12_arena_pages.cpp`) | fait : WWE '13, 2K14-2K17 vus en jeu |
| Musiques | `theme_source.h`, `theme_convert.h`, `wwise.h` | fait ; depuis la 1.3 en Wwise Vorbis (`docs/mod_maker.md`) |
| Victoires | `entrance13::Victory` | fait : WWE '13 et 2K14 vues en jeu |
| Vidéos | `video/bink2_decode.c`, `video/bink2/` | fait |

## Moveset (étape 1)

- **Fiche de prises** `pac/ch/chEtc.pac` CHAR/PRO, 1 072 o par catcheur, u16 gros-boutistes ; prises de +0x00A à
  +0x1C0. La sauvegarde garde sa copie (1 068 o). **Bloc d'entrée** à +0x1D4 (répété à +0x1E0), nombres de 16 bits
  **petit-boutistes** : musique, vidéo, entrée, deux entrées de champion (Cena 139, 139, 139, 602, 602).
- **Noms des prises** : `misc.pac` MOVS/WAZE, fiches de 160 o depuis 0x430, nom à +0x10, numéro u16 à +0x90 ; 2K14-2K16 :
  `createmode.pac` MOVS/WAZE. Le numéro de la prise lu dans l'autre sens d'octets = numéro de l'animation.
- **Correspondance des cases** calculée sur les catcheurs des deux jeux (paires de cases au même nom de prise, au moins
  15 %, méthode de `moveset_maps.py`), puis aussi par les catcheurs de même nom, les mêmes numéros de prise (WWE '13 et
  les 2K gardent la numérotation de WWE '12), hors de l'ordre ; noms comparés mot à mot ; prise de WWE '12 acceptée si
  ses bits de catégorie vont dans la case.
- **Couverture (2026-11-05)** : part des 210 cases variables remplies (prises retrouvées) : SvR 2007 15 % (57 %),
  SvR 2008 17 % (55 %), SvR 2009 24 % (66 %), SvR 2010 46 % (65 %), SvR 2011 75 % (95 %), WWE '13 74 % (93 %), 2K14
  73 % (95 %), 2K15 72 % (93 %), 2K16 52 % (96 %).
- Mod Maker (NOUVEAU CATCHEUR, PRISES ET ENTRÉES) : « N prises sur M passent », et pour les autres pourquoi. Case
  « Prises ajoutées (expérimental) » décochée par défaut.

## Entrées (étape 4)

- **WWE '12** : `pac/evt/Nyujyo0-9,A.pac`, groupes EVP (script, cm) et EVZ (pistes YANM + YAT2 à 60 Hz) ; tranches
  (`entrance13::Group12`, `File12`) : Nyujyo0 EVP1 100.., Nyujyo1 EVP2 137.., Nyujyo2 EVP3 179.., Nyujyo3 EVP4 272..,
  Nyujyo4 EVP5 328.., Nyujyo5 EVPE 480.., Nyujyo6 EVPF 512.., Nyujyo7 EVPG 532.., Nyujyo8 EVPH 552.., Nyujyo9 EVPI
  600..855, NyujyoA EVPK 50..55 ; variantes 10NNN..14NNN dans le même groupe ; noms sans zéros (« 179 »).
- **Jeu** : `superstar/entrance/EVP_n.bin`, `EVZ_n.bin`, `entrance_own=n` ; ajoutés au bout de copies servies des
  `NyujyoN.pac` (`Documents\wwe12\entrance_slots`, cvar `wwe12_entrance_mods`) ; le crochet du répertoire
  (`sub_827F29F0`) répond à `/EVPx/n` en empruntant une entrée du même fichier. Fiche PRO +0x1D8 = n.
- **Pièges réglés** : chaque morceau ajouté est rangé **compressé en BPE** comme l'entrée dont il emprunte la place
  (GMEV `gm.pac` et PCE6 `prtcls.pac` tels quels) ; la **queue** des scripts de WWE '12 (type 1 musique 999, type 8
  vidéo 999) est ajoutée (`entrance13::AddTail12`) ; victoires : un enregistrement type 8 (`AddVictoryTail12`) ;
  objets 2K14 « 25000254_A00_def » coupés après 5 chiffres (`NameTracks12`) ; jambes de WWE '13 (cible IK) résolues
  par `yanm_ik.h` (pied à moins de 3 mm de sa cible) ; 2K15-2K17 : cases de 788 octets → 212 (`Evp788To212`), YANM
  v5 → 4, blocs mode 4 vidés, enregistrement vide (96 o) par objet sans piste ; PCE6 de 2K16 / 2K17 font planter
  WWE '12 (sub_824DE430) : ceux de 2K15 / 2K14 / WWE '13, sinon de la base ; numéro déjà pris : renuméroté ;
  accessoire introuvable : un autre à sa place, caché.
- **Objets** (`pac\evt\evtobj.pac` `SOBJ/<modèle>`, `SOBJ/LIST`) : `entrance13::Objects` prend ceux que WWE '12 n'a pas
  (`superstar/objects/`), ajoutés en empruntant la place d'un objet numéroté (emprunter LIST faisait planter). Objets
  des prises (enfants JBOY 0x10-0xFF, Mr. Socko) repris par `port13_star::PortAttire`.
- **Victoires** : WWE '13 / 2K14 `Winning00-03.pac` n° 6000 + numéro → `superstar/victory/EVP.bin`, `EVZ.bin` ; servies
  dans `pac/evt/Winning0.pac` sous `/EVPA/` et `/EVZA/` n° 6000 + la place du catcheur (`superstar_slots.txt`).
- **Rangement 2K** : 2K15 `pac\evt\Entrance00-10.pac` ; 2K16 / 2K17 un pac par scène `pac\evt\NNNNN.pac` (victoire
  06NNN), script `pac\gimmick\gmevNNNN.pac`, effets `pac\particle\pce6NNNN.pac`, accessoires `pac\prop\NNNN.pac`,
  lumières `pac\evt\light\elNNNNN.pac`. Numéros dans la fiche : entrée 2K14 PRO +580, 2K15 +584, 2K16 +596 ; vidéo
  2K15 +580, 2K16 / 2K17 +592.
- **Vu en jeu** : Mankind (WWE '13, entrée, musique, victoire), Ultimate Warrior (2K14, entrée et victoire avec musique,
  foule, titantron), le Godfather et ses compagnes (objets), Seth Rollins (2K15), Kevin Owens (2K16), AJ Styles (2K17).
- **Vérif hors jeu** (2026-11-02) : WWE '13 234 entrées OK, 2K14 207 ; les autres ont un catcheur absent (partenaire,
  manager), un accessoire inconnu ou sont des entrées spéciales sans nom.

## Musique et vidéos

- **Musique** : `theme_source.h` trouve le thème (WWE '13 `Music.pck` PRO +576 ; 2K14 `Game_Sound_E_S.pck` +
  `Game_Sound_M.pck` ; 2K15 +576 ; 2K16 / 2K17 +588 ; SvR 2008-2010 `music*.yaf` ; SvR 2011 `Music.pck` ; SvR 2007 XACT).
  Le Mod Maker le décode avec vgmstream (outil du joueur, rien n'est livré) → `superstar/theme.wem` ; le jeu fait le
  paquet `sound\DLC_NN.pck` (banque `Music_DLC_NN`) et `pac\sound_dlc_NN.pac` (enregistrement PCKH en **vrai flux zlib**,
  `ZlibRecord`) dans le pack WWE12PCMODS ; numéro de musique = numéro de l'entrée propre, sinon 900 + sa place. Le
  Vorbis de WWE '13 fait planter le décodeur de WWE '12. Le Mod Maker propose tous les thèmes trouvés. Entendu en jeu
  (Mankind, MVP SvR 2010 « nickel »).
- **Vidéos** : notre lecteur Bink 2 (patch FFmpeg-devel de Paul B Mahol, LGPL 2.1+) lit 2K14 (KB2f) et 2K15-2K17 (KB2i) ;
  WWE '13 (KB2d) : `video/bink2/bink2d.inc`, notre propre C, branché (titantrons NNN.bik2, ruban et mur de son
  evtmov.pac) d'après la section du 2026-11-01. Vidéo 2K14 convertie à 320 × 320. RAD Video Tools / Librempeg en secours.
- **Graphismes par catcheur** (WWE '12) : `movies\titantron\NNN.bik` (haut 320 × 220 titantron, bas 320 × 100
  minitron), `pac\evt\evtmov.pac` `MVSB/M%03d` (ruban, Bink 1 1024 × 32) et `MVSW/M%03d` (mur, 256 × 256) ; un
  catcheur ajouté reçoit « non » au crochet de `sub_82A31578` : ceux de l'arène (vu en jeu). Vidéos des catcheurs ajoutés
  aussi servies depuis `Documents\wwe12\pack_movies`.

## Tenues portées

- Ressorts (entrées `60` SpringHairSetting et `61` collisions) repris, os `kami_*` gardés ; cheveux et rubans de
  Warrior balancent (vu en jeu).
- Jupettes des bottes de Warrior résolues (vu en jeu, 2026-11-01) : `Reskeleton(..., skeleton)` ajoute au squelette
  d'un autre modèle que le corps les os du corps qui lui manquent ; ficelles des genoux soudées au mollet.

## Ce qui reste (d'après le texte)

- Entrées « d'une seule prise » de 2K15 (~61, Sami Zayn 478) : jouées sans la foule, à découper en segments.
- Chargement très long des pistes 2K (~30 s) ; renders des 2K à recadrer.
- Pyro de SvR 2007-2009 : ancien format, non portable.
- Accessoires et autres catcheurs absents des entrées (voir la vérif hors jeu).

Historique complet, essais et détails : docs/archives/plan_movesets_anciennes_parties_2026-11-12.md
