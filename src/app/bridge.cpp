#include <cctype>
#include "bridge.h"

#include <algorithm>
#include <string>

#include <cstdlib>

#include <dirent.h>
#include <sys/stat.h>

#include <fstream>
#include <map>
#include <sstream>

#include "../core/catalog.h"
#include "../core/xpjoy.h"
#include "../mac/xplane.h"
#include "agent.h"

using namespace x52;

namespace {

Agent* agent = nullptr;
std::string xplaneDir;
std::vector<CatalogEntry> commandList, datarefList;
std::string catalogFor;   // the X-Plane folder the lists were read from

int* field(Profile& p, const std::string& n, int& lo, int& hi) {
    lo = 0;
    hi = 127;
    if (n == "led_brightness") return &p.ledBrightness;
    if (n == "mfd_brightness") return &p.mfdBrightness;
    lo = kClockOffsetMin;
    hi = kClockOffsetMax;
    if (n == "clock2_offset") return &p.clockOffset[0];
    if (n == "clock3_offset") return &p.clockOffset[1];
    lo = 0;
    hi = 2;
    if (n == "date_format") return &p.dateFormat;
    lo = 2;
    hi = 50;
    if (n == "dial_factor") return &p.dial.factor;
    // axis_<roll|pitch|yaw|throttle>_<deadzone|expo>
    for (int u : {0x30, 0x31, 0x35, 0x32}) {
        std::string base = std::string("axis_") + axisTarget(0x01, u)->name + "_";
        if (n == base + "deadzone") { lo = 0; hi = 40; return &axisCurve(p.axes, u).deadzone; }
        if (n == base + "expo") { lo = 0; hi = 100; return &axisCurve(p.axes, u).expo; }
    }
    return nullptr;
}

bool* flag(Profile& p, const std::string& n) {
    if (n == "clock1_12h") return &p.clock12h[0];
    if (n == "clock2_12h") return &p.clock12h[1];
    if (n == "clock3_12h") return &p.clock12h[2];
    if (n == "clutch_latched") return &p.clutchLatched;
    if (n == "dial_accel") return &p.dial.on;
    if (n == "axis_override") return &p.axes.overrideXPlane;
    for (int u : {0x30, 0x31, 0x35, 0x32})
        if (n == std::string("axis_") + axisTarget(0x01, u)->name + "_invert") return &axisCurve(p.axes, u).invert;
    return nullptr;
}

bool okLed(int mode, int led) { return mode >= 1 && mode <= 3 && led >= 0 && led < kLedCount; }

}  // namespace

const char* x52_find_xplane(void) {
    static std::string out;
    const char* home = getenv("HOME");
    SearchRoots roots;
    roots.home = home ? home : "";
    out.clear();
    for (const std::string& d : findXPlane(roots)) out += (out.empty() ? "" : "\n") + d;
    return out.c_str();
}

int x52_valid_xplane(const char* dir) { return dir && isXPlaneDir(dir); }

int x52_plugin_state(const char* xplane_dir, const char* bundled_xpl) {
    if (!xplane_dir || !bundled_xpl) return 0;
    return (int)pluginState(xplane_dir, bundled_xpl);
}

const char* x52_install_plugin(const char* xplane_dir, const char* bundled_xpl) {
    static std::string err;
    err.clear();
    if (!xplane_dir || !bundled_xpl) return "No X-Plane folder.";
    installPlugin(xplane_dir, bundled_xpl, &err);
    return err.c_str();
}

void x52_start(const char* xplane_dir, const char* fallback_dir, const char* key) {
    x52_stop();
    xplaneDir = xplane_dir ? xplane_dir : "";
    agent = new Agent();
    agent->open(Agent::chooseDir(xplane_dir ? xplane_dir : "", fallback_dir ? fallback_dir : ""), key ? key : "");
    agent->start();
}

void x52_stop(void) {
    delete agent;
    agent = nullptr;
}

void x52_tick(void) {
    if (agent) agent->tick();
}

int x52_connected(void) { return agent && agent->connected(); }
int x52_is_pro(void) { return agent && agent->isPro(); }
int x52_plugin_active(void) { return agent && agent->pluginActive(); }
int x52_mode(void) { return agent ? agent->mode() : 1; }
int x52_revision(void) { return agent ? agent->revision() : 0; }
const char* x52_dir(void) { return agent ? agent->dir().c_str() : ""; }
const char* x52_error(void) { return agent ? agent->error().c_str() : ""; }

