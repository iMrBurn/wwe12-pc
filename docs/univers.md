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

## 10. Remise à zéro : ça marche (2026-10-20)

`port/src/native/wwe12_universe.cpp` (cvar `wwe12_universe_dev`, off par défaut ;
`tools/dev/lancer_test_univers.bat`). Testé en jeu par l'utilisateur, sur sa sauvegarde
(copiée avant, remise après) : F8 sur un écran Univers → retour au tout début (calendrier,
cartes, champions), y compris après avoir simulé jusqu'à un PPV.

- Le jeu lit l'Univers de départ en entrant dans l'Univers : `sub_826EC008(this, pack)` reçoit le
  **pack BATL/SAVE ouvert** (une poignée du lecteur, sur la pile du chargeur, pas des données) ;
  l'entrée d'id 4 est l'Univers de départ, compressé en BPE. `sub_827D8E30(pack, id)` = index,
  `sub_827D9528(pack, index)` = copie décompressée (6 756 octets, un PACH de 6 entrées), libérée par
  `sub_827D2EA0(copie, 0)`. On en garde une copie (mémoire de jeu à nous, SystemHeapAlloc).
- La remise à zéro : `sub_826D5938(U)`, `sub_826BE3C0(copie, taille)`, champions Univers par défaut
  (table 0x8324E3A8 → 0x83226508). Lancée depuis le crochet des textes (`sub_821A4400`, fil du
  jeu) quand le texte demandé est un texte Univers (27000–27999).
- Sonde : le menu Univers (27041–27047) est construit par `sub_8255EC88` (texte demandé via
  0x824CA80C ← 0x8255ED44) ; les cartes de matchs par 0x82556A70 ← 0x8255778C.
- Prochaine étape (2) : un vrai choix « RESET UNIVERSE » (EDIT SUPERSTAR → EDIT UNIVERSE avec un
  sous-menu, comme WWE '13), avec confirmation, à la place de F8.

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

Dans l'ordre décidé : 1) tenue de chaque catcheur dans l'Univers (une précise ou Random, comme 2K14) ;
2) SAVE sur une carte de match modifiée avec MATCH CREATOR (WWE '13) au lieu de la lancer tout de suite ;
3) moins de matchs par émission (8 cases par émission dans la sauvegarde, donc 8 au plus) ;
4) catcheurs non suivis (« Vous ne pourrez pas suivre la progression de cette Superstar… », ex.
Christian) : l'Univers n'a que 180 fiches, toutes prises (50 créés + 130) : échanger un catcheur suivi
contre un non suivi ; 5) weekly n'importe quel jour et plusieurs PPV par mois (calendrier à cartographier).

Comme WWE '13 (captures de l'utilisateur, 2026-10-21) : CALENDRIER où l'on peut modifier ou supprimer une
émission ou un PPV existant (X MODIFIER, Y SUPPRIMER) et ajouter une émission majeure/mineure ou un PPV
n'importe quel jour, dimanche compris (X AJOUTER sur une case vide) ; CLASSEMENTS dont on change l'ordre
à la main (X MODIFIER). Calendrier = booking : 1.0 sauf si l'étude montre que c'est simple (décision de
l'utilisateur). Classements à la main : prévu pour la 0.9.

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

- **Paramètres d'aspirant (MAJEUR / MINEUR / TAG / DIVA par Superstar)** : WWE '12 n'en a pas dans sa
  sauvegarde. Le jeu décide « peut-il prétendre à ce titre » par `sub_82A39EF8(id, titre, 1)` (seulement
  homme/femme), appelé par les classements de l'Univers (`sub_826CFDA8` solo, retour 0x826CFF0C ;
  `sub_826D18E8` équipes, 0x826D1AE0) et par un filtre du booking (`sub_826E1380`, vtable 0x8203E3B4 slot 1,
  titre à +8, retour 0x826E13A8), mais aussi par TITLE MANAGEMENT, MATCH CREATOR et le match (non touchés).
  Le genre d'un titre = sa place dans les 4 titres de son émission (`sub_826CC180(émission, out[4])`,
  émission k = `sub_826CCB88(vue, k)`, vue de 428 octets `sub_826CDC48` / `sub_826CE098`). **Codé et testé le
  2026-10-27** (copie de la sauvegarde : MINOR = OFF pour un catcheur → il disparaît du classement US, reste au WWE) : dernière ligne de la boîte des tenues (Y sur la fiche) →
  « CONTENDER SETTINGS... » → MAJOR / MINOR / MORE..., TAG / DIVA, chaque ligne ON/OFF (textes du jeu 27606-27609,
  27613/27614 ; question = notre texte 27048) ; fichier `universe_contenders.txt` (« id masque »).
