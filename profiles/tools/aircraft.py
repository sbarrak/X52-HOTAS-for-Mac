"""Ready-made X52 Pro button layouts for the aircraft that come with X-Plane 12.

One table of features per aircraft; the button layout of every mode is worked out from those
features, so all aircraft follow the same idea:

  Mode 1  Takeoff   (power, lights, engine levers, gear)
  Mode 2  Cruise    (autopilot modes and values; all the weapons for the fighters: "Combat")
  Mode 3  Landing   (flaps, speedbrake, reversers on D, autobrake, gear, approach, trim on the wheel)

The pinkie switch is wheel brakes, except in F-14 Combat: it changes H2 to trim and leaves all other controls unchanged.
Buttons marked ANY do the same thing in all three modes. The parking brake is a hold on A (tap: gear or
landing light). The autopilot is one on/off button (B) in every mode. The Fire button drops bombs in the F-4 (the F-14 has no bombs: Fire fires its selected weapon); the F-14 trigger selects the Gun while held (the weapon comes back when let go) and Fire fires the selected weapon, the F-4 trigger is the guns.
The gear is on A in takeoff and landing only: in cruise there is no gear button.
"""

# Stick button numbers as the plugin numbers them (src/core/profile.cpp, kButtonNames).
TRIGGER, FIRE, A, B, C, PINKIE, D, E = 1, 2, 3, 4, 5, 6, 7, 8
T1U, T1D, T2U, T2D, T3U, T3D = 9, 10, 11, 12, 13, 14
TRIGGER2, MOUSE, SCROLL_F, SCROLL_B, SCROLL_C = 15, 16, 17, 18, 19
SHAT_U, SHAT_R, SHAT_D, SHAT_L = 20, 21, 22, 23
POV1_U, POV1_R, POV1_D, POV1_L = 40, 41, 42, 43   # POV 1, the main hat: the stick reports a hat switch, the plugin makes buttons of it
THAT_U, THAT_R, THAT_D, THAT_L = 24, 25, 26, 27
NUB_U, NUB_R, NUB_D, NUB_L = 44, 45, 46, 47       # the throttle's mouse nub (arrows); 48..51 the same pushed all the way (Shift + arrows)
CLUTCH = 31
MFD_PREV, MFD_NEXT, MFD_UP, MFD_DOWN, MFD_SELECT = 35, 36, 37, 38, 39

ANY = 0

# ---------------------------------------------------------------- aircraft
# kind: piston, turboprop, jet, airliner, fighter, heli, glider, ultralight, electric
AIRCRAFT = [
    dict(name="Cessna 172 SP Skyhawk", folder="Cessna 172 SP", icon="Cessna_172SP_icon11.png",
         acfs=["Cessna_172SP", "Cessna_172SP_G1000", "Cessna_172SP_seaplane"],
         kind="piston", gear=False, flaps=True, ap=True, mixture=True),
    dict(name="Piper PA-18 Super Cub", folder="Piper PA-18 Super Cub", icon="PA-18-150_icon11.png",
         acfs=["PA-18-150"], kind="piston", gear=False, flaps=True, ap=False, mixture=True, carb=True, tailwheel=True),
    dict(name="Stinson L-5 Sentinel", folder="Stinson L-5 Sentinel", icon="L5_Sentinel_icon11.png",
         acfs=["L5_Sentinel"], kind="piston", gear=False, flaps=True, ap=False, mixture=True, carb=True, tailwheel=True),
    dict(name="Van's RV-10", folder="Van's Aircraft RV-10", icon="RV-10_icon11.png",
         acfs=["RV-10"], kind="piston", gear=False, flaps=True, ap=True, mixture=True, prop=True),
    dict(name="Cirrus SR22", folder="Cirrus SR22", icon="Cirrus SR22_icon11.png",
         acfs=["Cirrus SR22"], kind="piston", gear=False, flaps=True, ap=True, mixture=True),
    # Flaps: its cockpit has a flap lever (flaps_up / flaps_down) and the .acf two detents (12 and 24 degrees).
    # No carb heat control in its cockpit.
    dict(name="Aero-Works Aerolite 103", folder="Aero-Works Aerolite 103", icon="Aerolite_103_icon11.png",
         acfs=["Aerolite_103"], kind="ultralight", gear=False, flaps=True, ap=False, mixture=True, master=False),
    dict(name="BETA Technologies Alia-250", folder="BETA Technologies Alia-250", icon="ALIA-250_icon11.png",
         acfs=["ALIA-250"], kind="electric", gear=False, flaps=True, ap=True),
    dict(name="Beechcraft Baron 58", folder="Beechcraft Baron 58", icon="Baron_58_icon11.png",
         acfs=["Baron_58"], kind="piston", engines=2, gear=True, flaps=True, ap=True, mixture=True, prop=True),
    dict(name="Beechcraft King Air C90B", folder="Beechcraft King Air C90B", icon="C90B_icon11.png",
         acfs=["C90B"], kind="turboprop", engines=2, gear=True, flaps=True, ap=True, prop=True, rev=True, caution=True),
    dict(name="Lancair Evolution", folder="Lancair Evolution", icon="N844X_icon11.png",
         acfs=["N844X"], kind="turboprop", gear=True, flaps=True, ap=True, prop=True),
    dict(name="Cirrus Vision SF50", folder="Cirrus Vision SF50", icon="CirrusSF50_icon11.png",
         acfs=["CirrusSF50"], kind="jet", gear=True, flaps=True, ap=True, caution=True),
    dict(name="Cessna Citation X", folder="Cessna Citation X", icon="Cessna_CitationX_icon11.png",
         acfs=["Cessna_CitationX"], kind="airliner", engines=2, gear=True, flaps=True, ap=True, at=True,
         speedbrake=True, rev=True, apu=True, caution=True),
    dict(name="Boeing 737-800", folder="Boeing 737-800", icon="b738_icon11.png",
         acfs=["b738"], kind="airliner", engines=2, gear=True, flaps=True, ap=True, at=True,
         speedbrake=True, rev=True, apu=True, autobrake=True, caution=True),
    dict(name="Airbus A330-300", folder="Airbus A330-300", icon="A330_icon11.png",
         acfs=["A330"], kind="airliner", engines=2, gear=True, flaps=True, ap=True, at=True,
         speedbrake=True, rev=True, apu=True, autobrake=True, caution=True),
    dict(name="McDonnell Douglas MD-82", folder="McDonnell Douglas MD-82", icon="MD80_icon11.png",
         acfs=["MD80"], kind="airliner", engines=2, gear=True, flaps=True, ap=True, at=True,
         speedbrake=True, rev=True, apu=True, autobrake=True, caution=True),
    dict(name="McDonnell Douglas F-4 Phantom II", folder="McDonnell Douglas F-4 Phantom II", icon="F-4_icon11.png",
         acfs=["F-4"], kind="fighter", engines=2, gear=True, flaps=True, ap=False, speedbrake=True, hook=True,
         caution=True),
    dict(name="Grumman F-14 Tomcat", folder="Grumman F-14 Tomcat", icon="F-14D_icon11.png",
         acfs=["F-14D"], kind="fighter", engines=2, gear=True, flaps=True, ap=False, speedbrake=True, hook=True,
         sweep=True, caution=True, f14=True),
    dict(name="Robinson R22 Beta II", folder="Robinson R22 Beta II", icon="Robinson R22 Beta II_icon11.png",
         acfs=["Robinson R22 Beta II", "Robinson R22 Beta II_mariner"], kind="heli", gear=False, flaps=False,
         ap=False, carb=True, governor=True),
    dict(name="Sikorsky S-76C", folder="Sikorsky S-76", icon="S-76C_icon11.png",
         acfs=["S-76C"], kind="heli", engines=2, gear=True, flaps=False, ap=True, caution=True),
    dict(name="Schleicher ASK 21", folder="Schleicher ASK 21", icon="ASK21_icon11.png",
         acfs=["ASK21"], kind="glider", gear=False, flaps=False, ap=False, master=False),
]

# ---------------------------------------------------------------- civil aircraft: what each cockpit really has
# Checked in each aircraft's cockpit objects (ATTR_manip_* commands and their tooltips) and .acf (issue #121).
# A light is one of:
#   ("tog", label, command)                       one switch: a tap toggles it
#   ("grp", label, dataref, [on...], [off...])    several lamps set together: they follow the first lamp's switch
#                                                 (off -> all on, on -> all off), so mixed states end up the same
#   ("pair", label, [(switch dataref, toggle)...]) several lamps with toggle commands only: tap = each one that is
#                                                 off goes on, hold = each one that is on goes off
#   ("step", label up, command up, label down, command down)
#                                                 a selector with three positions: tap = one up, hold = one down
# A slot left out stays empty: a blank slot is better than a button that does something else.
SW = "sim/cockpit2/switches/"


def _tog(label, c):
    return ("tog", label, c if c.startswith("laminar/") else "sim/lights/" + c)


