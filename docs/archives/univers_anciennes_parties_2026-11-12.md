# Mode Univers : anciennes parties (archives, 2026-11-12)

Parties de `docs/univers.md` déplacées le 2026-11-12 (rangement des docs demandé par l'utilisateur) : étude de WWE '13 dont les idées sont faites ou listées ailleurs, premiers essais de création et de remise à zéro (remplacés par §11, §16, §27), plantage 1.0.0 corrigé. Gardées telles quelles, avec leur numéro d'origine. Plus anciennes : `univers_anciennes_parties.md`.

---

## 7. Ce que fait WWE '13 (étudié le 2026-10-18, pour s'en inspirer)

Source : la copie de l'utilisateur (recomp WWE '13 de PromethenDev, fichiers du jeu fournis par
l'utilisateur). **Rien de WWE '13 ne doit aller dans le dépôt ni dans les zips.** On ne reprend pas son
code (autre exe) : il sert de modèle, même moteur Yuke's.

**Menus Univers de WWE '13** : mêmes numéros de textes que WWE '12 (27000…), les ajouts occupent des
numéros **vides dans WWE '12** :
- 27026–27030 : EDIT SUPERSTARS, EDIT ROSTERS, EDIT TEAMS, **EDIT TITLES**, **RESET UNIVERSE** (sous
  « EDIT UNIVERSE », 27045, qui remplace EDIT SUPERSTAR) ; 27048 FEUD MANAGER ; **27049 STATISTICS**.
- Édition des titres : 27651/27663/27665/27682/27896 **VACANT**, « No belt is assigned to this
  category », catégories MAJOR / MINOR / TAG / DIVA (27606–27609).
- Le nom interne de l'écran est `ResetUniverse` (0x8205FA54 dans l'exe de WWE '13) ; la remise à zéro
  relit les **valeurs de départ** (`CWWEUniverseStationed::BatPacReaderCallBack_SaveInit`).

**Statistiques de WWE '13** (textes 27824–27849 et 33200–33243) :
- par catcheur : victoires, **WIN%**, apparitions, temps d'activité ;
- **titres** : nombre de règnes (TIMES EARNED), **semaines de règne** (WEEKS HELD), anciens champions
  (PAST CHAMPIONS), nouveau record ;
- **Money in the Bank** : anciens gagnants, résultat du cash-in, % de victoires après cash-in ;
- **Royal Rumble** : gagnants, apparitions, adversaires éliminés, temps passé dans le Rumble ;
- **WrestleMania** : résultats, main events, % de victoires.

**Valeurs de départ de l'Univers dans WWE '12** : `pac/misc.pac`, groupe **BATL/SAVE** (même nom que
dans WWE '13) ; chargé par la classe à vtable 0x8203E7D8 (`sub_826EC0A8` ouvre « BATL/SAVE »,
`sub_826EC008` garde les entrées 2 et 3 via `sub_826D69C8` → globales 0x832315E4 / 0x832315E8).
- entrée 2 « LBTS » : équipes de départ (Kane & Big Show, Santino & Kozlov, Otunga & McGillicutty,
  Demolition, Road Warriors, Divas of Doom…) ;
- entrée 3 « HID » ;
- entrée 4 (PACH de 6) : « WUD » (fiches de départ des catcheurs), « WDR » (relations de départ),
  émissions (RAW, SmackDown, Superstars), calendrier des 13 PPV (Royal Rumble…), marques (RAW,
  SmackDown, Diva, Legend, FreeAgent…). Qui lit l'entrée 4 : **pas encore trouvé**.

**Conséquences pour WWE '12** :
- Les textes des nouvelles entrées peuvent reprendre les numéros libres (27026–27049…) avec les mêmes
  mots que WWE '13 (patch de string.pac au démarrage, comme menu.pac).
- Le **suivi des stats** doit être écrit par nous : WWE '12 ne garde ni victoires ni historique (§3.3).
  Idée : après chaque match Univers (`sub_826C3258`), ajouter une ligne dans un fichier à nous
  (Documents\wwe12\universe_stats…), et un écran STATISTICS (ImGui au style du jeu, comme GRAPHISMES).
- **Reset** : vider le bloc (`sub_826D5938`), remettre les champions Univers par défaut, puis laisser le
  jeu recréer l'Univers à partir de BATL/SAVE (à vérifier en jeu).

---

## 9. Comment le jeu crée l'Univers (test « sauvegarde neuve », 2026-10-20)

Test : sauvegarde de l'utilisateur mise de côté (puis remise, empreinte vérifiée), lancement 1 =
sauvegarde neuve sans entrer dans l'Univers, lancement 2 = première entrée dans l'Univers.

- **L'Univers est créé avec la sauvegarde**, pas à la première entrée : la sauvegarde neuve contient
  déjà les points de départ des catcheurs, les cartes, etc. La première entrée ne change que
  193 octets (dont 12 zones du bloc Univers et quelques octets du bloc 2 vers +0x576C4 / +0x584E3…,
  sans doute des drapeaux « déjà vu »).
- Création d'une sauvegarde neuve : `sub_8269E698` → `sub_82699F10` (bloc 1 remis à zéro, dont
  `sub_826D5938` pour l'Univers), puis un chargeur lit **BATL/SAVE** (`pac/misc.pac`) et appelle pour
  chaque entrée la fonction de la table à 0x83073240 : {2 → `sub_8269EF80` (équipes LBTS),
  3 → `sub_82A41578` (HID), **4 → `sub_826BE3C0` (Univers)**}.
- **`sub_826BE3C0(données, taille)`** = « SaveInit » de l'Univers (même rôle que
  `CWWEUniverseStationed::BatPacReaderCallBack_SaveInit` de WWE '13) : elle lit les 6 morceaux de
  l'entrée 4 (0 = WUD fiches de départ, 1 = WDR relations, 2 = émissions [cartes vidées par
  `sub_826D5228`], 3 = PPV, 4 = 7 réglages, 5 = marques) et les écrit dans le bloc Univers. C'est
  aussi elle qui imprime « RP Setup,Status,RosterID,No ».
