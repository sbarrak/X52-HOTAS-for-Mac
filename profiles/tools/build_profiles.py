"""Writes one X52 Pro profile (.x52) per aircraft file into profiles/.

The file name is what the plugin looks for: aircraft-<.acf file name>.x52
Usage: python3 build_profiles.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from aircraft import (AIRCRAFT, ANY, LED_ORDER, alarms, features, layout, mode_leds, pages, rules)

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
    o.append("pinkie_shift = 0")
    o.append("shift_button = 6")
    o.append("mode_buttons = 28 29 30")
    o.append("clutch_safe = 1")
    o.append("clutch_button = 31")
    o.append("hold_ms = 400")
    o.append("mode_settle_ms = 100")
    o.append("mfd_brightness = 127")
    o.append("led_brightness = 127")
    o.append("clutch_latched = 0")
    for i in (2, 3):
        o.append("clock%d_offset = 0" % i)
    for i in (1, 2, 3):
        o.append("clock%d_12h = 0" % i)
    o.append("date_format = 0")
    o.append("com_833 = 0")
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
    for layer in (ANY, 1, 2, 3):
        for button in sorted(L[layer]):
            label, steps = L[layer][button]
            o.append('bind %d %s press "%s" %s' % (button, "any" if layer == ANY else layer, label, steps))
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
