# Side-by-side radio screens and per-mode page order — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an exclusive "side by side" radio layout (COM 1+2, NAV 1+2, ADF 1+2), a `v` "more below" mark on the
classic radio pages, and a page order / hidden pages per stick mode that the mode dial follows.

**Architecture:** New pure-C++ screen code in `src/core/sidepages.{h,cpp}`, reached through the existing
`formatSimPage` / `pageInput` entry points with a new `side` flag, so the plugin and the Mac app's preview keep one call
site each. Per-mode wheel orders live in `Profile` and are resolved by `wheelEntries` / `wheelOrder` taking a mode. The
plugin passes `profile.radioSide` and `engine.mode()`, and jumps pages in `App::layerChanged`. The Mac app edits both
through `bridge.cpp` and SwiftUI in `main.swift`.

**Tech Stack:** C++17 (clang++, arm64), X-Plane SDK, SwiftUI built by `make` with command line tools only (no Xcode:
no `@State` macro, keep view state in the `Model` ObservableObject). Tests: `tests/check.h` (`TEST`, `CHECK`, `CHECK_EQ`), run with `make test`.

**Spec:** `docs/design/2026-11-18-mfd-side-by-side-radios-and-mode-pages-design.md`

## Global Constraints

- Screen: 16 columns × 3 lines, printable ASCII only; every line goes through `mfdText` (trailing spaces trimmed).
- Classic layout is the default; existing profile files load and behave exactly as today.
- "Don't break other features": every existing test keeps passing unchanged, except the classic line-3 assertions
  that the `v` mark changes on purpose (Task 2 names them).
- Labels on classic pages stay as they are (COM1, NAV1, ADF1, STBY ...). Side pages use COM1/COM2, NAV1/NAV2, ADF1/ADF2.
- Profile keys: `radio_layout = side` (written only when side; absent = classic), `wheel_order.N`, `wheel_hidden.N`
  for N = 1..3 (both written whenever mode N has its own order, `wheel_hidden.N =` even when empty).
- The mode dial jumps to the first page only when the new mode's resolved order differs from the old mode's.
- Only aircraft change and picking a profile (clutch + POV, or the plugin window's profile choice) start on the first
  page; file-watch reloads, save-as and Reload keep the current page.
- Pinkie does not count as a mode for page orders.
- Work on branch `feature/side-by-side-radios` off `main`.

## Review Focus

1. **Layout switched while the cursor is out of the other layout's range** (classic item 5, or side editing part 2,
   when the profile flips): the screen must clamp, never index past a table. Test in Task 1.
2. **Mode dial turned while editing a side-page value:** the jump (when orders differ) must leave edit view; when orders
   are the same, the edit stays as it was. Test in Task 4.
3. **A mode whose own order hides every page:** must still show one page (Buttons), as the shared order does. Test in Task 3.
4. **Removing a user page that a mode's own order names:** the token must leave every mode's lists, not only the
   shared ones. Test in Task 5.
5. **8.33 kHz on the side COM page:** kHz step must use the `_833` commands and the fast-turn repeat, like classic. Test in Task 1.

---

### Task 0: Branch and commit the design

- [ ] **Step 1:** `git checkout -b feature/side-by-side-radios`
- [ ] **Step 2:** commit the design doc and this plan:

```bash
git add docs/design/2026-11-18-mfd-side-by-side-radios-and-mode-pages-design.md docs/superpowers/plans/2026-11-18-side-by-side-radios-and-mode-pages.md
git commit -m "Design and plan: side-by-side radio screens, per-mode page order"
```

---

### Task 1: Side-by-side radio pages (core)

**Files:**
- Create: `src/core/sidepages.h`, `src/core/sidepages.cpp`
- Modify: `src/core/pages.h` (PageCursor, `formatSimPage`, `pageInput` signatures), `src/core/pages.cpp` (dispatch),
  `src/core/mfd.h` / `src/core/mfd.cpp` (`pageName` overload), `Makefile` (`CORE` list gets `src/core/sidepages.cpp`)
