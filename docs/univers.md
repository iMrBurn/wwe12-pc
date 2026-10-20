# Mode Univers : cartographie (étape 0)

Conversation « WWE 12 PC (Univers) », 2026-10-16. **Rien n'a été modifié** dans le jeu ni dans le
code du portage : ce document vient de la lecture des fichiers `.pac`, de l'exe (`image.bin`), du code
traduit (`port/generated`), d'une sauvegarde et du journal `univers.log` (sonde « [uni] » d'une
session précédente, même contenu que les lignes `[game 826D4228]` de `wwe12_game_log`).

Niveaux de certitude : **sûr** (vu dans le code ou les données), **probable** (déduit, très
vraisemblable), **à vérifier** (hypothèse, il faudra une sonde en jeu).

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

1. **Offsets dans le bloc Univers** : champions de chaque titre, relations entre catcheurs, équipes,
   scénarios en cours, classements. → sonde en jeu : copier le bloc avant/après un événement connu
   (changement de champion, nouvelle rivalité) et comparer.
2. **Où et comment les NEWS sont écrites** : trouver le code qui utilise les ids 27060–27098
   (probablement passés en nombres, pas en texte) et la liste des actualités dans la sauvegarde.
3. **Détail des fiches MTDT/STYT/WCID** (champ par champ), pour pouvoir **ajouter** des matchs de
   qualification et des scénarios.
4. **Lien scénario → numéro de scène** (Act) → archive `Season/RSC/Winning`.
5. **Royal Rumble** : comment la liste des 30 participants est remplie (point d'entrée pour les
   « surprises »).
6. Correspondance mémoire ↔ fichier `SaveData.Dat`.

Ces points demandent de **lancer le jeu** avec des sondes de lecture (sans rien changer au jeu).

## 6. Ce que ça veut dire pour la suite (étapes 1 à 4)

- **Booking (titres, PPV, qualifications, Rumble)** : surtout dans le **code** (sub_826C8690,
  sub_826E3920, sub_826F1028, Rumble). Les modèles de matchs sont dans MTDT (données) : on pourra en
  ajouter si on comprend le format, sinon intercepter la génération de carte.
- **Rivalités** : relations dans la sauvegarde + scénarios STYT (données) + choix en code (826E9338).
- **Cinématiques** : scènes = données (`evt`), choix = code (826DF6D8 / 826ECE78). Réutiliser des scènes
  existantes est réaliste ; en créer de nouvelles est un très gros travail.
- **Actualités** : textes modifiables par `mods` (string.pac) ; de **nouveaux** types (qualifications)
  = code + nouveaux ids de textes.
- Idée des règles de booking dans un fichier réglable : réaliste **si** on remplace/complète la
  génération de carte (826E3920) par notre propre code natif qui lit un fichier.

---

## 7. Ce que fait WWE '13 (étudié le 2026-10-18, pour s'en inspirer)

Source : la copie de l'utilisateur (recomp WWE '13 de PromethenDev, fichiers du jeu fournis par
l'utilisateur). **Rien de WWE '13 ne doit aller dans le dépôt ni dans les zips.** On ne reprend pas son
code (autre exe) : il sert de modèle, même moteur Yuke's.

**Menus Univers de WWE '13** : mêmes numéros de textes que WWE '12 (27000…), les ajouts occupent des
numéros **vides dans WWE '12** :
- 27026–27030 : EDIT SUPERSTARS, EDIT ROSTERS, EDIT TEAMS, **EDIT TITLES**, **RESET UNIVERSE** (sous
  « EDIT UNIVERSE », 27045, qui remplace EDIT SUPERSTAR) ; 27048 FEUD MANAGER ; **27049 STATISTICS**.
- Édition des titres : 27651/27663/27665/27682/27896 **VACANT**, « No belt is assigned to this
  category », catégories MAJOR / MINOR / TAG / DIVA (27606–27609).
- Le nom interne de l'écran est `ResetUniverse` (0x8205FA54 dans l'exe de WWE '13) ; la remise à zéro
  relit les **valeurs de départ** (`CWWEUniverseStationed::BatPacReaderCallBack_SaveInit`).

**Statistiques de WWE '13** (textes 27824–27849 et 33200–33243) :
- par catcheur : victoires, **WIN%**, apparitions, temps d'activité ;
- **titres** : nombre de règnes (TIMES EARNED), **semaines de règne** (WEEKS HELD), anciens champions
  (PAST CHAMPIONS), nouveau record ;
- **Money in the Bank** : anciens gagnants, résultat du cash-in, % de victoires après cash-in ;
- **Royal Rumble** : gagnants, apparitions, adversaires éliminés, temps passé dans le Rumble ;
- **WrestleMania** : résultats, main events, % de victoires.

**Valeurs de départ de l'Univers dans WWE '12** : `pac/misc.pac`, groupe **BATL/SAVE** (même nom que
dans WWE '13) ; chargé par la classe à vtable 0x8203E7D8 (`sub_826EC0A8` ouvre « BATL/SAVE »,
`sub_826EC008` garde les entrées 2 et 3 via `sub_826D69C8` → globales 0x832315E4 / 0x832315E8).
- entrée 2 « LBTS » : équipes de départ (Kane & Big Show, Santino & Kozlov, Otunga & McGillicutty,
  Demolition, Road Warriors, Divas of Doom…) ;
