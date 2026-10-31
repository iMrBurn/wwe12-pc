# Noms des entrées et des listes de coups prédéfinies : étude (2026-10-31)

Suggestion d'un joueur (Discord), transmise par la Main : dans le jeu, les entrées prédéfinies et les listes de coups
(move-sets) prédéfinies s'appellent « SUPERSTAR 01, 02… ». Il faudrait leur donner le nom du catcheur à qui elles
appartiennent, à deux endroits : l'écran CRÉER UNE ENTRÉE (choix d'une entrée prédéfinie) et la liste des move-sets
prédéfinis. Étude faite hors du jeu, sur les fichiers du disque : rien n'est codé et le jeu n'a pas été lancé.

Référence pour savoir à qui est quoi : les listes publiques de The SmackDown Hotel (« WWE '12 Preset Entrances List »
et « WWE '12 Preset Movesets List », thesmackdownhotel.com), lues sur le web. On ne les recopie pas dans le dépôt :
les noms qu'on utilisera seront vérifiés en jeu, et le site sera crédité.

## D'où viennent ces libellés

Ce sont **des textes du jeu**, pas des numéros fabriqués par le code. Ils sont dans `pac/string.pac`, dans les tables
« SDB » (enregistrements de 16 octets {décalage, longueur, numéro, 0}, le format déjà décrit dans `wwe12_quit.cpp`).

| Textes | Numéros | Nombre |
|---|---|---|
| DIVA 01-15, puis 16-20 | 8200-8214, 8266-8270 | 20 |
| SUPERSTAR 01-40, 41-50, 51-55 | 8215-8254, 8316-8325, 8630-8634 | 55 |
| LEGEND 01-15 | 8275-8289 | 15 |
| MIXED TAG 01-05, 06-10 | 8290-8294, 8326-8330 | 10 |
| TAG 01-10, 11-20, 21-29 (et un autre ailleurs) | 8295-8304, 8331-8340, 8560-8568 | 30 |
| TRIO 01-05, 06-10 | 8305-8309, 8311-8315 | 10 |
| TAG LEGEND 01-05 | 8818-8822 | 5 |

Ces numéros sont mêlés aux autres textes de CRÉER UNE ENTRÉE : 8010 CUSTOM ENTRANCE VIDEO, 8012 CREATE-AN-ENTRANCE,
8056+ MUSIC 01…, 8265 CUSTOM VIDEO, 8310 JEALOUS MIXED TAG. Juste après viennent les entrées prédéfinies **qui ont
déjà un nom** (8341 BATISTA, 8342 BETH PHOENIX… 8361 JOHN CENA, 8362 JOHN CENA (CHAMPION)…). Les « SUPERSTAR NN »
sont donc les entrées **sans catcheur dans le jeu** : celles des anciens jeux (Kurt Angle, RVD, Umaga, MVP…), plus
quelques entrées génériques (« Generic », « Boxer », « Zombie »…, d'après le site).

**Langues.** Le texte est dans chaque langue du disque (groupes `SDB 00LL`, `04LL`, `53LL`, `57LL`, `63LL` : 5 jeux
de tables, LL = langue 00 à 08). En anglais, français, allemand, espagnol et italien, il est le même partout :
« SUPERSTAR 01 », non traduit. Seul le japonais (LL = 06) a « スーパースター 1 ». Un seul nom par entrée suffit donc
pour les 5 langues du portage.

**Move-sets.** Le site compte 30 move-sets prédéfinis (Chris Masters, JTG, Primo… Roddy Piper). Le disque n'a
**aucun autre texte** du genre « MOVE-SET 01 » ou « PRESET 01 ». J'ai cherché dans les 6 650 textes anglais. La liste
des move-sets affiche donc très probablement **les mêmes textes SUPERSTAR 01-30** (8215-8244). Si c'est vrai, c'est le
point clé : SUPERSTAR 01 serait Kurt Angle dans les entrées mais Chris Masters dans les move-sets. **Renommer le
texte ne suffit pas** : il faut savoir quel écran le demande. À confirmer en jeu, voir plus bas.

Un piège à ne pas confondre : `misc.pac` (table des prises `MOVS:WAZE`) a aussi des prises nommées « Superstar 1-3 »
et « Diva 1-16 ». Ce sont des noms internes de prises, en anglais seulement, qui ne sont pas dans `string.pac`. Ce
ne sont pas les libellés de ces deux listes.

**Ce que le site ne dit pas.** Le jeu a plus de textes que le site n'a d'entrées : 55 SUPERSTAR (le site en liste 45),
20 DIVA (11), 15 LEGEND (12), 30 TAG (22), 10 TRIO (6), 10 MIXED TAG (7), 5 TAG LEGEND (3). Les textes en trop sont
peut-être des places vides ou des entrées des DLC. **L'ordre du site n'est pas forcément celui du jeu** : il faut
vérifier en jeu chaque numéro avant de lui donner un nom.

## Comment les remplacer sans toucher au disque

Deux façons, toutes deux en mémoire (rien n'est écrit sur le disque, et un cvar d'arrêt comme les autres, par
exemple `wwe12_preset_names`, défaut true) :

1. **Réécrire les tables chargées**, comme on le fait déjà pour « Xbox LIVE » → « ONLINE » (`wwe12_quit.cpp` :
   recherche dans la mémoire du jeu, enregistrement retrouvé par son numéro). Problème de place : « SUPERSTAR 01 »
   fait 12 octets, et la plupart des noms sont plus longs (« SHANE McMAHON », « JIMMY WANG YANG »). On ne réécrit
   donc pas le texte sur place : on écrit le nom dans une petite zone de mémoire du jeu qu'on réserve, et on change
   le **décalage** de l'enregistrement pour qu'il pointe dessus. Le décalage est un u32 compté depuis le début de la
   table, donc toute adresse au-dessus de la table marche ; on change aussi la longueur. Ça marche pour toutes les
   langues et pour la copie de `string.pac` que porte chaque DLC (le jeu charge la plus récente). Mais **c'est le
   même texte pour les deux écrans**.
2. **Accrocher la fonction du jeu qui lit un texte par son numéro** (pas encore trouvée), et rendre un autre nom
   selon l'écran ouvert. C'est nécessaire seulement si les move-sets utilisent les mêmes numéros que les entrées.
   On retrouve cette fonction comme on l'a fait pour la grille de lettres (`wwe12_kbd.cpp`) : un piège PAGE_GUARD
   sur l'enregistrement 8215 pendant qu'on ouvre la liste, puis l'appelant. Il faut ensuite reconnaître l'écran de
   la liste des move-sets, par son nom de page ou son objet.

Les noms resteraient en anglais et en majuscules, comme les autres noms de catcheurs du jeu. Les entrées génériques
(Boxer, Zombie, Cartwheeling Diva…) prendraient un libellé descriptif plutôt qu'un nom.

## Catcheurs ajoutés par le Mod Maker

Rien ne change pour eux. Les entrées et move-sets prédéfinis sont des places fixes du jeu, attachées à aucun
emplacement de catcheur. Un catcheur ajouté (DLC maison) ou remplacé garde sa propre entrée et ses propres coups
(`+0x1D4`, voir `plan_movesets.md`), et le Mod Maker n'ajoute pas de prédéfinis. Si un mod remplace par exemple Kurt
Angle, l'entrée « KURT ANGLE » garde son nom : c'est l'entrée d'origine, ce qui reste juste. Rien n'est à faire dans
le Mod Maker lui-même.

## Étapes et effort

| Étape | Contenu | Effort |
|---|---|---|
| 1 | **En jeu (avec le « go » de l'utilisateur)** : ouvrir les deux listes avec une sonde qui note les numéros de textes lus ; dire si les move-sets lisent 8215-8244 ; relever l'ordre réel des entrées (numéro → catcheur), écran par écran | ½ séance (et l'utilisateur, ou la télécommande `wwe12_remote_pad` + captures) |
| 2 | Liste des noms (vérifiée à l'étape 1, crédit au site), réécriture des tables chargées (façon 1), cvar d'arrêt, doc | ½ à 1 séance |
| 3 | Seulement si les move-sets partagent les textes : trouver la fonction de lecture des textes, l'accrocher, reconnaître l'écran des move-sets | +1 séance |

Total : **1 à 1,5 séance** si les deux listes ont leurs propres textes, **2 à 2,5** si elles les partagent (le plus
probable d'après le disque). Rien sur GitHub en données : le code ne contient que des noms de catcheurs (des faits) et
les numéros de textes, pas de texte du jeu.

Planification : prévu pour la 1.1. L'utilisateur dira s'il le veut dans la 1.0.
