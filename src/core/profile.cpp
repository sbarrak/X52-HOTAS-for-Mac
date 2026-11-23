#include "profile.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace x52 {

namespace {

const char* const kLedNames[kLedCount] = {"fire", "a", "b", "d", "e", "t1", "t2", "t3", "pov", "clutch", "throttle"};
const char* const kLedTitles[kLedCount] = {"Fire button", "Button A", "Button B", "Button D", "Button E",
                                           "Toggle T1/T2", "Toggle T3/T4", "Toggle T5/T6", "POV 2",
                                           "Clutch (i)", "Throttle"};
const char* const kColorNames[4] = {"off", "green", "amber", "red"};

// HID button number (1-based) -> control, in the X52 Pro's report order (libx52io parser.c).
const char* const kButtonNames[kMaxButton + 1] = {
    "",
    "Trigger", "Fire", "A", "B", "C", "Pinkie", "D", "E",
    "Toggle 1 up", "Toggle 1 down", "Toggle 2 up", "Toggle 2 down", "Toggle 3 up", "Toggle 3 down",
    "Trigger 2nd stage", "Mouse button", "Scroll wheel fwd", "Scroll wheel back", "Scroll wheel click",
    "POV 2 up", "POV 2 right", "POV 2 down", "POV 2 left",
    "Throttle hat up", "Throttle hat right", "Throttle hat down", "Throttle hat left",
    "Mode 1", "Mode 2", "Mode 3", "Clutch (i)",
    "MFD Function", "MFD Start/Stop", "MFD Reset", "MFD Page up", "MFD Page down",
    "MFD Up", "MFD Down", "MFD Select",
    "POV 1 up", "POV 1 right", "POV 1 down", "POV 1 left",
    "Mouse nub up", "Mouse nub right", "Mouse nub down", "Mouse nub left",
    "Mouse nub up (far)", "Mouse nub right (far)", "Mouse nub down (far)", "Mouse nub left (far)"};

const char* const kLayerNames[kLayerCount] = {"Any mode", "Mode 1", "Mode 2", "Mode 3",
                                              "Mode 1 + Pinkie", "Mode 2 + Pinkie", "Mode 3 + Pinkie"};

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool toInt(const std::string& s, int& out) {
    std::string t = trim(s);
    if (t.empty()) return false;
    char* end = nullptr;
    long v = strtol(t.c_str(), &end, 10);
    if (*end != 0) return false;
    out = (int)v;
    return true;
}

std::string cleanLabel(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '"') r += '\'';
        else if (c == '\n' || c == '\r' || c == '\t') r += ' ';
        else r += c;
    }
    return r;
}

int clampInt(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

}  // namespace

const char* ledName(int led) { return led >= 0 && led < kLedCount ? kLedNames[led] : ""; }
const char* ledTitle(int led) { return led >= 0 && led < kLedCount ? kLedTitles[led] : ""; }
bool ledIsSingleColor(int led) { return led == LedFire || led == LedThrottle; }
const char* colorName(Color c) { return kColorNames[(int)c & 3]; }
const char* buttonName(int button) { return button >= 1 && button <= kMaxButton ? kButtonNames[button] : ""; }
const char* layerName(int layer) { return layer >= 0 && layer < kLayerCount ? kLayerNames[layer] : ""; }

const char* compareName(Compare c) {
    static const char* const n[kCompareCount] = {">", ">=", "<", "<=", "==", "!="};
    return n[(int)c];
}

bool compareValues(Compare c, double v, double limit) {
    switch (c) {
        case Compare::Greater: return v > limit;
        case Compare::GreaterEq: return v >= limit;
        case Compare::Less: return v < limit;
        case Compare::LessEq: return v <= limit;
        case Compare::Equal: return v == limit;
        case Compare::NotEqual: return v != limit;
    }
    return false;
}

const char* ruleColorName(int color) { return color >= 0 && color <= 3 ? kColorNames[color] : "mode"; }

namespace {
TriggerState testCondition(const Condition& c, const DatarefReader& read) {
    double v = 0;
    if (!c.active() || !read || !read(c.dataref, v)) return TriggerState::Unknown;
    return compareValues(c.op, v, c.value) != c.negate ? TriggerState::True : TriggerState::False;
}
Color litOnly(int led, Color c) { return ledIsSingleColor(led) && c != Color::Off ? Color::Green : c; }
}  // namespace

TriggerState evaluate(const Trigger& t, const DatarefReader& read) {
    TriggerState a = testCondition(t, read);
    if (!t.hasSecond()) return a;
    TriggerState b = testCondition(t.second, read);
    if (a == TriggerState::Unknown || b == TriggerState::Unknown) return TriggerState::Unknown;
    bool both = a == TriggerState::True && b == TriggerState::True;
    bool either = a == TriggerState::True || b == TriggerState::True;
    return (t.join == Join::And ? both : either) ? TriggerState::True : TriggerState::False;
}

Color ruleColor(const LightRule& r, int led, bool valid, double current, Color modeColor) {
    if (!r.active() || !valid) return modeColor;
    int c = compareValues(r.op, current, r.value) != r.negate ? r.thenColor : r.elseColor;
    Color out = c >= 0 && c <= 3 ? (Color)c : modeColor;
    return litOnly(led, out);
}

Color liveColor(const std::vector<LightRule>& rules, int led, Color modeColor, const DatarefReader& read) {
    for (const LightRule& r : rules) {
        if (r.led != led || !r.active()) continue;
        TriggerState st = evaluate(r, read);
        if (st == TriggerState::Unknown) continue;
        int c = st == TriggerState::True ? r.thenColor : r.elseColor;
        if (c < 0 || c > 3) continue;   // "mode colour": this rule has nothing to say
        return litOnly(led, (Color)c);
    }
    return modeColor;
}

int ruleUsingLed(const std::vector<LightRule>& rules, int led, int self) {
    for (int i = 0; i < (int)rules.size(); ++i)
        if (i != self && rules[i].led == led) return i;
    return -1;
}

int firstFreeLed(const std::vector<LightRule>& rules) {
    for (int l = 0; l < kLedCount; ++l)
        if (ruleUsingLed(rules, l, -1) < 0) return l;
    return -1;
}

const char* effectName(Effect e) {
    return e == Effect::Steady ? "steady" : e == Effect::Blink ? "blink" : "alternate";
}