- Test: `tests/test_core.cpp`

**Interfaces:**
- Produces:
  - `struct PageCursor { int item = 0; bool editing = false; int part = 0; };` — `part`: which step of the edit view
    (COM/NAV 0 MHz, 1 kHz; ADF 0 hundreds, 1 tens, 2 ones). Classic code ignores `part`.
  - `Lines formatSimPage(int page, const SimData& d, const PageCursor& c = PageCursor(), bool side = false);`
  - `std::vector<std::string> pageInput(int page, PageCursor& c, Wheel w, bool com833 = false, int repeat = 1, bool side = false);`
  - `int pageItemCount(int page, bool side = false);` — side: 4 for PageComNav1/PageComNav2/PageAdf, else as classic.
  - `const char* pageName(int page, bool side);` — side: "COM 1+2", "NAV 1+2", "ADF 1+2" for the three slots, else `pageName(page)`.
  - In `sidepages.h` (namespace x52): `bool isSidePage(int page);` (PageComNav1, PageComNav2, PageAdf),
    `Lines formatSidePage(int page, const SimData&, const PageCursor&)`,
    `std::vector<std::string> sidePageInput(int page, PageCursor&, Wheel, bool com833, int repeat)`.
  - Slot meaning when side: `PageComNav1` = COM page, `PageComNav2` = NAV page, `PageAdf` = ADF page. Other pages
    with `side = true` fall through to classic.

Behaviour (from the spec): browse items 0 `[U1]`, 1 U1 standby, 2 `[U2]`, 3 U2 standby, wheel wraps 0..3.
Select on 0/2 → flip command; on 1/3 → `editing = true, part = 0`. In edit view Up/Down change the value (no extra
press); Select advances `part`; after the last part `editing = false, part = 0`, `item` unchanged. Clamp `item` to 0..3
and `part` to the page's range on every call.

Grid (browse): col 0 arrow for unit 1, cols 1-7 value 1, col 8 arrow for unit 2, cols 9-15 value 2.
Edit view (one unit): line 0 name, line 1 active, line 2 standby with arrow; numbers never move between parts.

- [ ] **Step 1: Write the failing tests** (add `#include "../src/core/sidepages.h"` if needed)