int x52_led_count(void) { return kLedCount; }
const char* x52_led_title(int led) { return ledTitle(led); }
int x52_led_single(int led) { return ledIsSingleColor(led); }

int x52_shift(void) { return agent && agent->shift(); }

int x52_get_led(int mode, int led, int pinkie) {
    return agent && okLed(mode, led) ? (int)agent->profile().ledAt(mode, pinkie != 0, led) : 0;
}

void x52_set_led(int mode, int led, int pinkie, int color) {
    if (!agent || !okLed(mode, led) || color < 0 || color > 3) return;
    if (ledIsSingleColor(led) && color > 1) color = 1;
    if ((int)agent->profile().ledAt(mode, pinkie != 0, led) == color) return;   // already so: leave the file alone
    agent->change([=](Profile& p) { p.ledAt(mode, pinkie != 0, led) = (Color)color; });
}

void x52_copy_mode(int from, int pinkie) {
    if (!agent || from < 1 || from > 3) return;
    agent->change([=](Profile& p) {
        for (int m = 1; m <= 3; ++m)
            for (int l = 0; l < kLedCount; ++l) p.ledAt(m, pinkie != 0, l) = p.ledAt(from, pinkie != 0, l);
    });
}

int x52_preset_count(void) { return rulePresetCount(); }
const char* x52_preset_name(int i) { return rulePreset(i).name; }
int x52_alarm_preset_count(void) { return alarmPresetCount(); }
const char* x52_alarm_preset_name(int i) { return alarmPreset(i).name; }

namespace {
const LightRule* ruleAt(int i) {
    return agent && i >= 0 && i < (int)agent->profile().rules.size() ? &agent->profile().rules[i] : nullptr;
}
const Alarm* alarmAt(int i) {
    return agent && i >= 0 && i < (int)agent->profile().alarms.size() ? &agent->profile().alarms[i] : nullptr;
}
const Trigger* trgAt(int kind, int i) {
    if (kind == 2) return agent && (i == 0 || i == 1) ? (i ? &agent->profile().masterLights : &agent->profile().masterScreen) : nullptr;
    return kind == 0 ? (const Trigger*)ruleAt(i) : (const Trigger*)alarmAt(i);
}
Trigger* trgIn(Profile& p, int kind, int i) {
    if (i < 0) return nullptr;
    if (kind == 2) return i == 0 ? &p.masterScreen : i == 1 ? &p.masterLights : nullptr;
    if (kind == 0) return i < (int)p.rules.size() ? (Trigger*)&p.rules[i] : nullptr;
    return i < (int)p.alarms.size() ? (Trigger*)&p.alarms[i] : nullptr;
}
const Condition& cond(const Trigger& t, int which) { return which ? t.second : (const Condition&)t; }
Condition& condIn(Trigger& t, int which) { return which ? t.second : (Condition&)t; }
}  // namespace

int x52_rule_count(void) { return agent ? (int)agent->profile().rules.size() : 0; }
int x52_rule_led(int i) { const LightRule* r = ruleAt(i); return r ? r->led : 0; }
int x52_rule_then(int i) { const LightRule* r = ruleAt(i); return r ? r->thenColor : 0; }
int x52_rule_else(int i) { const LightRule* r = ruleAt(i); return r ? r->elseColor : 0; }

int x52_rule_add(void) {
    if (!agent) return -1;
    int led = firstFreeLed(agent->profile().rules);
    if (led < 0) return -1;
    agent->change([led](Profile& p) {
        LightRule r;
        r.led = led;
        p.rules.push_back(r);
    });
    return (int)agent->profile().rules.size() - 1;
}

void x52_rule_remove(int i) {
    if (!agent || !ruleAt(i)) return;
    agent->change([=](Profile& p) {
        if (i < (int)p.rules.size()) p.rules.erase(p.rules.begin() + i);
    });
}

int x52_rule_conflict(int i) {
    const LightRule* r = ruleAt(i);
    return r ? ruleUsingLed(agent->profile().rules, r->led, i) : -1;
}

