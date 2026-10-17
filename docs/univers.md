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