```cpp
TEST(side_com_page_browses_names_and_standbys) {
    SimData d; d.com1 = 118005; d.com1s = 121900; d.com2 = 121505; d.com2s = 122800;
    PageCursor c;
    Lines p = formatSimPage(PageComNav1, d, c, true);
    CHECK_EQ(p.l[0], std::string("[COM1]   COM2"));
    CHECK_EQ(p.l[1], std::string(" 118.005 121.505"));
    CHECK_EQ(p.l[2], std::string(" 121.900 122.800"));
    CHECK_EQ(cmds(pageInput(PageComNav1, c, Wheel::Select, false, 1, true)), std::string("sim/radios/com1_standy_flip "));
    pageInput(PageComNav1, c, Wheel::Down, false, 1, true);
    CHECK_EQ(formatSimPage(PageComNav1, d, c, true).l[2], std::string(">121.900 122.800"));
    pageInput(PageComNav1, c, Wheel::Down, false, 1, true);
    CHECK_EQ(formatSimPage(PageComNav1, d, c, true).l[0], std::string(" COM1   [COM2]"));
    CHECK_EQ(cmds(pageInput(PageComNav1, c, Wheel::Select, false, 1, true)), std::string("sim/radios/com2_standy_flip "));
    pageInput(PageComNav1, c, Wheel::Down, false, 1, true);
    CHECK_EQ(formatSimPage(PageComNav1, d, c, true).l[2], std::string(" 121.900>122.800"));
    pageInput(PageComNav1, c, Wheel::Down, false, 1, true);
    CHECK_EQ(c.item, 0);   // wraps
}

TEST(side_com_edit_view_is_centred_and_steps_mhz_then_khz) {
    SimData d; d.com2 = 121505; d.com2s = 122800;
    PageCursor c; c.item = 3;
    pageInput(PageComNav1, c, Wheel::Select, false, 1, true);      // open edit view
    Lines p = formatSimPage(PageComNav1, d, c, true);
    CHECK_EQ(p.l[0], std::string("      COM2"));
    CHECK_EQ(p.l[1], std::string("     121.505"));
    CHECK_EQ(p.l[2], std::string("    >122.800"));
    CHECK_EQ(cmds(pageInput(PageComNav1, c, Wheel::Up, false, 1, true)), std::string("sim/radios/stby_com2_coarse_up "));
    pageInput(PageComNav1, c, Wheel::Select, false, 1, true);      // to kHz
    CHECK_EQ(formatSimPage(PageComNav1, d, c, true).l[2], std::string("     122.800<"));
    CHECK_EQ(cmds(pageInput(PageComNav1, c, Wheel::Down, true, 3, true)),
             std::string("sim/radios/stby_com2_fine_down_833 sim/radios/stby_com2_fine_down_833 sim/radios/stby_com2_fine_down_833 "));
    pageInput(PageComNav1, c, Wheel::Select, false, 1, true);      // done
    CHECK(!c.editing);
    CHECK_EQ(c.item, 3);
    CHECK_EQ(formatSimPage(PageComNav1, d, c, true).l[0], std::string(" COM1    COM2"));   // browse view again
}
```

```cpp
TEST(side_nav_page_uses_nav_commands) {
    SimData d; d.nav1 = 11050; d.nav1s = 11390; d.nav2 = 10800; d.nav2s = 11795;
    PageCursor c;
    Lines p = formatSimPage(PageComNav2, d, c, true);
    CHECK_EQ(p.l[0], std::string("[NAV1]   NAV2"));
    CHECK_EQ(p.l[1], std::string(" 110.50  108.00"));
    CHECK_EQ(cmds(pageInput(PageComNav2, c, Wheel::Select, false, 1, true)), std::string("sim/radios/nav1_standy_flip "));
    c.item = 1;
    pageInput(PageComNav2, c, Wheel::Select, false, 1, true);
    CHECK_EQ(formatSimPage(PageComNav2, d, c, true).l[2], std::string("    >113.90"));
    CHECK_EQ(cmds(pageInput(PageComNav2, c, Wheel::Up, false, 1, true)), std::string("sim/radios/stby_nav1_coarse_up "));
    pageInput(PageComNav2, c, Wheel::Select, false, 1, true);
    CHECK_EQ(cmds(pageInput(PageComNav2, c, Wheel::Up, false, 2, true)),
             std::string("sim/radios/stby_nav1_fine_up sim/radios/stby_nav1_fine_up "));
}

TEST(side_adf_page_edits_hundreds_tens_ones) {
    SimData d; d.adf1 = 365; d.adf1s = 379; d.adf2 = 410; d.adf2s = 520;
    PageCursor c;
    CHECK_EQ(formatSimPage(PageAdf, d, c, true).l[0], std::string("[ADF1]   ADF2"));
    CHECK_EQ(formatSimPage(PageAdf, d, c, true).l[1], std::string("  365     410"));
    c.item = 3;
    pageInput(PageAdf, c, Wheel::Select, false, 1, true);
    CHECK_EQ(formatSimPage(PageAdf, d, c, true).l[2], std::string("     > 520"));
    CHECK_EQ(cmds(pageInput(PageAdf, c, Wheel::Up, false, 5, true)), std::string("sim/radios/stby_adf2_hundreds_up "));  // no repeat
    pageInput(PageAdf, c, Wheel::Select, false, 1, true);
    CHECK_EQ(formatSimPage(PageAdf, d, c, true).l[2], std::string("       520<10"));
    CHECK_EQ(cmds(pageInput(PageAdf, c, Wheel::Down, false, 1, true)), std::string("sim/radios/stby_adf2_tens_down "));
    pageInput(PageAdf, c, Wheel::Select, false, 1, true);
    CHECK_EQ(formatSimPage(PageAdf, d, c, true).l[2], std::string("       520<1"));
    CHECK_EQ(cmds(pageInput(PageAdf, c, Wheel::Up, false, 1, true)), std::string("sim/radios/stby_adf2_ones_up "));
    pageInput(PageAdf, c, Wheel::Select, false, 1, true);
    CHECK(!c.editing);
}

TEST(side_pages_fit_and_clamp_out_of_range_cursors) {
    SimData d;
    d.com1 = d.com1s = d.com2 = d.com2s = 136975;
    d.nav1 = d.nav1s = d.nav2 = d.nav2s = 11795;
    d.adf1 = d.adf1s = d.adf2 = d.adf2s = 1799;
    for (int page : {(int)PageComNav1, (int)PageComNav2, (int)PageAdf})
        for (int item = -1; item < 7; ++item)
            for (int part = -1; part < 4; ++part)
                for (bool ed : {false, true}) {
                    PageCursor c; c.item = item; c.part = part; c.editing = ed;
                    Lines p = formatSimPage(page, d, c, true);
                    for (auto& l : p.l) CHECK(l.size() <= 16);
                    pageInput(page, c, Wheel::Up, false, 1, true);   // must not crash
                }
    PageCursor c; c.item = 1; c.editing = true; c.part = 2;   // side state left over, layout now classic
    CHECK(formatSimPage(PageComNav1, d, c).l[0].size() <= 16);
    CHECK_EQ(pageItemCount(PageAdf, true), 4);
    CHECK_EQ(std::string(pageName(PageComNav2, true)), std::string("NAV 1+2"));
    CHECK_EQ(std::string(pageName(PageComNav2, false)), std::string("COM2 / NAV2"));
}
```

