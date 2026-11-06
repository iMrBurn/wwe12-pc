# Les corrections des 2 mises à jour officielles (Title Updates) de WWE '12

> **État au 2026-11-06**
> - Étude faite (lecture seule, 2026-11-03 / 04). **Rien codé, rien testé en jeu.**
> - **Décision de l'utilisateur (2026-11-05) : voie B**, c'est-à-dire refaire nous-mêmes les corrections utiles en
>   crochets sur le code du disque, chacune avec son réglage. Pas de code généré depuis le xex de la TU.
> - **Version visée : 1.2**, en même temps que les registres locaux et le patch `volatile` (une seule
>   régénération). Ordre et contrôles : `docs/plan_1_2_codegen_tu.md`.
> - À faire en premier (1.2) : reproduire en jeu les gels de finisher (§2.7) et la perte des prises de DLC 3-4
>   (§2.1), puis les correctifs.
> - L'ancienne §5 « Suite possible » est archivée dans `docs/archives/title_update_corrections_anciennes_parties.md`.


Étude du 2026-11-03, session Gameplay, **lecture seule** : aucun correctif écrit, jeu jamais lancé. But : voir ce
que THQ a corrigé dans le code des mises à jour 1 (TU1) et 2 (TU2), pour le refaire nous-mêmes dans le port (comme
le correctif des répliques de Lawler, `wwe12_sound_tables.cpp`). Notre port tourne sur le jeu du disque (v0.0.0.4).
**Rien de ce document n'est corrigé chez nous, sauf mention contraire. Aucun correctif sans le go de l'utilisateur.**

## 1. Méthode

- Les paquets extraits par Main : `work/tu_study/tu1_files`, `tu2_files` (hors dépôt).
  - TU1 = v0.0.1.4 (exe daté du 16/11/2011).
  - TU2 = v0.0.2.4 (6/1/2012).
  - TU2 contient tout : il s'applique au disque, pas à la TU1.
- Images patchées faites avec **XexTool** (Team-Resurgent), déjà sur le Bureau de l'utilisateur. Commandes :
  `-p default.xexp -o p.xex default.xex`, puis `-b image.bin p.xex`.
  - Contrôle : l'image TU2 obtenue a le même MD5 que `patched.bin` d'octobre, et l'image du disque le même que
    `image.bin`.
  - Image TU1 : `work/tu_study/patched_tu1.bin` (hors dépôt).
- Comparaison fonction par fonction, avec la table `.pdata` de chaque image.
  - Attention : dans l'image du disque, `.pdata` est à 0x13D000, pas à 0x13D200. L'étude d'octobre (`fn.py`) lisait
    les deux images à 0x13D200, ses chiffres (352 changées) sont donc un peu faux.
  - Une fonction est « la même » si son code est identique une fois masqués les adresses et les offsets de données.
  - Les fonctions restantes sont appariées dans l'ordre du binaire (même voisinage, ressemblance ≥ 50 %).
  - Puis on filtre le bruit : le recompilateur de THQ a tout déplacé, et l'ordre des instructions et les registres
    changent sans que rien ne change vraiment.
  - Scripts : `work/tu_study/tools2026_11_03/` (`cmp_tu.py` compare, `nd.py` diff lisible, `where.py` adresse
    disque → TU1 → TU2).

| | TU1 (contre le disque) | TU2 (contre la TU1) |
|---|---|---|
| fonctions | 47 188 → 47 202 | 47 202 → 47 201 |
| même code (déplacé) | 47 003 | 47 000 |
| changées | 178, dont **140 vraiment** (le reste = bruit de recompilation) | 185, dont **159 vraiment** |
| nouvelles / supprimées | 21 / 7 | 16 / 17 |

Toutes les adresses ci-dessous sont celles du **jeu du disque** (les nôtres), sauf « nouvelle » (adresse dans la TU).

## 2. Par sujet

### 2.1 Movesets / CAW : les prises des DLC 3 et 4 (TU1) — le correctif le plus net