Color alarmColorAt(const Alarm& a, double t) {
    Color c1 = (Color)(a.color & 3), c2 = (Color)(a.color2 & 3);
    if (a.effect == Effect::Steady) return litOnly(a.led, c1);
    double period = a.fast ? kBlinkFast : kBlinkSlow;
    bool first = std::fmod(t < 0 ? 0 : t, period) < period / 2;
    if (a.effect == Effect::Blink) return first ? litOnly(a.led, c1) : Color::Off;
    return litOnly(a.led, first ? c1 : c2);
}

int activeAlarm(const std::vector<Alarm>& alarms, int led, const DatarefReader& read) {
    for (int i = 0; i < (int)alarms.size(); ++i)
        if (alarms[i].led == led && alarms[i].active() && evaluate(alarms[i], read) == TriggerState::True) return i;
    return -1;
}

Color shownColor(const std::vector<Alarm>& alarms, const std::vector<LightRule>& rules, int led, Color modeColor,
                 double t, const DatarefReader& read) {
    int a = activeAlarm(alarms, led, read);
    if (a >= 0) return alarmColorAt(alarms[a], t);
    return liveColor(rules, led, modeColor, read);
}

int followBrightness(const BrightnessLink& b, int fixed, const DatarefReader& read) {
    double v = 0;
    if (!b.active() || !read || !read(b.dataref, v)) return fixed;
    int lo = clampInt(b.outLow, 0, 127), hi = clampInt(b.outHigh, 0, 127);
    double span = b.inHigh - b.inLow;
    double f = span == 0 ? (v >= b.inHigh ? 1.0 : 0.0) : (v - b.inLow) / span;
    f = std::max(0.0, std::min(1.0, f));
    return clampInt((int)std::lround(lo + (hi - lo) * f), 0, 127);
}

bool masterOn(const Trigger& t, const DatarefReader& read) {
    return !t.active() || evaluate(t, read) != TriggerState::False;
}

std::vector<std::string> datarefsUsed(const std::vector<Alarm>& alarms, const std::vector<LightRule>& rules,
                                      const std::vector<Trigger>& masters) {
    std::vector<std::string> out;
    auto add = [&out](const Trigger& t) {
        for (const std::string& r : {t.dataref, t.hasSecond() ? t.second.dataref : std::string()})
            if (!r.empty() && std::find(out.begin(), out.end(), r) == out.end()) out.push_back(r);
    };
    for (const Alarm& a : alarms) add(a);
    for (const LightRule& r : rules) add(r);
    for (const Trigger& t : masters) add(t);
    return out;
}

namespace {
LightRule mk(const char* ref, Compare op, double v, Color t, Color e) {
    LightRule r;
    r.dataref = ref;
    r.op = op;
    r.value = v;
    r.thenColor = (int)t;
    r.elseColor = (int)e;
    return r;
}
const RulePreset kPresets[] = {
    {"Parking brake: red on, green off", mk("sim/flightmodel/controls/parkbrake", Compare::Greater, 0.5, Color::Red, Color::Green)},
    {"Gear handle down: green, up: off", mk("sim/cockpit/switches/gear_handle_status", Compare::Equal, 1, Color::Green, Color::Off)},
    {"Flaps out: amber", mk("sim/cockpit2/controls/flap_handle_deploy_ratio", Compare::Greater, 0.05, Color::Amber, Color::Off)},
    {"Speedbrake out: amber", mk("sim/cockpit2/controls/speedbrake_ratio", Compare::Greater, 0.05, Color::Amber, Color::Off)},
    {"Autopilot engaged: green", mk("sim/cockpit2/autopilot/servos_on", Compare::Equal, 1, Color::Green, Color::Off)},
    {"Landing lights on: amber", mk("sim/cockpit/electrical/landing_lights_on", Compare::Equal, 1, Color::Amber, Color::Off)},
    {"Stall warning: red", mk("sim/cockpit2/annunciators/stall_warning", Compare::Equal, 1, Color::Red, Color::Off)},
    {"Master warning: red", mk("sim/cockpit2/annunciators/master_warning", Compare::Equal, 1, Color::Red, Color::Off)},
};

Alarm al(const char* name, const char* ref, int led, Color c, Color c2, Effect e, bool fast) {
    Alarm a;
    a.name = name;
    a.dataref = ref;
    a.op = Compare::Greater;
    a.value = 0;
    a.led = led;
    a.color = (int)c;
    a.color2 = (int)c2;
    a.effect = e;
    a.fast = fast;
    return a;
}
const AlarmPreset kAlarmPresets[] = {
    {"Stall: red flashing", al("Stall", "sim/cockpit2/annunciators/stall_warning", LedPov, Color::Red, Color::Amber, Effect::Blink, true)},
    {"Gear unsafe: amber flashing", al("Gear unsafe", "sim/cockpit2/annunciators/gear_unsafe", LedB, Color::Amber, Color::Red, Effect::Blink, false)},
    {"Low fuel: red / amber", al("Low fuel", "sim/cockpit2/annunciators/fuel_quantity", LedD, Color::Red, Color::Amber, Effect::Alternate, false)},
    {"Overspeed (Vne): red flashing", al("Overspeed", "sim/flightmodel/failures/over_vne", LedT1, Color::Red, Color::Amber, Effect::Blink, true)},
    {"Autopilot disconnect: amber flashing", al("Autopilot disconnect", "sim/cockpit2/annunciators/autopilot_disconnect", LedT2, Color::Amber, Color::Red, Effect::Blink, true)},
};
}  // namespace

int rulePresetCount() { return (int)(sizeof(kPresets) / sizeof(kPresets[0])); }
const RulePreset& rulePreset(int i) { return kPresets[i < 0 || i >= rulePresetCount() ? 0 : i]; }
int alarmPresetCount() { return (int)(sizeof(kAlarmPresets) / sizeof(kAlarmPresets[0])); }
const AlarmPreset& alarmPreset(int i) { return kAlarmPresets[i < 0 || i >= alarmPresetCount() ? 0 : i]; }

std::string modeLabel(int mode, bool pinkie) {
    return (pinkie ? "Pinkie + " : "Mode ") + std::to_string(mode);
}

Profile::Profile() {
    for (auto& m : leds)
        for (auto& c : m) c = Color::Green;
    for (auto& m : ledsPinkie)
        for (auto& c : m) c = Color::Green;
}

const Binding* Profile::exact(int button, int layer) const {
    for (const Binding& b : bindings)
        if (b.button == button && b.layer == layer) return &b;
    return nullptr;
}

const Binding* Profile::find(int button, int layer) const {
    const Binding* b = exact(button, layer);
    if (b && !b->empty()) return b;
    b = exact(button, kLayerAny);
    return b && !b->empty() ? b : nullptr;
}

