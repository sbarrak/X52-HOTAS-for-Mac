# Changelog

## 1.7.2

### Added
- **A setup window (#139).** It shows the X-Plane 12 folder it found (or asks for it), asks what to install, and installs the plugin, the ready-made profiles and the guides (the manual and the button maps, in `Documents/X52 Pro`). Profiles of your own are never overwritten. **Open guides** opens the folder; **Setup...** opens the window again.
- **Uninstall (#139).** **Uninstall X52 Pro...** removes everything that was installed and asks whether to keep your own profiles and settings or move them to the Trash. The steps for removing it by hand are in the guides.

### Changed
- **The download is a disk image (#139).** Drag **X52 Pro** to **Applications**; "Read me first" says how to allow the app in Privacy & Security.
- **One left click on the menu bar icon opens the window (#139).** It took a double click before; a right click still opens the menu.

### Fixed
- **The Dock icon stayed after the app was hidden (#139).** Symptom: after Command+H, or after closing the last window, the app's icon stayed in the Dock. Cause: the app joined the Dock when a window opened and nothing took it out again. Fix: hiding the app or closing its last window removes the Dock icon; a minimised window keeps it. Fixed in 1.7.2.
- **A double click on the menu bar icon did not open the window (#139).** Symptom: double-clicking the icon did nothing. Cause: the window opened only when macOS reported the second click as a double click. Fix: one left click opens it. Fixed in 1.7.2.
- **The setup window's keyboard permission step was confusing (#139).** Symptom: Accessibility settings opened and X52 Pro was not in the list. Cause: X-Plane types the keystrokes, not X52 Pro, and the text did not say so. Fix: the text says that X52 Pro needs no permission and the ready-made profiles type no keys, and **Show X-Plane in Finder** helps add X-Plane. Fixed in 1.7.2.

## 1.7.1

### Changed
- **New button maps for the ready-made profiles (#133).** The Word and PDF maps in `docs/mapping/` have a new layout: what each control does in the three modes and with the Pinkie held, the screen pages, lights and alarms, the aircraft's systems with photos, and its lights. The civil aircraft maps also list holds and sequences in order. The 18 civil maps were checked against their profiles; none of this was a flight test. The checks are recorded in `docs/mapping/Map-update-report.md`.

### Fixed
- **The button maps on GitHub were broken (#133).** Symptom: the PDF and Word maps in `docs/mapping/` published with 1.3.0 were damaged, and later ones did not match the profiles. Cause: patching the text of the files in place failed and the result was committed. Fix: the maps are replaced with newly built ones. Fixed in 1.7.1.

## 1.7.0

### Added
- **A Demo button (#130).** Next to the connection status at the top of the app's window. It plays a show of about 12 seconds on the stick: the lights come on one by one, turn green, amber and red, flash, and the screen shows messages and scrolls a list down and back up. **Stop** ends it early; afterwards the stick shows the profile again. It is not available without a stick or while X-Plane drives it, and it changes nothing in the profile.

### Fixed
- **Finder and the open app showed the old icon (#130).** Symptom: after the new icon of 1.6.3, an app built over an earlier build still had the old icon in Finder and while open. Cause: the build only replaced files inside the app, so the app folder kept its old date and macOS kept its cached icon. Fix: every build and install marks the app as changed and registers it with macOS again. Fixed in 1.7.0.

## 1.6.7

### Changed
- **The menu bar icon and quitting work like other Mac apps (#129).** The menu bar shows the app's own icon. A double-click on it opens the window, a right-click (or Control-click) opens the menu, and a single click does nothing. **Command-Q** now quits the app (before, it only went back to the menu bar, and **Quit Completely** is gone); **Command-H** hides it and it keeps running.

## 1.6.6

### Changed
- **The update window explains the frequent updates and links to what is new (#127).** The "A new version is available" window says that the app is in its initial development stages, with constant updates and feature additions, and has a **See what is new** link to the new version's release page on GitHub, which lists its changes and issue numbers.

## 1.6.5

### Changed
- **The S-76's hat 2 has no pitch trim (#125).** X-Plane's pitch trim does nothing in the S-76 (tested with the stick), so hat 2 up and down are empty there; left and right are still the cyclic trim.
- **Every app icon is kept (#125).** The artwork of each icon used is in `assets/icons/`, so earlier icons are not lost when the icon changes.

### Fixed
- **The app did not find the aircraft's own X-Plane joystick settings (#125).** Symptom: with a joystick profile chosen for the aircraft in X-Plane, the app still read the main joystick settings (for example when showing which buttons X-Plane has bound). Cause: X-Plane keeps those profiles under the aircraft's name as it shows it ("Cessna Skyhawk"), but the plugin saved the aircraft's description ("Cessna 172 SP Skyhawk - 180HP"), which is other text, and is empty in some aircraft (the RV-10). Fix: the plugin saves the aircraft's name. Fixed in 1.6.5.

## 1.6.4

### Changed
- **One layout for the ready-made civil profiles (#121).** The 21 profiles of the civil aircraft that come with X-Plane 12, and `default.x52`, now put a function on the same control in every aircraft that has it, and leave a button empty where the aircraft does not:
  - **E held = wheel brakes** in every mode (on at the press, off at the release, no hold delay). The pinkie is no longer the brakes.
  - **A:** tap = gear in takeoff and landing; held = the parking brake (tested in X-Plane in every aircraft that has one: `park_brake_toggle` where the parking brake valve command could release but not set it). On fixed gear a tap does nothing.
  - **Throttle hat in takeoff and landing = exterior lights:** up landing, right taxi, down strobes, left nav. D in takeoff is the beacon. Selectors with three positions (737, MD-82, A330, Citation X) use their own commands: a tap moves them one position up, a hold one down. Landing lights made of several lamps go on or off together (in the Citation X, and the 737's runway turnoff lights, a tap turns them all on and a hold all off). The autothrottle button arms the A/T switch before engaging it (737, MD-82); the A330 uses its own A/THR and flight director buttons. Each command was tried in X-Plane 12.4 in every civil aircraft.
  - **Cruise:** T1 HDG / NAV, T2 ALT hold / VS, C approach (no longer a second ALT hold), throttle hat ALT and speed (airliners) or VS.
  - **Landing:** D held = reverse, T1 autobrake, T2 spoilers arm / retract, T3 up = TOGA on the airliners. TOGA is no longer on the trigger's second stage, which comes after the autopilot disconnect.
  - **The dials** do a job for the aircraft: the lower dial (i) the prop, speed brake, airbrakes or carb heat, the E dial the mixture (stopping at 5 %), rudder trim or carb heat; they zoom only where the aircraft has nothing else for them. Mixture, prop and carb heat left the throttle hat.
  - **The pinkie is the Shift** in every mode: with it the scroll wheel zooms; in takeoff and landing the throttle hat has the extra lights (runway turnoff, wing, recognition, logo) where the aircraft has them; in cruise C is the flight director and, in the airliners, the hat's right and left set the VS.
  - Profiles already saved, your own copies included, are not changed.

### Fixed
- **Buttons that did nothing in some aircraft (#121).** Symptom: found with the stick in X-Plane: the fuel pump buttons (T2) did nothing in the PA-18, L-5, Aerolite and R22; AP approach (C) did nothing in the Lancair Evolution and RV-10; hat 2 trim and rotor RPM (pinkie + T3) did nothing in the R22. Cause: these aircraft have no electric fuel pump, their autopilots have no approach mode, X-Plane's trim does not move the R22's cyclic, and its rotor RPM trim does nothing. Fix: those buttons are left empty in these profiles (pinkie + T3 in the R22 is the governor, as T3 is). Fixed in 1.6.4.
- **The Aerolite 103's flaps did not follow the slider (#121).** Symptom: the slider did nothing in the Aerolite. Cause: the ready-made profile said it had no flaps, but its cockpit has a flap lever with two detents. Fix: the slider sets its flaps. Fixed in 1.6.4.
- **The Aerolite 103 had carb heat buttons it does not have (#121).** Symptom: the carb heat buttons did nothing. Cause: the profile assumed carb heat; the cockpit has none (it has a mixture lever, now on the E dial). Fixed in 1.6.4.
- **The ASK 21's tow release used a different command from its cockpit (#121).** Symptom: none seen, but the cockpit's handle releases the tow and the winch. Cause: the profile used `glider_tow_release`, the cockpit `glider_all_release`. Fix: A uses the cockpit's command. Fixed in 1.6.4.
- **The 737's battery button did nothing (#121).** Symptom: T1 did not switch the battery off. Cause: the 737's battery switch is under a guard cover, and the switch does not move while the cover is shut (shutting the cover also puts it back on). Fix: battery off opens the cover first; battery on switches it on and shuts the cover. Found with the stick in X-Plane. Fixed in 1.6.4.
- **The 737's APU did not start (#121).** Symptom: T3 up did nothing. Cause: X-Plane's own APU command is not used by the 737, which has an OFF / ON / START switch. Fix: T3 up moves that switch down a notch (tap for ON, then hold for START, which springs back to ON), T3 down moves it up a notch. Fixed in 1.6.4.
- **The 737 needed two taps of A to lower the gear (#121).** Symptom: after the gear went up, a tap left the gear up. Cause: the 737's gear lever is UP / OFF / DOWN, and the toggle steps through them, so a tap from UP went to OFF. Fix: a tap puts the lever UP when it is down and DOWN otherwise. Fixed in 1.6.4.
- **The A330's APU did not start (#121).** Symptom: T3 up did nothing. Cause: the A330 does not use X-Plane's APU commands; it has MASTER SW and START push buttons. Fix: T3 down is the APU MASTER SW (on/off), T3 up is START. Fixed in 1.6.4.
- **The A330's probe heat buttons did nothing (#121).** Symptom: pinkie + T3 in landing did not change the probe heat. Cause: the A330 ignores X-Plane's pitot heat commands; its PROBE/WINDOW HEAT button is ON or AUTO. Fix: pinkie + T3 up sets the button to ON, down to AUTO. Fixed in 1.6.4.
- **The MD-82's probe heat buttons did nothing (#121).** Symptom: pinkie + T3 in landing did not turn the probe heat on. Cause: the MD-82's pitot and stall heat is a knob (OFF in the middle), and it undoes X-Plane's pitot heat commands. Fix: pinkie + T3 up and down turn the knob a notch each way. Fixed in 1.6.4.
- **The MD-82's autothrottle button did nothing (#121).** Symptom: T3 up in cruise left the A/T off. Cause: X-Plane's autothrottle commands are not used by the MD-82, whose A/T needs its AUTO THROTTLE switch on and then the IAS button. Fix: the button turns the switch on and presses IAS, or turns the switch off when the A/T is on. Fixed in 1.6.4.
- **The wing leveler did nothing with the autopilot off (#121).** Symptom: D in cruise did not level the wings in the King Air, Citation X, 737, A330 and MD-82, and only turned the flight director on in the G1000 aircraft. Cause: X-Plane's wing leveler command acts only on an engaged autopilot in those models. Fix: D uses X-Plane's return-to-level command (the SR22's and SF50's own LVL button), which engages the autopilot wings level in every civil aircraft (tested in each). Fixed in 1.6.4.
- **The Citation X's battery, avionics and pitot heat buttons did nothing (#121).** Symptom: T1, T2 and pinkie + T3 in landing did not move their switches. Cause: the Citation X keeps its own switch state and ignores X-Plane's commands for these. Fix: each button uses the cockpit switch's own toggle, only when the switch is not already where the button puts it. Fixed in 1.6.4.
- **The Citation X's APU buttons did not move its switches (#121).** Symptom: T3 started X-Plane's APU starter but left the APU MASTER switch off. Cause: the Citation X has its own APU master and starter switches. Fix: T3 up turns the master on (when off) and presses the starter; T3 down puts the starter down and the master off. Fixed in 1.6.4.
- **Reverse did not deploy in the Citation X and King Air C90B (#121).** Symptom: holding D in landing did nothing. Cause: these two models do not follow X-Plane's reverse-hold command; the reverse toggle works. Fix: D is "Reverse on/off" in these two (a tap in, a tap out). Fixed in 1.6.4.
- **The King Air C90B's flight director button did nothing (#121).** Symptom: C in takeoff did not turn the FD on, and HDG, NAV and ALT then did nothing either (they need the FD). Cause: the C90B uses its own FD switch, not X-Plane's FD command. Fix: C uses the cockpit's FD switch. Fixed in 1.6.4.

## 1.6.3

### Changed
- **New app icon (#122).** The icon in the Dock, Command+Tab and Finder is new artwork: the stick linked to an aircraft. It is kept as a picture in `assets/AppIcon.png`; `tools/make_icon.swift` now builds every icon size from it in the usual macOS rounded shape, instead of drawing the old icon in code.

## 1.6.2

### Fixed
- **The Axes tab flickered while turning the scroll wheel (#119).** Symptom: the controls list jumped as the wheel turned. Cause: the value text sat in a 90 pt column and the wheel's text ("forward  net -6") did not fit, so it wrapped and the row changed height on every notch. Fix: a wider column (150 pt) and the text is limited to one line. Fixed in 1.6.2.

## 1.6.1

### Added
- **Dataref search in tests (#117).** The dataref box of a live light, alarm or master switch searches X-Plane's list of datarefs (and the add-on aircraft's) as you type, like the buttons and screen pages do, also when X-Plane is not running.

- **Scroll wheel on the Axes tab (#117).** The scroll wheel behind the throttle is three buttons (forward, back, click), so it was missing from the Axes tab's list of controls. It now has a row there (the last notch and the net notches forward) in the app and in the X-Plane window, and a setting **Scroll wheel: zoom steps per notch** (1 to 24, default 1; profile line `wheel_zoom_steps`), like the zoom strength of the dials. It applies when a wheel notch runs X-Plane's zoom in or zoom out (mode 1 in the ready-made profiles); any other job of the wheel is unchanged.

### Changed
- **Updates are offered for every release (#117).** The update check compared only the first two numbers of the version, so 1.6.0 to 1.6.1 was never offered. It now compares all three: any newer release is an update. The manual and the project rules say so.

### Fixed
- **A new alarm could show an empty Colour picker (#117).** Symptom: after adding a blank alarm the Colour choice was empty. Cause: since levels (1.6.0) a new alarm went on the first light with room, the Fire button, which shows only off or on, while the alarm's colour was red. Fix: new alarms start on Hat 2 as before (then the next light with room), and an alarm moved to Fire or Throttle keeps a colour of off or on. Fixed in 1.6.1.

## 1.6.0

### Added
- **Three-state live lights and alarms (#115).** A light can have up to 3 levels, for live lights (rules) and for alarms. The first level whose test is true decides the colour, and the last live level also says what to show when none is true: for example Button A green with the gear down, amber while it moves, off when it is up. Alarms work the same way, in priority order, so amber, then red, then red flashing is three alarms on one light. A live level has **Add level** in the app and in the X-Plane window. A rule, an alarm or a master switch can now have up to three tests joined with AND, or all with OR (they cannot be mixed). Alarms still beat live lights, and when an alarm stops the light goes back to its live light or mode colour. Old profiles are read and written as before (the file lines are unchanged; a third test is written as one more `and` / `or` test at the end of the line), so no profile changes. The app refuses a fourth level on a light; a hand-edited file with more still works.

## 1.5.0

### Added
- **Export Current Template (#111).** The menu next to the profile picker has **Export Current Template...**: it fills a Word template with the profile in use and saves a `.docx` (for a PDF, use Save as PDF in Word). The built-in template, `templates/X52-Manual-Template.docx`, has the layout of the standard aircraft maps: the joystick and throttle pictures, the fixed assignment tables (Pinkie held too) and the display and lights dashboard with the screen pages, mode lights, live lights and alarms. The live lights and the alarms, which can grow, are on a page of their own after the dashboard. The lights are shown with 21 small icons (steady, flashing, two colours in order, the mode's own colour, off) that are bundled with the app (`templates/light-icons/`). **Choose Custom Template...** uses your own. A template is a Word file whose spots are content controls with tags (`docs/manual-data.md`), so the layout can change without an app update. Rows with nothing bound can be left out. `bin/x52manual profile.x52 template.docx out.docx` does the same from the command line. **Save a Built-In Template Blank Copy...** gives a starting point, and `docs/custom-manual-template.md` explains how to make your own template in Word.

## 1.4.3

### Fixed
- **F-14 Combat + Pinkie disabled controls other than H2** (#112). Symptom: holding Pinkie in Combat left only the trims working; the weapons, targets, flares, chaff, wing sweep and A tap/hold stopped. Cause: the shifted layer contained only the trims, and a shifted layer falls back to Any mode, not to the unshifted mode, so Combat-only bindings disappeared. Fix: the ready-made profile now copies the Combat press and hold bindings into the shifted layer before overriding H2 with trim. Other aircraft and personal profiles are unchanged. Fixed in 1.4.3.

## 1.4.2

### Fixed
- **The window jumped when the stick's mode or the pinkie changed.** The status line beside the Plugin on/off button
  ("X-Plane is running and its plugin is driving the stick (Mode 3)" or "(Pinkie + 3)") had no line limit, so it
  sometimes wrapped to two lines, the top bar grew and every tab moved. The line is now one line with a shorter
  wording ("X-Plane is driving the stick (Pinkie + 3)"), shortened in the middle in a very narrow window, with the
  full text as the tooltip. Fixed in 1.4.2. (issue #103)

### Changed
- **The Buttons tab follows the stick's mode and pinkie.** A press on the stick already picked that button; it now
  also picks the layer the press happened in (Mode 1 to 3, or Pinkie + 1 to 3), also when Any mode was shown. The
  plugin records the mode and the pinkie with the press, so a quick pinkie tap is not missed. (issue #103)

## 1.4.1

### Fixed
- **With the plugin driving the throttle, X-Plane's autothrottle could not hold a speed.** Symptom: with *The plugin
  drives the throttle* on, the speed wandered after engaging the autothrottle, and with it the pitch (the aircraft
  slowly climbed and dived, #76). Cause: the plugin wrote the engine power and the cockpit levers from the X52 lever
  every frame, with X-Plane's `override_throttles` set, so whatever the autothrottle set was put back on the next
  frame. Fix: while X-Plane's autothrottle is really working (`autothrottle_on`), the plugin leaves the throttle to
  it. When the autothrottle is switched off, the lever takes over again once it comes within 2 % of the power the
  autothrottle left, or is moved past it, so the power does not jump to wherever the lever was; until then the X-Plane
  window's Axes tab says where to move it. Add-on aircraft with an autothrottle of their own (not X-Plane's) are not
  covered. Fixed in 1.4.1 (issue #101).
- **Unticking *The plugin drives the throttle* left the throttle frozen while the plugin kept the stick.** Symptom:
  with both boxes ticked, unticking only the throttle one left the engine power where it was, and neither the X52
  nor X-Plane's own throttle axis moved it until the stick box was unticked too. Cause: the plugin only gave
  `override_throttles` back to X-Plane when it gave up every axis. Fix: the throttle is handed back on its own as soon
  as the plugin stops driving it. Fixed in 1.4.1 (issue #101).

## 1.4.0

### Added
- **A button can have an action when it is let go.** Besides "when pressed" and "when held", a binding can now have
  "when released", run after whatever the press or hold started has ended. The release runs from the binding found at
  the press, so turning the mode dial while the button is held does not change it. In the profile file:
  `bind <button> <layer> release "" step ; step ...`. Both button editors (the app and the in-sim window) show and edit
  it. Profiles without it load and behave as before. (issue #105)
- **Two new dataref steps, Save and Restore.** `save:<dataref>` remembers the dataref's value and `restore:<dataref>`
  writes it back (nothing happens when nothing was remembered). `restoreif:<dataref>=<from>,<to>,<command>` runs a command when the remembered value was in that range, for settings only a command can put back. Used with "when released" a button can change a
  setting while it is held and put it back afterwards. (issue #105)

### Changed
- **F-14: the trigger selects the Gun while held, and Fire fires the selected weapon.** Trigger stage 1 remembers
  the selected weapon (`sim/cockpit2/weapons/weapon_select_console_index`) and selects the Gun; stage 2 fires the
  guns; letting go of stage 1 selects the remembered weapon again with its own select command (tested in the sim: writing the index back left the guns/missiles armed flags wrong), so you always know what the aircraft would fire. Fire (button 2)
  runs the F-14's own trigger command (`laminar/f14/wpns/fwd_weapon_trigger`), which fires the selected weapon for as
  long as it is held. The profile version is now 11: an older F-14 profile whose trigger is still as the ready-made one
  had it gets the new trigger and Fire (when Fire is free); anything you changed stays. (issue #105) If the remembered weapon has nothing left (the F-14 refuses to select an empty weapon), the selection stays on the Gun, which is what Fire would fire; hat 2 picks another weapon.

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
