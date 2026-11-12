# Launcher PC : plan d'amélioration (1.2) — écrit le 2026-11-06

> **État au 2026-11-06** : PLAN SEULEMENT. Rien n'est codé, rien n'est compilé. Demande de l'utilisateur relayée par Main
> (« Launcher doit bosser pour améliorer le launcher ainsi que le design »). Ne touche pas à la 1.1 : tout ceci va dans la 1.2,
> sur des commits locaux, sans push. Les heures sont des heures de TRAVAIL (code + essais hors jeu), **sans** les tests
> de l'utilisateur ni des testeurs (marqués « test iel »). Maquettes : supprimées (design = look actuel, voir §5) ;
> (ancienne note sur les maquettes, sans objet)
>

## 0. Ce qui existe (pour fixer les idées)

- Fenêtre fixe 1120 × 780 (× DPI), menu à gauche (280 px, 7 gros boutons obliques or / rouge), page à droite.
- Pages : JOUER, RÉGLAGES, CONTRÔLES, MODS, SAUVEGARDES, MUSIQUE, À PROPOS. Polices Windows (Segoe UI 19, Impact 40).
- ImGui sans navigation manette ni clavier ; boutons ~28 px ; textes EN/FR dans la structure `Texts`.
- Page JOUER : version, jeu trouvé (vert) ou manquant (rouge), carte graphique utilisée, JOUER, « Jouer sans mods »,
  « Ajouter des DLC... », mises à jour, nouveautés.

## 1. Liste priorisée (gain / risque / durée)

Priorité = gain pour les joueurs ÷ risque. Durées = heures de travail hors tests.

