# Optimisations du recomp WWE 2K14 (Jarrod) : ce qu'on peut reprendre

Étude de la conversation Rendu, le 2026-10-31, à la demande de Main. **Lecture seulement** : les
notes de version (pages 1 à 4 des releases), le README, le CHANGELOG et la liste des fichiers de
`patches/`. Rien n'a été cloné ni téléchargé, et aucun code n'a été repris.

- **Licence :** BSD-3 (fichier `LICENSE`). Son code peut être repris avec crédit, mais pas le jeu
  recompilé ni les images. On demandera quand même son accord, comme pour Kaiko : c'est
  l'utilisateur qui lui écrit.
- **Son projet est différent du nôtre.** Il utilise aussi ReXGlue, mais son rendu « NATIVE » part
  du rendu du SDK (processeur de commandes, EDRAM émulée), qu'il a allégé petit à petit. Notre rendu
  (re:Blue + plume) est natif depuis le début : il n'émule jamais l'EDRAM. Beaucoup de ses gains GPU
  corrigent donc des coûts que nous n'avons pas. Ses gains **CPU** se transposent mieux.
- **Ses patchs du SDK** sont dans `patches/rexglue-sdk/` (patchs numérotés 0001 à 0019 et une série
  complète dans `full/`). Je ne les ai pas lus.

## Ce qu'on a déjà, ou l'équivalent

| Chez Jarrod | Chez nous |
|---|---|
| v0.2.10 : gel définitif des entrées sur les PC à 2 cœurs. La tâche est marquée « pas finie » avant de réveiller le fil d'aide (3 gels sur 6 avant, 0 sur 7 après, sur 4 fils). | **La même cause que nos gels.** Corrigé en 0.9 pour deux objets (`wwe12_actor_jobs.cpp`), puis aujourd'hui pour le troisième (Rey Mysterio, commit 78c2573). Son résultat confirme notre analyse. |
| v0.1.10 « low power » : le jeu ne tourne plus à vide en attendant le GPU ou l'image suivante (≈ 2 cœurs de moins). v0.2.0 : le fil d'horloge dort (7,6 % → < 1 % d'un cœur). | En partie : patch SDK `timer-sleep` (TimerQueue 20 % → 0 %), `wwe12_gpu_wait_yield`. **Reste :** l'attente active du fil de rendu sur le processeur de commandes (`sub_82AA7040`, ~17 % de son temps actif). |
| v0.2.6 : plusieurs lectures du GPU vers le jeu (read-backs) partagent une seule attente. | Read-backs différés en fin d'image, une par rafale. |
| v0.2.0 : les copies d'images vont directement là où le jeu les lit. Pas d'EDRAM dans les matchs. | C'est déjà notre fonctionnement de base (resolve = copie GPU directe). |
| v0.2.5 / v0.2.8 / v0.2.9 : liste de pipelines livrée, préparée en avance, clés exactes, plusieurs fils. | `pipelines_seed.w12p` + `bd_pso_precache`, préparés en arrière-plan au démarrage. Voir le point 3 ci-dessous. |
| v0.2.0-s54 : 360p à 1080p dessinés à leur vraie taille. | Fait (360p / 480p / 540p, 2026-10-28). |
| v0.2.13 : 16:10 et 4:3 remplis par plus d'arène en haut et en bas, menus et HUD en 16:9. | Fait aujourd'hui, avec la même idée (`wwe12_widescreen`). Son correctif « rouge en ultralarge » (une passe hors écran élargie à tort) est un piège à garder en tête pour nous. |
| v0.2.14 : yeux noirs Intel, multiplications « exactes comme la Xbox ». | Nos yeux noirs Intel ont été corrigés autrement le 2026-10-12 (garder Inf/NaN dans les shaders). |

## Ce qu'on pourrait reprendre (du plus intéressant au moins intéressant)

| # | Technique (version chez lui) | Gain chez lui | Pour nous | Petits PC ? | Risque | Séances |
|---|---|---|---|---|---|---|
| 1 | **Limite de read-back relevée** (v0.2.10) : les tenues Superstar Threads sortaient abîmées en match parce que les copies refaites plus de 4 images d'affilée n'étaient plus relues. Il passe à 32 images. | Un bug corrigé | Notre limite est de **6 images** (`kMaxStreak`, `svr_resources.cpp`). Le bug ouvert « tenue Superstar Threads avec des zones blanches (Daniel Bryan) » pourrait avoir la même cause. | Oui (tous) | Faible : un peu plus d'attentes GPU au chargement des tenues | 0,5 |
| 2 | **Plus d'attente active en attendant le GPU ou l'image suivante** (v0.1.10, « low power ») | ≈ 2 cœurs de moins en match | Notre reste connu : `sub_82AA7040` (~17 % du fil de rendu). Sur 2 cœurs, c'est le temps qui manque aux fils du jeu (notre mesure du 2026-10-28 : ce sont eux qui limitent). | **Oui, surtout** | Moyen : une attente mal réveillée peut faire geler (nos gels venaient de là) | 1 |
| 3 | **Tous les pipelines connus préparés avant de jouer**, avec plusieurs fils à basse priorité (v0.2.8 / v0.2.9 / v0.2.13) | Pause en début d'entrée 80 → 37 ms ; plus de pause de plusieurs secondes (Lesnar) | Comparer avec notre préparation en arrière-plan : nombre de fils, priorité, et si tout est prêt avant le premier match. Les lignes `[load] … pipelines N` du journal comptent les compilations en jeu. | Oui (iGPU : compilation lente) | Faible | 1 |
| 4 | **Image en retard affichée tout de suite** (v0.2.13, `--wwe2k14_late_frame_threshold`), au lieu d'attendre le rafraîchissement suivant | Début d'entrée 82-86 % → 95 % de la vitesse ; images lentes 34-36 → 9 | Avec la synchro verticale, une image de 17 ms en attend 33. Il faudrait présenter tout de suite quand on est en retard, sans déchirure d'image. | **Oui** (ce sont eux qui ont des images en retard) | Moyen : saccades ou déchirures si c'est mal réglé | 1-2 |
| 5 | **Copies et redessins inutiles sautés dans les entrées** (v0.2.12, « skip_restores », « skip_repeat_resolves ») | GPU −7 à −10 % (1440p), −3 à −5 % (720p) | À vérifier avec un F9 pendant une entrée : est-ce que WWE '12 recopie aussi la scène puis la redessine à l'identique ? | Un peu (GPU) | Moyen : image fausse si une copie était utile | 1-2 |
| 6 | **Code recompilé qui garde plus de registres** (v0.2.4) | Fil principal du jeu −15 % ; exe −13 Mo | C'est le générateur de code de ReXGlue, le même que le nôtre : le gain se transposerait. Il faut lire ses patchs SDK (téléchargement : go de l'utilisateur), recompiler tout le jeu et tout retester. | **Oui, beaucoup** (le CPU est notre limite sur petits PC) | **Élevé** : touche tout le code du jeu | 3-4 |
| 7 | **Foule dessinée par groupes** (v0.2.4) | −20 % du travail CPU qui nourrit le GPU, dans l'entrée la plus chargée | Notre rendu dessine chaque spectateur à part (cvar `wwe12_crowd` pour l'enlever). Grouper demande de reconnaître les dessins de la foule (shaders connus) et de les fusionner. | Oui (CPU) | Moyen | 2-3 |
| 8 | **Shader de post-effets réécrit à la main** (v0.2.11) | Ce shader −38 % (match), −50 % (entrée) | Propre à 2K14. Pour nous : mesurer d'abord le coût de nos passes de post-effets (RenderDoc), puis voir si une réécriture vaut le coup. | Oui pour les iGPU | Moyen (image légèrement différente) | 2-3 |
| 9 | **Constantes de dessin en mémoire GPU** quand la carte n'a pas le « Resizable BAR » (v0.2.1) | GPU −7 % sans ReBAR | À vérifier : où plume place nos blocs de constantes. | Cartes anciennes | Faible | 1 |
| 10 | **Déformation des catcheurs en code natif** (v0.2.6) | Ce fil −40 % | C'est de la décompilation d'une fonction du jeu, notre méthode « jeu 100 % PC » (1.1). | Oui | Élevé | 2+ |

