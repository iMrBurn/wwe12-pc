# Feuille de route (mise à jour le 2026-11-06, validée par l'utilisateur)

Les détails techniques sont dans `CLAUDE.md`, `docs/univers.md`, `docs/mod_maker.md`, `docs/title_update_corrections.md`, `docs/plan_1_2_codegen_tu.md`, `docs/perf_natif_steam_deck.md`, `docs/launcher_amelioration.md`, `docs/android_apres_1_1.md` et `docs/suggestions_joueurs.md`. L'ancienne feuille de route (textes de l'utilisateur, notes, décisions jour par jour) est gardée en bas, sous « Détails et historique ».

## Règles du jeu des numéros
- **1.0.x** : seulement pour un bug grave (plantage au lancement, sauvegarde cassée, mise à jour qui échoue). Pas de 1.0.1 prévue (décision du 2026-11-04).
- **1.x** : un thème par version. Ce qui n'est pas testé en jeu passe à la version suivante.
- **2.0** : seulement si les sauvegardes, le format des mods ou la façon d'installer changent.
- Rien n'est promis aux joueurs tant que ce n'est pas testé en jeu. Une seule liste officielle (ce fichier) ; les issues GitHub n'y sont que des liens.
- Rien de dérivé du jeu ne se publie (pas de données de jeu dans un zip ni sur GitHub).
- Estimations en **heures de travail hors tests de l'utilisateur** (pas en « séances »).

## 1.1 : « Mod Maker et entrées » (en cours, une seule version, sans date)