- Le test « Univers vidé » plantait parce qu'il ne faisait que la première moitié.

**Recette de la remise à zéro** (à coder) : `sub_826D5938(U)` puis `sub_826BE3C0` avec l'entrée 4 de
BATL/SAVE (lue depuis misc.pac), puis remettre les champions de la table Univers aux valeurs par
défaut (table 0x8324E3A8, comme `sub_82A39E08` mais seulement pour la table Univers), enfin laisser le
jeu enregistrer. À faire hors d'un match, depuis un menu.

---

## 10. Remise à zéro : ça marche (2026-10-20)

`port/src/native/wwe12_universe.cpp` (cvar `wwe12_universe_dev`, off par défaut ;
`tools/dev/lancer_test_univers.bat`). Testé en jeu par l'utilisateur, sur sa sauvegarde
(copiée avant, remise après) : F8 sur un écran Univers → retour au tout début (calendrier,
cartes, champions), y compris après avoir simulé jusqu'à un PPV.

- Le jeu lit l'Univers de départ en entrant dans l'Univers : `sub_826EC008(this, pack)` reçoit le
  **pack BATL/SAVE ouvert** (une poignée du lecteur, sur la pile du chargeur, pas des données) ;
  l'entrée d'id 4 est l'Univers de départ, compressé en BPE. `sub_827D8E30(pack, id)` = index,
  `sub_827D9528(pack, index)` = copie décompressée (6 756 octets, un PACH de 6 entrées), libérée par
  `sub_827D2EA0(copie, 0)`. On en garde une copie (mémoire de jeu à nous, SystemHeapAlloc).
- La remise à zéro : `sub_826D5938(U)`, `sub_826BE3C0(copie, taille)`, champions Univers par défaut
  (table 0x8324E3A8 → 0x83226508). Lancée depuis le crochet des textes (`sub_821A4400`, fil du
  jeu) quand le texte demandé est un texte Univers (27000–27999).
- Sonde : le menu Univers (27041–27047) est construit par `sub_8255EC88` (texte demandé via
  0x824CA80C ← 0x8255ED44) ; les cartes de matchs par 0x82556A70 ← 0x8255778C.
- Prochaine étape (2) : un vrai choix « RESET UNIVERSE » (EDIT SUPERSTAR → EDIT UNIVERSE avec un
  sous-menu, comme WWE '13), avec confirmation, à la place de F8.

---

## 39. Plantage 1.0.0 « 5 matchs RAW + 5 SmackDown » corrigé (2026-11-03, testé sur la copie, candidat 1.0.1)

Trace d'un joueur : C0000005 dans la règle de carte `sub_826EF988` (table de règles 0x8203E680, 25 règles, la plupart
lisent l'entrée +0), appelée par `sub_826EB510`. L'entrée n° 4 (le 5e match, 48 octets, +0 = modèle de match) était
vide. Le booker hebdo `sub_826F15E8` prend son modèle par `sub_826DC6E8(table, groupe, i)` : notre réécriture ne
visait que l'appel qui revient à 0x826F1690 ; l'autre choix (table 15 groupe 0, quand `sub_826CD010` répond 1 : les
shows d'après un PPV) prenait le modèle i tel quel, et ce groupe n'en a que 4, donc NULL. Correctif : tout appel venant
de `sub_826F15E8` qui dépasse les modèles du groupe prend le dernier (dernier match) ou tourne sur les autres ; filet :
un match hebdo au-delà du nombre du jeu resté sans modèle est re-booké comme un des matchs du jeu, puis remis à sa
place (+8). Vu : 2-3 mois simulés à 5/5 avec les qualifs, « template 4 of group 0 -> 3 » sur les RAW / SmackDown
d'après l'Elimination Chamber, plus de plantage ; le filet ne s'est pas déclenché.
