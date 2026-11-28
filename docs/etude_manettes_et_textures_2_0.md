# Études courtes : « toutes les manettes = joueur 1 » et copie des textures (2.0, chantiers 3 et 4)

Date : 2026-11-28 (Rendu).

## Chantier 3 : toute manette branchée joue le joueur 1

**Le problème.** Windows (ou le SDK pour une manette PlayStation / générique) peut mettre une manette dans la
place 2, 3 ou 4. Le jeu la prend alors pour le joueur 2, et le joueur 1 n'a que le clavier.

**Ce qui existe.** Notre crochet `XInputGetState` (`sub_82A8C070`, `wwe12_intros.cpp`) voit chaque lecture de
manette. Le clavier (mode « mnk » du SDK) est toujours la place 1.

**Ce que je fais (codé, à compiler).** Une option `wwe12_all_pads_p1` (coupée par défaut) :
- La lecture du joueur 1 **réunit toutes les manettes branchées** : boutons ensemble, et pour chaque stick et
  gâchette, celui qu'on pousse le plus. Le clavier reste dedans.
- Les places 2 à 4 répondent « pas de manette » : une manette ne joue pas aussi le joueur 2.
- Rien ne change quand l'option est coupée. Avec l'option, plus de jeu à plusieurs sur le même PC (c'est le but).

**Nouvelle clé** à valider par iMrBurn pour le launcher : `wwe12_all_pads_p1` (case « Every controller plays
player 1 »). Sans son accord, elle reste seulement en ligne de commande.

**Test** : manette branchée + clavier ; avec l'option, la manette et le clavier bougent le même catcheur ; une
2e manette aussi. Sans l'option : comme avant.

## Chantier 4 : une copie des textures plus économe

On ne refait **pas** les mesures du 2026-11-22 (priorités des fils, saut de l'empreinte des textures : aucun gain).

**Ce que disent les journaux du banc petit PC (2026-11-22, déjà mesuré)** :
- **En match : 0 copie de texture** par seconde. Rien à gagner là.
- **Pendant les entrées** : environ **280 copies par seconde**, pour **75 à 93 ms par seconde** de travail. Ce
  sont les vidéos des écrans (3 vidéos × 3 plans Y / U / V × 30 images). Les entrées tournent déjà à **31 i/s
  pour 30 visées** sur ce banc.

**Conclusion** : même en divisant ce coût par deux, le gain ne se verrait pas. Les entrées sont déjà à leur
cadence, et le match ne copie rien. **Je propose d'abandonner ce chantier** et de le dire à iMrBurn. Si un joueur
signale des saccades **seulement pendant les entrées** sur un petit PC, on y reviendra avec ses journaux.