GENERIC_LIGHTS = dict(up=_tog("Landing lights", "landing_lights_toggle"), right=_tog("Taxi light", "taxi_lights_toggle"),
                      down=_tog("Strobes", "strobe_lights_toggle"), left=_tog("Nav lights", "nav_lights_toggle"),
                      beacon=_tog("Beacon", "beacon_lights_toggle"))


def _lights(drop=(), **extra):
    d = {k: v for k, v in GENERIC_LIGHTS.items() if k not in drop}
    d.update(extra)
    return d


def _landing_group(*lamps, first=None):
    """Landing lamps that have their own on/off commands (landing_0N_light_on/off), set together."""
    idx = (first if first is not None else lamps[0]) - 1
    return ("grp", "Landing lights", SW + "landing_lights_switch[%d]" % idx,
            ["sim/lights/landing_%02d_light_on" % n for n in lamps], ["sim/lights/landing_%02d_light_off" % n for n in lamps])


# Autothrottle: arm the A/T switch first (nothing happens when it is armed), then engage / disengage (tested in the 737).
A333_PROBE = "laminar/A333/buttons/window_probe_ice"
GEAR_HANDLE = "sim/cockpit2/controls/gear_handle_down"
B738_GUARD = "laminar/B738/button_switch/guard_cover_pos[0]"
ARM_TOGGLE = ["sim/autopilot/autothrottle_arm", "sim/autopilot/autothrottle_toggle"]

CIVIL = {
    # The KAP140 autopilot (the classic and the seaplane) has no FLC; the G1000's GFC700 has (tested).
    "Cessna 172 SP Skyhawk": dict(lights=_lights(), park="sim/flight_controls/brakes_toggle_max", avionics=True,
                                  no_flc=("Cessna_172SP", "Cessna_172SP_seaplane"), dial_e="mixture"),
    "Piper PA-18 Super Cub": dict(lights=_lights(drop=("down",)), park="sim/flight_controls/brakes_toggle_max",
                                  avionics=True, dial_i="carb", dial_e="mixture"),
    "Stinson L-5 Sentinel": dict(lights=_lights(drop=("right",)), park="sim/flight_controls/brakes_toggle_max",
                                 dial_i="carb", dial_e="mixture"),
    "Van's RV-10": dict(lights=_lights(drop=("beacon",), up=_landing_group(1)), park="sim/flight_controls/park_brake_toggle",
                        avionics=True, dial_i="prop", dial_e="mixture"),
    "Cirrus SR22": dict(lights=_lights(drop=("right", "beacon"), up=_landing_group(1),
                                       x_right=_tog("Wing inspection", "generic_13_light_tog")),
                        park="sim/flight_controls/park_brake_toggle", avionics=True, dial_e="mixture"),
    # No exterior lights and no parking-brake control in the cockpit; its master switch has its own commands.
    "Aero-Works Aerolite 103": dict(lights={}, park="sim/flight_controls/brakes_toggle_max",
                                    battery=("laminar/aerolite/switch/master_on", "laminar/aerolite/switch/master_off", "Master"),
                                    dial_e="mixture"),
    "BETA Technologies Alia-250": dict(lights=_lights(drop=("right",), x_up=_tog("Search light", "generic_01_light_tog")),
                                       park="sim/flight_controls/brakes_toggle_max"),
    "Beechcraft Baron 58": dict(lights=_lights(up=_landing_group(1, 2), x_right=_tog("Wing ice light", "generic_01_light_tog")),
                                park="sim/flight_controls/park_brake_toggle", avionics=True, dial_i="prop", dial_e="mixture"),
    "Beechcraft King Air C90B": dict(lights=_lights(up=_landing_group(1, 2), x_right=_tog("Wing ice light", "generic_02_light_tog"),
                                                    x_down=_tog("Recognition", "generic_03_light_tog")),
                                     park="sim/flight_controls/park_brake_toggle", avionics=True, dial_i="prop", dial_e="rudder"),
    "Lancair Evolution": dict(lights=_lights(drop=("right", "beacon"), up=_landing_group(1)),
                              park="sim/flight_controls/brakes_toggle_max", dial_i="prop"),
    "Cirrus Vision SF50": dict(lights=_lights(drop=("right", "beacon")), park="sim/flight_controls/park_brake_toggle"),
    # Ground recognition / off / anti-collision is one selector (so no separate beacon on D).
    # Tested: its landing lights follow only its own L / R toggles; the park handle is a lever (no command);
    # the autothrottle and spoiler arm commands do nothing in this model.
    "Cessna Citation X": dict(lights=dict(up=("pair", "Landing lights", [(SW + "landing_lights_switch[0]", "laminar/CitX/lights/cmd_landing_left_toggle"),
                                                                        (SW + "landing_lights_switch[1]", "laminar/CitX/lights/cmd_landing_right_toggle")]),
                                          right=_tog("Taxi light", "laminar/CitX/lights/cmd_taxi_toggle"),
                                          down=("step", "Anti-coll sel up", "laminar/CitX/lights/cmd_gnd_rec_anti_coll_up",
                                                "Anti-coll sel down", "laminar/CitX/lights/cmd_gnd_rec_anti_coll_dwn"),
                                          left=_tog("Nav lights", "laminar/CitX/lights/cmd_navigation_toggle"),
                                          x_down=_tog("Recognition", "laminar/CitX/lights/cmd_recognition_toggle"),
                                          x_right=_tog("Wing lights", "laminar/CitX/ice/cmd_wing_light_toggle")),
                              park="sim/flight_controls/park_brake_toggle", avionics=True, athr=None, spoiler_arm=False,
                              dial_i="speedbrake", dial_e="rudder"),
    # Four landing lights (wing root L/R, belly L/R) with an all-on spring switch; position (STROBE & STEADY / OFF /
    # STEADY) and taxi are selectors; generic lights 1 wing, 2 logo, 3/4 runway turnoff L/R.
    "Boeing 737-800": dict(lights=dict(up=("grp", "Landing lights", SW + "landing_lights_switch[0]",
                                           ["laminar/B738/spring_switch/landing_lights_all"], ["sim/lights/landing_lights_off"]),
                                       right=("step", "Taxi light +", "laminar/B738/toggle_switch/taxi_light_brightness_pos_dn",
                                              "Taxi light -", "laminar/B738/toggle_switch/taxi_light_brightness_pos_up"),
                                       left=("step", "Position sel up", "laminar/B738/toggle_switch/position_light_up",
                                             "Position sel down", "laminar/B738/toggle_switch/position_light_down"),
                                       beacon=_tog("Beacon", "beacon_lights_toggle"),
                                       x_up=("pair", "Runway turnoff", [(SW + "generic_lights_switch[2]", "sim/lights/generic_03_light_tog"),
                                                                         (SW + "generic_lights_switch[3]", "sim/lights/generic_04_light_tog")]),
                                       x_right=_tog("Wing lights", "generic_01_light_tog"),
                                       x_left=_tog("Logo light", "generic_02_light_tog")),
                           park="sim/flight_controls/park_brake_toggle", athr=ARM_TOGGLE, dial_i="speedbrake", dial_e="rudder",
                           # The battery switch has a guard: off opens it first; closing it puts the switch back on.
                           battery=(["sim/electrical/battery_1_on", "sim/electrical/battery_2_on", "save:" + B738_GUARD,
                                     "restoreif:%s=0.5,1,laminar/B738/guard_cover00" % B738_GUARD],
                                    ["save:" + B738_GUARD, "restoreif:%s=0,0.5,laminar/B738/guard_cover00" % B738_GUARD,
                                     "wait:1000", "sim/electrical/battery_1_off", "sim/electrical/battery_2_off"], "Battery"),
                           # The APU switch is OFF / ON / START (spring back to ON): a tap moves it down a notch, so tap
                           # for ON, then hold for START; T3 down moves it up a notch (ON to OFF).
                           # The gear lever is UP / OFF / DOWN (-1 is OFF), so a toggle needs two taps from OFF to
                           # DOWN: a tap puts it UP when it is down and DOWN otherwise.
                           gear=["save:" + GEAR_HANDLE,
                                 "restoreif:%s=0.5,1.5,sim/flight_controls/landing_gear_up" % GEAR_HANDLE,
                                 "restoreif:%s=-0.4,0.4,sim/flight_controls/landing_gear_down" % GEAR_HANDLE,
                                 "restoreif:%s=-1.5,-0.6,sim/flight_controls/landing_gear_toggle" % GEAR_HANDLE],
                           apu=("laminar/B738/spring_toggle_switch/APU_start_pos_dn",
                                "laminar/B738/spring_toggle_switch/APU_start_pos_up")),
    # Strobe OFF / AUTO / ON, NAV & LOGO and nose OFF / TAXI / TO are selectors; generic 1 runway turnoff, 2 wing.
    "Airbus A330-300": dict(lights=dict(up=_landing_group(1),
                                        right=("step", "Nose sel up", "laminar/A333/switch/lighting/taxi_light_up",
                                               "Nose sel down", "laminar/A333/switch/lighting/taxi_light_dn"),
                                        down=("step", "Strobe sel up", "laminar/A333/toggle_switch/strobe_pos_up",
                                              "Strobe sel down", "laminar/A333/toggle_switch/strobe_pos_dn"),
                                        left=("step", "Nav & logo up", "laminar/A333/toggle_switch/nav_light_pos_up",
                                              "Nav & logo down", "laminar/A333/toggle_switch/nav_light_pos_dn"),
                                        beacon=_tog("Beacon", "beacon_lights_toggle"),
                                        x_up=_tog("Runway turnoff", "generic_01_light_tog"),
                                        x_right=_tog("Wing lights", "generic_02_light_tog")),
                            park="sim/flight_controls/brakes_toggle_max", fd="sim/autopilot/fdir_command_bars_toggle",
                            athr="laminar/A333/autopilot/a_thr_toggle", dial_i="speedbrake", dial_e="rudder",
                            # The APU has push buttons that need a real press: MASTER SW (on/off) on T3 down, START up.
                            apu=("laminar/A333/buttons/APU_start", "laminar/A333/buttons/APU_master",
                                 "APU start", "APU master on/off"),
                            # X-Plane's pitot heat commands do nothing here: the PROBE/WINDOW HEAT button is ON or
                            # AUTO (the probes are heated automatically with the engines running).
                            pitot=(["save:" + A333_PROBE, "restoreif:%s=0,0.5,laminar/A333/buttons/probe_window_heat_toggle" % A333_PROBE],
                                   ["save:" + A333_PROBE, "restoreif:%s=0.5,1,laminar/A333/buttons/probe_window_heat_toggle" % A333_PROBE],
                                   "Probe heat ON", "Probe heat AUTO")),
    # Wing landing lights L/R and the nose light are selectors (up / down); nav and strobe are one selector;
    # generic 1 tail logo, 4 wings and engines.
    # Tested: their "up" commands move towards OFF / RETRACT, so a tap uses "dwn" (lights on) and a hold "up".
    "McDonnell Douglas MD-82": dict(lights=dict(up=("step", "Landing lts +", ["laminar/md82cmd/switches/landing_lights_switch_L_dwn",
                                                                         "laminar/md82cmd/switches/landing_lights_switch_R_dwn"],
                                                     "Landing lts -", ["laminar/md82cmd/switches/landing_lights_switch_L_up",
                                                                       "laminar/md82cmd/switches/landing_lights_switch_R_up"]),
                                                right=("step", "Nose light +", "laminar/md82cmd/switches/landing_lights_switch_nose_dwn",
                                                       "Nose light -", "laminar/md82cmd/switches/landing_lights_switch_nose_up"),
                                                left=("step", "Nav/strobe +", "laminar/md82cmd/switches/navstrobe_lights_switch_dwn",
                                                      "Nav/strobe -", "laminar/md82cmd/switches/navstrobe_lights_switch_up"),
                                                beacon=_tog("Beacon", "beacon_lights_toggle"),
                                                x_right=_tog("Wing lights", "generic_04_light_tog"),
                                                x_left=_tog("Logo light", "generic_01_light_tog")),
                                    park="sim/flight_controls/brakes_toggle_max", athr=ARM_TOGGLE, dial_i="speedbrake", dial_e="rudder"),
    "Robinson R22 Beta II": dict(lights=_lights(drop=("right", "beacon")), park=None, dial_e="carb"),
    "Sikorsky S-76C": dict(lights=_lights(up=_landing_group(2, 3), x_right=_tog("Search light", "generic_01_light_tog")),
                           park="sim/flight_controls/brakes_toggle_max", avionics=True),
    # A glider: no lights, no parking brake; the airbrake handle is the speed brake.
    "Schleicher ASK 21": dict(lights={}, park=None, avionics=True, dial_i="speedbrake"),
}

