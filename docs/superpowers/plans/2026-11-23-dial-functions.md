# 1.3: dial functions (speed brake in steps, zoom) and a plugin on/off switch — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Each profile picks what the throttle's two dials do (lower "i" dial by the clutch, upper "E" dial): nothing,
the speed brake in four steps (Armed / Retracted / Half / Full), or the view zoom. The plugin drives them with the dial's
flicker filtered out, so X-Plane no longer has to read the dials itself. Then (Task 5) a switch in the Mac app turns
the plugin off completely and back on, shown left of the "Stick connected" status with a green (on) or red (off) light.

**Architecture:** Pure C++ in `src/core/axes.{h,cpp}` (the dial setting, the speed-brake step, the zoom clicks),
saved in the profile (`src/core/profile.cpp`), applied each frame in the plugin next to `App::applySliderFlaps`
(`src/plugin/plugin.cpp`), editable in the plugin window (`src/plugin/ui.cpp`) and the Mac app (`src/app/bridge.cpp`,
`src/app/main.swift`). The plugin reads the dial's *steady* value (`steadyValue`, already applied in `App::trackAxes`),
so a one-count flicker never reaches X-Plane.

**Tech Stack:** C++17 (clang++, arm64), X-Plane SDK, SwiftUI built by `make` (command line tools only, no Xcode).
Tests: `tests/check.h` (`TEST`, `CHECK`, `CHECK_EQ`), core in `tests/test_core.cpp`, plugin against the fake sim in
`tests/test_plugin.cpp`; `make` builds everything and runs them.

**Spec:** this plan's Design section (agreed with the user in the session of 2026-11-23).

## Design

- **Speed brake in steps.** The dial's travel is cut into four equal zones; each sets
  `sim/cockpit2/controls/speedbrake_ratio` (−0.5 armed, 0 retracted, 0.5 half, 1.0 full; X-Plane's own meaning of
  this dataref). It is written only when the dial enters another zone and only after the dial has moved since the
  aircraft loaded, the same rule as the slider's flaps. So speed-brake buttons, the auto-spoilers at touchdown and the
  mouse keep working in between.
- **Zoom.** Relative, not absolute: every 1/24 of the dial's travel fires one `sim/general/zoom_in` or
  `sim/general/zoom_out`. Turning back the same amount undoes it, and the mouse wheel and the zoom keys keep working.
  The plugin never writes `sim/graphics/view/field_of_view_deg` (that is the user's field-of-view setting, saved by
  X-Plane).
- **Per profile, user's choice.** Each dial has a function (`none`, `speed brake`, `zoom`) and an Invert switch, saved
  in the profile. The ready-made profiles get speed brake on the lower dial in aircraft that have speed brakes and zoom
  on the E dial everywhere; the user can change either per profile in the app or the plugin window.
- **X-Plane must not drive the same thing.** If any axis in X-Plane's Joystick settings is assigned to speed brakes
  (assignment 14) or view zoom (assignment 43), the plugin leaves that function alone and the settings say so ("X-Plane
  also uses a dial for speed brakes: set it to None in X-Plane's Joystick settings"). The values 14 and 43 come from
  the user's own X-Plane files (lower dial → 14, E dial → 43 in the "A330" profile); check them live in Task 7.
- **Plugin on/off (asked by the user for 1.3, built after the profiles).** A switch in the Mac app's status row, to the
  left of the stick's light and "Stick connected" text. On: green light, "Plugin on". Off: red light, "Plugin off", and
  the status reads "Stick connected, plugin off". Off means the plugin does nothing at all: no buttons, no axes, no
  dials, no lights or screen, and X-Plane's own Joystick settings are fully in charge. The setting is global (not per
  profile) and survives restarts of the app and of X-Plane. The app and the plugin share it the way they already share
  the profiles and the lock: a file `plugin-off` in the X52Pro folder (present = off), which the plugin checks with its
  once-a-second file checks (`src/plugin/plugin.cpp` ~line 335).

## Global Constraints

