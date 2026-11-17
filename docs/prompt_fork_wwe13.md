# Prompt de départ : forker le portage PC de WWE '13 et en faire un vrai bon jeu

À coller comme premier message d'une conversation Claude Code sur l'autre PC (modèle conseillé : Opus pour le code, Fable pour les études). Écrit par la conversation Main du projet WWE '12 PC (`iMrBurn/wwe12-pc`) à partir de ce qui a marché (et de ce qui a fait perdre du temps) chez nous.

---

Tu travailles avec iMrBurn (francophone, débutant : réponds en français simple, sans jargon inutile, il/iel « they » si un pronom est nécessaire ; textes publics GitHub / Discord / releases en anglais).

## Le but

Un portage PC de **WWE '13 (Xbox 360)** par recompilation statique existe (l'auteur l'a abandonné : beaucoup de bugs, peu d'optimisation). Le but : **forker ce dépôt** et en faire un jeu PC solide, rapide et agréable, comme le portage WWE '12 PC (voir plus bas). Priorité : **un jeu fluide et optimisé d'abord**, glitchs cosmétiques d'une image ensuite.

## Règles absolues (apprises à la dure)

1. **Jamais de données du jeu dans le dépôt ni dans un zip** : pas de .xex / .xexp / .iso / .pac / .bik / .bin / sons / textures / dumps / mises à jour du jeu / DLC, pas de lien de téléchargement du jeu ou de DLC. Risque de DMCA sur tout le dépôt. Le code recompilé (exe) est accepté. Ajoute un **contrôle anti-fuite** à l'empaquetage (le zip ne doit contenir aucune de ces extensions).
2. **Licence et crédit** : lis la licence du dépôt d'origine ; respecte-la ; mets l'auteur d'origine dans README + `THIRD_PARTY.md`. Si la licence interdit le fork, arrête-toi et demande à iMrBurn.
3. **Rien n'est poussé / publié sans le « go » explicite** d'iMrBurn. Commits locaux libres. Les releases sont créées par iMrBurn.
4. **Avant de lancer le jeu, prévenir** ; un seul jeu / une seule compile à la fois. Les tests en jeu sont à iMrBurn : tu proposes un test précis (quoi faire, quoi regarder), tu ne dis pas « c'est bon » avant d'avoir lu les journaux.
5. Demander avant tout téléchargement. Ne jamais toucher aux réglages de sécurité de Windows.
6. Tout nouveau comportement risqué est **coupé par défaut** tant qu'il n'est pas vu en jeu.

## Méthode de départ (ne code rien avant)