- **Options scénaristiques** (rivalités auto, tag teams, foule, blessures) : WWE '12 n'a aucune option de ce
  genre (aucun texte). Pistes dans le code : la mise hors service d'un Superstar (blessure) est
  `sub_826CEA88(vue, id, 1)` (bit 31 de la fiche Univers +92, semaine en +96, et il perd sa place d'aspirant
  n° 1) ; `sub_826E7A10` = blesser, `sub_826E7A70` = guérir (actions de scénario), autres appelants
  `sub_826C3388`, `sub_826C55E0`. Une option « BLESSURES OFF » = refuser l'appel avec 1 (la NEWS « Serious
  Injury », texte 27093-27094, s'écrit peut-être ailleurs : à vérifier). Rivalités et tag teams : non étudiés.
- **Statistiques** (titres : plus de règnes / de semaines ; émissions : anciens champions ; MITB : vainqueurs,
  cash-in ; Rumble : vainqueurs, WrestleMania) : à noter nous-mêmes. Changement de champion = toujours
  `sub_82A3A060(titre, catcheur, place)` (appelants : TITLE MANAGEMENT `sub_82564DD0`, résultats
  `sub_826C6530`, `sub_825396F8`, `sub_82684C00`, `sub_826D4030`, `sub_82DEA418`) ; semaine en cours =
  [vue+4] + 56772 (U + 0xDDC4) ; Rumble = `sub_826C8520` ; MITB = `sub_826F1028`. Le plus long : l'écran.
- **Calendrier (ajouter/supprimer une émission, PPV et organisateur)** : non (booking ; une émission vide fait
  planter les PPV, §20).
- **Nombre de matchs par émission** (MATCHES 5 de 2K14) : cartes de 8 cases au plus (§3.2), génération
  `sub_826C8690` non étudiée.

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

Sur la fiche d'un Superstar dans l'Univers, **RB** (barre du bas : « RB MORE ») passe de la fiche à
**CONTENDER SETTINGS** puis **STATISTICS** puis de nouveau la fiche ; **LB** dans l'autre sens ; **B** revient
à la fiche. Les pages réutilisent le vrai écran d'accueil de RANKINGS (4 ceintures de l'émission du
Superstar), comme les pages de 2K14 :
- en-tête (élément 830) : « NOM - CONTENDER SETTINGS / STATISTICS » ;
- ligne du champion de chaque titre (842/854/866/881 et leurs copies visibles 843/855/867/882) :
  ASPIRANT = sa place dans le classement (`sub_826D2508`), CHAMPION, OFF s'il est écarté, « - » si le titre
  n'est pas pour lui (`sub_82A39EF8`, ex. Divas pour un homme) ; haut/bas choisit, X active/désactive
  (rouge = ligne choisie) ; STATISTICS = « HELD n - n WEEK(S) » (`universe_stats.txt`).

Comment (écran actif du menu Univers M, r3 de `sub_82545A70`) : M+288 = panneau affiché, choisi dans la liste
de gauche par `sub_82544EE8` (0 MATCH CARDS +260, 1 SCHEDULE +276, 2 RANKINGS +264, 3 NEWS +280, 4 EDIT
UNIVERSE +268, 5 EDIT TEAM +272, 6 EDIT BRAND +284) ; changer = cacher l'ancien (`sub_82545070`, vtable +92
avec sa chaîne +256), M+288 = le nouveau, le montrer (`sub_82545008`, vtable +88). Pour nos pages : afficher
le panneau RANKINGS (R = M+264, R+2352 = émission 0/1, rafraîchi comme ses LB/RB : `sub_82552768`,
`sub_82551CB8`, `sub_82551F48`, `sub_82552048`), la fiche garde les touches (son +224 = 2, celui du menu
M+28+224 = 0). Au retour il faut remettre ce que le réaffichage d'EDIT UNIVERSE efface :
- **M+296** (2 = « l'écran d'un panneau est ouvert », sinon le menu ne reprend plus la main après la liste) ;
- la liste des Superstars (écran vtable 0x82027970 = panneau +412) : son défileur à +1760 ({+0 ligne,
  +4 première ligne affichée, +8 lignes visibles, +12 lignes, +16 boucle}, `sub_8254DE48`), puis sa fonction
  « ligne changée » `sub_8258D240` (avec +88 = 0) qui remet le Superstar courant (`sub_82DEA390`) et l'en-tête ;
- la fiche : `sub_825ACEC0` (son affichage) et `sub_825AAD38` (sa barre de boutons) ;
- les +224 de la fiche et du menu tels qu'avant.
Testé (télécommande `wwe12_remote_pad`, sur la copie) : RB → aspirant (CHAMPION / RANK 8), bas + X → OFF
(enregistré), RB → statistiques (HELD 1), RB → fiche, B → liste (curseur gardé), B → menu, RANKINGS normal.

## 24. Page STATISTIQUES en tableau, comme 2K14 (2026-10-27, testé sur la copie)

