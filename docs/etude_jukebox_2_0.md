# Étude : refaire le jukebox (2.0)

Session Univers, 2026-11-28. Demande d'un joueur, relayée par la Main : comme dans le portage SvR 2011, pouvoir
activer / couper les chansons une par une, et avoir sa propre playlist comme musique des menus (au lieu de toutes les
chansons). Rien n'est codé : étude et plan seulement.

## Ce qu'on a (1.0 → 1.5)

- `port/src/native/wwe12_jukebox.cpp` (630 lignes). OPTIONS > CREDITS devient JUKEBOX. La page a 4 lignes :
  JUKEBOX oui/non, ORDRE (aléatoire / thème choisi seulement), THÈME (un thème parmi tous), MUSIQUE DES MENUS DU JEU AUSSI.
- Liste des thèmes (`LoadThemes`), dans cet ordre :
  - la musique des menus du jeu (numéro 1) ;
  - les thèmes du jeu (table u16 0x83073BA0, sauf 272 = playlist du jeu) ;
  - les DLC possédés (50-99) ;
  - les thèmes des catcheurs ajoutés par mod (`Wwe12AddedThemes`).
  Il y en a environ 150 selon les DLC.
- Lecture d'un thème : on coupe les pistes 0-2, on charge le thème sur la piste 2, puis on le joue. C'est la même
  méthode que l'aperçu de Create an Entrance. Le thème suivant est tiré au hasard (`NextTheme`) parmi tous les thèmes,
  sauf celui qui joue et ceux qui n'ont pas pu se charger.
- Réglages enregistrés dans `wwe12.toml` : `wwe12_jukebox`, `wwe12_jukebox_order`, `wwe12_jukebox_theme`,
  `wwe12_jukebox_game_music`.
- La page est un script Lua compilé, produit par `tools/graphics_pages.py` (`jukebox_source`, fichier
  `wwe12_jukebox_page.inc`).
  - Chaque ligne a une valeur ; gauche / droite la change. Les textes sont des numéros servis par l'exe
    (`Wwe12JukeboxText`, noms des thèmes = 44000 + n).
  - L'exe ne voit les valeurs qu'à l'ouverture (`InitInGameOptions`) et à la validation (`UpdateInGameOptions`), par le
    tableau 32 bits de la page.
  - **Une page tient 5 lignes au plus** : une 6e fait planter la page (2026-10-28).
  - Les thèmes portés d'autres jeux par le Mod Maker sont déjà dans la liste (catcheurs ajoutés).

## Ce que fait le jeu pour la musique des menus

- La musique des menus du jeu, « @System-Menu_Music », est un conteneur du moteur de son. **Il choisit seul la chanson** :
  sa liste de 19 numéros (0x83074908) ne décide rien (test du 2026-10-26).
- On ne peut donc pas choisir une à une les chansons de menu d'origine du jeu. On peut seulement les garder ou non, en
  bloc (le choix « musique des menus du jeu aussi »).
- Le jukebox prend la main quand la mise à jour `sub_8271BFD0` relance la piste 0. Il prend aussi la main quand un écran
  lance la musique par `sub_821D6368`, et la rend quand un match charge.

Conséquence : la playlist est faite de **thèmes d'entrée** (du jeu, DLC, ajoutés par mod), plus une case « musique des
menus du jeu » en bloc.

## Stockage des choix (proposition, à valider par iMrBurn)

