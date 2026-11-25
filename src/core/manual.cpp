#include "manual.h"

#include <algorithm>
#include <cstdio>

#include "axes.h"
#include "mfd.h"
#include "pageview.h"

namespace x52 {

namespace {
std::string yesNo(bool b) { return b ? "yes" : "no"; }

// ---------------------------------------------------------------- the fixed rows of the assignment tables
// The manual lists the same controls in the same order for every profile (a form, not a list of what is bound), so
// the template has a row for each. A cell's tag is "<table>.<control>.m<mode>": see docs/manual-data.md.
enum class Kind { Press, Tap, Hold, Hat1, Nub, Fixed, DialUpper, DialLower, Lever, Slider, StickAxes, Clutch };
struct Control {
    const char* id;
    int button;           // the button it is (Press, Tap, Hold), else 0
    Kind kind;
    const char* fixed;    // Fixed: what it always does
};

const Control kStick[] = {
    {"tr1", 1, Kind::Press, ""}, {"tr2", 15, Kind::Press, ""}, {"f_covered_fire", 2, Kind::Press, ""},
    {"a_tap", 3, Kind::Tap, ""}, {"a_hold", 3, Kind::Hold, ""}, {"b", 4, Kind::Press, ""}, {"c", 5, Kind::Press, ""},
    {"p_pinkie", 6, Kind::Press, ""}, {"h1_main_hat", 0, Kind::Hat1, ""},
    {"h2_up", 20, Kind::Press, ""}, {"h2_right", 21, Kind::Press, ""}, {"h2_down", 22, Kind::Press, ""}, {"h2_left", 23, Kind::Press, ""},
    {"t1_up", 9, Kind::Press, ""}, {"t1_down", 10, Kind::Press, ""}, {"t2_up", 11, Kind::Press, ""}, {"t2_down", 12, Kind::Press, ""},
    {"t3_up", 13, Kind::Press, ""}, {"t3_down", 14, Kind::Press, ""}, {"roll_pitch_twist", 0, Kind::StickAxes, ""},
};
const Control kThrottle[] = {
    {"d", 7, Kind::Press, ""}, {"e_press", 8, Kind::Press, ""}, {"i_press", 31, Kind::Clutch, ""},
    {"mb_mouse_button", 16, Kind::Press, ""}, {"mw_forward", 17, Kind::Press, ""}, {"mw_aft", 18, Kind::Press, ""},
    {"mw_press", 19, Kind::Press, ""},
    {"th_up", 24, Kind::Press, ""}, {"th_right", 25, Kind::Press, ""}, {"th_down", 26, Kind::Press, ""}, {"th_left", 27, Kind::Press, ""},
    {"ms_mouse_nub", 0, Kind::Nub, ""}, {"l_turn", 0, Kind::Fixed, "Change page"}, {"r_turn_press", 0, Kind::Fixed, "Cursor / select"},
    {"e_upper_ring", 0, Kind::DialUpper, ""}, {"i_lower_ring", 0, Kind::DialLower, ""},
    {"ax_lever", 0, Kind::Lever, ""}, {"sl_slider", 0, Kind::Slider, ""},
};

const char* const kNone = "\xE2\x80\x94";   // an em dash: nothing is assigned

std::string pressText(const Binding& b) { return !b.label.empty() ? b.label : actionToText(b.press); }
std::string holdText(const Binding& b) { return !b.holdLabel.empty() ? b.holdLabel : actionToText(b.hold); }

std::string joinDistinct(const std::vector<std::string>& v, const char* sep) {
    std::string out;
    std::vector<std::string> seen;
    for (const std::string& s : v) {
        if (s.empty() || std::find(seen.begin(), seen.end(), s) != seen.end()) continue;
        seen.push_back(s);
        if (!out.empty()) out += sep;
        out += s;
    }
    return out;
}

std::string dialText(DialUse use) {
    switch (use) {
        case DialUse::None: return "X-Plane joystick setup";
        case DialUse::Zoom: return "View zoom";
        default: return dialUseName(use);
    }
}

// What one control does in one layer (mode 1..3, pinkie held or not): the text of its cell; kNone when nothing.
std::string controlText(const Profile& p, const Control& c, int mode, bool pinkie) {
    int layer = mode + (pinkie ? 3 : 0);
    std::string text;
    switch (c.kind) {
        case Kind::Press:
        case Kind::Tap:
        case Kind::Hold: {
            if (c.button == p.shiftButton && p.shiftIn(mode) && !pinkie) return "Shift";
            const Binding* b = p.find(c.button, layer);
            if (!b || b->empty()) break;
            if (c.kind == Kind::Hold) text = b->hold.empty() ? "" : holdText(*b);
            else if (c.kind == Kind::Tap) text = b->press.empty() ? "" : pressText(*b);
            else {
                text = b->press.empty() ? (b->hold.empty() ? "" : holdText(*b)) : pressText(*b);
                if (!b->press.empty() && !b->hold.empty()) text += " (hold: " + holdText(*b) + ")";
            }
            break;
        }
        case Kind::Hat1: {
            std::vector<std::string> labels;
            for (int i = 0; i < 4; ++i) {
                const Binding* b = p.find(kPov1Up + i, layer);
                if (b && !b->empty()) labels.push_back(pressText(*b));
            }
            bool look = labels.size() == 4;
            for (const std::string& l : labels) look = look && l.compare(0, 5, "Look ") == 0;
            text = look ? "Look in direction" : joinDistinct(labels, " / ");
            break;
        }
        case Kind::Nub: {
            std::vector<std::string> labels;
            bool near = true, far = false;
            for (int i = 0; i < 8; ++i) {
                const Binding* b = p.find(kNubUp + i, layer);
                if (!b || b->empty()) {
                    if (i < 4) near = false;
                    continue;
                }
                labels.push_back(pressText(*b));
                if (i >= 4) far = true;
                if (pressText(*b).compare(0, 4, "Pan ") != 0) near = false;
            }
            text = near && !labels.empty() ? (far ? "Pan / far: fast pan" : "Pan") : joinDistinct(labels, " / ");
            break;
        }
        case Kind::Fixed: text = pinkie ? "" : c.fixed; break;
        case Kind::DialUpper: text = pinkie ? "" : dialText(p.axes.upperUse); break;
        case Kind::DialLower: text = pinkie ? "" : dialText(p.axes.lowerUse); break;
        case Kind::Lever: text = pinkie ? "" : (p.axes.pluginThrottle ? "Engine power" : "X-Plane"); break;
        case Kind::Slider: text = pinkie ? "" : (p.axes.sliderFlaps ? "Flap detents" : "X-Plane"); break;
        case Kind::StickAxes: text = pinkie ? "" : (p.axes.overrideXPlane ? "Plugin axes" : "X-Plane"); break;
        case Kind::Clutch:
            if (pinkie) break;
            if (p.clutchSafe) text = "Safe mode / picker";
            else if (const Binding* b = p.find(c.button, layer)) text = b->empty() ? "" : pressText(*b);
            break;
    }
    return text.empty() ? kNone : text;
}

std::string conditionText(const Condition& c) {
    char num[32];
    snprintf(num, sizeof num, "%g", c.value);
    return std::string(c.negate ? "NOT " : "") + c.dataref + " " + compareName(c.op) + " " + num;
}
std::string triggerText(const Trigger& t) {
    if (!t.active()) return "";
    std::string s = conditionText(t);
    if (t.hasSecond()) s += std::string(t.join == Join::And ? " AND " : " OR ") + conditionText(t.second);
    return s;
}
std::string ruleColorText(int c) { return c == kRuleMode ? "mode light" : ruleColorName(c); }

// The light icons (templates/light-icons, drawn by tools/make_light_icons.py): a value puts one in its cell with
// \x04<name>\x05. Names: off; g a r; <name>_flash; ra ar rg gr ag ga (primary colour left, secondary right) and the same
// with _flash; gar and gar_flash (the mode's own colour).
char colorLetter(int c) { return c == (int)Color::Green ? 'g' : c == (int)Color::Amber ? 'a' : 'r'; }
std::string picture(const std::string& name) { return "\x04" + name + "\x05"; }
std::string dot(int c, bool flashing = false) {
    if (c == (int)Color::Off) return picture("off");
    return picture(std::string(1, colorLetter(c)) + (flashing ? "_flash" : ""));
}
std::string pairDot(int primary, int secondary, bool flashing = false) {
    if (primary == (int)Color::Off || secondary == (int)Color::Off || primary == secondary) return dot(primary, flashing);
    return picture(std::string(1, colorLetter(primary)) + colorLetter(secondary) + (flashing ? "_flash" : ""));
}
std::string modeDot(bool flashing = false) { return picture(flashing ? "gar_flash" : "gar"); }   // the mode's own colour
std::string ruleDot(int c) { return c == kRuleMode ? modeDot() : dot(c); }

// A live light's condition in the words of the matching ready-made rule ("Parking brake"), else as a test on the dataref.
std::string conditionWords(const LightRule& r) {
    for (int i = 0; i < rulePresetCount(); ++i)
        if (static_cast<const Trigger&>(rulePreset(i).rule) == static_cast<const Trigger&>(r)) {
            std::string name = rulePreset(i).name;
            return name.substr(0, name.find(':'));
        }
    return triggerText(r);
}

const char* const kLightLabels[kLedCount] = {"Fire", "A", "B", "D", "E", "T1", "T2", "T3", "H2 lit hat", "i clutch", "Throttle"};


// What the manual says a button does in one layer: its label, and what a hold does when it has one.
std::string cellText(const Profile& p, int button, int layer) {
    const Binding* b = p.find(button, layer);
    if (!b || b->empty()) return "";
    std::string text = !b->label.empty() ? b->label : actionToText(b->press.empty() ? b->hold : b->press);
    if (!b->hold.empty() && !b->press.empty()) {
        std::string hold = !b->holdLabel.empty() ? b->holdLabel : actionToText(b->hold);
        text += " (hold: " + hold + ")";
    }
    return text;
}
}  // namespace

std::string stripStyle(const std::string& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == '\x01') {
            size_t e = v.find('\x02', i);
            if (e == std::string::npos) break;
            i = e;
        } else if (v[i] == '\x04') {
            size_t e = v.find('\x05', i);
            if (e == std::string::npos) break;
            i = e;
        } else if (v[i] != '\x03') {
            out += v[i];
        }
    }
    return out;
}