# For profiles/default.x52 (any aircraft without its own profile): X-Plane's generic commands.
GENERIC = dict(name="Any aircraft", kind="airliner", gear=True, flaps=True, ap=True, at=False, rev=True, autobrake=True,
               speedbrake=True)
CIVIL["Any aircraft"] = dict(lights=_lights(), park="sim/flight_controls/brakes_toggle_max", avionics=True)

DIAL_USE = {None: 0, "speedbrake": 1, "zoom": 2, "prop": 3, "mixture": 4, "rudder": 5, "carb": 8}

DEFAULTS = dict(engines=1, gear=False, flaps=False, ap=False, at=False, mixture=False, prop=False, carb=False,
                tailwheel=False, rev=False, speedbrake=False, apu=False, autobrake=False, caution=False,
                hook=False, sweep=False, governor=False, master=True)


def features(ac):
    f = dict(DEFAULTS)
    f.update(ac)
    return f


def cmd(c):
    return "cmd:" + c


def S(c):
    return "cmd:sim/" + c


def mode_titles(f):
    if f["kind"] == "fighter":
        return {1: "Takeoff", 2: "Combat", 3: "Landing"}
    if f["kind"] == "glider":
        return {1: "Launch & tow", 2: "Soaring", 3: "Approach & landing"}
    return {1: "Takeoff", 2: "Cruise & autopilot" if f["ap"] else "Cruise & systems", 3: "Landing"}


def mode_names(f):
    """What the stick's screen says after the mode dial moves (editable in the app)."""
    if f["kind"] == "fighter":
        return {1: "Takeoff", 2: "Combat", 3: "Landing"}
    if f["kind"] == "glider":
        return {1: "Launch", 2: "Soaring", 3: "Landing"}
    return {1: "Takeoff", 2: "Cruise", 3: "Landing"}


def _steps(c):
    """Commands to steps. An item that is already a step (save:, restoreif:, wait:, set:, cmd:) is kept as it is."""
    keep = ("save:", "restoreif:", "wait:", "set:", "cmd:")
    return " ; ".join(x if x.startswith(keep) else "cmd:" + x for x in ([c] if isinstance(c, str) else c))