- [ ] **Step 2: Run `make test` — expect build failure** (`formatSimPage` takes no `side`, `PageCursor` has no `part`).

- [ ] **Step 3: Implement** `sidepages.{h,cpp}` with the signatures above; add `part` to `PageCursor`; add the `side`
  parameters with defaults and, at the top of `formatSimPage` / `pageInput` / `pageItemCount`, route
  `side && isSidePage(page)` to the side functions; add `pageName(int, bool)`; add `src/core/sidepages.cpp` to `CORE`.
  Reuse the classic COM/NAV/ADF command names exactly (see `itemFor` in pages.cpp: `comN_standy_flip`,
  `stby_comN_coarse_`, `stby_comN_fine_up|down[_833]` with accel, `navN_standy_flip`, `stby_navN_coarse_`,
  `stby_navN_fine_` with accel, `adfN_standy_flip`, `stby_adfN_hundreds_|tens_|ones_` without accel). Value
  formats: COM `%3d.%03d`, NAV `%3d.%02d`, ADF `%4d`, as classic.

- [ ] **Step 4: Run `make test` — all PASS** (new and existing).

- [ ] **Step 5: Commit** `git commit -am "Side-by-side COM, NAV and ADF screens (core)"` (add the new files first).

---

### Task 2: "More below" mark on classic radio pages

**Files:**
- Modify: `src/core/pages.cpp` (`formatSimPage`, cases PageComNav1/2 and PageAdf)
- Test: `tests/test_core.cpp`

**Interfaces:** none new.

Rule: classic COM/NAV page, `item < 3` → line 2 padded to 15 characters, then `v`. Classic ADF page, `item < 4` → same.
Second unit (COM/NAV `item >= 3`, ADF `item >= 4`): no mark, line unchanged.