Binding& Profile::edit(int button, int layer) {
    for (Binding& b : bindings)
        if (b.button == button && b.layer == layer) return b;
    Binding nb;
    nb.button = button;
    nb.layer = layer;
    bindings.push_back(nb);
    return bindings.back();
}

void Profile::prune() {
    bindings.erase(std::remove_if(bindings.begin(), bindings.end(), [](const Binding& b) { return b.empty(); }),
                   bindings.end());
}

bool Profile::isReserved(int button) const {
    for (int m : modeButtons)
        if (button == m) return true;
    if (pinkieShift && button == shiftButton) return true;
    if (clutchSafe && button == clutchButton) return true;
    return false;
}

Profile defaultProfile() {
    Profile p;
    Binding& up = p.edit(35, kLayerAny);
    up.label = "Screen page -";
    up.press.push_back({StepKind::Command, "x52pro/mfd_page_prev", 0});
    Binding& dn = p.edit(36, kLayerAny);
    dn.label = "Screen page +";
    dn.press.push_back({StepKind::Command, "x52pro/mfd_page_next", 0});
    addWheelBindings(p);
    addLookBindings(p);
    addNubBindings(p);
    return p;
}

bool addWheelBindings(Profile& p) {
    static const struct { int button; const char* label; const char* cmd; } kWheel[] = {
        {37, "Screen cursor up", "x52pro/mfd_up"},
        {38, "Screen cursor down", "x52pro/mfd_down"},
        {39, "Screen select", "x52pro/mfd_select"},
    };
    bool added = false;
    for (const auto& w : kWheel) {
        bool used = false;
        for (const Binding& b : p.bindings)
            if (b.button == w.button && !b.empty()) used = true;
        if (used) continue;
        Binding& b = p.edit(w.button, kLayerAny);
        b.label = w.label;
        b.press.push_back({StepKind::Command, w.cmd, 0});
        added = true;
    }
    return added;
}

// 1.2.6 gave POV 1 the keys Q E R F; keystrokes lagged and needed macOS permission, so the same thing is now done with
// X-Plane's own commands. A binding still exactly as 1.2.6 made it is switched over; anything the user changed stays.
static void upgradeLookKeys(Profile& p) {
    static const struct { int button; const char* label; const char* key; const char* cmd; } kOld[] = {
        {kPov1Up, "Look up", "r", "sim/general/rot_up"},       {kPov1Up + 1, "Look right", "e", "sim/general/rot_right"},
        {kPov1Up + 2, "Look down", "f", "sim/general/rot_down"}, {kPov1Up + 3, "Look left", "q", "sim/general/rot_left"},
    };
    for (const auto& o : kOld)
        for (Binding& b : p.bindings)
            if (b.button == o.button && b.layer == kLayerAny && b.label == o.label && b.press.size() == 1 &&
                b.press[0].kind == StepKind::Key && b.press[0].text == o.key && b.hold.empty())
                b.press[0] = {StepKind::Command, o.cmd, 0};
}

bool addLookBindings(Profile& p) {
    static const struct { int button; const char* label; const char* cmd; } kLook[] = {
        {kPov1Up, "Look up", "sim/general/rot_up"},          // what X-Plane's Q E R F keys do: rotate the view
        {kPov1Up + 1, "Look right", "sim/general/rot_right"},
        {kPov1Up + 2, "Look down", "sim/general/rot_down"},
        {kPov1Up + 3, "Look left", "sim/general/rot_left"},
    };
    bool added = false;
    for (const auto& w : kLook) {
        bool used = false;
        for (const Binding& b : p.bindings)
            if (b.button == w.button && !b.empty()) used = true;
        if (used) continue;
        Binding& b = p.edit(w.button, kLayerAny);
        b.label = w.label;
        b.press.push_back({StepKind::Command, w.cmd, 0});
        added = true;
    }
    return added;
}

bool addNubBindings(Profile& p) {
    static const struct { int button; const char* label; const char* cmd; } kNub[] = {
        {kNubUp, "Pan up", "sim/general/up"},           {kNubUp + 1, "Pan right", "sim/general/right"},
        {kNubUp + 2, "Pan down", "sim/general/down"},   {kNubUp + 3, "Pan left", "sim/general/left"},
        {kNubUp + 4, "Pan up fast", "sim/general/up_fast"},      {kNubUp + 5, "Pan right fast", "sim/general/right_fast"},
        {kNubUp + 6, "Pan down fast", "sim/general/down_fast"},  {kNubUp + 7, "Pan left fast", "sim/general/left_fast"},
    };
    bool added = false;
    for (const auto& w : kNub) {
        bool used = false;
        for (const Binding& b : p.bindings)
            if (b.button == w.button && !b.empty()) used = true;
        if (used) continue;
        Binding& b = p.edit(w.button, kLayerAny);
        b.label = w.label;
        b.press.push_back({StepKind::Command, w.cmd, 0});
        added = true;
    }
    return added;
}

bool datarefStepValue(const Step& s, bool haveCurrent, double current, double& out) {
    switch (s.op) {
        case DatarefOp::Set: out = s.value; return true;
        case DatarefOp::Toggle:
            out = haveCurrent && std::fabs(current - s.value) <= std::fabs(current - s.value2) ? s.value2 : s.value;
            return true;
        case DatarefOp::Add:
            if (!haveCurrent) return false;
            out = current + s.value;
            if (s.lo < s.hi) out = std::max(s.lo, std::min(s.hi, out));
            return true;
    }
    return false;
}

namespace {
std::string numberText(double v) {
    std::ostringstream o;
    o << v;
    return o.str();
}
}  // namespace

std::string actionToText(const Action& a) {
    std::string s;
    for (size_t i = 0; i < a.size(); ++i) {
        if (i) s += " ; ";
        switch (a[i].kind) {
            case StepKind::Command: s += "cmd:" + a[i].text; break;
            case StepKind::Key: s += "key:" + a[i].text; break;
            case StepKind::Wait: s += "wait:" + std::to_string(a[i].ms); break;
            case StepKind::Dataref: {
                const Step& st = a[i];
                if (st.op == DatarefOp::Set) s += "set:" + st.text + "=" + numberText(st.value);
                else if (st.op == DatarefOp::Toggle) s += "toggle:" + st.text + "=" + numberText(st.value) + "," + numberText(st.value2);
                else {
                    s += "add:" + st.text + "=" + numberText(st.value);
                    if (st.lo < st.hi) s += "," + numberText(st.lo) + "," + numberText(st.hi);
                }
                break;
            }
        }
    }
    return s;
}

