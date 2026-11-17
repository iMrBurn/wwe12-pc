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

---

## Annexe : rendu natif sur les 3 moteurs (Vulkan, DirectX 12, DirectX 11)

Ajoutée par Main après la demande d'iMrBurn : « aide-le à faire rapidement et facilement le rendu natif des 3 moteurs ». **Honnêtement : « facile » ne veut pas dire « sans travail »**. Le chemin le plus court est de **reprendre l'architecture de rendu de WWE '12 PC** (même moteur Yuke's, même bibliothèque D3D du jeu, même GPU Xbox 360), au lieu de la réinventer. Mais les adresses des fonctions du jeu et les shaders sont propres à WWE '13 : il faudra les retrouver.

### Étape 0 : regarder ce que fait déjà le portage WWE '13 d'origine
Avant de copier quoi que ce soit : quel moteur de rendu utilise-t-il (Vulkan seulement ? D3D12 ? un émulateur de GPU Xenos ?), et où sont les bugs / lenteurs ? Ne remplace que ce qui est mauvais. Mesure d'abord (test de performance, voir plus haut).

### Comment WWE '12 PC fait (références dans notre dépôt `iMrBurn/wwe12-pc`, GPL-3.0)
- Code du jeu recompilé en C++ (SDK ReXGlue, `recomp/rexglue-sdk`, modifié par les patchs de `port/patches/`). Le jeu écrit encore des commandes de GPU Xbox 360 : **ce n'est pas du « vrai natif »** (voir le README), mais le dessin, lui, passe par un renderer natif.
- **Dessin** : `port/src/reblue/gpu/` (re:Blue + bibliothèque graphique `plume` de nefariousjosiah, adaptée par nous). Crochets sur ≈ 50 fonctions D3D du jeu dans `port/src/reblue/gpu/hooks/` : le jeu appelle sa propre bibliothèque D3D, nos crochets dessinent avec plume. Fichiers clés : `device.cpp` (création du GPU, choix de carte), `present.cpp` (affichage, V-sync, netteté), `draw.cpp`, `resolve.cpp`, `surface_registry.cpp`, `physical_buffers.cpp`, `constant_buffers.cpp`, `pipeline/` et `shaders/`. Colle entre le jeu et ce dessin : `port/src/native/svr_resources.cpp` (cibles de rendu, read-backs vers la mémoire du jeu, échelle de résolution), `svr_geometry.cpp`, `gpu_svr_stubs.cpp`.
- **Les trois exe** : CMake construit `wwe12.exe` (Vulkan), `wwe12_d3d12.exe` (cible `WWE12_D3D12_EXE`) et `wwe12_d3d11.exe` (cible `WWE12_D3D11_EXE`). Réglage `gpu_backend` : `any` (Vulkan, puis D3D12, puis D3D11 si la carte échoue), `vulkan`, `d3d12`, `d3d11`. Patchs : `port/patches/plume-d3d12.patch` (read-backs, choix de carte), `plume-d3d11.patch`. Aucune DLL Microsoft dans le zip : shaders **prélinkés** (`wwe12_prelink.exe`) ; le D3D11 embarque ses shaders ; ne jamais livrer dxcompiler / dxil / d3dcompiler / fxc.
- Détails et pièges : `docs/dx11.md`, `docs/couches.md`, `docs/savoir_acquis.md`, `docs/perf_natif_steam_deck.md`.

### Marche à suivre conseillée
1. **Lis ces fichiers chez nous** (clone `iMrBurn/wwe12-pc` en lecture seule, dossier à part) et fais un tableau « ce qui dépend du jeu / ce qui est générique ».
2. **Générique (réutilisable presque tel quel)** : tout `plume` et les trois backends, `device`, `present` (V-sync, mode fenêtre, résolution interne, netteté), le cache de pipelines (`pipelines.bin` livré en « graine » + préparation au démarrage), le test de performance, les réglages (`gpu_backend`, échelle de rendu, anisotropie, rapport d'aspect).
3. **Propre à WWE '13 (à retrouver)** : (a) **les adresses des ≈ 50 fonctions D3D du jeu** à crocheter : WWE '12 et '13 utilisent la même bibliothèque Yuke's, donc cherche-les par **motifs d'octets / chaînes / structure** à partir des adresses de WWE '12 (liste dans `wwe12_d3d_names.h` et `hooks/`), pas à la main ; (b) **les shaders** : recompilés depuis ceux du jeu par XenosRecomp ; (c) les surfaces et les resolves (EDRAM) : mêmes pièges que chez nous (voir `svr_resources.cpp`) ; (d) la cadence (60 / 30) et les scripts de scène.
4. **Ordre** : d'abord **un seul moteur** qui rend une image correcte (Vulkan), le test de performance en main ; puis D3D12 (même code plume, un patch) ; puis D3D11 (le plus délicat : moins de fonctions, mêmes résultats ; chez nous les reflets du soleil ne sont pas dessinés en D3D11).
5. **Mesure à chaque étape** (images/s, pire image, pipelines compilés pendant le dessin) et garde une capture RenderDoc de référence par scène. Ne conclus jamais « ça marche » sans journal + capture.

### Licences et permissions : à demander à iMrBurn AVANT de copier
- Notre code est **GPL-3.0** : le dépôt WWE '13 forké doit avoir une licence compatible (sinon, soit on le relicencie en GPL si l'auteur d'origine l'autorise, soit on ne copie que ce qui est permis). Lis `THIRD_PARTY.md` et `LICENSE.reblue` pour les conditions de re:Blue / plume.
- **Le backend D3D11 vient de Kaiko** (portage SvR 2011), repris **avec sa permission pour le projet WWE '12 PC et son crédit**. Pour un autre projet, **demande à iMrBurn de redemander l'accord de Kaiko** ; sinon, réécris le D3D11 toi-même. Crédite aussi nefariousjosiah (re:Blue, plume) et l'auteur d'origine du portage WWE '13.
- Toujours : aucune donnée du jeu dans le dépôt ni le zip.

### À répondre à iMrBurn après l'étape 0
Un court résumé : moteur de rendu actuel du portage WWE '13, ce qui manque pour les 3 moteurs, quelles parties de WWE '12 PC tu comptes réutiliser, le risque de licence, une estimation en heures, et ce qu'il doit tester en jeu.