- [ ] **Step 1: Update the deliberate assertions and add the test**
  - `tests/test_core.cpp` `first_com_nav_page...` (≈ line 244): expect `"N110.50  113.90v"`.
  - Search `tests/` for any other assertion on classic COM/NAV or ADF line 2 with the cursor on the first unit
    (`grep -n 'l\[2\]' tests/*.cpp`, also `screen.l[2]` in `test_plugin.cpp`); update only those, adding the `v`.
    List each changed line in the commit message.

```cpp
TEST(classic_radio_pages_mark_more_below_on_the_first_unit_only) {
    SimData d; d.adf1 = 365; d.adf2 = 410; d.adf1s = 379; d.adf2s = 520;
    PageCursor c;
    CHECK_EQ(formatSimPage(PageAdf, d, c).l[2], std::string("ADF2  410  520 v"));
    c.item = 4;
    CHECK_EQ(formatSimPage(PageAdf, d, c).l[2], std::string("ADF1  365  379"));
    SimData n; n.com2 = 127275; n.com2s = 135000; n.nav2 = 10800; n.nav2s = 11795;
    PageCursor k; k.item = 4;
    CHECK_EQ(formatSimPage(PageComNav2, n, k).l[2], std::string("C127.275 135.000"));   // unchanged
}
```

- [ ] **Step 2: Run `make test` — expect the new/updated assertions to FAIL.**
- [ ] **Step 3: Implement the rule above.**
- [ ] **Step 4: Run `make test` — all PASS.**
- [ ] **Step 5: Commit** `"Classic radio pages: v marks more values below"`.

---

### Task 3: Profile — radio layout and per-mode wheel order

**Files:**
- Modify: `src/core/profile.h` (Profile fields), `src/core/profile.cpp` (parse ≈ line 855, serialize ≈ line 530),
  `src/core/pageview.h` / `src/core/pageview.cpp` (`wheelEntries`, `wheelOrder`)
- Test: `tests/test_core.cpp`

**Interfaces:**
- Produces:
  - `struct WheelSet { std::vector<std::string> order, hidden; bool own = false; };` (in profile.h)
  - `Profile::radioSide` (`bool`, default false) ↔ `radio_layout = side` / `classic` (anything else: classic).
  - `Profile::modeWheel[3]` (`WheelSet`), index = mode − 1. Reading either `wheel_order.N` or `wheel_hidden.N` sets
    `own = true`; a missing one of the pair is empty. Writing: when `own`, both lines are written
    (`wheel_hidden.N =` with nothing after it when empty).
  - `std::vector<WheelEntry> wheelEntries(const Profile& p, int mode = 0);`
  - `std::vector<int> wheelOrder(const Profile& p, int mode = 0);` — mode 0, out of range, or `!modeWheel[mode-1].own`:
    the shared `wheelOrder` / `wheelHidden`, exactly as today; otherwise that mode's lists through the same algorithm.
    Same "never empty" fallback (Buttons).

- [ ] **Step 1: Write the failing tests**

