# Menus du jeu réutilisés par le portage

Ce qu'on a appris en refaisant EDIT UNIVERSE (2026-10-20, voir aussi `docs/univers.md` §13) et
OPTIONS > GRAPHICS (2026-10-20) avec les **vrais menus du jeu**, sans rien dessiner par-dessus.

## Menus de `menu.pac` (menu principal, OPTIONS…)

- Deux tables (deux mises en page), en-tête `{1, nombre, …}` puis des fiches de 0x74 octets gros-boutistes.
- Champs utiles : +0x18 texte, +0x30 (pas le parent : groupe de position), +0x34 identifiant,
  **+0x50 parent** (identifiant de la fiche dont c'est un sous-menu), +0x54 genre (1 = sous-menu,
  5 = écran, 7 = dernière entrée de la liste), +0x6C lien (numéro d'écran).
- Le menu est servi patché par l'overlay (`wwe12_menu_patch.cpp`) ; on **renomme** des fiches existantes,
  on n'en insère pas (l'insertion casse la navigation B).
- Lien 1304 = GAMEPLAY > IN-GAME OPTIONS : `sub_825470B0(lien)` → page d'options `sub_8255E7C0(9)`.

## Pages d'options = scripts Lua 5.1

- `pac/menu/MScript.pac`, groupe `MENS` : un script compilé par page (`/MENS/0014` = IN-GAME OPTIONS
  « DrawIngameOptions », 4 lignes ; `/MENS/0015` = RTWM OPTIONS « DrawRTWMOptions », 5 lignes ;
  `GAMB` = « DrawGeneral », 9 lignes…). En-tête `1B 4C 75 61 51 00 01 04 04 04 04 00` : Lua 5.1,
  petit-boutiste, int / size_t / instruction sur 4 octets, **nombres en float** 4 octets.
- Lister un script : `luadis.py` (scratchpad de la conversation Univers ; à recopier dans `tools/` si
  besoin). Le script d'une page a les fonctions globales `Init`, `Action`, `Draw`, `FromDecision` (A),
  `FromCancel` (B), `FromCross` (croix), `FromSub2` ; natifs `yFrame.*` (calque, curseur, couleurs,
  `Get32BitArray` / `Set32BitArray`), `yMenu.*` (`SetHelp`, `MakeWarningYesNo`, `OptLib` qui définit
  `COL_WHITE` / `COL_GRAY`), `yOption.*` (`InitInGameOptions` / `UpdateInGameOptions`…).
- Le jeu charge le script par `luaL_loadbuffer` = `sub_827E9418(L, tampon, taille, nom)` (appelé par
  `sub_824CFD08`). **Son analyseur de code source plante** (accès invalide dans les listes de sauts de
  lcode) : il faut donner du **bytecode**. `tools/graphics_pages.py` écrit nos pages en Lua et les compile
  avec un `luac` 5.1.5 réglé sur ce format (recette dans l'en-tête du script ; source lua.org, licence MIT,
  pas dans le dépôt).
- Valeurs : tableau de la page = mots 852+i de `[0x8317C430 + 4 × [0x8306B718]]` ; `InitInGameOptions(n)`
  le remplit (`sub_82DEDD70`), `UpdateInGameOptions(n)` le relit (`sub_82DEC528`). Un `n` inconnu ne fait
  rien : nos pages passent 100 + onglet et nos crochets s'en chargent.
- Textes : numéros libres dans toutes les tables à partir de 43100 (43011–44999 libres), servis par le
  crochet du gestionnaire de textes `sub_821A4400` (`wwe12_universe.cpp`).

## OPTIONS > GRAPHICS (`wwe12_graphics_menu.cpp`)

L'entrée IN-GAME MANUAL (texte 41184, renommée GRAPHICS) garde son sous-menu ; ses trois entrées
visibles (TUTORIAL / ABILITIES / ATTRIBUTES) deviennent AFFICHAGE / GRAPHISMES / AVANCÉ (lien 1304).
Le crochet « decide » (`wwe12_quit.cpp`) note l'onglet choisi ; le chargement du script de 0014
(5 158 octets) est remplacé par notre page. Mêmes 13 réglages, mêmes clés de `wwe12.toml` que le
launcher ; « * » = appliqué au prochain lancement. Testé en jeu le 2026-10-20 (valeurs appliquées et
enregistrées). L'ancien écran dessiné en ImGui est retiré.

## Boîte de dialogue du jeu (`*0x8324DEF0`)

Oui/Non `sub_8254E7E8`, information `sub_8254E680`, **liste de choix** `sub_8260DD80` (2 lignes dans le
calque ; une 3e se place à la main, voir `docs/univers.md` §13), fermée ? `sub_8254E8C8`.