int x52_rule_set(int i, int led, int t, int e) {
    if (!ruleAt(i) || led < 0 || led >= kLedCount) return -1;
    int other = ruleUsingLed(agent->profile().rules, led, i);
    if (other >= 0) return other;   // one rule per light: the caller explains why
    if (t < kRuleMode || t > 3) t = kRuleMode;
    if (e < kRuleMode || e > 3) e = kRuleMode;
    agent->change([=](Profile& p) {
        if (i >= (int)p.rules.size()) return;
        p.rules[i].led = led;
        p.rules[i].thenColor = t;
        p.rules[i].elseColor = e;
    });
    return -1;
}

void x52_rule_preset(int i, int preset) {
    if (!ruleAt(i) || preset < 0 || preset >= rulePresetCount()) return;
    agent->change([=](Profile& p) {
        if (i >= (int)p.rules.size()) return;
        int led = p.rules[i].led;
        p.rules[i] = rulePreset(preset).rule;
        p.rules[i].led = led;
    });
}

int x52_alarm_count(void) { return agent ? (int)agent->profile().alarms.size() : 0; }
const char* x52_alarm_name(int i) {
    static std::string s;
    const Alarm* a = alarmAt(i);
    s = a ? a->name : "";
    return s.c_str();
}
int x52_alarm_led(int i) { const Alarm* a = alarmAt(i); return a ? a->led : 0; }
int x52_alarm_effect(int i) { const Alarm* a = alarmAt(i); return a ? (int)a->effect : 0; }
int x52_alarm_fast(int i) { const Alarm* a = alarmAt(i); return a && a->fast ? 1 : 0; }
int x52_alarm_color(int i) { const Alarm* a = alarmAt(i); return a ? a->color : 0; }
int x52_alarm_color2(int i) { const Alarm* a = alarmAt(i); return a ? a->color2 : 0; }

int x52_alarm_add(void) {
    if (!agent) return -1;
    agent->change([](Profile& p) {
        Alarm a;
        a.name = "Alarm " + std::to_string(p.alarms.size() + 1);
        p.alarms.push_back(a);
    });
    return (int)agent->profile().alarms.size() - 1;
}

int x52_alarm_add_preset(int preset) {
    if (!agent || preset < 0 || preset >= alarmPresetCount()) return -1;
    agent->change([preset](Profile& p) { p.alarms.push_back(alarmPreset(preset).alarm); });
    return (int)agent->profile().alarms.size() - 1;
}

void x52_alarm_remove(int i) {
    if (!alarmAt(i)) return;
    agent->change([=](Profile& p) {
        if (i < (int)p.alarms.size()) p.alarms.erase(p.alarms.begin() + i);
    });
}

void x52_alarm_move(int i, int dir) {
    if (!alarmAt(i) || (dir != -1 && dir != 1)) return;
    int j = i + dir;
    if (j < 0 || j >= x52_alarm_count()) return;
    agent->change([=](Profile& p) {
        if (i < (int)p.alarms.size() && j < (int)p.alarms.size()) std::swap(p.alarms[i], p.alarms[j]);
    });
}

void x52_alarm_set(int i, const char* name, int led, int effect, int fast, int color, int color2) {
    if (!alarmAt(i) || led < 0 || led >= kLedCount || effect < 0 || effect > 2) return;
    std::string n = name ? name : "";
    for (char& c : n)
        if (c == '"') c = '\'';
    if (n.size() > 40) n.resize(40);
    color = color < 0 ? 0 : color > 3 ? 3 : color;
    color2 = color2 < 0 ? 0 : color2 > 3 ? 3 : color2;
    agent->change([=](Profile& p) {
        if (i >= (int)p.alarms.size()) return;
        Alarm& a = p.alarms[i];
        a.name = n;
        a.led = led;
        a.effect = (Effect)effect;
        a.fast = fast != 0;
        a.color = color;
        a.color2 = color2;
    });
}

void x52_alarm_try(int i) {
    if (agent) agent->tryAlarm(i);
}
int x52_alarm_tried(void) { return agent ? agent->triedAlarm() : -1; }

