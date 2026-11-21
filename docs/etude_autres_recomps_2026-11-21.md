# Étude : ce que font les autres recompilations / portages (2026-11-21)

Étude en lecture seule : j'ai lu uniquement des notes de version, des README et des listes d'issues publiques (via `gh api` / `gh release view`). Rien téléchargé, rien cloné, rien écrit sur GitHub, aucun code copié.

Important : je n'ai PAS lu les Discord, les Reddit ni les vidéos. Tout ce qui est « ce que demandent les joueurs » vient seulement des issues GitHub publiques. Les points non lus sont marqués « non vérifié ».

---

## 1. Résumé en 10 lignes

1. Il y a maintenant beaucoup de projets bâtis sur ReXGlue (plus de 25 dépôts trouvés : Forza, GTA V, UFC 3, DBZ, FIFA, Burnout, Blue Dragon, SvR 2007 à 2011...). Les bonnes idées circulent vite entre eux.
2. Jarrodfeng (WWE 2K14) est sorti en v0.4.1 le 2026-11-20 : nouveau rendu maison, mais surtout beaucoup de réparations. Ses joueurs se plaignent surtout de la lenteur / chauffe sur téléphones (Android).
3. Kaiko (SvR 2011) est en v2.2.6 (2026-11-20) : lanceur qui se met à jour seul, Steam Deck via Proton, réglages de qualité fins (ombres, textures, résolution), mods, Mod Maker, Android, Switch.
4. nefariousjosiah (SvR 2007 à 2010) : approche très simple, « un dossier, un .iso à côté, on lance », menu de réglages dans le jeu (touche F1), mise à jour dans le jeu, « tout débloquer ».
5. re:Blue (Blue Dragon) et Unleashed Recompiled sont les références « confort » : menu d'options dans le jeu, remappage complet, icônes Xbox / PlayStation / Steam Deck, gestionnaire de mods, profils de sauvegarde, souris dans les menus.
6. Pour les PC faibles, les idées les plus copiables sont des réglages simples : qualité des textures, ombres coupées, résolution plafonnée, préparation des shaders, choix automatique de la bonne carte graphique sur portable.
7. Pour la manette : icônes PlayStation automatiques, option « toutes les manettes = joueur 1 » (évite le joueur 2 fantôme avec Steam Input / DS4Windows), clavier rebindable.
8. Piège fréquent chez les autres : réglages qui ne font rien (Jarrod en a retiré plusieurs en 0.4.1), mises à jour qui cassent une install, antivirus / SmartScreen, black screen sur Intel.
9. Le SDK ReXGlue a un bug ouvert (#615) : lecture des manettes SDL3 avec une image de retard (16,7 ms). À vérifier chez nous : si on utilise ce chemin, c'est un gain gratuit.
10. Nous sommes déjà très avancés sur plusieurs points (mods, entrées, FPS, Discord, arènes). Les vrais manques à vérifier : menu d'options dans le jeu, sauvegardes / profils visibles, auto-update, rapport de problème en un clic.

---

## 2. Tableau des idées

Légende : intérêt pour nous = fort / moyen / faible. Effort = petit / moyen / gros. « À vérifier » = je ne sais pas si on l'a déjà (l'étude ne regarde pas notre code).