std::string ManualData::value(const std::string& tag) const {
    auto it = values.find(tag);
    return it == values.end() ? std::string() : it->second;
}

const char* buttonDevice(int button) {
    if (button < 1 || button > kMaxButton) return "";
    // Stick: trigger, fire, A, B, C, pinkie; the three toggles and the trigger's second stage; hat 2; the main hat.
    if (button <= 6 || (button >= 9 && button <= 15) || (button >= 20 && button <= 23) ||
        (button >= kPov1Up && button < kPov1Up + 4))
        return "stick";
    return "throttle";
}

ManualData buildManualData(const Profile& p, const std::string& appVersion, const std::string& key) {
    ManualData d;
    d.values["manual.schema"] = std::to_string(kManualSchema);
    d.values["app.version"] = appVersion;
    d.values["profile.name"] = p.name;
    d.values["profile.file"] = key.empty() ? "" : key + ".x52";
    d.values["profile.based_on"] = p.basedOn;
    d.values["profile.standard"] = yesNo(p.standard);
    d.values["profile.pinkie_shift"] = yesNo(p.pinkieShift);
    d.values["profile.shift_button"] = buttonName(p.shiftButton);
    for (int m = 1; m <= 3; ++m) d.values["mode." + std::to_string(m) + ".name"] = p.modeNames[m - 1];

    for (int button = 1; button <= kMaxButton; ++button) {
        ManualRow row;
        row["button.number"] = std::to_string(button);
        row["button.name"] = buttonName(button);
        bool usedPlain = false, usedPinkie = false;
        for (int m = 1; m <= 3; ++m) {
            std::string plain = cellText(p, button, m), pinkie = cellText(p, button, m + 3);
            row["button.m" + std::to_string(m)] = plain;
            row["button.p" + std::to_string(m)] = pinkie;
            usedPlain = usedPlain || !plain.empty();
            usedPinkie = usedPinkie || !pinkie.empty();
        }
        row["button.used"] = yesNo(usedPlain || usedPinkie);
        row["button.used_plain"] = yesNo(usedPlain);
        row["button.used_pinkie"] = yesNo(usedPinkie);
        d.lists[std::string(buttonDevice(button)) + "_buttons"].push_back(row);
    }

    // The assignment tables: one cell for each control and mode.
    struct Table { const char* name; const Control* controls; size_t count; bool pinkie; };
    const Table tables[] = {{"stick", kStick, sizeof kStick / sizeof kStick[0], false},
                            {"throttle", kThrottle, sizeof kThrottle / sizeof kThrottle[0], false},
                            {"pinkie_stick", kStick, sizeof kStick / sizeof kStick[0], true},
                            {"pinkie_throttle", kThrottle, sizeof kThrottle / sizeof kThrottle[0], true}};
    for (const Table& tb : tables)
        for (size_t i = 0; i < tb.count; ++i)
            for (int m = 1; m <= 3; ++m)
                d.values[std::string(tb.name) + "." + tb.controls[i].id + ".m" + std::to_string(m)] = controlText(p, tb.controls[i], m, tb.pinkie);

    for (int m = 1; m <= 3; ++m) d.values["shift.m" + std::to_string(m)] = yesNo(p.shiftIn(m));
    d.values["profile.hold_ms"] = std::to_string(p.holdMs) + " ms";
    std::vector<std::string> releases;
    for (const Binding& b : p.bindings)
        if (!b.release.empty()) releases.push_back(std::string(buttonName(b.button)) + ": " + actionToText(b.release));
    d.values["profile.release_actions"] = releases.empty() ? "none" : joinDistinct(releases, "; ");
    std::string screen = triggerText(p.masterScreen), lights = triggerText(p.masterLights);
    d.values["profile.power_conditions"] = "Screen: " + (screen.empty() ? std::string("always on") : screen) + "; lights: " +
                                           (lights.empty() ? std::string("always on") : lights);

    // The screen: the left wheel's pages in each mode, and what the user's own pages show.
    for (int m = 1; m <= 3; ++m) {
        int n = 0;
        for (const WheelEntry& e : wheelEntries(p, m)) {
            if (e.hidden) continue;
            std::string title;
            if (e.page < kPageCount) title = pageName(e.page, p.radioSide);
            else {
                int i = p.pages.find(e.token.substr(1));
                title = i >= 0 ? p.pages.pages[i].title : e.token;
            }
            ++n;
            if (n <= 12) d.values["pages." + std::to_string(n) + ".m" + std::to_string(m)] = title;
        }
        for (++n; n <= 12; ++n) d.values["pages." + std::to_string(n) + ".m" + std::to_string(m)] = "";
    }
    for (int i : p.pages.topLevel()) {
        const UserPage& pg = p.pages.pages[i];
        std::vector<std::string> fields;
        for (const PageRow& r : pg.rows) fields.push_back(r.text);
        ManualRow row;
        row["page.title"] = pg.title;
        row["page.fields"] = joinDistinct(fields, ", ");
        d.lists["user_pages"].push_back(row);
    }
    while (d.lists["user_pages"].size() < 3) d.lists["user_pages"].push_back(ManualRow{{"page.title", ""}, {"page.fields", ""}});

    // The lights: what each mode shows, the live rules and the alarms.
    for (int l = 0; l < kLedCount; ++l)
        for (int m = 1; m <= 3; ++m) {
            Color plain = p.leds[m - 1][l], held = p.ledsPinkie[m - 1][l];
            std::string key = std::string("light.") + ledName(l) + ".m" + std::to_string(m);
            d.values[key] = dot((int)plain) + (plain == held ? "" : " Pinkie " + dot((int)held));
            d.values[key + ".name"] = plain == held ? std::string(colorName(plain)) : std::string(colorName(plain)) + " (Pinkie: " + colorName(held) + ")";
        }
    for (const LightRule& r : p.rules) {
        ManualRow row;
        row["rule.light"] = kLightLabels[r.led >= 0 && r.led < kLedCount ? r.led : 0];
        row["rule.condition"] = conditionWords(r);
        row["rule.active"] = ruleDot(r.thenColor);
        row["rule.otherwise"] = ruleDot(r.elseColor);
        row["rule.active_name"] = ruleColorText(r.thenColor);
        row["rule.otherwise_name"] = ruleColorText(r.elseColor);
        d.lists["rules"].push_back(row);
    }
    while (d.lists["rules"].size() < 3)
        d.lists["rules"].push_back(ManualRow{{"rule.light", ""}, {"rule.condition", ""}, {"rule.active", ""}, {"rule.otherwise", ""}, {"rule.active_name", ""}, {"rule.otherwise_name", ""}});
    for (int n = 1; n <= 8; ++n) {
        std::string pre = "alarm." + std::to_string(n) + ".";
        const Alarm* a = n <= (int)p.alarms.size() ? &p.alarms[n - 1] : nullptr;
        d.values[pre + "name"] = a ? (a->name.empty() ? triggerText(*a) : a->name) : "";
        d.values[pre + "light"] = a ? kLightLabels[a->led >= 0 && a->led < kLedCount ? a->led : 0] : "";
        std::string cell;
        if (a) {
            if (a->color == kRuleMode) cell = modeDot(a->effect == Effect::Blink);                         // "mode light": three colours
            else if (a->effect == Effect::Alternate) cell = pairDot(a->color, a->color2);                 // two colours, primary first
            else cell = dot(a->color, a->effect == Effect::Blink);
        }
        for (int m = 1; m <= 3; ++m) d.values[pre + "m" + std::to_string(m)] = cell;
        d.values[pre + "rate"] = !a ? "" : a->effect == Effect::Steady ? "Steady" : a->fast ? "Fast" : "Slow";
    }
    return d;
}

}  // namespace x52
