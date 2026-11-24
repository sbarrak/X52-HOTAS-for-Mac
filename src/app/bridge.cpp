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
#include "../core/pageview.h"
#include "../core/update.h"
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
    if (n == "clock1_offset") return &p.clock1Offset;
    if (n == "clock2_offset") return &p.clockOffset[0];
    if (n == "clock3_offset") return &p.clockOffset[1];
    lo = 0;
    hi = 2;
    if (n == "date_format") return &p.dateFormat;
    if (n == "mode_banner_ms") { lo = 0; hi = 10000; return &p.modeBannerMs; }
    hi = 3;
    if (n == "clock1_source") return &p.clock1Source;   // ClockSource
    lo = 2;
    hi = 50;
    if (n == "dial_factor") return &p.dial.factor;
    // axis_<roll|pitch|yaw|throttle>_<deadzone|expo>
    for (int u : {0x30, 0x31, 0x35, 0x32}) {
        std::string base = std::string("axis_") + axisTarget(0x01, u)->name + "_";
        if (n == base + "deadzone") { lo = 0; hi = 40; return &axisCurve(p.axes, u).deadzone; }
        if (n == base + "expo") { lo = 0; hi = 100; return &axisCurve(p.axes, u).expo; }
    }
    if (n == "stick_smoothing") { lo = 0; hi = 200; return &p.axes.smoothingMs; }
    if (n == "stick_damping") { lo = 0; hi = 100; return &p.axes.damping; }
    if (n == "steady_lower") { lo = 0; hi = 20; return &p.axes.lowerRotary.deadzone; }
    if (n == "steady_upper") { lo = 0; hi = 20; return &p.axes.upperRotary.deadzone; }
    if (n == "steady_slider") { lo = 0; hi = 20; return &p.axes.slider.deadzone; }
    if (n == "rotary_zoom_steps") { lo = 12; hi = 480; return &p.axes.zoomSteps; }
    if (n == "rotary_lower_from") { lo = 0; hi = 100; return &p.axes.lowerFrom; }
    if (n == "rotary_lower_to") { lo = 0; hi = 100; return &p.axes.lowerTo; }
    if (n == "rotary_upper_from") { lo = 0; hi = 100; return &p.axes.upperFrom; }
    if (n == "rotary_upper_to") { lo = 0; hi = 100; return &p.axes.upperTo; }
    return nullptr;
}

bool* flag(Profile& p, const std::string& n) {
    if (n == "clock1_12h") return &p.clock12h[0];
    if (n == "clock2_12h") return &p.clock12h[1];
    if (n == "clock3_12h") return &p.clock12h[2];
    if (n == "clutch_latched") return &p.clutchLatched;
    if (n == "dial_accel") return &p.dial.on;
    if (n == "axis_override") return &p.axes.overrideXPlane;
    if (n == "slider_flaps") return &p.axes.sliderFlaps;
    if (n == "radio_side") return &p.radioSide;
    if (n == "throttle_plugin") return &p.axes.pluginThrottle;
    if (n == "slider_invert") return &p.axes.slider.invert;
    if (n == "rotary_lower_invert") return &p.axes.lowerRotary.invert;
    if (n == "rotary_upper_invert") return &p.axes.upperRotary.invert;
    for (int u : {0x30, 0x31, 0x35, 0x32})
        if (n == std::string("axis_") + axisTarget(0x01, u)->name + "_invert") return &axisCurve(p.axes, u).invert;
    return nullptr;
}

// rotary_lower / rotary_upper: what the dial does (DialUse: 0 nothing, 1 speed brake, 2 zoom, 3..8 lever jobs).
DialUse* dialUseByName(Profile& p, const std::string& n) {
    return n == "rotary_lower" ? &p.axes.lowerUse : n == "rotary_upper" ? &p.axes.upperUse : nullptr;
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
const char* x52_dial_use_name(int use) {
    return dialUseName(use >= 0 && use < kDialUseCount ? (DialUse)use : DialUse::None);
}
int x52_dial_use_count(void) { return kDialUseCount; }

int x52_plugin_enabled(void) { return !(agent && pluginOff(agent->dir())); }
void x52_set_plugin_enabled(int on) {
    if (agent) setPluginOff(agent->dir(), on == 0);
}
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
    loadAircraftCatalog(xplaneDir, commandList, datarefList);   // the add-on aircraft's own (F-14 weapon select, ...)
}
}  // namespace