namespace {
// "<dataref>=<n>[,<n>[,<n>]]" for set / toggle / add; toggle may leave the numbers out (0 and 1).
bool datarefStepFromText(const std::string& kind, const std::string& arg, Step& st) {
    size_t eq = arg.find('=');
    std::string ref = trim(eq == std::string::npos ? arg : arg.substr(0, eq));
    if (ref.find_first_of(" \t") != std::string::npos) return false;
    std::vector<double> nums;
    if (eq != std::string::npos) {
        std::stringstream ns(arg.substr(eq + 1));
        std::string item;
        while (std::getline(ns, item, ',')) {
            item = trim(item);
            char* end = nullptr;
            double v = strtod(item.c_str(), &end);
            if (item.empty() || *end) return false;
            nums.push_back(v);
        }
    }
    st.kind = StepKind::Dataref;
    st.text = ref;
    if (kind == "set") {
        if (nums.size() != 1) return false;
        st.op = DatarefOp::Set;
        st.value = nums[0];
    } else if (kind == "toggle") {
        if (!nums.empty() && nums.size() != 2) return false;
        st.op = DatarefOp::Toggle;
        if (nums.size() == 2) { st.value = nums[0]; st.value2 = nums[1]; }
        else { st.value = 0; st.value2 = 1; }
    } else {
        if (nums.size() != 1 && nums.size() != 3) return false;
        st.op = DatarefOp::Add;
        st.value = nums[0];
        if (nums.size() == 3) { st.lo = nums[1]; st.hi = nums[2]; }
    }
    return true;
}
}  // namespace

bool actionFromText(const std::string& text, Action& out, std::string* error) {
    out.clear();
    std::stringstream ss(text);
    std::string part;
    while (std::getline(ss, part, ';')) {
        part = trim(part);
        if (part.empty()) continue;
        size_t colon = part.find(':');
        std::string kind = colon == std::string::npos ? part : part.substr(0, colon);
        std::string arg = colon == std::string::npos ? "" : trim(part.substr(colon + 1));
        Step st;
        // An empty command, key or dataref is a step still being filled in (the windows add it blank
        // and save at once); it is written and read back so it survives a reload.
        if (kind == "cmd") {
            st.kind = StepKind::Command;
            st.text = arg;
        } else if (kind == "key") {
            st.kind = StepKind::Key;
            st.text = arg;
        } else if (kind == "wait" && toInt(arg, st.ms) && st.ms >= 0) {
            st.kind = StepKind::Wait;
        } else if ((kind == "set" || kind == "toggle" || kind == "add") && datarefStepFromText(kind, arg, st)) {
            // parsed
        } else {
            if (error) *error = "cannot understand step '" + part + "'";
            out.clear();
            return false;
        }
        out.push_back(st);
    }
    return true;
}

namespace {
std::string numText(double v) {
    std::ostringstream o;
    o << v;
    return o.str();
}
// " and [not] <op> <number> <dataref>", or "" for a trigger with one condition
std::string secondText(const Trigger& t) {
    if (t.join == Join::None) return "";
    return std::string(t.join == Join::And ? " and " : " or ") + (t.second.negate ? "not " : "") + compareName(t.second.op) +
           " " + numText(t.second.value) + " " + (t.second.dataref.empty() ? "-" : t.second.dataref);
}
}  // namespace

