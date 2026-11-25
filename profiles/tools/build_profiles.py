"""Writes one X52 Pro profile (.x52) per aircraft file into profiles/.

The file name is what the plugin looks for: aircraft-<.acf file name>.x52
Usage: python3 build_profiles.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from aircraft import (AIRCRAFT, ANY, LED_ORDER, alarms, axes, features, holds, layout, mode_leds, releases, mode_names, pages, rules)

OUT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))


def key_for(acf):
    k = "aircraft-"
    for c in acf:
        k += c if (c.isalnum() or c in "-_. ") else "_"
    return k


def profile_text(ac, acf):
    f = features(ac)
    o = []
    o.append("x52pro-profile 3")
    o.append("# X52 Pro profile for the %s (ready-made, see profiles/README.md)." % ac["name"])
    o.append("name = " + acf)
    o.append("# standard: a ready-made profile, never changed by the app or the plugin (changes go to a copy)")
    o.append("standard = 1")
    if f.get("f14"):   # the F-14: the pinkie is the shift in combat (mode 2) only, for the trims on hat 2; a plain button (brakes) elsewhere
        o.append("pinkie_shift = 1")
        o.append("pinkie_shift_modes = 2")
    elif f["kind"] == "fighter":
        o.append("pinkie_shift = 0")
    else:   # civil (issue #121): the pinkie is the Shift in every mode (zoom on the wheel, extra lights, VS)
        o.append("pinkie_shift = 1")
    o.append("shift_button = 6")
    o.append("mode_buttons = 28 29 30")
    o.append("clutch_safe = 1")
    o.append("clutch_button = 31")
    o.append("hold_ms = 400")
    o.append("mode_settle_ms = 100")
    o.append("# modes: 1 takeoff, 2 cruise, 3 landing; the screen says the mode's name for mode_banner_ms (0: never)")
    o.append("mode_layout = 2")
    for m, n in sorted(mode_names(f).items()):
        o.append("mode_name_%d = %s" % (m, n))
    o.append("mode_banner_ms = 1500")
    o.append("mfd_brightness = 127")
    o.append("led_brightness = 127")
    o.append("clutch_latched = 0")
    for i in (2, 3):
        o.append("clock%d_offset = 0" % i)
    for i in (1, 2, 3):
        o.append("clock%d_12h = 0" % i)
    o.append("date_format = 0")
    o.append("com_833 = 0")
    o.extend(axes(f))
    for m in (1, 2, 3):
        leds = mode_leds(m)
        o.append("leds %d = " % m + " ".join("%s:%s" % (l, leds[l]) for l in LED_ORDER))
    for m in (1, 2, 3):
        leds = mode_leds(m)
        o.append("ledsp %d = " % m + " ".join("%s:%s" % (l, leds[l]) for l in LED_ORDER))
    for _, text, _ in rules(f):
        o.append("rule " + text)
    for a in alarms(f):
        o.append("alarm " + a[0])
    for pid, title, rows in pages(f):
        o.append('page %s "%s"' % (pid, title))
        for label, fmt, ref in rows:
            o.append('row %s value "%s" "%s" %s' % (pid, label, fmt, ref))
    if f["master"]:
        o.append("master screen == 1 sim/cockpit2/switches/avionics_power_on")
        o.append("master lights == 1 sim/cockpit2/switches/avionics_power_on")
    L = layout(f)
    H = holds(f)
    R = releases(f)
    for layer in range(7):   # any, modes 1-3, modes 1-3 with the pinkie held
        name = "any" if layer == ANY else str(layer)
        for button in sorted(set(L.get(layer, {})) | set(H.get(layer, {}))):
            if button in L.get(layer, {}):
                label, steps = L[layer][button]
                o.append('bind %d %s press "%s" %s' % (button, name, label, steps))
            if button in H.get(layer, {}):   # also alone: a tap then does nothing (A on fixed gear)
                hl, hs = H[layer][button]
                o.append('bind %d %s hold "%s" %s' % (button, name, hl, hs))
            if layer in R and button in R[layer]:
                o.append('bind %d %s release "" %s' % (button, name, R[layer][button]))
    return "\n".join(o) + "\n"


def main():
    written = []
    for ac in AIRCRAFT:
        for acf in ac["acfs"]:
            path = os.path.join(OUT, key_for(acf) + ".x52")
            with open(path, "w") as fh:
                fh.write(profile_text(ac, acf))
            written.append(path)
    for p in written:
        print(p)


if __name__ == "__main__":
    main()
