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
    bool overrideXPlane = false;   // true: the plugin writes the axes into X-Plane, replacing its own joystick reading
    AxisCurve roll, pitch, yaw, throttle;
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

// Raw value -> -1..1 (centred) or 0..1.
double normalizeAxis(long value, long min, long max, bool centred);
// Dead zone, curve and inversion applied to a normalised value.
double shapeAxis(double x, const AxisCurve& c, bool centred);

}  // namespace x52
