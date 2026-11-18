# Side-by-side radio screens, "more below" mark, and a page order per mode

Date: 2026-11-18. Status: draft for approval.

## Why

The stick's screen is a fixed 16 x 3 character grid (no font size). Today each radio page shows one unit
(COM1 + NAV1, COM2 + NAV2, ADF1 then ADF2 further down). The user wants a second way to see the radios: both units
of a kind side by side, so COM1 and COM2 (each with its standby) are on one screen, and the same for NAV and ADF.
They also want the screen to suit the phase of flight: e.g. in cruise, a mode whose first page is the autopilot.
And on the current pages it is not obvious that ADF2 is further down the ADF page.

## Goals

1. A second radio layout, "side by side", chosen per profile with an exclusive choice (radio buttons):
   **Classic** (today's pages, the default) or **Side by side**.
2. A "more below" mark on the classic pages whose cursor continues past what is on screen.
3. Each stick mode (1, 2, 3) can have its own page order and its own hidden pages. Turning the mode dial to a mode
   whose order differs opens the first page of that mode's order.

## Non-goals / anti-goals

- No change to the classic pages' text or labels (COM1, NAV1, ADF1, STBY ... stay as they are). Renaming to C1/N1
  was rejected by the user.
- **Don't break other features** (user's words). Every existing test must still pass unchanged; buttons, lights,
  user pages, Buttons/Brightness pages, test mode, dial acceleration, clutch + POV profile picking behave as today.
- No change for existing profiles: classic layout and one shared page order unless the user picks otherwise.
- Not mixing layouts in one profile (classic COM page and side-by-side NAV page at once): the choice is exclusive.
- The pinkie switch does not count as a mode for page orders.

## Constraints

- 16 columns, 3 lines, printable ASCII only (`mfdText`). Must be checked on the stick whether it has an arrow glyph.
- Pages are pure C++ in `src/core` (unit tested in `tests/test_core.cpp`), the plugin runs the commands
  (`pageInput` returns X-Plane command names), the Mac app edits the profile (`src/app/main.swift`, Pages tab).
- X-Plane 12 has every command used below (checked in `Resources/plugins/Commands.txt`, ADF2 included).

## Approaches considered

1. **Profile setting, exclusive (chosen by the user).** `radio_layout = classic | side`. The three radio page slots
   of the left wheel (`com1`, `com2`, `adf`) keep their keys and places; with `side` they show COM 1+2, NAV 1+2 and
   ADF 1+2. One choice, nothing to migrate in wheel orders.
2. Three extra built-in pages, placed or hidden in the Pages editor beside the classic ones. More flexible (one mode
   classic, another side by side) but the user preferred one exclusive choice.

## Design

### 1. Side-by-side pages (`radio_layout = side`)

Slots: `com1` -> COM page, `com2` -> NAV page, `adf` -> ADF page. The Pages editor list names them
"COM 1+2", "NAV 1+2", "ADF 1+2" while the layout is side by side.

**Browse view.** A fixed grid: column 0 = arrow for unit 1, columns 1-7 = unit 1's value, column 8 = arrow for unit 2,
columns 9-15 = unit 2's value. Line 1 names, line 2 active, line 3 standby.

```
[COM1]   COM2          cursor on COM1's name
 118.005 121.505
 121.900 122.800

 COM1    COM2
 118.005 121.505
>121.900 122.800       cursor on COM1's standby

 COM1   [COM2]         cursor on COM2's name
 ...
 121.900>122.800       cursor on COM2's standby
```

- Right wheel order: `[COM1]` -> COM1 standby -> `[COM2]` -> COM2 standby, wrapping like today.
- Select on a name: swap that unit's active and standby (`sim/radios/comN_standy_flip`).
- Select on a standby: open the edit view.

**Edit view** (one unit, centred, numbers never move; arrows have their own columns either side):

```
      COM2
     121.505
    >122.800           MHz: wheel changes it (stby_comN_coarse_up/down)

      COM2
     121.505
     122.800<          kHz: wheel changes it (fine, 8.33 or 25 kHz as the profile says, fast turns repeat)
```