- Profile lines (written by `serialize`, after `steady_slider`):
  ```
  # rotary_<lower|upper> = what the dial does: 0 nothing, 1 speed brake (armed, retracted, half, full), 2 zoom. rotary_<lower|upper>_invert = 1 turns it the other way
  rotary_lower = 1
  rotary_lower_invert = 0
  rotary_upper = 2
  rotary_upper_invert = 0
  ```
  Do not use a `dial_` prefix: `dial_accel` / `dial_*_ms` / `dial_factor` already belong to the right-hand scroll wheel.
- `Profile::version` and the header written by `serialize` go from 6 to 7. A file below 7 has no `rotary_` lines and
  gets the defaults on reading: lower = speed brake, upper = zoom, neither inverted. A line in the file always wins.
  The ready-made profiles keep their `x52pro-profile 3` header.
- Speed-brake zones, on the dial position x (0..1, after Invert): x < 0.25 armed, < 0.5 retracted, < 0.75 half, else full.
- Zoom: 24 clicks over the full travel (`kZoomClicksPerTravel = 24`); clockwise (raw value going up) zooms in unless
  inverted.
- X-Plane axis assignments: `kXpAxisSpeedbrakes = 14`, `kXpAxisViewZoom = 43`, read from
  `sim/joystick/joystick_axis_assignments` (int[500]) at most once a second.
- Version: **1.3.0** (decided by the user). Every "VERSION" below means 1.3.0; the app's update check will tell users
  about it.
- Work on branch `feat/dial-functions` off `main`, with its GitHub issue opened first (CLAUDE.md "Issues"). Run `make`
  before every commit; never commit with a failing build or test.

## Review Focus

1. **Dial resting exactly on a zone border, flickering one count:** the speed brake must not flip between two steps.
   `steadyValue` (2 %) is what prevents it; test in Task 2 (a 1-count flicker at the border writes once).
2. **Speed brake changed by a button or by the auto-spoilers after the dial set it:** the plugin must not put it back
   until the dial enters another zone. Test in Task 2.
3. **Aircraft loaded with the dial already turned:** nothing may be written or zoomed until the dial moves. Test in Task 2.
4. **X-Plane still has a dial assigned to speed brakes or zoom:** the plugin must not fight it (no writes, no zoom
   clicks) and must say why in the plugin window. Test in Task 2.
5. **Switching the profile (or the dial's function) while the dial is mid-travel:** zoom must not jump by the whole
   distance turned under the old setting. Test in Task 2 (changing the function re-anchors the zoom).

---

### Task 0: Issue and branch

- [ ] **Step 1:** Open the GitHub issue "1.3: dials for speed brake and zoom, per profile; plugin on/off switch
  (1.3.0)": symptom (a dial assigned in X-Plane flickers by one count, so zoom shivered; the clutch dial gives a
  continuous speed brake with no Armed), what changes for the dials, and the on/off switch in the app's status row.
  No personal data or local paths.
- [ ] **Step 2:** `git switch -c feat/dial-functions` from an up-to-date `main`.

### Task 1: Dial settings in the profile (core)

**Files:**
- Modify: `src/core/axes.h`, `src/core/axes.cpp`, `src/core/profile.h:236`, `src/core/profile.cpp` (serialize ~line 552,
  parse ~line 990, header ~line 515)
- Test: `tests/test_core.cpp`

**Interfaces:**
- Produces, in `axes.h`:
  ```cpp
  enum class DialUse { None = 0, SpeedBrake = 1, Zoom = 2 };
  // in AxisSettings:
  DialUse lowerUse = DialUse::SpeedBrake, upperUse = DialUse::Zoom;   // invert: lowerRotary.invert / upperRotary.invert
  DialUse* dialUse(AxisSettings& s, int usage);   // 0x33 lower, 0x34 upper; nullptr otherwise
  ```
- Profile keys as in Global Constraints; `Profile::version` default 7.