```cpp
TEST(radio_layout_round_trips_and_defaults_to_classic) {
    Profile p = defaultProfile();
    CHECK(serializeProfile(p).find("radio_layout") == std::string::npos);   // use the file's real serializer name
    p.radioSide = true;
    Profile q; std::string err;
    CHECK(parseProfile(serializeProfile(p), q, err));                      // ditto for the parser
    CHECK(q.radioSide);
}

TEST(a_mode_can_have_its_own_wheel_order) {
    Profile p = defaultProfile();
    p.modeWheel[1].own = true;
    p.modeWheel[1].order = {"ap", "com1"};
    std::vector<int> two = wheelOrder(p, 2);
    CHECK_EQ(two[0], (int)PageAutopilot);
    CHECK(wheelOrder(p, 1) == wheelOrder(p));          // mode 1 has none: shared
    CHECK(wheelOrder(p, 3) == wheelOrder(p));
    std::string text = serializeProfile(p);
    CHECK(text.find("wheel_order.2 = ap com1\n") != std::string::npos);
    CHECK(text.find("wheel_hidden.2 =\n") != std::string::npos);
    Profile q; std::string err;
    CHECK(parseProfile(text, q, err));
    CHECK(q.modeWheel[1].own && q.modeWheel[1].hidden.empty());
    CHECK(!q.modeWheel[0].own);
}

TEST(a_mode_with_only_hidden_pages_is_its_own_and_hiding_everything_keeps_buttons) {
    Profile q; std::string err;
    CHECK(parseProfile("wheel_hidden = adf\nwheel_hidden.3 = com1 com2 adf dme xpdr ap vorhdg buttons brightness\n", q, err));
    CHECK(q.modeWheel[2].own && q.modeWheel[2].order.empty());
    std::vector<int> three = wheelOrder(q, 3);
    CHECK_EQ((int)three.size(), 1);
    CHECK_EQ(three[0], (int)PageButtons);
    CHECK(std::find(wheelOrder(q, 3).begin(), wheelOrder(q, 3).end(), (int)PageAdf) == wheelOrder(q, 3).end());
}
```
(Use whatever the profile text reader/writer are actually called in `profile.h`; the tests' meaning is fixed, the
helper names follow the file. A parse fixture may need the file's header line — copy it from an existing parse test.)

- [ ] **Step 2: Run `make test` — expect build failure.**
- [ ] **Step 3: Implement** fields, parse (`wheel_order.N` / `wheel_hidden.N` with N 1..3; other N ignored),
  serialize (shared keys exactly as today, then per-mode pairs for `own` modes, then `radio_layout = side` only when
  set), and the `mode` parameter of `wheelEntries` / `wheelOrder`.
- [ ] **Step 4: Run `make test` — all PASS** (existing `wheel_*` tests in test_core and test_agent unchanged).
- [ ] **Step 5: Commit** `"Profile: radio layout and per-mode wheel order"`.

---

### Task 4: Plugin — use the layout, follow the mode dial, first page on profile pick

**Files:**
- Modify: `src/plugin/app.h`, `src/plugin/plugin.cpp` (≈ lines 217-235, 438-454, 474, 594, 614, 628, 680-690, 884),
  `src/plugin/ui.cpp` (≈ line 1064, settings checks)
- Test: `tests/test_plugin.cpp`

**Interfaces:**
- Consumes: Task 1 `formatSimPage(..., side)`, `pageInput(..., side)`; Task 3 `wheelOrder(p, mode)`, `Profile::radioSide`.
- Produces: `std::vector<int> App::pageOrder() const` now returns `wheelOrder(profile, engine.mode())`;
  `void App::showFirstPage();` (setPage(pageOrder()[0])); `int App::pageMode = 1;` (mode the current order belongs to).

Behaviour:
- Every `formatSimPage` / `pageInput` call in plugin.cpp passes `profile.radioSide`.
- `App::layerChanged(int mode, bool)`: if `wheelOrder(profile, mode) != wheelOrder(profile, pageMode)` → `showFirstPage()`;
  then `pageMode = mode`. Shift changes call it with the same mode: no jump.
- `showFirstPage()` after `reload()` in `loadForAircraft`, in the clutch + POV confirm branch (`direction == 1`), and in
  `chooseProfile`. Not in `reload()` itself (file watch, save-as, Reload menu/command).
- Plugin settings window: a check "Side-by-side radio screens (COM 1+2, NAV 1+2, ADF 1+2)" next to the 8.33 kHz check,
  toggling `profile.radioSide` then `profileChanged()`.

- [ ] **Step 1: Write the failing tests** (follow `the_left_wheel_follows_the_profiles_order_and_skips_hidden_pages` for
  setup/teardown; use the file's existing way of turning the mode dial — `press(28|29|30, ...)` plus time past
  `modeSettleMs` and a `sim().frame()`, as `mode_dial_switches_which_assignment_runs` does). Add
  `static void turnModeDial(int mode)` at the top of the new tests: presses mode button `27 + mode`, advances time past
  `modeSettleMs`, runs a frame, releases.)

