#include "profile.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace x52 {

namespace {

const char* const kLedNames[kLedCount] = {"fire", "a", "b", "d", "e", "t1", "t2", "t3", "pov", "clutch", "throttle"};
const char* const kLedTitles[kLedCount] = {"Fire button", "Button A", "Button B", "Button D", "Button E",
                                           "Toggle T1/T2", "Toggle T3/T4", "Toggle T5/T6", "POV hat 2",
                                           "Clutch (i)", "Throttle"};
const char* const kColorNames[4] = {"off", "green", "amber", "red"};

// HID button number (1-based) -> control, in the X52 Pro's report order (libx52io parser.c).
const char* const kButtonNames[kMaxButton + 1] = {
    "",
    "Trigger", "Fire", "A", "B", "C", "Pinkie", "D", "E",
    "T1", "T2", "T3", "T4", "T5", "T6",
    "Trigger 2nd stage", "Mouse button", "Scroll down", "Scroll up", "Scroll click",
    "Stick hat up", "Stick hat right", "Stick hat down", "Stick hat left",
    "Throttle hat up", "Throttle hat right", "Throttle hat down", "Throttle hat left",
    "Mode 1", "Mode 2", "Mode 3", "Clutch (i)",
    "MFD Function", "MFD Start/Stop", "MFD Reset", "MFD Page up", "MFD Page down",
    "MFD Up", "MFD Down", "MFD Select"};

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

Color ruleColor(const LightRule& r, int led, bool valid, double current, Color modeColor) {
    if (!r.active() || !valid) return modeColor;
    int c = compareValues(r.op, current, r.value) ? r.thenColor : r.elseColor;
    Color out = c >= 0 && c <= 3 ? (Color)c : modeColor;
    if (ledIsSingleColor(led) && out != Color::Off) out = Color::Green;
    return out;
}

Color liveColor(const std::vector<LightRule>& rules, int led, Color modeColor, const DatarefReader& read) {
    Color shown = modeColor;
    for (const LightRule& r : rules) {
        if (r.led != led || !r.active()) continue;
        double v = 0;
        bool ok = read && read(r.dataref, v);
        if (!ok) continue;
        int c = compareValues(r.op, v, r.value) ? r.thenColor : r.elseColor;
        if (c < 0 || c > 3) continue;   // "mode colour": this rule has nothing to say
        shown = ledIsSingleColor(led) && c != 0 ? Color::Green : (Color)c;
    }
    return shown;
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
    {"Parking brake: red on, green off", mk("sim/cockpit2/controls/parking_brake_ratio", Compare::Greater, 0.5, Color::Red, Color::Green)},
    {"Gear handle down: green, up: off", mk("sim/cockpit/switches/gear_handle_status", Compare::Equal, 1, Color::Green, Color::Off)},
    {"Flaps out: amber", mk("sim/cockpit2/controls/flap_ratio", Compare::Greater, 0.05, Color::Amber, Color::Off)},
    {"Speedbrake out: amber", mk("sim/cockpit2/controls/speedbrake_ratio", Compare::Greater, 0.05, Color::Amber, Color::Off)},
    {"Autopilot engaged: green", mk("sim/cockpit2/autopilot/servos_on", Compare::Equal, 1, Color::Green, Color::Off)},
    {"Landing lights on: amber", mk("sim/cockpit/electrical/landing_lights_on", Compare::Equal, 1, Color::Amber, Color::Off)},
    {"Stall warning: red", mk("sim/cockpit2/annunciators/stall_warning", Compare::Equal, 1, Color::Red, Color::Off)},
    {"Master warning: red", mk("sim/cockpit2/annunciators/master_warning", Compare::Equal, 1, Color::Red, Color::Off)},
};
}  // namespace

int rulePresetCount() { return (int)(sizeof(kPresets) / sizeof(kPresets[0])); }
const RulePreset& rulePreset(int i) { return kPresets[i < 0 || i >= rulePresetCount() ? 0 : i]; }

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

std::string actionToText(const Action& a) {
    std::string s;
    for (size_t i = 0; i < a.size(); ++i) {
        if (i) s += " ; ";
        switch (a[i].kind) {
            case StepKind::Command: s += "cmd:" + a[i].text; break;
            case StepKind::Key: s += "key:" + a[i].text; break;
            case StepKind::Wait: s += "wait:" + std::to_string(a[i].ms); break;
        }
    }
    return s;
}

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
        if (kind == "cmd" && !arg.empty()) {
            st.kind = StepKind::Command;
            st.text = arg;
        } else if (kind == "key" && !arg.empty()) {
            st.kind = StepKind::Key;
            st.text = arg;
        } else if (kind == "wait" && toInt(arg, st.ms) && st.ms >= 0) {
            st.kind = StepKind::Wait;
        } else {
            if (error) *error = "cannot understand step '" + part + "'";
            out.clear();
            return false;
        }
        out.push_back(st);
    }
    return true;
}

