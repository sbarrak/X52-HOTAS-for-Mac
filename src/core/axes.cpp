#include "axes.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

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

DialUse* dialUse(AxisSettings& s, int usage) {
    return usage == 0x33 ? &s.lowerUse : usage == 0x34 ? &s.upperUse : nullptr;
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

double normalizeStick(long value, long min, long max, const AxisCalibration* cal) {
    if (!cal || !cal->valid()) return normalizeAxis(value, min, max, true);
    double v = (double)value - (double)cal->centre;
    double span = v >= 0 ? (double)(cal->max - cal->centre) : (double)(cal->centre - cal->min);
    return std::max(-1.0, std::min(1.0, v / span));
}

std::map<int, AxisCalibration> parseCalibration(const std::string& text) {
    std::map<int, AxisCalibration> out;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ls(line);
        int usage = 0;
        AxisCalibration c;
        if (ls >> usage >> c.min >> c.centre >> c.max && c.valid()) out[usage] = c;
    }
    return out;
}

std::string serializeCalibration(const std::map<int, AxisCalibration>& cal) {
    std::ostringstream o;
    o << "# The X52 stick's own centre and ends, measured by the plugin's calibration: <usage> <min> <centre> <max>\n";
    for (const auto& kv : cal) o << kv.first << " " << kv.second.min << " " << kv.second.centre << " " << kv.second.max << "\n";
    return o.str();
}

void StickCalibrator::start() {
    seen_.clear();
    active_ = true;
}

void StickCalibrator::feed(int usage, long value, long min, long max) {
    if (!active_ || max <= min) return;
    auto it = seen_.find(usage);
    if (it == seen_.end()) {
        seen_[usage] = {value, value, min, max};
        return;
    }
    it->second.lo = std::min(it->second.lo, value);
    it->second.hi = std::max(it->second.hi, value);
    it->second.min = min;
    it->second.max = max;
}

bool StickCalibrator::reached(int usage, long& lo, long& hi, long& min, long& max) const {
    auto it = seen_.find(usage);
    if (it == seen_.end()) return false;
    lo = it->second.lo;
    hi = it->second.hi;
    min = it->second.min;
    max = it->second.max;
    return true;
}

bool StickCalibrator::finish(const std::map<int, long>& rest, std::map<int, AxisCalibration>& out, std::string* error) {
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    // The stick must be resting in the middle third (else it is still being held), and each side must cover a
    // quarter of the range (the X52's halves are about half each; less means it was not moved all the way).
    std::map<int, AxisCalibration> result;
    for (int usage : {0x30, 0x31, 0x35}) {
        bool required = usage != 0x35;   // the twist is optional: left alone, it keeps the full range
        auto s = seen_.find(usage);
        auto r = rest.find(usage);
        if (s == seen_.end() || r == rest.end()) {
            if (required) return fail("Move the stick all the way left, right, forward and back, then let go and press Done.");
            continue;
        }
        const Seen& v = s->second;
        long range = v.max - v.min, centre = r->second;
        if ((centre - v.min) * 100 < range * 35 || (centre - v.min) * 100 > range * 65)
            return fail("The stick is not resting in the middle: let go of the stick and the twist, then press Done.");
        AxisCalibration c{std::min(v.lo, centre), centre, std::max(v.hi, centre)};
        if ((c.centre - c.min) * 4 < range || (c.max - c.centre) * 4 < range) {
            if (required) return fail("Move the stick all the way left, right, forward and back, then let go and press Done.");
            continue;
        }
        result[usage] = c;
    }
    active_ = false;
    out = result;
    if (error) error->clear();
    return true;
}

double smoothStick(double prev, double x, double dt, int ms) {
    if (ms <= 0) return x;
    if (dt <= 0) return prev;
    const double band = 0.10;   // a change this big (a tenth of the travel) is passed on at once
    double d = std::fabs(x - prev);
    if (d >= band) return x;
    double k = 1.0 - d / band;
    double tau = ms / 1000.0 * k * k;
    if (tau < 1e-4) return x;
    return prev + (x - prev) * (1.0 - std::exp(-dt / tau));
}

