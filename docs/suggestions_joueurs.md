# Suggestions de joueurs : tri (2026-10-21)

Longue liste reçue d'un joueur (anglais), classée par effort. Rien n'est promis ; ce document sert de réserve
d'idées. « Univers » = la conversation Univers (docs/univers.md).

Reprendre des fonctions de WWE '13 / 2K14 dans WWE '12 : **autorisé par l'utilisateur** (déjà fait plusieurs fois
pour certaines fonctionnalités). Ce qui ne change pas : le dépôt et les versions publiées ne contiennent aucune
donnée de jeu (modèles, animations, cinématiques, sons, textures, .pac), de WWE '12 comme des autres jeux. Une
fonction reprise = soit réécrite par nous (logique, règles, tables), soit extraite en local depuis la copie du
joueur (jamais distribuée). Le code d'un autre projet (ex. recompilation 2K14 de Jarrod, BSD-3) demande la
revue de sa licence et l'accord de l'utilisateur.

## Déjà fait ou en cours (0.9)
| Demande | État |
|---|---|
| Rendre un titre vacant dans l'Univers | **fait** (titres qu'aucune émission n'utilise, via TITLE MANAGEMENT) ; limite voulue : un titre d'émission resterait sans match |
| Tenues alternatives dans l'Univers (choisir, ou RANDOM) | **fait** (Y sur la fiche du Superstar) |
| Remise à zéro de l'Univers | **fait** (EDIT UNIVERSE, copie auto de la sauvegarde) |
| Éditer un match de la carte et le sauvegarder sans le jouer (« Save & Return ») | **fait** (ligne SAVE dans le menu du match après EDIT ; écrit le match dans la carte puis revient à la carte ; commit 5179c17) |

## Rapide / probablement 1.0 (réglages ou tables, un crochet)
- Saignement des catcheuses : lever la restriction de genre (à chercher : un drapeau de sang).
- Liste complète des challengers pour titres mineurs, Divas et par équipes (comme les titres majeurs).
- Réordonner les classements à la main (écran + réécriture de la table des classements de la sauvegarde).
- Défense de titre spontanée à faible probabilité dans une émission hebdomadaire (crochet sur le tirage du booking).
- Titres par Superstar (limiter Cena au titre mondial, Bourne au titre US ; titres mixtes hommes/femmes) :
  crochet sur le choix des challengers, plus un écran de réglage.
- Entrants « Légendes » du Royal Rumble (0 à 4 surprises tirées d'une liste de Légendes).

## Moyen (plusieurs semaines, s'appuie sur ce qu'Univers a déjà cartographié)
- Calendrier libre (créer/déplacer/supprimer des émissions du lundi au samedi, PPV un dimanche quelconque),
  filtrage de marques par PPV.
- Tournois (titre vacant : élimination directe ; King of the Ring) : il faut un tableau et une planification.
- Qualifications Money in the Bank et Elimination Chamber sur les émissions hebdomadaires.
- Scripts « La Route de WrestleMania » (rivalité de The Streak avec The Undertaker).
- Suivi des statistiques : victoires/défaites par Superstar, règnes de titre (nombre, plus long, jours cumulés),
  vainqueurs du Rumble / MITB / KOTR. Le jeu ne les garde pas : les noter dans un fichier du port (comme universe_attires.txt).
- Capacité de CAW (50 → 100-200) et de logos (20 → 100-200) : format de sauvegarde, mémoire du jeu (voir docs/limit_breaking.md).

## Lourd (possible, long ; rien de distribuable)
- Cinématiques d'Univers de WWE '13 / 2K14 : il faudrait les extraire en local depuis la copie du joueur et les
  brancher sur les déclencheurs de l'Univers ; format différent à comprendre. Piste plus simple d'abord : les
  cinématiques de Create a Story de WWE '12 comme déclencheurs.
- Types de match de WWE '13 / 2K14 (I Quit, arbitre invité), OMG! Moments : la logique et les animations n'existent pas dans
  WWE '12 ; très lourd (logique à réécrire, animations à extraire en local), après la 1.1 (voir limit_breaking.md pour les stipulations).
- Moteur de matchs mixtes homme/femme complet : les jeux d'animations par genre ne sont pas faits pour s'échanger.
- Match Creator sans contraintes : à regarder avec les stipulations (limit_breaking.md, idée 1).

## Ordre proposé
1. 0.9 : SAVE sur match modifié (si faisable), le reste ci-dessus.
2. 1.0 : saignement des catcheuses, listes de challengers, classements manuels, défenses spontanées.
3. Après 1.0 : calendrier, tournois et qualifications, statistiques, capacités de CAW.
