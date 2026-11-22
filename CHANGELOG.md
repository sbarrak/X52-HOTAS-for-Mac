# Changelog

## 1.2.0

### Added
- **Stick handling in the plugin.** While the plugin drives the stick, X-Plane's own curve and stability
  augmentation are skipped, so the plugin now does their job. Both settings are saved in the profile
  (`stick_smoothing`, `stick_damping`) and shown in the app's Axes tab and the X-Plane window's Axes tab.
  - **Smoothing** (default 60 ms): small, slow stick changes (jitter, hand tremor) are smoothed; big or quick moves,
    such as letting go of the stick, reach X-Plane at once. 0 = off.
  - **Damping** (default 30 %): a light pitch and roll damper near the centre of the stick, never more than a few
    percent of the travel, off on the ground and with the autopilot on. 0 = off.
- **Stick calibration** (X-Plane window, Axes tab: **Calibrate stick**, **Reset**). Move the stick and twist to every
  limit, let go, press **Done**; each side is then scaled from the measured centre to the measured end. Kept in
  `stick-calibration.txt` next to the profiles, since it belongs to the stick and not to a profile.

### Changed
- The stick now starts soft around the centre (roll, pitch and yaw get a dead zone of 3 % and a curve), because the
  X52's short, light-sprung stick was far too twitchy for the elevator with a straight line. The throttle stays
  linear. Profile files are now version 4; an older file whose stick axes were never changed (5 % dead zone, no
  curve) gets the new default, and axes set on purpose are left alone.
- `.gitignore`: `my_profiles/` (own profiles for add-on aircraft) stays local.

### Fixed
- **The aircraft bounced up and down when the plugin drove the stick** (fixed in 1.2.0). With *The plugin drives
  the stick and twist* on, the nose would not stay put and every correction overshot, while the same aircraft flew
  steadily with X-Plane reading the stick. Cause: the override (`override_joystick`) makes X-Plane skip its own
  response curve and stability augmentation, and the plugin sent the X52's stick to the elevator in a straight
  line (expo 0), with no damping, assuming the centre was exactly mid-range. Fix: the soft default curves,
  smoothing, damping and calibration above. They leave the simulation alone where it matters: a change of a tenth
  of the travel or more (letting go of the stick, a quick input) reaches X-Plane in the same frame, and the damper
  is capped at 4.5 % of the travel by default (15 % at full strength), fades out as the stick moves from the centre,
  and is off on the ground and with the autopilot on.

## 1.1.1

### Added
- **Undo and redo for profile changes**, in the app and in the X-Plane window. Every edit to the profile in use
  (lights, alarms, buttons, screen pages, axes, ...) can be taken back and brought back, up to 50 steps.
  - App: **Undo** and **Redo** arrows at the top, next to the profile picker; Edit > Undo / Redo, **Command+Z**
    and **Shift+Command+Z**. While a text field is being edited, that field keeps its own undo.
  - X-Plane window: **Undo** and **Redo** buttons left of **Change profile...**; Command+Z / Shift+Command+Z while
    the window has the keyboard; and **Plugins > X52 Pro > Undo last profile change / Redo profile change**, with the
    commands `x52pro/undo` and `x52pro/redo` for a key or a button of your own.
  - Quick edits (a slider drag, typing) count as one step. Switching to another profile starts a new history.
    A change made by the other program (the app or the plugin) is a step too, so it can be undone.
  - Ready-made profiles are never changed, so they have nothing to undo.
- **Button tips.** Rest the mouse on Undo or Redo for two seconds and a short tip says what it does, or why it
  is greyed out.

### Changed
- X-Plane window: the tabs are a little narrower (and **Change profile...**) to make room for the two buttons.

## 1.1.0

### Added
- **Grouped profile menu.** The profile pickers list **Your profiles** first and **Ready-made profiles** second.
  In the app the picker is a popover, in the X-Plane window the **Change profile...** list; in both, each group
  folds open with **+** and shut with **-**. The group of the current profile starts open.
- **A popover that fits its content.** The app's profile popover grows and shrinks with the folded groups and is
  never taller than the window (it scrolls beyond that).
- **A scrolling plugin dropdown.** The plugin's lists scroll with the mouse wheel when they do not fit the window.
- **A status line that keeps its place.** One fixed-height line under the header shows an error, the
  ready-made notice or the plugin note (in that order of importance). The tabs and everything below no longer
  jump when a message appears or goes away. The ready-made notice is now a single line.

### Changed
- The "(ready-made)" suffix is gone from profile names; the group header says it.
- Docs: README and the manual's source describe the new menu. The manual's pictures and the Word file still
  show the old one and need regenerating.

### Fixed
- Selecting a ready-made profile showed the blue notice, and switching back to one of your own removed it and
  moved the whole window up; the notice now has its own reserved line.

### Developer
- The plugin test for the profile picker covers folding and unfolding the groups.
