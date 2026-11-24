// Profile: what every X52 Pro button does in every mode, plus light colours.
// Pure C++ (no X-Plane, no macOS) so it can be unit tested.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "axes.h"
#include "dial.h"
#include "pagetree.h"

namespace x52 {

constexpr int kMaxButton = 51;   // X52 Pro reports HID buttons 1..39; 40..43 are POV 1 (the hat switch) and 44..51 the throttle's mouse nub, as buttons made by us
constexpr int kNubUp = 44;       // mouse nub up, right, down, left = 44..47; pushed all the way: 48..51
constexpr int kPov1Up = 40;      // POV 1 up, right, down, left = 40, 41, 42, 43 (a diagonal holds two)
constexpr int kLayerAny = 0;     // 1..3 = mode 1..3, 4..6 = mode 1..3 + pinkie held
constexpr int kLayerCount = 7;

enum class StepKind { Command, Key, Wait, Dataref };

// What a Dataref step does to its dataref. Set: write `value`. Toggle: switch between `value` and
// `value2` (to `value2` when the dataref is nearer `value`, otherwise to `value`). Add: add `value`,
// kept between `lo` and `hi` when lo < hi.
enum class DatarefOp { Set, Toggle, Add };

struct Step {
    StepKind kind = StepKind::Command;
    std::string text;   // X-Plane command name, key combo such as "shift+f1", or dataref ("name" or "name[3]")
    int ms = 0;         // Wait only
    DatarefOp op = DatarefOp::Set;   // Dataref only, with the four numbers below
    double value = 0, value2 = 1, lo = 0, hi = 0;
    bool operator==(const Step& o) const {
        return kind == o.kind && text == o.text && ms == o.ms && op == o.op && value == o.value &&
               value2 == o.value2 && lo == o.lo && hi == o.hi;
    }
};

// The value a Dataref step writes, given the dataref's current value. False when there is nothing
// to write (Add needs to know the current value).
bool datarefStepValue(const Step& s, bool haveCurrent, double current, double& out);
typedef std::vector<Step> Action;

struct Binding {
    int button = 0;
    int layer = kLayerAny;
    std::string label;      // shown on the MFD when pressed
    Action press;
    std::string holdLabel;
    Action hold;            // optional: runs instead when the button is held
    bool empty() const { return press.empty() && hold.empty(); }
};

// What clock 1 shows: GMT, the simulator's local time (the Mac's while X-Plane is not running), the Mac's local
// time, or GMT plus a fixed number of minutes (clock1Offset).
enum ClockSource { ClockGmt = 0, ClockSimLocal = 1, ClockMacLocal = 2, ClockFixed = 3 };
enum Led { LedFire, LedA, LedB, LedD, LedE, LedT1, LedT2, LedT3, LedPov, LedClutch, LedThrottle, kLedCount };
enum class Color { Off, Green, Amber, Red };   // Fire and Throttle are single colour: Off or lit

const char* ledName(int led);          // "fire", "a", ...
const char* ledTitle(int led);         // "Fire button", ...
// The light that sits on a control: -1 for a button without one. (Fire, A, B, D, E, the three toggles, the stick's
// hat 2 and the clutch.) Only for showing the light next to the button; the colours are set per light.
int buttonLed(int button);
bool ledIsSingleColor(int led);
const char* colorName(Color c);        // "off", "green", "amber", "red"

const char* buttonName(int button);    // "Trigger", "Fire", ... ; "" if out of range
const char* layerName(int layer);
// "Mode 2" or "Pinkie + 2": what is selected on the stick right now.
std::string modeLabel(int mode, bool pinkie);      // "Any mode", "Mode 1", "Mode 1 + Pinkie", ...

// A live light: while the simulator runs, the light shows `thenColor` when the dataref
// satisfies the condition and `elseColor` when it does not. kRuleMode means "the colour
// this mode gives the light", so a rule can leave one of its two states alone.
typedef std::function<bool(const std::string&, double&)> DatarefReader;   // current value of a dataref; false if unreadable
enum class Compare { Greater, GreaterEq, Less, LessEq, Equal, NotEqual };
constexpr int kRuleMode = -1;
constexpr int kCompareCount = 6;
const char* compareName(Compare c);                  // ">", ">=", "<", "<=", "==", "!="
bool compareValues(Compare c, double value, double limit);
const char* ruleColorName(int color);                // "off".."red", or "mode"

// One test on a dataref: "<dataref> <op> <value>", optionally negated (NOT).
struct Condition {
    std::string dataref;
    Compare op = Compare::Greater;
    double value = 0.5;
    bool negate = false;
    bool active() const { return !dataref.empty(); }
    bool operator==(const Condition& o) const {
        return dataref == o.dataref && op == o.op && value == o.value && negate == o.negate;
    }
};

// What a light waits for: one condition, or two joined by AND / OR. The first condition's
// fields live directly in the trigger so a one-condition rule reads like it always did.
enum class Join { None, And, Or };
struct Trigger : Condition {
    Join join = Join::None;
    Condition second;               // only used when join is not None
    bool hasSecond() const { return join != Join::None && second.active(); }
    bool operator==(const Trigger& o) const {
        return Condition::operator==(o) && join == o.join && second == o.second;
    }
};

enum class TriggerState { Unknown, False, True };   // Unknown: a dataref could not be read (or none chosen)
TriggerState evaluate(const Trigger& t, const DatarefReader& read);

struct LightRule : Trigger {
    int led = 4;              // which light (an Led; Button E by default)
    int thenColor = (int)Color::Red;     // a Color, or kRuleMode
    int elseColor = (int)Color::Green;
    bool operator==(const LightRule& o) const {
        return Trigger::operator==(o) && led == o.led && thenColor == o.thenColor && elseColor == o.elseColor;
    }
};

// What the light shows. `valid` is false when the dataref could not be read; the mode's
// colour is shown then. Fire and Throttle can only be off or lit.
Color ruleColor(const LightRule& r, int led, bool valid, double current, Color modeColor);

// What a light shows given every rule of the profile, in order: a rule whose result is "mode
// colour" leaves the light alone, otherwise the FIRST rule for that light wins.
Color liveColor(const std::vector<LightRule>& rules, int led, Color modeColor, const DatarefReader& read);

// Conditional lights allow one rule per light. Returns the index of the other rule that
// already uses `led` (ignoring rule `self`), or -1 when the light is free.
int ruleUsingLed(const std::vector<LightRule>& rules, int led, int self);
// First light no rule uses yet, or -1 when every light has one.
int firstFreeLed(const std::vector<LightRule>& rules);

// ---- Alarms: the highest priority. The first alarm that is triggered decides the light;
// an alarm that is not triggered leaves the light to the next one.
enum class Effect { Steady, Blink, Alternate };   // Alternate: colour and colour2 take turns
constexpr double kBlinkSlow = 1.0;   // seconds for one full on/off cycle
constexpr double kBlinkFast = 0.5;

struct Alarm : Trigger {
    std::string name;
    int led = LedPov;
    int color = (int)Color::Red;
    int color2 = (int)Color::Amber;   // Alternate only
    Effect effect = Effect::Blink;
    bool fast = false;                // Blink / Alternate speed: false 1 s per cycle, true 0.5 s
    bool operator==(const Alarm& o) const {
        return Trigger::operator==(o) && name == o.name && led == o.led && color == o.color && color2 == o.color2 &&
               effect == o.effect && fast == o.fast;
    }
};
const char* effectName(Effect e);   // "steady", "blink", "alternate"

// The colour an alarm gives its light at time t (seconds).
Color alarmColorAt(const Alarm& a, double t);
// The first triggered alarm for `led`: index, or -1 when none is triggered.
int activeAlarm(const std::vector<Alarm>& alarms, int led, const DatarefReader& read);

// Everything together: alarms over conditional lights over the mode colour.
Color shownColor(const std::vector<Alarm>& alarms, const std::vector<LightRule>& rules, int led, Color modeColor,
                 double t, const DatarefReader& read);

// Brightness that follows a dataref: the value runs from inLow to inHigh and the brightness from
// outLow to outHigh (0..127), clamped. No dataref chosen, or one that cannot be read, keeps the
// fixed brightness of the profile.
struct BrightnessLink {
    std::string dataref;
    double inLow = 0, inHigh = 1;
    int outLow = 10, outHigh = 127;
    bool active() const { return !dataref.empty(); }
    bool operator==(const BrightnessLink& o) const {
        return dataref == o.dataref && inLow == o.inLow && inHigh == o.inHigh && outLow == o.outLow && outHigh == o.outHigh;
    }
};
int followBrightness(const BrightnessLink& b, int fixed, const DatarefReader& read);

// A master switch keeps the screen (or the lights) on only while its trigger is true. No trigger
// chosen, or a dataref that cannot be read, leaves it on, so a typo never blanks the stick.
bool masterOn(const Trigger& t, const DatarefReader& read);

// Every dataref the rules, alarms and master switches read, without repeats (what a live link has to ask the sim for).
std::vector<std::string> datarefsUsed(const std::vector<Alarm>& alarms, const std::vector<LightRule>& rules,
                                      const std::vector<Trigger>& masters = std::vector<Trigger>());

struct AlarmPreset {
    const char* name;
    Alarm alarm;
};
int alarmPresetCount();
const AlarmPreset& alarmPreset(int i);

struct RulePreset {
    const char* name;
    LightRule rule;
};
int rulePresetCount();
const RulePreset& rulePreset(int i);

// One stick mode's own left-wheel order (see Profile::modeWheel). own false: the mode uses the shared order.
struct WheelSet {
    std::vector<std::string> order, hidden;
    bool own = false;
};

struct Profile {
    std::string name = "default";
    bool pinkieShift = true;
    int shiftButton = 6;
    int modeButtons[3] = {28, 29, 30};
    bool clutchSafe = true;
    int clutchButton = 31;
    int holdMs = 400;
    int modeSettleMs = 100;   // the dial must rest this long on a position before the mode changes
    // Each mode's name, shown on the stick's screen for modeBannerMs after the mode dial moves (0: not shown).
    std::string modeNames[3] = {"Takeoff", "Cruise", "Landing"};
    int modeBannerMs = 1500;
    int mfdBrightness = 127;
    int ledBrightness = 127;
    Color leds[3][kLedCount];          // per mode
    Color ledsPinkie[3][kLedCount];    // per mode while the pinkie switch is held
    Color& ledAt(int mode, bool pinkie, int led) { return pinkie ? ledsPinkie[mode - 1][led] : leds[mode - 1][led]; }
    Color ledAt(int mode, bool pinkie, int led) const { return pinkie ? ledsPinkie[mode - 1][led] : leds[mode - 1][led]; }
    std::vector<LightRule> rules;   // conditional lights, the same in every mode; one rule per light, the first wins
    std::vector<Alarm> alarms;      // alarms, in priority order: the first triggered one wins
    PageTree pages;                 // screen pages written by the user (the radio pages are built in; none: just those)
    // The left wheel's order (see wheelEntries() in pageview.h): page keys ("com1", "adf", ...) and "@<id>" for the
    // user's top-level pages. Empty: the usual order. The pages in wheelHidden are skipped, built-in ones too.
    std::vector<std::string> wheelOrder;
    std::vector<std::string> wheelHidden;
    // Mode 1..3's own order (index mode - 1), as wheel_order.N / wheel_hidden.N. A mode with either line is its own,
    // both lists from its lines; a mode with neither uses wheelOrder / wheelHidden.
    WheelSet modeWheel[3];
    bool radioSide = false;   // the radio slots show the side-by-side screens (radio_layout = side)
    // A ready-made profile (shipped in profiles/): never changed by the app or the plugin, so it can always be
    // gone back to. A change to it is saved as a new profile instead (basedOn names the profile it came from).
    bool standard = false;
    std::string basedOn;
    Trigger masterScreen;           // the screen is on only while this is true (no dataref chosen: always on)
    Trigger masterLights;           // the same for every light
    BrightnessLink brightScreen;    // screen brightness follows a dataref (none chosen: mfdBrightness)
    BrightnessLink brightLights;    // the same for the lights (ledBrightness)
    int version = 10;          // file format the profile was read from; 1 = older file (wheel bindings added on reading); below 3 the rules were reversed on reading (first rule wins now); below 4 untouched stick curves got the new defaults; below 5 the POV 1 look-around bindings were added on reading; below 6 the mouse nub pan bindings; below 7 the dials had no function lines (they get speed brake and zoom, the defaults); below 8 no dial ranges (0..100); below 9 modes 2 and 3 were approach/landing and cruise (swapped on reading unless the file has `mode_layout = 2`), the parking brake was on Fire (moved to a hold on A), and modes had no names; below 10 the Fire button fired the selected weapon (it drops bombs now when unchanged)
    bool clutchLatched = false;   // clutch toggles safe mode instead of holding it
    int clockOffset[2] = {0, 0};  // clock 2 and 3: minutes from GMT
    int clock1Source = 0;         // ClockSource: what clock 1 shows (GMT unless chosen)
    int clock1Offset = 0;         // ClockFixed: clock 1's minutes from GMT
    bool clock12h[3] = {false, false, false};
    AxisSettings axes;        // flight axes processed by the plugin (off: X-Plane reads the stick itself)
    DialAccel dial;           // fast turns of the right wheel change values in bigger jumps
    bool com833 = false;      // fine COM tuning from the stick steps 8.33 kHz channels instead of 25 kHz
    int dateFormat = 0;       // 0 DD-MM-YY, 1 MM-DD-YY, 2 YY-MM-DD
    std::vector<Binding> bindings;