- **Ce que la TU1 change :** les listes de banques d'animations par catégorie de prise passent de **47 à 49 entrées** :
  - `/ZMOT/CA09` et `/ZMOT/CA10` sont insérées après `ATTR` ;
  - ce sont les banques des packs DLC 3 et 4 (`docs/plan_movesets.md`).
- **Fonctions qui le font :**
  - `sub_82460680` (liste `/MVMT/*`) et `sub_82460880` (liste `/ZMVM/*`) : les deux chaînes sont ajoutées ;
  - `sub_82462080`, `sub_82461128` : chargement des banques, avec beaucoup de nouveaux numéros d'animations ;
  - `sub_82462798`, `sub_82462960`, `sub_82462B80` et `sub_82DD4F00` / `82DD5068` / `82DD5600` / `82DD5798` /
    `82DE1780` : la borne 47 passe à 49 ;
  - `sub_824865C0` (même groupe) : ses appels changent ;
  - `sub_829D8148` : les listes `/CAV/*` de Create an Entrance reçoivent aussi CA09 et CA10.
- **Lecture probable :** sur le jeu du disque, une prise de DLC 3 ou 4 n'a pas de catégorie connue dans ces listes.
  D'où la note de patch « movesets des CAW réinitialisés » (un CAW qui utilise une prise de DLC).
- **Précision (2026-11-04, hors jeu) :**
  - 47 est le nombre de banques d'**aperçu** de l'éditeur de moveset : un tableau de 47 pointeurs dans
    l'objet `[0x8317AD50]+412`, chargé par `sub_82462080`. 47 sert aussi de « pas de banque » :
    - `sub_8244F218` donne la banque d'une prise ;
    - `sub_82DD4F00` saute l'aperçu si elle vaut 47.
  - La TU1 insère CA09/CA10 aux places 23-24 : toutes les banques suivantes sont renumérotées (+2), et
    `sub_82461128` reçoit une table de numéros d'animations des DLC (pile +224 octets).
  - Ce qu'on voit dans ce code : sur le disque, une prise de DLC 3-4 (ou de nos CA11+) **n'a pas d'aperçu** dans
    l'éditeur. **La perte de la prise à la sauvegarde n'est pas prouvée** par ce code : il faut l'essai en jeu.
  - **Pas de correctif court** : il faudrait renuméroter les banques dans une quinzaine de fonctions et agrandir un
    tableau sur la pile. Rien n'est préparé.
- **Chez nous :** non corrigé (nous tournons sur le code du disque).
  - **Cela touche aussi nos mods :** le Mod Maker met ses prises dans les banques CA11 et plus. Le jeu les charge
    (boucle CA00-CA19), mais ces listes de catégories ne les connaissent pas. Cela expliquerait des soucis de
    moveset avec les prises ajoutées.
  - À vérifier en jeu avant d'y toucher.
- **Difficulté :** moyenne. Il faut agrandir des tableaux de 47 à 49 (ou plus) dans 10 à 15 fonctions : les
  remplacer par crochet, ou corriger la borne et la table. Ces tableaux sont sur la pile, il faut donc réécrire les
  deux listes.

### 2.2 Univers (TU2)

- **Les CAW comptent dans l'effectif des émissions.** Le test « Superstar créé ? » (`sub_82A330D8(id) == 1`,
  ids < 50) est **retiré** dans trois fonctions :
  - `sub_82583B78` : marques de l'émission dans EDIT BRAND ;
  - `sub_82599DB0` : contrat ;
  - `sub_826D3B58` : peut-il quitter son émission.

  Sur le disque, un CAW n'est donc pas compté dans le minimum de 15. C'est sans doute la note « Univers et
  Superstars au-delà de 50 / CAW ».
  - **Chez nous :** notre recomptage de `sub_82583B78` (`wwe12_universe.cpp`, minimum réglable) **écarte encore les
    CAW**, comme le disque.
  - **Difficulté :** facile. Côté notre code, il suffit de ne plus appeler `sub_82A330D8` dans ce recomptage. Côté
    jeu, il faut 3 crochets qui font croire « pas un CAW » à ces trois appels-là.
