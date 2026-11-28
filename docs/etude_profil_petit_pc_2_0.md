# Étude : profil « petit PC » du launcher (2.0)

Date : 2026-11-28 (Launcher). Étude courte, **aucun code écrit**. Demande d'iMrBurn relayée par la Main. À valider avant de coder.

## 1. Ce qui existe déjà (rien à inventer)

| Quoi | Où | Clé de `wwe12.toml` |
|---|---|---|
| Résolution interne (auto = 720p sur GPU intégré / Deck, 540, 720…) | launcher, OPTIONS > GRAPHICS | `svr_render_scale` |
| Foule, ombres, flou de profondeur, filtrage des textures | idem | `wwe12_crowd`, `wwe12_shadows`, `wwe12_dof`, `bd_anisotropy` |
| 16:10 / ultra-large | idem | `wwe12_widescreen` |
| Fréquence d'images : originale / **30 en match (expérimental)** / 60 aux entrées | launcher (ADVANCED) et jeu | `wwe12_frame_mode` |
| Priorité du processeur | launcher (ADVANCED) | `wwe12_cpu_priority` |
| API graphique (Vulkan / DX12 / DX11) | launcher | `gpu_backend` |
| Compteur de FPS | launcher + F2 | `wwe12_show_fps` |
| Code natif PC de la peau et du « vmx » (≈ 1,5 ms de processeur en moins par image) | **déjà actif par défaut** | `wwe12_native_skin`, `wwe12_native_vmx` |
| Intros sautées (démarrage plus court) | **déjà actif par défaut** | `wwe12_skip_intros` |
| Bouton **PC MODESTE** (480p, sans foule ni ombres ni flou) + **Annuler le préréglage** (copie de sécurité) | launcher, RÉGLAGES | — |
| **Proposition automatique à l'ouverture** (Steam Deck / petit PC : détection par points, trois boutons Appliquer / Pas maintenant / Non merci) | `machine_profile.h`, `launcher.cpp` | — |
| Test de performance + conseil (« cpu » ou « gpu ») appliqué seulement par un bouton, avec annulation | `benchmark_run.h` | — |

Donc le « profil petit PC » existe en partie depuis la 1.2 : ce qui manque, c'est de le **rendre plus précis** (savoir ce qui freine) et de le **montrer** dans RÉGLAGES.

## 2. Ce qui a été MESURÉ (banc petit PC : 2 E-cores + GPU intégré, 720p)

- En match : **≈ 58-59 i/s, attente du GPU 0,0 ms** : la carte graphique ne freine jamais, **c'est le processeur** (fil du jeu ≈ 20 ms).
  Baisser la résolution ou couper des effets graphiques **ne gagne rien** sur ce banc ; ça n'aide qu'un PC dont la carte est plus faible.
- Ombres : ≈ **1 ms** de GPU par image à 1440p (donc presque rien à 720p) ; « ombres basses » à décider par Rendu (étude des réglages graphiques).
- **Aucun gain** (mesuré le 2026-11-22) : priorités des fils, saut de l'empreinte des textures ; copie des textures : 0 en match, ≈ 80 ms/s aux entrées
  (déjà à 31 i/s pour 30 visées). **À ne pas mettre dans le profil.**
- Code natif (peau + vmx) : ≈ 1,5 ms de processeur gagnés par image, déjà actif pour tout le monde.

## 3. Proposition de profil : réglage → valeur, avec la raison

On sépare deux cas, selon ce que dit le **test de performance** (ou, sans test, la détection).

**Cas « le GPU freine »** (carte intégrée faible, ancienne, < 3,5 Go) :

| Réglage | Valeur | Raison |
|---|---|---|
| `svr_render_scale` | 720 (1 point faible), 540 (3-4), 480 (≥ 5) | c'est le gain le plus sûr pour un GPU qui attend (déjà le cas) |
| `wwe12_dof` | false | flou : coût GPU, aucune gêne |
| `wwe12_shadows` | false | ≈ 1 ms, utile seulement à haute résolution |
| `wwe12_crowd` | false si score ≥ 3 | foule : beaucoup de dessins |
| `bd_anisotropy` | inchangé (16) | coût négligeable (non mesuré à part : à confirmer par Rendu) |

