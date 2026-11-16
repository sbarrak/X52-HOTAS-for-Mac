// Profile: what every X52 Pro button does in every mode, plus light colours.
// Pure C++ (no X-Plane, no macOS) so it can be unit tested.
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace x52 {

constexpr int kMaxButton = 39;   // X52 Pro reports HID buttons 1..39
constexpr int kLayerAny = 0;     // 1..3 = mode 1..3, 4..6 = mode 1..3 + pinkie held
constexpr int kLayerCount = 7;

enum class StepKind { Command, Key, Wait };

struct Step {
    StepKind kind = StepKind::Command;
    std::string text;   // X-Plane command name, or key combo such as "shift+f1"
    int ms = 0;         // Wait only
    bool operator==(const Step& o) const { return kind == o.kind && text == o.text && ms == o.ms; }
};
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

enum Led { LedFire, LedA, LedB, LedD, LedE, LedT1, LedT2, LedT3, LedPov, LedClutch, LedThrottle, kLedCount };
enum class Color { Off, Green, Amber, Red };   // Fire and Throttle are single colour: Off or lit

const char* ledName(int led);          // "fire", "a", ...
const char* ledTitle(int led);         // "Fire button", ...
bool ledIsSingleColor(int led);
const char* colorName(Color c);        // "off", "green", "amber", "red"

const char* buttonName(int button);    // "Trigger", "Fire", ... ; "" if out of range
const char* layerName(int layer);
// "Mode 2" or "Pinkie + 2": what is selected on the stick right now.
std::string modeLabel(int mode, bool pinkie);      // "Any mode", "Mode 1", "Mode 1 + Pinkie", ...

// A live light: while the simulator runs, the light shows `thenColor` when the dataref
// satisfies the condition and `elseColor` when it does not. kRuleMode means "the colour
// this mode gives the light", so a rule can leave one of its two states alone.
enum class Compare { Greater, GreaterEq, Less, LessEq, Equal, NotEqual };
constexpr int kRuleMode = -1;
constexpr int kCompareCount = 6;
const char* compareName(Compare c);                  // ">", ">=", "<", "<=", "==", "!="
bool compareValues(Compare c, double value, double limit);
const char* ruleColorName(int color);                // "off".."red", or "mode"

struct LightRule {
    int led = 4;              // which light (an Led; Button E by default)
    std::string dataref;      // "sim/x/y" or "sim/x/y[3]" for one element of an array; empty = no rule
    Compare op = Compare::Greater;
    double value = 0.5;
    int thenColor = (int)Color::Red;     // a Color, or kRuleMode
    int elseColor = (int)Color::Green;
    bool active() const { return !dataref.empty(); }
    bool operator==(const LightRule& o) const {
        return led == o.led && dataref == o.dataref && op == o.op && value == o.value && thenColor == o.thenColor &&
               elseColor == o.elseColor;
    }
};

// What the light shows. `valid` is false when the dataref could not be read; the mode's
// colour is shown then. Fire and Throttle can only be off or lit.
Color ruleColor(const LightRule& r, int led, bool valid, double current, Color modeColor);

// What a light shows given every rule of the profile, in order: a rule whose result is "mode
// colour" leaves the light alone, otherwise the last rule for that light wins. `read` fetches a
// dataref's current value (false if it cannot).
typedef std::function<bool(const std::string&, double&)> DatarefReader;
Color liveColor(const std::vector<LightRule>& rules, int led, Color modeColor, const DatarefReader& read);

struct RulePreset {
    const char* name;
    LightRule rule;
};
int rulePresetCount();
const RulePreset& rulePreset(int i);

struct Profile {
    std::string name = "default";
    bool pinkieShift = true;
    int shiftButton = 6;
    int modeButtons[3] = {28, 29, 30};
    bool clutchSafe = true;
    int clutchButton = 31;
    int holdMs = 400;
    int modeSettleMs = 100;   // the dial must rest this long on a position before the mode changes
    int mfdBrightness = 127;
    int ledBrightness = 127;
    Color leds[3][kLedCount];          // per mode
    Color ledsPinkie[3][kLedCount];    // per mode while the pinkie switch is held
    Color& ledAt(int mode, bool pinkie, int led) { return pinkie ? ledsPinkie[mode - 1][led] : leds[mode - 1][led]; }
    Color ledAt(int mode, bool pinkie, int led) const { return pinkie ? ledsPinkie[mode - 1][led] : leds[mode - 1][led]; }
    std::vector<LightRule> rules;   // live lights, the same in every mode; several per light are fine
    int version = 2;          // file format the profile was read from; 1 = older file, wheel bindings were added on reading
    bool clutchLatched = false;   // clutch toggles safe mode instead of holding it
    int clockOffset[2] = {0, 0};  // clock 2 and 3: minutes from GMT
    bool clock12h[3] = {false, false, false};
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
constexpr int kClockOffsetMin = -720, kClockOffsetMax = 840;
std::string serialize(const Profile& p);
bool parse(const std::string& text, Profile& out, std::string* error);

std::string actionToText(const Action& a);   // "cmd:sim/x ; wait:200 ; key:shift+f1"
bool actionFromText(const std::string& text, Action& out, std::string* error);

}  // namespace x52
