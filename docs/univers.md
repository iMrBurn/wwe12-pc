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