- entrée 3 « HID » ;
- entrée 4 (PACH de 6) : « WUD » (fiches de départ des catcheurs), « WDR » (relations de départ),
  émissions (RAW, SmackDown, Superstars), calendrier des 13 PPV (Royal Rumble…), marques (RAW,
  SmackDown, Diva, Legend, FreeAgent…). Qui lit l'entrée 4 : **pas encore trouvé**.

**Conséquences pour WWE '12** :
- Les textes des nouvelles entrées peuvent reprendre les numéros libres (27026–27049…) avec les mêmes
  mots que WWE '13 (patch de string.pac au démarrage, comme menu.pac).
- Le **suivi des stats** doit être écrit par nous : WWE '12 ne garde ni victoires ni historique (§3.3).
  Idée : après chaque match Univers (`sub_826C3258`), ajouter une ligne dans un fichier à nous
  (Documents\wwe12\universe_stats…), et un écran STATISTICS (ImGui au style du jeu, comme GRAPHISMES).
- **Reset** : vider le bloc (`sub_826D5938`), remettre les champions Univers par défaut, puis laisser le
  jeu recréer l'Univers à partir de BATL/SAVE (à vérifier en jeu).

## 8. Tests en jeu (2026-10-18, sauvegarde de l'utilisateur mise de côté puis remise)

- **Univers vidé** (copie où le bloc U est traité comme par `sub_826D5938`) : **plantage** en entrant
  dans l'Univers, juste après le chargement de WWEU/MTDT, STYT et BATL/SAVE. Lecture de 0x0000000C dans
  `sub_826EF988` (méthode virtuelle des scènes/actes, vtable 0x8203E684). Le jeu n'a pas recréé
  l'Univers : un interrupteur « l'Univers existe » est ailleurs. Prochaine étape : comparer une
  sauvegarde neuve avant et après la première entrée dans l'Univers.
- **Ceinture vacante** (titre 1 = **WWE Championship** mis à 512/512 dans la table Univers) : pas de
  plantage ; l'écran RANKINGS affiche le titre **sans nom ni portrait** ; le jeu garde le titre vacant
  et nomme quand même un aspirant n° 1 (Sheamus), mais **ne programme aucun match pour ce titre** (ni
  émission ni PPV). Il faudra donc ajouter nous-mêmes le match pour le titre vacant. L'utilisateur
  signale aussi un Univers « un peu cassé à certains moments » (à préciser).
- Confirmé : titre 1 = WWE Championship (RAW, MAJOR), 6 = United States, 12 = Divas, 13 = Unified Tag.
- **Décision de l'utilisateur (2026-10-18)** : pas d'option « ceinture vacante » (WWE '13 ne la
  propose pas dans l'Univers, et le jeu ne programme pas de match pour un titre vacant). On garde la
  remise à zéro de l'Univers et le suivi des stats.
- **Décision révisée (2026-10-18)**, après une suggestion de joueur (dans SvR 2011 on pouvait rendre
  vacants les titres non utilisés par l'émission) : on ajoute **VACANT seulement pour les titres
  qu'aucune émission n'utilise** (titres des légendes, etc.). Les titres des émissions restent
  comme avant (le jeu ne programme pas de match pour un titre vacant). WWE '12 n'a ni texte
  « VACANT » ni bouton : à ajouter (EDIT SUPERSTAR ou notre écran). Reste à trouver où le jeu
  range « quel titre appartient à quelle émission ».

## 9. Comment le jeu crée l'Univers (test « sauvegarde neuve », 2026-10-20)

Test : sauvegarde de l'utilisateur mise de côté (puis remise, empreinte vérifiée), lancement 1 =
sauvegarde neuve sans entrer dans l'Univers, lancement 2 = première entrée dans l'Univers.

- **L'Univers est créé avec la sauvegarde**, pas à la première entrée : la sauvegarde neuve contient
  déjà les points de départ des catcheurs, les cartes, etc. La première entrée ne change que
  193 octets (dont 12 zones du bloc Univers et quelques octets du bloc 2 vers +0x576C4 / +0x584E3…,
  sans doute des drapeaux « déjà vu »).
- Création d'une sauvegarde neuve : `sub_8269E698` → `sub_82699F10` (bloc 1 remis à zéro, dont
  `sub_826D5938` pour l'Univers), puis un chargeur lit **BATL/SAVE** (`pac/misc.pac`) et appelle pour
  chaque entrée la fonction de la table à 0x83073240 : {2 → `sub_8269EF80` (équipes LBTS),
  3 → `sub_82A41578` (HID), **4 → `sub_826BE3C0` (Univers)**}.
- **`sub_826BE3C0(données, taille)`** = « SaveInit » de l'Univers (même rôle que
  `CWWEUniverseStationed::BatPacReaderCallBack_SaveInit` de WWE '13) : elle lit les 6 morceaux de
  l'entrée 4 (0 = WUD fiches de départ, 1 = WDR relations, 2 = émissions [cartes vidées par
  `sub_826D5228`], 3 = PPV, 4 = 7 réglages, 5 = marques) et les écrit dans le bloc Univers. C'est
  aussi elle qui imprime « RP Setup,Status,RosterID,No ».
- Le test « Univers vidé » plantait parce qu'il ne faisait que la première moitié.

**Recette de la remise à zéro** (à coder) : `sub_826D5938(U)` puis `sub_826BE3C0` avec l'entrée 4 de
BATL/SAVE (lue depuis misc.pac), puis remettre les champions de la table Univers aux valeurs par
défaut (table 0x8324E3A8, comme `sub_82A39E08` mais seulement pour la table Univers), enfin laisser le
jeu enregistrer. À faire hors d'un match, depuis un menu.