**Fait et testé en jeu (poussé sur GitHub)**
- Univers : plantage à 5 matchs par show ; catcheurs ajoutés sans alliés ni rivaux, marque gardée ; image du PPV dans les news de qualification ; jukebox arrêté à chaque chargement.
- Rendu : D3D12 sur Intel UHD 620 (#7), tenues Threads blanches et peau GT 430 (corrections de Kaiko, crédit), saccades de shaders (shaders préparés au démarrage, `declarations_seed.w12d` validé pour le zip), mesure du CPU par image.
- Mod Maker : gel arènes + catcheurs ajoutés, mémoire pour 50+ catcheurs (tas 12 / 15), pages d'arènes, genre des catcheuses, titantron d'avant-match, entrées 2K14 (Sandow), Goldberg 2K17, yeux de SvR 2009/2010, DLC comme base, catcheurs installés modifiables, foule et victoires SvR 2007, prises : couverture améliorée (2K14 48 → 73 %), MOVES & ENTRANCES.
- Launcher : profil « Steam Deck / petit PC » (testé sur ce PC avec un Deck simulé, pas sur un vrai Deck) ; FAQ Steam Deck ; Android : vieux noyaux + journal GPU, nouveau look (Yexclsv), graine `declarations`.

**À tester avant la sortie**
- Arène RAW de SvR 2007 refaite dans le Mod Maker (transition compressée) : avant / après match sans plantage.
- Étape 1 des prises WWE '13 (case « Prises ajoutées (expérimental) ») : sinon marquée expérimentale dans les notes.
- Royal Rumble de Jay avec ses mods (à lui faire retester) ; PPV à une seule marque (Univers) ; réglage « Largeur » du portrait (685c950, local).
- Android : correctif des vieux noyaux (joueur de #41), look sur vrai téléphone (Yexclsv).

**Dans la 1.1 sur décision de l'utilisateur (2026-11-06), NON testé en jeu, annoncé « expérimental »** : reprise automatique des notes / taille / genre depuis SvR 2007-2011 (8fc2fc1, remis dans `main`). **Hors 1.1** : vraies animations de prises SvR 2011 / 2K, personnes cachées des entrées 2K, 2K17, détail des yeux SvR 2010 en courant, tenues de The Miz (abandonné), base neutre de NOUVEAU CATCHEUR (refusée).

**Notes publiques** : les notes de la 1.0 annonçaient pour la 1.1 « squash, finishers de départ, I Quit, Special Referee, deux mallettes MITB, icônes de touches » : les notes de la 1.1 disent que c'est repoussé (1.3).

## 1.2 : « Moveset update » : PUBLIÉE le 2026-11-10 (PC + Android, tag v1.2.0, zip SHA ab13bf6d…, APK 2c72767b…)

**Livré** : prises SvR 2007-2011 et WWE '13 / 2K14-2K17 dans le Mod Maker (expérimental) + page MOVES & ENTRANCES ; choix de la marque (RAW / SmackDown / aucune) d'un catcheur ajouté, marque gardée (sauvegarde, match, reset de l'Univers) ; genre Homme / Femme qui compte enfin (0xEE autorisé : une diva sur base d'homme est une femme) ; entrée choisie dans le jeu gardée ; moveset copié gardé ; titantron d'équipe respecté ; news de l'Univers avec le vrai nom ; noms de fichiers à accents / japonais lus en UTF-8 (jeu, launcher, Mod Maker : plantage au démarrage et page MUSIC) ; gamma (curseur du jeu actif, image un peu plus sombre par défaut, option « Console picture ») ; saccade de début de match (read-backs différés, ~10 fois moins d'attente) ; moins de CPU (blocs de constantes réutilisés, hachage 4096) ; mode 30 fps EXPÉRIMENTAL en matchs (jamais par défaut ; liste « Frame rate » du launcher PC et Android) ; journal des étapes de l'Univers ; message clair au-delà de 35 arènes ajoutées. Android : ligne Frame rate, APK signé avec la même clé.
**Non vérifié en jeu avant la sortie** : moveset copié après relance, diva « Femme » dans l'Univers (matchs / classements Divas), gamma sur un vrai téléphone, hachage 4096 et read-backs différés sur Android.
**Ouvert après la 1.2** (voir 1.3) : plantage des équipes avec des modèles SvR 2011 (il faut le crash_trace du joueur), gel des 2K17 de l'issue #60 (journal du joueur ; à refaire avec le nouveau Mod Maker), blocage de fin de match de l'Univers sur Steam Deck (le journal d'étapes dira où), image noire avant la cloche (capture RenderDoc), arènes 2K14 / SvR 2009 (défauts graphiques ; « la précédente reste » : journal du joueur).

(Texte d'origine de la section, gardé pour l'historique : décision de l'utilisateur, 2026-11-08 : pas de 1.1.1, tout va directement dans la 1.2.)

Même chemin que la 1.1 pour la sortie : « go build », gel des sessions, APK à refaire si le code partagé change.

**Moveset update**
- Port des prises de SvR 2007-2011 et de 2K14-2K17 dans le Mod Maker (état au 2026-11-08 : prises ajoutées WWE '13 / 2K jouables en jeu « pas parfait » depuis 625aaea ; il manque l'écriture des paramètres `m3` pour WWE '13 / 2K, sans eux elles gèlent ; notes dans `docs/prises_svr11.md`). Mods continue 2K17 puis SvR.
- Puis une grosse session de corrections de bugs et quelques améliorations.

**Retours de joueurs sur la 1.1.0 (ex-« 1.1.1 »)**
- **Entrées de catcheurs ajoutés non sauvegardées** (retour d'un joueur, 1.1.0 ; fait en 1.2) : cause probable trouvée par Mods, correctif d'une ligne (`wwe12_superstar_mods.cpp`, test « fiche étrangère » à partir de 0x1EC), commit local 6e7d1d1. Touche le jeu. À tester : catcheur ajouté, choisir une entrée, sauvegarder, relancer. À confirmer avec le joueur qu'il s'agit bien de catcheurs ajoutés.
- **Gamma / luminosité** (retour d'un joueur) : étude de Rendu faite. Le curseur du jeu ne fait rien (la rampe gamma du jeu est captée par le GPU « null » du SDK mais personne ne la lit) ; la Xbox 360 corrige aussi pour une télé. Plan : (1) faire marcher le curseur, 3-5 h ; (2) réglage « Image : CONSOLE (TV) / PC » dans GRAPHICS et le launcher, 1-2 h, défaut PC proposé. Étape 0 : une ligne de journal pour confirmer avant de coder (build + lancement, avec le « vas-y » de l'utilisateur).
- **Page MOVES & ENTRANCES au niveau de celle de Kaiko** (comparaison de Mods) : (~2 jours, fait par Mods : onglets Prises / Provocations, « Jouer » en 3D, entrée d'un autre catcheur, commits 0a28d9c 35362d2 4448fb8, vérifié hors jeu seulement) l'entrée d'un AUTRE catcheur de WWE '12 choisie par nom (3-5 h), changer le moveset case par case avec les prises du jeu (6-10 h), raccourcis signatures / finishers (2 h) ; si le temps le permet : provocations (3-4 h). Plus tard : aperçu complet d'une entrée (25 h et plus), packs de prises autonomes (10-15 h), porter UNE prise dans une case (8-12 h). Mise en page de Kaiko reprise avec crédit (MIT).
- **Univers : après un match, le jeu ne passe pas à la victoire** (retour d'un joueur) : rien de bloquant dans le code Univers (relu). Hypothèses : scène de victoire d'un catcheur ajouté / mod de prises, ou blocage du moteur. À confirmer avec le joueur (version, image figée ou non, `hang_trace.txt`, mods, match exact, même résultat en Exhibition). Pas de correctif sans ces réponses.
- **Saccade de début de match + clignotement d'écran (Intel RaptorLake, Vulkan, journal wwe12_048)** : les shaders ne sont plus la cause (0 pipeline en match). Cause probable (Rendu) : read-backs d'une cible par image pendant les 32 premières images (~10 cibles 64x64 + 512x512), qui font attendre le GPU ; clignotement : nouvelle cible 1280x720 remplie en noir. Plan : (a) diagnostic seul dans la ligne [load] (~1 h), (b) passer l'erreur « unknown guest format 0xC000000F » en debug (voulue), (c) vrai correctif (regrouper les read-backs) après mesure et test des tenues Threads ; clignotement : capture RenderDoc d'abord.

- Restent pour plus tard les aperçus 3D complets d'une entrée (25 h et plus), les packs de prises autonomes (10-15 h) et le port d'UNE prise dans une case choisie (8-12 h), si non faits.
- **Tout ce qui était prévu pour l'ancienne 1.2 est décalé en 1.3, l'ancienne 1.3 en 1.4, l'ancienne « 1.4 et après » en 1.5 et après.** Les sections « Détails et historique » plus bas gardent l'ancienne numérotation (1.2 = THQ / vitesse, 1.3 = modes de match).

## 1.3 : « Les corrections de THQ, la vitesse et le test de performance » (liste de l'utilisateur, 2026-11-07 ; ex-1.2, décalée le 2026-11-08)

**Décisions de l'utilisateur du 2026-11-10 (soir)** : vraie table de gamma : NON (le réglage actuel lui convient) ; image noire avant la cloche : il s'en occupe lui-même (outil de capture automatique `wwe12_autocap.cpp` / `lancer_autocap_*.bat`, version de diagnostic) ; foule réglable : non retenue ; mailbox : mis de côté ; test minimiser / restaurer : fait pendant un autre test. Fait dans la 1.3 : chargements 2,5 fois plus rapides, démarrage jusqu'à l'écran-titre 15,5 s → 5,4 s, netteté du 720p (`svr_upscale_sharpness`), benchmark « 1 contre 1 avec entrées » (une passe, ~3,5 min ; bouton du launcher grisé jusqu'à validation), `memcpy` natifs écrits et mesurés (gain trop petit : désactivés).

**Fait le 2026-11-10 (soir), commits locaux Rendu ff601b0, 5f3c2ae, 40905da, non poussés** : 60 fps limité aux entrées (script de scène < 1000) et aux victoires (6000-6999) ; cinématiques Road to WrestleMania (1010/1020/1810/1820) et Create a Story (4000) restent à 30 comme sur Xbox (vérifié en jeu sur iGPU Intel : règle #63) ; réglage `wwe12_60fps_all_scenes` pour l'ancien comportement ; 30 fps dès le début du match sans entrées (#61) compilé, PAS vérifié en jeu (Android devra recompiler) ; 60 adaptatif abandonné. Caches de démarrage de Mods : entrées (17 ms au lieu de 910, vérifié), arènes (compilé, à vérifier) ; prises 2K/WWE'13 refaites sur un modèle WWE '12 (d71ae95, validé en jeu, plus de gel).

**Décision de l'utilisateur (2026-11-10, nuit) : prises ajoutées d'autres jeux COUPÉES par défaut** (réglage `wwe12_added_moves`, false ; Mods 189cead, local, non compilé). Preuve sur la copie de test : avec des mods à prises ajoutées, un catcheur envoyé dans les cordes (irish whip) traverse le ring, et les prises d'AJ Styles 2K17 finissent l'adversaire 1 à 2 m trop loin ; colle avec le Gut Kick cassé chez les joueurs (Maria SvR 2009). Cause exacte non trouvée ; essai « comme LA Max » (zmvm_add.h, faeb35f) fait mais non prouvé. L'utilisateur attend l'outil de MrSmith pour reprendre ce sujet. En attendant : refaire le mod en décochant « Prises ajoutées ». À écrire dans les notes de la 1.3 (la 1.2 « Moveset update » a ce défaut connu).

**1.3.0 PC EMPAQUETÉE le 2026-11-12** (VERSION 1.3.0, zip 110 623 182 o, SHA-256 6e71c88d…7b79, `dist/PUBLICATION_v1.3.0/PC`), en attente de la publication par l'utilisateur ; l'APK 1.3 vient après (décision : « PC, APK ensuite »).

**Ajouts 1.3 de l'utilisateur (2026-11-11)** : titantrons AJOUTÉS (pas seulement remplacés) ; vidéos pour ruban et walltron ; entrées spéciales WM / titre portées ; arènes portées retravaillées ; aperçu des entrées dans le Mod Maker (comme SvR 2011) ; bugs : musique du menu sous les thèmes du jukebox et après START, attributs non sauvegardés, quelques modèles et entrées à corriger.

**Reporté de la 1.2 (2026-11-10)**
- Mode 30 fps plus fluide pour les petits processeurs (issues #53, #57, #41) : la voie Kaiko (une image sur deux, logique à 60 Hz) a été mesurée : ~15 % de CPU en moins seulement, et des prises qui paraissent buguées ; le mode natif gagne ~60 % mais des prises sont moins fluides. Autres pistes de CPU : code D3D du jeu dans nos crochets (~0,7 ms, risque de gel élevé, plan `docs/couches.md`), constantes (~0,3 ms, demande de modifier XenosRecomp).
- Gamma : vraie table (LUT) pour les réglages extrêmes (une simple puissance colle mal) ; image noire d'une image avant la cloche (capture RenderDoc automatique).
- Mod Maker : importer un FBX comme tenue ; tenue d'entrée pour une tenue d'un catcheur AJOUTÉ (Cody American Dream) ; limite de 35 arènes ajoutées (textures de bannière libres du paquet) ; défauts graphiques des arènes 2K14 / SvR 2009 ; « option intergender » limitée à l'Univers (sub_82A39EF8) ; alerte CHECK MODS « catcheuse sur base d'homme ? » (fausses alertes possibles).
- Plantage des équipes SvR 2011, gel 2K17 de #60, fin de match de l'Univers sur Steam Deck : selon les traces des joueurs.

**THQ (voie B : nos propres crochets sur le code du disque, un par un, chacun avec son réglage)**
- Comparaison des 2 mises à jour officielles : faite (`docs/title_update_corrections.md`, session Gameplay) ; reste le tri et le test de chaque correctif en jeu.
- **Objectif de l'utilisateur (bloc-notes Discord du 2026-11-07, relu le 2026-11-10) : tester les bugs du jeu d'origine et appliquer nous-mêmes les DEUX Title Updates** (nos propres crochets, voie B). Bugs du jeu d'origine à tester : finishers qui gèlent (TU2, 6-7 crochets), cage / barrières de Hell in a Cell, tables sous le ring, sons après Backstage Brawl, movesets CAW qui se réinitialisent, évasion de tombé en Elimination Chamber, aperçu des prises DLC dans l'éditeur de CAW (TU1). Cage Hell in a Cell et évasion de tombé : rien trouvé dans le code, à vérifier en jeu.
- **Ordre de travail THQ proposé par Gameplay le 2026-11-10 (notes de patch face au code, accord de l'utilisateur)** : texte des notes dans `work/title_update_patch_notes.txt` (hors dépôt ; texte web non officiel, la partie TU2 « soumissions / IA / contre majeur » est probablement reconstituée : le code ne la montre pas). Du plus sûr au moins sûr : (1) les CAW comptent dans l'effectif des émissions Univers (3 crochets + notre recomptage, ~0,5 j, vérifiable dans EDIT BRAND) ; (2) contrôles d'index < 23 dans 5 fonctions Univers (~0,5 j) ; (3) gels de finisher, prise par prise (Skull Crushing Finale d'abord ; reproduire en jeu avant ; c'est dans la TU2, pas la TU1 ; 2-3 j) ; (4) 3 réglages de `sub_82276590` 40/60/80 → 70/80/90 (chercher leur lecteur, 0,5-1 j d'enquête) ; (5) banques CA09 / CA10 dans les listes de prises + nos CA11+ (essai en jeu d'abord ; avec Mods ; 3-4 j) ; (6) paramètre armes / objets pour tables / son (3-5 j, gain non confirmé) ; (7) Univers : résultat de match (`sub_826CEF28`), booking PPV, draft (pas compris). Pas trouvés dans le code : cage HIAC et barrières, évasion de tombé en Elimination Chamber, soumissions, IA, contre majeur. Estimations en jours de travail hors tests de l'utilisateur.

**Vitesse**
- Chargements plus rapides (étude : `docs/chargements_1_2.md`) : turbo de chargement (`wwe12_load_turbo`, existe mais coupé par défaut : match ~4,0 → 2,8 s, fin des entrées ~4,5 → 2 s), bridage des animations 2D réécrit par nous (2-3 h), contre-épreuve sur 4 fils, décision de l'utilisateur sur le turbo par défaut ; avertissement disque dur dans le launcher.
- 30 fps partout : le mode 30 du jeu imposé aux matchs (clé `wwe12_frame_mode` = original / 30 / 60_entrances), PC + Android (~7-8 h).
- Code natif plus rapide : `memcpy` / `memset` (fait, désactivé par défaut), (registres locaux du générateur de code, patch « volatile » : RÉGÉNÉRATION DU CODE sortie de la 1.3 le 2026-11-11, dans « Plus tard » sans numéro de version, décision de l'utilisateur ; étude dans `docs/regeneration_1_3.md`).
- Idées de Jarrod v0.2.19 (BSD-3, méthodes seulement, crédit THIRD_PARTY.md ; étude de Rendu dans `docs/perf_natif_steam_deck.md`) : mode mailbox (déchirement, 2-3 h), netteté de l'agrandissement 720p (3-4 h), test minimiser / restaurer (5 min), foule réglable (3-5 h). Plus tard : résolution dynamique de la 3D, foule en gros lots.

**Benchmark (test de performance)**
- Match imposé : Hell in a Cell à 6 sans menu (voie 2 : `sub_82449428`, 9 à 15 h) ; repli = méthode de Kaiko (6 à 9 h).
- Résumé + verdict (ok / cpu / gpu / failed), condition « faible » (2 cœurs), bouton dans le launcher.

**Autres**
- ~~Yeux de SvR 2010 en courant~~ : corrigé (confirmé par l'utilisateur le 2026-11-10) ; à rouvrir seulement si un joueur le signale.
- Page ROSTERS du launcher (marques hors de l'Univers, `docs/launcher_rosters.md`, 3 séances, écrit dans la sauvegarde).
- Encodeur Vorbis (Wwise) à nous, pour les thèmes portés de plus de 20 Mo (en 1.1 : coupés à 100 s).

## 1.4 : « Modes de match et Univers » (liste de l'utilisateur, 2026-11-07 ; ex-1.3, décalée le 2026-11-08)

**Modes de match**
- I Quit (la version légère d'abord), Special Guest Referee.
- Matchs squash (vie de départ réduite).
- Stock de finishers de départ (1 / 2 / 3 / infini, comme 2K14).
- Changer de joueur en plein match.
- Améliorer le système de tombé (sortie de tombé automatique ou jauge de kickout plus grande : suggestion d'un joueur).

**Univers**
- Deux mallettes Money in the Bank (RAW / SmackDown), embuscade en coulisses.
- Débloquer Edge et Christian etc. pour le mode Univers.
- Choisir le type d'intervention (bouton Y), par exemple attaque en coulisses ou autre.
- Backport des cinématiques WWE '13 / 2K14, Create a Story, tournoi pour titres vacants.
- Réglage pour couper les blessures et les fins de carrière.
- Classement des titres par équipe modifiable.
- Rivalités menées jusqu'au PPV, et rivalité inter-marques sans match (forcer un match au PPV ?).
- Titres de SmackDown dans EDIT SUPERSTAR (R1), si c'est le jeu d'origine.
- Photo propre aux arènes ajoutées.
- AJOUTER des logos et des matchup screens (les listes du jeu sont pleines).
- Nouveau show comme NXT.

**Décision du 2026-11-13 : pas de 1.3.1.** Les correctifs V-sync adaptative coupée par défaut (1787306) et 30 fps sur Android (#61, 35fb76c) partent dans la prochaine version, avec le travail Univers (news, mallettes, blessures, BACK ; NXT derrière `wwe12_universe_nxt`, coupé). Sortie visée : au plus tard le 2026-11-19 ; l'APK sera refaite pour cette version.

**Suivi 1.4 (bloc-notes de l'utilisateur du 2026-11-12, un point à la fois, dans CET ordre ; cocher quand codé + testé par l'utilisateur)**

Décision du 2026-11-12 : 1.4 à sortir en une semaine environ ; l'utilisateur commence par le mode Univers, un point après l'autre, en suivant son bloc-notes. Estimations : Univers et Gameplay (messages du 2026-11-12).

*Matchs (Gameplay)*
- [ ] I Quit (léger : le match SUBMISSION affiche « I QUIT », 3-4 h)
- [ ] Special Guest Referee (risque moyen, 8-16 h : probablement reporté)
- [ ] Matchs squash (léger : option « prochain match : squash », sonde STARTING HEALTH d'abord)
- [ ] Stock de finishers de départ (OFF / 1 / 2 / 3 / INFINI, page d'options ; sonde d'abord)
- [ ] Changer de joueur en plein match (non étudié, risque élevé : probablement reporté)
- [ ] Améliorer le système de tombé (zone de sortie plus grande, non étudié)

*Univers (Univers + Mods)*
- [x] Deux mallettes Money in the Bank (RAW / SmackDown) : version complète, testée par l'utilisateur le 2026-11-12 (65d4548, e6d1471 ; réglage UNIVERSE OPTIONS, `wwe12_universe_mitb_briefcases` = 2). Embuscade en coulisses : pas faite (cinématiques de Create a Story, 20 h et plus). Mallette bleue pour SmackDown, rouge pour RAW (Mods a9958f1 + Univers a666549), choix de qui encaisse quand les deux peuvent (NON au premier = question au second, 55632bb / 7f6962a) : testés par l'utilisateur le 2026-11-12. Idée suivante : entrée avec la mallette en main (comme depuis 2K18), à faire avec Mods.
- [ ] Débloquer Edge, Christian etc. dans l'Univers (3-6 h ; il faut la liste de ceux qui manquent)
- [ ] Choisir le type d'intervention (bouton Y) : non en 7 jours (20 h et plus)
- [ ] Backport des cinématiques WWE '13 / 2K14, Create a Story, tournoi pour titres vacants : non en 7 jours
- [x] Réglage pour couper les blessures (UNIVERSE OPTIONS > BLESSURES, `wwe12_universe_injuries`, 51a36fe) : gardé tel quel par l'utilisateur le 2026-11-12 sans preuve en jeu (le jeu ne blesse que dans les matchs JOUÉS, membre très abîmé, ou par une histoire ; jamais en simulation : 143 matchs simulés, aucune tentative). WWE '12 n'a pas d'autre fin de carrière que les blessures.
- [ ] Classement des titres par équipe modifiable (6-10 h)
- [ ] Rivalités menées jusqu'au PPV, rivalité inter-marques sans match (10-20 h ; léger ~8 h)
- [ ] Titres de SmackDown dans EDIT SUPERSTAR (R1), si c'est le jeu d'origine (2-5 h)
- [ ] Photo propre aux arènes ajoutées : codée (Univers 1e7b9e7 + Mods, Wwe12ArenaPictureName), pas testée (il faut une arène de mod installée sur la version de test).
- [ ] Ajouter des logos et des matchup screens (léger : Mods remplace Add_01..15 et PPVbg_origin01..10 par les images du joueur)
- [ ] Nouveau show comme NXT : mis de côté le 2026-11-13 (décision de l'utilisateur : pas de titre ni de roster propre, peu d'intérêt). Code derrière `wwe12_universe_nxt`, coupé par défaut (= 1.3.0) ; le choix des catcheurs reste celui de RAW/SmackDown ; état dans docs/univers.md § 45.
- [x] Beaucoup plus de News : 1er lot (Jay) codé, séries et Double Gold vus dans le journal ; #1 contender (match + gagnant), adversaire mystère et fond « grande victoire » testés par l'utilisateur le 2026-11-13 ; vus sur une année simulée le 2026-11-13 : momentum, sortie du top 5 (une par semaine au plus), longs règnes, mallette gardée 3 mois, Past Glory, représentant à WrestleMania, Fighting Champion, partenaire mystère ; pas encore vus (cas rares) : Historic Reign, équipes de Survivor Series, Double Gold après correctif, nouvelles médicales (pas de blessure en simulation). Pas faits : éliminations du Rumble (non notées par le jeu), qualifications de Survivor Series (gros chantier)

## 1.5 et après : plateformes et finitions (à décider selon les retours)
- Launcher : améliorations (`docs/launcher_amelioration.md`, 20 à 28 h : état du jeu, messages, confort Steam Deck à la manette, bouton « Test de performance », finitions) ; pas de refonte du design.
- Android après la 1.1 PC : replay d'après match, performances, ajouts de Yexclsv (`docs/android_apres_1_1.md`).
- Prises avec leurs animations d'autres jeux (SvR 2011, puis 2K) .
- Version Linux native (étude faite, `docs/linux_natif_etude.md` : 8 à 12 séances).
- 16:10 / 21:9 (D3D11/D3D12, menu pause, HUD en 21:9) à confirmer ; icônes de touches de clavier ; test sur un vrai Steam Deck.
- Android : vrais téléphones, boutons PlayStation, ce que disent les joueurs.
- Shaders qui font saccader à la première fois (en 1.1 : préparés en arrière-plan ; reste à voir ce qui saccade encore, voir l'issue #53 : surtout le processeur).

## Plus tard (sans date)
- **Univers + Create a Story** (souhait de l'utilisateur, 2026-11-13) : jouer dans l'Univers les scènes de Create a Story (embuscade en coulisses, choix du type d'intervention), puis des scènes portées des autres jeux (WWE '13, 2K14-2K17, avec Mods) et, à terme, des histoires façon Story Collection (2K15+). Base trouvée : la table des demandes de cinématiques du match (`docs/univers.md` §44) ; une scène imposée est bien chargée dans un match d'Univers, mais le jeu se fige ensuite (cible, décor de Create a Story ou scripts manquants : à creuser). Plusieurs jours, avec Mods.
- **Bloc-notes de l'utilisateur, 2026-11-11 : EN 1.3 (décision de l'utilisateur le 2026-11-11, voir aussi la section 1.3)** :
  - Mod Maker : ajouter des titantrons (et pas seulement remplacer) ; vidéos pour le ruban et le walltron ; faire porter des entrées spéciales WM / titre ; retravailler les arènes portées ; aperçu des entrées dans le Mod Maker comme SvR 2011.
  - Bugs : avec les thèmes du jukebox activés, on entend la musique normale du menu en dessous, et elle repart juste après START à l'écran-titre (le thème du jukebox s'arrête) ; les attributs ne se sauvegardent pas ; corriger quelques modèles et entrées.
- **Régénération du code** (registres locaux + patch « volatile », 10 à 14 h + un gros test de ~2 h, risque de casse) : sans version, décision de l'utilisateur du 2026-11-11. Étude : `docs/regeneration_1_3.md`.
- Plus de 6 superstars sur le ring ; 100 / 200 slots CAW et logos (probablement 2.0) ; le jeu en full natif ; Univers : cinématiques WWE '13 / 2K14, Create a Story, tournoi pour titres vacants ; choix de caméra de match.

## Abandonné / pas prévu
- Moments OMG de WWE '13 ; Title Update complète comme version du code (remplacée par la voie B) ; GM Mode ; le DLC dans les textes publics ; sauvegardes Wii ; tenues de The Miz verrouillées ; base neutre de NOUVEAU CATCHEUR.

## Comment suivre l'avancement
- Chaque élément a un état : **idée → étudié → fait (commit local) → testé en jeu → publié**.
- À chaque version : on décide les éléments au début, on les teste en jeu au milieu, on publie à la fin ; ce qui n'est pas testé sort de la version.
- Une ligne de notes de version écrite au fur et à mesure (pas à la fin).

---

# Détails et historique (ancienne feuille de route)

## Archives

Les anciennes listes de l'utilisateur (roadmaps 1.0 / 1.1, 1.0.1, décisions d'octobre et de début novembre, études du mode 30 fps, Linux, Steam Deck) sont dans `docs/archives/feuille_de_route_anciennes_parties_2026-11-12.md`.