Pas pour nous : le passage de l'EDRAM émulée au rendu natif (déjà fait chez nous), FSR / DLSS,
les choix de rendu LIGHT / ACCURATE.

## Sa méthode (pour la 1.1)

Ses notes ne parlent pas de décompilation en C++ lisible. Elles disent seulement :
- **Code natif à la main :** une fonction du jeu (la déformation des catcheurs) a été réécrite en
  natif.
- **Vérifications :** chaque changement est contrôlé image par image (« pixel-identique »,
  « read-backs octet pour octet », replay comparé image par image).
- **Mesures :** il fait un test de performance intégré (deux entrées et un début de match, deux
  fois) et demande aux joueurs leurs résultats sur portables.
- **Organisation :** il travaille par séances numérotées et garde un patch par séance.

Une idée à reprendre : un **test de performance intégré**, reproductible, qui donne aux joueurs des
chiffres comparables.

## Proposition

1. **Tout de suite (petit, sans risque) :** le point 1, la limite de read-back de 6 à 32 images. Il
   faut tester avec une tenue Superstar Threads en match.
2. **Pour les petits PC, en 1.0 :** les points 2, 3 et 4. Ils touchent l'attente du CPU, les
   pipelines et l'affichage des images en retard. On mesure avec `wwe12_dev_affinity=F` et le
   journal `[load]`.
3. **Plus tard (1.1) :** les points 6, 7, 8 et 10. Ils demandent de lire ses patchs (accord de
   l'utilisateur pour le téléchargement, et de Jarrod pour la reprise).

Rien n'est codé : on attend le choix de l'utilisateur.
