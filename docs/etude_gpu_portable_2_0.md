# Étude : la bonne carte graphique sur portable, et un message clair si le pilote plante (2.0, chantier 2)

Date : 2026-11-28 (Rendu).

## Ce qui existe déjà (rien à refaire)

| Quoi | Où |
|---|---|
| L'exe demande la carte dédiée à Windows (NVIDIA Optimus, AMD) | `port/src/main.cpp:22-23` (`NvOptimusEnablement`, `AmdPowerXpressRequestHighPerformance`) |
| Vulkan : sans choix, la carte **dédiée** d'abord, puis l'intégrée | `reblue/gpu/device.cpp:447-449`, plume |
| DirectX 12 : la carte qui a le plus de mémoire vidéo (= la dédiée) ; un choix fait au launcher passe par le fabricant (les noms DXGI diffèrent de ceux de Vulkan) | `port/patches/plume-d3d12.patch` |
| Launcher : liste des cartes, « Carte utilisée : X (dédiée / intégrée) », choix manuel | `launcher.cpp:722-731`, `vulkan_check.h` |
| Plantage du pilote : fenêtre en anglais clair (nom de la carte, API, que faire : pilote, autre API, résolution, portable branché sur la carte dédiée) | `reblue/gpu/device_lost.cpp:100-125` (2026-11-22) |
| Résolution « auto » en 720p sur une carte intégrée | `svr_resources.cpp:337-360` |

Piège connu : les noms ne sont pas les mêmes selon l'API (« Intel(R) UHD Graphics » en DXGI, « RaptorLake-S Mobile
Graphics Controller » en Vulkan). Sur le PC d'iMrBurn, le RTX 5070 n'apparaît pas toujours dans les réglages
graphiques de Windows : il ne faut donc pas s'appuyer sur ce panneau.

## Ce qui manque

1. **En jeu**, rien ne dit au joueur que le jeu tourne sur la puce intégrée alors qu'une carte dédiée est là. Ça
   arrive quand le réglage est sur « auto » mais que la carte dédiée n'est pas utilisable (pilote, portable sur
   batterie, MUX). Le joueur voit seulement un jeu lent.
2. **Après un plantage**, la fenêtre d'erreur se ferme avec le jeu. Au lancement suivant, le launcher ne rappelle rien.

## Ce que je fais

1. **En jeu (fait, à tester)** : 5 s après le démarrage, si le jeu tourne sur la carte intégrée, que le réglage est
   sur « auto » et qu'une carte dédiée existe (NVIDIA, GeForce, Radeon RX / Pro, Arc A), un message reste 10 s en
   haut de l'écran : « THE GAME RUNS ON THE INTEGRATED GPU (…). <carte dédiée> IS FASTER: PICK IT IN THE LAUNCHER
   (GRAPHICS CARD), PLUGGED IN. » Une seule fois, et rien si le joueur a choisi sa carte lui-même. Ligne
   `[display] GPU:` dans le journal. (`wwe12_display.cpp`, aucune nouvelle clé.)
2. **Launcher (pour la session Launcher)** : au démarrage, s'il trouve dans le dernier journal du jeu
   (`Documents\wwe12\logs`, fichier le plus récent) la ligne `graphics device lost on '<carte>'`, il affiche un
   bandeau rouge : « Last time, the graphics driver stopped (<carte>). Try: update the driver, another GRAPHICS API,
   a lower INTERNAL RESOLUTION; on a laptop, plug it in and pick the dedicated card. » Le bandeau a un bouton
   « Copy diagnostic » (déjà existant) et disparaît au lancement suivant sans plantage. Aucune nouvelle clé.

## Test

- Message en jeu : lancer avec `--svr_gpu_device ""` sur le PC d'iMrBurn **en forçant l'intégrée** n'est pas
  possible sans la choisir (et le choix coupe le message). Test réel : un portable dont la carte dédiée n'est pas
  prise. Sur le PC d'iMrBurn : vérifier seulement que **rien** ne s'affiche (carte dédiée utilisée) et que le
  journal dit « dedicated ».
- Bandeau du launcher : copier une ligne `graphics device lost on 'X'` dans un journal de test, ouvrir le launcher.
