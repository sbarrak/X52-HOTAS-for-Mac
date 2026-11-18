#include "axes.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace x52 {

namespace {
const AxisTarget kTargets[] = {
    {0x01, 0x30, "roll", "sim/joystick/yoke_roll_ratio", "sim/operation/override/override_joystick", true, 1},
    {0x01, 0x31, "pitch", "sim/joystick/yoke_pitch_ratio", "sim/operation/override/override_joystick", true, 1},
    {0x01, 0x35, "yaw", "sim/joystick/yoke_heading_ratio", "sim/operation/override/override_joystick", true, 1},
    {0x01, 0x32, "throttle", "sim/flightmodel/engine/ENGN_thro_use", "sim/operation/override/override_throttles", false, 16},
};
}  // namespace

const AxisTarget* axisTarget(int usagePage, int usage) {
    for (const AxisTarget& t : kTargets)
        if (t.usagePage == usagePage && t.usage == usage) return &t;
    return nullptr;
}

AxisCurve& axisCurve(AxisSettings& s, int usage) {
    switch (usage) {
        case 0x31: return s.pitch;
        case 0x35: return s.yaw;
        case 0x32: return s.throttle;
        default: return s.roll;
    }
}
const AxisCurve& axisCurve(const AxisSettings& s, int usage) { return axisCurve(const_cast<AxisSettings&>(s), usage); }

AxisCurve* dialCurve(AxisSettings& s, int usage) {
    switch (usage) {
        case 0x33: return &s.lowerRotary;
        case 0x34: return &s.upperRotary;
        case 0x36: return &s.slider;
        default: return nullptr;
    }
}

long steadyValue(long shown, long value, long min, long max, int pct) {
    if (value <= min || value >= max) return value;
    return labs(value - shown) * 100 > (long)pct * (max - min) ? value : shown;
}

double normalizeAxis(long value, long min, long max, bool centred) {
    if (max <= min) return centred ? 0.0 : 0.0;
    double t = (double)(std::max(min, std::min(max, value)) - min) / (double)(max - min);
    return centred ? 2.0 * t - 1.0 : t;
}

double shapeAxis(double x, const AxisCurve& c, bool centred) {
    double dz = std::max(0, std::min(90, c.deadzone)) / 100.0;
    double e = std::max(0, std::min(100, c.expo)) / 100.0;
    double mag = centred ? std::fabs(x) : x;
    double sign = centred && x < 0 ? -1.0 : 1.0;
    mag = mag <= dz ? 0.0 : (mag - dz) / (1.0 - dz);
    mag = std::min(1.0, mag);
    mag = (1.0 - e) * mag + e * mag * mag * mag;
    double y = sign * mag;
    if (c.invert) y = centred ? -y : 1.0 - y;
    return y;
}

}  // namespace x52