const char* x52_trg_ref(int kind, int i, int which) {
    static std::string s;
    const Trigger* t = trgAt(kind, i);
    s = t ? cond(*t, which ? 1 : 0).dataref : "";
    return s.c_str();
}
int x52_trg_op(int kind, int i, int which) { const Trigger* t = trgAt(kind, i); return t ? (int)cond(*t, which ? 1 : 0).op : 0; }
double x52_trg_value(int kind, int i, int which) { const Trigger* t = trgAt(kind, i); return t ? cond(*t, which ? 1 : 0).value : 0; }
int x52_trg_negate(int kind, int i, int which) { const Trigger* t = trgAt(kind, i); return t && cond(*t, which ? 1 : 0).negate ? 1 : 0; }
int x52_trg_join(int kind, int i) { const Trigger* t = trgAt(kind, i); return t ? (int)t->join : 0; }

void x52_trg_set(int kind, int i, int which, const char* dataref, int op, double value, int negate) {
    if (!trgAt(kind, i) || op < 0 || op >= kCompareCount) return;
    std::string ref;
    for (const char* c = dataref ? dataref : ""; *c; ++c)
        if (!isspace((unsigned char)*c)) ref += *c;   // a dataref has no blanks (a pasted one often has a stray one)
    agent->change([=](Profile& p) {
        Trigger* t = trgIn(p, kind, i);
        if (!t) return;
        Condition& c = condIn(*t, which ? 1 : 0);
        c.dataref = ref;
        c.op = (Compare)op;
        c.value = value;
        c.negate = negate != 0;
    });
}

void x52_trg_set_join(int kind, int i, int join) {
    if (!trgAt(kind, i) || join < 0 || join > 2) return;
    agent->change([=](Profile& p) {
        Trigger* t = trgIn(p, kind, i);
        if (t) t->join = (Join)join;
    });
}

const char* x52_trg_status(int kind, int i, int which) {
    static std::string out;
    const Trigger* t = trgAt(kind, i);
    out = t && agent ? agent->liveStatus(cond(*t, which ? 1 : 0).dataref) : "";
    return out.c_str();
}

namespace {
const BrightnessLink* brightAt(int w) {
    return agent && (w == 0 || w == 1) ? (w ? &agent->profile().brightLights : &agent->profile().brightScreen) : nullptr;
}
}  // namespace
const char* x52_bright_ref(int w) {
    static std::string s;
    const BrightnessLink* b = brightAt(w);
    s = b ? b->dataref : "";
    return s.c_str();
}
double x52_bright_in_low(int w) { const BrightnessLink* b = brightAt(w); return b ? b->inLow : 0; }
double x52_bright_in_high(int w) { const BrightnessLink* b = brightAt(w); return b ? b->inHigh : 1; }
int x52_bright_out_low(int w) { const BrightnessLink* b = brightAt(w); return b ? b->outLow : 0; }
int x52_bright_out_high(int w) { const BrightnessLink* b = brightAt(w); return b ? b->outHigh : 127; }
void x52_bright_set(int w, const char* dataref, double inLow, double inHigh, int outLow, int outHigh) {
    if (!brightAt(w)) return;
    std::string ref;
    for (const char* c = dataref ? dataref : ""; *c; ++c)
        if (!isspace((unsigned char)*c)) ref += *c;
    int lo = std::max(0, std::min(127, outLow)), hi = std::max(0, std::min(127, outHigh));
    agent->change([=](Profile& p) {
        BrightnessLink& b = w ? p.brightLights : p.brightScreen;
        b.dataref = ref;
        b.inLow = inLow;
        b.inHigh = inHigh;
        b.outLow = lo;
        b.outHigh = hi;
    });
}
const char* x52_bright_status(int w) {
    static std::string s;
    const BrightnessLink* b = brightAt(w);
    s = b && agent ? agent->liveStatus(b->dataref) : "";
    return s.c_str();
}
void x52_bright_try(int w, int level) {
    if (agent) agent->tryBright(w, level);
}
int x52_bright_tried(void) { return agent ? agent->triedBright() : -1; }

void x52_master_try(int which) {
    if (agent) agent->tryMaster(which);
}
int x52_master_tried(void) { return agent ? agent->triedMaster() : -1; }

// ---- button assignments