```cpp
TEST(side_layout_shows_both_coms_on_the_first_radio_page) {
    gApp->profile.radioSide = true;
    gApp->setPage(PageComNav1);
    gApp->nextDeviceUpdate = 0;
    sim().frame();
    CHECK_EQ(gApp->screen.l[0].substr(0, 6), std::string("[COM1]"));
    gApp->profile.radioSide = false;
}

TEST(the_mode_dial_opens_the_first_page_only_when_the_orders_differ) {
    gApp->setPage(PageXpdr);
    turnModeDial(2);                       // helper built from the existing dial test
    CHECK_EQ(gApp->page, (int)PageXpdr);   // same order in every mode: stays
    gApp->profile.modeWheel[2].own = true;
    gApp->profile.modeWheel[2].order = {"ap"};
    gApp->cursor.editing = true;
    turnModeDial(3);
    CHECK_EQ(gApp->page, (int)PageAutopilot);
    CHECK(!gApp->cursor.editing);
    turnModeDial(1);
    CHECK_EQ(gApp->page, (int)PageComNav1);   // back to the shared order's first page
    gApp->profile.modeWheel[2] = WheelSet();
}

TEST(a_file_reload_keeps_the_page_and_a_new_aircraft_starts_on_the_first) {
    gApp->setPage(PageXpdr);
    gApp->reload();
    CHECK_EQ(gApp->page, (int)PageXpdr);
    gApp->loadForAircraft("Cessna_172SP");
    CHECK_EQ(gApp->page, gApp->pageOrder()[0]);
}
```

- [ ] **Step 2: Run `make test` — expect FAIL.**
- [ ] **Step 3: Implement the behaviour above.**
- [ ] **Step 4: Run `make test` — all PASS**, including every existing plugin test (if an existing test now fails
  because a load starts on the first page, stop and report it: that is a behaviour question, not a test to rewrite).
- [ ] **Step 5: Commit** `"Plugin: side-by-side layout, mode dial follows per-mode page order"`.

---

### Task 5: Mac app — layout radio buttons and per-mode order in the Pages editor

**Files:**
- Modify: `src/app/bridge.h`, `src/app/bridge.cpp` (`flag()` ≈ line 58; `x52_page_remove` ≈ 925;
  `x52_pages_restore_default` ≈ 1035; wheel functions ≈ 1100-1160), `src/app/main.swift` (`PagesTab` ≈ 1813; the
  wheel list view that calls `wheelEntryRows()` ≈ 1535; `Model`)
- Test: `tests/test_agent.cpp`

**Interfaces:**
- Consumes: Task 3 `WheelSet`, `Profile::modeWheel`, `Profile::radioSide`, `wheelEntries(p, mode)`; Task 1 `pageName(int, bool)`.
- Produces (bridge.h, C linkage like the others):
  - `x52_get("radio_side")` / `x52_set("radio_side", 0|1)` via `flag()`.
  - `void x52_wheel_edit_mode(int mode);` — 0 every mode, 1..3; which order the wheel functions below act on.
  - `int x52_wheel_mode_own(int mode);` — 1 when that mode has its own order.
  - `void x52_wheel_set_mode_own(int mode, int own);` — own 1: copy the shared order (the full `wheelEntries(p)` token
    list) and hidden list into the mode; own 0: clear it (`WheelSet()`).
  - `x52_wheel_entries`, `x52_wheel_move`, `x52_wheel_hide` act on the edited mode (a mode without its own order:
    move/hide first make it own, copying the shared lists, as `x52_wheel_set_mode_own(mode, 1)`). Built-in titles use
    `pageName(page, p.radioSide)`. `x52_builtin_preview` passes `p.radioSide`.
  - `x52_page_remove` erases the page's token from every `modeWheel[i].order/hidden` too; `x52_pages_restore_default`
    also resets `modeWheel[0..2]`.