- **Contrôles de bornes ajoutés** (index < 23 avant d'accéder à un tableau de fiches de 964 octets, à U + 1492 et
  U + 23664). Fonctions : `sub_826E1E60`, `826E1F08`, `826E21A8`, `826E23C0`, `826E24F8`. Ce sont des protections
  contre des écritures hors limites.
  - Chez nous : non.
  - Difficulté : facile (crochet qui refuse un index ≥ 23). Il faut d'abord savoir ce que sont ces 23 fiches.
- **Résultat d'un match (`sub_826CEF28`) réécrit** (1 072 → 1 372 octets, nouveaux appels `sub_826D4F28` et
  `sub_826CCDA8` au début). C'est notre crochet de résultats Univers.
  - Pas encore compris.
  - **Attention :** si on recopie le changement de la TU2, notre crochet est en jeu.
- **Draft (`sub_826CD010`, « Draft ce jour-là ? »)** : un test en plus, sur un bit de la fiche de l'émission
  (+928).
- **Booking du PPV (`sub_826F0290`)** : la boucle change de bornes. C'est la fonction qui plante chez nous avec une
  émission vide (`docs/univers.md` §20).
- **Autres changements :**
  - MITB (`sub_826F1028`) ;
  - calendrier (`sub_826C8690`, sans doute du bruit) ;
  - écrans du menu Univers (`sub_82545440`, `82545A70`, `825463E8`, `82590B60`, `825AADA0`…) : nouveaux textes
    (0xCF76-0xCF7A), des listes qui passent de 9 à 11 et de 5 à 7/8 entrées. Ce sont des options ou des messages
    ajoutés.

### 2.3 Armes (TU1)

- Un paramètre de plus est passé à la recherche dans une table d'animations/effets (`sub_8246F488`, dont le 4e
  argument était 0) : `objet+48 → +1764`.
- Ce paramètre suit tout le chemin :
  - `sub_826F63A8`, `826F6538`, `826F6610`, `826F6690`, `826FC6B0` (près de `weapon_%s_breakdown/setup/strike`) ;
  - `sub_822F4038`, `822F4788`, `822F7A90`, `822F8470`, `822F8690`, `822FAEA8` (objets : `table`, `s_walkUpon`).
- **Lecture possible :** les objets (tables…) prennent la bonne variante de leur animation ou de leur son. Cela
  pourrait toucher les notes « tables sous le ring » ou « son qui boucle après un Backstage Brawl ». **Non
  confirmé.**
- Chez nous : non. Difficulté : moyenne à difficile (une dizaine de fonctions).

### 2.4 Réglages du gameplay (TU2)

- `sub_82276590` remplit au démarrage 64 réglages entiers (0x83170004… et 0x831740BC… sur le disque).
- La TU2 change **3 valeurs : 40 / 60 / 80 → 70 / 80 / 90** (5e à 7e entrées, à 0x831740C0-C8 ; la suite est
  100, 150).
- Lecteurs connus des entrées voisines : `sub_82248730` (près du momentum, `docs/omg_finishers.md`) et
  `sub_82276930` (un facteur choisi selon un niveau 0-3).
- Ce que règlent ces 3 valeurs : **pas trouvé** (aucun lecteur direct).
- Chez nous : non. Difficulté : très facile (3 mots écrits après `sub_82276590`). Il faut d'abord savoir ce qu'ils
  font.

### 2.5 Chargement des fichiers de la mise à jour (TU1 et TU2, pas utile pour nous)

- Ajout du volume `UPDATE:` : `sub_826A5FE8` (plist360.arc), nouvelle `0x827F41C0` (`UPDATE:\%s.pac`).
- TU2 : `sub_826B1058` (`UPDATE:\pac\string.pac`, `ch\chEtc.pac`, `DLC.pac`) et `sub_82683140`
  (`UPDATE:\pac\_startup360.pac`). Ce sont les fichiers du paquet (nouveaux textes `strMenu.pac`, etc.).