namespace {
bool okBind(int button, int layer) { return button >= 1 && button <= kMaxButton && layer >= 0 && layer < kLayerCount; }
const Binding* bindAt(int button, int layer) {
    return agent && okBind(button, layer) ? agent->profile().exact(button, layer) : nullptr;
}
const Step* stepAt(int button, int layer, int hold, int i) {
    const Binding* b = bindAt(button, layer);
    if (!b) return nullptr;
    const Action& a = hold ? b->hold : b->press;
    return i >= 0 && i < (int)a.size() ? &a[i] : nullptr;
}
// Edits one binding (never a reserved button) and drops it again if it ends up empty.
void editBinding(int button, int layer, const std::function<void(Binding&)>& fn) {
    if (!agent || !okBind(button, layer) || agent->profile().isReserved(button)) return;
    agent->change([=](Profile& p) {
        fn(p.edit(button, layer));
        p.prune();
    });
}
Action& actionIn(Binding& b, int hold) { return hold ? b.hold : b.press; }
std::string stepName(const Step& s) {
    std::string t = s.text;
    size_t slash = t.rfind('/');
    if (slash != std::string::npos) t = t.substr(slash + 1);
    return s.kind == StepKind::Wait ? "wait" : t;
}
void loadCatalogs() {
    if (catalogFor == xplaneDir && !xplaneDir.empty()) return;
    catalogFor = xplaneDir;
    commandList.clear();
    datarefList.clear();
    if (xplaneDir.empty()) return;
    commandList = loadCommandCatalog(xplaneDir + "/Resources/plugins/Commands.txt");
    datarefList = loadDatarefCatalog(xplaneDir + "/Resources/plugins/DataRefs.txt");
}
}  // namespace

const char* x52_button_name(int b) { return buttonName(b); }
const char* x52_layer_name(int l) { return layerName(l); }
int x52_button_reserved(int b) { return agent && agent->profile().isReserved(b) ? 1 : 0; }
int x52_bind_exists(int b, int l) { const Binding* x = bindAt(b, l); return x && !x->empty() ? 1 : 0; }

const char* x52_bind_summary(int b, int l) {
    static std::string s;
    s.clear();
    const Binding* x = bindAt(b, l);
    if (x && !x->empty()) {
        if (!x->label.empty()) s = x->label;
        else if (!x->press.empty()) {
            s = stepName(x->press[0]);
            if (x->press.size() > 1) s += " +" + std::to_string(x->press.size() - 1);
        } else {
            s = x->holdLabel.empty() ? "(hold)" : x->holdLabel;
        }
    }
    return s.c_str();
}

void x52_bind_clear(int b, int l) {
    if (!agent || !okBind(b, l) || !agent->profile().exact(b, l)) return;
    agent->change([=](Profile& p) {
        for (size_t i = 0; i < p.bindings.size(); ++i)
            if (p.bindings[i].button == b && p.bindings[i].layer == l) {
                p.bindings.erase(p.bindings.begin() + i);
                break;
            }
    });
}

const char* x52_bind_label(int b, int l, int hold) {
    static std::string s;
    const Binding* x = bindAt(b, l);
    s = x ? (hold ? x->holdLabel : x->label) : "";
    return s.c_str();
}
void x52_bind_set_label(int b, int l, int hold, const char* label) {
    std::string t = mfdText(label ? label : "");
    editBinding(b, l, [=](Binding& x) { (hold ? x.holdLabel : x.label) = t; });
}

int x52_step_count(int b, int l, int hold) {
    const Binding* x = bindAt(b, l);
    return x ? (int)(hold ? x->hold : x->press).size() : 0;
}
int x52_step_kind(int b, int l, int h, int i) { const Step* s = stepAt(b, l, h, i); return s ? (int)s->kind : 0; }
const char* x52_step_text(int b, int l, int h, int i) {
    static std::string out;
    const Step* s = stepAt(b, l, h, i);
    out = s ? s->text : "";
    return out.c_str();
}
int x52_step_ms(int b, int l, int h, int i) { const Step* s = stepAt(b, l, h, i); return s ? s->ms : 0; }
int x52_step_op(int b, int l, int h, int i) { const Step* s = stepAt(b, l, h, i); return s ? (int)s->op : 0; }
double x52_step_num(int b, int l, int h, int i, int which) {
    const Step* s = stepAt(b, l, h, i);
    if (!s) return 0;
    return which == 0 ? s->value : which == 1 ? s->value2 : which == 2 ? s->lo : s->hi;
}

