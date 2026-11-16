#include <cctype>
#include "bridge.h"

#include <algorithm>
#include <string>

#include <cstdlib>

#include "../mac/xplane.h"
#include "agent.h"

using namespace x52;

namespace {

Agent* agent = nullptr;

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
    return nullptr;
}

bool* flag(Profile& p, const std::string& n) {
    if (n == "clock1_12h") return &p.clock12h[0];
    if (n == "clock2_12h") return &p.clock12h[1];
    if (n == "clock3_12h") return &p.clock12h[2];
    if (n == "clutch_latched") return &p.clutchLatched;
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
    agent->previewMode(mode, pinkie != 0);
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

namespace {
const LightRule* ruleAt(int i) {
    return agent && i >= 0 && i < (int)agent->profile().rules.size() ? &agent->profile().rules[i] : nullptr;
}
}  // namespace

int x52_rule_count(void) { return agent ? (int)agent->profile().rules.size() : 0; }
int x52_rule_led(int i) { const LightRule* r = ruleAt(i); return r ? r->led : 0; }
const char* x52_rule_dataref(int i) {
    static std::string s;
    const LightRule* r = ruleAt(i);
    s = r ? r->dataref : "";
    return s.c_str();
}
int x52_rule_op(int i) { const LightRule* r = ruleAt(i); return r ? (int)r->op : 0; }
double x52_rule_value(int i) { const LightRule* r = ruleAt(i); return r ? r->value : 0; }
int x52_rule_then(int i) { const LightRule* r = ruleAt(i); return r ? r->thenColor : 0; }
int x52_rule_else(int i) { const LightRule* r = ruleAt(i); return r ? r->elseColor : 0; }

const char* x52_rule_status(int i) {
    static std::string out;
    const LightRule* r = ruleAt(i);
    out = r && agent ? agent->liveStatus(r->dataref) : "";
    return out.c_str();
}

int x52_rule_add(void) {
    if (!agent) return -1;
    agent->change([](Profile& p) { p.rules.push_back(LightRule()); });
    return (int)agent->profile().rules.size() - 1;
}

void x52_rule_remove(int i) {
    if (!agent || !ruleAt(i)) return;
    agent->change([=](Profile& p) {
        if (i < (int)p.rules.size()) p.rules.erase(p.rules.begin() + i);
    });
}

void x52_rule_set(int i, int led, const char* dataref, int op, double value, int t, int e) {
    if (!ruleAt(i) || led < 0 || led >= kLedCount || op < 0 || op >= kCompareCount) return;
    std::string ref;
    for (const char* c = dataref ? dataref : ""; *c; ++c)
        if (!isspace((unsigned char)*c)) ref += *c;   // a dataref has no blanks (a pasted one often has a stray one)
    if (t < kRuleMode || t > 3) t = kRuleMode;
    if (e < kRuleMode || e > 3) e = kRuleMode;
    agent->change([=](Profile& p) {
        if (i >= (int)p.rules.size()) return;
        LightRule& r = p.rules[i];
        r.led = led;
        r.dataref = ref;
        r.op = (Compare)op;
        r.value = value;
        r.thenColor = t;
        r.elseColor = e;
    });
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
