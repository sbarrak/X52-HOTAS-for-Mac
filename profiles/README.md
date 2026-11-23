# Ready-made profiles for the X-Plane 12 aircraft

One X52 Pro profile per aircraft that comes with X-Plane 12, plus a printable button map
(`mapping/<aircraft> - X52 Pro map.pdf`, A4 landscape) for each.

## Modes

| Dial | Mode | What the buttons are for |
|---|---|---|
| 1 | Takeoff | battery, avionics, lights, APU or beacon, mixture / prop, gear |
| 2 | Cruise | autopilot modes, altitude / heading / VS values (fighters: combat, all the weapons; no autopilot: fuel, lights, views) |
| 3 | Landing | landing lights, speedbrake, reverse on D, autobrake, approach, gear, trim on the scroll wheel |

The same in every mode: trigger (autopilot disconnect, or guns), B (autopilot on/off, one button in every
mode, on aircraft with an autopilot), Hat 2 (trim), Pinkie (wheel brakes while held), screen wheels. The pinkie
is a normal button in these profiles, not a shift. **A:** a tap is the gear (takeoff and landing; cruise has no gear
button, so A is the landing light there) and **holding A** sets the parking brake, in every mode. **Fire** is
free, except in the fighters, where it shoots. When the mode dial moves, the stick's screen says the mode's name
for 1.5 s (`mode_name_1..3`, `mode_banner_ms`; editable on the app's Screen tab).

**Flaps are on the slider** on the throttle (`slider_flaps = 1`, `slider_invert = 0`): pushed away from you is
flaps deployed, towards you flaps up, and the flap lever goes to the aircraft's own stops. **Invert flaps** on
the app's Axes tab reverses the direction for a profile. **B and C:**

| Aircraft | Mode 1 | Mode 2 | Mode 3 |
|---|---|---|---|
| With an autopilot | B AP on/off (every mode), C flight director | B AP on/off, C AP ALT hold | B AP on/off, C AP approach |
| Fighters | B speedbrake in, C speedbrake out | same | same |
| Light pistons without autopilot (PA-18, L-5, Aerolite) | B magnetos both, C starter (hold) | same | same |
| Glider | B airbrakes in, C airbrakes out | same | same |
| R22 | B governor, C carb heat | same | same |

Lights: mode 1 green, mode 2 amber, mode 3 red; the throttle light is green in every mode. Live lights show
parking brake (Fire), gear (A) and flaps (B); fighters: master arm (Fire). Alarms: engine fire, stall / low rotor RPM, gear unsafe,
master warning / caution, low fuel, autopilot disconnect, overspeed, low oil pressure.
The screen gets Flight, Engine and Fuel pages after the built-in radio pages.

## Installing

The plugin picks a profile by the aircraft's `.acf` file name: `aircraft-<acf name>.x52` in
`X-Plane 12/Output/preferences/X52Pro/`. Copy the `.x52` files there. They never change once
installed, so copying a newer version over one is safe; your own changes live in your copies. Then, in X-Plane, set every button these profiles use to **Do nothing**
(Settings > Joystick), or X-Plane and the plugin both act on it.

Flight axes: the plugin drives them, because X-Plane's own axis assignment did not reach the controls (seen
live in the Citation X: X-Plane saw the stick and throttle move, but the yoke and throttle stayed at 0).

- Stick and twist (`axis_override = 1`, 5 % dead zone) in every aircraft.
- Throttle (`throttle_plugin = 1`): engine power and the cockpit levers follow the X52 lever, set the way the
  X52 reports it (`axis_throttle = 5 0 1`: inverted, 5 % dead zone), taken over once it has moved. The
  helicopters leave the lever to X-Plane (`throttle_plugin = 0`): there it is the collective.
- The slider is the flaps (above). If X-Plane also uses the slider for something, set it to None in X-Plane's
  Joystick settings. **The dials:** the lower dial (i, by the clutch) is the speed brake in four steps (Armed,
  Retracted, Half, Full) in the aircraft that have one (Citation X, 737, A330, MD-82, F-4, F-14; `rotary_lower = 1`),
  nothing elsewhere; the E dial zooms the view everywhere (`rotary_upper = 2`, `rotary_zoom_steps = 96`: zoom steps per full turn). Either dial can instead be a smooth lever
  (prop, mixture, trims, cowl flaps, carb heat: `rotary_<lower|upper>` 3 to 8) over the part of the game's range set
  by `rotary_<lower|upper>_range = 0 100`. Set both dials to None in X-Plane's
  Joystick settings. **POV 1** (the main hat on top of the stick) looks around in every mode (buttons 40 to 43, X-Plane's rotate-view commands: the same as the Q, E, R and F keys); set that hat to None in X-Plane's Joystick settings, and hold the clutch to use it as the profile picker instead. The **mouse nub** on the throttle pans the view in every mode (X-Plane's pan commands, the same as the arrow keys; pushed all the way, the fast pan, the same as Shift + arrow keys; buttons 44 to 51).

All of it can be changed per profile on the app's Axes tab (dead zone, expo, invert, Invert flaps, what each dial does).

## Changing them

These profiles are ready-made (`standard = 1`): the app and the plugin never change them, so the original is
always there to go back to. Change one in the menu bar app and it asks for a name and saves your change as a new
profile, which X-Plane then uses for that aircraft; in X-Plane, **Save as my copy** does the same (see the main
README). To change the layout for all aircraft, edit `tools/aircraft.py` and run:

```bash
python3 tools/build_profiles.py
python3 tools/build_sheets.py
```

`build_sheets.py` needs `reportlab` and reads the aircraft pictures from the X-Plane folder. The PDFs
contain those pictures (Laminar Research's artwork), so keep `mapping/` out of a public repository.
