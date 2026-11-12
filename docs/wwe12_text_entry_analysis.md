# Saisie de texte au clavier (grille de lettres du jeu)

> **État au 2026-11-12 : FAIT** (2026-10-19, `port/src/native/wwe12_kbd.cpp`). Le clavier écrit dans les grilles
> de lettres du jeu (Create a Story, noms…) : AZERTY, Maj, répétition ; les accents donnent la lettre simple.
> - Le vrai texte est dans l'objet « éditeur » de la grille (écran − 0x740). Les lettres sont rejouées par
>   `sub_828A47D0(éditeur, s, 1, 0)`, et BACK par `sub_828A6158`. Détails : `docs/savoir_acquis.md` (« Saisie de
>   texte au clavier »).
> - Sonde `wwe12_kbd_log` ; lanceur `tools/dev/lancer_test_clavier.bat`.

L'analyse hors ligne d'origine (OpenCode, en anglais) est archivée :
`docs/archives/wwe12_text_entry_analysis_anciennes_parties_2026-11-12.md`. **Elle est dépassée** : elle visait
`[widget+200]`, qui n'est qu'une copie d'affichage, et `sub_82BFD248`, qui n'est jamais appelé.
