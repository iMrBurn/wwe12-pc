# WWE '12 PC — FAQ

*Français plus bas.*

This is a free fan project made in spare time: updates come when they're ready, with no dates. Bugs go
in [GitHub issues](https://github.com/iMrBurn/wwe12-pc/issues) (the form tells you what to attach).

## Installing and updating

**How do I install it?** Download the latest zip from
[Releases](https://github.com/iMrBurn/wwe12-pc/releases), unzip it, put the disc image (`.iso`) of
**your** WWE '12 for Xbox 360 in the `game` folder, then run `WWE12 Launcher.exe`.

**How do I update?** The launcher offers new versions on its PLAY page: one click. Your settings,
`game` folder, saves and mods are kept.

**Windows Defender / my antivirus says the exe is dangerous.** It's a false positive: the exes aren't
digitally signed (that costs money), they're new and rare, and the launcher downloads updates, which
antivirus heuristics dislike. The source code is public here. Each release lists the SHA-256 of its
files so you can check yours.

## DLC

**How do I add DLC?** Put the DLC packages of **your** game (the files with long hexadecimal names) in
the `dlc` folder next to the game. They're installed at the next start. See `dlc\HOW_TO_INSTALL_DLC.txt`.

**Some DLC wrestlers are missing (Batista…).** Fixed automatically in 0.9. Before that: close the game,
then in `Documents\wwe12` delete the `xbox_cache` folder and the
`0000000000000000\54510890\00000002` folder (**not** `00000001`: that's your saves). Start the game
again: the DLC are reinstalled and every wrestler shows up.

**Do I need the Title Update?** No. It isn't supported yet (planned for 1.1).

## Graphics and performance

**Black screen, or a message about Vulkan.** The port needs a graphics card with a Vulkan driver (most
cards since 2016). Intel HD 4000/5000/6000 and some old AMD laptop cards don't have one on Windows.
Update your graphics driver first.

**The game is slow / in slow motion.** The game runs at 60 frames per second; when your PC can't keep up,
the whole game slows down (that's how the original works). Try, in the launcher: a lower internal
resolution (720p or 540p); empty stands with `wwe12_crowd = false` in `wwe12.toml`. On a laptop: plug in the charger, use Windows' "Best
performance" mode, and make sure the launcher's PLAY page uses your dedicated graphics card. Close
OBS, recording tools and overlays. Laptops with 2-core processors are at the limit.

**Coloured blocks during entrances or matches (Intel / AMD / Steam Deck).** Version 0.9 has a
probable fix. If you still see them, add `svr_rt_no_compression = true` at the end of `wwe12.toml`, and tell us in
issue #7 whether it helps.

**The picture froze but the sound goes on.** Wait 15 seconds, close the game, and open an issue with
`hang_trace.txt`, `stderr.txt` and the log from `Documents\wwe12\logs`. Please test without OBS or
overlays first: they're a known cause of freezes.

## Controls

**Can I play with a PlayStation controller?** Yes (DualShock 4, DualSense). Plug it in **before**
starting the game to get PlayStation button icons (automatic since 0.9; in 0.8 add
`wwe12_button_icons = "playstation"` at the end of `wwe12.toml`, or use the launcher's CONTROLS page).

**Two players with one controller?** Launcher → CONTROLS → "Keyboard plays as" Player 2.

**FPS counter?** F2 in game.

**Which key names can I use for the keyboard?** Launcher → CONTROLS → Set, then press the key: it
writes the exact name. Typed names are case-sensitive: `A`-`Z`, `0`-`9`, `F1`-`F24`, `Up` `Down`
`Left` `Right`, `Space` `Return` `Escape` `Tab` `Backspace` `Delete` `Insert` `Home` `End` `PageUp`
`PageDown`, `Minus` `Plus` `Comma` `Period` `Semicolon` `Slash` `Backslash` `LBracket` `RBracket`
`Quote` `Backtick`, `Numpad0`-`Numpad9` `NumpadEnter` `NumpadPlus` `NumpadMinus` `NumpadStar`
`NumpadSlash`, `CapsLock` `NumLock` `ScrollLock` `PrintScreen` `Pause`, mouse `LMB` `RMB` `MMB`, and
`Shift`, `Ctrl`, `Alt`. Several keys: separate them with commas (`Z,W`); a combination: `Shift+Up`.
Since 1.0, `Shift`, `Ctrl` or `Alt` can be a button on its own (issue #18). Left and right Shift are
the same key. A modifier set on its own no longer works in combinations (`Shift+Up` does nothing
then): the launcher warns about it.

## Saves

**Can I use my Xbox 360 saves?** Yes: launcher → SAVES → import your WWE '12 save packages.

**Where are my saves? Are they backed up?** In `Documents\wwe12`. The SAVES page makes backups, and one
is made automatically before anything is replaced or deleted.

**"You are not currently signed in with a gamer profile" / can't save.** Fixed in 0.9 (it happened
with some controller setups in 0.8).

## Universe mode

**Title defenses in weekly shows.** Off by default (the original game never puts a title match in a weekly show).
Turn it on in the launcher (Universe options) or in the game: EDIT UNIVERSE > UNIVERSE OPTIONS > TITLE DEFENSES
(10 / 25 / 50 / 100 %, `wwe12_universe_title_defenses` in `wwe12.toml`). A single match between the champion and
another Superstar can then become a title match. 25 % is a good start.

**Qualifying matches.** Off by default. EDIT UNIVERSE > UNIVERSE OPTIONS > QUALIFYING MATCHES, or
`wwe12_universe_qualifiers = true`. The two weeks before Money in the Bank and Elimination Chamber, qualifying
matches are played on the shows, with news; the winners take their place in the match at the pay-per-view.
There are no qualifiers for the Royal Rumble.

**Fewer than 15 Superstars per show.** `wwe12_universe_min_roster` (1 to 15, default 15 = original game). The game
itself warns that matches may be badly balanced under 15.

## Mods

Files in the `mods` folder next to the game replace the game's own (same path as in the game). See
`mods\HOW_TO_USE_MODS.txt`.

**Removing an added superstar (Mod Maker).** Before you turn off or remove a superstar mod, stop using him in
your save (Universe, matches, rivalries). A save made with an added superstar needs him: the game keeps his
pack as it was (the launcher's MODS page says so, and superstar mods added since don't get in) until you take
him out in the Mod Maker (CHECK MODS > Take out of the pack). The save is copied (SaveBackups) before the pack
changes.

---

# FAQ en français

Projet de fan gratuit, fait sur le temps libre : les mises à jour sortent quand elles sont prêtes, sans
date. Les bugs se signalent dans les [issues GitHub](https://github.com/iMrBurn/wwe12-pc/issues).

- **Installer** : téléchargez le zip dans « Releases », dézippez, mettez l'ISO de **votre** WWE '12 Xbox
  360 dans `game`, lancez `WWE12 Launcher.exe`. Les mises à jour se font en un clic depuis le launcher.
- **Antivirus** : faux positif (exe non signé, nouveau, et le launcher télécharge les mises à jour). Le
  code est public ; chaque release donne l'empreinte SHA-256 de ses fichiers.
- **DLC** : mettez les paquets de **votre** jeu dans `dlc`. Des catcheurs DLC manquent (Batista…) ?
  Corrigé automatiquement en 0.9 ; avant : jeu fermé, supprimez dans `Documents\wwe12` le dossier
  `xbox_cache` et le dossier `0000000000000000\54510890\00000002` (**pas** `00000001` : vos sauvegardes),
  puis relancez.
- **Title Update** : pas encore prise en charge (prévue pour la 1.1), pas nécessaire.
- **Écran noir / message Vulkan** : il faut une carte graphique avec un pilote Vulkan (pas les Intel HD
  4000/5000/6000). Mettez à jour votre pilote.
- **Ralentis** : le jeu ralentit si le PC ne tient pas 60 images/s. Baissez la résolution interne (720p,
  540p), tribunes vides avec `wwe12_crowd = false` dans `wwe12.toml`, branchez le chargeur, mode « Meilleures performances », bonne carte graphique
  sur la page JOUER, fermez OBS et les overlays.
- **Pavés colorés à droite pendant les entrées** : essayez `svr_rt_no_compression = true` à la fin de
  `wwe12.toml` et dites-nous dans l'issue #7 si ça aide.
- **Image figée, son qui continue** : attendez 15 s, fermez, et ouvrez une issue avec `hang_trace.txt`,
  `stderr.txt` et le journal de `Documents\wwe12\logs` (testez d'abord sans OBS).
- **Manette PlayStation** : branchez-la avant de lancer le jeu pour avoir les icônes PlayStation (en 0.8 la détection automatique ne marche pas : ajoutez
  `wwe12_button_icons = "playstation"` à la fin de `wwe12.toml`, corrigé en 0.9). **Deux
  joueurs avec une manette** : launcher → CONTRÔLES → clavier = joueur 2. **Compteur FPS** : F2.
- **Noms des touches du clavier** : launcher → CONTRÔLES → Définir, puis appuyez sur la touche (liste des
  noms exacts dans la partie anglaise). Depuis la 1.0, `Shift`, `Ctrl` ou `Alt` peut être un bouton à lui
  seul (issue #18) ; il ne marche plus alors dans les combinaisons (`Shift+Up`), le launcher le signale.
- **Sauvegardes Xbox 360** : launcher → SAUVEGARDES → importer. Copies de sauvegarde automatiques.
- **« Not signed in with a gamer profile »** : corrigé en 0.9.
- **Mode Univers, défenses de titre en weekly** : désactivé par défaut (le jeu d'origine n'en fait jamais).
  Launcher (options de l'Univers) ou en jeu : MODIFIER L'UNIVERS > OPTIONS DE L'UNIVERS > DÉFENSES DE TITRE
  (10 / 25 / 50 / 100 %, `wwe12_universe_title_defenses`). 25 % pour commencer.
- **Mode Univers, matchs de qualification** : désactivé par défaut. `wwe12_universe_qualifiers = true` ou la ligne
  MATCHS DE QUALIFICATION en jeu. Les 2 semaines avant Money in the Bank et Elimination Chamber, des qualifications
  ont lieu dans les émissions, avec des news. Pas de qualifications pour le Royal Rumble.
- **Moins de 15 catcheurs par émission** : `wwe12_universe_min_roster` (1 à 15, 15 = jeu d'origine).
- **Mods** : les fichiers du dossier `mods` remplacent ceux du jeu (même chemin).
- **Enlever un catcheur ajouté (Mod Maker)** : avant de désactiver ou d'enlever son mod, ne l'utilisez plus
  dans votre sauvegarde (Univers, matchs, rivalités). Le jeu garde son pack tant que vous ne l'avez pas retiré
  dans le Mod Maker (VÉRIFIER > Retirer du pack) : la page MODS du launcher le signale. La sauvegarde est
  copiée (SaveBackups) avant chaque changement du pack.
