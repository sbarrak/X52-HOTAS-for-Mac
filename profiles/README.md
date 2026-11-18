# Ready-made profiles for the X-Plane 12 aircraft

One X52 Pro profile per aircraft that comes with X-Plane 12, plus a printable button map
(`mapping/<aircraft> - X52 Pro map.pdf`, A4 landscape) for each.

## Modes

| Dial | Mode | What the buttons are for |
|---|---|---|
| 1 | Ground & take-off | battery, avionics, lights, APU or beacon, mixture / prop |
| 2 | Approach & landing | landing lights, speedbrake, reversers, autobrake, trim on the scroll wheel |
| 3 | Cruise & autopilot | autopilot modes, altitude / heading / VS values (fighters: combat; no autopilot: fuel, lights, views) |

The same in every mode: trigger (autopilot disconnect, or guns), Fire (parking brake), A (gear or
landing light), B / C (flaps up / down), Hat 2 (trim), Pinkie (wheel brakes while held), screen wheels.
The pinkie is a normal button in these profiles, not a shift.

Lights: mode 1 green, mode 2 amber, mode 3 red. Live lights show parking brake (Fire), gear (A),
flaps (B) and autopilot (Throttle). Alarms: engine fire, stall / low rotor RPM, gear unsafe,
master warning / caution, low fuel, autopilot disconnect, overspeed, low oil pressure.
The screen gets Flight, Engine and Fuel pages after the built-in radio pages.

## Installing

The plugin picks a profile by the aircraft's `.acf` file name: `aircraft-<acf name>.x52` in
`X-Plane 12/Output/preferences/X52Pro/`. Copy the `.x52` files there; do not replace one you have
already changed. Then, in X-Plane, set every button these profiles use to **Do nothing**
(Settings > Joystick), or X-Plane and the plugin both act on it. Axes, rotaries, the slider and the
main hat stay with X-Plane.

## Changing them

Edit a profile in the menu bar app or in X-Plane like any other. To change the layout for all
aircraft, edit `tools/aircraft.py` and run:

```bash
python3 tools/build_profiles.py
python3 tools/build_sheets.py
```

`build_sheets.py` needs `reportlab` and reads the aircraft pictures from the X-Plane folder. The PDFs
contain those pictures (Laminar Research's artwork), so keep `mapping/` out of a public repository.