LB depuis la fiche (ou RB depuis la page aspirant) : la page détaillée d'un classement (RANKINGS puis A)
sert de tableau. Ouverture `sub_82553770(R+28)` (R+56 = place du titre, ici 0), fermeture `sub_82552D40(R+28)`
(son B) ; pages de liste R+56 = 1, 2 (R+60 = nombre de pages), 12 lignes : 1020 + k (libellé), 1032 + k
(colonne du rang, vidée), 1044 + k (valeur ; texte en +200, drapeau 0x10 de +228 effacé). Le jeu redessine
les lignes (`sub_82552A78`) et le haut (`sub_82552240`) : nos crochets remettent nos textes juste après. Haut :
897 = « STATISTICS », 898/899 vidés, 911 vidé, 912 = le nom du Superstar, 913-915 (portrait du champion) cachés.
Lignes : TITLES (DÉTENU - SEMAINES par titre de son émission, « -- » si pas pour lui, TOTAL), ROYAL RUMBLE
(victoires, participations), WRESTLEMANIA (matchs, main events, victoires, taux), MONEY IN THE BANK
(victoires, taux après cash-in) ; sections en rouge ; haut/bas = page 1/2 du tableau. Les blocs Rumble,
WrestleMania et MITB lisent des lignes « rumble / mania / mitb / cashin » de `universe_stats.txt` qui ne sont
**pas encore écrites** (prochaine étape : le résultat des matchs de l'Univers).

## 25. Résultats des matchs notés pour les statistiques (2026-10-27, compilé, pas encore vu en jeu)

Tous les résultats de l'Univers (match joué ou simulé) passent par `sub_826CEF28(vue, émission, -, match,
place du gagnant, place du perdant, 1)`. Émission : 0-9 les shows de la semaine, 10-22 les PPV dans l'ordre du
jeu (10 Royal Rumble, 11 Elimination Chamber, **12 WrestleMania**…). Matchs de l'émission : `sub_826CCC38(vue,
émission)` +4 = fiches de 104 octets, +8 = nombre (même lecture que `sub_826D0B90`). Fiche : +0 type de match
(`sub_82A2FA18(type) == 1` = Royal Rumble), +2 nombre de places (`sub_826D4F90`), +3 = 2 une fois joué. Place :
`sub_826D4958(fiche, sortie, place)`, sortie +0 Superstar (u16), +3 son équipe (Rumble : sa place, +5 = gagnant) ;
gagnant 6 (40 au Rumble) = pas de gagnant. Le Rumble garde son vainqueur à U + 51000 (`sub_826CCA20`).
Notre crochet lit tout ça avant l'original et ajoute à `universe_stats.txt` : « rumble <id> <gagné> <jour> »
pour chaque participant du Royal Rumble, « mania <id> <main event> <gagné> <jour> » pour chaque participant
d'un match de WrestleMania (main event = dernier match de la carte ; gagné = même équipe que le gagnant).
Journal : ligne « [univers] result: show … ». Reste : Money in the Bank (gagnant de la mallette, cash-in) pas
encore trouvé.

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

Demande de joueurs (Discord) : après un match de l'Univers, le jeu lance tout seul le match suivant de la carte.
Sondes (`wwe12_universe_probe`, lignes « flow: ») sur 3 matchs joués :
- Le jeu enchaîne des étapes créées par la fabrique `sub_823DCC38(-, id 1..34)` (table de sauts à 0x823DCC70 +
  4 × (id − 1)), pilotées par un « propriétaire » (vtable 0x8200A8D0, `sub_823894C0` : +32 l'étape en cours, finie quand
  son +4 = 5 ; créateur vtable +56 → +40 objet, vtable +4). Après le résultat : ids 17, 18, 28, puis l'après-match
  (vtable 0x8200D310, `sub_823DDBB0` qui note le match suivant U+5068 via `sub_826BF4C0` / `sub_826BAF28`), puis id 0 = fin.
- Un nouveau mode démarre alors depuis `sub_822B3420` (gestionnaire de modes : this+8 objet, vtable 0 appelé avec
  this+12 = mode demandé) → `sub_822B5598` → `sub_82389E20` → id 1, puis 24, 20, 2, 25, 3, 5, 6, 7, 23, 8, 9, 10, 11
  (11 = préparation du match suivant, vtable 0x8200DF18, `sub_823F2E60` → `sub_826C8638`). L'écran MATCH CARDS ne
  tourne pas entre les deux (`sub_82547EE0` muet).
- Reste à trouver qui remplit this+12 (le mode demandé) à la fin de l'après-match : c'est là qu'il faudrait demander
  le mode « menu Univers » au lieu de « match suivant » quand l'option est active. Piste suivante : sonde sur les
  écritures de this+12 de `sub_822B3420` (ou sur `sub_822B5598` avec la valeur demandée et la pile).

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