std::string serialize(const Profile& p) {
    std::ostringstream o;
    o << "x52pro-profile 7\n";
    o << "# X52 Pro button profile. Written by the X52 Pro plugin; safe to edit by hand.\n";
    o << "name = " << cleanLabel(p.name) << "\n";
    if (p.standard) o << "# standard: a ready-made profile, never changed by the app or the plugin (changes go to a copy)\nstandard = 1\n";
    if (!p.basedOn.empty()) o << "based_on = " << p.basedOn << "\n";
    o << "pinkie_shift = " << (p.pinkieShift ? 1 : 0) << "\n";
    o << "shift_button = " << p.shiftButton << "\n";
    o << "mode_buttons = " << p.modeButtons[0] << " " << p.modeButtons[1] << " " << p.modeButtons[2] << "\n";
    o << "clutch_safe = " << (p.clutchSafe ? 1 : 0) << "\n";
    o << "clutch_button = " << p.clutchButton << "\n";
    o << "hold_ms = " << p.holdMs << "\n";
    o << "mode_settle_ms = " << p.modeSettleMs << "\n";
    o << "mfd_brightness = " << p.mfdBrightness << "\n";
    o << "led_brightness = " << p.ledBrightness << "\n";
    o << "clutch_latched = " << (p.clutchLatched ? 1 : 0) << "\n";
    o << "# clocks: clock1_source 0 GMT, 1 simulator local time, 2 Mac local time, 3 GMT + clock1_offset minutes.\n";
    o << "# clock 2 and 3 are minutes from GMT. date_format: 0 DD-MM-YY, 1 MM-DD-YY, 2 YY-MM-DD\n";
    o << "clock1_source = " << p.clock1Source << "\n";
    o << "clock1_offset = " << p.clock1Offset << "\n";
    o << "clock2_offset = " << p.clockOffset[0] << "\n";
    o << "clock3_offset = " << p.clockOffset[1] << "\n";
    for (int c = 0; c < 3; ++c) o << "clock" << (c + 1) << "_12h = " << (p.clock12h[c] ? 1 : 0) << "\n";
    o << "date_format = " << p.dateFormat << "\n";
    o << "com_833 = " << (p.com833 ? 1 : 0) << "\n";
    o << "axis_override = " << (p.axes.overrideXPlane ? 1 : 0) << "\n";
    o << "# axis_override = 1: the plugin drives the stick and twist. throttle_plugin = 1: it drives the throttle (engine power and levers)\n";
    o << "throttle_plugin = " << (p.axes.pluginThrottle ? 1 : 0) << "\n";
    o << "# axis_<name> = dead zone %, expo %, inverted (1/0)\n";
    for (int u : {0x30, 0x31, 0x35, 0x32}) {
        const AxisCurve& c = axisCurve(p.axes, u);
        o << "axis_" << axisTarget(0x01, u)->name << " = " << c.deadzone << " " << c.expo << " " << (c.invert ? 1 : 0) << "\n";
    }
    o << "# stick_smoothing = ms over which small stick changes are smoothed (big moves pass at once; 0 off). stick_damping = 0..100 pitch/roll damper (0 off)\n";
    o << "stick_smoothing = " << p.axes.smoothingMs << "\n";
    o << "stick_damping = " << p.axes.damping << "\n";
    o << "# steady_<lower|upper|slider> = a dial or slider ignores changes smaller than this % of its travel\n";
    o << "steady_lower = " << p.axes.lowerRotary.deadzone << "\n";
    o << "steady_upper = " << p.axes.upperRotary.deadzone << "\n";
    o << "steady_slider = " << p.axes.slider.deadzone << "\n";
    o << "# rotary_<lower|upper> = what the dial does: 0 nothing, 1 speed brake (armed, retracted, half, full), 2 zoom. rotary_<lower|upper>_invert = 1 turns it the other way\n";
    o << "rotary_lower = " << (int)p.axes.lowerUse << "\n";
    o << "rotary_lower_invert = " << (p.axes.lowerRotary.invert ? 1 : 0) << "\n";
    o << "rotary_upper = " << (int)p.axes.upperUse << "\n";
    o << "rotary_upper_invert = " << (p.axes.upperRotary.invert ? 1 : 0) << "\n";
    o << "# slider_flaps = 1: the slider on the throttle sets the flaps, at the aircraft's detents (high reading = deployed; slider_invert = 1 swaps)\n";
    o << "slider_flaps = " << (p.axes.sliderFlaps ? 1 : 0) << "\n";
    o << "slider_invert = " << (p.axes.slider.invert ? 1 : 0) << "\n";
    o << "dial_accel = " << (p.dial.on ? 1 : 0) << "\n";
    o << "dial_slow_ms = " << p.dial.slowMs << "\n";
    o << "dial_fast_ms = " << p.dial.fastMs << "\n";
    o << "dial_factor = " << p.dial.factor << "\n";
    for (int m = 0; m < 3; ++m) {
        o << "leds " << (m + 1) << " =";
        for (int l = 0; l < kLedCount; ++l) o << " " << ledName(l) << ":" << colorName(p.leds[m][l]);
        o << "\n";
    }
    o << "# leds with the pinkie held, per mode\n";
    for (int m = 0; m < 3; ++m) {
        o << "ledsp " << (m + 1) << " =";
        for (int l = 0; l < kLedCount; ++l) o << " " << ledName(l) << ":" << colorName(p.ledsPinkie[m][l]);
        o << "\n";
    }
    o << "# rule <light> <op> <number> <colour if true> <colour if not> [not] <dataref> [and|or [not] <op> <number> <dataref>]\n";
    o << "#   colours: off green amber red mode. One rule per light; the first rule that has a colour wins. A dataref of - is not chosen yet.\n";
    for (const LightRule& r : p.rules) {
        if (r.led < 0 || r.led >= kLedCount) continue;
        o << "rule " << ledName(r.led) << " " << compareName(r.op) << " " << numText(r.value) << " " << ruleColorName(r.thenColor)
          << " " << ruleColorName(r.elseColor) << " " << (r.negate ? "not " : "") << (r.dataref.empty() ? "-" : r.dataref)
          << secondText(r) << "\n";
    }
    o << "# alarm <light> <steady|blink|alternate> <slow|fast> <colour> <colour 2> \"name\" [not] <op> <number> <dataref> [and|or [not] <op> <number> <dataref>]\n";
    o << "#   alarms beat the rules above and the mode colours; the first alarm that is triggered wins, one that is not leaves the light to the next.\n";
    for (const Alarm& a : p.alarms) {
        if (a.led < 0 || a.led >= kLedCount) continue;
        std::string name = a.name;
        for (char& ch : name) if (ch == '"') ch = '\'';
        o << "alarm " << ledName(a.led) << " " << effectName(a.effect) << " " << (a.fast ? "fast" : "slow") << " "
          << colorName((Color)(a.color & 3)) << " " << colorName((Color)(a.color2 & 3)) << " \"" << name << "\" "
          << (a.negate ? "not " : "") << compareName(a.op) << " " << numText(a.value) << " "
          << (a.dataref.empty() ? "-" : a.dataref) << secondText(a) << "\n";
    }
    o << serializePages(p.pages);
    auto words = [](const std::vector<std::string>& v) {
        std::string s;
        for (const std::string& w : v) s += " " + w;
        return s;
    };
    if (!p.wheelOrder.empty()) o << "wheel_order =" << words(p.wheelOrder) << "\n";
    if (!p.wheelHidden.empty()) o << "wheel_hidden =" << words(p.wheelHidden) << "\n";
    for (int m = 0; m < 3; ++m)
        if (p.modeWheel[m].own) {
            o << "wheel_order." << m + 1 << " =" << words(p.modeWheel[m].order) << "\n";
            o << "wheel_hidden." << m + 1 << " =" << words(p.modeWheel[m].hidden) << "\n";
        }
    if (p.radioSide) o << "radio_layout = side\n";
    o << "# master <screen|lights> [not] <op> <number> <dataref> [and|or [not] <op> <number> <dataref>]\n";
    o << "#   the screen (or every light) is on only while this is true, e.g. avionics power. No master line: always on.\n";
    for (int w = 0; w < 2; ++w) {
        const Trigger& t = w ? p.masterLights : p.masterScreen;
        if (!t.active()) continue;
        o << "master " << (w ? "lights " : "screen ") << (t.negate ? "not " : "") << compareName(t.op) << " "
          << numText(t.value) << " " << t.dataref << secondText(t) << "\n";
    }
    o << "# brightness <screen|lights> <value at dim> <value at bright> <dim 0..127> <bright 0..127> <dataref>\n";
    o << "#   the brightness follows the dataref between those two points. No line: the fixed brightness above.\n";
    for (int w = 0; w < 2; ++w) {
        const BrightnessLink& b = w ? p.brightLights : p.brightScreen;
        if (!b.active()) continue;
        o << "brightness " << (w ? "lights " : "screen ") << numText(b.inLow) << " " << numText(b.inHigh) << " "
          << b.outLow << " " << b.outHigh << " " << b.dataref << "\n";
    }
    o << "# bind <button> <any|1..6> <press|hold> \"label\" step ; step ...\n";
    o << "#   layers 1-3 = mode 1-3, 4-6 = mode 1-3 with the pinkie held\n";
    o << "#   steps: cmd:<x-plane command>   key:<combo e.g. shift+f1>   wait:<milliseconds>\n";
    o << "#          set:<dataref>=<value>   toggle:<dataref>=<a>,<b>   add:<dataref>=<amount>[,<min>,<max>]\n";
    std::vector<const Binding*> sorted;
    for (const Binding& b : p.bindings) sorted.push_back(&b);
    std::stable_sort(sorted.begin(), sorted.end(), [](const Binding* a, const Binding* b) {
        return a->button != b->button ? a->button < b->button : a->layer < b->layer;
    });
    for (const Binding* b : sorted) {
        std::string layer = b->layer == kLayerAny ? "any" : std::to_string(b->layer);
        if (!b->press.empty())
            o << "bind " << b->button << " " << layer << " press \"" << cleanLabel(b->label) << "\" "
              << actionToText(b->press) << "\n";
        if (!b->hold.empty())
            o << "bind " << b->button << " " << layer << " hold \"" << cleanLabel(b->holdLabel) << "\" "
              << actionToText(b->hold) << "\n";
    }
    return o.str();
}

