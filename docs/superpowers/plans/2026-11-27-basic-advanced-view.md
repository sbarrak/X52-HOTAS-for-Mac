# Basic and Advanced Views Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A Basic / Advanced switch in the app window, with long explanations moved behind ⓘ buttons.

**Architecture:** The rules that can be tested (first-launch view, "does this profile use something Basic hides") are three C functions in the bridge. SwiftUI reads them and an `advanced` app preference to hide controls. Profiles and the plugin do not change.

**Tech Stack:** C++17 bridge (`src/app/bridge.cpp`), SwiftUI (`src/app/main.swift`), the project's own test harness (`tests/check.h`), `make`.

**Spec:** `docs/superpowers/specs/2026-11-27-basic-advanced-view-design.md` (its table "What each view shows" is binding).

## Global Constraints

- Version **1.8.0** (`VERSION`, `CHANGELOG.md`). Issue **#141**, branch `feat/basic-advanced-view`.
- The view is an app preference (`UserDefaults` key `advancedView`), never part of a profile. Switching it writes no profile file.
- No change to profiles, the plugin, the Pages editor, calibration, setup or uninstall windows.
- Both views keep errors, the ready-made profile notice, the header, the profile menu and the **Do nothing** instruction.
- ⓘ popovers hold the full old text; no information is dropped.
- App builds for macOS 13. Follow the file's pattern for view state: `@StateObject` with an `ObservableObject` class.
- Run `make` before every commit; do not commit if it fails. Commit messages mention `#141`.
- Public repository: no personal data or local paths in commits, PR or comments. Never merge without the user's yes.

## Review Focus

1. Someone upgrading, who has saved preferences, must open in Advanced (Task 1 test).
2. A ready-made profile in Basic: switching view or pressing **Show** must not ask for a new profile name (Task 1 test: no file change; Task 2 check).
3. Choosing another profile must update both "in use" lines at once (Task 2 check).
4. A profile with keystrokes or held actions must show them in Basic, so nothing acts unseen (Task 3 check).
5. At the smallest window (900 × 600) the header with the new switch must not push the profile menu off screen (Task 2 check).

How to look at the app (used by the checks):

```bash
make app && bin/X52Pro.app/Contents/MacOS/X52Pro --snapshot build/shots-basic --basic --profile default
```

---

### Task 1: View rules in the bridge

**Files:**
- Modify: `src/app/bridge.h` (after `x52_set`), `src/app/bridge.cpp` (after `x52_set`)
- Test: `tests/test_agent.cpp` (append at the end)

**Interfaces:**
- Produces:
  - `int x52_view_initial(int saved, int used_before);` returns 0 Basic, 1 Advanced. `saved` is -1 (none), 0 or 1.
  - `int x52_hidden_tabs(void);` bit `1 << tab` for each tab hidden in Basic that the profile in use fills. Tabs: 1 Live lights, 5 Master & brightness, 8 Pages.
  - `int x52_hidden_settings(int tab);` 1 when a setting hidden in Basic on visible tab `tab` differs from a default-constructed `Profile`. Tabs: 0 Lights, 3 Screen & clock, 7 Axes; every other tab 0.

| Result | True when |
|---|---|
| tab 1 | `rules` is not empty |
| tab 5 | `masterScreen` or `masterLights` has a dataref, or `brightScreen` or `brightLights` is active |
| tab 8 | `pages` not empty, or `wheelOrder` / `wheelHidden` not empty, or any `modeWheel[i].own`, or `radioSide` |
| settings 0 | `brightLights.active()` |
| settings 3 | `brightScreen.active()`, or `modeBannerMs`, `clutchLatched`, `dial.on`, `dial.factor` differ |
| settings 7 | `axes` smoothing, damping, lower/upper rotary and slider steady, either dial's From/To, `zoomSteps` or `wheelZoomSteps` differ |

- [ ] **Step 1: Write the failing tests**

