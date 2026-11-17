# Story Designer / Create a Story: notes from the WWE '12 PC port (for the SvR 2011 port)

Short, shareable version of our study of 2026-11-16. It contains no game data, only findings.

## Please read first

- **Everything here is about WWE '12 (Xbox 360).** Function addresses (`sub_82xxxxxx`) are WWE '12's and will not match
  SvR 2011. What may carry over is the method and the structure, not the addresses.
- **We have not checked SvR 2011.** We do not know whether it has the same library file, the same scene numbers or the
  same player. Treat every point as "worth checking", not as a fact about your game.
- **Static analysis only.** We read the recompiled code and the game files; nothing below was tested in game yet. The
  proposed fix in section 1 is untried.

## 1. Why a Create a Story scene gives a black screen outside the story mode

In WWE '12, a cut scene numbered 4000-4999 always gets the Create a Story scene director and player, whatever the game
mode. That player only runs its per-frame update when the game is in the "story being played" mode (or the editor
preview). Elsewhere (for us, Universe mode) it never starts: everything loads, frames keep being presented, but the
step that shows the world again after the loading screen never runs, so the screen stays black. Missing optional
per-scene scripts (particles, game events) were not the cause.

- Forcing the "story" mode to make it run does not work: at the end of the sequence the game reads that mode and sends
  you back to the story editor's menu.
- Proposed fix (untried): leave the mode alone and make the single "is the editor previewing?" check return true while
  your injected scene is the current one.
- A scene whose number is outside the story range gets the plain director and has no mode gate (this is how entrances
  work in every mode).

Things to look at in your game: which object chooses the director from the scene number, and which update function is
gated on the mode.

## 2. How the library of scenes is stored

In WWE '12 the list of scenes in the editor is **data, not code**: a PACH inside `pac/menu/sas.pac` (group `SASE`,
entry `DATA`).

- Child 0: categories and scenes. 17 categories, 197 records of 24 bytes: category, a few option bytes, a bit mask of
  allowed places (arena, GM office, parking lot...), scene number (u16), text ids for name and description.
- Child 3: branching questions (three answers each). Child 6: one placement record per scene (88 bytes).
- Roles are **not** in the table: the editor reads them from the scene's own script (up to 6 cast tracks).
- Lookups are by scene number, linear, range 4000-4999. The number of categories (17) is hard-coded in the code.
- A saved story stores the scene number, so adding records does not break existing saves.
- Thumbnails are optional; missing per-scene scripts are tolerated.

WWE '13 (18 categories, 40-byte records, same first 16 bytes) and 2K14 use the same layout, so their records can tell
you the category and place mask of each extra scene.

## 3. Scenes from other games

The scene files have the same format across WWE '12, '13 and 2K14 (EVP script + EVZ track bank, BPE-wrapped). WWE '12
has 201 scene files (197 in the library), '13 has 254, 2K14 has 263; 64 are new in total. The same scene number is the
same scene across the three games, **except show intros**, whose number means a different show in each game.

Of the 64 new scenes: 18 need nothing special, 12 use script commands WWE '12 never uses (not known if its engine
ignores them), 7 need extra props and an unknown place code, 26 are show intros tied to arenas WWE '12 lacks. Converting
tracks needs the usual work (leg IK baked, 2K14 track names; the face bones are missing, so faces may be frozen).

## 4. What we would do first

One in-game test with logging (does playback start, does the scene end, does the actor have a model) before writing
any bigger feature.