std::string serialize(const Profile& p) {
    std::ostringstream o;
    o << "x52pro-profile 2\n";
    o << "# X52 Pro button profile. Written by the X52 Pro plugin; safe to edit by hand.\n";
    o << "name = " << cleanLabel(p.name) << "\n";
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
    o << "# clocks: clock 1 is GMT; clock 2 and 3 are minutes from GMT. date_format: 0 DD-MM-YY, 1 MM-DD-YY, 2 YY-MM-DD\n";
    o << "clock2_offset = " << p.clockOffset[0] << "\n";
    o << "clock3_offset = " << p.clockOffset[1] << "\n";
    for (int c = 0; c < 3; ++c) o << "clock" << (c + 1) << "_12h = " << (p.clock12h[c] ? 1 : 0) << "\n";
    o << "date_format = " << p.dateFormat << "\n";
    o << "com_833 = " << (p.com833 ? 1 : 0) << "\n";
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
    o << "# rule <light> <op> <number> <colour if true> <colour if not> <dataref>   (colours: off green amber red mode)\n";
    o << "# several rules per light are fine; for one light the last rule that has a colour wins. A dataref of - is not chosen yet.\n";
    for (const LightRule& r : p.rules) {
        if (r.led < 0 || r.led >= kLedCount) continue;
        std::ostringstream v;
        v << r.value;
        o << "rule " << ledName(r.led) << " " << compareName(r.op) << " " << v.str() << " " << ruleColorName(r.thenColor)
          << " " << ruleColorName(r.elseColor) << " " << (r.dataref.empty() ? "-" : r.dataref) << "\n";
    }
    o << "# bind <button> <any|1..6> <press|hold> \"label\" step ; step ...\n";
    o << "#   layers 1-3 = mode 1-3, 4-6 = mode 1-3 with the pinkie held\n";
    o << "#   steps: cmd:<x-plane command>   key:<combo e.g. shift+f1>   wait:<milliseconds>\n";
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

bool parseRule(const std::string& rest, Profile& p, std::string& err) {
    std::stringstream ss(rest);
    std::string sLed, sOp, sValue, sThen, sElse, ref;
    if (!(ss >> sLed >> sOp >> sValue >> sThen >> sElse >> ref)) {
        err = "rule needs: light op number colour colour dataref";
        return false;
    }
    int led = -1, op = -1;
    for (int l = 0; l < kLedCount; ++l)
        if (sLed == kLedNames[l]) led = l;
    for (int c = 0; c < kCompareCount; ++c)
        if (sOp == compareName((Compare)c)) op = c;
    char* end = nullptr;
    double value = strtod(sValue.c_str(), &end);
    int t = parseRuleColor(sThen), e = parseRuleColor(sElse);
    if (led < 0) { err = "unknown light '" + sLed + "'"; return false; }
    if (op < 0) { err = "unknown comparison '" + sOp + "'"; return false; }
    if (sValue.empty() || *end) { err = "'" + sValue + "' is not a number"; return false; }
    if (t == -2 || e == -2) { err = "colours are off, green, amber, red or mode"; return false; }
    p.rules.emplace_back();
    LightRule& r = p.rules.back();
    r.led = led;
    r.dataref = ref == "-" ? "" : ref;
    r.op = (Compare)op;
    r.value = value;
    r.thenColor = t;
    r.elseColor = e;
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
        } else if (key == "date_format") {
            p.dateFormat = clampInt(n, 0, 2);
        }
        // Unknown numeric settings are ignored so newer files still load.
    }
    // Written before the screen cursor existed: add the wheel bindings. `version` stays
    // as read so a caller can tell the file on disk is still the old one.
    if (p.version < 2) addWheelBindings(p);
    // A file from before the pinkie lights: with the pinkie held the lights stay as the mode has them.
    for (int m = 0; m < 3; ++m)
        if (!pinkieSeen[m])
            for (int l = 0; l < kLedCount; ++l) p.ledsPinkie[m][l] = p.leds[m][l];
    out = p;
    if (error) error->clear();
    return true;
}

}  // namespace x52
