# Changelog

## 1.4.0

### Added
- **A button can have an action when it is let go.** Besides "when pressed" and "when held", a binding can now have
  "when released", run after whatever the press or hold started has ended. The release runs from the binding found at
  the press, so turning the mode dial while the button is held does not change it. In the profile file:
  `bind <button> <layer> release "" step ; step ...`. Both button editors (the app and the in-sim window) show and edit
  it. Profiles without it load and behave as before. (issue #105)
- **Two new dataref steps, Save and Restore.** `save:<dataref>` remembers the dataref's value and `restore:<dataref>`
  writes it back (nothing happens when nothing was remembered). Used with "when released" a button can change a
  setting while it is held and put it back afterwards. (issue #105)

### Changed
- **F-14: the trigger selects the Gun while held, and Fire fires the selected weapon.** Trigger stage 1 remembers
  the selected weapon (`sim/cockpit2/weapons/weapon_select_console_index`) and selects the Gun; stage 2 fires the
  guns; letting go of stage 1 puts the weapon back, so you always know what the aircraft would fire. Fire (button 2)
  runs the F-14's own trigger command (`laminar/f14/wpns/fwd_weapon_trigger`), which fires the selected weapon for as
  long as it is held. The profile version is now 11: an older F-14 profile whose trigger is still as the ready-made one
  had it gets the new trigger and Fire (when Fire is free); anything you changed stays. (issue #105)

## 1.3.8

### Changed
- **F-14: the hat names its weapons and the trigger fires the one that is selected.** The aircraft's manual binds
  the throttle hat to Weapon Select Switch 1 to 4 and the forward trigger to "fire the active weapon". The hat's
  four buttons are now called by what they select (up Sidewinder, right Sparrow, down Gun, left Phoenix, from the
  aircraft's own scripts), the trigger fires the selected weapon (`sim/weapons/fire_any_armed`) and the second pull
  is the guns. The manual says bombs are not supported in the F-14, so Fire (it was the bomb release, which does
  nothing) is free; the flares and chaff stay on T2 in combat. (issue #99)
- **F-14: master arm, nav lights and taxi light use the aircraft's own switches.** Tested in the sim through
  X-Plane's web API: the F-14 ignores the generic `master_arm_on/off`, `nav_lights_off` and `taxi_lights_off`.
  D in mode 2 is now its master arm switch (a toggle, `laminar/f14/weapons/switch/master_arm_switch`; E is free), and
  in mode 1 the throttle hat uses its wing nav light switch (up, down) and taxi light switch (right, a toggle; left
  is free). Gear, canopy, tail hook, speed brake, trim, parking brake, pitot heat, landing lights, strobes,
  avionics, battery, flares, chaff and the four weapon selects were tested and work with the generic or listed
  commands. The wing sweep commands work in flight while held (the F-14 sweeps by itself in auto), so they are unchanged.
  (issue #99)
- **F-14: stick hat 2 is the weapon select, in every mode.** Up weapon 1 (Sidewinder), right 2 (Sparrow), down 3
  (Gun), left 4 (Phoenix), as the manual describes the stick's four-way switch. The throttle hat keeps the same four
  in cruise. Hat 2 had the pitch and rudder trim: the F-14 has pitch trim on the scroll wheel in cruise and landing
  only. The F-4 keeps its trims on hat 2. (issue #99)
- **The pinkie can be the Shift in some modes only (new profile setting `pinkie_shift_modes`).** Until now the pinkie
  was a Shift in every mode or in none. With the setting at 2 it is the Shift in mode 2 only and a plain button in
  modes 1 and 3; the ready-made F-14 profile uses it: in combat (mode 2) the pinkie plus hat 2 is the trims (pitch
  up and down, rudder left and right), and in takeoff and landing the pinkie is still the wheel brakes. Changing
  the dial while the pinkie is held lets go of the brakes cleanly. Profiles that do not have the setting behave as
  before (shift in every mode, or none). (issue #99)

## 1.3.7

### Added
- **Press a button on the stick to pick it in the Buttons tab.** With the Buttons tab open, pressing a stick
  button selects it in the list and scrolls to it, so you do not have to find its number. The mode dial and the
  pinkie are not programmable and select nothing; the layer in "Programming for" stays as it is. A press still does
  what it does in X-Plane when the sim is running. (issue #90)
- **Each button's light colour on the left of its row.** Fire, A, B, D, E, the three toggles, the stick's hat 2 and
  the clutch show the colour of their light for the layer being edited (for Any mode: the mode the stick is in now,
  with the pinkie if it is held). It is only a picture; the colours are still set on the Lights tab. The button
  list is a little wider (250 to 275) to make room. (issue #90)
- **Lights tab: the Up and Down arrows move between the lights.** Left and right still change the colour of the
  light that has the keyboard (macOS 14 and newer); up and down now go to the light above or below in the same
  mode column. (issue #90)

## 1.3.6

### Fixed
- **The app's command and dataref field dropped the space, so a search like "f14 weapon" found nothing.** Symptom: in
  the Buttons tab, typing `f14 weapon` showed `f14weapon` and no suggestions (the in-sim window was fine). Cause:
  the field wrote every keystroke into the button's step, and a step keeps no blanks in a command or dataref name.
  Fix: the field keeps what you type as a search of its own and only changes the step when you pick a suggestion,
  press Return, or leave the field with a name that has no blanks (a text with blanks is dropped). The suggestions
  are a scrollable list of up to 50 matches (it was 8). (issue #89)

### Changed
- **Arrow keys move through the suggestions.** With the suggestions showing, Down from the text field selects the
  first one, Down and Up move through the list (it scrolls to follow), Up from the first goes back to the field,
  and Return picks the selected one. (issue #89)

## 1.3.5

### Fixed
- **The command and dataref search did not list the aircraft's own commands.** Symptom: the F-14's Weapon Select
  Switch 1 to 4 (`laminar/f14/fltc/cs_wpn_select_switch1..4`) and the bomb release, which X-Plane's own button screen
  shows, could not be found or picked in the app or the in-sim window; they had to be typed in full. Cause: the
  search read only X-Plane's `Commands.txt` and `DataRefs.txt`, and add-on aircraft define their commands in their
  own scripts, not in those files. Fix: the search also reads `create_command` and `create_dataref` from the
  aircraft's xlua scripts (`Aircraft/**/plugins/xlua/scripts`, with the aircraft's name as the description), and the
  names in DataRefTool's `drt_last_run_commandrefs.txt` and `drt_last_run_datarefs.txt` (X-Plane's last run, names
  only), which also covers aircraft with compiled plugins. Aircraft that use neither, and have never been run with
  DataRefTool, still need the full name typed. (issue #87)

## 1.3.4

### Changed
- **Fighters: the Fire button drops bombs, the trigger is the guns.** In the F-4 and F-14 profiles Fire was the
  selected weapon (the same as the trigger's second stage); now it is `sim/weapons/fire_air_to_ground`
  ("Drop bomb"). T1 still picks the weapon, and the trigger's second stage still fires it, so missiles are one
  trigger pull away. A Fire button that still fires the selected weapon in an older profile gets the change when
  it is read; one you changed stays. The profile format is 10. (issue #85)
- **F-14: the throttle hat picks the weapon, like the aircraft's manual.** Up, right, down and left are the F-14's
  own Weapon Select Switch 1 to 4 (`laminar/f14/fltc/cs_wpn_select_switch1..4`); Fire is its bomb release
  button (`laminar/f14/fltc/cs_bomb_release_button`). Target next/prev moved to T1 (the weapon next/prev there
  did not work well in the F-14); the circle and chase views left the hat. Ready-made F-14 profile only; copies
  of it keep what they have. (issue #85)

## 1.3.3

### Changed
- **The modes are phases of flight: 1 takeoff, 2 cruise, 3 landing.** Mode 2 and mode 3 of the ready-made profiles
  traded places (mode 2 was approach and landing, mode 3 cruise). Landing has the flaps and approach jobs, the
  speed brake, autobrake and the reverse thrust on D (held), takeoff has the gear and the lights, cruise has the
  autopilot, or all the weapons in the fighters (Combat). The gear is on A in takeoff and landing only; in cruise A is the
  landing light. Same job, same button where a feature is in two modes (lights, strobes). Older profiles, yours
  included, are changed when read: whatever they had on mode 2 and mode 3 trades places (with and without the
  pinkie, and the left wheel's page order), unless the file says `mode_layout = 2`. (issue #81)
- **Autopilot on/off is one button, B, in every mode** of the ready-made profiles that have an autopilot (it was
  an AP on button and an AP off button in takeoff and landing). The trigger is still the disconnect. C is the
  flight director in takeoff, AP altitude hold in cruise and AP approach in landing. (issue #81)
- **The Fire button no longer sets the parking brake; holding A does.** In the fighters Fire now shoots. A tap on A
  still does the gear or the landing light. A Fire button that still has the parking brake in an older profile is
  moved to a hold on A when it is read (A with a hold of its own, or a Fire button you changed, stay). The Fire light
  still shows the parking brake. (issue #81)
- **The profile format is 9.** New keys `mode_layout`, `mode_name_1..3` and `mode_banner_ms`.

### Added
- **The stick's screen says the mode's name when the mode dial moves.** "MODE 3 / Landing" for 1.5 s, then the page
  you were on comes back. The names (16 characters) and the seconds (0 to 10, 0 turns it off) are per profile, set
  on the app's Screen tab; the ready-made profiles say Takeoff, Cruise, Landing (Combat for the fighters, Launch,
  Soaring, Landing for the glider). (issue #81)

## 1.3.2

### Fixed
- **A trim button press smaller than 1 % of the range was undone by the next turn of the dial.** With a dial on
  rudder or aileron trim (or any lever job), one small trim step from a button kept the dial in charge: the dial
  counted X-Plane's value as unmoved while it stayed within 1 % of what the dial last wrote, and having let go it
  took over again at once when it was within 1 % of X-Plane's value. Now any move by a button, the mouse or the
  autopilot lets go of the dial, and the dial takes over again only when turned across X-Plane's new value. (1.3.2)
- **Twins with split levers were set to the same value once the dial took over.** Prop and mixture were written
  through X-Plane's `_all` datarefs, and cowl flaps and carb heat wrote one value to every engine, so levers set
  apart jumped together. Now every engine moves by the same amount as the dial and keeps its distance from engine 1
  (within the game's range); prop and mixture use the per-engine datarefs. (1.3.2)
- **A code comment in the plugin window was on the wrong line.** "X-Plane doing the same job wins" sat on the line
  that looks up the dial's job instead of the branch that says so. (1.3.2)

## 1.3.1

### Added
- **More jobs for the throttle's dials: smooth levers within the game's limits.** Besides Speed brake, Zoom and
  Nothing, the lower dial (i) and the E dial can each be *Prop*, *Mixture*, *Rudder trim*, *Aileron trim*, *Cowl
  flaps* or *Carb heat*. A full turn covers the game's range for the job, within the aircraft's own rudder and
  aileron trim limits; prop, mixture, cowl flaps and carb heat move every engine together. *From* and *To* (percent)
  narrow the range a full turn covers, for finer control. **Soft takeover:** after loading an aircraft or profile, or
  when a button, the mouse or the autopilot moved the value, the dial does nothing until it is turned to X-Plane's
  own value, so nothing jumps; the plugin window says where to turn it. Pick the jobs and ranges on the app's Axes
  tab or in the plugin window. New profile keys `rotary_lower_range` and `rotary_upper_range` (`0 100` by default),
  jobs 3 to 8 for `rotary_lower` / `rotary_upper`; profile format 8. The ready-made profiles keep speed brake and
  zoom. (issue #78)

## 1.3.0

### Added
- **The throttle's dials can be the speed brake or the zoom, chosen per profile.** Each profile says what the lower
  dial (i, by the clutch) and the E dial do: *Speed brake*, in four steps over the dial's turn (Armed, Retracted,
  Half, Full: X-Plane's `speedbrake_ratio` -0.5, 0, 0.5, 1), *Zoom*, with X-Plane's own zoom commands (so the mouse wheel
  and the zoom keys still work; *Zoom strength* sets how many zoom steps a full turn gives, 96 by default, 12 to 480,
  and can be changed while flying), or *Nothing*, each with *Invert*. Pick them on the app's Axes tab
  or in the plugin window. The speed brake is set only when the dial reaches another step, so speed-brake buttons and
  the auto-spoilers at touchdown still work in between. If X-Plane's own Joystick settings still use a dial for speed
  brakes or zoom, the plugin leaves that job to X-Plane and its window says to set the dial to None there. The
  ready-made profiles have the speed brake on the lower dial in the Citation X, 737, A330, MD-82, F-4 and F-14 and
  zoom on the E dial in every aircraft; any other profile (including your own) gets the speed brake and the zoom when
  it is read. New profile keys `rotary_lower`, `rotary_upper` (0 nothing, 1 speed brake, 2 zoom),
  `rotary_lower_invert`, `rotary_upper_invert` and `rotary_zoom_steps`; profile format 7. An aircraft that does not
  take the armed position (the Citation X) stays retracted in the first step. (issue #75)
- **Switch the plugin off and on from the app.** A button at the left of the app's status row: green *Plugin on*,
  red *Plugin off*. Off, the plugin does nothing in X-Plane (no buttons, axes, dials, screen or lights), X-Plane's own
  Joystick settings are in charge and the app drives the stick's screen and lights; the status reads "Stick
  connected, plugin off". It takes effect within a second and lasts until switched back on, across restarts of the
  app and of X-Plane. (issue #75)

### Fixed
- **A dial assigned in X-Plane flickered: the zoom shivered in and out.** With the E dial assigned to zoom (or the
  lower dial to speed brakes) in X-Plane's Joystick settings, the view zoomed in and out by tiny amounts all the
  time, and even X-Plane's Joystick panel showed the curve flickering. The cause is the dial itself: resting between
  two of its 256 positions, its reading flips by one, and X-Plane passes every reading on. The plugin now drives the
  dials from a steady reading (a change under 2 % of the turn is ignored), so the flicker never reaches X-Plane; set
  the dials to None in X-Plane's Joystick settings. Fixed in 1.3.0 (issue #75).

## 1.2.7

### Added
- **The throttle's mouse nub pans the view.** The small stick on the throttle did nothing. Pushed part of the way it
  does what the arrow keys do in X-Plane (pan), pushed all the way it does what Shift + the arrow does (a faster pan).
  Like POV 1,
  the plugin turns it into buttons (44 to 47 for the arrows, 48 to 51 for Shift + arrows) that every profile can
  bind. The ready-made profiles have them, and any other profile (including your own) gets them when it is read,
  unless you had already programmed those buttons. Set the nub to None in X-Plane's Joystick settings so only the
  plugin acts on it. The printed button maps mention it in their footer. (issue #67)

### Changed
- **POV 1 and the nub use X-Plane's own commands, not keystrokes.** 1.2.6 sent the keys Q, E, R and F for POV 1.
  Keystrokes go through macOS and need its permission; with the sim busy they queued up, so the view reacted late and
  then spun. POV 1 now runs `sim/general/rot_up`, `rot_right`, `rot_down` and `rot_left` (what those keys do) and the
  nub `sim/general/up`, `right`, `down`, `left` and their `_fast` versions (what the arrow keys and Shift + arrow keys
  do), inside X-Plane, with no permission needed. A POV 1 binding still exactly as 1.2.6 made it is switched over when
  the profile is read; a binding you changed stays as it is.

## 1.2.6

### Fixed
- **POV 1 (the stick's main hat) did not look around in any profile.** The stick reports POV 1 as a hat switch, not
  as buttons, and the plugin only used it to pick a profile with the clutch held, so no profile could bind it and
  it did nothing unless X-Plane had been told to by hand. POV 1 now gives four buttons (40 to 43: up, right, down,
  left; a diagonal holds two) that look around while pushed, with X-Plane's own keys: Q left, E right, R up, F down.
  The ready-made profiles have them, and any other profile (including your own) gets them when it is read, unless
  you had already programmed those buttons; you can change or clear them in the app like any button. With the
  clutch held the hat still picks a profile. If X-Plane also has an assignment for that hat, set it to None.
  The printed button maps (PDF and Word) now show the main hat as "Look around" and say so in their footer.
  Fixed in 1.2.6 (issue #63).

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
