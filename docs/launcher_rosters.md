# Page « ROSTERS » du launcher (1.3)

> **État au 2026-11-12** (tout en commits locaux, rien poussé) :
> - **Fait** : page visible pour tous (`port/src/launcher/rosters.h`, `PageRosters()` dans `launcher.cpp`). Liste des 323 fiches
>   (nom, numéro, marque), recherche, filtre par marque, tri, compteurs ; changer la marque de l'**exhibition** de fiches cochées
>   (RAW / SmackDown / Sans marque) avec copie de sécurité, contrôle, annulation.
> - **Testé** : sur des COPIES de sauvegarde (écriture = 3 octets de marque + somme, annulation = fichier identique, fiche incohérente
>   refusée, copie de sécurité jamais élaguée) ; **en jeu par l'utilisateur** sur `Documents\wwe12_test` : Batista SmackDown → RAW,
>   écran de sélection de l'exhibition en RAW, Univers inchangé.
> - **Pas testé** : écriture sur la VRAIE sauvegarde de l'utilisateur (refusé pour l'instant) ; vraie copie de sécurité depuis la page
>   (en mode copie de test elle n'en fait pas) ; catcheurs ajoutés par un mod ; sauvegarde importée de la Xbox 360.
> - **Décision de l'utilisateur (2026-11-10)** : la page change la marque de l'**exhibition seulement** (octets +24..+26) ; elle ne
>   touche **jamais +27** (marque de l'Univers, qu'on change déjà dans le jeu). Exhibition et Univers restent séparés.
> - Étude et historique (options, questions, réponses d'Univers) : `docs/archives/launcher_rosters_anciennes_parties_2026-11-12.md`.

## 1. Format (vérifié sur des copies de la sauvegarde de l'utilisateur)

Fichier : `Documents\wwe12\B13EBABEBABEBABE\54510890\00000001\SaveData.dat\SaveData.Dat` (5 701 624 o), fichier en clair posé par le jeu.

- En-tête de 28 octets (7 u32 gros-boutistes) `{0x4F490, 0x5EE90, 0x38, 0x13, 9, 2, somme}` ; **somme = somme simple (mod 2^32) des octets
  28 .. 28 + 0xAE358**, à recalculer à chaque écriture.
- 323 fiches de **286 octets** à `28 + index × 286` ; **numéro du catcheur = u16 gros-boutiste à +40/+41 = index + 50** pour les 323
  (confirmé par Univers ; ne pas toucher aux catcheurs créés, numéros < 50, zone CAW). 65 emplacements vides (numéro 0, sans nom) sur
  la sauvegarde de l'utilisateur : comptés, cachés, jamais écrits.
- Marque : **+24..+26 = marque de l'exhibition** (trois octets égaux), **+27 = marque de l'Univers** (le jeu l'écrit seul en
  Univers : il peut valoir autre chose, vu 1,1,1,2). Valeurs : 0 RAW, 1 SmackDown, 6 sans marque (légendes, DLC, free agents),
  4 (19 catcheurs, sens inconnu), 2 (vu sur un catcheur ajouté, sens inconnu). **Seuls 0, 1, 6 sont écrits** ; le reste s'affiche
  « Autre (n) » et reste verrouillé ; une fiche dont les octets +24..+26 diffèrent ou dont le numéro ≠ index + 50 s'affiche « ? »
  et reste verrouillée.
- Nom : +42 (ASCII), souvent « 0 » ou un nombre (« 22 ») : ce n'est pas un nom ; le launcher le prend alors dans `pac/ch/chEtc.pac`
  du jeu de l'utilisateur (lecture seule, `game_disc.h` `SuperstarNames`), sinon « (#numéro) » (en bas de la liste). Nom court +178, genre +234.

## 2. Garde-fous de l'écriture (`rosters::Editor`)

Jeu fermé et aucune autre opération de sauvegarde ; en-tête + somme valides ; **copie de sécurité manuelle** (jamais élaguée) avant
d'écrire (`Saves::BackupBeforeChange`) ; chaque fiche choisie contrôlée ; fichier temporaire `.rosters.tmp` puis remplacement ;
relecture et vérification, sinon remise de l'original ; « Annuler le dernier changement » refusé si la sauvegarde a changé depuis.

## 3. Tester sans risque

- Dossier de test unique : `Documents\wwe12_test` ; lanceur `tools/dev/lancer_test_rosters.bat`.
- Aide de test : variable `WWE12_LAUNCHER_ROSTERS_FILE` = chemin d'une COPIE de `SaveData.Dat` (la page l'utilise à la place de la vraie
  et ne fait pas de copie de sécurité). Ne jamais écrire sur la vraie sauvegarde pour essayer.
- Le jeu réécrit la sauvegarde après une partie : comparer les compteurs d'une copie ancienne et d'une copie récente n'a pas de sens.

## 4. Crédit

Idée de l'onglet : launcher de KaikoClanworth1 (SvR 2011 PC port, MIT : « Copyright (c) 2026 KaikoClanworth1 and the SvR 2011 PC port
contributors »). Son onglet règle des catégories de tri (fichier `sort_tags.txt`), pas la sauvegarde ; notre page est différente et n'a
repris aucun code ni la table `roster_names.inc` (les noms viennent du jeu de l'utilisateur).