**Cas « le processeur freine »** (2 à 4 cœurs, verdict `cpu`) :

| Réglage | Valeur | Raison |
|---|---|---|
| `wwe12_crowd` | false | moins de travail pour le fil du jeu |
| résolution / ombres / flou | **inchangés** | mesuré : aucun gain quand le CPU freine |
| `wwe12_frame_mode` | **proposé, jamais imposé** : « 30 en match (expérimental) » par un bouton séparé | plus stable qu'un taux qui saute entre 40 et 60 ; décision de la Main : jamais automatique, jamais sur Deck / petit PC dans le profil |
| `wwe12_cpu_priority` | **inchangé** | mesuré : aucun gain |

Ne touche jamais : intros, natifs (déjà actifs), FPS affiché, API (sauf Wine/Proton → Vulkan, comme aujourd'hui), vsync.

## 4. Détection automatique (sans l'imposer)

Déjà faite à l'ouverture (`machine::Choose`) : Deck par nom de carte / variable `SteamDeck`, puis des points (carte intégrée +2, dédiée < 3,5 Go +2,
≤ 4 cœurs physiques +1, ≤ 8 Go de RAM +1). Un PC normal ne reçoit rien. Pour la 2.0, trois ajouts possibles :
1. **≤ 2 cœurs = +2** (au lieu de +1) : le cas « 2 E-cores » du banc ;
2. une ligne dans RÉGLAGES « Votre PC semble limité par le processeur / la carte : appliquer le profil ? » avec le bouton, en plus de la fenêtre du démarrage (qu'on peut avoir fermée avec « Non merci ») ;
3. **si le test de performance a été fait**, le verdict (`cpu` / `gpu`) choisit entre les deux tableaux du §3 au lieu de deviner.

## 5. Retour facile aux réglages normaux

Déjà là : « Annuler le préréglage » rend les 6 valeurs sauvegardées (`lowend_preset_backup.txt`). À ajouter (petit) : la copie de sécurité garde aussi
`wwe12_frame_mode` si le profil l'a changé, et un second bouton « Remettre les réglages par défaut du jeu » **seulement si iMrBurn le veut**.

## 6. Nouvelles clés de `wwe12.toml`

**Aucune.** Tout passe par des clés qui existent. (Si Rendu fait `wwe12_shadows_low`, le profil l'utilisera, mais cette clé est à valider à part.)

## 7. Ce qu'il reste à décider (iMrBurn)

1. Deux profils selon la cause (GPU / processeur) comme au §3, ou un seul comme aujourd'hui ?
2. La ligne « Votre PC semble limité » dans RÉGLAGES : oui / non ?
3. Le « 30 en match » reste un bouton séparé, jamais dans le profil : d'accord ?
4. Valeurs mesurées à confirmer par Rendu : gain de la foule et du flou sur le banc, coût de l'anisotropie.

Estimation si tout est validé : 3 à 4 h (tableaux, ligne dans RÉGLAGES, ≤ 2 cœurs, verdict du test, essai hors jeu avec `SteamDeck=1` et des cartes simulées).

## 8. Réponse de Rendu (2026-11-28) : aucun des trois n'est mesuré sur le banc

- **Foule coupée** : le seul qui peut aider un banc limité par le processeur. Retire ≈ 435 des ≈ 670 dessins d'une scène de match (capture
  2026-10-17 : 339 cartes de foule + 96 personnes en 3D), donc du travail pour le fil de rendu (4,5 à 5 ms par image en match). **Gain
  probable, pas chiffré** (à confirmer ; Rendu peut ajouter un paramètre « foule » à `bench_gfx.bat`, 2 passes oui / non en 720p).
- **Flou de profondeur coupé** : aucun dessin retiré (constantes à zéro) : gain GPU seulement, ≈ 0 sur ce banc.
- **Anisotropie 16 contre 0** : coût GPU seulement, ≈ 0 sur ce banc ; ne compte que sur un GPU vraiment faible.
- Conséquence pour le §3 : dans le cas « le processeur freine », **la foule est le seul réglage utile** (cohérent avec la proposition).