namespace {

bool parseLeds(const std::string& key, size_t prefix, bool pinkie, const std::string& value, Profile& p, std::string& err) {
    int mode = 0;
    if (!toInt(key.substr(prefix), mode) || mode < 1 || mode > 3) {
        err = "leds needs a mode 1-3";
        return false;
    }
    std::stringstream ss(value);
    std::string item;
    while (ss >> item) {
        size_t colon = item.find(':');
        if (colon == std::string::npos) {
            err = "expected light:colour, got '" + item + "'";
            return false;
        }
        std::string ln = item.substr(0, colon), cn = item.substr(colon + 1);
        int led = -1, col = -1;
        for (int l = 0; l < kLedCount; ++l)
            if (ln == kLedNames[l]) led = l;
        for (int c = 0; c < 4; ++c)
            if (cn == kColorNames[c]) col = c;
        if (cn == "on") col = (int)Color::Green;
        if (led < 0 || col < 0) {
            err = "unknown light or colour '" + item + "'";
            return false;
        }
        p.ledAt(mode, pinkie, led) = (Color)col;
    }
    return true;
}

int parseRuleColor(const std::string& s) {
    if (s == "mode") return kRuleMode;
    for (int c = 0; c < 4; ++c)
        if (s == kColorNames[c]) return c;
    return -2;
}

int findLed(const std::string& n) {
    for (int l = 0; l < kLedCount; ++l)
        if (n == kLedNames[l]) return l;
    return -1;
}
int findOp(const std::string& n) {
    for (int c = 0; c < kCompareCount; ++c)
        if (n == compareName((Compare)c)) return c;
    return -1;
}

// "[not] <op> <number> <dataref>"
bool parseCondition(std::istream& in, Condition& c, std::string& err) {
    std::string tok, sOp, sValue, ref;
    in >> tok;
    c.negate = tok == "not";
    if (c.negate) in >> sOp; else sOp = tok;
    if (!(in >> sValue >> ref)) { err = "a condition needs: [not] op number dataref"; return false; }
    int op = findOp(sOp);
    char* end = nullptr;
    double value = strtod(sValue.c_str(), &end);
    if (op < 0) { err = "unknown comparison '" + sOp + "'"; return false; }
    if (sValue.empty() || *end) { err = "'" + sValue + "' is not a number"; return false; }
    c.op = (Compare)op;
    c.value = value;
    c.dataref = ref == "-" ? "" : ref;
    return true;
}

// Optional "and|or <condition>" at the end of a line.
bool parseSecond(std::istream& in, Trigger& t, std::string& err) {
    std::string j;
    if (!(in >> j)) return true;
    if (j != "and" && j != "or") { err = "expected 'and' or 'or', not '" + j + "'"; return false; }
    t.join = j == "and" ? Join::And : Join::Or;
    return parseCondition(in, t.second, err);
}

bool parseRule(const std::string& rest, Profile& p, std::string& err) {
    std::stringstream ss(rest);
    std::string sLed, sOp, sValue, sThen, sElse, ref;
    if (!(ss >> sLed >> sOp >> sValue >> sThen >> sElse >> ref)) {
        err = "rule needs: light op number colour colour dataref";
        return false;
    }
    bool negate = ref == "not";
    if (negate && !(ss >> ref)) { err = "rule needs: light op number colour colour dataref"; return false; }
    int led = findLed(sLed), op = findOp(sOp);
    char* end = nullptr;
    double value = strtod(sValue.c_str(), &end);
    int t = parseRuleColor(sThen), e = parseRuleColor(sElse);
    if (led < 0) { err = "unknown light '" + sLed + "'"; return false; }
    if (op < 0) { err = "unknown comparison '" + sOp + "'"; return false; }
    if (sValue.empty() || *end) { err = "'" + sValue + "' is not a number"; return false; }
    if (t == -2 || e == -2) { err = "colours are off, green, amber, red or mode"; return false; }
    LightRule r;
    r.led = led;
    r.dataref = ref == "-" ? "" : ref;
    r.op = (Compare)op;
    r.value = value;
    r.negate = negate;
    r.thenColor = t;
    r.elseColor = e;
    if (!parseSecond(ss, r, err)) return false;
    p.rules.push_back(r);
    return true;
}

bool parseMaster(const std::string& rest, Profile& p, std::string& err) {
    std::stringstream ss(rest);
    std::string which;
    ss >> which;
    if (which != "screen" && which != "lights") { err = "master is for 'screen' or 'lights'"; return false; }
    Trigger t;
    if (!parseCondition(ss, t, err)) return false;
    if (!parseSecond(ss, t, err)) return false;
    (which == "screen" ? p.masterScreen : p.masterLights) = t;
    return true;
}

bool parseBrightness(const std::string& rest, Profile& p, std::string& err) {
    std::stringstream ss(rest);
    std::string which, a, b, c, d, ref;
    if (!(ss >> which >> a >> b >> c >> d >> ref)) {
        err = "brightness needs: screen|lights low high dim bright dataref";
        return false;
    }
    if (which != "screen" && which != "lights") { err = "brightness is for 'screen' or 'lights'"; return false; }
    char *e1 = nullptr, *e2 = nullptr;
    BrightnessLink l;
    l.inLow = strtod(a.c_str(), &e1);
    l.inHigh = strtod(b.c_str(), &e2);
    int lo = 0, hi = 0;
    if (*e1 || *e2 || !toInt(c, lo) || !toInt(d, hi)) { err = "brightness values must be numbers"; return false; }
    l.outLow = clampInt(lo, 0, 127);
    l.outHigh = clampInt(hi, 0, 127);
    l.dataref = ref == "-" ? "" : ref;
    (which == "screen" ? p.brightScreen : p.brightLights) = l;
    return true;
}

bool parseAlarm(const std::string& rest, Profile& p, std::string& err) {
    std::stringstream ss(rest);
    std::string sLed, sEffect, sSpeed, sC1, sC2;
    if (!(ss >> sLed >> sEffect >> sSpeed >> sC1 >> sC2)) {
        err = "alarm needs: light effect speed colour colour \"name\" condition";
        return false;
    }
    std::string tail;
    std::getline(ss, tail);
    size_t q1 = tail.find('"');
    size_t q2 = q1 == std::string::npos ? std::string::npos : tail.find('"', q1 + 1);
    if (q2 == std::string::npos) { err = "the alarm's name must be in double quotes"; return false; }
    Alarm a;
    a.name = tail.substr(q1 + 1, q2 - q1 - 1);
    a.led = findLed(sLed);
    if (a.led < 0) { err = "unknown light '" + sLed + "'"; return false; }
    if (sEffect == "steady") a.effect = Effect::Steady;
    else if (sEffect == "blink") a.effect = Effect::Blink;
    else if (sEffect == "alternate") a.effect = Effect::Alternate;
    else { err = "effect is steady, blink or alternate"; return false; }
    if (sSpeed != "slow" && sSpeed != "fast") { err = "speed is slow or fast"; return false; }
    a.fast = sSpeed == "fast";
    int c1 = parseRuleColor(sC1), c2 = parseRuleColor(sC2);
    if (c1 < 0 || c2 < 0) { err = "alarm colours are off, green, amber or red"; return false; }
    a.color = c1;
    a.color2 = c2;
    std::stringstream cs(tail.substr(q2 + 1));
    if (!parseCondition(cs, a, err)) return false;
    if (!parseSecond(cs, a, err)) return false;
    p.alarms.push_back(a);
    return true;
}

bool parseBind(const std::string& rest, Profile& p, std::string& err) {
    std::stringstream ss(rest);
    std::string sButton, sLayer, sKind;
    ss >> sButton >> sLayer >> sKind;
    int button = 0, layer = 0;
    if (!toInt(sButton, button) || button < 1 || button > kMaxButton) {
        err = "button must be 1-" + std::to_string(kMaxButton);
        return false;
    }
    if (sLayer == "any") layer = kLayerAny;
    else if (!toInt(sLayer, layer) || layer < 1 || layer >= kLayerCount) {
        err = "layer must be 'any' or 1-6";
        return false;
    }
    if (sKind != "press" && sKind != "hold") {
        err = "expected 'press' or 'hold'";
        return false;
    }
    std::string tail;
    std::getline(ss, tail);
    size_t q1 = tail.find('"');
    size_t q2 = q1 == std::string::npos ? std::string::npos : tail.find('"', q1 + 1);
    if (q2 == std::string::npos) {
        err = "label must be in double quotes";
        return false;
    }
    std::string label = tail.substr(q1 + 1, q2 - q1 - 1);
    Action a;
    if (!actionFromText(tail.substr(q2 + 1), a, &err)) return false;
    Binding& b = p.edit(button, layer);
    if (sKind == "press") {
        b.label = label;
        b.press = a;
    } else {
        b.holdLabel = label;
        b.hold = a;
    }
    return true;
}

}  // namespace

