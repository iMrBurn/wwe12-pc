# New look of the Android app (yellow and black)

Design and code by **Yexclsv** (Android co-maintainer), integrated in 1.1. Thank you!

## State on 2026-11-06

- **Done:** integrated in the repo (commit 54622e1, credit in `THIRD_PARTY.md`). It compiles; the launcher
  pages PLAY and SETTINGS and the start of the game were checked on the x86_64 emulator (portrait, 1080x2400).
- **Not checked:** a real phone, landscape (side menu), the DLC / CONTROLS / DRIVER / DEBUG pages, the button
  layout editor, MY SAVES, the loading screen over the game (it passed too quickly to be seen). **Yexclsv tests the
  test APK on a phone before the release.**
- Original text of the note (before this cleanup): `docs/archives/android_UI_REDESIGN_version_2026-11-05.md`.

UI only: no game logic, settings, files or native code changed. Not compiled or tested on a device (written without
a build environment): please build and report errors.

- **Layout.** The launcher is no longer one long scroll. On wide screens (>= 600 dp, a phone held sideways) there is
  a side menu on the left and one section at a time on the right; on a phone held upright, tabs at the top and a PLAY
  bar at the bottom. Sections: PLAY (game, phone check, update, support), DLC, SETTINGS (quick presets first, then
  graphics, then WWE Universe), CONTROLS, DRIVER, DEBUG. PLAY is always on screen. The section shown is kept when
  the screen is rebuilt (presets).
- **Look.** Black background, yellow accent (the PC launcher's), condensed italic headings, rounded panels with a
  title mark, ripple buttons, yellow switches instead of check boxes, drop-downs with an arrow, caution-tape stripes
  under titles. All drawn in code: no images, no font files. `Ui.java` holds the palette and the widgets; the
  launcher, MY SAVES and the button layout editor use it.
- **Loading.** A moving caution-tape bar with a changing tip: while the phone check runs, when PLAY is tapped
  (`LauncherActivity.showLoading`, taken down when the launcher comes back), and over the game itself until it has
  drawn its first frame (`GameLoading.java`, started by `WWE12Activity`). The game one waits for the log line
  "Android performance hint" (written at the first presented frame, wwe12_perf_hint.cpp) and gives up by itself
  after 90 s.
- Phone warning panels get a red or orange border.

Files: Ui.java (new), GameLoading.java (new), LauncherActivity.java, SavesActivity.java, LayoutEditorActivity.java,
WWE12Activity.java (one line).