```cpp
TEST(the_first_view_is_advanced_after_earlier_use_and_basic_for_a_new_user) {
    CHECK_EQ(x52_view_initial(-1, 0), 0);
    CHECK_EQ(x52_view_initial(-1, 1), 1);
    CHECK_EQ(x52_view_initial(0, 1), 0);     // a saved choice wins
    CHECK_EQ(x52_view_initial(1, 0), 1);
}

TEST(a_plain_profile_has_nothing_hidden_in_basic) {
    std::string dir = freshDir("view_plain");
    { std::ofstream f(dir + "/default.x52"); f << "x52pro-profile 12\nname = default\n"; }
    x52_start("", dir.c_str(), "default");
    CHECK_EQ(x52_hidden_tabs(), 0);
    for (int tab = 0; tab <= 8; ++tab) CHECK_EQ(x52_hidden_settings(tab), 0);
    std::string before = slurp(dir + "/default.x52");
    x52_hidden_tabs();
    x52_hidden_settings(7);
    CHECK_EQ(slurp(dir + "/default.x52"), before);   // asking never writes
    x52_stop();
}

TEST(hidden_tabs_and_settings_are_reported_when_the_profile_uses_them) {
    std::string dir = freshDir("view_used");
    { std::ofstream f(dir + "/default.x52"); f << "x52pro-profile 12\nname = default\n"; }
    x52_start("", dir.c_str(), "default");
    CHECK(x52_rule_add() >= 0);
    CHECK_EQ(x52_hidden_tabs(), 1 << 1);
    x52_set("radio_side", 1);
    CHECK_EQ(x52_hidden_tabs(), (1 << 1) | (1 << 8));
    x52_bright_set(0, "sim/cockpit2/electrical/instrument_brightness_ratio[0]", 0, 1, 10, 127);
    CHECK_EQ(x52_hidden_tabs(), (1 << 1) | (1 << 5) | (1 << 8));
    CHECK_EQ(x52_hidden_settings(3), 1);             // screen brightness follows a dataref
    CHECK_EQ(x52_hidden_settings(0), 0);
    x52_set("stick_smoothing", 100);
    CHECK_EQ(x52_hidden_settings(7), 1);
    x52_set("stick_smoothing", 60);
    CHECK_EQ(x52_hidden_settings(7), 0);
    x52_set("axis_roll_deadzone", 10);               // visible in Basic: not reported
    CHECK_EQ(x52_hidden_settings(7), 0);
    x52_stop();
}
```

- [ ] **Step 2:** Run `make build/test_agent`. Expected: compile error, `x52_view_initial` undeclared.
- [ ] **Step 3:** Implement the three functions as the table says, reading `agent->profile()`; with no agent they return 0. `x52_view_initial` needs no agent.
- [ ] **Step 4:** Run `make`. Expected: every test program ends with all tests passed.
- [ ] **Step 5: Commit** the code, tests, spec and this plan: `feat: view rules for Basic and Advanced (#141)`.

---

### Task 2: The switch, tabs, footer and notices

**Files:**
- Modify: `src/app/main.swift` (`Model`, `tabList`, `SettingsView`, `snapshots`)

**Interfaces:**
- Consumes: the three functions of Task 1.
- Produces: `Model.advanced: Bool` (published), `Model.setAdvanced(_ on: Bool)`, `func visibleTabs(_ advanced: Bool) -> [(Int, String)]`, `struct AdvancedNotice: View { @ObservedObject var m: Model; let text: String }` (secondary callout text, "·", and a **Show** button calling `m.setAdvanced(true)`).