def civil(f):
    """The shared civil layout (issue #121). Returns (L, H): {layer: {button: (label, steps)}} for the presses and
    {layer: {button: (label, steps)}} for the holds. Layer 0 is every mode, 1-3 the modes, 4-6 the modes with the
    pinkie held. A shifted layer starts as a full copy of its mode (a shifted binding falls back to Any mode, not
    to the unshifted mode), then changes only the buttons meant to change."""
    k = f["kind"]
    c = CIVIL[f["name"]]
    lt = c["lights"]
    L = {layer: {} for layer in range(7)}
    H = {layer: {} for layer in range(7)}
    any_, m1, m2, m3 = L[ANY], L[1], L[2], L[3]
    airliner = f["at"]   # a dedicated speed selector and TOGA

    def put(layer, button, spec, hold=None):
        """spec: a light (see CIVIL) or (label, steps)."""
        if spec is None:
            return
        kind = spec[0]
        if kind == "tog":
            L[layer][button] = (spec[1], _steps(spec[2]))
        elif kind == "grp":
            _, label, ref, on, off = spec
            steps = ["save:" + ref] + ["restoreif:%s=0,0.5,%s" % (ref, x) for x in on] + \
                    ["restoreif:%s=0.5,1,%s" % (ref, x) for x in off]
            L[layer][button] = (label, " ; ".join(steps))
        elif kind == "pair":
            _, label, lamps = spec
            saves = ["save:" + ref for ref, _ in lamps]
            L[layer][button] = (label + " on", " ; ".join(saves + ["restoreif:%s=0,0.5,%s" % (r, t) for r, t in lamps]))
            H[layer][button] = (label + " off", " ; ".join(saves + ["restoreif:%s=0.5,1,%s" % (r, t) for r, t in lamps]))
        elif kind == "step":
            _, lu, cu, ld, cd = spec
            L[layer][button] = (lu, _steps(cu))
            H[layer][button] = (ld, _steps(cd))
        else:
            L[layer][button] = spec
        if hold:
            H[layer][button] = hold

    # ---- every mode
    if f["ap"]:
        any_[TRIGGER] = ("AP disconnect", S("autopilot/servos_off_any"))
        any_[TRIGGER2] = ("Contact ATC", S("operation/contact_atc"))   # stage 2 is only reached through stage 1
        any_[B] = ("AP on/off", S("autopilot/servos_toggle"))
    else:
        any_[TRIGGER] = ("Contact ATC", S("operation/contact_atc"))
    any_[E] = ("Brakes (hold)", S("flight_controls/brakes_regular"))   # down = brakes on, up = off: no hold delay
    if k == "glider":
        any_[B] = ("Airbrakes in", S("flight_controls/speed_brakes_up_one"))
        any_[C] = ("Airbrakes out", S("flight_controls/speed_brakes_down_one"))
    elif f["governor"]:
        any_[B] = ("Governor", S("engines/governor_toggle"))
        any_[C] = ("Carb heat", S("engines/carb_heat_toggle"))
    elif k in ("piston", "ultralight") and not f["ap"]:
        any_[B] = ("Magnetos both", S("magnetos/magnetos_both_1"))
        any_[C] = ("Starter (hold)", S("starters/engage_starter_1"))
    # A: tap = gear in takeoff and landing (none in cruise), the tow release in the glider; hold = parking brake.
    park = ("Parking brake", _steps(c["park"])) if c["park"] else None
    for mode in (1, 2, 3):
        if k == "glider":
            L[mode][A] = ("Tow release", S("flight_controls/glider_all_release"))   # the cockpit's own release
        elif f["gear"] and mode != 2:
            L[mode][A] = ("Gear up/down", _steps(c.get("gear", "sim/flight_controls/landing_gear_toggle")))
        if park:
            H[mode][A] = park   # alone (no tap) on fixed gear and in cruise
    if k == "heli":
        any_[SHAT_U] = ("Cyclic trim fwd", S("flight_controls/pitch_trim_down"))
        any_[SHAT_D] = ("Cyclic trim aft", S("flight_controls/pitch_trim_up"))
        any_[SHAT_L] = ("Cyclic trim L", S("flight_controls/aileron_trim_left"))
        any_[SHAT_R] = ("Cyclic trim R", S("flight_controls/aileron_trim_right"))
    elif k != "ultralight":
        any_[SHAT_U] = ("Trim nose down", S("flight_controls/pitch_trim_down"))
        any_[SHAT_D] = ("Trim nose up", S("flight_controls/pitch_trim_up"))
        if k != "glider":
            any_[SHAT_L] = ("Rudder trim L", S("flight_controls/rudder_trim_left"))
            any_[SHAT_R] = ("Rudder trim R", S("flight_controls/rudder_trim_right"))
    any_[MOUSE] = ("Cockpit view", S("view/3d_cockpit_cmnd_look"))
    any_[POV1_U] = ("Look up", S("general/rot_up"))
    any_[POV1_R] = ("Look right", S("general/rot_right"))
    any_[POV1_D] = ("Look down", S("general/rot_down"))
    any_[POV1_L] = ("Look left", S("general/rot_left"))
    for i, d in enumerate(("up", "right", "down", "left")):
        any_[NUB_U + i] = ("Pan " + d, S("general/" + d))
        any_[NUB_U + 4 + i] = ("Pan %s fast" % d, S("general/%s_fast" % d))
    any_[MFD_PREV] = ("Screen page -", cmd("x52pro/mfd_page_prev"))
    any_[MFD_NEXT] = ("Screen page +", cmd("x52pro/mfd_page_next"))
    any_[MFD_UP] = ("Screen cursor up", cmd("x52pro/mfd_up"))
    any_[MFD_DOWN] = ("Screen cursor down", cmd("x52pro/mfd_down"))
    any_[MFD_SELECT] = ("Screen select", cmd("x52pro/mfd_select"))

    def light_hat(m):
        """The exterior lights, the same four directions in modes 1 and 3."""
        for d, b in (("up", THAT_U), ("right", THAT_R), ("down", THAT_D), ("left", THAT_L)):
            put(m, b, lt.get(d))

    def view_hat(m):
        m[THAT_U] = ("Cockpit view", S("view/3d_cockpit_cmnd_look"))
        m[THAT_D] = ("Chase view", S("view/chase"))
        m[THAT_R] = ("Circle view", S("view/circle"))
        m[THAT_L] = ("Free camera", S("view/free_camera"))

    def trim_wheel(m):
        if k == "ultralight":
            return
        m[SCROLL_F] = ("Trim nose down", S("flight_controls/pitch_trim_down"))
        m[SCROLL_B] = ("Trim nose up", S("flight_controls/pitch_trim_up"))
        if k not in ("heli", "glider"):
            m[SCROLL_C] = ("Take-off trim", S("flight_controls/pitch_trim_takeoff"))

    # ---- Mode 1: ground & takeoff
    put(1, D, lt.get("beacon"))
    if k != "glider" or c.get("avionics"):
        # Batteries 1 and 2 together (the S-76 has two; a second one that does not exist is left alone).
        on, off, name = c.get("battery", (["sim/electrical/battery_1_on", "sim/electrical/battery_2_on"],
                                          ["sim/electrical/battery_1_off", "sim/electrical/battery_2_off"], "Battery"))
        m1[T1U] = (name + " on", _steps(on))
        m1[T1D] = (name + " off", _steps(off))
    if c.get("avionics"):
        m1[T2U] = ("Avionics on", S("systems/avionics_on"))
        m1[T2D] = ("Avionics off", S("systems/avionics_off"))
    if f["apu"]:
        up, dn, lup, ldn = (tuple(c["apu"]) + ("APU start", "APU off"))[:4] if c.get("apu") else \
            ("sim/electrical/APU_start", "sim/electrical/APU_off", "APU start", "APU off")
        m1[T3U] = (lup, _steps(up))
        m1[T3D] = (ldn, _steps(dn))
    elif f["governor"]:
        m1[T3U] = ("Governor on", S("engines/governor_on"))
        m1[T3D] = ("Governor off", S("engines/governor_off"))
    elif f["tailwheel"]:
        m1[T3U] = ("Tailwheel lock", S("flight_controls/tail_wheel_lock_toggle"))
    elif k == "heli":
        m1[T3U] = ("Rotor RPM +", S("flight_controls/rotor_rpm_trim_up"))
        m1[T3D] = ("Rotor RPM -", S("flight_controls/rotor_rpm_trim_down"))
    if f["ap"]:
        m1[C] = ("Flight director", _steps(c.get("fd", "sim/autopilot/fdir_toggle")))
    light_hat(1)
    m1[SCROLL_F] = ("Zoom in", S("general/zoom_in"))
    m1[SCROLL_B] = ("Zoom out", S("general/zoom_out"))
    m1[SCROLL_C] = ("Chase view", S("view/chase"))

    # ---- Mode 2: cruise
    if f["ap"]:
        m2[C] = ("AP approach", S("autopilot/approach"))
        m2[D] = ("AP wing leveler", S("autopilot/wing_leveler"))
        m2[T1U] = ("AP HDG", S("autopilot/heading"))
        m2[T1D] = ("AP NAV", S("autopilot/NAV"))
        m2[T2U] = ("AP ALT hold", S("autopilot/altitude_hold"))
        m2[T2D] = ("AP VS", S("autopilot/vertical_speed"))
        if airliner:
            athr = c.get("athr", "sim/autopilot/autothrottle_toggle")
            if athr:
                m2[T3U] = ("Autothrottle", _steps(athr))
            m2[T3D] = ("AP level change", S("autopilot/level_change"))
        elif f.get("acf") not in c.get("no_flc", ()):
            m2[T3U] = ("AP FLC", S("autopilot/level_change"))
        m2[THAT_U] = ("ALT select +", S("autopilot/altitude_up"))
        m2[THAT_D] = ("ALT select -", S("autopilot/altitude_down"))
        if airliner:
            m2[THAT_R] = ("Speed +", S("autopilot/airspeed_up"))
            m2[THAT_L] = ("Speed -", S("autopilot/airspeed_down"))
        else:
            m2[THAT_R] = ("VS +", S("autopilot/vertical_speed_up"))
            m2[THAT_L] = ("VS -", S("autopilot/vertical_speed_down"))
        m2[SCROLL_F] = ("HDG bug +", S("autopilot/heading_up"))
        m2[SCROLL_B] = ("HDG bug -", S("autopilot/heading_down"))
        m2[SCROLL_C] = ("HDG sync", S("autopilot/heading_sync"))
    else:
        if k not in ("glider",):
            if k not in ("ultralight", "heli"):   # the R22 has carb heat on C; the Aerolite neither
                m2[D] = ("Carb heat", S("engines/carb_heat_toggle")) if f["carb"] else ("Pitot heat", S("ice/pitot_heat0_tog"))
            m2[T1U] = ("Fuel tank left", S("fuel/fuel_tank_selector_lft_one"))
            m2[T1D] = ("Fuel tank right", S("fuel/fuel_tank_selector_rgt_one"))
            m2[T2U] = ("Fuel pumps on", S("fuel/fuel_pumps_on"))
            m2[T2D] = ("Fuel pumps off", S("fuel/fuel_pumps_off"))
            if not f["carb"] and k != "ultralight":   # the carburettor aircraft never had pitot heat here (carb heat: dial, D)
                m2[T3U] = ("Pitot heat on", S("ice/pitot_heat0_on"))
                m2[T3D] = ("Pitot heat off", S("ice/pitot_heat0_off"))
        view_hat(m2)
        trim_wheel(m2)

    # ---- Mode 3: approach & landing
    if f["ap"]:
        m3[C] = ("AP approach", S("autopilot/approach"))
    if f["rev"]:
        m3[D] = ("Reverse (hold)", S("engines/thrust_reverse_hold"))
    if f["autobrake"]:
        m3[T1U] = ("Autobrake up", S("flight_controls/brakes_up_auto"))
        m3[T1D] = ("Autobrake down", S("flight_controls/brakes_dn_auto"))
    elif f["ap"]:
        m3[T1U] = ("AP HDG", S("autopilot/heading"))
        m3[T1D] = ("AP NAV", S("autopilot/NAV"))
    if f["speedbrake"] and c.get("spoiler_arm", True):
        m3[T2U] = ("Spoilers arm", S("flight_controls/speed_brakes_arm_toggle"))
        m3[T2D] = ("Spoilers retract", S("flight_controls/speed_brakes_up_all"))
    elif k in ("piston", "turboprop", "heli", "jet"):
        m3[T2U] = ("Fuel pumps on", S("fuel/fuel_pumps_on"))
        m3[T2D] = ("Fuel pumps off", S("fuel/fuel_pumps_off"))
    if airliner:
        m3[T3U] = ("TOGA", S("autopilot/take_off_go_around"))
    elif k not in ("glider", "ultralight") and not f["carb"]:   # carb heat (here before) is on a dial now
        m3[T3U] = ("Pitot heat on", S("ice/pitot_heat0_on"))
        m3[T3D] = ("Pitot heat off", S("ice/pitot_heat0_off"))
    light_hat(3)
    trim_wheel(m3)

    # ---- the pinkie: a full copy of each mode, then the changes
    for mode in (1, 2, 3):
        L[mode + 3] = dict(L[mode])
        H[mode + 3] = dict(H[mode])
        L[mode + 3][SCROLL_F] = ("Zoom in", S("general/zoom_in"))
        L[mode + 3][SCROLL_B] = ("Zoom out", S("general/zoom_out"))
        L[mode + 3].pop(SCROLL_C, None)   # zoom only: the click keeps its Any-mode job (none)
        if mode != 2:   # the extra lights, the same directions in modes 1 and 3
            for d, b in (("x_up", THAT_U), ("x_right", THAT_R), ("x_down", THAT_D), ("x_left", THAT_L)):
                if lt.get(d):
                    H[mode + 3].pop(b, None)
                    put(mode + 3, b, lt[d])
        else:
            if f["ap"]:
                L[5][C] = ("Flight director", _steps(c.get("fd", "sim/autopilot/fdir_toggle")))
            if airliner:
                L[5][THAT_R] = ("VS +", S("autopilot/vertical_speed_up"))
                L[5][THAT_L] = ("VS -", S("autopilot/vertical_speed_down"))
    if airliner:   # TOGA took T3 in landing: the pitot heat that was there is on the pinkie
        on, off, lon, loff = c.get("pitot", ("sim/ice/pitot_heat0_on", "sim/ice/pitot_heat0_off",
                                             "Pitot heat on", "Pitot heat off"))
        L[6][T3U] = (lon, _steps(on))
        L[6][T3D] = (loff, _steps(off))
    if f["kind"] == "heli" and f["governor"]:   # the R22: T3 is the governor, rotor RPM with the pinkie
        L[4][T3U] = ("Rotor RPM +", S("flight_controls/rotor_rpm_trim_up"))
        L[4][T3D] = ("Rotor RPM -", S("flight_controls/rotor_rpm_trim_down"))
    return L, H


