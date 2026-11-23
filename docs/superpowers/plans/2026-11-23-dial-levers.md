# 1.3.1: more jobs for the throttle dials (levers within the game's limits) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Besides Speed brake, Zoom and Nothing, each throttle dial can be a smooth lever for Prop, Mixture, Rudder
trim, Aileron trim, Cowl flaps or Carb heat. A full turn covers the game's range for that job (the
aircraft's own limits where X-Plane has them), optionally narrowed per dial, and the dial never makes a value jump.
No dial dims anything: turning the Screen brightness on the stick's Brightness page dims the aircraft's panel too.

**Architecture:** One general "lever" job. A table in `src/core/axes.{h,cpp}` gives each lever its X-Plane setting,
its range and where its aircraft limits come from; pure functions map the dial onto that range and decide the soft
takeover. The plugin (`App::applyDials` in `src/plugin/plugin.cpp`) reads the limits and the current value from
X-Plane and writes the lever. Profiles, the plugin window and the Mac app gain the new jobs and a From/To range per dial.

**Tech Stack:** C++17, X-Plane SDK, SwiftUI built by `make`. Tests: `tests/check.h`, `tests/test_core.cpp`,
`tests/test_plugin.cpp` (fake sim), `tests/test_agent.cpp`; `make` builds and runs them.

**Spec:** this plan's Design section, agreed with the user on 2026-11-23 after 1.3.0 (proposal of jobs, limits and
soft takeover; the user picked all four groups and version 1.3.1).

## Design

**Jobs** (new `DialUse` values; 0..2 stay as in 1.3.0):

| # | Job | X-Plane setting written | Game's range | Aircraft limit read on load |
|---|---|---|---|---|
| 3 | Prop | `sim/cockpit2/engine/actuators/prop_ratio_all` | 0..1 | none (X-Plane maps 0..1 onto the governor range) |
| 4 | Mixture | `sim/cockpit2/engine/actuators/mixture_ratio_all` | 0..1 | none |
| 5 | Rudder trim | `sim/cockpit2/controls/rudder_trim` | −1..1 | −`acf_min_trim_rudd` .. +`acf_max_trim_rudd` (`sim/aircraft/controls/`) |
| 6 | Aileron trim | `sim/cockpit2/controls/aileron_trim` | −1..1 | −`acf_min_trim_ailn` .. +`acf_max_trim_ailn` |
| 7 | Cowl flaps | `sim/cockpit2/engine/actuators/cowl_flap_ratio[0..n-1]` | 0..1 | n = `sim/aircraft/engine/acf_num_engines` |
| 8 | Carb heat | `sim/cockpit2/engine/actuators/carb_heat_ratio[0..n-1]` | 0..1 | n as above |

- **Range per dial.** `rotary_<lower|upper>_range = <from> <to>`, percent of the job's range after the aircraft's
  limits, default `0 100`. A full turn of the dial covers From..To; From > To is allowed and reverses the direction.
  Not used by Speed brake (steps) or Zoom (strength).
- **Mapping.** Dial position x (0..1 after Invert, from the steady value) → `lo + (hi − lo) × (from + (to − from) × x)/100`
  where lo/hi is the job's range after the aircraft limits.
- **Soft takeover.** After an aircraft or profile change, or whenever X-Plane's value differs from what the plugin
  last wrote by more than 1 % of the range (a button, the mouse or the autopilot moved it), the lever is *waiting*:
  nothing is written until the dial's mapped value reaches X-Plane's value (crosses it, or comes within 1 % of the
  range). Then the lever is *held* and every change of the dial's steady value is written. Arrays are compared on [0].
- **X-Plane doing the same job.** If any X-Plane axis is assigned to the same job, the plugin leaves the lever alone
  and the window says so, as for speed brake and zoom. X-Plane's assignment numbers for these jobs are not known yet:
  Task 6 reads them live (`sim/joystick/joystick_axis_assignments`) and Task 2 keeps them in one table.
- **Panel brightness follows the stick's screen** (user's choice, 2026-11-23: not a dial job). When the Screen value
  changes on the MFD Brightness page (the wheel), the plugin also writes
  `sim/cockpit2/switches/instrument_brightness_ratio[0]` = Screen percent / 100 (10 %..100 %, as the screen). Only
  that page does it: the window's and the app's Screen sliders, a Screen linked to a dataref, and profile loads leave
  the panel alone, so the cockpit's own knob is never fought.
- **The window says when a lever waits**: under the dial's row, "Turn the dial to <value> to take over" (dim), the
  value in the job's own terms (percent for ratios, −/+ percent for trims).

