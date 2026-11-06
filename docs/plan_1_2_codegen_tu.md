# Plan 1.2 : un seul nouveau code généré (TU de THQ + registres locaux + « volatile »)

> **État au 2026-11-06** : plan écrit, **rien codé, rien testé**. **Version visée : 1.2** (après la sortie de la
> 1.1). **Voie B choisie** par l'utilisateur (2026-11-05). La voie A est archivée. À faire en premier : l'étape 0
> (mesure de référence, avec le go de l'utilisateur pour lancer le jeu).


Gameplay, 2026-11-05. **Décision de l'utilisateur :** tout ce qui change le code généré se fait ensemble après la
1.1, pour la 1.2 : une seule régénération, un seul gros test.
- Ce qui change le code généré : les options de registres locaux, le patch `volatile`, le passage au xex patché
  des mises à jour de THQ.
- **Rien n'est fait maintenant** (pas de codegen, pas de compilation).
- Bases : `docs/title_update_corrections.md` (ce que les TU corrigent) et `docs/perf_natif_steam_deck.md` (section
  « Natif / code du jeu »).

## 1. Deux voies pour les corrections de THQ

### Voie A (écartée) : générer le code depuis le xex de la TU2

Écartée par l'utilisateur le 2026-11-05 : chaque joueur aurait dû fournir le fichier de mise à jour (interdit de le
distribuer), ou il aurait fallu deux exe. Détails et chiffres du remappage :
`docs/archives/plan_1_2_codegen_tu_anciennes_parties.md`.

### Voie B (choisie) : rester sur le code du disque et refaire nous-mêmes les corrections utiles

Chaque correctif a son propre réglage pour revenir en arrière.

| Correctif (voir `title_update_corrections.md`) | Coût | Remarque |
|---|---|---|
| Finishers (TU2) : 6 à 7 cas spéciaux (Spear 3, Sleeper Hold 2, Ankle Lock 3, Edgecution, Skull Crushing Finale, 37229, 57006) dans `sub_821D3F58`, `8223F440`, `8223F878`, `82280DF0`, `823CCDB8`, `823F24C8` | Petit à moyen : un crochet par fonction, chaque condition refaite au bon endroit (souvent au milieu de la fonction, donc il faut reproduire le chemin avant). Le plus simple : Skull Crushing Finale, en fin de fonction. | Il faut d'abord reproduire chaque gel en jeu, prise par prise. |
| Univers : les CAW comptent dans l'effectif (TU2) | Facile : 3 crochets, plus une ligne dans notre recomptage. | Change les règles : accord de l'utilisateur. |
| Univers : index < 23 (TU2, 5 fonctions) | Facile. | Protection contre une corruption. |
| Prises des DLC 3-4 (CA09/CA10) dans l'éditeur de moveset (TU1) | Moyen à gros : renuméroter les banques dans ~15 fonctions et agrandir un tableau sur la pile. | Utile aussi pour nos prises CA11+ ; perte des prises à la sauvegarde pas prouvée (essai en jeu d'abord). |
| Résultats Univers `sub_826CEF28` réécrite, booking du PPV, Draft | Inconnu (pas compris). | À étudier seulement si un bug y correspond. |
| Armes (paramètre ajouté), 3 réglages 40/60/80 → 70/80/90 | Moyen / très facile, mais sens inconnu. | Plus tard. |

**Coût de la voie B :** petit à moyen, par morceaux. Aucun recalage d'adresse, un seul exe, les joueurs n'ont
rien à fournir. **Limite :** seulement ce qu'on a compris.

**Décision de l'utilisateur (2026-11-05) : voie B** (nos propres correctifs sur le code du disque). Recommandation d'origine : la voie B. La voie A impose aux joueurs un fichier qu'on ne peut pas leur donner, ou deux exe.
À trancher par l'utilisateur.

## 2. Ordre de travail proposé pour la 1.2 (une seule sortie, mais des étapes internes pour retrouver une panne)

0. **Référence avant tout changement**, sur la compilation 1.1. Données isolées, `lancer_isole.bat`.
   - Scènes fixes, jouées à la télécommande (`wwe12_remote_pad`) : menu principal, éditeur de moveset (CAW), match
     1 contre 1, match à 6, émission Univers, une entrée, un replay d'après-match.
   - Pour chaque scène :
     - captures d'écran aux mêmes moments ;
     - `[perf] cpu per frame` (Rendu) ;
     - échantillons du fil principal (`SVR_SAMPLE_STACKS=5`, `samples.txt` effacé avant, carte `.map` de la même
       compilation) ;
     - un vidage de quelques zones de la mémoire du jeu après N images (fiches d'os d'un catcheur, état du
       match), pour comparer octet par octet.
1. **Options de registres locaux**, dans `port/wwe12_config.toml` :
   - d'abord `cr_as_local`, `ctr_as_local`, `xer_as_local`, `reserved_as_local` (risque faible) ;
   - puis `non_argument_as_local` ;
   - puis `non_volatile_as_local`.
   - À chaque palier : codegen, compilation, mêmes scènes, mêmes mesures. Les vidages mémoire doivent être
     identiques ; seul le temps doit changer.
   - Point à vérifier : nos crochets qui lisent `ctx` après l'appel de l'original. `ctx.rN` reste-t-il à jour pour
     les registres devenus locaux ? Le SDK doit les recopier aux appels et aux sorties : à confirmer dans le code
     généré avant de compiler.
2. **Patch `volatile`** du modèle `pch_h.inja` (patch SDK dans `port/patches/`) : on le garde pour MMIO et pour les
   boucles d'attente entre fils (à repérer). Mêmes contrôles.
3. **Corrections de THQ** :
   - en voie B, ajoutées une à une, chacune avec son réglage ;
4. **Gros test final** (le « un seul gros test » de l'utilisateur) :
   - toutes les scènes de l'étape 0 ;
   - plus Univers sur plusieurs semaines, Create a Superstar, Create an Entrance, Mod Maker, DLC ;
   - plus les 3 exe (Vulkan, D3D12, D3D11) et Android ;
   - plus le 4 fils.

Pourquoi des paliers internes malgré « une seule régénération » : si l'étape 4 casse quelque chose, on sait à quel
palier le chercher. Ces paliers ne sortent jamais chez les joueurs.

## 3. Ce qu'il faudra pour démarrer (go de l'utilisateur)

- Lancer le jeu pour l'étape 0 (mesure de référence).
- Pour lire les patchs du SDK de Jarrod (registres, volatile) : téléchargement = go de l'utilisateur, reprise de
  code = accord de Jarrod.