int x52_step_add(int b, int l, int hold, int kind) {
    if (!agent || !okBind(b, l) || agent->profile().isReserved(b) || kind < 0 || kind > 3) return -1;
    Step st;
    st.kind = (StepKind)kind;
    if (kind == 2) st.ms = 200;
    editBinding(b, l, [=](Binding& x) { actionIn(x, hold).push_back(st); });
    return x52_step_count(b, l, hold) - 1;
}
void x52_step_remove(int b, int l, int hold, int i) {
    editBinding(b, l, [=](Binding& x) {
        Action& a = actionIn(x, hold);
        if (i >= 0 && i < (int)a.size()) a.erase(a.begin() + i);
    });
}
void x52_step_move(int b, int l, int hold, int i, int dir) {
    editBinding(b, l, [=](Binding& x) {
        Action& a = actionIn(x, hold);
        int j = i + (dir < 0 ? -1 : 1);
        if (i >= 0 && j >= 0 && i < (int)a.size() && j < (int)a.size()) std::swap(a[i], a[j]);
    });
}
void x52_step_set(int b, int l, int hold, int i, const char* text, int ms, int op, double v, double v2, double lo, double hi) {
    std::string t;
    for (const char* c = text ? text : ""; *c; ++c) t += *c;
    editBinding(b, l, [=](Binding& x) {
        Action& a = actionIn(x, hold);
        if (i < 0 || i >= (int)a.size()) return;
        Step& s = a[i];
        std::string name = t;
        if (s.kind == StepKind::Command || s.kind == StepKind::Dataref) {
            name.clear();
            for (char c : t) if (!isspace((unsigned char)c)) name += c;   // these names never contain blanks
        }
        s.text = name;
        s.ms = std::max(0, std::min(60000, ms));
        if (op >= 0 && op <= 2) s.op = (DatarefOp)op;
        s.value = v;
        s.value2 = v2;
        s.lo = lo;
        s.hi = hi;
    });
}

namespace {
// Re-reads a file only when it changed (the joystick settings file is a quarter of a megabyte).
struct FileCache {
    std::string path;
    long long stamp = -1;
    std::string text;
    const std::string& get(const std::string& p) {
        struct stat st;
        long long s = stat(p.c_str(), &st) == 0 ? (long long)st.st_mtime * 1000003LL + (long long)st.st_size : -2;
        if (p != path || s != stamp) {
            path = p;
            stamp = s;
            text.clear();
            std::ifstream f(p);
            std::stringstream ss;
            ss << f.rdbuf();
            text = ss.str();
        }
        return text;
    }
};
}  // namespace

// The file with the joystick assignments that apply now: the aircraft's control profile (found through the aircraft
// description the plugin saved and Miscellaneous.prf), else the main settings file.
static std::string assignmentFile() {
    static FileCache aircraftFile, miscFile;
    if (xplaneDir.empty() || !agent) return "";
    std::string prefs = xplaneDir + "/Output/preferences";
    std::string desc = aircraftFile.get(agent->dir() + "/xplane-aircraft.txt");
    std::string name = controlProfileFor(miscFile.get(prefs + "/Miscellaneous.prf"), desc);
    if (!name.empty()) {
        std::string path = prefs + "/control profiles/" + name + " - Joysticks.prf";
        struct stat st;
        if (stat(path.c_str(), &st) == 0) return path;
    }
    return prefs + "/X-Plane Joystick Settings.prf";
}

const char* x52_assignment_source(void) {
    static std::string s;
    std::string p = assignmentFile();
    size_t slash = p.rfind('/');
    s = slash == std::string::npos ? "" : p.substr(slash + 1);
    return s.c_str();
}

