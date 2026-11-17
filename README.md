# X52 Pro for X-Plane 12 on Apple Silicon

A native arm64 X-Plane plugin that does what Logitech's Windows-only programming
software and X-Plane plugin do, from inside the simulator:

- **Modes and pinkie shift** – the mode dial (1/2/3) and the pinkie switch give every
  button up to six different jobs.
- **Macros** – one press runs several commands and keystrokes in a row, with waits.
- **Tap versus hold** – a short press and a long press can do different things.
- **Keyboard keystrokes** – a button can type a key or shortcut.
- **Screen and lights** – the throttle's screen shows the profile, mode and the name of
  the button you pressed, or radio / autopilot pages; button lights change colour by mode.
- **Live lights** – a button light can follow the simulator instead of the mode: for
  example Button E red while the parking brake is on, green while it is off.
- **Alarms** – lights that flash or alternate colours when something is wrong (stall, gear
  unsafe, low fuel, overspeed, autopilot disconnect). They override live lights, which
  override the plain mode colours.
- **A window in X-Plane** to set all of this up, with one profile for all aircraft and
  optional profiles per aircraft.

- **A menu bar app** (outside X-Plane) to set and save the lights, screen and clocks,
  which keeps them applied in the background.

It is a new program, not a conversion: the Logitech installer contains no source code.

## Install

You need an Apple Silicon Mac (M1 or later), macOS 13 or newer, X-Plane 12 and an X52 or
X52 Pro stick.

**From a download (`X52-Pro.zip`)**

1. Unzip it and drag **X52 Pro** into **Applications**.
2. Open it. The first time, right-click it and choose **Open**, because the app is not
   signed with a paid Apple developer account.
3. In the window, press **Install plugin**. The app finds X-Plane 12 by itself, Steam
   or not. If it cannot, press **X-Plane folder...** and pick it.
4. Restart X-Plane.

**From the source**

Double-click **Install.command**. It builds the app (it asks for Apple's free command
line tools if you do not have them), copies it to Applications and opens it; then follow
steps 3 and 4 above. From a terminal, `make install-app` does the same.

To put the plugin into X-Plane without the app: `make install`, or
`make install XPLANE="/path/to/X-Plane 12"` if the folder is not found.

## The menu bar app

```bash
make install-app
```

This builds `X52 Pro.app` and copies it to `/Applications`. Open it once; it then lives
in the menu bar (game controller icon): **Open settings...**, **Start at login**, **Quit**.

- **Lights** – a colour for every light in each of the three modes, and brightness.
- **Screen and clock** – screen brightness, Clock 2 and 3 time zones, 12/24 hour, date
  order, latched clutch.
- **Test** – what is held or moved right now, and three lines of your own on the screen.

The stick forgets its lights when it loses power, so the app sends them again whenever
the stick is plugged in. Lights and screen settings hold in any program; button
programming stays inside X-Plane.

The app and the plugin share the same profile files. While X-Plane is running, the
plugin drives the stick and the app stays quiet (it still saves what you change, and
the plugin applies it within a second). When X-Plane quits, the app takes over again.
Without X-Plane installed, profiles live in `~/Library/Application Support/X52Pro/`.

**How X-Plane is found:** the app reads the install list X-Plane keeps in
`~/Library/Preferences/x-plane_install_12.txt`, then Steam's library list, then the usual
places (Applications, your home folder, Desktop, other disks). A folder you choose by hand
is remembered.

## Pinkie lights

Every light has two sets of colours: one per mode, and one per mode while the pinkie switch
is held (Pinkie + 1, + 2, + 3). In X-Plane (**Lights** tab, *Pinkie held*) and in the app
(**Lights** tab, *Pinkie released / Pinkie held*) choose which set you are editing. Both
show what is selected on the stick now ("Mode 2" or "Pinkie + 2"), and so does the app's
status line. A profile from before this keeps its colours for the pinkie too.

## Live lights

A live light follows the simulator instead of the mode: for example Button E red while the
parking brake is on, green while it is off. Each light can have one rule. A rule
overrides the plain colours of the Lights tab; alarms (below) override rules.