1. **Cloner et construire d'abord le dépôt tel quel**, sans rien changer, et vérifier que le jeu démarre avec la copie d'iMrBurn du jeu. Noter la version de chaque outil (SDK de recompilation, compilateur, CMake, Vulkan).
2. **Piège CMake** : ne jamais lancer un `cmake -S . -B` nu ; si une configuration échoue elle laisse des `CMAKE_CXX_FLAGS` vides → build non optimisé + validation Vulkan = jeu très lent. Toujours passer par un script de build (`build.ps1` qui charge l'environnement), vérifier `-O3`/Release dans `CMakeCache.txt`.
3. **Mesurer avant d'optimiser** : écris un **test de performance reproductible** (un match 1 contre 1 avec entrées, parcours des menus automatisé, même dossier de données neuf, caches vides) qui écrit un fichier de résultats : images/s moyennes, pire image, % d'images lentes, temps de travail CPU, temps d'attente GPU, nombre de pipelines compilés pendant le dessin, temps de chargement. Chez nous : un 1v1 avec 2 entrées, 30 s de match, cible 60 fps en match / 30 en entrées. Garde l'historique des mesures. **Aucune optimisation sans chiffre avant / après.**
4. Lis les journaux du jeu (`logs\`) à chaque lancement : erreurs, avertissements, durées des chargements.
5. Fais un **état des lieux** (`docs/etat_des_lieux.md`) : liste des bugs connus du dépôt d'origine (issues GitHub, Discord s'il y en a), classés : plantage / gel / très lent / graphique / son / manette / menus. Propose un plan par ordre de gain ÷ risque et demande l'accord d'iMrBurn avant de commencer.

## Ordre de travail conseillé

1. **Stabilité** : plantages et gels (garder un `crash_trace` / `hang_trace` + la carte des symboles `.map` de chaque version publiée pour traduire les rapports des joueurs).
2. **Chargements et démarrage** : écrans de chargement, démarrage (shaders préparés au démarrage, `pipelines.bin` livré en « graine », pas de compilation pendant le jeu).
3. **Fréquence d'images** : modes fixes (jamais de bascule « intelligente » qui change toute seule), V-sync correcte (attention : une V-sync « adaptative » qui juge une image en retard par rapport à l'écran et pas à la cadence du jeu a fait déchirer l'image chez nous).
4. **Rendu** : trois moteurs si possible (Vulkan, DirectX 12, DirectX 11 pour les vieilles cartes), résolution interne réglable, 16:9 / 16:10 / ultra-large, netteté de l'agrandissement (coupée sur GPU intégré), Steam Deck.
5. **Manettes, clavier, menus d'options** (options graphiques dans les vrais menus du jeu si possible).
6. **Code du jeu plus natif** (gros chantier, risqué, en dernier) : registres du PowerPC gardés en variables locales plutôt qu'en mémoire (options du générateur de code), accès mémoire sans `volatile` sauf matériel : le gain annoncé par d'autres portages est ~15 % de CPU sur le fil principal, mais tout le jeu change : test complet obligatoire.

## Leçons techniques de WWE '12 PC (à vérifier chez toi, pas à croire)

- **Ne jamais soumettre / rouvrir la command list au milieu d'une image** : saccades, yeux noirs. Les attentes GPU vont en fin d'image (read-backs différés).
- Les surfaces nouvellement créées dessinées une fois peuvent sortir noires : ne pas les présenter (une image noire avant le début d'un match).
- Les écrans de chargement : le jeu attend inutilement ; un « turbo » (images cachées, plusieurs images par image montrée) divise le temps par 2,5.
- Un démarrage très rapide casse sur les processeurs à peu de fils : le garder pour ≥ 8 fils logiques.
- Les menus d'options du jeu sont souvent des scripts Lua compilés (5.1) : on peut les remplacer par nos pages via des crochets.
- Cadence : un jeu console tourne à 30 ou 60 selon la scène ; chaque scène demande un script ; ne pas forcer 60 partout (cinématiques et mouvements cassent).
- Hooks de fonctions du jeu : `extern "C"` + appel de l'original ; le code généré est dans un dossier non versionné (énorme) : ne le mets pas dans le dépôt.
- Journaux (`[perf]`, `[load]`, `[present]`) à chaque évolution : c'est ce qui permet de répondre aux joueurs.

## Version, publication, issues

- Un fichier `VERSION` unique ; tag GitHub `v<VERSION>` ; jamais « prerelease ». Ajoute les empreintes SHA-256 (zip + exe) aux notes ; les exe non signés déclenchent de faux positifs d'antivirus : l'expliquer dans le README.
- Réponds aux issues en anglais, simplement, en demandant toujours : version, diagnostic copié depuis le lanceur, dernier `.log`, processeur / carte graphique, branché ou sur batterie. Ferme les issues silencieuses après ~1 semaine avec un message poli.
- Garde un fichier de feuille de route + un fichier « savoir acquis » (adresses, structures, pièges) : l'abonnement et les conversations ne sont pas éternels, une autre session doit pouvoir reprendre.

## Premier message attendu de toi

Réponds avec : (1) le résultat de la construction du dépôt tel quel (ou ce qui bloque), (2) la licence et ce qu'elle autorise, (3) ton plan d'état des lieux. Ne modifie rien avant l'accord d'iMrBurn.
