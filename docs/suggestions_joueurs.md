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

## Suggestions du 2026-10-31 (Discord ; rien promis)
- **Interférence = embuscade en coulisses (Jay)** : dans l'Univers, le choix INTERFERE propose RINGSIDE RUN-IN (existant) ou
  BACKSTAGE AMBUSH, puis RUN DOWN ou CAR WINDOW (cinématiques de Create a Story jouées à la place de l'interférence ; même résultat :
  l'attaqué perd, la rivalité grandit ; news « Backstage Chaos: [ATTAQUANT] ambushed [VICTIME]… »). Dépend du chantier « cinématiques
  Create a Story dans l'Univers » (1.1). Conversation : Univers.
- **Noms des entrées et mouvements prédéfinis (Mod Maker)** : « Superstar 1, 2… » remplacés par le nom du catcheur (liste du Smackdown
  Hotel). Conversation : Mods ; les noms sont des textes, pas de la donnée à publier sans vérification.
- **MITB RAW et SmackDown (2 mallettes)** : une mallette par marque à l'époque. À voir côté Univers (2 matchs MITB, 2 détenteurs).
- **Touches de clavier comme le SvR 2011 de Kaiko** (ajouter / copier ses réglages) : Launcher (`keybinds.h`).
- **Chronomètre de match** et **interférence forcée sur un bouton** : Gameplay, à étudier.
- **Types de match comme le port SvR 2011** : à préciser avec le joueur (lesquels ?) ; stipulations = 1.1.

## Suggestions du 2026-11-03 (un joueur, Discord ; rien promis)
Liste du joueur, rangée par la Main. Les numéros de version sont ceux de `docs/feuille_de_route.md`.

**Déjà fait ou déjà prévu**
- Liberté sur les règles d'un match : le Match Creator étendu de la 1.0 va dans ce sens (`docs/match_creator.md`).
- Touches du clavier affichées comme notifications dans le jeu : icônes de touches, 1.4.
- Match squash avec finishers chargés : squash, 1.3 (`docs/squash.md`) ; la question « finishers chargés » : à voir avec le stock de finishers de départ.
- Interférence sur un bouton ; chronomètre de match visible : déjà à étudier (Gameplay).
- King of the Ring / tournoi : « Plus tard » (il faut un tableau et une planification).

**Simples ou moyens (à étudier)**
- Option du launcher « configurations de match sans restriction » avec avertissement (peut planter) : pas chère, dans l'esprit de la 1.3.
- Restreindre les types de match par émission, et les catcheurs de l'Univers depuis le launcher : comme nos réglages Univers (`wwe12_universe_*`), 1.3.
- Afficher les touches de chaque catcheur à l'écran ; chronomètre pour tous les types de match : de l'affichage, 1.4.

**Gros chantiers (aucune promesse)**
- Nouveaux types de match : 2 contre 2 contre 2 (SvR 2008), Tornado Tag à 6 (et variantes Elimination, Hardcore Triple Threat, 2 contre 2 Table / Extreme Rules, 2 contre 3), Slobber Knocker, 3 Stages of Hell, Gauntlet (menu pour le nombre d'adversaires), Beat the Clock Sprint, King of the Ring. Vraies règles à écrire ; les Tornado Tag et le 3 équipes demandent plus de 4 catcheurs actifs (lié à « plus de 6 superstars »). Règles du genre « 2 sur 3 Last Man Standing en Hell in a Cell / Inferno » : Match Creator.
- Changer de catcheur pendant un match ; recommencer un match entre deux : logique des équipes et caméra.
- Spectateurs qui donnent des armes ; monter sur la table des commentateurs : nouvelles animations et collisions, très long.

## Suggestions du 2026-11-04 (Discord ; rien promis)
- ~~Déplacer les matchs de la carte d'une émission~~ : **retiré (correction de l'utilisateur, 2026-11-04)** : le joueur ne demandait pas ça. Il voulait, comme dans SvR 2011, des **catcheurs animés dans l'écran de sélection et dans l'écran VS** : à voir (l'utilisateur n'en a pas trop envie), plutôt Mod Maker / Rendu, avec les conseils de Kaiko sur les écrans VS.
- **Choisir quelles émissions reçoivent certains PPV** (PPV propres à une marque) : Univers, « HOST SHOW » du calendrier fait peut-être déjà une partie. 1.3.
- **Importeur de logos** (comme le SvR 2011 recomp : textures importées en logos custom, arènes aussi) : Mod Maker. 1.1 à étudier.
- **Remplacer Create a Titantron** (utiliser les emplacements pour stocker nos titantrons) : Mod Maker. 1.1 à étudier.
- **Choisir la musique de victoire d'un catcheur** (Cena, Ryder, Triple H ont une musique de victoire séparée, plus courte ; page « VICTORY MUSIC : DEFAULT » dans Create an Entrance) : déjà dans la liste 1.1 (musique de victoire du Mod Maker).

## Bugs remontés le 2026-11-04 (Discord / GitHub, après la 1.0)
- **Mod Maker** (Jay, 8 bugs + Shawn Michaels DLC absent de SUPERSTARS ; Max Matus : image de mur à la place du minitron) : session Mods. Captures de Jay : `Downloads\Jay screens`.
- **Tenues custom toutes blanches après la 1.0** (Nothing) : Rendu, possible régression, candidat 1.0.1 si reproductible.
- **Intel HD 4000, D3D11 à 25 images/s** (Fac7) ; **GT 430, peaux cassées en D3D11** (#55, correction de Kaiko pour SvR 2011 à regarder) ; **chutes d'images aux changements de caméra** (#53) : Rendu.
- **Ceinture qui traverse les CAW** (Wes Smoove) : bug d'origine, Mods / Gameplay (le « belt fit » des catcheurs ajoutés peut aider).
- **Android** : #54 (Moto G100, DEVICE_LOST avec Turnip), #41 (Poco X3 Pro, nouveaux journaux) : session Android.

## Suggestion du 2026-11-06 (GitHub #57, Android)
- **Mode 30 fps pour les appareils moyens** (Redmi Note 13 Pro 5G, Snapdragon 7s Gen 2 : « jeu au ralenti ») : le port WWE 2K14 de Jarrodfeng a cette option. Un plafond simple ne suffit pas (le jeu tourne à 60 pas par seconde : il ralentirait) : il faut dessiner une image sur deux en gardant la vitesse. Utile aussi au Steam Deck et aux petits PC. Étude confiée à Android et Rendu (1.2) ; réponse faite sur l'issue (version, journal, préréglage LOW-END PHONE en attendant).


## 2026-11-06 : sortie de tombé (kickout) et changement de joueur

- Suggestion d'un joueur (Discord) : « Switch Players / Auto Kickout » pour rendre l'Univers et les autres modes moins pénibles, car le système de sortie de tombé (kickout) agace parfois ; ou bien rendre la jauge de kickout plus grande (toute la barre).
- Deux idées distinctes : (1) sortie de tombé automatique (option) ou jauge élargie = règle du jeu, crochet sur le code du compteur de tombé (à chercher par Gameplay, cvar à part, désactivé par défaut) ; (2) changer de joueur en match (tag, passer d'un catcheur à un autre) = plus gros, dépend du moteur de match, pas étudié.
- Rien promis. Classement : idée, candidat 1.3 (modes de match) avec la jauge élargie en premier (le plus simple).