## Global Constraints

- Version **1.3.1** (user's choice: no update prompt). `Profile::version` and the written header go 7 → 8; a file
  below 8 has no `_range` lines and gets `0 100`. Ready-made profiles keep `x52pro-profile 3`.
- Profile lines, after `rotary_upper_invert`: `rotary_lower_range = 0 100`, `rotary_upper_range = 0 100`. Comment
  line for `rotary_<lower|upper>` lists all jobs 0..8. Unknown job numbers read as Nothing (as in 1.3.0).
- Ready-made profiles keep their 1.3.0 jobs (speed brake / zoom). No job changes on disk; only the two `_range` lines
  are added (CLAUDE.md "Feature checklist": edit only the lines the change adds).
- Branch `feat/dial-levers` off `main`, its own GitHub issue first. `make` before every commit.

## Review Focus

1. **Aircraft with no trim limits set** (`acf_max_trim_rudd` = 0): the range collapses to 0..0; the lever must do
   nothing rather than divide by zero, and the window should say "this aircraft has no rudder trim". Test in Task 1.
2. **Single-engine vs twin for array jobs**: `acf_num_engines` = 0 or missing must still write engine 0. Test in Task 2.
3. **Autopilot or buttons moving trim while the dial is held**: the lever must drop to *waiting*, not fight. Test in Task 2.
4. **From = To**: every dial position maps to one value; takeover must still work (the dial reaches it at once). Test in Task 1.
5. **Switching a dial from a lever to Zoom or Speed brake mid-turn**: no write of the old job afterwards. Test in Task 2.

---

### Task 0: Issue and branch
- [ ] Open the GitHub issue "1.3.1: more dial jobs (prop, mixture, trims, cowl flaps, carb heat, panel brightness)
  within the game's limits" with the Design summary; no personal data or local paths.
- [ ] `git switch -c feat/dial-levers` from an up-to-date `main`.

### Task 1: Lever jobs in core (pure)

**Files:** Modify `src/core/axes.h`, `src/core/axes.cpp`, `src/core/profile.h`, `src/core/profile.cpp`; Test `tests/test_core.cpp`.

**Interfaces (produces, in `axes.h`):**
```cpp
enum class DialUse { None = 0, SpeedBrake = 1, Zoom = 2, Prop = 3, Mixture = 4, RudderTrim = 5, AileronTrim = 6,
                     CowlFlaps = 7, CarbHeat = 8 };
constexpr int kDialUseCount = 9;
struct LeverJob {                 // one smooth dial job
    DialUse use; const char* name;         // "Prop", "Rudder trim", ... (window and app labels)
    const char* dataref; bool perEngine;   // perEngine: write [0..n-1]; otherwise one value (index 0 for arrays)
    bool isArray; double lo, hi;           // the game's range
    const char* limLow; const char* limHigh;   // aircraft limits (nullptr: none): range = -limLow .. +limHigh
    int xpAxis;                            // X-Plane's own assignment number for the job (0 until Task 6 confirms)
};
const LeverJob* leverJob(DialUse use);    // nullptr for None, SpeedBrake, Zoom
const char* dialUseName(DialUse use);     // every job, "Nothing" for None
// in AxisSettings: int lowerFrom = 0, lowerTo = 100, upperFrom = 0, upperTo = 100;   (percent)
double leverValue(double x, double lo, double hi, int fromPct, int toPct);
// Soft takeover: state per dial.
struct LeverPickup { bool held = false; double lastWritten = 0; double lastTarget = -1e9; };
// target: the dial's mapped value now; sim: X-Plane's value; tol: 1 % of the range. True: write `target` now.
bool leverMayWrite(LeverPickup& s, double target, double sim, double tol);
```

- [ ] **Tests first** (`tests/test_core.cpp`):
  - `TEST(lever_value_covers_the_range_from_to)`: `leverValue(0, -0.3, 0.5, 0, 100) == -0.3`, `(1, …) == 0.5`,
    `(0.5, 0, 1, 20, 60) == 0.4`, `(0.25, 0, 1, 100, 0) == 0.75` (reversed), `(0.7, 0, 1, 50, 50) == 0.5`.
  - `TEST(a_lever_waits_until_the_dial_reaches_x_planes_value)` (tol 0.02 throughout): sim 0.6; targets 0.1, 0.3 →
    false; 0.65 (crossed) → true; sim 0.65, target 0.7 → true; sim moved to 0.2 by a button (lastWritten 0.7), target
    0.72 → false (waiting again); target 0.21 → true (within tol).
  - `TEST(a_lever_with_an_empty_range_never_writes)`: lo = hi = 0 → `leverMayWrite` false for any target (tol 0).
  - `TEST(dial_ranges_and_new_jobs_round_trip_through_the_profile)`: lower = RudderTrim (5), range `25 75` →
    `"rotary_lower = 5\n"`, `"rotary_lower_range = 25 75\n"`, header `x52pro-profile 8`; parse back equal.
  - `TEST(an_older_profile_gets_the_full_range)`: `x52pro-profile 7` without `_range` → 0..100 both dials.
  - Update the four existing `version == 7` / `"x52pro-profile 7"` asserts to 8 (intended bump).
- [ ] Run `make test` → new tests fail to build (no `leverValue`).
- [ ] Implement: the enum, the `LeverJob` table (Design table; `xpAxis` 0), `leverJob`, `dialUseName`,
  `leverValue`, `leverMayWrite` (held: write when |sim − lastWritten| ≤ tol, else drop to waiting; waiting: write
  when |target − sim| ≤ tol or target and lastTarget are on different sides of sim; always update lastTarget; on
  write set held and lastWritten), parse/serialize of jobs 0..8 and `_range` (clamped 0..100), version 8.
- [ ] `make test` → pass. `make`, commit "Dial lever jobs and ranges in profiles (1.3.1, #78)".

### Task 2: The plugin drives the lever jobs

**Files:** Modify `src/plugin/app.h`, `src/plugin/plugin.cpp` (`applyDials`, `resetDials`); Test `tests/test_plugin.cpp`.

**Interfaces:** Consumes Task 1. Produces in `App`: `std::map<int, LeverPickup> leverState;` (by dial usage,
cleared in `resetDials`), `bool leverRange(const LeverJob&, double& lo, double& hi);` (reads the aircraft limits;
false when the range is empty) and `std::string leverNote(int usage);` (the window's "Turn the dial to …" or
"This aircraft has no …", "" when held).

- [ ] **Tests first**, each with the fake sim's datarefs set (`freshDials()` style; add `acf_*` limits and
  `acf_num_engines` to `sim().datarefs`):
  - `TEST(a_dial_set_to_rudder_trim_takes_over_at_x_planes_trim_and_stays_in_the_aircrafts_limits)`: limits 0.2 left,
    0.3 right; X-Plane trim 0.1. Dial from rest to the left end → trim unchanged (waiting); dial across the middle →
    takes over; dial full right → trim 0.3; full left → −0.2.
  - `TEST(a_trim_moved_by_a_button_is_not_fought_by_the_dial)`: held, then set trim by hand → next dial nudge does
    not write; reaching the new value takes over again.
  - `TEST(cowl_flaps_move_every_engine)`: `acf_num_engines` 2 → both `cowl_flap_ratio[0]` and `[1]` written (the fake
    stores one value per name: extend `XPLMSetDatavf` in `tests/fake_xplm.cpp` to record each index as
    `name[i]`); with `acf_num_engines` 0 → `[0]` written.
  - `TEST(an_aircraft_without_rudder_trim_leaves_the_dial_idle_and_says_so)`: limits 0/0 → nothing written,
    `leverNote` says "no rudder trim".
  - `TEST(switching_a_dial_from_a_lever_to_zoom_stops_the_lever)`: held on Mixture, `setDialUse(…, Zoom)`, turn →
    mixture unchanged, zoom commands sent.
