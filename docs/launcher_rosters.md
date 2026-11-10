# Page « ROSTERS » du launcher : étude (2026-11-05, prévue pour la 1.3)

> **État au 2026-11-06** : étude terminée, **rien n'est codé** ni écrit sur une sauvegarde (tout a été lu sur une copie).
> Décision de l'utilisateur : page prévue pour la **1.3** (la 1.2 = Title Update / perf). Reste à faire : poser les 4 questions du
> §6 à la conversation Univers, puis séance 1 (lecture seule). Rien de ce document n'est périmé : rien à archiver.

Idée de l'utilisateur : choisir la marque (RAW / SmackDown / autre) de chaque catcheur **hors de l'Univers**, « comme le
launcher de Kaiko ». Étude en lecture seule : rien n'est codé, aucune sauvegarde réelle touchée (tout ce qui est lu ci-dessous
l'a été sur une COPIE de `SaveData.Dat`).

## 1. Ce que fait le launcher de Kaiko (SvR 2011, MIT, crédit obligatoire)

Sources : `work/kaiko-svr2011/port/launcher/roster_tab.c` (391 lignes), `roster_names.inc` (99 lignes),
`port/src/sort_filters.cpp` (côté jeu).

- Son onglet « Roster » ne touche **pas à la sauvegarde** et ne règle pas des marques d'Univers : il règle les
  **catégories de tri des menus de liste** du jeu (X = « Sort Category » : SMACKDOWN, RAW, NPC, MODS).
- Le fichier est `<jeu>\UserData\sort_tags.txt`, lignes `<id> = SMACKDOWN | RAW | NPC | MODS` (commentaires `#`).
  Le **jeu** le lit au démarrage (`sort_filters.cpp`, crochet sur la construction des listes) ; le launcher se contente de
  réécrire le fichier en gardant les commentaires et les lignes qu'il ne connaît pas.
- Écran : recherche + liste (nom, note « DLC / Diva / non sélectionnable / mod ») + boutons SMACKDOWN / RAW / NPC / MODS /
  DEFAULT + ligne d'état + « ouvrir le fichier ». Les noms viennent d'une table figée `roster_names.inc` (id, nom, note)
  plus les mods de catcheurs (`Mods\Superstars\slots.txt`).
- Conclusion : modèle **« fichier texte lu par le jeu »**, sans risque pour la sauvegarde. Chez nous, ce modèle ne change
  PAS la marque que l'Univers lit (elle est dans la sauvegarde, voir 2) : il faudrait que le jeu applique le fichier aux
  fiches.

## 2. Où est la marque chez nous (vérifié sur une copie de la sauvegarde de l'utilisateur)