- **Fichier du joueur** `Documents\wwe12\jukebox_playlist.txt`, dans le même dossier que `universe_stats.txt` (racine
  des données de l'utilisateur).
  - Une ligne par thème **retiré** : `off <numéro>  # NOM`. On note les retirés plutôt que les gardés : par défaut tout
    est dans la playlist, et un nouveau thème (DLC, mod) y entre tout seul.
  - Le nom en commentaire sert à le lire à la main ; seul le numéro compte.
  - Les catcheurs ajoutés par mod ont des numéros fixes (Mod Maker), donc leurs choix restent valables.
- **Aucune nouvelle clé toml.** Seule petite chose : `wwe12_jukebox_order` prend une valeur de plus, 2 = MA PLAYLIST
  (0 = tous au hasard, 1 = thème choisi seulement). C'est une valeur nouvelle d'une clé existante, pas une clé nouvelle ;
  à confirmer par iMrBurn.
  - Autre solution sans rien toucher au toml : ALÉATOIRE suit toujours la playlist. Avec une playlist complète, c'est
    exactement le comportement actuel.
  - **Je recommande cette solution** : plus simple pour le joueur, rien à apprendre. L'ORDRE reste à 2 valeurs.
- Le fichier n'est jamais dans le zip. Le launcher le garde lors des mises à jour (il est dans `Documents\wwe12`, pas à
  côté de l'exe).

## Interface dans la page JUKEBOX

Elle passe de 4 à 5 lignes (le maximum) :

| Ligne | Valeurs |
|---|---|
| JUKEBOX | DÉSACTIVÉ / ACTIVÉ |
| ORDRE | ALÉATOIRE (dans ma playlist) / THÈME CHOISI SEULEMENT |
| THÈME | on parcourt les thèmes ; le nom porte une marque : `JOHN CENA` ou `JOHN CENA (OFF)` |
| DANS MA PLAYLIST | OUI / NON : coche ou décoche le thème affiché sur la ligne THÈME |
| MUSIQUE DES MENUS DU JEU AUSSI | NON / OUI (inchangé) |

Le point technique : la ligne DANS MA PLAYLIST doit suivre la ligne THÈME pendant que la page est ouverte. Aujourd'hui,
l'exe ne parle à la page qu'à l'ouverture et à la validation. La solution prévue est un petit canal « en direct » par nos
propres crochets, qui interceptent déjà `InitInGameOptions` et `UpdateInGameOptions` :

- Quand la ligne THÈME change, le Lua écrit le n° du thème dans une case libre du tableau, puis appelle
  `yOption.InitInGameOptions(104)`.
  - Notre crochet voit 104 et répond OUI / NON dans une autre case.
  - Le Lua l'affiche sur la ligne 4.
- Quand la ligne 4 change, le Lua appelle `yOption.UpdateInGameOptions(104)`.
  - L'exe coche ou décoche le thème et l'écrit dans le fichier.
  - Le Lua réécrit le nom de la ligne THÈME : l'exe sert le nom avec ou sans « (OFF) ».
- B / annuler ne défait pas les coches. On peut le prévoir (copie au début, remise à la sortie) ; pas indispensable.

À vérifier avant de coder :

- la taille du tableau 32 bits de la page : on en utilise 3 × 5 = 15 cases, et il en faut 2 de plus ;
- que la page tient en 5 lignes avec le cadre `DrawRTWMOptions`, comme les pages GRAPHICS à 5 lignes.

Idées en plus (palier 3, si le joueur les veut) :

- TOUT COCHER / TOUT DÉCOCHER : deux valeurs de plus sur la ligne 4, ou une page du launcher.
- Une page JUKEBOX dans le launcher (case par thème, plus confortable que 150 lignes au pad) : à voir avec la session
  Launcher, car elle a besoin de la liste des noms. L'exe pourrait l'écrire dans le fichier en commentaire.
- La playlist seulement pour le menu principal, et la musique du jeu ailleurs : demande à préciser. Aujourd'hui le
  jukebox joue dans tous les menus.

Note : je n'ai pas le portage SvR 2011 sous la main pour comparer son interface. Si iMrBurn a une capture de sa page
jukebox, on peut s'en rapprocher.

## Plan en paliers

1. **Playlist dans le moteur** (petit, environ 1 h) :
   - lire / écrire `jukebox_playlist.txt` ;
   - `NextTheme` tire seulement parmi les thèmes gardés (si la playlist est vide, tous, avec une ligne de log) ;
   - journal « [jukebox] playlist: N of M themes ».
   - On peut le tester en éditant le fichier à la main.
2. **Page JUKEBOX à 5 lignes** (moyen, environ 3 h) :
   - `graphics_pages.py` : 5e ligne, cadre à 5 lignes, canal 104 ;
   - textes de la ligne et du « (OFF) » en 5 langues ;
   - crochets 104 dans `wwe12_graphics_menu.cpp`. Ce fichier est partagé avec Rendu (page GRAPHICS) et Gameplay
     (page DAMAGE) : on le prévient avant d'y toucher.
   - Test par iMrBurn : cocher / décocher, quitter, revenir, relancer le jeu.
3. **Confort** (selon les retours) : tout cocher / décocher, page du launcher, menu principal seulement.

Effort total : petit à moyen (une demi-journée de code pour les paliers 1 et 2, puis un test en jeu). Il n'y a pas de
risque pour les matchs : rien ne change hors des menus.
