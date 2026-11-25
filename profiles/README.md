# Ready-made profiles for the X-Plane 12 aircraft

One X52 Pro profile per aircraft that comes with X-Plane 12, plus a printable button map
(`mapping/<aircraft> - X52 Pro map.pdf`, A4 landscape) for each.

## Modes

| Dial | Mode | What the buttons are for |
|---|---|---|
| 1 | Takeoff | battery, avionics, APU, beacon, exterior lights, gear |
| 2 | Cruise | autopilot modes and their targets (fighters: combat, all the weapons; no autopilot: fuel, views) |
| 3 | Landing | exterior lights, reverse, autobrake, spoilers, TOGA, approach, gear, trim on the scroll wheel |

### One layout for the civil aircraft (1.6.4, issue #121)

A control is in the same place and does the same thing in every civil aircraft that has the function. Where an
aircraft does not have it, the button is left empty rather than given something else.

| Control | Takeoff (1) | Cruise (2) | Landing (3) |
|---|---|---|---|
| E (held) | wheel brakes | wheel brakes | wheel brakes |
| A tap / hold | gear / parking brake | - / parking brake | gear / parking brake |
| B | autopilot on/off | same | same |
| Trigger (1st / 2nd stage) | AP disconnect / Contact ATC | same | same |
| C | flight director | AP approach | AP approach |
| D | beacon | AP wing leveler | reverse (held) |
| T1 up / down | battery on / off | AP HDG / NAV | autobrake + / - (no autobrake: HDG / NAV) |
| T2 up / down | avionics on / off | AP ALT hold / VS | spoilers arm / retract (others: fuel pumps) |
| T3 up / down | APU start / off | autothrottle / level change (airliners), FLC | TOGA (airliners), pitot heat |
| Throttle hat up, right, down, left | landing, taxi, strobes, nav lights | ALT select + / speed + (airliners) or VS + / ALT - / speed - or VS - | the same lights as takeoff |
| Scroll wheel | zoom | heading bug, click: HDG sync | pitch trim, click: takeoff trim |
| Hat 2 | pitch trim, rudder trim (helicopters: cyclic trim) | same | same |

- **Wheel brakes on E** begin when E goes down and end when it comes up (no hold delay), and stay on across a mode
  or pinkie change until E is let go. The parking brake is each aircraft's own control (A held).
- **Gliders and ultralights:** the ASK 21's A tap is the tow release (`glider_all_release`, as in its cockpit), B and C
  the airbrakes. The Aerolite has no lights; A held is its parking brake, T1 its master switch.
- **Light selectors** with three positions (737 position and taxi, MD-82 nav/strobe, nose and landing lights, A330
  strobe, nav & logo and nose, Citation X anti-collision): a tap moves the selector one position up, a hold one down.
  Landing lights made of several lamps go on or off together.
- **The pinkie is the Shift** in every mode: each shifted mode is the same as the mode, except the scroll wheel, which
  zooms, and these:

| Pinkie + | Takeoff and landing | Cruise |
|---|---|---|
| Throttle hat | extra lights where the aircraft has them: up runway turnoff (or search light), right wing / ice light, down recognition, left logo | airliners: right / left = VS + / - |
| C | | flight director |
| T3 | R22: rotor RPM (takeoff); airliners: pitot heat (landing) | |

- **The dials** do the same in every mode. The lower dial (i, by the clutch) is the prop (RV-10, Baron, King Air,
  Lancair), the speed brake (Citation X, 737, A330, MD-82, F-4, F-14), the airbrakes (ASK 21) or the carb heat
  (PA-18, L-5). The E dial is the mixture (Skyhawk, PA-18, L-5, RV-10, SR22, Baron, Aerolite; it stops at 5 %, so a
  dial turned right down does not cut the engine), the rudder trim (King Air, Citation X, 737, A330, MD-82) or the
  carb heat (R22), and zooms elsewhere. A dial takes over a lever only once you turn it.

**Fire** is free, except in the fighters: the F-4 drops bombs and its trigger is the guns; the F-14 trigger selects the Gun while it is held and puts the previous weapon back when let go (the second pull fires the guns), the throttle hat in mode 2 picks the weapon (up Sidewinder, right Sparrow, down Gun, left Phoenix) and Fire fires the selected weapon. If the previous weapon has run out, it cannot be selected again and the Gun stays selected until you pick another weapon with the hat. In the F-14 hat 2 is the weapon select (up, right, down, left = Sidewinder, Sparrow, Gun, Phoenix), so the trims are on it only with the pinkie held in combat (pitch up and down, rudder left and right); in the fighters the pinkie is the wheel brakes in the other modes and A taps the gear in takeoff and landing. Holding Pinkie in Combat changes only H2 to trim; every other Combat control, including A tap/hold, stays available. When the mode dial moves, the stick's screen says the mode's name
for 1.5 s (`mode_name_1..3`, `mode_banner_ms`; editable on the app's Screen tab).

**Flaps are on the slider** on the throttle (`slider_flaps = 1`, `slider_invert = 0`): pushed away from you is
flaps deployed, towards you flaps up, and the flap lever goes to the aircraft's own stops. **Invert flaps** on
the app's Axes tab reverses the direction for a profile. The fighters keep B speedbrake in and C speedbrake out.

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
  Joystick settings. **The dials** (see the table above): the speed brake goes in four steps (Armed,
  Retracted, Half, Full; `rotary_<lower|upper> = 1`), zoom is `2` (`rotary_zoom_steps = 96`: zoom steps per full
  turn), and the smooth levers (prop, mixture, rudder and aileron trim, cowl flaps, carb heat: `3` to `8`) cover the
  part of the game's range set by `rotary_<lower|upper>_range` (the mixture `5 100`). Set both dials to None in X-Plane's
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
