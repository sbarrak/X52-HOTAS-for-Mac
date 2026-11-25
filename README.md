<div align="center">

# X52 Pro for X-Plane 12 on Apple Silicon

**Your X52 Pro, fully programmable, on a Mac.**

[![Version](https://img.shields.io/badge/version-1.0.0-2f6fed)](VERSION)
[![Platform](https://img.shields.io/badge/macOS-Apple%20Silicon-000000?logo=apple&logoColor=white)](#install)
[![X-Plane 12](https://img.shields.io/badge/X--Plane-12-1d6fa5)](#install)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Donate](https://img.shields.io/badge/Donate-PayPal-00457C?logo=paypal&logoColor=white)](https://paypal.me/sbarrak)

<img src="docs/images/app-buttons.png" alt="The X52 Pro app: programming a button" width="780">

**Modes · Pinkie shift · Macros · Radio pages on the stick's screen · Live lights · Ready-made profiles**

</div>

---

Version 1.0.

Logitech's programming software and X-Plane plugin for the Saitek / Logitech X52 and X52 Pro only ever ran on
Windows. X52 Pro is a new, native Apple Silicon program that does the same job on a Mac: a plugin that runs
inside X-Plane 12 and a menu bar app that works with or without the simulator. Give every button up to six jobs,
write macros, put radios and autopilot on the throttle's screen, and have the lights follow the aircraft.

## What you get

**Buttons that do more**

- **Three modes and a pinkie shift** – the mode dial (1/2/3) and the pinkie switch give every button up to six
  different jobs. The ready-made profiles use them as phases of flight: takeoff, cruise, landing.
- **The mode's name on the screen** – when the mode dial moves, the stick's screen says the mode's name (you can
  rename the modes) for 1.5 seconds, or as long as you set.
- **Macros** – one press runs several commands and keystrokes in a row, with waits between them.
- **Tap versus hold** – a short press and a long press can do different things. A button can also do something when it is let go, and a *Save* dataref step plus a *Restore* step put a setting back (the F-14 trigger selects the Gun while held and puts the weapon back).
- **Commands, datarefs and keystrokes** – a button can run any X-Plane command, set, toggle or add to a dataref, or
  type a key or shortcut. Searches use X-Plane's own command and dataref lists, plus the add-on aircraft's own (the F-14's weapon select, ...), so the app works without the sim. Type several words to narrow the list; the Down and Up arrows move through the suggestions and Return picks one.
- **Press to select** – with the Buttons tab open, pressing a button on the stick selects it in the list. Each button that has a light shows that light's colour next to its number.
- **Import what X-Plane has** – start from the buttons X-Plane already has on the stick instead of from nothing.

**Lights that tell you something**

- **Mode colours** – every light has a colour per mode, and another per mode while the pinkie is held.
- **Live lights** – a light follows the simulator: Button E red while the parking brake is on, green while it is off.
  Rules can combine two tests with AND, OR and NOT.
- **Alarms** – lights that blink or alternate colours for stall, gear unsafe, fire, low fuel, overspeed or autopilot
  disconnect. They override live lights, which override the mode colours.
- **Master switch** – keep the screen and the lights on only while a dataref has the value you choose, for example
  while the avionics are powered.

![Light colours for each mode, with and without the pinkie](docs/images/app-lights.png)

**A screen that is a cockpit panel**

- **Radio and autopilot pages** – COM1/NAV1, COM2/NAV2, ADF, DME, transponder, autopilot and OBS/heading, driven by
  the two wheels. Fast turns jump in bigger steps for heading, altitude and speed.
- **Two radio layouts** – classic, one radio per screen (COM1 / NAV1, COM2 / NAV2, ADF), or side by side, with COM 1 and
  2, NAV 1 and 2 and ADF 1 and 2 together on one screen.
- **A page order for each mode** – put the autopilot first in mode 2 and the radios first in mode 1; skip the pages
  you never use.
- **Your own pages** – a tree of pages with text, live dataref values and links, built in a pages editor with a
  live preview of the three-line screen.
- **Clocks and brightness** – three clocks (GMT, the simulator's time, the Mac's time or any time zone), stopwatch,
  and screen and light brightness you can change from the stick itself.

![The pages editor, with a preview of the stick's screen](docs/images/app-pages-editor.png)

**Flying**

- **Axes** – dead zone, expo curve and inversion for the stick, twist and throttle; the slider as a flap lever; the
  dials on the throttle as the speed brake (in four steps: Armed, Retracted, Half, Full), the zoom, or a smooth
  lever for prop, mixture, rudder or aileron trim, cowl flaps or carb heat within the game's limits, chosen per
  profile; a view of every control the stick reports.
- **Plugin on/off** – a switch in the app's status row turns the X-Plane plugin off completely (red light) and back
  on (green light), for when you want X-Plane's own Joystick settings alone.
- **Clutch safe mode** – while the clutch (i) is held, buttons only show their name, so you can look at the stick
  without doing anything.
- **Test mode** – the screen names whatever you move or press.

**Profiles you can trust**

- **Ready-made profiles** for the aircraft that come with X-Plane 12 (see `profiles/README.md`). They are never
  changed: edit one and the app saves your change as a new profile of yours.
- **One profile for all aircraft, or one per aircraft.** Pick a profile in the app, in the X-Plane window, or on the
  stick itself with the clutch and the POV hat.
- **Plain text files** you can read, edit by hand, copy and share.
- **Set aside, not deleted** – a deleted profile can be brought back with Undo delete.

**Two ways to work**

- **A window inside X-Plane** (Plugins > X52 Pro) to program buttons while you fly.
- **A menu bar app** outside X-Plane that holds the lights, screen and clocks (it sends them again whenever the stick
  is plugged in) and edits everything else without starting the simulator.

It is a new program, not a conversion: the Logitech installer contains no source code. The full
[manual (Word)](docs/manual/X52-Pro-Manual.docx) describes the app, every window and the X-Plane plugin.

## Install

You need an Apple Silicon Mac (M1 or later), macOS 13 or newer, X-Plane 12 and an X52 or
X52 Pro stick.

**From a download (`X52-Pro-1.0.0.zip`)**

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
- **Screen and clock** – screen brightness, what Clock 1 shows (GMT, the simulator's local time, the Mac's
  local time or a fixed time zone), Clock 2 and 3 time zones (always from GMT), 12/24 hour, date order,
  latched clutch.
- **Test** – what is held or moved right now, and three lines of your own on the screen.

The stick forgets its lights when it loses power, so the app sends them again whenever
the stick is plugged in. Lights and screen settings hold in any program; button
programming stays inside X-Plane.

The app and the plugin share the same profile files. While X-Plane is running, the
plugin drives the stick and the app stays quiet (it still saves what you change, and
the plugin applies it within a second). When X-Plane quits, the app takes over again.
Without X-Plane installed, profiles live in `~/Library/Application Support/X52Pro/`.

**Ready-made profiles are never changed.** The profiles that come with this project (Default and one for each
X-Plane aircraft, see `profiles/README.md`) say `standard = 1`, and the app and the plugin never write them, so
you can always go back to the original. The settings window shows a lock and **Save as new profile...** while
one is selected, and the profile pickers list them in their own **Ready-made profiles** group.

- Change anything on a ready-made profile in the app (lights, alarms, buttons, screen pages, axes) and it asks
  for a name first: your change, and any after it, go into a new profile of yours, which the app switches to.
  Cancel drops the change.
- Your copy remembers where it came from (`based_on = aircraft-Cessna_CitationX`) and X-Plane uses it for that
  aircraft from then on (or instead of Default, for a copy of Default). Picking the ready-made profile again,
  in the app, the X-Plane window or on the stick, goes back to it; picking your copy again uses the copy. The
  choice is kept in `profile-choices.txt` next to the profiles.
- In X-Plane, a change made on a ready-made profile (the X-Plane window's editors, the Brightness page on the
  stick) works for that flight but is not saved. **Save as my copy** on the window's bottom line keeps it, as
  "<aircraft> mine". **Delete my copy** sets your copy aside and the ready-made profile is used again; a
  ready-made profile itself cannot be deleted.
- Profiles you make yourself (an import, a copy, an aircraft profile made in X-Plane) save as you change them.
- The menu next to the app's profile picker has **New blank profile...** (no buttons, live lights or alarms,
  the built-in pages; the axis settings of the profile in use are kept so the aircraft still flies) and
  **Delete this profile...** for a profile of yours. Default and ready-made profiles are never deleted. A
  deleted profile is set aside as `<name>.x52.deleted`: **Undo delete** brings it back.

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
A page has rows, and a row is plain text, a dataref value, or a link that opens another page.

**In the menu bar app:** the **Pages** tab lists your pages and shows the selected one as the stick shows it.
**Open pages editor...** opens a window of its own for the selected profile (picked at its top):

- Left, the left wheel's pages in their order: the built-in ones (a lock: COM1 / NAV1, COM2 / NAV2, ADF, DME,
  XPDR, AP, VOR / HDG, Buttons, Brightness) and yours, with the pages inside each of yours indented under it.
  The eye shows or skips a page: a skipped page stays in the list (crossed out) and the left wheel passes it by.
  The arrows move the selected page in the wheel's order (a page inside another moves among its neighbours).
  **Add page**, **Add inside** (a page only a link on the selected page opens), **Remove** (it also removes the
  pages inside it, and clears the links that opened them). A built-in page cannot be removed or changed, only
  skipped or moved.
- Middle, the selected page: its **Title**, **Opens from** (the left wheel, or inside another page) and its
  rows. Each row is **Text**, **Value** or **Link**. A value row takes a dataref (suggestions come from X-Plane's
  `DataRefs.txt` while you type), a label and a format, with an example of what the format makes of 1234.5.
  A link row picks the page it opens, or makes a new page inside this one.
- Right, the page on a black three-line screen (with X-Plane's live numbers when it answers over UDP, made-up
  ones otherwise) and the page lines as they are saved in the profile file, with **Show file**.
- **Restore default pages...** asks first, then removes all your pages and brings back the usual order with every
  built-in page shown.

Every change is saved at once into the profile file (button assignments and other settings in it are kept), and a
running X-Plane picks it up within a second.

**In the profile file** the pages are plain lines, and you can still write them by hand
(`Plugins > X52 Pro > Reload profile from disk` afterwards, or save the file while X-Plane runs and it reloads
within a second). Example, pasted before the `bind` lines:

```
page fly "Flight"
row fly value "Alt" "%.0f ft" sim/cockpit2/gauges/indicators/altitude_ft_pilot
row fly value "IAS" "%.0f kt" sim/cockpit2/gauges/indicators/airspeed_kts_pilot
row fly link "Fuel" fuel
page fuel "Fuel" in fly
row fuel value "Total" "%.0f kg" sim/flightmodel/weight/m_fuel_total
row fuel value "Tank 1" "%.0f kg" sim/cockpit2/fuel/fuel_quantity[0]
```

- A page without `in` is reached with the left wheel: after the radio pages, before the Buttons page, unless the
  order says otherwise (below). A page with `in <page id>` is only opened by a link row. Page ids are letters,
  digits, `-` and `_`.
- The left wheel's order and the pages it skips, as the editor saves them:
  ```
  wheel_order = @fly com1 com2 xpdr ap vorhdg buttons brightness
  wheel_hidden = adf dme
  ```
  Built-in pages are `com1 com2 adf dme xpdr ap vorhdg buttons brightness`; your top-level pages are `@<id>`.
  A page the list does not name comes after the ones it names, in the usual order (before Buttons and Brightness
  when those are named). With neither line the usual order applies and nothing is skipped.
- Rows: `text "words"`, `value "label" "format" dataref` (the format has one `%d`, `%f`, `%e` or `%g`, with an
  optional width and decimals, for example `%.0f kg`; `name[3]` reads one item of a list) and `link "label" page-id`.
- On the screen the cursor `>` is on one row (the right wheel moves it, up and down; the screen scrolls to
  keep it in view). Pressing the wheel on a link opens its page; inside a sub-page the first row is `< Back`.
  Leaving a page with the left wheel forgets where you were. A value that cannot be read shows `--`.
- To get the default screen back, use **Restore default pages...** or delete the `page`, `row`, `wheel_order`
  and `wheel_hidden` lines: with none, only the built-in pages remain (radios, Buttons and Brightness).
- The Function, Start/Stop and Reset buttons under the screen are not used for going back, because the stick
  itself uses them for its clocks and stopwatch.

## Programming buttons outside X-Plane

The **Buttons** tab of the menu bar app does what the X-Plane window's button editor does, without
starting the simulator. Pick the mode (or mode + pinkie) at the top, click a button in the list,
and add steps under **When pressed**, **When held** and **When released**: **+ Command**, **+ Keystroke**,
**+ Dataref** and **+ Wait**. While you type a command or dataref, the app suggests matches from
X-Plane's own lists (`Resources/plugins/Commands.txt` and `DataRefs.txt`), so no sim is needed. The aircraft's own commands and datarefs are in the search too: they are read from the aircraft's xlua scripts (`Aircraft/**/plugins/xlua/scripts`, with the aircraft's name shown), and from the DataRefTool lists of X-Plane's last run (`Output/preferences/drt_last_run_commandrefs.txt` and `drt_last_run_datarefs.txt`, names only), when you have that plugin.
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
leave unprogrammed here keep working through X-Plane as before. Axes stay with
X-Plane unless you tick the override on the **Axes** tab (below). The main POV hat (POV 1) is four buttons (40 to 43; a
diagonal holds two) that look around by default (X-Plane's rotate-view commands, the same as the Q, E, R and F keys); if X-Plane also uses that hat, set it to None there. The throttle's mouse nub is eight more
buttons (44 to 47: X-Plane's pan commands, the same as the arrow keys; 48 to 51: the fast pan, the same as Shift + the arrow
keys, when the nub is pushed all the way); set the nub to None in X-Plane too.

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
  **Fast turns jump:** while changing heading, altitude, vertical speed, speed, OBS or a fine COM/NAV
  value, spinning the wheel quickly repeats each click (up to x10 by default: HDG moves 10 degrees, ALT 1000 ft),
  while slow turns still step by one. Tune or switch it off on the Settings tab (profile keys `dial_*`).
- **Axes tab:** shows every control the stick reports with its value, and names the one that is moving, the
  rotaries and slider included. X-Plane stays in charge of the flight axes by default. Tick *Process the flight
  axes in this plugin* and the plugin reads roll, pitch, twist and throttle itself, applies a dead zone, an
  expo curve and optional inversion, and writes the result into X-Plane (it sets X-Plane's
  `override_joystick` / `override_throttles`, and releases them when you untick it). The throttle is only
  taken over once it has moved. Test mode on the Settings tab also names moving controls on the stick's screen.
  **The dials:** each profile says what the lower dial (i, by the clutch) and the E dial do: *Speed brake* (four
  steps over the dial's turn: Armed, Retracted, Half, Full), *Zoom* (X-Plane's own zoom commands; *Zoom
  strength* sets how many zoom steps a full turn gives, 96 by default, 12 to 480), a smooth lever (*Prop*,
  *Mixture*, *Rudder trim*, *Aileron trim*, *Cowl flaps*, *Carb heat*) or *Nothing*, each with *Invert*. A lever
  covers the game's range in one full turn, within the aircraft's own trim limits; *From* and *To* (percent) narrow
  it for finer control. Prop, mixture, cowl flaps and carb heat move every engine. **Soft takeover:** after loading
  an aircraft or profile, or when a button, the mouse or the autopilot moved the value, the dial does nothing until
  you turn it to X-Plane's own value, so nothing jumps; the plugin window says where to turn it. The plugin reads the dial's steady value, so its one-step flicker
  never reaches X-Plane, and acts only while you turn it: speed-brake buttons, the mouse wheel and the auto-spoilers
  still work in between. Set the dials to None in X-Plane's Joystick settings; if X-Plane still uses one for speed
  brakes or zoom, the plugin leaves that job to X-Plane and its window says so. Profile keys `rotary_lower`,
  `rotary_upper` (0 nothing, 1 speed brake, 2 zoom, 3 prop, 4 mixture, 5 rudder trim, 6 aileron trim, 7 cowl flaps,
  8 carb heat), `rotary_lower_invert`, `rotary_upper_invert`, `rotary_zoom_steps` and
  `rotary_lower_range` / `rotary_upper_range` (`<from> <to>` in percent, `0 100` by default).
  An aircraft that does not take the armed position (the Citation X) stays retracted in the first step.
- **Plugin on/off:** the button at the left of the status row (green *Plugin on*, red *Plugin off*). Off, the plugin
  does nothing in X-Plane (no buttons, axes, dials, screen or lights) and the status says "Stick connected, plugin
  off"; the app drives the stick's screen and lights instead. It takes effect within a second and lasts until you
  switch it back on (a file `plugin-off` in the profiles folder).
  While the plugin drives the stick, X-Plane's own response curve and stability augmentation are skipped, so the
  plugin stands in for them: soft default curves (expo 50 pitch, 40 roll, 30 twist), **Smoothing** (60 ms for
  small jitter; big moves such as letting go of the stick reach X-Plane at once) and a light pitch/roll
  **Damping** (30 %, capped at a few percent of the travel, near the centre only, off on the ground and with the
  autopilot on). **Calibrate stick** on the Axes tab (in the app or the X-Plane window) measures the stick's own centre and ends
  (`stick-calibration.txt` next to the profiles).
- **Undo and redo:** the arrows next to the profile picker (and Edit > Undo / Redo, Command+Z and Shift+Command+Z)
  take back and bring back your changes to the profile in use, up to 50 steps; quick edits such as a slider drag
  count as one. In the X-Plane window the same are the **Undo** and **Redo** buttons beside **Change profile...**,
  the items under Plugins > X52 Pro, and the commands `x52pro/undo` and `x52pro/redo`. Ready-made profiles are never
  changed, so there is nothing to undo on them.
- **Picking a profile in the window:** **Change profile...** (top right, next to the tabs) lists every profile,
  in two groups, **Your profiles** and **Ready-made profiles**, that fold open with **[+]** and shut with **[-]**
  (the group of the current profile starts open; a long list scrolls), and **Automatic** to go back to the
  aircraft's own or Default. A profile picked
  here, like one picked on the stick, stays until the aircraft changes; the window says "(picked)".
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

## License, credits and trademarks

- Released under the [MIT License](LICENSE). Copyright (c) 2026 Sergio Barrak.
- Uses the X-Plane Plugin SDK; its licence and notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
- USB protocol knowledge comes from the open-source [libx52](https://github.com/nirenjan/libx52) project.
- X-Plane is a trademark of Laminar Research. Logitech, Saitek and X52 are trademarks of Logitech International S.A.
  This project is independent and not affiliated with either company.

## Found it useful?

If this brings your X52 back to life on a Mac, a star on GitHub helps other simmers find it.
Found a bug? Open an issue. For security problems see [SECURITY.md](SECURITY.md).

<p align="center"><sub>
#x52 #x52pro #xplane #xplane12 #hotas #saitek #logitech #joystick #flightsim #macos #applesilicon
</sub></p>

Builds everything into `bin/` and runs the tests: `tests/test_core.cpp` (profiles, modes, hold,
macros, screen pages and cursor, screen and light protocol), `tests/test_plugin.cpp`
(the real plugin driven through a stand-in for X-Plane: window clicks, typing, button
presses) `tests/test_agent.cpp` (the menu bar app's saving rules) and `tests/test_xplane.cpp`
(finding X-Plane and installing the plugin). `make release` makes `bin/X52-Pro-<version>.zip`; the version comes from the `VERSION` file.

**Pictures and manual.** The pictures in `docs/images` are drawn by the app itself: `bin/X52Pro.app/Contents/MacOS/X52Pro
--snapshot <folder> --profile aircraft-Cessna_CitationX` writes one PNG per tab. The Word manual is made from them by
`node docs/manual/build_manual.js` (it needs the `docx` npm package).

## Layout

| Folder | What |
|---|---|
| `Install.command`, `Test Stick.command` | Double-click installer and hardware test. |
| `VERSION` | The version number (shown in the app, in X-Plane and in the download's name). |
| `src/core` | Profile format, button engine, screen/light protocol. No macOS or X-Plane code. |
| `src/mac` | USB output (IOKit), button input (HID manager), keystrokes (Quartz), finding X-Plane. |
| `src/plugin` | X-Plane glue and the programming window. |
| `src/app` | Menu bar app: C++ agent, C bridge, Swift window. |
| `tools` | `x52probe`, the hardware check tool. |
| `tests` | Unit tests. |
| `docs` | The manual (`docs/manual`), pictures (`docs/images`), wording for the page (`docs/taglines-and-keywords.md`) and design notes. |
| `third_party/XPSDK` | X-Plane plugin SDK 4.2.0 (headers and Mac link libraries). |
| `bin` | Built programs: `X52Pro.app`, `X52Pro.xpl`, `x52probe` (made by `make`). |

The USB commands and button numbering follow the open-source
[libx52](https://github.com/nirenjan/libx52) project's documentation of the device.

## License

[MIT](LICENSE). The bundled X-Plane SDK keeps its own licence; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Not affiliated with Laminar Research,
Logitech or Saitek.
