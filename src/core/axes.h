// Flight axes processed by the plugin instead of X-Plane: dead zone, response curve and inversion.
// Off by default (X-Plane stays in charge); a profile can switch it on. Pure C++.
#pragma once
#include <map>
#include <string>

namespace x52 {

struct AxisCurve {
    int deadzone = 5;     // percent of the travel that does nothing (around the centre; at the bottom for the throttle)
    int expo = 0;         // 0 linear .. 100 fully cubic: gentle near the middle, full travel kept at the ends
    bool invert = false;
};

// What a throttle dial does (lower "i" dial 0x33, upper "E" dial 0x34). The plugin drives it from the dial's steady value.
enum class DialUse { None = 0, SpeedBrake = 1, Zoom = 2 };

struct AxisSettings {
    // true: the plugin writes the stick and twist (roll, pitch, yaw) into X-Plane, replacing its own joystick reading.
    bool overrideXPlane = false;
    // true: the plugin drives the throttle (engine power and the cockpit levers), whatever X-Plane's own axis
    // assignment does. Separate from the stick, so a helicopter's lever (the collective) can stay X-Plane's.
    // A file without its own line takes the value of axis_override (older files drove both with that one switch).
    bool pluginThrottle = false;
    // The stick starts soft around the centre (X-Plane's own default curve is not linear either); the X52's
    // short, light-sprung stick is far too twitchy for the elevator with a straight line. The throttle stays linear.
    AxisCurve roll = {3, 40, false}, pitch = {3, 50, false}, yaw = {3, 30, false}, throttle;
    // While the plugin drives the stick (overrideXPlane), X-Plane's own processing is skipped, so the plugin
    // does its job instead:
    // smoothingMs: small, slow changes (jitter, hand tremor) are smoothed over about this long; big or quick
    // moves, such as letting go of the stick, reach X-Plane at once. 0 = off.
    int smoothingMs = 60;
    // damping 0..100: a light pitch and roll damper, standing in for X-Plane's stability augmentation. It only
    // acts with the stick near the centre and is capped at a few percent of the travel. 0 = off.
    int damping = 30;
    // The dials and the slider: only a "steady" percentage (deadzone field), so a value that flickers by a hair stays put.
    AxisCurve lowerRotary = {2, 0, false}, upperRotary = {2, 0, false}, slider = {2, 0, false};
    // The slider on the throttle sets the flap handle, at the aircraft's own detents (independent of overrideXPlane).
    // A high slider reading is flaps fully deployed, a low one flaps up; slider.invert ("Invert flaps") swaps that.
    // On the X52, with it off (as in the ready-made profiles), the slider pushed away from you deploys the flaps.
    bool sliderFlaps = false;
    // The dials: SpeedBrake sets the speed brake in four steps (armed, retracted, half, full), Zoom zooms the view a
    // step at a time. lowerRotary.invert / upperRotary.invert turn them the other way.
    DialUse lowerUse = DialUse::SpeedBrake, upperUse = DialUse::Zoom;
    // Zoom strength: steps of X-Plane's own zoom (sim/general/zoom_in / zoom_out) per full turn of a dial, 12..480.
    int zoomSteps = 96;
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
// What a dial does (0x33 lower, 0x34 upper); nullptr for anything else.
DialUse* dialUse(AxisSettings& s, int usage);
// The value to show: `shown` stays until `value` is more than pct % of the travel away; the ends are always reached.
long steadyValue(long shown, long value, long min, long max, int pct);

// Raw value -> -1..1 (centred) or 0..1.
double normalizeAxis(long value, long min, long max, bool centred);
// Dead zone, curve and inversion applied to a normalised value.
double shapeAxis(double x, const AxisCurve& c, bool centred);

// The stick's own centre and ends, measured by a calibration (raw stick values). Not part of a profile: it
// belongs to the stick, so it is kept in its own file next to the profiles.
struct AxisCalibration {
    long min = 0, centre = 0, max = 0;
    bool valid() const { return min < centre && centre < max; }
};
// Raw value -> -1..1 with the centre at 0: with a calibration each side is scaled on its own, from the
// measured centre to the measured end (beyond the end stays at full travel); without one, the full HID range.
double normalizeStick(long value, long min, long max, const AxisCalibration* cal);
// usage -> calibration, as kept in the file: "<usage> <min> <centre> <max>" per line. Broken lines are skipped.
std::map<int, AxisCalibration> parseCalibration(const std::string& text);
std::string serializeCalibration(const std::map<int, AxisCalibration>& cal);

// Calibrating: start(), move the stick and twist to every limit (feed() every frame), let go, finish().
// Stick left/right and forward/back must be measured; the twist is measured when it was moved all the way too.
class StickCalibrator {
public:
    void start();
    void cancel() { active_ = false; }
    bool active() const { return active_; }
    void feed(int usage, long value, long min, long max);
    // What has been seen of an axis since start(): the lowest and highest raw values and the HID range. False
    // while the axis has not reported anything.
    bool reached(int usage, long& lo, long& hi, long& min, long& max) const;
    // rest: where each axis is now (the stick let go). False, with `error` saying what to do, when the stick was
    // not moved far enough or is not resting near its middle; `out` is left alone then.
    bool finish(const std::map<int, long>& rest, std::map<int, AxisCalibration>& out, std::string* error);

private:
    struct Seen {
        long lo, hi, min, max;
    };
    std::map<int, Seen> seen_;
    bool active_ = false;
};

// One frame of the stick smoothing: `prev` is what was sent last, `x` the new shaped value, `dt` the frame time.
// Small changes are smoothed over about `ms`; the bigger the change the quicker it follows, and a change of a
// tenth of the travel or more (a quick move, letting go of the stick) is passed on at once. ms <= 0: off.
// dt <= 0 (sim paused, no time passing): `prev` stays.
double smoothStick(double prev, double x, double dt, int ms);

// The damper: `cmd` is the pilot's stick (-1..1, X-Plane's sense), `rateDegS` the aircraft's rate about the
// same axis (pitch rate Q for the elevator, roll rate P for the ailerons), strength 0..100. Adds a small input
// against the rate, fading out as the stick moves away from the centre (none from half travel) and capped at
// 15 % of the travel at full strength. The pilot can always override it.
double dampStick(double cmd, double rateDegS, int strength);

// The dials. kZoomClicksPerTravel: the zoomClicks default (a profile's zoomSteps is what the plugin uses). The kXpAxis values are X-Plane's
// own axis assignments (sim/joystick/joystick_axis_assignments) for the same jobs: with one of them in use, X-Plane
// drives that job itself and the plugin leaves it alone.
constexpr int kZoomClicksPerTravel = 24;
constexpr int kXpAxisSpeedbrakes = 14, kXpAxisViewZoom = 43;
// The speed-brake handle for a dial position 0..1 (after Invert), in four equal steps: -0.5 armed, 0 retracted,
// 0.5 half, 1 full (X-Plane's sim/cockpit2/controls/speedbrake_ratio).
double speedBrakeFor(double x);
// Whole zoom steps between `anchor` and `x` (both 0..1), positive = zoom in. `anchor` moves by the steps returned, so
// what is left over counts towards the next step.
int zoomClicks(double& anchor, double x, int clicksPerTravel = kZoomClicksPerTravel);

// The flap handle (0 up .. 1 fully down) for a slider position 0..1: the nearest of the aircraft's `detents` stops
// (0, 1/n, ..., 1), or the position itself when the aircraft reports no detents.
double flapHandleFor(double slider, int detents);

}  // namespace x52