bool parse(const std::string& text, Profile& out, std::string* error) {
    Profile p;
    p.version = 1;   // files without a readable header are treated as the first format
    std::stringstream ss(text);
    std::string raw;
    int lineNo = 0;
    bool pinkieSeen[3] = {false, false, false};
    bool throttleSeen = false;
    auto fail = [&](const std::string& msg) {
        if (error) *error = "line " + std::to_string(lineNo) + ": " + msg;
        return false;
    };
    while (std::getline(ss, raw)) {
        ++lineNo;
        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') continue;
        if (line.compare(0, 14, "x52pro-profile") == 0) {
            int v = 0;
            if (toInt(line.substr(14), v) && v >= 1) p.version = v;
            continue;
        }
        std::string err;
        if (line.compare(0, 5, "rule ") == 0) {
            if (!parseRule(line.substr(5), p, err)) return fail(err);
            continue;
        }
        if (line.compare(0, 5, "page ") == 0) {
            if (!parsePageLine(line.substr(5), p.pages, err)) return fail(err);
            continue;
        }
        if (line.compare(0, 4, "row ") == 0) {
            if (!parseRowLine(line.substr(4), p.pages, err)) return fail(err);
            continue;
        }
        if (line.compare(0, 11, "brightness ") == 0) {
            if (!parseBrightness(line.substr(11), p, err)) return fail(err);
            continue;
        }
        if (line.compare(0, 7, "master ") == 0) {
            if (!parseMaster(line.substr(7), p, err)) return fail(err);
            continue;
        }
        if (line.compare(0, 6, "alarm ") == 0) {
            if (!parseAlarm(line.substr(6), p, err)) return fail(err);
            continue;
        }
        if (line.compare(0, 5, "bind ") == 0) {
            if (!parseBind(line.substr(5), p, err)) return fail(err);
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) return fail("expected 'setting = value'");
        std::string key = trim(line.substr(0, eq)), value = trim(line.substr(eq + 1));
        int n = 0;
        if (key == "name") {
            p.name = value;
        } else if (key == "based_on") {
            p.basedOn = value;
        } else if (key.compare(0, 5, "ledsp") == 0) {
            if (!parseLeds(key, 5, true, value, p, err)) return fail(err);
            for (int m = 0; m < 3; ++m)
                if (key == "ledsp " + std::to_string(m + 1)) pinkieSeen[m] = true;
        } else if (key.compare(0, 4, "leds") == 0) {
            if (!parseLeds(key, 4, false, value, p, err)) return fail(err);
        } else if (key == "mode_buttons") {
            std::stringstream ms(value);
            int a = 0, b = 0, c = 0;
            if (!(ms >> a >> b >> c)) return fail("mode_buttons needs three button numbers");
            p.modeButtons[0] = a;
            p.modeButtons[1] = b;
            p.modeButtons[2] = c;
        } else if ((key.size() == 13 && key.compare(0, 12, "wheel_order.") == 0) ||
                   (key.size() == 14 && key.compare(0, 13, "wheel_hidden.") == 0)) {
            int m = key.back() - '1';
            if (m < 0 || m > 2) continue;   // not a mode: ignored so newer files still load
            WheelSet& ws = p.modeWheel[m];
            std::vector<std::string>& v = key[6] == 'o' ? ws.order : ws.hidden;
            ws.own = true;
            v.clear();
            std::stringstream mw(value);
            for (std::string w; mw >> w;) v.push_back(w);
        } else if (key == "radio_layout") {
            p.radioSide = value == "side";
        } else if (key == "wheel_order" || key == "wheel_hidden") {
            std::vector<std::string>& v = key == "wheel_order" ? p.wheelOrder : p.wheelHidden;
            v.clear();
            std::stringstream ws(value);
            for (std::string w; ws >> w;) v.push_back(w);
        } else if (key.compare(0, 5, "axis_") == 0 && key != "axis_override") {
            int usage = 0;
            for (int u : {0x30, 0x31, 0x35, 0x32})
                if (key == std::string("axis_") + axisTarget(0x01, u)->name) usage = u;
            if (!usage) continue;   // unknown axis: ignored so newer files still load
            std::stringstream as(value);
            int dz = 0, ex = 0, inv = 0;
            if (!(as >> dz >> ex >> inv)) return fail("'" + key + "' needs dead zone, expo and inverted");
            AxisCurve& c = axisCurve(p.axes, usage);
            c.deadzone = clampInt(dz, 0, 90);
            c.expo = clampInt(ex, 0, 100);
            c.invert = inv != 0;
        } else if (!toInt(value, n)) {
            return fail("'" + key + "' needs a number");
        } else if (key == "pinkie_shift") {
            p.pinkieShift = n != 0;
        } else if (key == "shift_button") {
            p.shiftButton = n;
        } else if (key == "clutch_safe") {
            p.clutchSafe = n != 0;
        } else if (key == "clutch_button") {
            p.clutchButton = n;
        } else if (key == "hold_ms") {
            p.holdMs = clampInt(n, 100, 5000);
        } else if (key == "mode_settle_ms") {
            p.modeSettleMs = clampInt(n, 0, 1000);
        } else if (key == "mfd_brightness") {
            p.mfdBrightness = clampInt(n, 0, 127);
        } else if (key == "led_brightness") {
            p.ledBrightness = clampInt(n, 0, 127);
        } else if (key == "clutch_latched") {
            p.clutchLatched = n != 0;
        } else if (key == "clock1_source") {
            p.clock1Source = clampInt(n, ClockGmt, ClockFixed);
        } else if (key == "clock1_offset") {
            p.clock1Offset = clampInt(n, kClockOffsetMin, kClockOffsetMax);
        } else if (key == "clock2_offset") {
            p.clockOffset[0] = clampInt(n, kClockOffsetMin, kClockOffsetMax);
        } else if (key == "clock3_offset") {
            p.clockOffset[1] = clampInt(n, kClockOffsetMin, kClockOffsetMax);
        } else if (key == "clock1_12h") {
            p.clock12h[0] = n != 0;
        } else if (key == "clock2_12h") {
            p.clock12h[1] = n != 0;
        } else if (key == "clock3_12h") {
            p.clock12h[2] = n != 0;
        } else if (key == "com_833") {
            p.com833 = n != 0;
        } else if (key == "steady_lower" || key == "steady_upper" || key == "steady_slider") {
            AxisCurve& c = key == "steady_lower" ? p.axes.lowerRotary : key == "steady_upper" ? p.axes.upperRotary : p.axes.slider;
            c.deadzone = clampInt((int)n, 0, 20);
        } else if (key == "rotary_lower" || key == "rotary_upper") {
            (key == "rotary_lower" ? p.axes.lowerUse : p.axes.upperUse) = n >= 0 && n <= 2 ? (DialUse)(int)n : DialUse::None;
        } else if (key == "rotary_lower_invert") {
            p.axes.lowerRotary.invert = n != 0;
        } else if (key == "rotary_upper_invert") {
            p.axes.upperRotary.invert = n != 0;
        } else if (key == "stick_smoothing") {
            p.axes.smoothingMs = clampInt(n, 0, 200);
        } else if (key == "stick_damping") {
            p.axes.damping = clampInt(n, 0, 100);
        } else if (key == "throttle_plugin") {
            p.axes.pluginThrottle = n != 0;
            throttleSeen = true;
        } else if (key == "slider_flaps") {
            p.axes.sliderFlaps = n != 0;
        } else if (key == "slider_invert") {
            p.axes.slider.invert = n != 0;
        } else if (key == "axis_override") {
            p.axes.overrideXPlane = n != 0;
        } else if (key == "dial_accel") {
            p.dial.on = n != 0;
        } else if (key == "dial_slow_ms") {
            p.dial.slowMs = clampInt(n, 20, 1000);
        } else if (key == "dial_fast_ms") {
            p.dial.fastMs = clampInt(n, 5, 1000);
        } else if (key == "dial_factor") {
            p.dial.factor = clampInt(n, 1, 50);
        } else if (key == "date_format") {
            p.dateFormat = clampInt(n, 0, 2);
        } else if (key == "standard") {
            p.standard = n != 0;

        }
        // Unknown numeric settings are ignored so newer files still load.
    }
    // Written before the screen cursor existed: add the wheel bindings. `version` stays
    // as read so a caller can tell the file on disk is still the old one.
    if (!throttleSeen) p.axes.pluginThrottle = p.axes.overrideXPlane;   // older files: one switch for both
    if (p.version < 2) addWheelBindings(p);
    // Before version 5 POV 1 (the hat switch) was not a set of buttons, so no profile could look around with it.
    if (p.version < 5) addLookBindings(p);
    else if (p.version < 6) upgradeLookKeys(p);
    // Before version 6 the throttle's mouse nub was not buttons either.
    if (p.version < 6) addNubBindings(p);
    // Before version 3 the last rule for a light won; now the first does. Reverse the order
    // once so the lights keep behaving as they did.
    if (p.version < 3) std::reverse(p.rules.begin(), p.rules.end());
    // Before version 4 every stick axis started linear with a 5 % dead zone, far too twitchy for the elevator
    // with the plugin driving the stick. A stick axis still at exactly that (never changed) gets the new
    // default curve; one that was set on purpose, and the throttle, stay as they are.
    if (p.version < 4) {
        const AxisSettings fresh;
        for (int u : {0x30, 0x31, 0x35}) {
            AxisCurve& c = axisCurve(p.axes, u);
            if (c.deadzone == 5 && c.expo == 0) {
                c.deadzone = axisCurve(fresh, u).deadzone;
                c.expo = axisCurve(fresh, u).expo;
            }
        }
    }
    // A file from before the pinkie lights: with the pinkie held the lights stay as the mode has them.
    for (int m = 0; m < 3; ++m)
        if (!pinkieSeen[m])
            for (int l = 0; l < kLedCount; ++l) p.ledsPinkie[m][l] = p.leds[m][l];
    out = p;
    if (error) error->clear();
    return true;
}

}  // namespace x52