| Idée | Qui l'a fait | Version | Intérêt pour nous | Effort | Risque |
|---|---|---|---|---|---|
| Qualité des textures Haute / Moyenne / Basse (utilise les versions réduites du jeu) | Kaiko | 2.x (lecture README) | fort (petits PC) | moyen | faible (visuel) |
| Qualité des ombres OFF / BASSE / MOY / HAUTE | Kaiko | 2.2.4 | fort | petit à moyen | faible |
| Plafond de résolution du rendu (480p à 4K) séparé de la fenêtre | Kaiko, nefariousjosiah (480p), Jarrod (360p-540p) | v2.x / 0.4.1 | fort | petit | faible |
| Entrées en 30 fps au choix (déjà notre mode « 30 ») | Kaiko (« Entrance frame rate ») | 2.x | moyen (on l'a) | petit | faible |
| Préparation des shaders au premier lancement (« Preparing shaders ») | Jarrod ; Unleashed (compilation intégrée au chargement) | 0.2.9 / 0.4.1 | fort (supprime les à-coups) | moyen | moyen (Jarrod a eu un bug : la liste n'était pas enregistrée) |
| Choisir la carte graphique rapide sur portable à 2 cartes | Kaiko | 0.2.1 | fort | petit | faible |
| Message clair si la carte graphique plante (« GPU perdu ») au lieu d'un crash | Kaiko 2.2.5, Jarrod 0.4.1 | 2.2.5 / 0.4.1 | moyen | petit | faible |
| Rapport de problème en un clic (zip avec journal + résumé, rien envoyé tout seul) | Jarrod (« Report a problem »), Kaiko (dossier crashes) | 0.4.1 | fort | petit à moyen | faible |
| Dossier de crash automatique + nom du fichier abîmé dans le journal | Kaiko | 2.2.6 | moyen | petit | faible |
| Lanceur qui se met à jour seul (sauvegardes gardées) | Kaiko, UFC3ReX, nefariousjosiah (dans le jeu) | 2.2.x | fort si absent (à vérifier) | moyen | moyen (voir pièges) |
| Menu d'options dans le jeu (touche F1 ou Back+Start) | nefariousjosiah, re:Blue, Unleashed | 2.2 | moyen à fort | moyen | faible |
| Écran d'installation guidé (vérifie les fichiers, propose des réglages pour le PC) | Jarrod (assistant de 1er lancement), re:Blue, Unleashed | 0.4.1 | moyen | moyen | faible |
| Onglet Sauvegardes : copie de secours, restauration, export / import | Kaiko (onglet Saves), nefariousjosiah (copie auto avant mise à jour) | 2.x | fort | petit à moyen | faible |
| Profils de sauvegarde (plusieurs jeux de sauvegardes) | re:Blue (`--profile`) ; demandé « en jeu » par un joueur | 1.3 | moyen | moyen | faible |
| Icônes de boutons PlayStation / Switch / Steam Deck automatiques | nefariousjosiah, re:Blue, Unleashed, FIFA WC14 (0.3.0) ; demandé chez Jarrod (#303) | 2.x / 0.3.0 | fort | moyen (images) | faible |
| Option « Toutes les manettes = joueur 1 » (évite le joueur 2 fantôme) | nefariousjosiah | 2.x | moyen à fort | petit | faible |
| Remappage clavier complet, avec rappel des touches | nefariousjosiah (F4), re:Blue, FIFA WC14 | 2.x / 0.3.0 | moyen | moyen | faible |
| Clavier comme joueur 2 | Jarrod | 0.2.14 | moyen | moyen | moyen |
| Souris dans les menus (survol, clic, molette) | re:Blue, Jarrod | 0.4.1 | faible à moyen | gros (chaque menu) | moyen |
| Écrans larges (21:9, 32:9, 16:10) : la 3D remplit l'écran, le HUD reste en 16:9 | Kaiko, Jarrod, nefariousjosiah, re:Blue | 2.x / 0.4.1 | moyen | gros | moyen |
| Option « ne pas mettre en pause quand la fenêtre perd le focus » | demandé chez re:Blue | issue ouverte | faible | petit | faible |
| Steam Deck / Linux via Proton : mode d'emploi pas à pas (jeu non-Steam + Proton Experimental), 16:10, clavier Steam+X | Kaiko, nefariousjosiah | 2.2.x / 2.2 | fort pour « Petits PC » | petit (surtout un guide) | faible (à tester) |
| Réglage « Tout débloquer » sans toucher la sauvegarde | nefariousjosiah | 2.2 | faible (on a nos modes) | petit | moyen (progression perdue au retour) |
| « Finishers infinis » (option, hors ligne) | Kaiko | 2.2.6 | faible à moyen | petit | faible |
| Ping / delai / netcode « rollback » en ligne | DBZ Burst Limit | 0.8.1 | faible (nous : hors ligne) | gros | gros |
| Gestionnaire de mods dans le jeu ou le lanceur (installer un zip, désinstaller exactement ce qui a été ajouté) | re:Blue, UFC3ReX (« Mod Pack »), Kaiko (.svrmod) | 1.3 / 1.1.10 | fort (on a le Mod Maker) | moyen | moyen |
| Packs de textures (déposer / activer) + « vider les textures » pour en créer | Kaiko | 2.x | moyen | gros | moyen |
| Superstars / contenu ajouté : importateur dans le lanceur | Jarrod (Superstar Importer) | 0.3.1 | moyen (on a déjà notre version) | — | — |
| Caméra libre / replays / outils de tournage (CineCam) | Jarrod | 0.2.x | moyen | gros | moyen |
| Lancer un match directement depuis le lanceur (Quick Match, 30 Years) | Jarrod | 0.4.0 | moyen | moyen | faible |
| Support DualSense : LED qui change, pavé tactile | Unleashed | 1.0.x | faible | moyen | faible |
| Succès (achievements) avec fenêtre pop-up | Kaiko (39 succès), re:Blue, Unleashed | 2.x | moyen (fun) | gros | moyen |
| Onglet « Merci aux soutiens » dans le lanceur | Kaiko | 2.2.5 | faible | petit | faible |
| Réduire la chauffe du CPU (arrêter une boucle d'attente à 100 %) | Jarrod (SDL sur Android) | 0.4.1 | moyen (portables) | petit | faible |
| Vérification automatique : « aucune option du lanceur sans effet » | Jarrod | 0.4.1 | moyen | petit | faible |
| Mode d'emploi + SHA-256 du zip + page de téléchargement officielle unique, avertissement « ne téléchargez qu'ici » | Kaiko, FIFA WC14 (hashes + VirusTotal) | 2.2.x / 0.3.0 | moyen | petit | faible |

---

## 3. Top 5 pour la 1.5 « Petits PC » (rapide et sûr)

1. **Réglages de qualité simples pour petits PC** : textures Moyenne / Basse, ombres OFF / BASSE, plafond de résolution du rendu. Kaiko les a déjà, avec une aide « Entrances ou matches lents sur PC faible : faites X, Y, Z » dans son dépannage. Très compréhensible pour un joueur.
2. **Choix de la bonne carte graphique sur un portable** (puissante plutôt qu'intégrée) et **message clair si le pilote graphique plante**. Les deux projets l'ont eu comme source de plaintes (très faibles fps ou écran noir sur portable). Petit effort, gros effet sur les petits PC.
3. **Rapport de problème en un clic** (un zip avec le journal et un résumé, rien envoyé automatiquement), comme Jarrod. Cela nous donnerait des données réelles sur les petits PC sans demander aux joueurs de chercher des fichiers.
4. **Guide Steam Deck / petit PC portable pas à pas** (Proton, 16:10, clavier à l'écran). Kaiko et nefariousjosiah annoncent 55 à 60 fps sur Deck en 720p. Nous n'avons peut-être besoin que d'un guide et d'un test (non vérifié si notre version tourne sous Proton).
5. **Option « Toutes les manettes = joueur 1 »** + **icônes PlayStation automatiques**. Les deux touchent beaucoup de joueurs sur petit PC / Deck (Steam Input crée souvent un « joueur 2 fantôme »).

Bonus très rapide à vérifier chez nous : le bug du SDK ReXGlue #615 (lecture de manette SDL3 en retard d'une image). Si notre jeu lit les manettes par SDL3, une mise à jour ou un petit correctif gagne 16 ms de réactivité.

---

## 4. Top 5 pour plus tard (1.6 / 2.0)

1. **Menu d'options dans le jeu** (F1 / Back+Start) avec : résolution, FPS, langue, manettes, mise à jour. C'est ce que font nefariousjosiah, re:Blue et Unleashed : le joueur n'a plus besoin du lanceur. Effort moyen à gros (il faut dessiner dans les menus du jeu).
2. **Mise à jour automatique + copie de secours des sauvegardes avant chaque mise à jour**, avec retour en arrière si ça se passe mal (nefariousjosiah le fait). Très protecteur pour nos joueurs débutants.
3. **Préparation des shaders au premier lancement** (écran « Preparing shaders ») pour supprimer les à-coups du premier match. Jarrod l'a corrigée en 0.4.1 après avoir découvert qu'elle ne préparait rien : à faire avec une vérification que le fichier de la liste est vraiment chargé.
4. **Onglet Sauvegardes + profils** (copier, restaurer, exporter, plusieurs profils). Les joueurs de re:Blue le demandent même « dans le jeu ».
5. **Écrans larges propres** (21:9, 32:9, 16:10) : la 3D remplit l'écran, le HUD reste en 16:9. Tous les projets sérieux le proposent ; gros effort et risque d'objets qui apparaissent au bord de l'écran (Jarrod l'avoue encore non testé sur un vrai écran ultra-large).

Idées intéressantes mais plus « bonus » : packs de textures et leur « dump » (Kaiko), succès avec pop-up, support DualSense, caméra libre.

---

## 5. Ce que ces projets font mal / pièges que nos joueurs ont déjà évités

Voici ce que j'ai lu dans leurs notes et issues. Je ne compare pas à notre code : ce sont des mises en garde.

- **Réglages qui ne font rien.** Jarrod a dû retirer UPSCALING et le choix de la forme d'écran en 0.4.1 car ils ne changeaient plus rien ; il a ajouté un contrôle automatique qui échoue s'une option n'a rien derrière. Idée à copier : une vérification interne.
- **Résolution qui ne change pas l'image.** Chez Jarrod, 1440p / 4K étaient étirés depuis du 720p jusqu'à la 0.4.1.
- **Mise à jour qui casse.** Kaiko : les vieux lanceurs sous Proton ne pouvaient pas décompresser une mise à jour (« Could not unpack the update »). nefariousjosiah : si on vient d'une très vieille version, il faut retélécharger à la main. re:Blue a une issue « Error during update ». Unleashed : si on lance le jeu via la recherche Windows, les fichiers peuvent se retrouver dans System32. Leçon : garder la mise à jour simple et toujours sauvegarder avant.
- **Sauvegardes copiées à la main qui « semblent abîmées »** (Kaiko) ou qui oublient des fichiers cachés (logos). Leçon : un onglet de sauvegarde officiel évite les erreurs de copie.
- **Mauvaise carte graphique choisie sur portable** (Kaiko) et **Intel intégré qui plante ou dessine mal** (Jarrod : gel après une longue session ; FIFA WC14 : Intel UHD / Iris Xe jouent mal).
- **Chauffe et ralentissement des téléphones** : la plainte numéro un chez Jarrod (beaucoup d'issues Snapdragon 695 / 870, Dimensity 6080). Sans rapport direct avec nous (PC), mais ça montre que mesurer sur un appareil faible après une longue session est essentiel.
- **Nouveau rendu : beaucoup de petits bugs graphiques après la 0.4.0** de Jarrod (cheveux, ceintures, ombres, nom de champion absent, goutte de sang absente, crowd...). Plus de 260 issues ouvertes. Leçon : changer de moteur de rendu a un coût élevé en réparations ; sa propre étude (docs/etude_jarrod_v040.md) allait déjà dans ce sens.
- **Options exigeantes cachées dans un fichier de réglages** : Jarrod demande d'ajouter à la main `--wwe2k14_present_mailbox=true` pour tester une correction du déchirement. Pas pratique pour un débutant.
- **Antivirus / SmartScreen** : tous les projets mettent un avertissement (« le programme n'est pas signé, cliquez sur Plus d'infos puis Exécuter quand même »). FIFA WC14 publie aussi les empreintes et un lien VirusTotal ; Kaiko ajoute « ne téléchargez que depuis cette page ». À copier.
- **Contradiction dans la doc de Kaiko** : le README dit que le mode en ligne n'est pas disponible, mais sa v2.2.4 parle d'un jeu en ligne plus rapide. Non vérifié lequel est à jour.
- **Annonces de « fix potentiel »** : Kaiko écrit « potential » quand il n'a pas pu prouver un correctif. Honnête et utile aux joueurs ; à imiter dans nos notes.
- **Joueur 2 fantôme** avec Steam Input / DS4Windows / DSX (nefariousjosiah) : à anticiper.
- **Pas de test réel de combinaisons** : Jarrod écrit lui-même que l'ultra-large, l'AMD, le 4:3 et plusieurs téléphones ne sont pas testés. Rassurant pour nous : on peut dire la même chose honnêtement.

---

## 6. Sources lues (toutes publiques)

Notes de version, README et listes d'issues, lus avec `gh api` / `gh release view` / `gh issue list` / `gh search repos` :

- https://github.com/Jarrodfeng/wwe2k14-recomp (releases v0.4.1, v0.4.0, README, CHANGELOG, issues ouvertes jusqu'au n° 343)
- https://github.com/KaikoClanworth1/wwe-svr2011-pc (releases v2.2.6, v2.2.5, v2.2.4, README complet ; les issues du dépôt sont désactivées, donc aucune demande de joueurs lue)
- https://github.com/nefariousjosiah/WWE-SVR2010-RECOMP (release v2.2, README, titres d'issues) ; les dépôts frères SvR 2007 / 2008 / 2009 : seulement description et dates (non lus en détail)
- https://github.com/zolaware/reblue (README, nightly 1.4.0, stable 1.3.1 vue en liste, titres d'issues)
- https://github.com/rexglue/rexglue-sdk (v0.11.0, nightlies récentes, titres d'issues dont #615)
- https://github.com/hedge-dev/UnleashedRecomp (README, v1.0.3 en liste ; les titres d'issues par nombre de commentaires n'ont pas pu être lus : non vérifié)
- https://github.com/360recomper/UFC3ReX (README, v1.1.10, titres d'issues)
- https://github.com/iExplosiveRage/DBZ-Burst-Limit-Recompiled (v0.8.1, v0.8.0-beta)
- https://github.com/novapowers0/DBZ-Budokai-3-HD-Collection (v1.5.6.2 : launcher, mods, builds Linux / Mac)
- https://github.com/HossEz/fifa-world-cup-2014-recompiled (v0.3.0)
- Recherche de dépôts : `gh search repos` pour « rexglue », « xbox 360 recompilation », « wwe recomp », « WWE 2K15 recompilation », « smackdown recomp » (résultats lus en liste seulement : Xerenge / Burnout, Forza, GTA V, Asura's Wrath, Condemned 2, OpenJam, SvR07-Recomp, etc., non approfondis)

Non vérifié / non lu :
- WWE 2K15, 2K16, 2K17 : aucun dépôt public de recompilation trouvé (la recherche n'a rien donné).
- Dépôts SvR 2007 / 2008 / 2009 : versions et dates vues seulement.
- Discord, Reddit, YouTube, Steam : non lus.
- Les commentaires à l'intérieur des issues (je n'ai lu que les titres).
- Sonic Unleashed : liste de fonctions lue, mais pas les versions récentes (dernière v1.0.3 d'avril 2025, plus de nouvelle version depuis).
- Le contenu exact de nos propres fonctions (menus, sauvegardes, auto-update) : je n'ai pas relu notre dépôt, donc « à vérifier » reste ouvert.