- [ ] **Step 1: Write the failing tests** in `tests/test_core.cpp`:
  - `TEST(dial_functions_round_trip_through_the_profile)`: set lower = Zoom, upper = None, `lowerRotary.invert = true`;
    `serialize` then `parse`; `CHECK(q.axes.lowerUse == DialUse::Zoom)`, `CHECK(q.axes.upperUse == DialUse::None)`,
    `CHECK(q.axes.lowerRotary.invert)`, `CHECK(!q.axes.upperRotary.invert)`; the text contains `"rotary_lower = 2\n"`
    and `"x52pro-profile 7\n"`.
  - `TEST(an_older_profile_gets_speed_brake_and_zoom_on_the_dials)`: parse a `x52pro-profile 6` text with no `rotary_`
    lines → lower SpeedBrake, upper Zoom.
  - `TEST(a_dial_set_to_nothing_stays_nothing_in_an_older_file)`: `x52pro-profile 3` text with `rotary_lower = 0` →
    lower None.
  - `TEST(an_unknown_dial_function_reads_as_nothing)`: `rotary_upper = 9` → None.
- [ ] **Step 2:** `make test` → the new tests fail (no `DialUse`).
- [ ] **Step 3:** Implement: the enum and fields; `dialUse`; parse `rotary_lower`, `rotary_upper` (values outside
  0..2 → None) and `rotary_lower_invert`, `rotary_upper_invert` (into `lowerRotary.invert` / `upperRotary.invert`);
  serialize the five lines; version 7 with the header comment in `profile.h` extended ("below 7 the dials got speed
  brake and zoom").
- [ ] **Step 4:** `make test` → all pass.
- [ ] **Step 5:** `make`, then commit "Profiles say what each throttle dial does (VERSION, #75)".

### Task 2: The plugin drives the dials

**Files:**
- Modify: `src/core/axes.h`, `src/core/axes.cpp` (pure helpers), `src/plugin/app.h` (state next to `flapWritten`),
  `src/plugin/plugin.cpp` (new `App::applyDials()` next to `applySliderFlaps`, called right after it at ~line 355;
  reset its state where `flapWritten = -1` at ~line 834)
- Test: `tests/test_core.cpp`, `tests/test_plugin.cpp`

**Interfaces:**
- Consumes: `DialUse`, `dialUse`, `AxisSettings::lowerUse/upperUse` (Task 1); `App::axisLive` (steady values).
- Produces, in `axes.h`:
  ```cpp
  constexpr int kZoomClicksPerTravel = 24;
  constexpr int kXpAxisSpeedbrakes = 14, kXpAxisViewZoom = 43;
  // Speed-brake handle for a dial position 0..1 (after Invert): -0.5, 0, 0.5 or 1.0 (zones in Global Constraints).
  double speedBrakeFor(double x);
  // Whole zoom clicks between `anchor` and `x` (both 0..1): positive = zoom in. Moves `anchor` by the clicks returned,
  // so the remainder carries over to the next call.
  int zoomClicks(double& anchor, double x, int clicksPerTravel = kZoomClicksPerTravel);
  ```
- Produces, in `App`: `void applyDials(); double speedBrakeWritten = -9; std::map<int, double> zoomAnchor;`
  (keyed by usage; empty / -9 = not since the aircraft loaded or the setting changed) and
  `bool xplaneDrives(int assignment)` (cached once a second from `sim/joystick/joystick_axis_assignments`).

- [ ] **Step 1: Write the failing core tests:**
  - `TEST(speed_brake_steps)`: `speedBrakeFor(0.0) == -0.5`, `(0.3) == 0.0`, `(0.6) == 0.5`, `(0.99) == 1.0`,
    `(0.25) == 0.0`.
  - `TEST(zoom_clicks_carry_the_remainder)`: anchor 0.5; x = 0.5 + 1.5/24 → 1, anchor = 0.5 + 1/24; x back to 0.5 →
    −1; x = 0.5 + 0.4/24 → 0.
- [ ] **Step 2: Write the failing plugin tests** in `tests/test_plugin.cpp`, in the style of
  `the_throttle_moves_the_cockpit_levers_too_and_the_slider_sets_the_flaps_at_their_detents` (push `{0x01, 0x33, v, 0, 255}`,
  `sim().frame()`, read `sim().datarefs`, count `sim().onces`; add `sim/general/zoom_in` / `zoom_out` to
  `sim().knownCommands` and `sim/joystick/joystick_axis_assignments` to the fake if it is missing):
  - `TEST(the_lower_dial_sets_the_speed_brake_in_steps_and_leaves_it_alone_in_between)`: first push (rest at 20) writes
    nothing; 100 → 0.0; 140 → 0.5; set the dataref to 0.8 by hand, frame → stays 0.8; 250 → 1.0; 10 → −0.5.
  - `TEST(a_dial_flickering_on_a_zone_border_sets_the_speed_brake_once)`: move to 127, then alternate 127/128 ten frames →
    one write (track with a value written by hand in between, as above).
  - `TEST(the_e_dial_zooms_one_click_per_step_and_back)`: rest 128; 128 + 3 × 255/24 → 3 `zoom_in` onces; back to 128 →
    3 `zoom_out`.
  - `TEST(a_dial_x_plane_also_drives_is_left_alone_and_the_window_says_so)`: assignments contain 14 → turning the lower
    dial writes nothing; the plugin window's Axes view `sees("set it to None in X-Plane")`.
  - `TEST(changing_a_dial_function_does_not_zoom_by_the_distance_already_turned)`: upper = None, turn 128 → 250;
    set upper = Zoom (as the window does), frame, then 250 → 260 → at most 1 click.
- [ ] **Step 3:** `make test` → the new tests fail.
- [ ] **Step 4:** Implement `speedBrakeFor`, `zoomClicks` and `App::applyDials`: for each of 0x33 / 0x34 with a
  function, skip if `changedAt <= 0` or `xplaneDrives(...)`; x = `normalizeAxis(steady value)`, `1 - x` when inverted;
  SpeedBrake → write `speedbrake_ratio` only when `speedBrakeFor(x)` differs from `speedBrakeWritten`; Zoom → first call
  sets the anchor, then `commandOnce` the clicks. Changing a dial's function (window or app) clears its anchor and
  `speedBrakeWritten`.
- [ ] **Step 5:** `make test` → all pass.
- [ ] **Step 6:** `make`, then commit "The plugin drives the throttle dials: speed brake in steps, zoom (VERSION, #75)".

### Task 3: Settings in the plugin window and the Mac app

**Files:**
- Modify: `src/plugin/ui.cpp:1340-1371` (dials loop), `src/app/bridge.cpp` (`number()` ~line 60, `flag()` ~line 75),
  `src/app/bridge.h:200` (key list comment), `src/app/main.swift` (`DialSteadyRow` ~line 1990, help text ~line 2050)
- Test: `tests/test_plugin.cpp`, `tests/test_agent.cpp` (bridge keys, if bridge keys are tested there; otherwise
  `test_plugin.cpp`)

**Interfaces:**
- Consumes: `DialUse`, `dialUse` (Task 1); `App::xplaneDrives`, the reset of `zoomAnchor` / `speedBrakeWritten` (Task 2).
- Bridge keys: `rotary_lower`, `rotary_upper` (int 0..2), `rotary_lower_invert`, `rotary_upper_invert` (flags).

- [ ] **Step 1: Write the failing tests:**
  - `TEST(the_window_cycles_a_dials_function_and_saves_it)`: in the Axes view, click the lower dial's function button
    ("Speed brake") → it reads "Zoom", then "Nothing", then "Speed brake"; the saved profile file contains
    `rotary_lower = 2` after the first click.
  - A bridge test: `x52_set("rotary_upper", 0)` → profile `upperUse == None`; `x52_set("rotary_upper", 7)` is clamped
    to 2; `rotary_lower_invert` is a flag.
- [ ] **Step 2:** `make test` → fail.
- [ ] **Step 3:** Implement:
  - Plugin window, for 0x33 / 0x34: a button cycling "Speed brake" → "Zoom" → "Nothing", and "[x] Invert". When
    `xplaneDrives` is true for the chosen function, an amber line under the row: "X-Plane also uses a dial for
    <speed brakes|zoom>: set it to None in X-Plane's Joystick settings."
  - Bridge: the four keys.
  - App: in `DialSteadyRow` for lower/upper, a `Picker` (Nothing / Speed brake / Zoom) and an "Invert" `Toggle`. Under
    the rows, one help text: "The lower dial (i) and the E dial can set the speed brake in four steps (Armed,
    Retracted, Half, Full) or zoom the view. The plugin only acts while you turn the dial, so buttons, the mouse and the
    auto-spoilers still work. Set the dial to None in X-Plane's Joystick settings if X-Plane uses it too." Rename the
    rows' titles to "Lower dial (i)" / "E dial" only if `controlName` in `src/core/mfd.cpp:163-164` changes with them
    (keep one name everywhere).