| # | Amélioration | Gain | Risque | Durée | Test iel |
|---|---|---|---|---|---|
| 1 | **Confort Steam Deck** (§2) : manette, fenêtre ≤ 800 px, échelle × 1,25, grands boutons | élevé (aucune issue Deck encore, mais c'est la cible n°1 de la 1.2) | moyen (Proton non testable ici) | 5 – 7 h | oui + testeur Deck |
| 2 | **État du jeu sur JOUER** (§6a) : version du jeu / TU, fichiers trouvés en détail, carte, API, mémoire, profil Deck/Wine | élevé (moins de questions « ça ne marche pas ») | faible | 2 – 3 h | non (lecture) |
| 3 | **Bouton « Test de performance »** (§3) | élevé (chiffres comparables des joueurs) | moyen (dépend du premier essai de Rendu) | 3 – 4 h | oui, avec « vas-y » |
| 4 | **Messages d'erreur plus clairs** (§6b) : jeu manquant, Vulkan manquant, exe d'API absent, partie déjà lancée, import refusé | moyen-élevé | faible | 2 – 3 h | non |
| 5 | **« Copier les infos de diagnostic » plus visible** (§6c) + liens FAQ / Discord / issues | moyen | faible | 1 – 1,5 h | non |
| 6 | **Option FRAME RATE** (§4) + profil Deck à 30 | moyen-élevé (mode 30 fps = gros gain petits PC) | faible côté launcher (la clé est à Rendu) | 1,5 – 2 h | après le code de Rendu |
| 7 | **Finitions visuelles** (§5, look actuel gardé) | moyen | faible | 3 – 4 h | oui (goût) |
| 8 | **Bouton « Vérifier le jeu »** (§6d) : fichiers attendus présents, tailles, version | moyen | faible | 2 h | non |
| 9 | Textes EN/FR relus, libellés CONTRÔLES en français (déjà noté) | faible-moyen | très faible | 1 – 1,5 h | relecture iel |
| 10 | Page ROSTERS | — | — | **1.3 : FAIT 2026-11-10** (exhibition seulement, voir docs/launcher_rosters.md §9) | — |

Total hors finitions : **≈ 17 – 24 h** ; avec les finitions : **≈ 20 – 28 h**. Ordre conseillé : 2 → 4 → 5 → 1 → 3 → 6 → 8 → 9 →
7 en dernier (le design touche tout : le faire après, pour ne pas refaire le confort Deck deux fois ; mais le **choix** de la
direction doit venir tôt, car il fixe la taille des boutons du confort Deck).

Séquence réaliste : séance 1 (≈ 6 h) = 2, 4, 5 ; séance 2 (≈ 6 h) = 1 ; séance 3 (≈ 5 h) = 3, 6, 8 ; séance 4 (3 – 4 h) = finitions visuelles.

## 2. Confort Steam Deck (détail)

Base : `docs/launcher_steam_deck.md` §3.

1. **Manette** (1,5 – 2 h) : `io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad | NavEnableKeyboard`. Les `BigButton` sont des
   `InvisibleButton` : ils entrent dans la navigation, mais le focus n'est pas dessiné → dessiner un cadre or sur
   `IsItemFocused()`. Brancher XInput : ImGui 1.92 n'en lit pas seul avec notre backend Win32 maison ; ajouter la lecture XInput
   (A = valider, B = retour, croix = navigation, LB/RB = page précédente / suivante) dans la boucle. Sous Proton, Steam Input
   présente la manette en XInput : à confirmer par un testeur. Risque : pages avec capture de touche (CONTRÔLES) → la navigation
   manette y est coupée pendant la capture.
2. **Fenêtre dans 1280 × 800** (1 h) : au démarrage, borner la taille à la zone de travail (`SPI_GETWORKAREA`) ; si la hauteur
   disponible < 780 + cadre, démarrer maximisée ; menu à gauche plus étroit (220) et boutons plus bas (44) en dessous de 800 px.
   Les pages défilent déjà (`BeginChild`) : vérifier qu'aucun bouton clé (JOUER) ne sort de l'écran.
3. **Échelle × 1,25 en mode Deck** (1 h) : `FontScaleDpi` / `ScaleAllSizes` quand `machine::Choose` dit Deck (ou
   `SteamDeck=1`), plus une case « Grande interface » dans RÉGLAGES pour les autres (réglage du LAUNCHER seulement, clé dans
   `launcher.ini`, pas dans `wwe12.toml` : le jeu ne l'ignorerait pas mais on évite d'ajouter une clé du jeu).
4. **Boutons plus grands** (1 h) : hauteur mini 44 px en mode Deck (cases à cocher, listes, boutons ImGui) via `FramePadding`.
5. **Fichiers** (0,5 h) : sur Deck, les boîtes de choix de fichier Wine sont utilisables mais peu pratiques ; ajouter la phrase
   « DLC / sauvegardes : copiez les fichiers dans le dossier indiqué » comme solution de repli (texte, pas de code).
6. **Liens** (0,5 h) : `ShellExecute` sous Proton non vérifié ; afficher aussi l'adresse en texte copiable.
- Test : hors Deck, `SteamDeck=1` + fenêtre redimensionnée à 1280 × 800 par `resize` ; test XInput avec la manette Xbox de
  l'utilisateur. Un vrai Deck = testeur (Discord).

## 3. Bouton « Test de performance »

Source : `docs/perf_natif_steam_deck.md` (« Test de performance intégré », Rendu, commit 85441d9, **écrit mais pas encore
essayé en jeu**).

- Où : page JOUER, sous « Jouer sans mods » : bouton « Test de performance » + courte explication (≈ 3 minutes, la manette n'est
  pas nécessaire, la sauvegarde n'est pas touchée : dossier `Documents\wwe12_benchmark`).
- Lancement : même exe que le bouton JOUER (`gpu_backend`), avec `--user_data_root "<Documents>\wwe12_benchmark" --wwe12_benchmark=true`
  (le .bat de Rendu efface d'abord ce dossier ; le launcher fait pareil, **seulement ce dossier-là**, après vérification du nom).
  Le launcher attend la fin (le jeu se relance seul pour la passe 2 : suivre le résumé plutôt que le processus) ; il ne
  permet pas de lancer le jeu normal pendant ce temps.
- Résultat : lire `logs\benchmark_*.txt` le plus récent, montrer le **verdict** (OK 60 fps / limité par le processeur /
  limité par la carte graphique ou l'écran), les chiffres clés (i/s moyen, 1 % pires, API, échelle, Windows / Proton) et deux
  boutons : **Copier le résultat** (presse-papiers, comme les infos de diagnostic) et **Ouvrir le dossier**.
- Garde-fous : refuser si le jeu tourne ; avertir avant le lancement (dialogue : « ça lance le jeu, ~3 min, sans toucher à vos
  sauvegardes ») ; aucune donnée envoyée.
- Durée : 3 – 4 h (lancement + attente + lecture du fichier + affichage) ; dépend du format exact du fichier de Rendu (à lire sur un
  vrai résumé : il n'existe pas encore).
- Test iel : lancer en vrai (avec son « vas-y »), une fois, sur ce PC.

## 4. Option FRAME RATE (mode 30 fps, 1.2)

- Clé `wwe12_frame_mode` = `original` / `30` / `60_entrances` (Rendu la code ; l'ancienne `wwe12_60fps` est relue par le jeu).
- Launcher : une liste « Fréquence d'images » dans RÉGLAGES > GRAPHISME (remplace la case « 60 fps en entrées ») avec 3 entrées
  claires : « Originale (60 en match, 30 en entrées) », « 30 partout (petits PC, Steam Deck) », « 60 aussi en entrées ».
  Lecture : si seule `wwe12_60fps = true` existe → afficher « 60 aussi en entrées ».
- Profil Steam Deck / petit PC : le préréglage mettra `30` quand Rendu aura validé le mode (champ `frame_mode` ajouté à `Preset` et
  à la copie de sécurité, comme `gpu_backend`).
- 1,5 – 2 h une fois la clé en place ; **ne rien écrire dans `wwe12.toml` avant** (clé inconnue du jeu).

## 5. Design : DÉCISION DE L'UTILISATEUR (2026-11-06) — le look actuel est très bien, on l'améliore seulement

> « Le launcher PC est très bien, juste l'améliorer encore. » Donc **pas de refonte** : les directions B (jaune / noir Android) et C
> (menu en haut) ci-dessous sont **abandonnées** (gardées pour mémoire). Seule la direction A, en petites touches, reste au plan
> (≈ 3 – 4 h au lieu de 6 – 12 h) : ligne d'état en pastilles sur JOUER, espacements réguliers, quelques icônes simples,
> focus manette visible. Rouge / or, coups de pinceau et Impact ne changent pas. Les maquettes HTML ont été supprimées.

**A. « Même look, plus propre » (le launcher actuel affiné)** — 6 – 7 h
Garde le rouge et l'or, les coups de pinceau obliques, Impact. Change : cartes (panneaux arrondis) pour regrouper les réglages,
icônes simples devant chaque page, états vides plus clairs, espacements réguliers, ligne d'état en haut de JOUER (pastilles
« Jeu trouvé · Vulkan · RTX… · 1.1.0 »). Risque faible, identité PC conservée.

**B. « Jaune et noir commun avec Android »** — 9 – 12 h
Reprend la nouvelle identité de Yexclsv (`android/game/UI_REDESIGN.md` : fond noir, accent jaune, titres condensés italiques,
panneaux arrondis avec marque de titre, interrupteurs jaunes à la place des cases, bande « caution tape » sous les titres,
barre de chargement animée). Gain : une seule image PC / Android, captures de la FAQ cohérentes. Coût : remplacer cases à cocher
par interrupteurs (widget maison), nouvelles polices (condensée italique : à choisir, sans police livrée si possible : Bahnschrift
existe sous Windows 10/11 ; sous Proton elle manque → repli Segoe), barre animée. Les boutons obliques rouges disparaissent.

**C. « Compact manette / Steam Deck »** — 8 – 10 h
Menu en haut (onglets larges) au lieu d'à gauche, une page = un écran sans défilement, grosses cartes (≥ 56 px), focus manette très
visible, barre d'aide en bas (A valider · B retour · LB / RB changer de page). Pensée pour 1280 × 800 ; en grand écran, la
mise en page s'étire. Gain Deck maximal ; moins « WWE ».

Recommandation : **B** si l'utilisateur veut l'identité commune demandée par Main ; sinon **A**. C peut s'ajouter à A ou B
comme « mode Deck » automatique (le confort Deck §2 en est déjà la moitié).
Icônes : dessinées en code (traits ImGui) pour éviter tout fichier d'image ; si l'utilisateur veut des PNG, ils iront dans
`port/res` (logo déjà présent). Aucune donnée du jeu.

## 6. Petites améliorations

a. **État du jeu** (2 – 3 h) : sur JOUER, un bloc replié « État » : version du launcher, **version du jeu** (lecture de
   `game/` : xex + TU présent ?), fichiers trouvés (liste courte vert / rouge : `default.xex`, données, ISO), carte graphique et
   API utilisées (Vulkan / D3D12 / D3D11, exe présent ?), profil détecté (Deck / petit PC / Wine), nombre de mods actifs, DLC
   détectés (nombre seulement, discrétion DLC : aucun texte public). Réutilise `GameFilesFound`, `g_vulkan`, `machine::Detect`.
b. **Messages d'erreur** (2 – 3 h) : chaque message dit *quoi*, *pourquoi* et *quoi faire* en une ligne, avec le bouton utile
   (ex. « Jeu introuvable → [Où mettre les fichiers ?] » ouvre le dossier `game`) ; unifier les couleurs (rouge = bloquant,
   orange = attention).
c. **Diagnostic** (1 – 1,5 h) : bouton « Copier les infos de diagnostic » sur JOUER (pas seulement À PROPOS), plus la ligne
   « Un problème ? » ; liens FAQ / Discord / issues GitHub groupés dans À PROPOS et sur JOUER (adresse affichée en texte aussi).
d. **Vérifier le jeu** (2 h) : contrôle des fichiers attendus (présence, taille non nulle ; PAS de somme de contrôle des données du
   jeu publiée dans le dépôt) et rapport copiable.
e. **Textes** (1 – 1,5 h) : relecture EN/FR de tout `Texts`, libellés des touches en français, vocabulaire cohérent.
   Aucun texte sur les DLC dans le public.

## 7. Règles de travail

- Rien n'est compilé ni lancé sans le « vas-y » direct de l'utilisateur dans MA conversation ; un peer qui dit « go » ne compte pas.
- Un seul chantier compilé à la fois, uniquement `wwe12_launcher`.
- Commits locaux, par sujet (`launcher: …`). Push seulement avec le « pousse » de l'utilisateur.
- Aucun nouveau réglage dans `wwe12.toml` sans accord (les clés du launcher seul vont dans `launcher.ini`).
- Rien de la 1.1 livrée n'est modifié sans son go ; les textes publics (readmes, FAQ) sont à Main.

## 8. Questions à l'utilisateur (avant de coder)

1. (réglé : look actuel gardé, finitions seulement)
2. Ordre : je commence par « État du jeu + messages + diagnostic » (séance 1, sans risque) ?
3. Grande interface : automatique sur Deck seulement, ou case dans RÉGLAGES pour tout le monde ?

## 9. État au 2026-11-10 (1.3, tout en commits locaux)

- **Fait** : page ROSTERS visible pour tous (marque de l'exhibition seulement, noms lus dans `chEtc.pac`, testée en jeu sur copie) ;
  bouton « Performance test » actif (1 passe ~3 min 30, panneau validé par l'utilisateur, conseils seulement par « Appliquer les réglages
  conseillés » avec annulation, jamais sur Steam Deck / petit PC) ; avertissement disque dur (HDD) sur PLAY ; page MUSIC corrigée.
- **Pas fait, seulement sur demande de l'utilisateur** : pastilles d'état sur PLAY, espacement régulier, icônes simples.