SwiftUI:
- `PagesTab`: `Picker("Radio screens", selection: Binding(get: { m.get("radio_side") }, set: { m.set("radio_side", $0) })) { Text("Classic").tag(0); Text("Side by side").tag(1) }.pickerStyle(.radioGroup)` at the top.
- Wheel list: above the rows, a segmented `Picker` "Order for" with Every mode / Mode 1 / Mode 2 / Mode 3 bound to a new
  `@Published var wheelMode = 0` on `Model` (setter calls `x52_wheel_edit_mode` and `tick()`); when `wheelMode != 0`,
  `Toggle("Same as every mode", isOn: ...)` bound to `x52_wheel_mode_own(m.wheelMode) == 0`, calling `x52_wheel_set_mode_own`.
  When "Same as every mode" is on, the rows show the shared order and their eye / arrows are disabled.

- [ ] **Step 1: Write the failing agent test** (follow the existing wheel test ≈ line 500 for setup)

```cpp
TEST(the_pages_editor_sets_the_radio_layout_and_a_mode_order) {
    // setup as the existing wheel test: x52_start(...), x52_pages_restore_default()
    x52_set("radio_side", 1);
    CHECK(has(slurp(file), "radio_layout = side\n"));
    CHECK(has(x52_wheel_entries(), "com1\tCOM 1+2\t0\t1\n"));
    x52_set("radio_side", 0);
    CHECK(!has(slurp(file), "radio_layout"));
    x52_wheel_edit_mode(2);
    CHECK_EQ(x52_wheel_mode_own(2), 0);
    x52_wheel_move("ap", -1);                                  // makes mode 2 its own
    CHECK_EQ(x52_wheel_mode_own(2), 1);
    CHECK(has(slurp(file), "wheel_order.2 = com1 com2 adf dme ap xpdr"));
    CHECK(!has(slurp(file), "wheel_order ="));                 // the shared order is untouched
    int r = x52_page_add("Radio", "");
    x52_wheel_hide("@radio", 1);
    x52_page_remove(r);
    CHECK(!has(slurp(file), "@radio"));                        // gone from mode 2's lists too
    x52_wheel_set_mode_own(2, 0);
    CHECK(!has(slurp(file), "wheel_order.2"));
    x52_wheel_edit_mode(0);
}
```

- [ ] **Step 2: Run `make test` — expect build failure.**
- [ ] **Step 3: Implement the bridge functions and the SwiftUI controls above.**
- [ ] **Step 4: Run `make test` — all PASS; then `make app` — builds with no errors.**
- [ ] **Step 5: Commit** `"Mac app: radio layout choice and per-mode page order"`.

---

### Task 6: Check on the stick (manual, with the user)

- [ ] **Step 1:** `make install-app` (or `make install`), start X-Plane, Cessna 172.
- [ ] **Step 2:** Classic: ADF page shows `v` at the end of line 3 on ADF1; it disappears on ADF2. Ask whether the stick
  shows a real arrow for any character code the user wants to try; if yes, swap `v` for it in Task 2's code and test.
- [ ] **Step 3:** Pick "Side by side" in the Mac app; on the stick check the COM page browse marks, swap, the centred
  edit view (MHz, kHz, back), NAV and ADF pages. Ask the user whether the centred view looks right.
- [ ] **Step 4:** Give mode 2 its own order starting with AP; turn the dial 1 → 2 → 1 and check the jumps; a light-colour
  change saved from the Mac app must not move the page.
- [ ] **Step 5:** Note results in memory (`x52pro-setup.md` "unverified on hardware" line) and in the PR description.