def layout(f):
    """{layer: {button: (label, steps)}} - layer 0 is every mode."""
    k = f["kind"]
    if k != "fighter":
        return civil(f)[0]
    jetlike = k in ("jet", "airliner", "fighter")
    L = {ANY: {}, 1: {}, 2: {}, 3: {}, 5: {}}   # 5 = mode 2 + pinkie (the F-14 only)
    any_, m1, m2, m3 = L[ANY], L[1], L[2], L[3]   # m2 is cruise, m3 is landing

    # ---- every mode
    if k == "fighter":
        if f.get("f14"):
            # Trigger stage 1 remembers the selected weapon and selects the Gun; stage 2 fires the guns; letting go of stage 1
            # puts the weapon back (see releases()), so the pilot always knows what the aircraft would fire.
            any_[TRIGGER] = ("Gun while held", "save:" + F14_SELECTED + " ; cmd:laminar/f14/fltc/cs_wpn_select_switch3")
            any_[FIRE] = ("Fire weapon", "cmd:laminar/f14/wpns/fwd_weapon_trigger")
            any_[TRIGGER2] = ("Guns", S("weapons/fire_guns"))
            # Fire fires the selected weapon (the F-14's own trigger command; a held button keeps it firing). No bombs in the F-14.
        else:
            any_[TRIGGER] = ("Guns", S("weapons/fire_guns"))
            any_[TRIGGER2] = ("Fire weapon", S("weapons/fire_any_armed"))
            any_[FIRE] = ("Drop bomb", S("weapons/fire_air_to_ground"))
    elif f["ap"]:
        any_[TRIGGER] = ("AP disconnect", S("autopilot/servos_off_any"))
        any_[TRIGGER2] = ("TOGA", S("autopilot/take_off_go_around")) if f["at"] else ("Contact ATC", S("operation/contact_atc"))
    else:
        any_[TRIGGER] = ("Contact ATC", S("operation/contact_atc"))
    any_[PINKIE] = ("Brakes (hold)", S("flight_controls/brakes_regular"))
    # A: tap = gear (takeoff and landing; cruise has the landing light), hold = parking brake, in every mode.
    # The hold is added to each mode's A below (a binding made for one mode hides the ANY one, hold included).
    for mode in (1, 2, 3):
        if f["gear"] and mode != 2:
            L[mode][A] = ("Gear up/down", S("flight_controls/landing_gear_toggle"))
        elif k == "glider":
            L[mode][A] = ("Tow release", S("flight_controls/glider_tow_release"))
        else:
            L[mode][A] = ("Landing light", S("lights/landing_lights_toggle"))
    # The flaps are on the slider (slider_flaps). B and C: autopilot aircraft get them per mode (below).
    if f["ap"]:
        pass
    elif k == "fighter":
        any_[B] = ("Speedbrake in", S("flight_controls/speed_brakes_up_one"))
        any_[C] = ("Speedbrake out", S("flight_controls/speed_brakes_down_one"))
    elif k == "glider":
        any_[B] = ("Airbrakes in", S("flight_controls/speed_brakes_up_one"))
        any_[C] = ("Airbrakes out", S("flight_controls/speed_brakes_down_one"))
    elif f["governor"]:
        any_[B] = ("Governor", S("engines/governor_toggle"))
        any_[C] = ("Carb heat", S("engines/carb_heat_toggle"))
    elif k == "heli":
        any_[B] = ("Searchlight", S("lights/spot_lights_toggle"))
        any_[C] = ("Taxi light", S("lights/taxi_lights_toggle"))
    elif k in ("piston", "ultralight"):
        any_[B] = ("Magnetos both", S("magnetos/magnetos_both_1"))
        any_[C] = ("Starter (hold)", S("starters/engage_starter_1"))
    if f["ap"]:
        any_[B] = ("AP on/off", S("autopilot/servos_toggle"))   # one button in every mode
        m1[C] = ("Flight director", S("autopilot/fdir_toggle"))
        m2[C] = ("AP ALT hold", S("autopilot/altitude_hold"))
        m3[C] = ("AP approach", S("autopilot/approach"))
    if k != "ultralight":
        if k == "heli":
            any_[SHAT_U] = ("Cyclic trim fwd", S("flight_controls/pitch_trim_down"))
            any_[SHAT_D] = ("Cyclic trim aft", S("flight_controls/pitch_trim_up"))
            any_[SHAT_L] = ("Cyclic trim L", S("flight_controls/aileron_trim_left"))
            any_[SHAT_R] = ("Cyclic trim R", S("flight_controls/aileron_trim_right"))
        else:
            any_[SHAT_U] = ("Trim nose down", S("flight_controls/pitch_trim_down"))
            any_[SHAT_D] = ("Trim nose up", S("flight_controls/pitch_trim_up"))
            if k == "fighter" and f.get("f14"):
                # The F-14's hat 2 is its weapon select, as in the manual: up 1 Sidewinder, right 2 Sparrow, down 3 Gun, left 4 Phoenix.
                any_[SHAT_U] = ("Sidewinder", "cmd:laminar/f14/fltc/cs_wpn_select_switch1")
                any_[SHAT_R] = ("Sparrow", "cmd:laminar/f14/fltc/cs_wpn_select_switch2")
                any_[SHAT_D] = ("Gun", "cmd:laminar/f14/fltc/cs_wpn_select_switch3")
                any_[SHAT_L] = ("Phoenix", "cmd:laminar/f14/fltc/cs_wpn_select_switch4")
                # Combat (mode 2) with the pinkie held: the trims on the same hat (the pinkie is a shift in mode 2 only).
                L[5][SHAT_U] = ("Trim nose down", S("flight_controls/pitch_trim_down"))
                L[5][SHAT_D] = ("Trim nose up", S("flight_controls/pitch_trim_up"))
                L[5][SHAT_L] = ("Rudder trim L", S("flight_controls/rudder_trim_left"))
                L[5][SHAT_R] = ("Rudder trim R", S("flight_controls/rudder_trim_right"))
            if k != "glider" and not f.get("f14"):
                any_[SHAT_L] = ("Rudder trim L", S("flight_controls/rudder_trim_left"))
                any_[SHAT_R] = ("Rudder trim R", S("flight_controls/rudder_trim_right"))
    any_[MOUSE] = ("Cockpit view", S("view/3d_cockpit_cmnd_look"))
    any_[POV1_U] = ("Look up", S("general/rot_up"))   # what X-Plane's Q E R F keys do: rotate the view
    any_[POV1_R] = ("Look right", S("general/rot_right"))
    any_[POV1_D] = ("Look down", S("general/rot_down"))
    any_[POV1_L] = ("Look left", S("general/rot_left"))
    for i, d in enumerate(("up", "right", "down", "left")):   # what the arrow keys do: pan; Shift + arrow: faster
        any_[NUB_U + i] = ("Pan " + d, S("general/" + d))
        any_[NUB_U + 4 + i] = ("Pan %s fast" % d, S("general/%s_fast" % d))
    any_[MFD_PREV] = ("Screen page -", cmd("x52pro/mfd_page_prev"))
    any_[MFD_NEXT] = ("Screen page +", cmd("x52pro/mfd_page_next"))
    any_[MFD_UP] = ("Screen cursor up", cmd("x52pro/mfd_up"))
    any_[MFD_DOWN] = ("Screen cursor down", cmd("x52pro/mfd_down"))
    any_[MFD_SELECT] = ("Screen select", cmd("x52pro/mfd_select"))

    def engine_hat(m):
        """Throttle hat for engine levers; used in modes 1 and 2."""
        if k == "piston":
            m[THAT_U] = ("Mixture rich", S("engines/mixture_up"))
            m[THAT_D] = ("Mixture lean", S("engines/mixture_down"))
            if f["prop"]:
                m[THAT_R] = ("Prop RPM +", S("engines/prop_up"))
                m[THAT_L] = ("Prop RPM -", S("engines/prop_down"))
            elif f["carb"]:
                m[THAT_R] = ("Carb heat on", S("engines/carb_heat_on"))
                m[THAT_L] = ("Carb heat off", S("engines/carb_heat_off"))
            else:
                m[THAT_R] = ("Nav lights on", S("lights/nav_lights_on"))
                m[THAT_L] = ("Nav lights off", S("lights/nav_lights_off"))
        elif k == "turboprop":
            m[THAT_U] = ("Prop RPM +", S("engines/prop_up"))
            m[THAT_D] = ("Prop RPM -", S("engines/prop_down"))
            m[THAT_R] = ("Nav lights on", S("lights/nav_lights_on"))
            m[THAT_L] = ("Nav lights off", S("lights/nav_lights_off"))
        elif k == "heli":
            m[THAT_U] = ("Rotor RPM +", S("flight_controls/rotor_rpm_trim_up"))
            m[THAT_D] = ("Rotor RPM -", S("flight_controls/rotor_rpm_trim_down"))
            m[THAT_R] = ("Nav lights on", S("lights/nav_lights_on"))
            m[THAT_L] = ("Nav lights off", S("lights/nav_lights_off"))
        elif k == "ultralight":
            m[THAT_R] = ("Carb heat on", S("engines/carb_heat_on"))
            m[THAT_L] = ("Carb heat off", S("engines/carb_heat_off"))
        elif k != "glider":
            m[THAT_U] = ("Nav lights on", S("lights/nav_lights_on"))
            m[THAT_D] = ("Nav lights off", S("lights/nav_lights_off"))
            m[THAT_R] = ("Taxi light on", S("lights/taxi_lights_on"))
            m[THAT_L] = ("Taxi light off", S("lights/taxi_lights_off"))

    def view_hat(m):
        m[THAT_U] = ("Cockpit view", S("view/3d_cockpit_cmnd_look"))
        m[THAT_D] = ("Chase view", S("view/chase"))
        m[THAT_R] = ("Circle view", S("view/circle"))
        m[THAT_L] = ("Free camera", S("view/free_camera"))

    def trim_wheel(m):
        if k == "ultralight":
            return
        m[SCROLL_F] = ("Trim nose down", S("flight_controls/pitch_trim_down"))
        m[SCROLL_B] = ("Trim nose up", S("flight_controls/pitch_trim_up"))
        if k not in ("heli", "glider"):
            m[SCROLL_C] = ("Take-off trim", S("flight_controls/pitch_trim_takeoff"))

    # ---- Mode 1: ground & take-off
    if k != "glider":
        m1[D] = ("Taxi light", S("lights/taxi_lights_toggle")) if not f["gear"] else ("Landing lights", S("lights/landing_lights_toggle"))
        m1[E] = ("Tailwheel lock", S("flight_controls/tail_wheel_lock_toggle")) if f["tailwheel"] else ("Strobes", S("lights/strobe_lights_toggle"))
    m1[T1U] = ("Battery on", S("electrical/battery_1_on"))
    m1[T1D] = ("Battery off", S("electrical/battery_1_off"))
    m1[T2U] = ("Avionics on", S("systems/avionics_on"))
    m1[T2D] = ("Avionics off", S("systems/avionics_off"))
    if f["apu"]:
        m1[T3U] = ("APU start", S("electrical/APU_start"))
        m1[T3D] = ("APU off", S("electrical/APU_off"))
    elif k == "fighter":
        m1[T3U] = ("Canopy open", S("flight_controls/canopy_open"))
        m1[T3D] = ("Canopy close", S("flight_controls/canopy_close"))
    elif f["governor"]:
        m1[T3U] = ("Governor on", S("engines/governor_on"))
        m1[T3D] = ("Governor off", S("engines/governor_off"))
    elif k != "glider":
        m1[T3U] = ("Beacon on", S("lights/beacon_lights_on"))
        m1[T3D] = ("Beacon off", S("lights/beacon_lights_off"))
    engine_hat(m1)
    if f.get("f14"):
        # The F-14 ignores the generic nav light off and taxi light off (tested in the sim); its own switches work.
        m1[THAT_U] = ("Nav lights on", "cmd:laminar/f14/ltng/wing_nav_light_switch_up")
        m1[THAT_D] = ("Nav lights off", "cmd:laminar/f14/ltng/wing_nav_light_switch_dn")
        m1[THAT_R] = ("Taxi light", "cmd:laminar/f14/ltng/taxi_light_switch")
        del m1[THAT_L]
    m1[SCROLL_F] = ("Zoom in", S("general/zoom_in"))
    m1[SCROLL_B] = ("Zoom out", S("general/zoom_out"))
    m1[SCROLL_C] = ("Chase view", S("view/chase"))

    # ---- Mode 3: landing (approach, flaps, speedbrake, reverse, autobrake)
    if f["rev"]:
        m3[D] = ("Reverse (hold)", S("engines/thrust_reverse_hold"))   # the lights are the same lights as in mode 1: on the hat, left
    elif f["speedbrake"]:
        m3[D] = ("Speedbrake", S("flight_controls/speed_brakes_toggle"))
    elif k != "glider":
        m3[D] = ("Taxi light", S("lights/taxi_lights_toggle")) if not f["gear"] else ("Landing lights", S("lights/landing_lights_toggle"))
    if f["hook"]:
        m3[E] = ("Tail hook", S("flight_controls/tailhook_toggle"))
    elif f["tailwheel"]:
        m3[E] = ("Tailwheel lock", S("flight_controls/tail_wheel_lock_toggle"))
    elif k not in ("glider",):
        m3[E] = ("Strobes", S("lights/strobe_lights_toggle"))
    if f["autobrake"]:
        m3[T1U] = ("Autobrake up", S("flight_controls/brakes_up_auto"))
        m3[T1D] = ("Autobrake down", S("flight_controls/brakes_dn_auto"))
    elif f["ap"]:
        m3[T1U] = ("AP NAV", S("autopilot/NAV"))
        m3[T1D] = ("AP HDG", S("autopilot/heading"))
    elif k != "glider":
        m3[T1U] = ("Landing lt on", S("lights/landing_lights_on"))
        m3[T1D] = ("Landing lt off", S("lights/landing_lights_off"))
    if f["speedbrake"] and k != "fighter":
        m3[T2U] = ("Spoilers arm", S("flight_controls/speed_brakes_arm_toggle"))
        m3[T2D] = ("Spoilers retract", S("flight_controls/speed_brakes_up_all"))
    elif k in ("piston", "turboprop", "heli", "jet"):
        m3[T2U] = ("Fuel pumps on", S("fuel/fuel_pumps_on"))
        m3[T2D] = ("Fuel pumps off", S("fuel/fuel_pumps_off"))
    if f["carb"] and k != "ultralight":
        m3[T3U] = ("Carb heat on", S("engines/carb_heat_on"))
        m3[T3D] = ("Carb heat off", S("engines/carb_heat_off"))
    elif k not in ("glider", "ultralight"):
        m3[T3U] = ("Pitot heat on", S("ice/pitot_heat0_on"))
        m3[T3D] = ("Pitot heat off", S("ice/pitot_heat0_off"))
    if f["speedbrake"]:
        m3[THAT_U] = ("Speedbrake in", S("flight_controls/speed_brakes_up_one"))
        m3[THAT_D] = ("Speedbrake out", S("flight_controls/speed_brakes_down_one"))
        if f["rev"]:
            m3[THAT_L] = ("Landing lights", S("lights/landing_lights_toggle"))
        if f["at"]:
            m3[THAT_R] = ("Autothrottle", S("autopilot/autothrottle_toggle"))
    else:
        engine_hat(m3)
        if f["rev"]:
            m3[THAT_L] = ("Landing lights", S("lights/landing_lights_toggle"))
    trim_wheel(m3)

    # ---- Mode 2: cruise (autopilot / systems) and combat (all the weapons)
    if k == "fighter":
        if f.get("f14"):   # the generic master arm commands do nothing in the F-14; its own switch toggles it
            m2[D] = ("Master arm", "cmd:laminar/f14/weapons/switch/master_arm_switch")
        else:
            m2[D] = ("Master arm on", S("weapons/master_arm_on"))
            m2[E] = ("Master arm off", S("weapons/master_arm_off"))
        if f.get("f14"):
            # The F-14's own weapon select switch, on the throttle hat like the manual says: 1 up, 2 right, 3 down, 4 left.
            # (its switches 1 to 4 are Sidewinder, Sparrow, Gun, Phoenix, as in its scripts and the SMS page)
            m2[THAT_U] = ("Sidewinder", "cmd:laminar/f14/fltc/cs_wpn_select_switch1")
            m2[THAT_R] = ("Sparrow", "cmd:laminar/f14/fltc/cs_wpn_select_switch2")
            m2[THAT_D] = ("Gun", "cmd:laminar/f14/fltc/cs_wpn_select_switch3")
            m2[THAT_L] = ("Phoenix", "cmd:laminar/f14/fltc/cs_wpn_select_switch4")
            m2[T1U] = ("Target next", S("weapons/weapon_target_up"))
            m2[T1D] = ("Target prev", S("weapons/weapon_target_down"))
        else:
            m2[T1U] = ("Weapon next", S("weapons/weapon_select_up"))
            m2[T1D] = ("Weapon prev", S("weapons/weapon_select_down"))
        m2[T2U] = ("Flares", S("weapons/deploy_flares"))
        m2[T2D] = ("Chaff", S("weapons/deploy_chaff"))
        if f["sweep"]:
            m2[T3U] = ("Wings forward", S("flight_controls/vector_sweep_forward"))
            m2[T3D] = ("Wings aft", S("flight_controls/vector_sweep_aft"))
        else:
            m2[T3U] = ("Fire mode +", S("weapons/fire_mode_up"))
            m2[T3D] = ("Fire mode -", S("weapons/fire_mode_down"))
        if not f.get("f14"):
            m2[THAT_U] = ("Target next", S("weapons/weapon_target_up"))
            m2[THAT_D] = ("Target prev", S("weapons/weapon_target_down"))
            m2[THAT_R] = ("Circle view", S("view/circle"))
            m2[THAT_L] = ("Chase view", S("view/chase"))
        trim_wheel(m2)
    elif f["ap"]:
        m2[D] = ("AP wing leveler", S("autopilot/wing_leveler"))
        m2[E] = ("Flight director", S("autopilot/fdir_toggle"))
        m2[T1U] = ("AP HDG", S("autopilot/heading"))
        m2[T1D] = ("AP NAV", S("autopilot/NAV"))
        m2[T2U] = ("AP ALT hold", S("autopilot/altitude_hold"))
        m2[T2D] = ("AP VS", S("autopilot/vertical_speed"))
        if f["at"]:
            m2[T3U] = ("Autothrottle", S("autopilot/autothrottle_toggle"))
            m2[T3D] = ("AP level change", S("autopilot/level_change"))
        else:
            m2[T3U] = ("AP FLC", S("autopilot/level_change"))
            m2[T3D] = ("AP approach", S("autopilot/approach"))
        m2[THAT_U] = ("ALT select +", S("autopilot/altitude_up"))
        m2[THAT_D] = ("ALT select -", S("autopilot/altitude_down"))
        if f["at"]:
            m2[THAT_R] = ("Speed +", S("autopilot/airspeed_up"))
            m2[THAT_L] = ("Speed -", S("autopilot/airspeed_down"))
        else:
            m2[THAT_R] = ("VS +", S("autopilot/vertical_speed_up"))
            m2[THAT_L] = ("VS -", S("autopilot/vertical_speed_down"))
        m2[SCROLL_F] = ("HDG bug +", S("autopilot/heading_up"))
        m2[SCROLL_B] = ("HDG bug -", S("autopilot/heading_down"))
        m2[SCROLL_C] = ("HDG sync", S("autopilot/heading_sync"))
    else:
        if k not in ("glider",):
            m2[D] = ("Nav lights", S("lights/nav_lights_toggle"))
            m2[E] = ("Carb heat", S("engines/carb_heat_toggle")) if f["carb"] else ("Pitot heat", S("ice/pitot_heat0_tog"))
            m2[T1U] = ("Fuel tank left", S("fuel/fuel_tank_selector_lft_one"))
            m2[T1D] = ("Fuel tank right", S("fuel/fuel_tank_selector_rgt_one"))
            m2[T2U] = ("Fuel pumps on", S("fuel/fuel_pumps_on"))
            m2[T2D] = ("Fuel pumps off", S("fuel/fuel_pumps_off"))
            m2[T3U] = ("Strobes on", S("lights/strobe_lights_on"))
            m2[T3D] = ("Strobes off", S("lights/strobe_lights_off"))
        view_hat(m2)
        trim_wheel(m2)
    if f.get("f14"):
        # Pinkie changes H2 only. Explicitly retain every other Combat binding:
        # shifted layers fall back to Any mode, not to the unshifted mode.
        L[5] = {**m2, **L[5]}
    return L