- [ ] **Step 1:** `Model.advanced` is initialised before `start()` from `x52_view_initial` and saved at once. `used_before` is 1 when any of the keys `profileKey`, `xplaneFolder`, `lastUpdateCheck`, `manualTemplate` exists. `setAdvanced` saves the key; going to Basic with `tab` not in `visibleTabs(false)` sets `tab = 6`.
- [ ] **Step 2:** Basic tabs are ids `[6, 0, 2, 3, 7, 4]`, in that order. The tab strip, `stepTab` and the Command-number shortcuts use `visibleTabs(m.advanced)`.
- [ ] **Step 3:** In the header, before `ProfilePicker`, add a segmented control **Basic | Advanced** (accessibility label "View"). Let the profile picker shrink to 200 so 900 pt wide still fits.
- [ ] **Step 4:** In Basic, under the tab strip, when `x52_hidden_tabs() != 0`: `AdvancedNotice` "In Advanced: <names, comma separated>". At the top of the tab content, when `x52_hidden_settings(m.tab) != 0`: `AdvancedNotice` "Advanced settings in use".
- [ ] **Step 5:** Footer: the X-Plane path, the profiles folder path and **X-Plane folder...** show when `m.advanced || m.xplane.isEmpty || x52_catalog_size(0) == 0`. Plugin state, install or update, messages and **Open guides** always show.
- [ ] **Step 6:** `snapshots`: Advanced unless `--basic` is passed; shoot only visible tabs; restore the saved `advancedView` value (or remove it) at the end, as `profileKey` is restored.
- [ ] **Step 7: Check.** Run the snapshot command in both views with `--profile default` and with `--profile aircraft-Cessna_172SP`. Expected: Basic shows six tabs and the notice for that profile; Advanced shows nine and no notices. In the running app at 900 × 600, switch view and press **Show** on a ready-made profile: the whole header is visible and no name is asked.
- [ ] **Step 8:** Run `make`, then commit: `feat: Basic and Advanced switch (#141)`.

---

### Task 3: What Basic hides on each tab

**Files:**
- Modify: `src/app/main.swift` (`ButtonsTab`, `StepList`, `AlarmsTab`, `AlarmRow`, `ScreenTab`, `ModeNamesRow`, `AxesTab`, `CalibrationRow`, `DialSteadyRow`)

**Interfaces:**
- Consumes: `m.advanced`.

- [ ] **Step 1: Buttons.** In Basic, `StepList` offers only **+ Command**. The held and released sections (and their dividers) show when `m.advanced`, or `x52_step_count(b, l, hold) > 0`, or, for held, its label is not empty. Existing steps of every kind stay editable. The numbering paragraph is Advanced only.
- [ ] **Step 2: Alarms.** Basic hides **Add blank alarm** and replaces "Triggered when" + `TriggerEditor` with one read-only line "Triggered when: " + `triggerSummary(1, alarm)`. The empty-list text in Basic is "No alarms yet. Pick a template, for example the stall alarm."
- [ ] **Step 3: Screen & clock.** Basic hides the "Show the name for" row and its paragraph, the latched clutch toggle and its paragraph, the fast wheel toggle, stepper and paragraph, and the dividers that become empty.
- [ ] **Step 4: Axes.** Basic hides Stick handling and its paragraph; calibration values (`measured` lines); the Steady paragraph; each Steady stepper (title, bar, percent, job and Invert stay); From/To; Zoom strength; Scroll wheel steps; the smooth jobs paragraph; and everything from the last divider down (Moving now, every control). The Slider row without its stepper has nothing to set, so hide it and keep the two flap toggles.
- [ ] **Step 5:** Any message that says the X-Plane folder is "shown at the bottom of the window" stays true because of Task 2 Step 5; read it once in both views.
- [ ] **Step 6: Check.** Snapshots in both views. Compare every tab against the spec's table, row by row. Then, in Advanced, give a button a keystroke and a held action in a scratch profile, switch to Basic: both are visible and editable. Delete the scratch profile.
- [ ] **Step 7:** Run `make`, then commit: `feat: Basic view hides rarely changed settings (#141)`.

---

### Task 4: Explanations behind ⓘ

**Files:**
- Modify: `src/app/main.swift`

