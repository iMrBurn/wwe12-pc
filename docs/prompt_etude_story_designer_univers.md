# Prompt de départ : étude « Cinématiques et Story Designer + Univers » (WWE '12 PC)

À coller comme premier message d'une NOUVELLE conversation (modèle conseillé : Fable pour l'étude, Opus ensuite pour le code), dans le dossier du projet `C:\Users\melfo\Documents\WWE12-PC\wwe12-pc`. Cette conversation s'appelle « Cinématiques / Story Designer » : elle ne touche pas aux sujets des autres (Rendu, Gameplay, Univers, Mods, Launcher, Android).

---

Tu es la conversation « Cinématiques / Story Designer » du projet WWE '12 PC (portage PC par recompilation statique, dépôt iMrBurn/wwe12-pc). Lis d'abord `CLAUDE.md` (règles absolues), puis `docs/savoir_acquis.md`, `docs/univers.md`, `docs/arbitre_special.md` (méthode d'import d'animations de WWE '13) et la ligne « Cinématiques Create a Story de WWE '13 / 2K14 » de `docs/feuille_de_route.md`. Réponds à l'utilisateur en français simple (il est débutant) ; textes publics en anglais.

**Objectif de l'utilisateur** (long terme, après la 1.4, retour prévu en janvier 2027) : deux choses, comme un autre projet de portage de SvR 2011 les annonce :
1. **Cinématiques d'autres jeux** : de nouvelles scènes pour les rivalités et les grands moments (attaques, face-à-face, interventions), prises dans WWE '13 / 2K14 / SvR, jouables dans WWE '12.
2. **Story Designer + Univers** : faire fonctionner ensemble le mode Create a Story (« Story Designer ») et le mode Univers de WWE '12.

**Ce qu'on sait déjà** (à vérifier, pas à croire) : les scènes de Create a Story ont le même format dans WWE '12, WWE '13 et 2K14 (EPK8/EPAC, groupes EVP / EVZ, scripts /GMEV/%04d, numéros 4000-4399) : WWE '12 = 201 scènes, WWE '13 = 254 (53 absentes du 12), 2K14 = 263 (63 absentes), environ 64 nouvelles. L'outil `entrance13.h` du Mod Maker convertit déjà des entrées de WWE '13. **Non étudié** : la table de la bibliothèque (catégorie, nom, rôles, place dans le menu ; `sub_8275A958` remplit la table des scènes demandées), les objets / textures / décors nécessaires. **Piste connue qui échoue** : imposer une scène Create a Story dans l'Univers donne un **écran noir** (le lecteur de scènes dépend du mode) ; la session Univers avait mis de côté les scènes d'autres jeux dans l'Univers.

**Ce que je te demande (étude, pas de code livré au départ)** :
1. Comprendre pourquoi la scène Create a Story donne un écran noir dans l'Univers (ce qui manque : mode, objets, caméra, script, décor) avec des preuves (journal, traces, adresses), pas des suppositions.
2. Comment la bibliothèque de Create a Story liste ses scènes, et ce qu'il faut pour en ajouter proprement (table, noms, rôles, menu).
3. Ce qu'il faut importer de WWE '13 / 2K14 pour les ~64 scènes nouvelles (objets, textures, décors, animations).
4. Un rapport `docs/etude_story_designer_univers.md` : faisable ou non, étapes, estimation en heures et en essais de l'utilisateur, risques, ce qui peut se faire par morceaux jouables, et ce qui doit rester désactivé par défaut.

**Règles** : lecture seule au début ; données du jeu extraites seulement dans un dossier temporaire puis supprimées, jamais dans le dépôt ni dans un zip (sauf accord explicite de l'utilisateur, déjà donné pour I Quit et l'arbitre spécial de WWE '13 seulement) ; ne jamais lancer le jeu sans prévenir l'utilisateur et seulement avec son « vas-y » ; un seul jeu / une seule compile à la fois ; commits locaux seulement, la Main pousse sur le « go » de l'utilisateur ; tout nouveau comportement coupé par défaut tant qu'il n'est pas vu en jeu ; garde ton rapport à jour pour qu'une autre session puisse reprendre.
