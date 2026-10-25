# Plan : la page ARÈNES du Mod Maker refaite (2026-10-25)

Inspiré du SvR2011 Mod Maker de Kaiko (son manuel : `work/kaiko-svr2011/docs/MOD_MAKER_MANUAL.md`). On reprend ses
**idées** ; son code seulement après relecture et accord de l'utilisateur (règle du dépôt).

## Aujourd'hui

Une seule page ARÈNES avec 5 boutons radio mélangés : Retoucher une arène, Autre fichier d'arène, Porter de WWE '13,
Porter de SvR 2011, Vue 3D. Chaque portage demande son jeu à part (dossier de WWE '13, ISO de SvR), rien n'est retenu
d'une fois sur l'autre, et la base se choisit en haut, avant de savoir ce qu'on porte.

## Ce qu'on vise

### 1. Une nouvelle page « Mes disques de jeu » (dans le menu de gauche)

- On ajoute **une fois** chaque jeu qu'on possède : son ISO ou son dossier (SvR 2011, WWE '13, WWE 2K14, 2K15, 2K16,
  2K17...). Bouton « Chercher mes disques » qui fouille un dossier (ex. `Desktop\WWE ISOS`).
- Le Mod Maker reconnaît le jeu tout seul et le garde en mémoire (fichier de réglages à côté de l'exe).
- Pour chaque jeu, une ligne dit ce qu'il peut apporter : **arènes** (oui / bientôt), **catcheurs** (oui / bientôt),
  et ce qui manque si le dossier est incomplet.
- Rien n'est copié ni distribué : le Mod Maker lit les fichiers du joueur au moment du portage.

### 2. ARÈNES : trois onglets

- **Nouvelle arène** (l'assistant, ci-dessous) : remplace « Porter de WWE '13 », « Porter de SvR 2011 » et
  « Autre fichier d'arène ».
- **Retoucher une arène du jeu** : l'actuel « Retoucher » (changer ses images), sans changement.
- **Vue 3D** : l'actuelle, sans changement.

### 3. L'assistant « Nouvelle arène », étape par étape

Une barre d'étapes à gauche, « Retour » / « Suivant » en bas, une barre de progression ; chaque étape a **une seule
chose** à choisir ; ce qu'on saute garde la valeur proposée.

1. **Point de départ** : « Une arène de mes autres jeux » (liste des jeux de *Mes disques*, puis leurs arènes par
   nom, avec leur bannière quand elle existe ; les arènes qui ne marchent pas encore sont grisées avec la raison) ou
   « Un fichier d'arène (.pac) » (sans disque, ex. un fichier envoyé par un ami).
2. **Arène de base** (à la place de laquelle elle joue) : la liste des 21 arènes du jeu avec leur place disponible ;
   **les bases déjà prises par une arène ajoutée sont marquées** (une par base, à cause de l'écran VS) ; la base
   proposée = une base libre de taille suffisante.
3. **Nom et bannière** : le nom affiché, la bannière (celle du jeu d'origine, celle de la base, ou une image à soi).
4. **Écran VS, plaques et vidéo** : ce qui est repris, avec un aperçu : écran VS (WWE '13 : le sien ; SvR : un écran
   de WWE '12 habillé de ses images), plaques d'entrée et locator (WWE '13 : les siens), vidéo de transition, foule.
   Une case par élément pour garder celui de la base à la place.
5. **Aperçu** : la vue 3D de l'arène convertie + un rapport clair (objets convertis, images réduites pour tenir dans
   la place, avertissement de rampe, ce qui manque).
6. **Terminer** : « Installer dans le jeu », « Enregistrer le .svrmod » (pour partager), et **« Tester en jeu »**
   (installe puis lance le jeu).

### 4. Les « problèmes » en direct (comme Kaiko)

En bas de la page : en **rouge** ce qui empêche de terminer (ex. pas de base choisie, arène trop grande même réduite),
en **jaune** les avertissements (base déjà prise, rampe à une autre hauteur). Les boutons grisés disent pourquoi au
survol.

## Ordre de travail (une étape testée avant la suivante)

| # | Étape | Test |
|---|---|---|
| 1 | Page **Mes disques de jeu** : ajouter / retirer un jeu, reconnaissance (SvR 2011, WWE '13, 2K14-2K17), mémorisé | tes ISO de SvR 2011, 2K14, 2K15, 2K16, 2K17 et ton dossier WWE '13 reconnus |
| 2 | **Assistant** étapes 1 à 3 et 6 (départ, base, nom/bannière, installer) branché sur les portages actuels (SvR 2011, WWE '13) | refaire Vengeance et WM 26 par l'assistant, en jeu |
| 3 | Étapes 4 et 5 (choix de ce qui est repris, aperçu, rapport) + problèmes en direct | refaire Royal Rumble (SvR) en gardant / enlevant l'écran VS |
| 4 | « Tester en jeu » | un clic installe et lance |
| 5 | Retrait des anciens boutons radio, manuel (`docs/mod_maker.md`) | — |
| plus tard | Arènes de 2K14-2K17 (format à étudier : Kaiko les convertit depuis les fichiers extraits), arène vide, éditeur 3D, projets à rouvrir | — |

## À décider par l'utilisateur

- Garder l'onglet « Autre fichier d'arène » (remplacer une arène par un `bgNN.pac` déjà prêt) ou le fondre dans
  l'étape 1 de l'assistant ?
- Les arènes ajoutées **remplacent-elles** encore la base si on le veut (« Remplacer l'arène » au lieu de « nouvelle
  arène ») ? Aujourd'hui c'est possible ; l'assistant ne proposerait que « nouvelle arène ».