- Set them in X-Plane (**Program buttons... > Live lights**) or in the menu bar app
  (**Live lights** tab); both write the same profile file.
- **Add rule** adds a line; click a line (its arrow) to open or close it. Tick the boxes and
  press **Remove selected** to delete several at once (*Select all* ticks every line).
- A rule is: light, dataref, comparison, number, colour if true, colour if not. A second
  test can be joined with **AND** or **OR**, and either test can be turned around with
  **NOT**. Closing a line hides all of it.
  **Start from** fills in a ready-made rule (parking brake, gear, flaps, speedbrake,
  autopilot, landing lights, stall and master warning) or type any dataref; `name[3]`
  reads one item of a list. A colour can also be *mode colour*, which leaves that state to
  the mode dial's colour.
- A light can follow only one rule: the app explains why and refuses when you pick a light
  that already has one. In a profile written by an older version, rules were reversed once
  on loading (the first rule wins now, the last one used to) so the lights look the same.
- Inside X-Plane the plugin reads the datarefs directly, ten times a second.
- With X-Plane running but the plugin not, the app asks X-Plane over its network
  interface (UDP port 49000, on this Mac only). That needs *Settings > Network > Accept
  incoming connections* in X-Plane.
- If a dataref cannot be read (unknown name, X-Plane not running) that rule says nothing
  and the light keeps its mode colour. Fire and Throttle can only be off or lit. Test mode
  shows mode colours.
- In the profile file a rule looks like
  `rule e > 0.5 red green sim/cockpit2/controls/parking_brake_ratio`.

## Alarms

The **Alarms** tab of the menu bar app (the plugin applies them inside X-Plane). Priority,
from highest: **alarms**, then **live lights**, then the plain **mode colours**.

- The list is in priority order. The first alarm that is **triggered** decides its light; an
  alarm that is not triggered leaves the light to the next one. So with Stall above Gear
  unsafe, a gear problem still shows while the plane is not stalling.
- An alarm has a name, a light, an effect (**steady**, **blink**, or **alternate** between
  two colours), a speed for the last two (slow: one cycle a second, fast: two a second) and
  a trigger: one test on a dataref, or two joined with AND / OR, each with an optional NOT.
- **Add from template** adds stall, gear unsafe, low fuel, overspeed (Vne) and autopilot
  disconnect; change the light, colours or speed afterwards. **Move up / Move down** change
  the priority.
- **Try on the stick** (also on the **Test** tab) shows an alarm for a few seconds whatever
  the simulator says, so you can see the blinking.
- In the profile file:
  `alarm pov blink fast red amber "Stall" > 0 sim/cockpit2/annunciators/stall_warning`,
  and a second test as `... and not == 1 sim/other/dataref`.
- Alarms are edited in the menu bar app, not in the X-Plane window. Brightness is not
  affected.

## Your own screen pages

The screen's radio pages are built in and stay as they are. You can add pages of your own, in a tree.
A page has rows, and a row is plain text, a dataref value, or a link that opens another page. Write them
in the profile file (`Plugins > X52 Pro > Reload profile from disk` afterwards, or save the file while X-Plane
runs and it reloads within a second). Example, pasted before the `bind` lines:

```
page fly "Flight"
row fly value "Alt" "%.0f ft" sim/cockpit2/gauges/indicators/altitude_ft_pilot
row fly value "IAS" "%.0f kt" sim/cockpit2/gauges/indicators/airspeed_kts_pilot
row fly link "Fuel" fuel
page fuel "Fuel" in fly
row fuel value "Total" "%.0f kg" sim/flightmodel/weight/m_fuel_total
row fuel value "Tank 1" "%.0f kg" sim/cockpit2/fuel/fuel_quantity[0]
```

- A page without `in` is reached with the left wheel: after the radio pages, before the Buttons page. A page
  with `in <page id>` is only opened by a link row. Page ids are letters, digits, `-` and `_`.
- Rows: `text "words"`, `value "label" "format" dataref` (the format has one `%d`, `%f`, `%e` or `%g`, with an
  optional width and decimals, for example `%.0f kg`; `name[3]` reads one item of a list) and `link "label" page-id`.