- Chez nous : pas utile. Ces fichiers sont des données de THQ (jamais dans le dépôt). Notre système de couches
  (overlay) sait déjà servir des fichiers par-dessus le disque.

### 2.6 Create an Entrance / entrées (TU1 et TU2)

- `sub_829F9F90` (listes MOTION…, accrochée dans `wwe12_arena_pages.cpp`) : nouvelles bornes 406 et 1004/1005.
- `sub_828F2410` (choix de minitron, accrochée dans `wwe12_superstar_mods.cpp`) : un test en plus.
- `sub_829BE988`, `sub_829BF7F8` (`E/ST%02d`), `sub_829E9C08` (280 → 524 octets, 10 appels de plus),
  `sub_829E3E10`, et une nouvelle `0x829EA550` (`show`, `posi`, `dire`).
- Attention : deux de ces fonctions sont accrochées par d'autres sessions.

### 2.7 Ralenti / Highlight Reel, flux du match (TU2)

- `sub_822A3E68`, `822A5470` : textes 45109 « PREVIEW », 45110 « SPEED CHANGE », 45123 « ADD CLIP TO REEL ». C'est
  l'éditeur de ralentis.
- **Cas spéciaux pour des finishers précis (TU2) : très probablement le « gel de finisher ».** Ajout du
  2026-11-04, hors jeu.
  - Les numéros comparés sont des numéros de prises (liste des signatures et finishers de la session Gameplay,
    `sigfin.txt`) :
    - 33901 Spear 3 ; 33902 Sweet Chin Music ; 58012 Sleeper Hold 2 ; 58204 Ankle Lock 3 ;
    - 50151 Edgecution 1 ; 51062 Skull Crushing Finale ;
    - 37229, 57006 : non nommés.
  - Champs lus sur le catcheur : +680 → u16 = état / animation en cours, +700 = numéro de la prise,
    +684 / +688 → octets d'étape. L'adversaire est `[0x8317B770 + 4 × [catcheur + 3032]]`.
  - `sub_8223F440` (prédicat, renvoie 0/1) : renvoie 0 si état 37229 + prise Edgecution et adversaire à l'état 20.
  - `sub_821D3F58` : un traitement à part si état 53 + prise Spear 3 et adversaire à l'état 284.
  - `sub_8223F878` : Spear 3 et Sleeper Hold 2 à certaines étapes (+684 = 0 ou 7, +688 = 0).
  - `sub_82280DF0` : Ankle Lock 3 à l'étape 7.
  - `sub_823CCDB8` : prise 57006 (+702 = +703 = 0).
  - `sub_823F24C8` : en fin de fonction, si l'adversaire (`obj+36`) est en Skull Crushing Finale, les champs +8,
    +12, +16 sont mis à 0 et le résultat aussi.
  - `sub_82220020` : un cas 1333 (état ?) avec `sub_8223DCB8`.
  - **Chez nous :** aucun de ces cas (code du disque). Le jeu n'est pas encore lancé pour voir si ces prises gèlent.
  - **Difficulté :** chaque cas est petit, mais il est placé au milieu d'une fonction, donc il faut un crochet par
    fonction qui refait la condition au bon endroit. Le plus simple : Skull Crushing Finale (à la fin de
    `sub_823F24C8`, après l'appel). Il faut le tester en jeu, prise par prise.

### 2.8 Autres

- **En ligne / réseau (TU2, une quarantaine de fonctions vers 0x82B96000-0x82BA4000)** : `%d.%d.%d.%d`, `%I64d`,
  nouvelles vérifications d'erreur. Pas utile (pas de Xbox Live).
- **Système (TU2)** : `sub_821B5440`, nouvelle `0x821AC930` (« GameMainThreadEvent »), `sub_821AD1E8` (`D:\`). Ce
  sont les fils d'exécution du jeu.
  - Proche de notre fil d'exécution à 4 cœurs (`docs/wwe12_chamber_hang.md` cite `sub_821AD1E8`).
  - Pas compris.
- **Ceintures (TU1)** : `sub_827B16D0`, `827B30E8` (près de `belt_WorldHeavy`, `belt_WWE`…), une borne 23 retirée.
- **Création (TU1, structure de 0x52xx-0x58xx octets)** : `sub_825DC108`… `825E4900`, `825ED660`, `825EC580`
  (vignettes `thum_cas%02d`). Les champs d'une grosse structure sont décalés : un champ a été ajouté.
- **Havok et grosses fonctions vers 0x82C0000-0x82F6000** : surtout du bruit de recompilation. Les offsets changent,
  le code non.
- **Non identifiés :** blocs 0x8263xxxx-0x8265xxxx (TU1, `sub_8265BD40` +240 octets), 0x82A1xxxx (TU1),
  `sub_82895B28` / `82DCA9C8` (TU2, mêmes ajouts dans les deux : bornes 54, 110, 276).

## 3. Les notes de patch (liste non vérifiée) face au code

| Note | Trouvé ? |
|---|---|
| Movesets de CAW réinitialisés | **Oui, très probable** : CA09/CA10 ajoutées aux listes de catégories (TU1, §2.1) |
| Univers, au-delà de 50 Superstars / CAW | **Probable** : les CAW (ids < 50) comptent enfin dans les effectifs (TU2, §2.2) |
| Tables sous le ring | Peut-être : nouveau paramètre dans les fonctions d'objets / armes (TU1, §2.3) |
| Son qui boucle après un Backstage Brawl | Peut-être : même famille (§2.3) ; rien de net dans le son |
| Gel pendant un finisher | **Très probable** : cas spéciaux ajoutés pour Spear, Ankle Lock, Sleeper Hold, Edgecution, Skull Crushing Finale… (TU2, §2.7) |
| Passer à travers la cage HIAC / les barrières | Pas trouvé : la collision (Havok) ne change que par recompilation ; peut-être un réglage (§2.4) |
| Évasion d'un tombé en Elimination Chamber | Pas trouvé |

## 4. Tableau court

| Fonction (disque) | Ce que THQ a changé | Chez nous ? | Difficulté |
|---|---|---|---|
| `sub_82460680`, `82460880` + 10 bornes 47 → 49 | Banques CA09/CA10 (prises DLC 3-4) ajoutées aux catégories de prises | Non (touche aussi nos CA11+) | Moyenne |
| `sub_829D8148` | Pareil pour les listes de Create an Entrance | Non | Moyenne |
| `sub_82583B78`, `82599DB0`, `826D3B58` | Les CAW comptent dans l'effectif des émissions Univers | Non (notre recomptage les écarte aussi) | Facile |
| `sub_826E1E60` + 4 voisines | Contrôle d'index < 23 avant un tableau Univers | Non | Facile |
| `sub_826CEF28` | Résultat d'un match Univers réécrit | Non (notre crochet est dessus) | Difficile, pas compris |
| `sub_826CD010` | Test de Draft en plus | Non | Facile une fois compris |
| `sub_826F0290` | Bornes du booking des PPV | Non | Moyenne, pas compris |
| `sub_8246F488` et ~10 appelants | Un paramètre de plus (objet+48 → +1764) pour les armes / objets | Non | Moyenne à difficile |
| `sub_82276590` | 3 réglages 40/60/80 → 70/80/90 | Non | Très facile (sens inconnu) |
| `sub_822A3E68`, `822A5470` | Éditeur de ralentis | Non | Pas prioritaire |
| `UPDATE:` (`826A5FE8`, `826B1058`, `82683140`) | Fichiers de la mise à jour | Inutile | — |
| ~40 fonctions réseau | Xbox Live | Inutile | — |