Select steps MHz -> kHz -> back to the browse view (cursor stays on that standby). The wheel changes the value
directly in the edit view: no extra press to start changing.

**NAV page**: identical flow and grid (`110.50` is 6 wide, so it sits in the same 7-column field), NAV commands
`navN_standy_flip`, `stby_navN_coarse_`, `stby_navN_fine_` (fine repeats on fast turns).

**ADF page**: same flow; values are 4-digit kHz. The edit view has three steps, hundreds -> tens -> ones -> back,
marked like the classic ADF page's step tags:

```
      ADF1
      365
    > 379              hundreds
      379<10           tens
      379<1            ones
```

(`stby_adfN_hundreds_`, `_tens_`, `_ones_`.) Exact columns are the builder's call as long as the number does not move.

### 2. "More below" mark on classic pages

While the cursor is on the first unit (items 0-2 of COM/NAV, 0-3 of ADF), put `v` in the last (16th) column of
line 3: more values follow (NAV1, ADF2). Line 3 is 15 characters there today (`N110.50  113.90`,
`ADF2 1799 1799`), so nothing moves. On the second unit there is no mark: line 3 is already 16 characters
(`C127.275 135.000`) and the cursor simply wraps back. If the stick has an arrow character, use it instead of `v`;
check on hardware before settling. *(Builder's call, settled in review.)* The two existing tests that assert the
first-unit line 3 exactly (tests/test_core.cpp:244 and :259 area) are updated for the mark: that is the requested
change, not a break.

### 3. Page order per mode

- Profile keys: `wheel_order.1/.2/.3` and `wheel_hidden.1/.2/.3`. A mode without its own keys uses the shared
  `wheel_order` / `wheel_hidden` (today's keys), so existing profiles behave as now.
- `wheelEntries` / `wheelOrder` take the mode. Everything that builds the left wheel's order (plugin, Pages editor,
  bridge `x52_wheel_*`) passes the current or edited mode.
- A mode has an order of its own when the profile has either `wheel_order.N` or `wheel_hidden.N`; then both come from
  the mode's keys (a missing one means empty), never mixed with the shared keys. A mode with its own order always
  writes both keys, `wheel_hidden.N =` even when empty, so a reload does not fall back. Bridge calls that clear the
  shared order today (`x52_page_remove`, `x52_pages_restore_default`) treat the per-mode keys the same way.
  *(Builder's call, settled in review.)*
- When the mode dial settles on a new mode (after `modeSettleMs`, as the button modes do) **and the new mode's
  order differs from the old mode's** (the pages the left wheel visits, in order), the plugin jumps to the first page
  of the new mode's order and resets the cursor. When the orders are the same (every profile today), the page stays
  as it is, exactly as now. *(User's decision, 2026-11-18.)*
- Loads that start on the current mode's first page: aircraft change and picking a profile with clutch + POV.
  Reloads from the file watcher (the Mac app or an editor saving), save-as and the Reload command keep the current
  page, as today, unless that page is no longer in the order. *(Builder's call, settled in review.)*
- Pages editor: above the page list, "Order for: [Every mode] [Mode 1] [Mode 2] [Mode 3]". "Every mode" edits the
  shared order. Picking a mode shows that mode's order; a "Same as every mode" checkbox (on by default) means the mode
  has no keys of its own; unticking it copies the shared order into that mode to start from.

### 4. Where the layout choice lives

- Mac app, Pages tab, at the top: "Radio screens: (o) Classic  ( ) Side by side" (radio buttons), saved as
  `radio_layout` in the profile.
- Plugin settings window: the same choice, alongside the existing "COM fine tuning ... 8.33 kHz" check.

## Tests

Core tests for each new screen state (browse cursor positions, swap command, edit steps and commands, 8.33 vs 25 kHz,
NAV and ADF), the more-below marks, profile round-trip of the new keys, per-mode wheel order with fallback, and a
plugin test that a mode change jumps to that mode's first page.

## Left to the builder

- Exact columns of the ADF edit view and the NAV values inside the 7-column field.
- Whether the browse view remembers the cursor after leaving the page (today's pages reset it; keep that).
- Wording in the editor.
