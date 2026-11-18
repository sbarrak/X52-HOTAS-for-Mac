// Flight axes processed by the plugin instead of X-Plane: dead zone, response curve and inversion.
// Off by default (X-Plane stays in charge); a profile can switch it on. Pure C++.
#pragma once

namespace x52 {

struct AxisCurve {
    int deadzone = 5;     // percent of the travel that does nothing (around the centre; at the bottom for the throttle)
    int expo = 0;         // 0 linear .. 100 fully cubic: gentle near the middle, full travel kept at the ends
    bool invert = false;
};

struct AxisSettings {
    // true: the plugin writes the stick and twist (roll, pitch, yaw) into X-Plane, replacing its own joystick reading.
    bool overrideXPlane = false;
    // true: the plugin drives the throttle (engine power and the cockpit levers), whatever X-Plane's own axis
    // assignment does. Separate from the stick, so a helicopter's lever (the collective) can stay X-Plane's.
    // A file without its own line takes the value of axis_override (older files drove both with that one switch).
    bool pluginThrottle = false;
    AxisCurve roll, pitch, yaw, throttle;
    // The dials and the slider: only a "steady" percentage (deadzone field), so a value that flickers by a hair stays put.
    AxisCurve lowerRotary = {2, 0, false}, upperRotary = {2, 0, false}, slider = {2, 0, false};
    // The slider on the throttle sets the flap handle, at the aircraft's own detents (independent of overrideXPlane).
    // A high slider reading is flaps fully deployed, a low one flaps up; slider.invert ("Invert flaps") swaps that.
    // On the X52, with it off (as in the ready-made profiles), the slider pushed away from you deploys the flaps.
    bool sliderFlaps = false;
};

// The X-Plane value a stick axis replaces.
struct AxisTarget {
    int usagePage, usage;
    const char* name;          // "roll", "pitch", "yaw", "throttle"
    const char* dataref;       // written every frame while the override is on
    const char* overrideRef;   // set to 1 so X-Plane stops writing `dataref` itself
    bool centred;              // -1..1 (true) or 0..1 (false)
    int count;                 // values in the dataref: 1, or one per engine for the throttle
};
const AxisTarget* axisTarget(int usagePage, int usage);   // nullptr: not a flight axis
AxisCurve& axisCurve(AxisSettings& s, int usage);          // the curve for a flight axis usage (roll when unknown)
const AxisCurve& axisCurve(const AxisSettings& s, int usage);

// The curve of a dial (0x33 lower rotary, 0x34 upper rotary, 0x36 slider); nullptr for anything else.
AxisCurve* dialCurve(AxisSettings& s, int usage);
// The value to show: `shown` stays until `value` is more than pct % of the travel away; the ends are always reached.
long steadyValue(long shown, long value, long min, long max, int pct);

// Raw value -> -1..1 (centred) or 0..1.
double normalizeAxis(long value, long min, long max, bool centred);
// Dead zone, curve and inversion applied to a normalised value.
double shapeAxis(double x, const AxisCurve& c, bool centred);

// The flap handle (0 up .. 1 fully down) for a slider position 0..1: the nearest of the aircraft's `detents` stops
// (0, 1/n, ..., 1), or the position itself when the aircraft reports no detents.
double flapHandleFor(double slider, int detents);

}  // namespace x52