- [ ] **Step 4:** `make test` → pass.
- [ ] **Step 5:** `make`, then commit "Pick each dial's function in the plugin window and the app (VERSION, #75)".

### Task 4: Ready-made profiles and every profile on disk

**Files:**
- Modify: `profiles/tools/aircraft.py` (`axes(f)`, ~line 393), regenerated `profiles/aircraft-*.x52`, the hand-made
  `profiles/default.x52`, `profiles/A330_new.x52`, `profiles/aircraft-Cessna_CitationX_mine.x52`,
  `my_profiles/out/profiles/*.x52`, and the user's X-Plane `Output/preferences/X52Pro/*.x52` (back that folder up first).

- [ ] **Step 1:** In `axes(f)` add `"rotary_lower = %d" % (1 if f["speedbrake"] else 0)`, `"rotary_lower_invert = 0"`,
  `"rotary_upper = 2"`, `"rotary_upper_invert = 0"`.
- [ ] **Step 2:** `python3 profiles/tools/build_profiles.py`; check the headers are still `x52pro-profile 3` and that
  the Citation X, 737, A330, MD-82, F-4 and F-14 have `rotary_lower = 1`, and every other profile has 0.
- [ ] **Step 3:** Hand-made, `my_profiles/out/profiles/` and the user's X-Plane profiles: add only the four lines,
  after `steady_slider` (speed brake where the aircraft has speed brakes, otherwise 0; zoom on the E dial). Leave
  `.deleted` files alone.