F14_SELECTED = "sim/cockpit2/weapons/weapon_select_console_index"


def releases(f):
    """{layer: {button: steps}} - what a button does when it is let go. F-14 trigger: put the remembered weapon back."""
    if f.get("f14"):
        # The weapon select switch also sets guns_armed and missiles_armed, so writing the index back is not enough
        # (tested in the sim): the matching select command puts everything back. Index: Sidewinder 2-3, Sparrow 0-1, Gun 26, Phoenix 4-6.
        c = "laminar/f14/fltc/cs_wpn_select_switch"
        steps = [(2, 3, 1), (0, 1, 2), (26, 26, 3), (4, 6, 4)]
        return {ANY: {TRIGGER: " ; ".join("restoreif:%s=%d,%d,%s%d" % (F14_SELECTED, lo, hi, c, n) for lo, hi, n in steps)}}
    return {}


def holds(f):
    """{layer: {button: (label, steps)}} - what a button does when held longer than the hold time (400 ms).
    A: the parking brake, in every mode (its tap is the gear or the landing light)."""
    if f["kind"] != "fighter":
        return civil(f)[1]
    return {m: {A: ("Parking brake", S("flight_controls/brakes_toggle_max"))} for m in ((1, 2, 3, 5) if f.get("f14") else (1, 2, 3))}