**Interfaces:**
- Produces: `struct InfoButton: View { let topic: String; let text: String }` (an `info.circle` button opening a popover 360 wide, text wraps and is selectable, accessibility label "About <topic>") and `struct HelpLine: View { let topic: String; var short: String = ""; let long: String }` (short callout text, secondary, followed by `InfoButton`).

- [ ] **Step 1:** Add both views; popover state in an `ObservableObject` held by `@StateObject`.
- [ ] **Step 2:** Replace each paragraph below with a `HelpLine`. `long` is the old text, unchanged, in both views. An empty short line means the ⓘ sits next to the control or heading it explains.

| Tab, paragraph | Short line |
|---|---|
| Buttons: import box intro and the "Searches use..." paragraph (one ⓘ, both texts) | Set each button you program here to Do nothing in X-Plane's Settings > Joystick. |
| Buttons: numbering (Advanced) | Button numbers |
| Lights: mode dial | The mode dial and the pinkie pick the column. |
| Live lights: intro, then network note (one ⓘ) | Live lights follow the simulator. |
| Alarms: intro, then blinking note (one ⓘ) | Alarms override live lights and colours. |
| Screen: clocks | (empty) |
| Screen: mode name, latched clutch, slow turn | (empty) |
| Pages: intro | Screen pages of your own. |
| Master: switch, brightness | The screen and lights follow a dataref. / (empty) |
| Axes: override, on | Dead zone, expo and invert below apply. |
| Axes: override, off | Off: X-Plane reads the stick itself. |
| Axes: throttle, stick handling, steady, smooth jobs | (empty) |
| Axes: calibration | Calibrate on the ground or paused: the stick still flies the aircraft. |
| Axes: dial jobs | Set a dial used here to None in X-Plane's Joystick settings. |
| Axes: slider | Set the slider to None in X-Plane if X-Plane also uses it. |

- [ ] **Step 3:** Leave alone: errors, orange and red notices, status lines, empty-list texts, "A new profile is made...", the ready-made banner and the Test tab.
- [ ] **Step 4: Check.** Snapshots in both views: no tab has an explanation longer than one line. In the running app, reach three ⓘ buttons with Tab, open them with Space, close with Escape, and compare each popover with the old text in `git show main:src/app/main.swift`.
- [ ] **Step 5:** Run `make`, then commit: `feat: long explanations behind info buttons (#141)`.

---

### Task 5: Version, documents and pull request

**Files:**
- Modify: `VERSION`, `CHANGELOG.md`, `README.md`, `DETAILS.md`, `docs/manual/build_manual.js`, `docs/manual/X52-Pro-Manual.docx` (rebuilt), `docs/images/app-*.png`

- [ ] **Step 1:** `VERSION` is `1.8.0`. `CHANGELOG.md` gets `## 1.8.0` with "Added" (the two views, #141) and "Changed" (ⓘ help, #141).
- [ ] **Step 2:** Describe both views and the ⓘ buttons in `DETAILS.md` (menu bar app section) and the manual; one sentence in `README.md`. Where text says a feature is on a tab that Basic hides, say it is in Advanced. Rebuild: `cd docs/manual && node build_manual.js`, then `python3 tools/check_docx.py docs/manual/X52-Pro-Manual.docx`.
- [ ] **Step 3:** Redo `docs/images/app-*.png` from Advanced snapshots (`--sandbox` folders as used before, so no personal paths show; look at every picture before adding it).
- [ ] **Step 4:** Run `make`. Run `git diff --stat main` and confirm no file under `profiles/` or `src/plugin/` changed.
- [ ] **Step 5:** Commit `docs: 1.8.0, Basic and Advanced views (#141)`, push, open the pull request with `Closes #141`, comment the status on the issue.
- [ ] **Step 6:** Ask the user to try it (`make install-app`) and ask before merging. After the merge: delete the branch, tag `v1.8.0`, `make release`, create the release with the DMG.