- [ ] **Step 4:** `make` (the profile-reading tests cover the regenerated files), then commit
  "Ready-made and existing profiles: speed brake and zoom on the dials (VERSION, #75)".

### Task 5: Plugin on/off switch

**Files:**
- Modify: `src/plugin/store.h`, `src/plugin/store.cpp` (read/write the `plugin-off` file, next to the lock),
  `src/plugin/app.h`, `src/plugin/plugin.cpp` (the flight loop's once-a-second checks ~line 335; disable/enable at
  ~lines 1101 and 1119), `src/app/agent.h`, `src/app/agent.cpp` (~line 489 checks), `src/app/bridge.h`,
  `src/app/bridge.cpp`, `src/app/main.swift` (`Model` ~line 30 and 130, `statusText` ~line 344, status row ~line 2581)
- Test: `tests/test_plugin.cpp`, `tests/test_agent.cpp`

**Interfaces:**
- Produces, in `store.h` (namespace `x52`):
  ```cpp
  bool pluginOff(const std::string& dir);              // true when <dir>/plugin-off exists
  bool setPluginOff(const std::string& dir, bool off); // creates / removes it; false if that failed
  ```
- `App`: `bool off = false;` and `void setOff(bool off);` (turning off does what disabling the plugin does today:
  `releaseAxes()`, all held buttons released, `lock.release()` so the menu bar app may drive the stick's lights and
  screen; turning on takes the lock again and reloads the profile).
- Bridge: `int x52_plugin_enabled(void);` and `void x52_set_plugin_enabled(int on);`.
- Swift `Model`: `@Published var pluginEnabled = true`.

- [ ] **Step 1: Write the failing tests:**
  - `TEST(the_plugin_switched_off_leaves_everything_to_x_plane)` (plugin, fake sim): with `axis_override = 1` and a
    bound button; create `plugin-off` in the store folder; frames for 1.1 s → `override_joystick == 0`, pressing the
    bound button runs no command, turning a dial writes nothing.
  - `TEST(the_plugin_switched_back_on_takes_over_again)`: remove the file; frames for 1.1 s → the bound button runs its
    command again and `override_joystick == 1` with the override on.
  - `TEST(the_switch_survives_a_restart)`: off, stop and start the plugin (as the existing restart tests do) → still off.
  - Agent test: `setPluginOff(dir, true)` → `pluginOff(dir)`; with the plugin off the agent drives the stick even if a
    plugin once ran (no lock held).
