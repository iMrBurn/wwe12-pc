# Étude : réglages graphiques en plus (2.0, chantier 1)

Date : 2026-11-28 (Rendu). Étude courte avant de coder.

Ce qu'on demande : **qualité des textures**, **ombres en 3 niveaux** (coupées / basses / normales), **plafond de
résolution**. Le but : un jeu plus fluide sur les petits PC.

## Ce qui existe déjà

| Réglage | Où | Clé |
|---|---|---|
| Résolution interne 360p à 2880p, et « auto » (720p sur GPU intégré et Steam Deck) | OPTIONS > GRAPHICS, launcher | `svr_render_scale` |
| Ombres oui / non | OPTIONS > GRAPHICS, launcher | `wwe12_shadows` |
| Foule oui / non, flou de profondeur, filtrage anisotrope, AA | OPTIONS > GRAPHICS, launcher | `wwe12_crowd`, `wwe12_dof`, `bd_anisotropy`… |
| Préréglage « petit PC » (480p, sans foule ni ombres ni flou) | launcher | — |

Le **plafond de résolution existe donc déjà** : c'est la résolution interne, et le mode « auto » reste en 720p sur
les GPU intégrés. Rien à ajouter, sauf si iMrBurn veut un autre comportement.

## Ce que disent les mesures déjà faites (banc petit PC, 2026-11-22)

Sur le banc « petit PC » (iGPU Intel, 4 fils, 720p), en match : **59 i/s, attente du GPU 0,0 ms**. La carte
graphique n'est **jamais** ce qui freine : c'est le processeur (fil du jeu à environ 20 ms). Un réglage graphique
ne peut donc rien gagner sur ce banc-là. Il ne peut aider qu'un PC dont la carte graphique est plus faible.

## Mesure proposée avant de coder (Main lance, environ 15 min)

Pour voir ce que coûtent vraiment les ombres et les textures, on **charge** la carte graphique : même banc,
mais en **1440p** sur l'iGPU (là, le GPU devient le frein).

| Passe | Commande (dans `Documents\WWE12-PC`) |
|---|---|
| A1, A2 : ombres oui | `bench_gfx.bat <exe> A1 1440` puis `A2` |
| B1, B2 : ombres non | `bench_gfx.bat <exe> B1 1440 false` puis `B2` |

On compare l'« attente du GPU » et les i/s du match (2 passes chacun : le bruit est grand).

## Décision selon le résultat

- **Ombres basses** (carte d'ombres à moitié de sa taille, 560 au lieu de 1120) : à coder **seulement si** B
  gagne nettement sur A (au moins 2 ms de GPU par image). Sinon « oui / non » suffit.
- **Qualité des textures** (textures à moitié de leur taille) : la mémoire vidéo n'est pas pleine (360 Mo
  utilisés), et lire une texture plus petite ne gagne presque rien sur un GPU qui attend déjà. Main l'avait mis
  de côté le 2026-11-22. **Proposition : ne pas le faire**, sauf si iMrBurn le veut pour un PC précis.
- **Plafond de résolution** : déjà là.

## Nouvelles clés (à valider par iMrBurn avant de coder)

Si les ombres basses valent le coup : une seule clé en plus, `wwe12_shadows_low` (oui / non, défaut non).
`wwe12_shadows` reste tel quel. OPTIONS > GRAPHICS et le launcher montrent une seule ligne « Ombres : coupées /
basses / normales », qui règle les deux clés.
