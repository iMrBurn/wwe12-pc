# New look of the Android app (yellow and black)

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
