# Tester « comme un petit PC »

Constat du 2026-10-28 (conversation Rendu) : **`start /affinity F wwe12.exe` ne bride pas le jeu**.
Le SDK redonne tous les processeurs au jeu quand son runtime démarre
(`rex::thread::EnableAffinityConfiguration`, appelé par `runtime.cpp`). Les anciens tests « 4 fils »
(`tools/dev/lancer_test_4fils.bat`, turbo à 4 fils…) tournaient donc en réalité sur tout le processeur.

## Le réglage de test

`wwe12_dev_affinity` (désactivé par défaut, vide) : masque hexadécimal des processeurs que le jeu
peut utiliser. Il est appliqué 3 s après le démarrage, une fois que le SDK a fini
(`port/src/native/wwe12_dev_affinity.cpp`). Le journal écrit `[dev] processor affinity set to 0x…`.

    wwe12.exe --wwe12_dev_affinity=30000

Sur le PC de l'utilisateur (i7-14700HX) :
- les fils 0 à 15 sont les 8 gros cœurs (P), avec hyperthreading ;
- les fils 16 à 27 sont les 12 petits cœurs (E).

| Masque | Ce que ça imite |
|---|---|
| `30000` | 2 petits cœurs : un petit portable à 2 cœurs (le cas le plus dur mesuré) |
| `F0000` | 4 petits cœurs |
| `F` | 2 gros cœurs avec hyperthreading (2C/4T rapide) |

Autre moyen, sans recompiler : après le démarrage, en PowerShell,
`(Get-Process wwe12).ProcessorAffinity = 0x30000`.

## Mesure du 2026-10-28 (2 petits cœurs, 720p, matchs de démo)

- Menus : 54-57 images/s, avec seulement 3-5 ms de dessin par image. La limite, ce sont les fils de
  logique du jeu.
- Match : Vulkan 46-56 images/s, D3D12 53-55 images/s. Pas de vainqueur net.
- D'où : pas d'option 30 images/s en 1.0, et le scan des textes des menus a été limité au démarrage
  et aux écrans de chargement (`wwe12_quit.cpp`, `wwe12_text_scan_periodic` pour revenir à l'ancien
  comportement).