- On the screen the cursor `>` is on one row (the right wheel moves it, up and down; the screen scrolls to
  keep it in view). Pressing the wheel on a link opens its page; inside a sub-page the first row is `< Back`.
  Leaving a page with the left wheel forgets where you were. A value that cannot be read shows `--`.
- To get the default screen back, delete the `page` and `row` lines: with none, only the radio pages and the
  Buttons page remain.
- The Function, Start/Stop and Reset buttons under the screen are not used for going back, because the stick
  itself uses them for its clocks and stopwatch.

## Programming buttons outside X-Plane

The **Buttons** tab of the menu bar app does what the X-Plane window's button editor does, without
starting the simulator. Pick the mode (or mode + pinkie) at the top, click a button in the list,
and add steps under **When pressed** and **When held**: **+ Command**, **+ Keystroke**,
**+ Dataref** and **+ Wait**. While you type a command or dataref, the app suggests matches from
X-Plane's own lists (`Resources/plugins/Commands.txt` and `DataRefs.txt`), so no sim is needed.
Changes are saved at once; a running X-Plane applies them within a second.

**Import what X-Plane has.** On the Buttons tab, **Import what X-Plane has...** asks which of X-Plane's joystick
profiles to import (the one for the aircraft last loaded is first) and suggests a name for the new profile.
It makes a new profile whose buttons carry X-Plane's commands as Any mode assignments, and switches to it.
An existing profile is never overwritten or changed; if the name is taken, pick another. Or start from an
empty profile instead. Either way, set each button you program to Do nothing in X-Plane's Settings > Joystick.

The Buttons tab also lists the buttons you program under **Set to Do nothing in X-Plane**. Once you
have pressed a button with X-Plane running, the plugin learns X-Plane's own number for it (saved in
`xplane-map.txt` next to the profiles), and the list shows what X-Plane has on that button, read from
X-Plane's joystick settings file: green when it is Do nothing, orange when it is still set. This is
read-only; the plugin and the app never change X-Plane's joystick settings.

## Master switch

The **Master switch** tab of the menu bar app keeps the screen, and separately every light,
on only while a dataref has the value you choose, for example while the avionics are powered
(**Use avionics power** fills that in). When the test is false the screen goes blank or the
lights go dark. With no dataref chosen, or one X-Plane does not know, it stays on.

- **Try it off on the stick** switches it off for five seconds without X-Plane, so you can see
  it working. It is also a normal condition, so AND / OR / NOT work as for alarms.
- In the profile file: `master screen > 0.5 sim/cockpit2/switches/avionics_power_on` (or
  `master lights ...`).
- Test mode shows the screen and lights regardless.

## Use

1. In X-Plane open **Plugins > X52 Pro > Program buttons...**
2. Press a button on the stick (or click it in the list), choose **Any mode**, a mode,
   or a mode + pinkie, then add steps under **When pressed** / **When held**:
   - **+ Command** – search X-Plane's commands by word ("gear", "flaps", ...). Commands
     from add-on aircraft can be typed in full (`author/aircraft/command`).
   - **+ Keystroke** – click **Record** and press the key, or type e.g. `shift+f1`.
   - **+ Dataref** – write a value to a dataref: **Set** puts a number there, **Toggle**
     flips between 0 and 1, **Add** adds an amount (use a negative number to subtract). The
     dataref name turns red if X-Plane does not know it. In the profile file:
     `set:sim/x=1`, `toggle:sim/x=0,1`, `add:sim/x=0.1,0,1` (the last two numbers keep the
     result between a minimum and a maximum). Some datarefs are read-only and X-Plane
     refuses to change them; the window then says so.
   - **+ Wait** – a pause between steps.
   - One command or key on its own follows the button (held while you hold it). Two or
     more steps run once as a macro.
3. Changes are saved as you make them.

**Important:** a button you program here must be set to *Do nothing* in X-Plane's own
*Settings > Joystick*, otherwise X-Plane and the plugin both act on it. Buttons you
leave unprogrammed here keep working through X-Plane as before. Axes and the main POV
hat always stay with X-Plane.

Other things to know:

- The **MFD page wheel** (left) flips the screen between COM1/NAV1, COM2/NAV2, ADF, DME,
  Transponder, Autopilot and OBS/Heading, then your own pages, then Buttons, then Brightness (the last page).
- The **Brightness page** sets the screen's and the lights' brightness from the stick: turn the right wheel to
  pick Screen or Lights, press to change (`>>`), turn in 5 % steps, press again to finish. It is saved at once.
  The screen never goes below 10 %. A brightness that follows a dataref shows `dataref` and is not changed here.
- The **right wheel and its button** change what the page shows, as in the manual:
  turn to move the cursor, press to act. `[118.000]` is an active frequency: press to
  swap it with standby. `>` or `<` marks a part of a value: press (it becomes `>>` or
  `<<`), turn to change it, press again to finish. Turning alone never changes anything.
  COM fine steps are 25 kHz, or 8.33 kHz channels if you tick that on the Settings tab.
- **Picking a profile on the stick:** hold the clutch, push the main POV hat up or down
  to scroll the profiles, right to use the one shown, left to go back to automatic.
  (X-Plane also sees the hat, so the view may move while you do this.)
- A worn **mode dial** can flicker between positions; the mode only changes once the
  dial has rested for 0.1 s (`mode_settle_ms` in the profile file).
- **Clutch (i)** is a safe mode: while held, buttons only show their name on the screen
  and the clutch and POV lights flash. *Latched clutch* (Settings tab) makes one press
  turn it on and the next turn it off.
- Any button shows its name on the screen when pressed, programmed or not.
- **Keystrokes** need a one-time permission: *System Settings > Privacy & Security >
  Accessibility > X-Plane*. The Settings tab shows whether it is granted.
- Profiles are plain text in `X-Plane 12/Output/preferences/X52Pro/`. They can be
  edited by hand; use *Plugins > X52 Pro > Reload profile from disk* afterwards.

## Test the stick

**Without X-Plane:** double-click `Test Stick.command`. It opens a small menu:
name controls (move or press anything and the stick's screen says what it is), write a
message on the screen, set the lights, clear the screen, list the controls.

**Inside X-Plane:** *Plugins > X52 Pro > Test mode on/off*, or the checkbox on the
window's Settings tab. While it is on, the stick's screen names whatever you move or
press and the plugin's button assignments are paused. The Settings tab also mirrors
what the stick's screen is showing.

The same tool from a terminal:

```bash
./bin/x52probe --identify
```

```bash
./bin/x52probe --text "HELLO" "X52 PRO" "ON A MAC"
```

```bash
./bin/x52probe --leds red
```

## Build and test

```bash
make
```

Builds everything into `bin/` and runs the tests: `tests/test_core.cpp` (profiles, modes, hold,
macros, screen pages and cursor, screen and light protocol), `tests/test_plugin.cpp`
(the real plugin driven through a stand-in for X-Plane: window clicks, typing, button
presses) `tests/test_agent.cpp` (the menu bar app's saving rules) and `tests/test_xplane.cpp`
(finding X-Plane and installing the plugin). `make release` makes `bin/X52-Pro.zip`.

## Layout

| Folder | What |
|---|---|
| `Install.command`, `Test Stick.command` | Double-click installer and hardware test. |
| `src/core` | Profile format, button engine, screen/light protocol. No macOS or X-Plane code. |
| `src/mac` | USB output (IOKit), button input (HID manager), keystrokes (Quartz), finding X-Plane. |
| `src/plugin` | X-Plane glue and the programming window. |
| `src/app` | Menu bar app: C++ agent, C bridge, Swift window. |
| `tools` | `x52probe`, the hardware check tool. |
| `tests` | Unit tests. |
| `docs` | Design notes. |
| `third_party/XPSDK` | X-Plane plugin SDK 4.2.0 (headers and Mac link libraries). |
| `bin` | Built programs: `X52Pro.app`, `X52Pro.xpl`, `x52probe` (made by `make`). |

The USB commands and button numbering follow the open-source
[libx52](https://github.com/nirenjan/libx52) project's documentation of the device.

## License

[MIT](LICENSE). The bundled X-Plane SDK keeps its own licence; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Not affiliated with Laminar Research,
Logitech or Saitek.
