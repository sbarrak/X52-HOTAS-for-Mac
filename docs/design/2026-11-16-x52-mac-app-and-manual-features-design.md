# X52 Pro: Mac setup app and the missing manual features

Date: 2026-11-16. Status: waiting for approval.

## Why

The X-Plane plugin covers button programming, but two things from Logitech's Windows
software are still missing on the Mac:

1. Several features the manual describes do not work yet, most visibly changing radio
   channels from the stick's screen (the Flight Simulator plug-in feature).
2. There is no control panel outside the simulator. The only way to set lights without
   X-Plane is the text menu in `X52 Test.command`.

## Goals

- A Mac app with a menu bar icon that sets and saves lights, screen and clock settings,
  keeps them applied in the background, and can be quit from its icon.
- Lights and screen settings hold for any program that uses the stick, not just X-Plane.
- The plugin gains every manual feature listed under "Plugin changes".
- The app and the plugin never fight over the stick.

## Not in this work

- Button programming (modes, macros, keystrokes) outside X-Plane. It stays in the
  plugin's window. Could come later by running the button engine in the app.
- Deadzones and axis curves: X-Plane's own joystick settings do this.
- Intel Macs, Windows, Linux.

## What would make it a failure

- Lights flickering or flipping between two settings because app and plugin both write.
- The app overwriting button assignments made in the plugin's window.
- A background program the user cannot see or stop.

## Approaches

| | Approach | Trade-off |
|---|---|---|
| A (chosen) | Native Mac menu bar app in Swift, reusing the existing C++ profile and USB code through a small C bridge. Built by the Makefile with `swiftc`; no Xcode project. | Small, starts at login cleanly, one profile format. Needs a bridge layer. |
| B | Local web page served by a small helper program. | Easy to style, but needs a browser tab, a local port and still a helper; more moving parts. |
| C | Only extend the window inside X-Plane. | No new program, but nothing works unless X-Plane is running, which is the gap to close. |

A was chosen by Claude (the user left the how open): it is the only one that gives a
menu bar icon, background lights and no extra runtime.

## The app ("X52 Pro.app")

- Menu bar icon only (no Dock icon). Menu: stick status, **Open settings...**,
  **Start at login** (tick), **Quit**. Quit stops everything; the stick then keeps its
  last lights until it loses power.
- Settings window, three tabs:
  - **Lights**: a picture-like list of the 11 lights, a column per mode (1/2/3), each
    cell off / green / amber / red (Fire and Throttle: off / on). Brightness slider.
    Clicking changes the stick at once; "Copy mode 1 to all".
  - **Screen and clock**: screen brightness; Clock 2 and Clock 3 time offset; 12 or
    24 hour per clock; date order; latched clutch on/off.
  - **Test**: live view of buttons held and controls moved, a field to write three
    lines on the stick's screen.
- Background job: when the stick is plugged in (or replugged), send the lights for the
  current mode, brightness and the clock. Follows the mode dial so colours change with
  the mode. Outside X-Plane the screen shows profile name, mode and clock.
- Saves on every change, into the same profile file the plugin uses
  (`X-Plane 12/Output/preferences/X52Pro/default.x52`), changing only the settings it
  owns and leaving button assignments as they are. X-Plane's folder is found at the
  Steam path, changeable in the window; if there is no X-Plane, the file lives in
  `~/Library/Application Support/X52Pro/`.
- Install: `make app` builds it, `make install-app` copies it to `/Applications`.
  Ad-hoc signed, arm64, macOS 13 or later (needed for start at login).

## Who owns the stick

- While the plugin is enabled in X-Plane it holds a lock file in the profile folder
  (released by the system even if X-Plane crashes).
- While that lock is held the app sends nothing to the stick. Its window still works:
  it saves the file, the plugin notices the file changed (checked once a second) and
  applies it. So the result is the same, but there is one writer.
- When X-Plane quits, the app takes over again within a second or two.
- In X-Plane, per-aircraft profiles keep working; the app edits the default profile
  and, via a picker, any aircraft profile that exists.

## Plugin changes

1. **Radio stack editing** as in the manual. Right wheel (buttons 37/38) moves a cursor,
   Select (39) acts:
   - COM/NAV pages: `[ ]` on the active frequency, Select swaps active and standby;
     `>` then `>>` edits the whole-number part of the standby, `<` then `<<` the decimals.
   - ADF: swap and edit the same way. Transponder: edit each digit and the mode.
     Autopilot: edit heading, altitude, vertical speed, speed. DME stays read-only.
   - Changes go through X-Plane's own radio and autopilot commands where they exist, so
     add-on aircraft follow; direct value writes only as fallback.
   - New commands `x52pro/mfd_up`, `mfd_down`, `mfd_select`, bound by default to the
     wheel; existing profiles get the three bindings added if those buttons are free.
2. **Button names**: an unprogrammed button shows its standard name when pressed.
3. **Clutch**: optional latched mode; clutch and POV lights flash while it is engaged.
4. **Profile picking**: with the clutch engaged, POV hat up/down scrolls the profiles in
   the folder, right activates, left goes back to the automatic choice.
5. **Clock**: Clock 2 and 3 offsets, 12/24 hour, date order.
6. Lock file and file-change reload (see above).

## Left to the builder

- Exact USB values for flashing, clock offsets and date order: take them from libx52's
  protocol documentation and confirm on the stick, which is available.
- Exact X-Plane command names for each radio step.
- Window layout details, icon drawing.

## Checking

- Unit tests for cursor/edit logic, clock packing, profile fields, partial save.
- Plugin tests through the stand-in X-Plane for wheel editing, clutch, profile picking.
- On the real stick, outside X-Plane: lights per mode, brightness, clock, flashing,
  replug, quit.
- Inside X-Plane: needs the user to start the simulator; until then in-sim behaviour is
  only covered by the stand-in tests.