Fichier : `Documents\wwe12\B13EBABEBABEBABE\54510890\00000001\SaveData.dat\SaveData.Dat` (5 701 624 o). C'est un fichier
**en clair posé par le jeu dans un dossier** (pas de STFS à lire ni à refaire : le STFS n'existe que dans les fichiers
Xbox 360 qu'on importe).

- En-tête de 28 octets (7 u32 gros-boutistes) `{0x4F490, 0x5EE90, 0x38, 0x13, 9, 2, somme}` ; **somme = somme simple (mod 2^32)
  des octets 28 .. 28 + 0xAE358** : recalculée sur la copie, elle vaut `0x12BA99A` = le dernier mot de l'en-tête (vérifié).
  Toute écriture doit la recalculer (docs/univers.md §3.3).
- Bloc 1 (à partir du fichier + 28) : les fiches de catcheurs, **286 octets chacune**, `fiche(index) = 28 + index × 286`.
  Pour un catcheur du jeu ou ajouté, `index = id − 50` (vu : id 50 Michael Cole = index 0, id 62 Trish = index 12 ; la table
  officielle est `u16[373]` à 0x8307B5E0 : id → index, à relire en jeu pour les autres ids ; les catcheurs créés, ids < 50,
  sont dans la zone des CAW, index × 1 832 à bloc + 0x1B9CC : hors de cette étude).
- Dans une fiche : **+24..+27 = marque** (quatre octets identiques ; l'Univers lit +27), **+41 = id**, **+42 = nom complet** (ASCII),
  **+178 = nom court**, +234 = genre (corrigé le 2026-11-10 : ce document disait à tort +238), +247 = « débloqué », +283 = drapeau DLC (docs/mod_maker.md, commit d7918b0).
- Valeurs de marque vues dans la copie (330 fiches) : **0 = RAW (111), 1 = SmackDown (34), 4 (19), 6 (160, « autres » :
  légendes/DLC/free agents)**. Quelques fiches ont des octets +24..+26 différents de +27 (6 sur 330) ou des valeurs hors
  table (125, 71 : ce ne sont probablement pas de vraies fiches, au-delà du dernier catcheur) : le launcher doit les montrer
  en « ? » et ne jamais y écrire.
- Le pack des catcheurs ajoutés (WWE12PCMODS) refait la fiche d'un catcheur ajouté à chaque lancement MAIS, depuis d7918b0, ne
  touche plus à la marque du joueur : la marque choisie dans la sauvegarde est donc conservée. Les fiches d'origine
  (misc.pac) sont remises par « remise à zéro de l'Univers » (docs/univers.md §27).

## 3. Ce qu'il faut pour la modifier depuis le launcher

### Option A : écrire la sauvegarde (la marque est bien DANS la sauvegarde)
1. Jeu fermé (refus sinon : `game::IsRunning()` existe déjà) et aucun import en attente.
2. **Copie de sécurité automatique** par la page SAUVEGARDES (`MakeBackup`, déjà utilisée par restaurer/supprimer/repartir
   de zéro) : c'est aussi le « retour arrière ».
3. Lire `SaveData.Dat` en mémoire, vérifier l'en-tête (les trois premiers mots, et que la somme actuelle est juste :
   sinon on ne touche à rien et on explique pourquoi), lister les fiches (nom +42, id +41, marque +27).
4. Changer +24..+27 (les quatre octets) des fiches choisies, recalculer la somme, écrire **dans un fichier temporaire puis
   remplacer** (jamais de fichier à moitié écrit).
5. Relire le fichier écrit et vérifier la somme ; sinon remettre la copie.
- Avantages : la marque est vraiment celle de la sauvegarde, marche pour l'exhibition ET l'Univers ; aucun code côté jeu ;
  pas de dépendance au format STFS.
- Risques : sauvegarde abîmée (atténué : copie, vérification, fichier temporaire), jeu ouvert (refusé), **Univers déjà
  commencé** : on ne sait pas si d'autres endroits du bloc Univers recopient la marque (ex. les effectifs des émissions, les
  compteurs d'EDIT BRAND, les fiches de 112 octets) : à confirmer avec la conversation Univers avant d'autoriser le changement
  « en cours de partie » (au minimum : avertir, ou limiter à une sauvegarde sans Univers commencé).

### Option B : fichier `rosters.txt` lu par le jeu (modèle Kaiko)
- Le launcher écrit `id = RAW | SMACKDOWN | AUTRE` ; le jeu l'applique aux fiches (+24..+27) au démarrage, après le chargement
  de la sauvegarde. Pas de risque pour le fichier de sauvegarde, mais : code de jeu à écrire (Univers, qui connaît
  `sub_82684910` et le bloc), à rejouer à chaque chargement de sauvegarde, et un « retour arrière » = supprimer le fichier
  (mais la sauvegarde garde alors les marques déjà écrites au dernier enregistrement).
- Option intermédiaire : A pour l'écriture + un jeu qui ne fait rien : la plus simple et sans nouveau code de jeu.

**Recommandation : option A**, une page qui n'écrit que +24..+27 des fiches choisies, derrière la copie automatique, avec
un bouton « Remettre les marques d'origine » (copie de la sauvegarde d'avant, ou valeurs d'origine de misc.pac si Univers
les fournit).

## 4. Page « ROSTERS » proposée (launcher)

- Liste (tableau ImGui comme SAUVEGARDES) : **Nom** (+42), **n°** (+41), **Marque** (RAW / SmackDown / Autre / ?), note
  (DLC, ajouté par un mod, « non modifiable » si les 4 octets sont incohérents). Tri par colonne.
