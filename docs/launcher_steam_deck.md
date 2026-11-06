# Launcher : profil « Steam Deck / petit PC » (2026-11-06, axe A de la 1.1+)

> **État au 2026-11-06** : code écrit (`port/src/launcher/machine_profile.h` + `launcher.cpp`), **jamais compilé ni lancé**
> (seulement : test hors ligne de la logique de choix, et vérification de syntaxe `clang -fsyntax-only` de `launcher.cpp`, sans
> erreur). Demande de l'utilisateur relayée par Main. Rien poussé. Reste : compilation + essai en vrai avec le « vas-y » de
> l'utilisateur, puis essai sur un vrai Steam Deck par un testeur.

## 1. Détection (`machine::Detect`, `machine::Choose`)

| Indice | Comment | Utilisé pour |
|---|---|---|
| Steam Deck | variable d'environnement `SteamDeck=1` (posée par Steam, transmise par Proton : le jeu s'en sert déjà, `SvrOnSteamDeck`) **ou** nom de la carte : « Custom GPU 0405 / 0932 », « VANGOGH », « Van Gogh », « Steam Deck » | profil **Deck** |
| Wine / Proton | `ntdll` exporte `wine_get_version` | API Vulkan imposée dans le profil |
| Carte utilisée | celle du jeu (`g_vulkan.UsedDevice`), genre : intégrée (2) / dédiée (1) | +2 points si intégrée |
| Mémoire vidéo | DXGI, **seulement pour la carte choisie, retrouvée par son nom** (la mémoire « de n'importe quelle carte » est fausse sur les portables à deux cartes : mesuré sur ce PC, 128 Mo) | +2 points si dédiée < 3,5 Go (limite de Rendu) |
| Cœurs physiques | `GetLogicalProcessorInformationEx` | +1 point si ≤ 4 |
| RAM | `GlobalMemoryStatusEx` | +1 point si ≤ 8 Go |
| Batterie | `GetSystemPowerStatus` (lu, pas encore utilisé pour décider) | informatif |

Profil **Deck** : 720p, tribunes / ombres / flou coupés, 16:10 activé, API Vulkan.
Profil **petit PC** (score ≥ 2) : 1 point faible = 720p ; 3-4 points = 540p ; ≥ 5 points ou ≤ 4 Go de RAM = 480p ; tribunes coupées
si score ≥ 3 ; ombres et flou coupés ; 16:10 activé ; Vulkan seulement sous Wine / Proton. Un PC normal (score < 2) : rien n'est proposé.
Valeurs mesurées par le test hors ligne : voir §4.

## 2. Ce que voit le joueur

- À l'ouverture du launcher (une fois la vérification Vulkan finie) : fenêtre « Cela ressemble à un Steam Deck / à un petit PC »,
  une phrase pour dire pourquoi (c'est le processeur qui limite), la liste de ce qui changera, et trois boutons :
  **Appliquer le profil**, **Pas maintenant** (on redemande au prochain lancement), **Non merci, ne plus demander**.
- « Appliquer » et « Non merci » écrivent `profile_offer_done.txt` à côté du launcher : plus de proposition. Pas de proposition non
  plus si le préréglage PC MODESTE a déjà été appliqué (sa copie de sécurité existe).
- **Annuler** : RÉGLAGES > « Annuler le préréglage » (le même bouton que PC MODESTE). La copie de sécurité (`lowend_preset_backup.txt`)
  garde maintenant aussi `wwe12_widescreen` et `gpu_backend` ; les anciennes copies (4 clés) se relisent comme avant.
- Une seule logique : `ApplyPreset(Preset)` ; le bouton PC MODESTE appelle `ApplyPreset(Preset{})` (480p, tout coupé, 16:10 et API
  inchangés). Nouveauté commune : un préréglage **ne remonte jamais** une résolution déjà plus basse que la sienne.
- Texte de guide (Deck seulement, dans la fenêtre) : « voir la FAQ, section Steam Deck ».

## 3. Confort Deck : ce qui gênerait (liste, rien de codé)

1. **Manette** : ImGui n'a pas `NavEnableGamepad` (ni clavier) activé ; le launcher se pilote donc à la souris / au pavé tactile / à
   l'écran tactile seulement. Piste (petite) : `io.ConfigFlags |= NavEnableGamepad | NavEnableKeyboard` ; à vérifier : les gros boutons
   dessinés à la main (`BigButton`) doivent être « focusables », et la lecture XInput sous Proton (Steam Input).
2. **Taille de la fenêtre** : 1120 × 780 (× DPI) + cadre > 800 px de haut : le bas serait coupé sur l'écran 1280 × 800 du Deck.
   Piste : limiter la fenêtre à la zone de travail de l'écran (ou démarrer agrandie quand l'écran fait ≤ 800 px de haut).
3. **Taille des boutons et du texte** : boutons ImGui de ~28 px, texte d'aide en gris clair : trop petits au doigt sur 7 pouces.
   Piste : en mode Deck, échelle de police et de style × 1,25 (comme le DPI).
4. **Fenêtres de choix de fichier** (import de sauvegarde, DLC) : les boîtes Windows marchent sous Wine mais voient `Z:\` et les
   dossiers Linux ; utilisables, pas confortables à la manette.
5. **Page CONTRÔLES** : la capture d'une touche demande un clavier physique (clavier à l'écran de Steam : Steam + X) ; sans objet pour
   jouer à la manette.
6. Ouvrir un dossier / un lien depuis le launcher (`ShellExecute`) dépend de `xdg-open` sous Proton : non vérifié.

## 4. Essais faits (hors ligne, `clang++` à part, jamais le projet)

Sortie du test de `Choose` (cartes inventées + ce PC) : Deck (par nom de carte ou par variable) → 720p, tribunes/ombres/flou coupés,
Vulkan ; vieux PC 2 cœurs / 4 Go / 940MX → 480p ; 4 cœurs / 8 Go / GTX 1050 2 Go → 540p ; PC 6 cœurs / 16 Go / GTX 1660 → **rien
proposé** ; Wine + carte intégrée → 720p + Vulkan ; ce PC (RTX 5070 dédiée) → **rien proposé** (un premier essai proposait à tort un profil à
cause de la mémoire DXGI d'une autre carte : corrigé).
**Pas essayé** : la fenêtre dans le launcher, le bouton Annuler après le profil, un vrai Steam Deck, Proton.

## 5. Où citer le guide de la FAQ

- Fait : phrase dans la fenêtre de proposition (Deck seulement).
- Proposé : page JOUER, une ligne grise sous la ligne « Carte graphique utilisée », affichée seulement si le profil Deck ou Wine est
  détecté (« Steam Deck ? Voir la FAQ, section Steam Deck ») ; et la page À PROPOS, dans la liste des liens, en permanence.
  Un lien cliquable dépend de `ShellExecute` sous Proton (point 6) : mettre d'abord du texte simple.