namespace {
const char* const kCurrentSettings = "(settings in use)";
std::string importMessage;

std::string controlProfilesDir() { return xplaneDir + "/Output/preferences/control profiles"; }

// The joystick file behind a profile name in the list above.
std::string xplaneProfileFile(const std::string& name) {
    if (name == kCurrentSettings || name.empty()) return xplaneDir + "/Output/preferences/X-Plane Joystick Settings.prf";
    return controlProfilesDir() + "/" + name + " - Joysticks.prf";
}

std::string safeProfileName(const std::string& raw) {
    std::string out;
    for (unsigned char c : raw) out += (isalnum(c) || c == '-' || c == '_' || c == '.' || c == ' ') ? (char)c : '_';
    size_t a = out.find_first_not_of(' '), b = out.find_last_not_of(' ');
    return a == std::string::npos ? "" : out.substr(a, b - a + 1);
}

std::string freeProfileName(const std::string& wanted) {
    std::string base = safeProfileName(wanted);
    if (base.empty()) base = "X-Plane import";
    std::string name = base;
    std::vector<std::string> taken = agent ? agent->profiles() : std::vector<std::string>();
    for (int n = 2; name == "default" || std::find(taken.begin(), taken.end(), name) != taken.end(); ++n)
        name = base + " " + std::to_string(n);
    return name;
}
}  // namespace

const char* x52_xplane_profiles(void) {
    static std::string out;
    out.clear();
    std::vector<std::string> names;
    if (DIR* d = opendir(controlProfilesDir().c_str())) {
        const std::string tail = " - Joysticks.prf";
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() > tail.size() && n.compare(n.size() - tail.size(), tail.size(), tail) == 0)
                names.push_back(n.substr(0, n.size() - tail.size()));
        }
        closedir(d);
    }
    std::sort(names.begin(), names.end());
    std::string used = x52_assignment_source();   // the aircraft's own profile goes first
    const std::string tail = " - Joysticks.prf";
    if (used.size() > tail.size() && used.compare(used.size() - tail.size(), tail.size(), tail) == 0) {
        std::string cur = used.substr(0, used.size() - tail.size());
        auto it = std::find(names.begin(), names.end(), cur);
        if (it != names.end()) {
            names.erase(it);
            names.insert(names.begin(), cur);
        }
    }
    for (const std::string& n : names) out += n + "\n";
    out += kCurrentSettings;
    out += "\n";
    return out.c_str();
}

const char* x52_import_suggest(const char* xp) {
    static std::string s;
    std::string name = xp ? xp : "";
    s = freeProfileName(name == kCurrentSettings ? "X-Plane settings" : name);
    return s.c_str();
}

const char* x52_import_message(void) { return importMessage.c_str(); }