- [ ] `make test` → fail. Implement in `applyDials`: for a lever job, skip if X-Plane drives `xpAxis` (when non-zero),
  `leverRange` (false → continue), target = `leverValue(x, lo, hi, from, to)`, sim = current value (index 0), tol =
  1 % of |hi − lo|, `leverMayWrite` → write (`XPLMSetDataf` or `XPLMSetDatavf` for 0..n-1 / index 0).
- [ ] `make test` → pass. `make`, commit "The plugin drives the lever jobs with soft takeover (1.3.1, #78)".

### Task 2b: The stick's Screen brightness dims the panel

**Files:** Modify `src/plugin/plugin.cpp` (`App::wheelAt`, the `PageBrightness` branch); Test `tests/test_plugin.cpp`.

- [ ] **Test first:** `TEST(the_screen_brightness_page_dims_the_aircrafts_panel_too)`: panel ratio 1.0 in the fake;
  on the Brightness page, Screen 100 % → turn down once → panel `instrument_brightness_ratio[0]` 0.95; Lights changed
  → panel unchanged; Screen linked to a dataref → panel unchanged; window slider change → panel unchanged.
- [ ] `make test` → fail. Implement: when `brightnessInput` changed item 0, `XPLMSetDatavf` index 0 with
  `percentOf(mfdBrightness) / 100.0` (expose `percentOf` from `pages.h` if needed).
