# Optimisations Android de Jarrod (WWE 2K14) : étude pour notre version

Étude du 2026-10-31 (conversation Android), demandée par la Main. **Lecture web seulement** (page des
releases de Jarrodfeng/wwe2k14-recomp) : rien téléchargé, aucun code repris. La page ne montre **aucune
licence open source** (seulement « projet non commercial de préservation ») : donc on ne reprend pas de
code ; on ne s'inspire que des idées (une idée n'est pas du code), et on lui demande avant tout emprunt.
Rien n'est compilé ni testé ici. Priorité basse après #41 et l'alpha 8.

**Mise à jour 2026-10-31 (Discord)** : Jarrodfeng a répondu « that's fine » quand l'utilisateur a demandé
de s'inspirer de ses dernières mises à jour Android (en lisant son GitHub). C'est un accord oral pour
s'inspirer, **pas une licence écrite** : on lit son dépôt pour comprendre ses méthodes et on réécrit nous-mêmes ;
copier de longs morceaux de code = d'abord l'accord de l'utilisateur, un crédit dans `THIRD_PARTY.md`, et
idéalement une phrase écrite de lui (ou un fichier LICENSE dans son dépôt).

## Ce qu'il a fait (Android 0.1.5 → 0.2.0, 28-30 oct)

| Version | Élément |
|---|---|
| 0.2.0 | Mode « performance » (désactivé par défaut) : scène à 540p max, SGSR, moins de public |
| 0.2.0 | SGSR seul (option d'image) ; résolution dynamique (baisse quand les images sont longues) |
| 0.2.0 | « Gros cœurs » : garder le jeu sur les cœurs CPU rapides |
| 0.2.0 | Écrans 90/120 Hz : images régulièrement espacées (au lieu de 60 fps affichés de façon inégale) |
| 0.2.0 | Cache de shaders gardé entre les lancements (« les téléphones ne gardaient jamais leurs shaders ») |
| 0.2.0 | Copies GPU ponctuelles : chacune revient seule, sans attendre tout le GPU. Chargement de match + premières secondes : 70 images > 33 ms → 4 (sur son téléphone) |
| 0.2.0 | Correctifs de ralenti au début des entrées/matchs ; « fast shaders » retirés (écran noir sur Adreno) |
| 0.1.9 | Arrondis comme PC/Xbox (pas une vitesse) |
| 0.1.8 | Pilotes Snapdragon récents sur le même rendu que le sien ; Mali démarre sur son pilote ; Turnip pour Adreno 710/720/722 ; « Safe shaders » ; instructions d'ordre mémoire ARM (correction de fils, pas de vitesse) |
| 0.1.5 | Launcher qui ne se redessine à pleine fréquence que pendant l'usage (avant : « sensation de 15 fps ») |

## Ce que nous avons déjà

- **Cache VkPipelineCache sur disque** (`plume-android-cache.patch`, sauvé à la sortie par
  `plume_save_pipeline_cache`). Équivalent de son « cache de shaders ». À vérifier : qu'il est bien
  rechargé et utile sur vrai téléphone (jamais mesuré hors émulateur) ; piste : sauvegarder aussi
  pendant la partie (pas seulement à la sortie, qui n'a pas lieu si le jeu plante ou est tué).
- **ARMv8.2** (alpha 2) ; boucles à vide corrigées (`rexglue-sdk-android2-cpu.patch`).
- **Pilotes Turnip** (libadrenotools, `wwe12_driver.cpp`), message clair si le pilote manque de
  fonctions (alpha 8), limites du pilote dans le journal (`plume: limits`).
- **Températures** : `TempMonitor` (batterie) et `ThermalWatch` (avertissement thermique).
- **Préréglages** : résolution interne (`svr_render_scale` 360…), ombres, public, flou, netteté des
  textures ; alpha 8 : préréglage léger choisi au premier lancement selon RAM / fréquence des cœurs.
- **Launcher en vues Android natives** : le piège « se redessine en boucle » de son launcher ImGui ne
  nous concerne pas.
- Mali : conversion des textures compressées (alpha 6), non confirmé sur vrai téléphone.

## Ce qui est applicable

| # | Idée | Gain attendu | Risque | Séances | Remarque |
|---|---|---|---|---|---|
| 1 | **Écrans 90/120 Hz** : choisir le mode d'écran à 60 Hz (ou multiple de 60) et/ou rythme d'images régulier (`Surface.setFrameRate`, `preferredDisplayModeId`, ou Swappy/Choreographer) | Moyen à fort sur 90/120 Hz : fluidité perçue, moins de chaleur si on force 60 Hz | Faible | 1 | On n'a **rien** de tel (aucune référence trouvée dans `android/`). Quasi tous les téléphones récents sont 90/120 Hz : à regarder en premier. Le plus simple : `setFrameRate(60, FIXED_SOURCE)` (API 30) sur la surface |
| 2 | **Résolution dynamique** (baisse l'échelle interne quand une image dépasse ~18 ms, remonte après) | Fort sur téléphones moyens : évite les chutes | Moyen : changer la taille des cibles en jeu ré-alloue (notre piège : les cibles de rendu sont recréées, lectures/read-backs) | 2-3 | Notre `svr_render_scale` est lu au démarrage ; à regarder si on peut le changer entre deux scènes (menu/chargement) plutôt qu'en pleine image. Version simple : choisir l'échelle au chargement d'un match d'après les temps du match précédent |
| 3 | **Gros cœurs** (affinité CPU sur les cœurs rapides) | Moyen sur big.LITTLE (les fils du jeu atterrissent parfois sur les petits cœurs : micro-saccades) | Faible à moyen : mauvaise liste de cœurs = plus lent ; nos fils du jeu sont déjà 6 émulés | 1 | `DeviceCheck.fastestCoreMhz()` lit déjà les fréquences : on peut classer les cœurs. Mesure indispensable (impossible sur l'émulateur) ; en option désactivée au début |
| 4 | **Copies GPU sans attente globale** (attendre la copie seule, pas tout le GPU) | Potentiellement fort pour le chargement de match et les premières secondes | Moyen-fort : touche aux read-backs de `svr_resources.cpp` (zone de Rendu ; règle : jamais d'attente au milieu d'une image) | 2 + accord de Rendu | Sur PC les attentes sont déjà reportées en fin d'image. À vérifier si Android a le même chemin ; mesurer d'abord avec `[pso]`/temps d'image |
| 5 | **Mode « performance » en un bouton** (540p max + public réduit, option SGSR) | Facile à offrir, gros effet | Faible | 0,5 | On a déjà les préréglages (LOW-END / BALANCED-lite) : c'est surtout un nom/une case de plus. Voir 6 pour SGSR |
| 6 | **SGSR** (suréchantillonnage spatial de Qualcomm, shader open source BSD-3) : rendre en 540p et agrandir proprement | Fort : 540p au lieu de 720p avec une image presque aussi nette | Moyen : un passe de plus dans le rendu (plume), licence à relire, marche aussi hors Adreno | 2-3 | **Pas besoin de son code** : le shader SGSR vient de Qualcomm (github.com/SnapdragonStudios, licence à lire avant). Après la résolution dynamique |
| 7 | Correctifs de ralenti au début des entrées/matchs | Moyen | ? | ? | Pas assez de détails publics ; nos propres mesures `[pso]` (alpha 8) diront où sont nos pics |
| 8 | Ordre mémoire ARM (barrières) | Correction, pas vitesse | Moyen | 1 | Utile contre nos gels inexpliqués (#46, #47, #51 ?) : notre recompilation traduit des accès PPC à ordre fort ; vérifier ce que `rexglue` émet pour `sync/lwsync/eieio` sur ARM64. À soumettre à la Main/SDK : c'est le même SDK que le PC |
| 9 | Mali sur son pilote / Safe shaders | Déjà fait pour nous (alpha 6, pas confirmé) | — | — | « Safe shaders » = son rendu rapide qu'on n'a pas : sans objet |

Non applicable : son launcher ImGui (nous avons des vues natives), PC v0.2.11-0.2.14 (shader de
post-traitement refait à la main, copies sautées pendant les entrées, présentation d'une image en
retard dès qu'elle est prête) : **idées utiles aussi pour le PC** (à signaler à Rendu), surtout
« afficher une image en retard dès qu'elle est prête » (pacing) et sauter les copies redondantes.

## Ordre proposé

1. **Écrans 90/120 Hz** (séance 1) : le plus gros rapport gain/risque, rien à mesurer côté jeu.
2. **Sauvegarde du cache de pipelines pendant la partie** (petit) + vérifier son effet sur vrai téléphone.
3. **Mesure sur vrai téléphone** (gros cœurs, copies GPU) : on n'a pas d'ARM64 sous la main ; demander à
   un testeur un journal avec temps d'image avant de toucher aux read-backs.
4. **Résolution dynamique** simple (au chargement d'un match), puis **SGSR** si le gain se confirme.
5. Barrières mémoire ARM : à confier à la Main (SDK commun) si les gels persistent après l'alpha 8.

## À ne pas faire

- Reprendre son code (pas de licence) ; télécharger ses APK ; promettre ses chiffres (« 70 → 4 images
  lentes » vient de **son** téléphone, avec **son** jeu).
- Toucher au rendu partagé (`svr_resources.cpp`, `present.cpp`) sans prévenir Rendu.