- Filtres : recherche par nom, filtre par marque (Tous / RAW / SmackDown / Autre), « seulement les catcheurs ajoutés ».
- Action : cocher plusieurs lignes puis « → RAW », « → SmackDown », « → Autre », ou un sélecteur par ligne.
- « Appliquer » : confirmation (« Votre sauvegarde est copiée d'abord »), exécution (copie auto, écriture, vérification),
  message de résultat ; « Annuler le dernier changement » (restaure la copie de ce changement) ; bouton « Ouvrir le dossier
  des sauvegardes ».
- Garde-fous : jeu lancé → page grisée avec le message habituel ; en-tête ou somme invalide → liste en lecture seule et raison
  affichée ; jamais d'écriture hors +24..+27 ; fiches « ? » intouchables.
- Textes EN/FR comme les autres pages ; AUCUN texte dans les readmes avant que l'utilisateur ne décide de la sortie.

## 5. Estimation et tests

- Séance 1 : lecture seule (analyse de l'en-tête + liste + filtres), testée sur des COPIES de sauvegardes (dont celle
  de Petchy, 95 paquets, et un SaveData sans Univers).
- Séance 2 : écriture (copie auto, fichier temporaire, somme, retour arrière) ; tests hors ligne sur copies ; relecture par
  le jeu seulement avec l'utilisateur (« vas-y »), sur `Documents\wwe12_test_launcher`.
- Séance 3 : vérification en jeu (exhibition + Univers : le catcheur apparaît dans la bonne émission / liste de marque),
  Univers déjà commencé, catcheurs ajoutés (WWE12PCMODS), textes EN/FR, finitions.
- Total : **3 séances** (2 si on limite aux sauvegardes sans Univers commencé et sans option B).

## 6. Questions pour la conversation Univers

1. Changer +24..+27 d'une fiche suffit-il dans un Univers déjà commencé, ou d'autres blocs recopient-ils la marque
   (effectifs d'émission, EDIT BRAND, fiches de 112 octets, matchs déjà placés dans les cartes) ?
2. Valeur « Autre » : 4 et 6 sont-ils tous deux « pas RAW/SmackDown » ? Quelle valeur proposer pour un catcheur ajouté ?
3. Le calcul de la table `u16[373]` id → index : est-il le même pour les catcheurs ajoutés par le pack (index > 323) ?
4. Les fiches « d'origine » sont-elles disponibles hors du jeu (misc.pac) pour un bouton « marques d'origine » ?

## 7. Crédit

Idée et organisation de l'onglet : launcher de KaikoClanworth1 (SvR 2011 PC port, MIT : « Copyright (c) 2026 KaikoClanworth1
and the SvR 2011 PC port contributors »). Si du code ou la table `roster_names.inc` est repris, garder cet avis et le citer dans
`THIRD_PARTY_NOTICES`. Pour notre page, les noms viennent de la sauvegarde elle-même : la table de Kaiko n'est pas nécessaire.

## 8. Réponses d'Univers aux 4 questions (2026-11-10, lecture du code, rien testé en jeu)

- **Q1** : changer +24..+27 suffit, même dans un Univers déjà commencé (le jeu ne garde pas d'autre copie ; effectifs recalculés
  depuis +27 par `sub_826D2B50` ; EDIT BRAND ne stocke que les marques des émissions ; fiches de 112 octets = points de titre /
  stats, sans marque ; le transfert du jeu écrit ces mêmes octets). La page n'a donc pas à se limiter aux sauvegardes sans
  Univers, mais elle doit AVERTIR : (a) minimum de 15 catcheurs par émission (`wwe12_universe_min_roster`) : compter par marque
  et avertir sous 15 ; (b) un catcheur transféré peut encore apparaître une fois sur une carte déjà placée de son ancienne
  émission (non vérifié) ; (c) un champion transféré garde son titre.
- **Q2** : « Autre » = **6** (« sans marque » du Mod Maker : légendes, DLC, free agents). La valeur **4** (19 catcheurs, The Rock...)
  a un sens inconnu : l'afficher « Autre (4) » et ne JAMAIS l'écrire. Catcheur ajouté : RAW 0, SmackDown 1, sans marque 6 ; toujours
  les 4 octets +24..+27 identiques.
- **Q3** : pas d'index > 323 (la table compte 323 fiches, numéros 50..372, index = numéro − 50). Identifier chaque fiche par SON
  numéro (u16 à +41), pas par un calcul. Ne pas toucher aux catcheurs créés (numéros < 50, zone CAW de 1832 octets).
- **Q4** : les fiches d'origine sont dans `misc.pac`, groupe /CHAR/DAT (une entrée par place k < 323 : u16 version 1, u16 taille + 4,
  puis 286 octets ; `sub_82DCAA58`, docs/univers.md §27) ; retrouver le catcheur par le numéro. Il faut un lecteur PAC (Mods en
  a un) ; on ne sait pas si le groupe est compressé. Pour un catcheur AJOUTÉ l'entrée du disque est celle d'une case vide ou d'un
  DLC : ne PAS la remettre. La remise à zéro de l'Univers dans le jeu (`RestoreBrands`) remet les marques d'origine des catcheurs
  du jeu et saute les créés (< 50), ceux hors des données du jeu et les ajoutés : à écrire dans l'aide de la page. Sans lecteur PAC,
  « remettre les marques d'origine » = restaurer la copie automatique.
- Les commits d'Univers 3d47e4f / d702618 (marque gardée pour les ajoutés) sont dans la 1.2.0 publiée.
