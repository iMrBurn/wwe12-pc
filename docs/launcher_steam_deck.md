# Launcher : profil « Steam Deck / petit PC »

> **État au 2026-11-12** (commits locaux ; la détection et la fenêtre de proposition datent du 2026-11-06) :
> - **Fait** : détection (`machine_profile.h`, `machine::Detect` / `Choose`), fenêtre de proposition à l'ouverture, « Annuler le préréglage »,
>   copie de sécurité étendue (`wwe12_widescreen`, `gpu_backend`) ; confort Deck du launcher : manette / clavier, fenêtre bornée à
>   la zone de travail, tailles plus grandes (commits ce45166, fa35037).
> - **Testé** : logique de choix hors ligne (cartes inventées + ce PC : un PC normal ne reçoit aucune proposition) ; fenêtre de
>   proposition ouverte avec le journal de test `WWE12_LAUNCHER_PROFILE_LOG` ; hors Deck, `SteamDeck=1` pour simuler.
> - **Pas testé** : un vrai Steam Deck, Proton / Wine, lecture XInput sous Steam Input, `ShellExecute` (liens) sous Proton, boîtes de
>   choix de fichier sous Proton, le bouton Annuler après le profil Deck. Aucun testeur Deck ni issue Deck ouverte.
> - Le conseil « 30 en match (expérimental) » du test de performance n'est jamais appliqué sur Deck / petit PC (décision de la Main) ;
>   le champ `frame_mode` du profil Deck reste à décider.
> - Ancien texte complet (liste des gênes Deck avant le code, essais hors ligne détaillés, où citer la FAQ) :
>   `docs/archives/launcher_steam_deck_anciennes_parties_2026-11-12.md`.

## 1. Détection (`machine::Detect`, `machine::Choose`)

| Indice | Comment | Utilisé pour |
|---|---|---|
| Steam Deck | variable `SteamDeck=1` (posée par Steam, transmise par Proton) **ou** nom de carte : « Custom GPU 0405 / 0932 », « VANGOGH », « Van Gogh », « Steam Deck » | profil **Deck** |
| Wine / Proton | `ntdll` exporte `wine_get_version` | API Vulkan imposée dans le profil |
| Carte utilisée | celle du jeu (`g_vulkan.UsedDevice`), intégrée (2) / dédiée (1) | +2 points si intégrée |
| Mémoire vidéo | DXGI, seulement pour la carte choisie retrouvée par son nom (la mémoire « de n'importe quelle carte » est fausse sur les portables à deux cartes) | +2 points si dédiée < 3,5 Go |
| Cœurs physiques | `GetLogicalProcessorInformationEx` | +1 point si ≤ 4 |
| RAM | `GlobalMemoryStatusEx` | +1 point si ≤ 8 Go |
| Batterie | `GetSystemPowerStatus` (lu, pas utilisé pour décider) | informatif |

Profil **Deck** : 720p, foule / ombres / flou coupés, 16:10 activé, API Vulkan.
Profil **petit PC** (score ≥ 2) : 1 point = 720p ; 3-4 points = 540p ; ≥ 5 points ou ≤ 4 Go de RAM = 480p ; foule coupée si score ≥ 3 ;
ombres et flou coupés ; 16:10 activé ; Vulkan seulement sous Wine / Proton. Un PC normal (score < 2) : rien n'est proposé.

## 2. Ce que voit le joueur

- À l'ouverture du launcher (après la vérification Vulkan) : fenêtre « Cela ressemble à un Steam Deck / à un petit PC », une phrase
  qui dit pourquoi, la liste de ce qui changera, trois boutons : **Appliquer le profil**, **Pas maintenant** (redemandé au prochain
  lancement), **Non merci, ne plus demander**.
- « Appliquer » et « Non merci » écrivent `profile_offer_done.txt` à côté du launcher ; pas de proposition non plus si le préréglage
  PC MODESTE a déjà été appliqué.
- **Annuler** : RÉGLAGES > « Annuler le préréglage » (même bouton que PC MODESTE, copie `lowend_preset_backup.txt`).
- Une seule logique, `ApplyPreset(Preset)` ; un préréglage **ne remonte jamais** une résolution déjà plus basse que la sienne.
- Texte de guide (Deck seulement, dans la fenêtre) : « voir la FAQ, section Steam Deck ».