- [ ] `make test` → pass. `make`, commit "The stick's Screen brightness dims the aircraft's panel (1.3.1, #78)".

### Task 3: Pick the jobs and ranges in the window and the app

**Files:** Modify `src/plugin/ui.cpp` (dial rows ~1358-1400), `src/app/bridge.cpp` (`dialUseByName` clamp 0..8; new
fields `rotary_lower_from/_to`, `rotary_upper_from/_to` 0..100), `src/app/bridge.h`, `src/app/main.swift`
(`DialSteadyRow` picker, a From/To row under a dial set to a lever job, help text). Test `tests/test_plugin.cpp`,
`tests/test_agent.cpp`.

- [ ] **Tests first:** window: the job button cycles through all nine names in `DialUse` order and saves; under a
  lever dial the "From"/"To" steppers change `lowerFrom`/`lowerTo` and save `rotary_lower_range`; the waiting note
  is drawn (`sees("Turn the dial to")`). Agent: `x52_set("rotary_lower", 8)` → 8, `x52_set("rotary_lower", 12)` →
  8 (clamped), `x52_set("rotary_lower_from", 30)` → file has `rotary_lower_range = 30 100`.
- [ ] `make test` → fail. Implement: window cycle via `dialUseName` (all jobs), From/To steppers (step 5) only for
  lever jobs, amber conflict note for lever jobs too (`xpAxis`), dim waiting note from `leverNote`. App: `Picker` with
  all nine `dialUseName` labels (export them through a bridge call `x52_dial_use_name(int)` so the names live in one
  place), From/To `Stepper`s when the job is 3..8, one help line: "Smooth jobs cover the game's range for this
  aircraft (From/To narrow it). The plugin waits until you turn the dial to X-Plane's own value, so nothing jumps."
- [ ] `make test` → pass. `make`, commit "Pick lever jobs and their range in the window and the app (1.3.1, #78)".

### Task 4: Profiles on disk
- [ ] `profiles/tools/aircraft.py` `axes(f)`: add `"rotary_lower_range = 0 100"`, `"rotary_upper_range = 0 100"`;
  `python3 profiles/tools/build_profiles.py`; check each regenerated file changed by exactly 2 lines and keeps
  `x52pro-profile 3`.
- [ ] Hand-made (`profiles/default.x52`, `A330_new.x52`, `aircraft-Cessna_CitationX_mine.x52`),
  `my_profiles/out/profiles/`, and the user's X-Plane `X52Pro` folder (back it up first): add only the two lines after
  `rotary_upper_invert`.
- [ ] `make`, commit "Profiles: full range for the dials (1.3.1, #78)".

### Task 5: Docs and version
- [ ] README (Axes tab paragraph), `profiles/README.md`, manual (`docs/manual/build_manual.js`, rebuilt with
  `node`), the Axes picture (`X52Pro --snapshot`, with a dial set to a lever job). The button maps need no change
  (their footer already says the plugin drives the rotaries).
- [ ] Brightness page help (README, manual, in-app help): turning Screen on the stick also dims the aircraft's panel.
- [ ] `VERSION` → 1.3.1; `CHANGELOG.md` 1.3.1 "Added" (the jobs, the limits, From/To, soft takeover, the panel
  following the stick's Screen brightness, #78).
- [ ] `make`, `git diff --stat`, commit "Docs and VERSION 1.3.1 for the dial lever jobs (#78)".

### Task 6: Try it in X-Plane
- [ ] `make install`; user restarts X-Plane and the app.
- [ ] Read X-Plane's assignment numbers for prop, mixture, rudder trim, aileron trim, cowl flaps, carb heat (the user
  assigns each to a spare axis for a moment, `joystick_axis_assignments` read through the web API, then set back to
  None); put them in the `LeverJob` table (`xpAxis`) with a test, then reinstall.
- [ ] In a piston twin (Baron 58): Prop and Mixture on the dials; check soft takeover (no jump after loading), the
  levers move both engines, From/To narrows the turn. In the Citation X: Rudder trim within its limits, the waiting
  note in the window. At night, Screen on the stick's Brightness page dims the panel (check index 0 is the
  panel in the Baron and the Citation X).
- [ ] PR "Closes #78", merge, delete the branch, tag `v1.3.1`, `make release`, GitHub release "X52 Pro 1.3.1".
