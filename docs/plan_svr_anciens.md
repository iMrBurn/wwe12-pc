# SvR 2007 à 2010 dans le Mod Maker : étude (2026-10-31)

Demande de l'utilisateur : porter dans WWE '12 les catcheurs et les arènes des jeux d'avant SvR 2011. Il a les ISO
Xbox 360 de SvR 2007, 2008, 2009 et 2010 (et 2011) dans `Desktop\WWE ISOS`. Étude seulement : rien n'est codé.

## Ce qu'il y a sur les disques

| Jeu | Titre | Catcheurs (`pac/ch`) | Arènes (`pac/bg`) |
|---|---|---|---|
| SvR 2007 | 545107E0 | 81 `chNN.pac` | 8 `bgN.pac` (plusieurs scènes par fichier) |
| SvR 2008 | 5451080B | 74 `chNN.pac` | 45 `bgNN.pac` |
| SvR 2009 | 54510826 | 86 `chNNN.pac` | 45 `bgNN.pac` |
| SvR 2010 | 54510844 | 86 `chNNN.pac` | 52 `bgNN.pac` |

## Ce que font déjà les outils de Kaiko (`Desktop\SVR 2011 KAIKO\Mod Maker Tools`, Python)

Ils portent ces jeux **vers SvR 2011** (son portage PC) :

- **Catcheurs.** `svr10_char.py` : SvR 2010 = même format que 2011 (EPK8 / EMD / PACH / JBOY, même squelette) ;
  seules l'animation du visage (0x64, « JUDE », remplacée par la copie générique de 2011) et la taille du portrait
  (0xc8 / 0xc9) changent. `svr08_char.py` (2008, et 2009 par le même chemin) : EPAC / PACH / JBOY, même squelette et
  mêmes unités, mais anciens shaders et formats de sommets, un seul jeu de textures, pièces de tenue en JBOY à part :
  table de nœuds de 2011, palettes par nom, blocs de shaders de 2011, textures rangées comme 2011, Mk tiré de l'AO,
  pièces fusionnées. `svr07_char.py` : comme 2008, mais une texture par partie du corps (46 pour Steve Austin) cuites
  sur les planches de 2011 (couleur 2048 × 1024, normales 1024 × 1024, Mk 512 × 256).
- **Catcheur SvR 2010 complet** (`svr10_superstar.py`, 988 lignes) : modèle, rendus (DLC_HD.pac SSFB / SSFC), thème
  (music1.yaf, XMA2), vidéo d'entrée (Bink 320 × 320 telle quelle), notes, capacités, prises (`svr10_moves.py`,
  1461 lignes : table des cases 2010 → 2011, prises manquantes portées), entrée de 2011 du même nom.
- **Arènes** (`arena_port.py`) : `svr2010_convert` (le fichier presque tel quel) et `svr2008_convert` pour 2007,
  2008, 2009 (2007 : une scène d'un fichier à plusieurs scènes) ; elles prennent dans SvR 2011 la foule, les images
  de table, l'écran VS et l'éclairage (d'où le besoin du disque SvR 2011). Statut chez Kaiko : « ready », RAW testé.
- **Entrées** : `svr08_entrance.py` (EVP / EVT de 2008 → 2011, 674 lignes), `svr10_entrance.py`.

## Le chemin pour WWE '12

Le Mod Maker porte déjà **SvR 2011 → WWE '12** : catcheurs (`port_svr.h`, vu en jeu : CM Punk) et arènes
(`port_svr_arena.h`, vu en jeu : WM 26, Breaking Point). Donc :

**SvR 2007-2010 → format SvR 2011 (méthode de Kaiko) → WWE '12 (notre portage existant).**

Il n'y a rien à refaire pour la seconde moitié. La première moitié doit passer en C++ dans le Mod Maker (les joueurs
n'ont pas Python), comme on l'a fait pour `yanm_decode.py` et `rig_map` : **code tiers, il faut l'accord de
l'utilisateur et le crédit à Kaiko** (sa permission connue couvre son travail D3D11 ; pour ces outils, lui
redemander ou s'en tenir aux idées).

## Ordre proposé et effort

| Étape | Contenu | Effort |
|---|---|---|
| 1 | Page « Mes disques » : reconnaître SvR 2007-2010 (numéros de titre ci-dessus) | court |
| 2 | Catcheurs SvR 2010 → WWE '12 (visage et portrait seulement) | 1 séance |
| 3 | Arènes SvR 2010 → WWE '12 | 1 séance |
| 4 | Catcheurs SvR 2008 / 2009 (shaders, textures, pièces de tenue) | 2 séances |
| 5 | Arènes SvR 2007 / 2008 / 2009 | 2 séances |
| 6 | Catcheurs SvR 2007 (textures cuites sur planches) | 1-2 séances |
| 7 | Entrées et prises (2008, 2010) | plus tard, gros |

Chaque étape : vérifiée hors du jeu, puis un essai en jeu par l'utilisateur.

## Questions pour l'utilisateur

- D'accord pour reprendre en C++ la méthode des outils de Kaiko (avec crédit) ? Faut-il lui redemander ?
- Par quoi commencer : catcheurs ou arènes ?
