# Étude : cinématiques d'autres jeux et Story Designer + Univers

Conversation « Cinématiques / Story Designer ». Étude du 2026-11-16, **en lecture seule** : rien n'a été lancé, rien
n'a été compilé, aucun code du jeu n'a été modifié. Les données lues dans WWE '12, WWE '13 et WWE 2K14 ont été
décompressées dans un dossier temporaire puis supprimées. Reprise prévue en janvier 2027, après la 1.4.

Légende : **[PROUVÉ]** = lu dans le code ou dans les fichiers ; **[PROBABLE]** = déduit, pas lu jusqu'au bout ;
**[À VOIR EN JEU]** = seul un essai avec journal peut le dire.

## 1. En bref

- **Faisable : oui**, pour les deux objectifs, par morceaux jouables.
- **L'écran noir est expliqué [PROUVÉ]** : une scène 4000-4999 reçoit toujours le lecteur « Create a Story », et ce
  lecteur ne démarre que si le jeu est en mode « histoire jouée ». Dans l'Univers il ne démarre jamais. Ce n'était ni
  le décor, ni les rôles vides, ni les scripts manquants. Un seul petit crochet devrait suffire (§2.4).
- **La bibliothèque de Create a Story est un fichier de données [PROUVÉ]** : `pac/menu/sas.pac`, `SASE/DATA`.
  197 scènes en 17 catégories. Ajouter une scène dans une catégorie existante = données seulement (§3).
- **Les 64 scènes nouvelles sont confirmées [PROUVÉ]** (§4) : 18 n'ont besoin de rien de nouveau, 12 utilisent des
  commandes que WWE '12 n'utilise jamais, 7 (APA) demandent 7 objets et un lieu inconnu, 26 sont des intros de show
  liées à des arènes que WWE '12 n'a pas (à laisser de côté).
- **Estimation** : 45 à 75 heures et 20 à 30 essais de l'utilisateur pour tout sauf les intros de show (§6).
- **Données** : aucune scène d'un autre jeu dans le dépôt ni dans le zip. Le Mod Maker les convertit chez le joueur,
  depuis son propre disque, comme pour les entrées.

## 2. Pourquoi une scène de Create a Story donne un écran noir dans l'Univers

### 2.1 Ce que valent vraiment les deux « modes »

L'ancienne lecture (`docs/univers.md` §44 : « +12456 = 2 et +12460 = 4 = mode Create a Story ») était fausse.
G = `sub_8267CF38()` = 0x8317C850.

- **G+12456 = l'étape en cours du gestionnaire de scènes [PROUVÉ]**, écrite par `sub_828CB960` selon le genre de la
  case : 2 = le match, 6 = une entrée, 7 = une cinématique. Ce n'est pas un mode.
- **G+12460 = le mode de jeu.** Il est recopié depuis l'objet de lancement par `sub_82A38718`, et lu à la fin par
  `sub_8267EE48` (un `switch` qui décide ce qui vient après).

| Valeur | Sens | Preuve |
|---|---|---|
| 0 | exhibition, Univers | remis à 0 par de nombreuses fonctions ; journal du 2026-11-13 |
| 2 | Road to WrestleMania | `sub_824400E0` écrit 2 (à côté de « CRTWBeginSequence ») [PROBABLE] |
| 3 | **une histoire créée en train d'être jouée** | `sub_823E3220` écrit 3 en 0x823E3774 [PROUVÉ] |
| 4 | **l'éditeur de Create a Story (aperçu)** | seul `sub_8275A958` le pose [PROUVÉ] |
| 9 | « recommencer » depuis la pause | [PROBABLE] |
| 1, 5-8 | inconnus | |

Conséquences : `sub_8275A958` n'est pas le lecteur d'histoires, c'est **l'aperçu de l'éditeur** (scène 4000 par défaut,
six rôles vides). Le vrai lecteur est `sub_823E3220`. Le journal « 2 / 4 puis 7 / 3 » d'une histoire jouée se lit donc :
l'éditeur lance sa coquille (mode 4, scène 4000), puis les vraies scènes passent en mode 3.

### 2.2 La chaîne qui laisse l'écran noir [PROUVÉ]

1. Le réalisateur d'une scène est choisi **par son numéro**, pas par le mode (`sub_828CA9D0`) : 1000-3999 → Road to
   WrestleMania ; **4000-4999 → Create a Story** (`sub_828C6B70`) ; le reste → réalisateur de base (entrées, victoires).