const char* x52_button_name(int b) { return buttonName(b); }
int x52_button_led(int b) { return buttonLed(b); }
int x52_press_seq(void) { return agent ? agent->pressSeq() : 0; }
int x52_press_button(void) { return agent ? agent->pressButton() : 0; }
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
    for (int b = 1; b <= 39; ++b) {   // POV 1 (40..43) has no X-Plane button slot
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

const char* x52_mode_name(int mode) {
    static std::string s;
    s = agent && mode >= 1 && mode <= 3 ? agent->profile().modeNames[mode - 1] : "";
    return s.c_str();
}

void x52_set_mode_name(int mode, const char* name) {
    if (!agent || mode < 1 || mode > 3) return;
    std::string n = name ? name : "";
    for (char& c : n)
        if (c < 32 || c > 126) c = ' ';
    if (n.size() > 16) n.resize(16);
    agent->change([=](Profile& p) { p.modeNames[mode - 1] = n; });
}

int x52_get(const char* name) {
    if (!agent || !name) return 0;
    Profile p = agent->profile();
    int lo, hi;
    if (DialUse* u = dialUseByName(p, name)) return (int)*u;
    if (int* f = field(p, name, lo, hi)) return *f;
    if (bool* b = flag(p, name)) return *b;
    return 0;
}

void x52_set(const char* name, int value) {
    if (!agent || !name) return;
    std::string n = name;
    agent->change([=](Profile& p) {
        int lo, hi;
        if (DialUse* u = dialUseByName(p, n)) *u = (DialUse)std::max(0, std::min(kDialUseCount - 1, value));
        else if (int* f = field(p, n, lo, hi)) *f = std::max(lo, std::min(hi, value));
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

void x52_calibrate(int action) {
    if (!agent) return;
    switch (action) {
        case 1: agent->startCalibration(); break;
        case 2: agent->finishCalibration(); break;
        case 3: agent->resetCalibration(); break;
        default: agent->cancelCalibration(); break;
    }
}
int x52_calibrating(void) { return agent && agent->calibrating(); }

const char* x52_calibration(void) {
    static std::string out;
    out.clear();
    if (!agent) return "";
    const std::string& note = agent->calibrationNote();
    out = !note.empty() ? note
        : agent->calibration().empty() ? std::string("Not calibrated: the stick's full range is used.")
                                       : std::string("Calibrated.");
    out += "\n" + describeCalibration(agent->calibration());
    return out.c_str();
}

const char* x52_calibration_live(void) {
    static std::string out;
    out.clear();
    if (!agent) return "";
    for (const Agent::CalibrationLive& l : agent->calibrationLive())
        out += std::to_string(l.usage) + "\t" + std::to_string(l.value) + "\t" + std::to_string(l.lo) + "\t" +
               std::to_string(l.hi) + "\t" + std::to_string(l.min) + "\t" + std::to_string(l.max) + "\n";
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
const char* x52_blank_suggest(void) {
    static std::string s;
    if (!agent) return "";
    Store st;
    st.dir = agent->dir();
    s = st.freeKey("New profile");
    return s.c_str();
}
const char* x52_blank_create(const char* name) {
    static std::string s;
    s.clear();
    if (agent && !agent->createBlank(name ? name : "", &s) && s.empty()) s = "Could not make the new profile.";
    return s.c_str();
}
const char* x52_delete_profile(const char* key) {
    static std::string s;
    s.clear();
    if (agent && !agent->deleteProfile(key ? key : "", &s) && s.empty()) s = "Could not delete the profile.";
    return s.c_str();
}
int x52_restore_profile(const char* key) { return agent && key && agent->restoreProfile(key) ? 1 : 0; }

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

// ---- the user's own screen pages

namespace {
const UserPage* pageAt(int i) {
    return agent && i >= 0 && i < (int)agent->profile().pages.pages.size() ? &agent->profile().pages.pages[i] : nullptr;
}
const PageRow* rowAt(int page, int row) {
    const UserPage* p = pageAt(page);
    return p && row >= 0 && row < (int)p->rows.size() ? &p->rows[row] : nullptr;
}
// Text that fits the file's quoted words: no double quotes or line breaks, not too long.
std::string pageText(const char* t, size_t max) {
    std::string s = t ? t : "";
    for (char& c : s) {
        if (c == '"') c = '\'';
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    }
    if (s.size() > max) s.resize(max);
    return s;
}
std::string noBlanks(const char* t) {
    std::string s;
    for (const char* c = t ? t : ""; *c; ++c)
        if (!isspace((unsigned char)*c) && *c != '"') s += *c;
    return s;
}
// A free page id made from a title: "Fuel & weight" -> "fuel-weight", then "fuel-weight-2", ...
std::string idFor(const PageTree& t, const std::string& title) {
    std::string base;
    for (char c : title) {
        if (isalnum((unsigned char)c)) base += (char)tolower((unsigned char)c);
        else if (!base.empty() && base.back() != '-') base += '-';
    }
    while (!base.empty() && base.back() == '-') base.pop_back();
    if (base.size() > 24) base.resize(24);
    while (!base.empty() && base.back() == '-') base.pop_back();
    if (base.empty()) base = "page";
    std::string id = base;
    for (int n = 2; t.find(id) >= 0; ++n) id = base + "-" + std::to_string(n);
    return id;
}
// True when `id` is `ancestor` or sits (at any depth) inside it.
bool within(const PageTree& t, std::string id, const std::string& ancestor) {
    for (int guard = 0; guard <= (int)t.pages.size() && !id.empty(); ++guard) {
        if (id == ancestor) return true;
        int i = t.find(id);
        if (i < 0) return false;
        id = t.pages[i].parent;
    }
    return false;
}
// Edits the page with this id in the file as it is now (its number may differ there).
void editPage(const std::string& id, const std::function<void(PageTree&, UserPage&)>& fn) {
    if (!agent || id.empty()) return;
    agent->change([&](Profile& p) {
        int i = p.pages.find(id);
        if (i >= 0) fn(p.pages, p.pages.pages[i]);
    });
}
std::string idOf(int page) { const UserPage* p = pageAt(page); return p ? p->id : ""; }
}  // namespace

int x52_page_count(void) { return agent ? (int)agent->profile().pages.pages.size() : 0; }
const char* x52_page_id(int i) {
    static std::string s;
    s = idOf(i);
    return s.c_str();
}
const char* x52_page_title(int i) {
    static std::string s;
    const UserPage* p = pageAt(i);
    s = p ? p->title : "";
    return s.c_str();
}
const char* x52_page_parent(int i) {
    static std::string s;
    const UserPage* p = pageAt(i);
    s = p ? p->parent : "";
    return s.c_str();
}

const char* x52_pages_outline(void) {
    static std::string s;
    s.clear();
    if (!agent) return s.c_str();
    const PageTree& t = agent->profile().pages;
    std::vector<bool> shown(t.pages.size(), false);
    std::function<void(int, int)> walk = [&](int i, int depth) {
        if (shown[i] || depth > 8) return;
        shown[i] = true;
        s += std::to_string(i) + "\t" + std::to_string(depth) + "\n";
        for (size_t k = 0; k < t.pages.size(); ++k)
            if (t.pages[k].parent == t.pages[i].id) walk((int)k, depth + 1);
    };
    for (int i : t.topLevel()) walk(i, 0);
    for (size_t i = 0; i < t.pages.size(); ++i)
        if (!shown[i]) walk((int)i, 0);   // its parent is missing (or a loop in a hand-written file)
    return s.c_str();
}

int x52_page_add(const char* title, const char* parent) {
    if (!agent) return -1;
    std::string t = pageText(title, 40), par = noBlanks(parent);
    std::string id;
    agent->change([&](Profile& p) {
        UserPage pg;
        pg.title = t;
        pg.id = idFor(p.pages, t);
        pg.parent = p.pages.find(par) >= 0 ? par : "";
        id = pg.id;
        p.pages.pages.push_back(pg);
    });
    return agent->profile().pages.find(id);
}

void x52_page_remove(int i) {
    std::string id = idOf(i);
    if (id.empty()) return;
    agent->change([&](Profile& p) {
        PageTree& t = p.pages;
        std::vector<std::string> gone;
        for (const UserPage& pg : t.pages)
            if (within(t, pg.id, id)) gone.push_back(pg.id);
        auto isGone = [&](const std::string& x) { return std::find(gone.begin(), gone.end(), x) != gone.end(); };
        t.pages.erase(std::remove_if(t.pages.begin(), t.pages.end(), [&](const UserPage& pg) { return isGone(pg.id); }),
                      t.pages.end());
        for (UserPage& pg : t.pages)
            for (PageRow& r : pg.rows)
                if (r.kind == RowKind::Link && isGone(r.target)) r.target.clear();
        for (std::vector<std::string>* v : {&p.wheelOrder, &p.wheelHidden, &p.modeWheel[0].order, &p.modeWheel[0].hidden,
                                            &p.modeWheel[1].order, &p.modeWheel[1].hidden, &p.modeWheel[2].order, &p.modeWheel[2].hidden})
            v->erase(std::remove_if(v->begin(), v->end(), [&](const std::string& tok) { return tok.size() > 1 && tok[0] == '@' && isGone(tok.substr(1)); }),
                     v->end());
    });
}

void x52_page_move(int i, int dir) {
    std::string id = idOf(i);
    if (id.empty() || (dir != -1 && dir != 1)) return;
    agent->change([&](Profile& p) {
        std::vector<UserPage>& v = p.pages.pages;
        int a = p.pages.find(id);
        if (a < 0) return;
        for (int b = a + dir; b >= 0 && b < (int)v.size(); b += dir)
            if (v[b].parent == v[a].parent) {
                std::swap(v[a], v[b]);
                return;
            }
    });
}

int x52_page_set(int i, const char* title, const char* parent) {
    std::string id = idOf(i);
    if (id.empty()) return 0;
    std::string t = pageText(title, 40), par = noBlanks(parent);
    const PageTree& now = agent->profile().pages;
    if (!par.empty() && (now.find(par) < 0 || within(now, par, id))) return 0;
    editPage(id, [&](PageTree& tree, UserPage& pg) {
        pg.title = t;
        if (par.empty() || (tree.find(par) >= 0 && !within(tree, par, id))) pg.parent = par;
    });
    return 1;
}

int x52_row_count(int i) { const UserPage* p = pageAt(i); return p ? (int)p->rows.size() : 0; }
int x52_row_kind(int i, int r) { const PageRow* x = rowAt(i, r); return x ? (int)x->kind : 0; }
const char* x52_row_text(int i, int r) {
    static std::string s;
    const PageRow* x = rowAt(i, r);
    s = x ? x->text : "";
    return s.c_str();
}
const char* x52_row_format(int i, int r) {
    static std::string s;
    const PageRow* x = rowAt(i, r);
    s = x ? x->format : "";
    return s.c_str();
}
const char* x52_row_dataref(int i, int r) {
    static std::string s;
    const PageRow* x = rowAt(i, r);
    s = x ? x->dataref : "";
    return s.c_str();
}
const char* x52_row_target(int i, int r) {
    static std::string s;
    const PageRow* x = rowAt(i, r);
    s = x ? x->target : "";
    return s.c_str();
}

int x52_row_add(int i, int kind) {
    std::string id = idOf(i);
    if (id.empty() || kind < 0 || kind > 2) return -1;
    editPage(id, [&](PageTree&, UserPage& pg) {
        PageRow r;
        r.kind = (RowKind)kind;
        pg.rows.push_back(r);
    });
    return x52_row_count(agent->profile().pages.find(id)) - 1;
}
void x52_row_remove(int i, int r) {
    editPage(idOf(i), [&](PageTree&, UserPage& pg) {
        if (r >= 0 && r < (int)pg.rows.size()) pg.rows.erase(pg.rows.begin() + r);
    });
}
void x52_row_move(int i, int r, int dir) {
    editPage(idOf(i), [&](PageTree&, UserPage& pg) {
        int j = r + (dir < 0 ? -1 : 1);
        if (r >= 0 && j >= 0 && r < (int)pg.rows.size() && j < (int)pg.rows.size()) std::swap(pg.rows[r], pg.rows[j]);
    });
}
void x52_row_set(int i, int r, int kind, const char* text, const char* format, const char* dataref, const char* target) {
    if (kind < 0 || kind > 2) return;
    std::string t = pageText(text, 40), f = pageText(format, 24), d = noBlanks(dataref), g = noBlanks(target);
    editPage(idOf(i), [&](PageTree& tree, UserPage& pg) {
        if (r < 0 || r >= (int)pg.rows.size()) return;
        PageRow& x = pg.rows[r];
        x.kind = (RowKind)kind;
        x.text = t;
        x.format = f;
        x.dataref = d;
        x.target = tree.find(g) >= 0 ? g : "";
    });
}

void x52_pages_restore_default(void) {
    if (agent)
        agent->change([](Profile& p) {
            p.pages = PageTree();
            p.wheelOrder.clear();
            p.wheelHidden.clear();
            for (WheelSet& w : p.modeWheel) w = WheelSet();
        });
}

namespace {
// The stick's three lines for a page, opened the way the wheels would reach it (through its parents).
Lines previewLines(int i, bool* live) {
    Lines out;
    const UserPage* pg = pageAt(i);
    if (!pg) return out;
    const PageTree& t = agent->profile().pages;
    std::vector<std::string> chain{pg->id};
    for (int guard = 0; guard < 8; ++guard) {
        int k = t.find(chain.back());
        if (k < 0 || t.pages[k].parent.empty() || t.find(t.pages[k].parent) < 0) break;
        chain.push_back(t.pages[k].parent);
    }
    PageNav nav;
    for (size_t k = chain.size() - 1; k-- > 0;) {   // every page below the top one, outermost first
        nav.stack.push_back(chain[k]);
        nav.cursors.push_back(0);
    }
    bool anyLive = false;
    out = formatUserPage(t, chain.back(), nav, [&](const std::string& ref, double& v) {
        if (agent->peek(ref, v)) {
            anyLive = true;
            return true;
        }
        v = 1234.5;
        return true;
    });
    if (live) *live = anyLive;
    return out;
}
}  // namespace

const char* x52_page_preview(int i, int line) {
    static std::string s;
    s = line >= 0 && line < kMfdLines ? previewLines(i, nullptr).l[line] : "";
    return s.c_str();
}
int x52_page_preview_live(int i) {
    bool live = false;
    previewLines(i, &live);
    return live ? 1 : 0;
}
void x52_pages_watch(int on) {
    if (agent) agent->watchPages(on != 0);
}
const char* x52_pages_source(void) {
    static std::string s;
    s = agent ? serializePages(agent->profile().pages) : "";
    return s.c_str();
}
const char* x52_format_example(const char* format, double value) {
    static std::string s;
    s = formatPageValue(format ? format : "", value);
    return s.c_str();
}

// ---- the left wheel's order

namespace {
int gWheelMode = 0;   // the order being edited: 0 every mode, 1..3 that mode's own

void makeModeOwn(Profile& p, int mode) {
    WheelSet w;
    for (const WheelEntry& e : wheelEntries(p)) w.order.push_back(e.token);
    w.hidden = p.wheelHidden;
    w.own = true;
    p.modeWheel[mode - 1] = w;
}

// The lists the wheel calls change: the shared ones, or the edited mode's own (given one first).
std::vector<std::string>& editedOrder(Profile& p) {
    if (gWheelMode < 1) return p.wheelOrder;
    if (!p.modeWheel[gWheelMode - 1].own) makeModeOwn(p, gWheelMode);
    return p.modeWheel[gWheelMode - 1].order;
}
std::vector<std::string>& editedHidden(Profile& p) {
    if (gWheelMode < 1) return p.wheelHidden;
    if (!p.modeWheel[gWheelMode - 1].own) makeModeOwn(p, gWheelMode);
    return p.modeWheel[gWheelMode - 1].hidden;
}
}  // namespace

void x52_wheel_edit_mode(int mode) { gWheelMode = mode >= 1 && mode <= 3 ? mode : 0; }

int x52_wheel_mode_own(int mode) {
    return agent && mode >= 1 && mode <= 3 && agent->profile().modeWheel[mode - 1].own ? 1 : 0;
}

void x52_wheel_set_mode_own(int mode, int own) {
    if (!agent || mode < 1 || mode > 3) return;
    agent->change([&](Profile& p) {
        if (own) {
            if (!p.modeWheel[mode - 1].own) makeModeOwn(p, mode);
        } else {
            p.modeWheel[mode - 1] = WheelSet();
        }
    });
}

const char* x52_wheel_entries(void) {
    static std::string s;
    s.clear();
    if (!agent) return s.c_str();
    const Profile& p = agent->profile();
    for (const WheelEntry& e : wheelEntries(p, gWheelMode)) {
        bool builtin = e.page < kPageCount;
        std::string title = builtin ? pageName(e.page, p.radioSide) : "";
        if (!builtin) {
            int i = p.pages.find(e.token.substr(1));
            title = i >= 0 ? p.pages.pages[i].title : e.token;
        }
        s += e.token + "\t" + title + "\t" + (e.hidden ? "1" : "0") + "\t" + (builtin ? "1" : "0") + "\n";
    }
    return s.c_str();
}

void x52_wheel_move(const char* token, int dir) {
    std::string tok = token ? token : "";
    if (!agent || tok.empty() || (dir != -1 && dir != 1)) return;
    agent->change([&](Profile& p) {
        std::vector<std::string> order;
        for (const WheelEntry& e : wheelEntries(p, gWheelMode)) order.push_back(e.token);
        auto it = std::find(order.begin(), order.end(), tok);
        if (it == order.end()) return;
        int a = (int)(it - order.begin()), b = a + dir;
        if (b < 0 || b >= (int)order.size()) return;
        std::swap(order[a], order[b]);
        editedOrder(p) = order;
    });
}

void x52_wheel_hide(const char* token, int hidden) {
    std::string tok = token ? token : "";
    if (!agent || tok.empty()) return;
    agent->change([&](Profile& p) {
        std::vector<std::string>& h = editedHidden(p);
        h.erase(std::remove(h.begin(), h.end(), tok), h.end());
        if (hidden) h.push_back(tok);
    });
}

const char* x52_builtin_preview(const char* token, int line) {
    static std::string s;
    s.clear();
    std::string tok = token ? token : "";
    int page = -1;
    for (int b = 0; b < kPageCount; ++b)
        if (tok == builtinPageKey(b)) page = b;
    if (!agent || page < 0 || line < 0 || line >= kMfdLines) return s.c_str();
    const Profile& p = agent->profile();
    Lines l;
    if (page == PageButtons) l = formatButtonsPage(p.name, "", 1, false);
    else if (page == PageBrightness)
        l = formatBrightnessPage(p.mfdBrightness, p.ledBrightness, p.brightScreen.active(), p.brightLights.active(), PageCursor());
    else l = formatSimPage(page, SimData(), PageCursor(), p.radioSide);
    s = l.l[line];
    return s.c_str();
}

// ---- ready-made (standard) profiles: changes go to a copy

int x52_can_undo(void) { return agent && agent->canUndo() ? 1 : 0; }
int x52_can_redo(void) { return agent && agent->canRedo() ? 1 : 0; }
int x52_undo(void) { return agent && agent->undo() ? 1 : 0; }
int x52_redo(void) { return agent && agent->redo() ? 1 : 0; }
int x52_standard(void) { return agent && agent->standard() ? 1 : 0; }
int x52_profile_standard(const char* key) {
    if (!agent || !key) return 0;
    Store s;
    s.dir = agent->dir();
    return s.isStandard(key) ? 1 : 0;
}
int x52_copy_needed(void) { return agent && agent->copyNeeded() ? 1 : 0; }
void x52_copy_ask(void) {
    if (agent) agent->askCopy();
}
const char* x52_copy_suggest(void) {
    static std::string s;
    s = agent ? agent->suggestCopyName() : "";
    return s.c_str();
}
const char* x52_copy_save(const char* name) {
    static std::string s;
    s.clear();
    if (agent && !agent->saveCopy(name ? name : "", &s) && s.empty()) s = "Could not save the new profile.";
    return s.c_str();
}
void x52_copy_cancel(void) {
    if (agent) agent->cancelCopy();
}

int x52_version_newer(const char* current, const char* latest) {
    return current && latest && x52::isNewerVersion(current, latest) ? 1 : 0;
}