int x52_import_profile(const char* xp, const char* newName) {
    importMessage.clear();
    if (!agent) return -1;
    std::string key = safeProfileName(newName ? newName : "");
    if (key.empty()) {
        importMessage = "Type a name for the new profile.";
        return -1;
    }
    std::vector<std::string> taken = agent->profiles();
    if (key == "default" || std::find(taken.begin(), taken.end(), key) != taken.end()) {
        importMessage = "A profile called '" + key + "' already exists. Existing profiles are never overwritten: choose another name.";
        return -1;
    }
    std::ifstream f(xplaneProfileFile(xp ? xp : ""));
    if (xplaneDir.empty() || !f) {
        importMessage = "X-Plane's joystick settings were not found.";
        return -1;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::map<int, std::string> assigned = parseButtonAssignments(ss.str());
    std::ifstream mf(agent->dir() + "/xplane-map.txt");
    std::stringstream ms;
    ms << mf.rdbuf();
    std::map<int, int> slots = completeButtonMap(parseButtonMap(ms.str()));
    Profile p = defaultProfile();
    p.name = key;
    int added = 0;
    for (int b = 1; b <= kMaxButton; ++b) {
        if (p.isReserved(b) || p.exact(b, kLayerAny)) continue;
        auto cmd = assigned.find(slots[b]);
        if (cmd == assigned.end() || cmd->second.empty() || cmd->second == "sim/none/none") continue;
        p.edit(b, kLayerAny).press.push_back({StepKind::Command, cmd->second, 0});
        ++added;
    }
    std::string err;
    if (!agent->createProfile(key, p, &err)) {
        importMessage = err;
        return -1;
    }
    importMessage = "Created profile '" + key + "' with " + std::to_string(added) + " button" + (added == 1 ? "" : "s") +
                    " from X-Plane. Now set each of them to Do nothing in X-Plane.";
    return added;
}

const char* x52_bound_buttons(void) {
    static std::string out;
    static FileCache prfFile, mapFile;
    static std::string prfText, mapText;
    static std::map<int, std::string> assignments;
    static std::map<int, int> slots;
    out.clear();
    if (!agent) return out.c_str();
    const std::string& prf = prfFile.get(assignmentFile());
    if (prf != prfText) {
        prfText = prf;
        assignments = parseButtonAssignments(prfText);
    }
    const std::string& mp = mapFile.get(agent->dir() + "/xplane-map.txt");
    if (mp != mapText) {
        mapText = mp;
        slots = completeButtonMap(parseButtonMap(mapText));
    }
    for (int b = 1; b <= kMaxButton; ++b) {
        std::string layers;
        for (int l = 0; l < kLayerCount; ++l) {
            const Binding* x = agent->profile().exact(b, l);
            if (x && !x->empty()) layers += (layers.empty() ? "" : ", ") + std::string(layerName(l));
        }
        if (layers.empty()) continue;
        auto slot = slots.find(b);
        std::string assigned;
        if (slot != slots.end()) {
            auto cmd = assignments.find(slot->second);
            if (cmd != assignments.end()) assigned = cmd->second;
        }
        out += std::to_string(b) + "\t" + buttonName(b) + "\t" + layers + "\t" +
               std::to_string(slot == slots.end() ? -1 : xplaneButtonNumber(slot->second)) + "\t" + assigned + "\n";
    }
    return out.c_str();
}

int x52_catalog_size(int kind) {
    loadCatalogs();
    return (int)(kind == 0 ? commandList.size() : datarefList.size());
}
const char* x52_catalog_search(int kind, const char* query, int max) {
    static std::string out;
    out.clear();
    loadCatalogs();
    for (const CatalogEntry* e : searchCatalog(kind == 0 ? commandList : datarefList, query ? query : "", max))
        out += e->name + "\t" + e->info + "\t" + (e->writable ? "1" : "0") + "\n";
    return out.c_str();
}

int x52_get(const char* name) {
    if (!agent || !name) return 0;
    Profile p = agent->profile();
    int lo, hi;
    if (int* f = field(p, name, lo, hi)) return *f;
    if (bool* b = flag(p, name)) return *b;
    return 0;
}

void x52_set(const char* name, int value) {
    if (!agent || !name) return;
    std::string n = name;
    agent->change([=](Profile& p) {
        int lo, hi;
        if (int* f = field(p, n, lo, hi)) *f = std::max(lo, std::min(hi, value));
        else if (bool* b = flag(p, n)) *b = value != 0;
    });
}

const char* x52_axes(void) {
    static std::string out;
    out.clear();
    if (!agent) return "";
    for (const Agent::AxisView& a : agent->axes()) {
        bool hat = a.usagePage == 0x01 && a.usage == 0x39;
        long range = a.max - a.min;
        int pc = hat || range <= 0 ? 0 : (int)std::max(0L, std::min(100L, (a.value - a.min) * 100 / range));
        out += std::string(controlName(a.usagePage, a.usage)) + "\t" + std::to_string(pc) + "\t" +
               controlValueText(a.usagePage, a.usage, a.value, a.min, a.max) + "\t" + std::to_string(a.value) + "\t" +
               (a.moving ? "1" : "0") + "\t" + std::to_string(a.usage) + "\n";
    }
    return out.c_str();
}

const char* x52_profiles(void) {
    static std::string s;
    s.clear();
    if (agent)
        for (const std::string& k : agent->profiles()) s += k + "\n";
    return s.c_str();
}
const char* x52_key(void) { return agent ? agent->key().c_str() : "default"; }
void x52_select(const char* key) {
    if (agent && key) agent->select(key);
}

const char* x52_screen(int line) {
    return agent && line >= 0 && line < kMfdLines ? agent->screen().l[line].c_str() : "";
}
const char* x52_held(void) {
    static std::string s;
    s = agent ? agent->held() : "";
    return s.c_str();
}
const char* x52_moved(void) { return agent ? agent->moved().c_str() : ""; }
void x52_test(int on) {
    if (agent) agent->setTest(on != 0);
}
void x52_text(const char* l0, const char* l1, const char* l2) {
    if (agent) agent->setText(l0 ? l0 : "", l1 ? l1 : "", l2 ? l2 : "");
}
void x52_clear_text(void) {
    if (agent) agent->clearText();
}
