# Three-state live lights and alarms Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Live lights and alarms get up to 3 levels per light, up to 3 AND/OR conditions per level, with no acknowledge: an alarm stops when its situation ends.

**Architecture:** The engine already supports several rules / alarms per light (first match wins, "mode" colour falls through; alarms beat rules beat the mode colour). So a *level* is one `rule` or `alarm` line, and the file format stays backward compatible (old files load and save unchanged, no migration). The new pieces are a third condition per trigger and a per-light cap of 3 levels. Editors group by light and enforce the cap.

**Tech Stack:** C++17 core (`src/core`), X-Plane plugin (`src/plugin`), Swift/AppKit app (`src/app`), `make` for build and tests, Python profile generator (`profiles/tools`), Node manual builder (`docs/manual`).

**Spec:** `docs/superpowers/specs/2026-11-25-three-state-lights-design.md` (issue #115)

## Global Constraints

- Version **1.6.0** (`VERSION`, `CHANGELOG.md` "Added" under 1.6.0). `Profile::version` goes 11 → 12; no migration is needed because every old line keeps its meaning (say so in the comment on `version`). A third condition in a file is the only new syntax.
- Max levels per light: 3 (`kMaxLevels`). Max conditions per trigger: 3, all joined by the same `and` or all by `or`; no mixing.
- Alarms still beat live lights, which beat the mode colour. When an alarm stops, the light returns to the live light or mode colour. Do not change this.
- No acknowledge: an alarm stops when the situation that triggered it ends.
- Single-colour lights (Fire, Throttle) show only off or lit (`litOnly`).
- Before every commit: `make` passes (builds and runs tests). Public repo: no personal data or local paths in commits, issues or comments. Commit messages mention `#115`.
- Do not touch `docs/mapping/` or `my_profiles/out/mapping/` (see memory: button maps are regenerated later); this change adds no buttons.
- Branch `feat/three-state-lights`; the owner pushes; nobody merges.

## Review Focus

- Three conditions where one dataref is unreadable: the level is skipped (Unknown), never true.
- Old `rule` / `alarm` lines without a third condition save back byte-identical.
- An alarm with levels that stops falls to the next level, then the live light, then the mode colour.
- A hand-edited file with 4 levels on one light loads and works; only the editors refuse the 4th.

---

### Task 1: Third condition per trigger

**Files:**
- Modify: `src/core/profile.h:101-111` (Trigger), `src/core/profile.cpp` (`evaluate`, `datarefsUsed`, `secondText`, `parseSecond`), `src/core/manual.cpp:138`
- Test: `tests/test_core.cpp` (near `two_conditions_join_with_and_or_and_not`, ~line 1380)

**Interfaces:**
- Produces: `Trigger::third` (`Condition`), `bool Trigger::hasThird() const` (`hasSecond() && third.active()`), `Trigger::operator==` includes `third`. File syntax: `... [and|or <cond> [and|or <cond>]]` with the same join word both times. `parseConditions(std::istream&, Trigger&, std::string&)` replaces `parseSecond`.

- [ ] **Step 1: Write failing tests** in `tests/test_core.cpp`:
  `three_conditions_join_with_and`: a>0 and b>0 and c>0 true only when all three; `three_conditions_join_with_or`: true when any; `a_third_condition_with_an_unreadable_dataref_is_unknown`: `evaluate(...) == TriggerState::Unknown`; `three_conditions_survive_save_and_load`: a rule and an alarm with three conditions round-trip equal (`==`) and the text contains `and > 0 b and > 0 c`; `mixed_and_or_is_refused`: `parse` of `... and > 0 b or > 0 c` returns false with an error naming "and" / "or"; `datarefs_used_includes_the_third_condition`.
- [ ] **Step 2: Run** `make test` — Expected: FAIL (does not compile: no `third`).
- [ ] **Step 3: Implement** the interfaces above. `evaluate` combines all active conditions with the join; any Unknown gives Unknown (as for two). `secondText` becomes `moreText` writing up to two tails. Update `manual.cpp:138` so the conditions text lists the third.
- [ ] **Step 4: Run** `make test` — Expected: PASS.
- [ ] **Step 5: Commit** `feat: up to three conditions per rule and alarm (#115)`.

### Task 2: Up to 3 levels per light

**Files:**
- Modify: `src/core/profile.h:133-137`, `src/core/profile.cpp:146-156`, `src/app/bridge.cpp:213-260,276-290`
- Test: `tests/test_core.cpp` (near `an_alarm_that_is_not_triggered_leaves_the_light_to_the_next_one`, ~line 1810)

**Interfaces:**
- Produces: `constexpr int kMaxLevels = 3;` `int levelsOnLed(const std::vector<LightRule>&, int led)`; `int levelsOnLed(const std::vector<Alarm>&, int led)` (overload); `int firstFreeLed(rules)` now returns the first light with fewer than `kMaxLevels` rules. `ruleUsingLed` is removed together with its callers (`bridge.cpp` `x52_rule_conflict`, `x52_rule_set`; `ui.cpp:1052`). `x52_rule_set` returns `-2` when moving a rule to a light that already has 3 levels (it returned the other rule's index before). `x52_alarm_add` and `x52_alarm_add_preset` return `-1` when the light already has 3 levels.

- [ ] **Step 1: Write failing tests**: `a_light_takes_three_live_levels_first_match_wins` (rules on one light, `liveColor` follows order, a level whose colour is `kRuleMode` is skipped, last level's else colour is the "otherwise"); `gear_example_green_amber_off` (gear down green; moving amber; up off, built from 3 rules); `levels_on_led_counts_rules_and_alarms`; `first_free_led_skips_full_lights`; `when_an_alarm_stops_the_live_light_returns` (extends the existing layering test: stall 1 → red, stall 0 → live colour, no live rule → mode colour); `a_file_with_four_levels_still_loads`.
- [ ] **Step 2: Run** `make test` — Expected: FAIL.
- [ ] **Step 3: Implement** the signatures above; the engine functions (`liveColor`, `activeAlarm`) stay as they are.
- [ ] **Step 4: Run** `make test` — Expected: PASS.
- [ ] **Step 5: Commit** `feat: up to three levels per light (#115)`.

### Task 3: Editors

**Files:**
- Modify: `src/app/bridge.h`, `src/app/bridge.cpp` (condition `which` 0..2), `src/app/main.swift` (`TriggerEditor` ~line 799, `AlarmRow` ~1067, live lights tab ~988), `src/plugin/ui.cpp:959-1090`
- Test: `tests/test_agent.cpp` (bridge round trips), `tests/test_plugin.cpp` (window rule add)

**Interfaces:**
- Produces: `x52_trigger_*` calls take `which` 0, 1 or 2 (third); `int x52_rule_levels(int ledOrRule)` for the editors' "Level n of m" labels (implementer picks the exact accessor, same style as neighbours).

- [ ] **Step 1: Write failing tests** for the bridge round trip of a third condition, and `x52_rule_add` refusing a 4th level on a light, and the plugin window's Add rule using a light with fewer than 3 levels.
- [ ] **Step 2: Run** `make test` — Expected: FAIL.
- [ ] **Step 3: Implement.** Swift: rows for the same light show "Level n of m" and an "Add level" button that appends a level on the same light (disabled at 3, tooltip says why); the last level of a live light labels its else colour "Otherwise"; the condition editor offers up to two extra conditions with one shared AND/OR picker. Plugin window: drop the one-rule-per-light message at `ui.cpp:1052`, show the level, update the heading text at `ui.cpp:959`.
- [ ] **Step 4: Run** `make test` then `make` — Expected: PASS and a built app in `bin/`. Open the app and check by eye: add a 3-level live light and a 3-level alarm.
- [ ] **Step 5: Commit** `feat: editors for levels and the third condition (#115)`.

### Task 4: Docs and version

**Files:**
- Modify: `VERSION` (1.6.0; the built-in presets stay single alarms, escalation is documented in the manual instead), `CHANGELOG.md`, `README.md`, `profiles/README.md`, `my_profiles/README.md`, `docs/manual/build_manual.js`, `docs/manual-data.md`, in-app help text in `main.swift` and `ui.cpp`
- Check: `profiles/tools/build_profiles.py` output is unchanged

- [ ] **Step 1: Write the documents**: changelog "Added" entry under `## 1.6.0` (levels, three conditions, no profile migration), manual section with the gear example (green down, amber moving, off up) and the low-fuel escalation example, the README feature line, in-app help.
- [ ] **Step 2: Run** `python3 profiles/tools/build_profiles.py` then `git diff --stat profiles/` — Expected: no ready-made profile changes. If any, stop and investigate.
- [ ] **Step 3: Run** `cd docs/manual && node build_manual.js` — Expected: manual rebuilt without errors.
- [ ] **Step 4: Run** `make` — Expected: PASS. Tell the user to run `make install` and restart X-Plane to try it (not run automatically).
- [ ] **Step 5: Commit** `docs: three-state lights and alarms, version 1.6.0 (#115)`. Then open the PR with `Closes #115` for the user to merge; after the merge: tag `v1.6.0`, `make release`, GitHub release `X52 Pro 1.6.0` per CLAUDE.md.