- [ ] **Step 2:** `make test` → fail.
- [ ] **Step 3:** Implement the store functions, `App::setOff` and its once-a-second check (everything the flight loop
  does is skipped while `off`, except that check), the agent's use of it, and the two bridge functions.
- [ ] **Step 4: Mac app status row**, left of the stick's light: a button with its own light, green circle + "Plugin
  on" or red circle + "Plugin off", clicking it flips `x52_set_plugin_enabled`. `statusText` when off and connected:
  "Stick connected, plugin off" (when not connected: "Stick not connected, plugin off"). Help text on hover: "Off: the
  plugin does nothing in X-Plane, and X-Plane's own Joystick settings are in charge. Takes effect within a second."
- [ ] **Step 5:** `make test` → pass. `make`, then commit "Switch the plugin off and on from the app (1.3.0, #75)".

### Task 6: Documents, button maps, version

**Files:**
- Modify: `README.md`, `profiles/README.md`, `my_profiles/README.md`, `docs/manual/build_manual.js` (rebuild with
  `node`), in-app help text, `profiles/tools/build_sheets.py`, `tools/patch_maps.py` `RULES` (maps in `docs/mapping/`
  and `my_profiles/out/mapping/`, backed up first), `CHANGELOG.md`, `VERSION`.

- [ ] **Step 1:** Docs and manual: what each dial does, the four speed-brake steps, zoom by steps, Invert, and "set the
  dial to None in X-Plane's Joystick settings"; the plugin on/off switch, its lights and what "off" means (with a new
  picture of the status row in the manual).
- [ ] **Step 2:** Button maps: add a `RULES` entry for the dials' text; run `tools/patch_maps.py` in its virtual
  environment; render one page of the Citation X map and one of the Cessna 172 and look at them. Mirror the same
  words in `build_sheets.py`.
- [ ] **Step 3:** `VERSION` → 1.3.0; `CHANGELOG.md` "Added" entries for the dials and the on/off switch (and a "Fixed" entry for the flicker: symptom
  "the view shivered / the speed brake flickered with a dial assigned in X-Plane", cause "the dial's reading flickers by
  one count and X-Plane passes it on", fix "the plugin drives the dials from a steady reading").
- [ ] **Step 4:** `make`, `git diff --stat`, check every fix has its changelog entry, commit
  "Docs, maps and VERSION for the dial functions (#75)".

### Task 7: Try it in X-Plane

- [ ] **Step 1:** `make install`; tell the user to restart X-Plane and to set the lower dial (Rx) and the E dial (Ry)
  to None in X-Plane's Joystick settings for the profiles they fly.
- [ ] **Step 2:** With the web API (`http://localhost:8086/api/v2`), read `sim/joystick/joystick_axis_assignments`
  before and after the user sets a dial to speed brakes and to view zoom in X-Plane, to confirm 14 and 43.
- [ ] **Step 3:** With the user turning the dials in the Citation X: `speedbrake_ratio` goes −0.5 / 0 / 0.5 / 1 and
  stays put with the dial at rest; the direction feels right (more brake the way the user expects; if not, flip the
  default Invert in the generator and the profiles); one zoom click is a sensible step (if not, change
  `kZoomClicksPerTravel` and its test). Ask the user to confirm the view no longer shivers.
- [ ] **Step 3b:** Plugin switch: with X-Plane running, switch it off in the app → within a second
  `sim/operation/override/override_joystick` reads 0, the stick's buttons do only what X-Plane's own settings say, the
  app shows the red light and "Stick connected, plugin off"; switch it on → green light, the plugin takes over again.
- [ ] **Step 4:** PR "Closes #75", merge, delete the branch, tag `vVERSION`, `make release`, GitHub release
  "X52 Pro VERSION" with the zip (CLAUDE.md "Releases").