# ---------------------------------------------------------------- lights, alarms, screen
LED_ORDER = ["fire", "a", "b", "d", "e", "t1", "t2", "t3", "pov", "clutch", "throttle"]
LED_TITLES = {"fire": "Fire", "a": "A", "b": "B", "d": "D", "e": "E", "t1": "T1", "t2": "T2", "t3": "T3",
              "pov": "Hat 2", "clutch": "Clutch (i)", "throttle": "Throttle"}
MODE_COLOUR = {1: "green", 2: "amber", 3: "red"}


def mode_leds(mode):
    c = MODE_COLOUR[mode]
    leds = {l: "green" for l in LED_ORDER}
    for l in ("d", "e", "t1", "t2", "t3", "pov"):
        leds[l] = c
    return leds


def rules(f):
    """Live lights: (light, text for the file, words for the sheet)."""
    r = []
    if f["kind"] == "fighter":
        r.append(("fire", "fire > 0.5 green off sim/cockpit2/weapons/master_arm", "Fire: lit while master arm is on"))
    else:
        r.append(("fire", "fire > 0.5 green off sim/flightmodel/controls/parkbrake", "Fire: lit while the parking brake is set"))
    if f["gear"]:
        r.append(("a", "a > 0.99 green off sim/flightmodel2/gear/deploy_ratio[0]", "A: green when the gear is down and locked, off when up"))
    elif f["kind"] == "fighter":   # civil fixed-gear A no longer has the landing light (issue #121)
        r.append(("a", "a > 0.5 green mode sim/cockpit2/switches/landing_lights_on", "A: green while the landing light is on"))
    if f["flaps"]:
        r.append(("b", "b > 0.01 amber green sim/cockpit2/controls/flap_handle_deploy_ratio", "B: amber while flaps are out, green when up"))
    elif f["kind"] == "glider":
        r.append(("b", "b > 0.01 amber green sim/cockpit2/controls/speedbrake_ratio", "B: amber while airbrakes are out"))
    # The throttle light has no rule: it shows its mode colour (green) in every profile.
    return r


