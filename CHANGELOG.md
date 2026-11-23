# Changelog

## 1.2.5

### Fixed
- **Parts of the settings window did not follow its size.** On the Test tab the list of alarms to try stayed
  130 points tall however big the window was, so with a tall window it showed four alarms and cut the fifth in half
  with empty space below. The list was given a fixed height. It now takes all the height the tab has left.
  The same kind of fixed size was found elsewhere: the page list on the Pages tab (fixed 360 points) now grows
  with the window, and the Axes, Lights and Screen & clock tabs, which were taller than the window at its smallest
  size and were cut off at the bottom, now scroll. Fixed in 1.2.5.

## 1.2.4

### Changed
- **Manual and pictures brought up to date.** The pictures in `docs/images` and the Word manual now show the
  grouped profile picker, the undo and redo buttons, the calibration and the update check. The manual describes the
  new menu bar items. The picture maker (`X52Pro --snapshot`) gives the Axes tab a taller window so its top is no
  longer cut off.

## 1.2.3

### Added
- **The app tells you when a newer version is out.** The menu bar menu has **Check for updates...** (it says
  *Update available: v1.x.x...* once one is found) and **Check for updates automatically**, on by default. The
  automatic check runs when the app starts, at most once a day, and stays silent unless there is something new.
  Only a change of the first two numbers counts (1.2.3 to 1.3.0 does, 1.2.3 to 1.2.40 does not), so small
  changes never ask anyone to download again. A check by hand always answers: new version, up to date, or why it could not reach GitHub. It reads the project's
  latest release on GitHub (one request, nothing about you or your Mac is sent) and only opens the release page;
  nothing is downloaded or installed by itself.

## 1.2.2

### Fixed
- **Command+Z and Shift+Command+Z did nothing in the app** (the Undo and Redo buttons worked). Edit > Undo and Redo
  were left to the responder chain, and the SwiftUI window answered them first with its own, always empty, undo
  manager, so the items stayed greyed out and the keys went nowhere. They are now aimed at the app itself: they
  undo and redo the profile, and while a text field is being edited they still undo that field's typing.
  Fixed in 1.2.2.

## 1.2.1

### Added
- **Stick calibration in the app, step by step.** **Calibrate stick...** (Axes tab, or the menu bar menu) opens a
  window that guides you through it: (1) the stick to its four edges, drawn as a circle with a dot at each edge and a
  bar growing towards it as you push, (2) the twist both ways, drawn as a ring (or skip it), (3) let go, so the
  resting point becomes the centre, (4) a summary of min / centre / max per axis and **Save**. The raw values show
  live at every step; nothing changes until Save. The Axes tab shows the saved values and has **Reset**. Before,
  calibrating was only possible in the X-Plane window and the app said nothing about it. The app and the plugin
  share one calibration file: a calibration saved in the app is picked up by a running X-Plane within a second,
  and one made in X-Plane shows in the app. Profiles (curves, smoothing, damping) are not touched by it.
- **Pitch check** on the X-Plane window's Axes tab while the plugin drives the stick: what the plugin sends for
  pitch (and how much of it is the damper) next to what X-Plane then uses (its yoke, the elevator trim and the pitch
  rate), to find out where a climb or a dive comes from.

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
