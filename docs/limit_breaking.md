# Limit breaking — idées à tester

Réserve d'idées pour repousser les limites du jeu d'origine. Rien n'est commencé ;
on ajoute au fil de l'eau et on note ce qu'on découvre.

## Contexte

- La 360 n'avait que 512 Mo partagés. La plupart des plafonds (catcheurs à l'écran,
  slots de création, roster) viennent probablement de là, pas du moteur.
- Le PC a beaucoup de marge : ~390 Mo de VRAM par match en 1440p.
- Probable premier mur : les tas mémoire du jeu (déjà des « PhysicalHeap::Alloc unable
  to alloc » au chargement).
- Outils déjà en place : patch de .pac au démarrage servi par l'overlay VFS
  (cf. wwe12_menu_patch.cpp), hooks de fonctions (menu, fps, intros), scanner mémoire F7
  (wwe12_memscan.cpp).
- Acquis : le 6-man Elimination Chamber tient 60 fps sans crash.
- Prévu après la 1.1 (feuille de route de CLAUDE.md). Limite des 6 catcheurs gravée dans le
  code : tableaux à 6 cases, budget mémoire Xbox, animations.
- Kaiko a fait un « Limit Breaking » pour son portage SvR 2011 : lui demander jusqu'où il est allé
  (plus de 6 catcheurs ?) et comment.
- Demandé par des joueurs : matchs à 8-10 catcheurs, 5 contre 5.
- Le mode Univers enrichi (qualifications, surprises du Rumble) a sa propre conversation
  « Univers » (docs/univers.md).

## Idées (de la plus accessible à la plus lourde)

| # | Idée | Piste | Statut |
|---|------|-------|--------|
| 1 | Nouvelles combinaisons de stipulations (cage en tag, HIAC à plus de 2, élimination ailleurs…) | Les types de match sont-ils des tables de données comme menu.pac ? | idée |
| 2 | Plus de catcheurs sur le ring (8, 10…) | Plafond codé en dur (tableaux de 6 ?), slots manettes/IA, spawn | idée |
| 3 | Royal Rumble : plus de catcheurs en même temps sur le ring | Même plafond que le n° 2 | idée |
| 4 | Création : plus de slots CAW, de logos et d'arènes, roster plus grand | Taille des sauvegardes et de EditEx.cac (400 Mo) | idée |
| 5 | Visuel : foule plus dense, distance d'affichage, LOD au max | Paramètres de la foule et des LOD | idée |

## Notes de recherche

_(vide pour l'instant)_
