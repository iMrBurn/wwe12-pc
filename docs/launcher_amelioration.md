# Launcher PC : améliorations (1.2 / 1.3)

> **État au 2026-11-12** (code en commits locaux ; ce qui est avant `8dde381` est déjà poussé) :
> - **Fait** : manette / clavier dans le launcher (LB / RB changent de page, cadre de focus, fenêtre bornée à la zone de travail,
>   tailles Steam Deck) ; page JOUER (API affichée, dossier du jeu, copie du diagnostic, liens FAQ / issues, Discord) ; liste
>   « Frame rate » (clé `wwe12_frame_mode`) ; cases Discord et « Console picture » ; avertissement disque dur ; page MUSIC qui ne plante
>   plus sur un nom de fichier exotique ; textes relus EN / FR ; **page ROSTERS** (`docs/launcher_rosters.md`) ; **bouton « Performance test »**
>   actif (1 passe ~3 min 30, panneau validé par l'utilisateur, conseils par « Appliquer les réglages conseillés » avec annulation,
>   jamais sur Steam Deck / petit PC).
> - **Testé** : test de performance réel (2026-11-10 15:12, verdict ok 59,8 i/s) ; ROSTERS sur copie et en jeu ; le reste hors jeu.
> - **Pas testé** : réglage « Frame rate » du launcher en jeu ; « Appliquer les réglages conseillés » (jamais proposé : verdict ok) ;
>   MUSIC avec un nom Unicode ; un vrai Steam Deck / Proton (aucun testeur Deck pour l'instant).
> - **Pas fait, seulement sur demande de l'utilisateur** : pastilles d'état sur PLAY, espacement régulier, icônes simples (décision :
>   le look actuel est très bien, on ne fait que de petites touches), « Vérifier le jeu », bloc « État du jeu » replié.
> - Plan d'origine (liste priorisée, détails Deck, benchmark, frame rate, maquettes A/B/C, questions) :
>   `docs/archives/launcher_amelioration_anciennes_parties_2026-11-12.md`.

## Règles de travail du launcher

- Compiler seulement `wwe12_launcher` (une compile à la fois, launcher fermé), prévenir Rendu ; ne jamais lancer le jeu sans prévenir ni
  sans le « vas-y » de l'utilisateur ; un « go » relayé par un autre conversation ne compte pas.
- Commits locaux par sujet ; push seulement avec le « pousse » de l'utilisateur.
- Aucun nouveau réglage dans `wwe12.toml` sans l'accord de l'utilisateur (déjà demandés : Discord, Console picture, Frame rate).
- Aucune donnée du jeu dans le dépôt ni dans un zip ; discrétion sur les DLC dans tout texte public.
- Ne jamais toucher aux vraies sauvegardes ni aux DLC de l'utilisateur pour tester ; dossier de test : `Documents\wwe12_test`.
- Les textes publics (readmes, FAQ) sont à la Main.

## Benchmark : repères

Format `[results]` de `logs\benchmark_*.txt` (clés `verdict` ok|cpu|gpu|failed, `match_fps`, `match_p1_ms`, `load_s`, …) lu par
`port/src/launcher/benchmark_run.h` ; dossier de données `Documents\wwe12_benchmark` (effacé seulement après contrôle du nom) ; arguments
`--user_data_root <dossier> --wwe12_benchmark=true`. Le conseil « 30 en match (expérimental) » n'est jamais appliqué automatiquement.