double dampStick(double cmd, double rateDegS, int strength) {
    double s = std::max(0, std::min(100, strength)) / 100.0;
    if (s <= 0) return cmd;
    const double gainPerDegS = 0.02, maxAuthority = 0.15;   // at full strength
    double fade = std::max(0.0, 1.0 - std::fabs(cmd) / 0.5);
    double nudge = std::max(-maxAuthority * s, std::min(maxAuthority * s, -gainPerDegS * s * rateDegS));
    return std::max(-1.0, std::min(1.0, cmd + nudge * fade));
}

double flapHandleFor(double slider, int detents) {
    double x = std::max(0.0, std::min(1.0, slider));
    if (detents <= 0) return x;
    return std::round(x * detents) / detents;
}

double speedBrakeFor(double x) {
    return x < 0.25 ? -0.5 : x < 0.5 ? 0.0 : x < 0.75 ? 0.5 : 1.0;
}

int zoomClicks(double& anchor, double x, int clicksPerTravel) {
    double d = (x - anchor) * clicksPerTravel;
    int c = (int)std::trunc(d + (d > 0 ? 1e-9 : -1e-9));   // whole steps only, towards zero
    anchor += (double)c / clicksPerTravel;
    return c;
}

namespace {
const LeverJob kLevers[] = {
    {DialUse::Prop, "Prop", "sim/cockpit2/engine/actuators/prop_rotation_speed_rad_sec_all", false, 0, 1,
     "sim/aircraft/controls/acf_RSC_mingov_prp", "sim/aircraft/controls/acf_RSC_redline_prp", true, {8, 24, 25, 26, 27}},
    {DialUse::Mixture, "Mixture", "sim/cockpit2/engine/actuators/mixture_ratio_all", false, 0, 1, nullptr, nullptr, false, {9, 28, 29, 30, 31}},
    {DialUse::RudderTrim, "Rudder trim", "sim/cockpit2/controls/rudder_trim", false, -1, 1,
     "sim/aircraft/controls/acf_min_trim_rudd", "sim/aircraft/controls/acf_max_trim_rudd", false, {19}},
    {DialUse::AileronTrim, "Aileron trim", "sim/cockpit2/controls/aileron_trim", false, -1, 1,
     "sim/aircraft/controls/acf_min_trim_ailn", "sim/aircraft/controls/acf_max_trim_ailn", false, {18}},
    {DialUse::CowlFlaps, "Cowl flaps", "sim/cockpit2/engine/actuators/cowl_flap_ratio", true, 0, 1, nullptr, nullptr, false, {39}},
    {DialUse::CarbHeat, "Carb heat", "sim/cockpit2/engine/actuators/carb_heat_ratio", true, 0, 1, nullptr, nullptr, false, {10}},
};
}  // namespace

const LeverJob* leverJob(DialUse use) {
    for (const LeverJob& j : kLevers)
        if (j.use == use) return &j;
    return nullptr;
}

const char* dialUseName(DialUse use) {
    if (use == DialUse::SpeedBrake) return "Speed brake";
    if (use == DialUse::Zoom) return "Zoom";
    if (const LeverJob* j = leverJob(use)) return j->name;
    return "Nothing";
}

double leverValue(double x, double lo, double hi, int fromPct, int toPct) {
    x = std::max(0.0, std::min(1.0, x));
    double pct = fromPct + (toPct - fromPct) * x;
    return lo + (hi - lo) * pct / 100.0;
}

bool leverMayWrite(LeverPickup& s, double target, double sim, double tol) {
    if (tol <= 0) {
        s.held = false;
        return false;
    }
    bool write;
    if (s.held && std::fabs(sim - s.lastWritten) <= tol) {
        write = true;
    } else {
        s.held = false;
        bool crossed = s.seen && ((s.lastTarget - sim) * (target - sim) < 0);
        write = std::fabs(target - sim) <= tol || crossed;
    }
    s.lastTarget = target;
    s.seen = true;
    if (write) {
        s.held = true;
        s.lastWritten = target;
    }
    return write;
}

}  // namespace x52