def axes(f):
    """Flight axes stay with X-Plane (its own Joystick settings drive the throttle, levers and autothrottle); the
    plugin's override can still be switched on per profile in the app. The slider on the throttle sets the flaps,
    at the aircraft's detents, in every aircraft that has flaps (slider pushed away = flaps deployed)."""
    # X-Plane's own axis assignment did not reach the controls (seen live in the Citation X: the stick and the
    # throttle moved in X-Plane's joystick values, the yoke and throttle stayed at 0), so the plugin drives the
    # stick and twist everywhere, and the throttle in every aircraft but the helicopters (their lever is the collective).
    if f["kind"] == "fighter":
        dial_i, dial_e = (1 if f["speedbrake"] else 0), 2
    else:
        c = CIVIL[f["name"]]
        dial_i, dial_e = DIAL_USE[c.get("dial_i")], DIAL_USE[c.get("dial_e", "zoom")]
    return ["axis_override = 1", "throttle_plugin = %d" % (0 if f["kind"] == "heli" else 1),
            "axis_roll = 5 0 0", "axis_pitch = 5 0 0", "axis_yaw = 5 0 0", "axis_throttle = 5 0 1",
            "slider_flaps = %d" % (1 if f["flaps"] else 0),
            # Invert flaps off: the X52 slider pushed away from you deploys the flaps, towards you retracts them
            # (confirmed by the user in the Citation X cockpit).
            "slider_invert = 0",
            # The dials (issue #121): the lower one (i) is the prop, speed brake, airbrakes or carb heat; the upper
            # one (E) the mixture, rudder trim or carb heat; zoom where the aircraft has nothing better for it.
            # Fighters: speed brake and zoom.
            "rotary_lower = %d" % dial_i, "rotary_lower_invert = 0",
            "rotary_upper = %d" % dial_e, "rotary_upper_invert = 0",
            # The part of a lever job's range one full turn covers (percent). The mixture stops at 5 %, so a dial
            # turned right down does not cut the engine (idle cut-off stays in the cockpit).
            "rotary_lower_range = 0 100", "rotary_upper_range = %s" % ("5 100" if dial_e == DIAL_USE["mixture"] else "0 100"),
            # Zoom strength: X-Plane zoom steps per full turn of the E dial (tried in the Citation X; 24 was too little).
            "rotary_zoom_steps = 96"]


def alarms(f):
    """(file text, words for the sheet) in priority order."""
    k = f["kind"]
    out = []
    if k not in ("glider",):
        fire = "> 0 sim/cockpit2/annunciators/engine_fires[0]"
        if f["engines"] > 1:
            fire += " or > 0 sim/cockpit2/annunciators/engine_fires[1]"
        out.append(('fire blink fast red amber "Engine fire" ' + fire, "Engine fire", "Fire", "flashes fast"))
    if k == "heli":
        out.append(('pov blink fast red amber "Low rotor RPM" > 0 sim/cockpit2/annunciators/low_rotor', "Low rotor RPM", "Hat 2", "red, flashes fast"))
    else:
        out.append(('pov blink fast red amber "Stall" > 0 sim/cockpit2/annunciators/stall_warning', "Stall warning", "Hat 2", "red, flashes fast"))
    if f["gear"]:
        out.append(('a blink slow red amber "Gear unsafe" > 0 sim/cockpit2/annunciators/gear_unsafe', "Gear unsafe", "A", "red, flashes slowly"))
    if f["caution"]:
        out.append(('clutch blink fast red amber "Master warning" > 0 sim/cockpit2/annunciators/master_warning', "Master warning", "Clutch (i)", "red, flashes fast"))
        out.append(('clutch blink slow amber red "Master caution" > 0 sim/cockpit2/annunciators/master_caution', "Master caution", "Clutch (i)", "amber, flashes slowly"))
    if k != "glider":
        out.append(('t1 alternate slow red amber "Low fuel" > 0 sim/cockpit2/annunciators/fuel_quantity', "Low fuel", "T1", "red / amber"))
    if f["ap"]:
        out.append(('t2 blink fast amber red "Autopilot disconnect" > 0 sim/cockpit2/annunciators/autopilot_disconnect', "Autopilot disconnect", "T2", "amber, flashes fast"))
    out.append(('t3 blink fast red amber "Overspeed" > 0 sim/flightmodel/failures/over_vne', "Overspeed (Vne)", "T3", "red, flashes fast"))
    if k in ("piston", "ultralight"):
        out.append(('d blink slow amber red "Low oil pressure" > 0 sim/cockpit2/annunciators/oil_pressure_low[0] and > 500 sim/cockpit2/engine/indicators/engine_speed_rpm[0]',
                    "Low oil pressure (engine running)", "D", "amber, flashes slowly"))
    return out


def pages(f):
    """Screen pages: [(id, title, [(label, format, dataref)])]."""
    k = f["kind"]
    G = "sim/cockpit2/gauges/indicators/"
    E = "sim/cockpit2/engine/indicators/"
    flight = [("IAS", "%.0f kt", G + "airspeed_kts_pilot"), ("Alt", "%.0f ft", G + "altitude_ft_pilot"),
              ("VS", "%.0f fpm", G + "vvi_fpm_pilot"), ("Hdg", "%.0f deg", G + "heading_AHARS_deg_mag_pilot")]
    if k in ("airliner", "jet", "fighter"):
        flight.append(("Mach", "%.2f", G + "mach_pilot"))
    if k in ("airliner", "heli", "jet"):
        flight.append(("Radio", "%.0f ft", G + "radio_altimeter_height_ft_pilot"))
    if k == "fighter":
        flight.append(("G", "%.1f", "sim/flightmodel2/misc/gforce_normal"))
        flight.append(("AoA", "%.1f deg", "sim/flightmodel2/misc/AoA_angle_degrees"))
    flight.append(("OAT", "%.0f C", "sim/cockpit2/temperature/outside_air_temp_degc"))
    if k == "glider":
        return [("soar", "Soaring", [("IAS", "%.0f kt", G + "airspeed_kts_pilot"), ("Alt", "%.0f ft", G + "altitude_ft_pilot"),
                                      ("Vario", "%.0f fpm", G + "total_energy_fpm"), ("Wind", "%.0f kt", G + "wind_speed_kts"),
                                      ("From", "%.0f deg", G + "wind_heading_deg_mag")])]
    eng = []
    n = f["engines"]
    side = ["L", "R"] if n > 1 else [""]
    for i, s in enumerate(side):
        if k in ("airliner", "jet", "fighter"):
            eng.append((("N1 " + s).strip(), "%.1f", E + "N1_percent[%d]" % i))
        elif k == "turboprop":
            eng.append((("Trq " + s).strip(), "%.0f Nm", E + "torque_n_mtr[%d]" % i))
            eng.append((("ITT " + s).strip(), "%.0f C", E + "ITT_deg_C[%d]" % i))
        elif k == "heli":
            pass
        elif k != "electric":
            eng.append((("RPM " + s).strip(), "%.0f", E + "engine_speed_rpm[%d]" % i))
        if f["prop"] and k == "piston":
            eng.append((("MP " + s).strip(), "%.1f in", E + "MPR_in_hg[%d]" % i))
        if k == "turboprop":
            eng.append((("Prop " + s).strip(), "%.0f", E + "prop_speed_rpm[%d]" % i))
    if k == "heli":
        eng.append(("Rotor", "%.0f rpm", E + "prop_speed_rpm[0]"))
        eng.append(("Eng", "%.0f rpm", E + "engine_speed_rpm[0]"))
    if k == "electric":
        eng.append(("Prop", "%.0f rpm", E + "prop_speed_rpm[0]"))
    if k != "electric":
        eng.append(("Oil T", "%.0f C", E + "oil_temperature_deg_C[0]"))
        eng.append(("Oil P", "%.0f psi", E + "oil_pressure_psi[0]"))
    out = [("flt", "Flight", flight), ("eng", "Engine", eng)]
    if k != "electric":
        out.append(("fuel", "Fuel", [("Total", "%.0f kg", "sim/flightmodel/weight/m_fuel_total"),
                                     ("Left", "%.0f kg", "sim/cockpit2/fuel/fuel_quantity[0]"),
                                     ("Right", "%.0f kg", "sim/cockpit2/fuel/fuel_quantity[1]")]))
    return out


BUILT_IN_PAGES = ["COM1 / NAV1", "COM2 / NAV2", "ADF", "DME", "Transponder", "Autopilot", "OBS / Heading"]