    Profile();
    // Binding that applies to this button in this layer: exact layer first, else "any".
    const Binding* find(int button, int layer) const;
    // Binding stored for exactly this button+layer, or nullptr.
    const Binding* exact(int button, int layer) const;
    Binding& edit(int button, int layer);   // creates it if missing
    void prune();                           // drop bindings with no actions
    // Mode switch, pinkie-as-shift and clutch-as-safe-mode cannot carry bindings.
    bool isReserved(int button) const;
};

Profile defaultProfile();
// Binds the MFD's right wheel (37/38) and its button (39) to the screen cursor
// commands, unless the profile already uses those buttons. True if anything was added.
bool addWheelBindings(Profile& p);
// POV 1 (buttons 40..43) looks around: adds the view commands to each of them that has no binding. True if any was added.
bool addLookBindings(Profile& p);
// The throttle's mouse nub pans the view (buttons 44..51: arrows, and Shift + arrows when pushed all the way); same rule.
bool addNubBindings(Profile& p);
constexpr int kClockOffsetMin = -720, kClockOffsetMax = 840;
std::string serialize(const Profile& p);
bool parse(const std::string& text, Profile& out, std::string* error);

std::string actionToText(const Action& a);   // "cmd:sim/x ; wait:200 ; key:shift+f1"
bool actionFromText(const std::string& text, Action& out, std::string* error);

}  // namespace x52