2. Avant la scène, l'écran de chargement s'affiche et le décor est caché (`sub_828DFF68`).
3. Le lecteur de Create a Story n'avance que par `sub_828D40B8` :
   `si (mode == 8 || mode == 3 || sub_828CE648(éditeur)) → sub_828D9E40`.
   `sub_828CE648` rend 0 dès que le mode n'est pas 4.
4. Dans l'Univers le mode vaut 0 : `sub_828D9E40` n'est jamais appelé (c'est son seul appelant). Donc
   `sub_828DB508`, qui **remontre le décor** et lance caméra, script et mouvements, ne tourne jamais.
5. Le réalisateur attend pour toujours la fin d'un script qui n'a pas commencé [PROBABLE].

C'est exactement ce qui a été vu les 3 premiers essais : tout se charge, 30 images/s, des images sont présentées,
écran noir sans fin. Les fichiers `/PGEN`, `/GMEV`, `/PCE6` introuvables sont normaux (WWE '12 n'a aucun groupe PGEN ;
seules les intros de show ont un GMEV).

### 2.3 Pourquoi le 4e essai (mode 4 forcé) revenait à Create a Story [PROUVÉ]

- En mode 4 sans aperçu en cours, `sub_828DDAC0` arrête la scène tout de suite : rien à l'écran.
- À la fin de la séquence, `sub_821D4CE0` puis `sub_8267EE48` regardent le mode : s'il vaut encore 4, retour au menu
  de Create a Story. Notre crochet laissait le 4 en place.

**Il ne faut donc jamais toucher au mode.**

### 2.4 La correction proposée (pas écrite, pas essayée)

- Garder l'injection d'une demande dans la table (`sub_828D2660`, déjà écrite, coupée par défaut).
- **Un crochet sur `sub_828CE648`** : rendre 1 quand la scène en cours (`sub_828C2208()`) est la nôtre, sinon appeler
  l'original. Ses deux appelants utiles (`sub_828D40B8`, `sub_828D4DA0`) l'appellent sans condition ; les autres ne
  l'appellent qu'en mode 4. Reste à lire : `sub_828EDB10`.
- Rien à faire à la fin : en mode 0, `sub_8267EE48` continue le flux de l'Univers tout seul.
- Ne pas simuler le mode 3 : d'autres fonctions du mode 3 utilisent l'objet histoire (`*0x832317CC`), vide ici.

Autre voie pour les scènes portées [PROBABLE] : leur donner un numéro **hors de 1000-4999**. Elles reçoivent alors le
réalisateur de base, sans la porte du mode (c'est ainsi que les entrées marchent partout). Utile pour l'Univers, pas
pour la bibliothèque (qui n'accepte que 4000-4999).

### 2.5 Ce que seul un essai avec journal dira [À VOIR EN JEU]

| Question | Où poser le journal |
|---|---|
| La lecture démarre-t-elle ? | `sub_828D40B8`, `sub_828D4DA0`, `sub_828DB508` |
| Le catcheur de la scène a-t-il un modèle ? Dans une vraie histoire, `sub_823E3220` inscrit chaque rôle comme participant du match (`sub_82A323F8`). Sinon : acteur invisible | `sub_828DFC90` (rôles de la case, 6 participants) |
| La scène se termine-t-elle ? | `sub_828DBE80`, `sub_828DF770` |
| Le décor revient-il pour les entrées ? | `sub_828C2AE8`, `sub_828C4368` |
| Passer la scène : en mode 0, `sub_828D2190` annule aussi les entrées | `sub_828DF770` |
| Scène après la cloche : quel genre est lu ? | `sub_828D2660` (r4, lr), `sub_823F6B38` |

## 3. Comment la bibliothèque liste ses scènes

### 3.1 Le fichier [PROUVÉ]

`pac/menu/sas.pac`, groupe `SASE`, entrée `DATA` : un PACH de 7 morceaux, petit-boutiste sur le disque.

| Morceau | Contenu |
|---|---|
| 0 | **la bibliothèque** : 17 catégories, 197 scènes (fiches de 24 octets) |
| 1, 2 | listes de sons et de noms |
| 3 | **embranchements** : 11 questions de 44 octets (3 réponses chacune) |
| 4, 5 | événements de commentaire |
| 6 | **placement** : une fiche de 88 octets par scène (angles et positions par lieu) |

197 et non 201 : les scènes 4055-4058 (variantes de ceinture) ont des fichiers mais pas de fiche.

Catégories (nombre de scènes) : ATTACK 12, BRANCHING 41, CAGE 4, COMMENTARY 3, DIVA 13, ENCOUNTER 16, FOUND 2, GM 10,
INTERVIEW 2, MID-MATCH 9, OUTSIDE 10, POST-MATCH 14, PRE-MATCH 10, PROMO 18, PROP 6, SHOW-INTRO 22, UNION 5.

Fiche d'une scène (24 octets) :

| Octet | Contenu |
|---|---|
| +0 | catégorie |
| +1 | genre 0-4 (1 = avant / pendant / après match, 4 = embranchement) [PROBABLE] |
| +2, +3 | contraintes de distribution (Diva…), lues par `sub_82787468` |
| +6, +7 | options (bureau du GM…) |
| +10 | **masque des lieux permis** : 0x01 ring, 0x40 bureau du GM, 0x80 parking ; autres bits à préciser |
| +12 | **numéro de la scène** |
| +14 | déblocage (0xFFFF partout) |
| +16, +20 | n° du texte du nom, n° du texte de la description (`string.pac`) |

**Les rôles ne sont pas dans la fiche** : l'éditeur les tire du script de la scène (pistes de personnages, 6 au plus,
`sub_8276D2A0`). Le « CAST:2 » affiché est un simple texte.

Vignettes : `/RSCP/NNNN` dans `menuHD.pac` / `menuSD.pac` (194 images). Facultatives : sans vignette le jeu continue.

### 3.2 Le code du menu [PROUVÉ]

- Éditeur : `*0x832317D0` ; bibliothèque à +120, construite par `sub_8277CB00`, lue par `sub_8277CEE0`.
- Recherche par numéro : `sub_8277CA20`, **recherche simple** (pas de tri à respecter), numéros **4000 à 4999**
  seulement, **17 catégories écrites en dur**.
- Liste du menu : `sub_8278C2C8` (tableau de 17 catégories sur la pile).
- Aucun « 197 » ou « 201 » en dur trouvé : le nombre vient du fichier.
- **Une histoire sauvegardée garde le numéro de la scène**, pas sa place dans la table. Ajouter des fiches ne casse
  donc aucune histoire existante.

### 3.3 Ce qu'il faut pour ajouter UNE scène

Par les données seules (copies servies par l'overlay, mécanisme de Mods) :

1. les fichiers `/EVPx/N` et `/EVZx/N` sous un numéro libre de 4000-4999, dans une copie servie d'un `RSC*.pac` (le jeu
   cherche dans tous les groupes montés, `sub_828CCC10`) ;
2. une fiche de 24 octets dans le morceau 0, dans une **catégorie existante** ;
3. deux textes (nom, description) : par le crochet de textes existant (`sub_821A4400`) ;
4. une fiche de placement de 88 octets (morceau 6), copiée d'une scène jouée de la même façon ; sans elle le jeu prend
   celle de la scène 4105 ;
5. facultatif : une vignette `/RSCP/NNNN` ;
6. pour une question : une fiche de 44 octets dans le morceau 3.

Il faut du code seulement pour : une 18e catégorie (dépassement de mémoire sinon), ou protéger un joueur sans le mod
qui reçoit une histoire avec une scène inconnue (`sub_82787468` lit une fiche nulle en 0x82787494 : plantage probable).

Risques : rester à 41 scènes au plus par catégorie (le plus grand nombre livré ; capacité de la liste non vérifiée) ;
un mauvais masque de lieux rend la scène inchoisissable ; les histoires qui utilisent ces scènes ne marchent pas dans
un jeu sans le mod.

## 4. Ce qu'il faut prendre dans WWE '13 / 2K14

### 4.1 Comptes [PROUVÉ]

| Jeu | Paquets | Scènes |
|---|---|---|
| WWE '12 | `RSC0-3.pac` (EVPU-X / EVZU-X) | 201 |
| WWE '13 | `WSD00-05.pac` (xcompressés) | 254, dont 53 absentes du 12 |
| 2K14 | `wsd00-04.pac` (xcompressés) | 263, dont 63 absentes du 12 |

64 scènes nouvelles en tout (les 53 de WWE '13 + 4285-4295 de 2K14). Trois sont hors de 4000-4399 : 4651, 4662, 4670.

**Même numéro = même scène** dans les trois jeux (nom, acteurs, durée), **sauf les intros de show** (4226-4244,
4263-4265), où le numéro désigne un autre show dans chaque jeu. Ne jamais prendre une intro de show par son numéro.

La bibliothèque de WWE '13 a 531 fiches de 40 octets (18 catégories : + ENTRANCE) ; les 16 premiers octets sont ceux
de WWE '12 : on peut en tirer catégorie, masque de lieux et options de chaque scène nouvelle.

### 4.2 Format [PROUVÉ]

- Scripts EVP : même forme dans les trois jeux.
- Banques EVZ : même contenant. Différences déjà gérées par `entrance13.h` : noms de pistes de 2K14, jambes en IK à
  cuire. Les corps ont 63 pistes au lieu de 95 : les 32 en moins sont sans doute le **visage** → visages figés dans
  les scènes parlées tant que `face12.h` ne les ajoute pas [À VOIR EN JEU].
- Une piste porte un bit (0x0002) que la bibliothèque de WWE '12 n'a jamais.
- **Commandes qu'aucune scène de WWE '12 n'utilise** : 53, 54, 55 (cordes du ring, probable), 186 (effet d'écran des
  scènes « Titantron Message », probable), 0. On ne sait pas si le code de WWE '12 les connaît : il faudra lire sa
  table des commandes, ou les retirer.

### 4.3 Les 64 scènes, par besoin

| Groupe | Scènes | Nombre |
|---|---|---|
| a. rien de nouveau | 4125, 4167, 4281, 4282, 4355, 4358, 4370, 4372, 4373, 4662 | 10 |
| b. objets que WWE '12 a déjà (chaise 2034, micro 2038, ceinture 2009) | 4071, 4165, 4283, 4380, 4383, 4391-4393 | 8 |
| c. commandes inconnues | 4123, 4164, 4356, 4357, 4371 ; 4166, 4168, 4284, 4381, 4382, 4390 ; 4670 | 12 |
| d. APA : 7 objets absents (2697-2699, 2702, 2703, 2708, 2709 ; 2 Mo) + lieu de code 81 | 4069, 4070, 4124, 4385-4388 | 7 |
| e. intros de show (arènes absentes du 12, GMEV et PCE6 à elles) | 4266-4280, 4285-4295 | 26 |
| hors bibliothèque | 4651 (WWE '13 seulement, sans nom) | 1 |

Durée totale : environ 15 minutes. Acteurs : 15 scènes à 2, 18 à 3, 4 à 4. Les embranchements vont par familles
(une question + 3 réponses) : une famille dont une réponse est du groupe c attend le groupe c.

Lieux : 149 scripts sur 201 ne nomment aucun lieu (le joueur le choisit, dans le masque de la fiche). Les lieux fixes
(bureau du GM 68, restaurant 67, parking 83, 64) sont des codes ; **le code 81 des scènes APA n'existe dans aucun
script de WWE '12** : à chercher dans le code.

Textes : les noms et descriptions des scènes nouvelles n'existent pas dans WWE '12 ; à écrire nous-mêmes en anglais
(et FR / DE / ES / IT), sans recopier ceux des autres jeux dans le dépôt.

## 5. Les deux objectifs

**Objectif 1, cinématiques d'autres jeux.** Deux endroits où les jouer :
- dans **Create a Story** : bibliothèque étendue (§3.3), le lecteur marche déjà (mode 3) ;
- dans **l'Univers** : après la correction du §2.4, ou sous un numéro hors 1000-4999.

**Objectif 2, Story Designer + Univers.** Trois niveaux, du plus simple au plus lourd :
1. des scènes de la bibliothèque jouées dans l'Univers (embuscade en coulisses à la place d'une intervention, idée de
   Jay : RUN DOWN, CAR WINDOW 4120) ;
2. des scènes choisies par les histoires de l'Univers (fiches WCID / STYT ajoutées, avec les rôles de l'histoire) ;
3. une histoire créée jouée depuis l'Univers avec ses catcheurs et ses résultats : le lecteur d'histoires dépend de
   l'objet histoire et du mode 3 ; non étudié, exploratoire.

**À décider par l'utilisateur en janvier** : jusqu'à quel niveau aller (le 1 et le 2 sont réalistes).

## 6. Étapes, heures, essais

Chaque étape est jouable seule et coupée par défaut. Heures de travail de Claude ; un essai = un lancement du jeu par
l'utilisateur.

| Étape | Contenu | Heures | Essais | Risque |
|---|---|---|---|---|
| A | Scène de WWE '12 jouée dans un match d'Univers : crochet `sub_828CE648` + journal du §2.5, acteur inscrit comme participant | 6-10 | 3-5 | moyen |
| B | Embuscade en coulisses : choix au moment d'INTERFERE, lieu, résultat, news | 10-15 | 4-6 | moyen |
| C | Première scène portée dans la bibliothèque (groupe a) : conversion par le Mod Maker, copie servie, fiche, placement, textes | 12-18 | 4-6 | moyen |
| D | Les 17 autres scènes des groupes a et b, questions comprises, vignettes | 8-12 | 3-4 | faible |
| E | Groupe c : lire la table des commandes du jeu, garder ou retirer 53 / 54 / 55 / 186 / 0 | 6-12 | 3-5 | moyen à élevé |
| F | Groupe d (APA) : 7 objets depuis le disque du joueur, lieu 81 | 6-10 | 2-4 | élevé |
| G | Scènes portées dans l'Univers (niveau 2 du §5) | 8-15 | 3-5 | moyen |
| — | Intros de show (groupe e) | non conseillé | | |
| — | Histoire créée jouée depuis l'Univers (niveau 3) | 20-40, à étudier d'abord | | élevé |

Total A à G : **environ 55 à 90 heures, 22 à 35 essais**. Sans F et G : 42 à 67 heures. Ordre conseillé : A (elle
valide la cause de l'écran noir), puis C et D, puis B, puis E.

## 7. Risques

- La correction du §2.4 n'a été lue que dans le code : l'acteur invisible et la fin de scène restent à voir en jeu.
- Visages figés et commandes inconnues des scènes portées.
- Table des 17 catégories : ne jamais en ajouter une par les données.
- Histoires partagées avec des scènes ajoutées : plantage possible chez un joueur sans le mod.
- Droits : scènes, objets et textes des autres jeux restent chez le joueur (conversion locale), rien dans le dépôt.
- Les `sas.pac`, `RSC*.pac`, `menuHD.pac` servis doivent suivre la règle de Mods : ajouter à la fin d'une copie, ne
  jamais reconstruire l'archive.

## 8. Réglages, tous coupés par défaut

- Existants (Univers, dev) : `wwe12_universe_scene_inject`, `wwe12_universe_scene_table_probe`,
  `wwe12_universe_scene_inject_mode` (ce dernier est à **retirer** : il force le mode 4, cause du retour à Create a
  Story).
- À créer : un réglage pour les scènes de bibliothèque dans l'Univers, un pour la bibliothèque étendue. Sans mod de
  scènes installé, la bibliothèque doit rester exactement celle du jeu.

## 9. Pour reprendre

- Outils de lecture du code (sans donnée du jeu) : `tools/dev/lecture_code/` — `fn.py` (une fonction en clair, ses
  appelants), `grepfn.py`, `around.py`, `modecmp.py` (valeurs comparées après la lecture d'un champ), `mem.py`
  (textes et données de `image.bin`). Le premier lancement de `fn.py` fabrique un index (1 à 2 minutes).
- Les scripts de lecture des données (paquets WSD, `sas.pac`, pistes, commandes) étaient dans le dossier temporaire
  de la conversation et peuvent avoir disparu ; tout ce qu'ils ont trouvé est dans ce document. Lecteurs à réutiliser :
  `port/src/modmaker/svrfmt/`, `entrance13.h`, `tools/dev/w13_scene_convert.cpp`.
- À corriger ailleurs (pour la Main / Univers) : `docs/univers.md` §44 (sens de +12456 / +12460, rôle de
  `sub_8275A958`) ; ligne de la feuille de route (« 201 scènes » : 201 fichiers, 197 dans la bibliothèque).
- Pas encore lu : la table des commandes de scène du jeu ; le code des lieux 64 / 81 / 83 ; `sub_828EDB10` ; qui lit
  le texte tapé par le joueur pendant une scène ; les groupes SAS0-SAS5 de `sas.pac` (histoires d'exemple).
