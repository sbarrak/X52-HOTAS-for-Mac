// X52 Pro plugin for X-Plane 12 (macOS, arm64).
// Glue between X-Plane (commands, datarefs, flight loop, menu) and the engine + hardware.
#include <algorithm>
#include "../core/version.h"
#include <cmath>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "XPLMDataAccess.h"
#include "XPLMMenus.h"
#include "XPLMPlanes.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include "../core/keys.h"
#include "../mac/keysend.h"
#include "app.h"

namespace x52 {

App* gApp = nullptr;

namespace {

void logLine(const std::string& s) { XPLMDebugString(("X52Pro: " + s + "\n").c_str()); }

std::string lower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// ---- datarefs for the screen pages

struct Refs {
    XPLMDataRef com1, com1s, com2, com2s, nav1, nav1s, nav2, nav2s, adf1, adf1s, adf2, adf2s;
    XPLMDataRef dme1has, dme1nm, dme1kt, dme2has, dme2nm, dme2kt;
    XPLMDataRef xpdrCode, xpdrMode;
    XPLMDataRef apHdg, apAlt, apVs, apIas, apOn, fdMode, obs1, obs2;
    XPLMDataRef zuluH, zuluM, localH, localM, day, month;
} refs;

void findRefs() {
    auto f = [](const char* n) { return XPLMFindDataRef(n); };
    refs.com1 = f("sim/cockpit2/radios/actuators/com1_frequency_hz_833");
    refs.com1s = f("sim/cockpit2/radios/actuators/com1_standby_frequency_hz_833");
    refs.com2 = f("sim/cockpit2/radios/actuators/com2_frequency_hz_833");
    refs.com2s = f("sim/cockpit2/radios/actuators/com2_standby_frequency_hz_833");
    refs.nav1 = f("sim/cockpit2/radios/actuators/nav1_frequency_hz");
    refs.nav1s = f("sim/cockpit2/radios/actuators/nav1_standby_frequency_hz");
    refs.nav2 = f("sim/cockpit2/radios/actuators/nav2_frequency_hz");
    refs.nav2s = f("sim/cockpit2/radios/actuators/nav2_standby_frequency_hz");
    refs.adf1 = f("sim/cockpit2/radios/actuators/adf1_frequency_hz");
    refs.adf1s = f("sim/cockpit2/radios/actuators/adf1_standby_frequency_hz");
    refs.adf2 = f("sim/cockpit2/radios/actuators/adf2_frequency_hz");
    refs.adf2s = f("sim/cockpit2/radios/actuators/adf2_standby_frequency_hz");
    refs.dme1has = f("sim/cockpit2/radios/indicators/nav1_has_dme");
    refs.dme1nm = f("sim/cockpit2/radios/indicators/nav1_dme_distance_nm");
    refs.dme1kt = f("sim/cockpit2/radios/indicators/nav1_dme_speed_kts");
    refs.dme2has = f("sim/cockpit2/radios/indicators/nav2_has_dme");
    refs.dme2nm = f("sim/cockpit2/radios/indicators/nav2_dme_distance_nm");
    refs.dme2kt = f("sim/cockpit2/radios/indicators/nav2_dme_speed_kts");
    refs.xpdrCode = f("sim/cockpit2/radios/actuators/transponder_code");
    refs.xpdrMode = f("sim/cockpit2/radios/actuators/transponder_mode");
    refs.apHdg = f("sim/cockpit2/autopilot/heading_dial_deg_mag_pilot");
    refs.apAlt = f("sim/cockpit2/autopilot/altitude_dial_ft");
    refs.apVs = f("sim/cockpit2/autopilot/vvi_dial_fpm");
    refs.apIas = f("sim/cockpit2/autopilot/airspeed_dial_kts");
    refs.apOn = f("sim/cockpit2/autopilot/servos_on");
    refs.fdMode = f("sim/cockpit2/autopilot/flight_director_mode");
    refs.obs1 = f("sim/cockpit2/radios/actuators/nav1_obs_deg_mag_pilot");
    refs.obs2 = f("sim/cockpit2/radios/actuators/nav2_obs_deg_mag_pilot");
    refs.zuluH = f("sim/cockpit2/clock_timer/zulu_time_hours");
    refs.zuluM = f("sim/cockpit2/clock_timer/zulu_time_minutes");
    refs.localH = f("sim/cockpit2/clock_timer/local_time_hours");
    refs.localM = f("sim/cockpit2/clock_timer/local_time_minutes");
    refs.day = f("sim/cockpit2/clock_timer/current_day");
    refs.month = f("sim/cockpit2/clock_timer/current_month");
}

int geti(XPLMDataRef r) { return r ? XPLMGetDatai(r) : 0; }
float getf(XPLMDataRef r) { return r ? XPLMGetDataf(r) : 0.0f; }

// ---- any dataref by name, for the live light rules

struct RefInfo {
    XPLMDataRef ref = nullptr;
    int index = -1;       // element of an array, -1 for a plain value
    double retryAt = 0;   // not found: look again then
};
std::map<std::string, RefInfo> refCache;

// "name" or "name[3]"; blanks anywhere are ignored (they can never be part of a dataref).
struct RefName {
    std::string name;
    int index = -1;
};
RefName splitRef(const std::string& spec) {
    std::string t;
    for (char c : spec)
        if (!isspace((unsigned char)c)) t += c;
    RefName r;
    r.name = t;
    size_t br = t.find('[');
    if (br != std::string::npos && t.back() == ']') {
        r.name = t.substr(0, br);
        r.index = atoi(t.c_str() + br + 1);
    }
    return r;
}

bool readDataref(const std::string& spec, double now, double& out) {
    RefInfo& info = refCache[spec];
    if (!info.ref) {
        if (now < info.retryAt) return false;
        info.retryAt = now + 5.0;   // add-on aircraft may publish it later
        RefName n = splitRef(spec);
        info.index = n.index;
        info.ref = XPLMFindDataRef(n.name.c_str());
        if (!info.ref) return false;
    }
    int types = XPLMGetDataRefTypes(info.ref);
    int i = info.index;
    bool isArray = (types & (xplmType_FloatArray | xplmType_IntArray)) != 0;
    if (i < 0 && isArray) i = 0;   // a list without [n]: its first item
    if (i >= 0) {
        if (types & xplmType_FloatArray) {
            float v = 0;
            if (XPLMGetDatavf(info.ref, &v, i, 1) != 1) return false;
            out = v;
            return true;
        }
        if (types & xplmType_IntArray) {
            int v = 0;
            if (XPLMGetDatavi(info.ref, &v, i, 1) != 1) return false;
            out = v;
            return true;
        }
        return false;
    }
    if (types & xplmType_Double) out = XPLMGetDatad(info.ref);
    else if (types & xplmType_Float) out = XPLMGetDataf(info.ref);
    else if (types & xplmType_Int) out = XPLMGetDatai(info.ref);
    else return false;
    return true;
}

// Writes one dataref ("name" or "name[3]"); says why when it cannot.
bool writeDataref(const std::string& spec, double value, std::string& why) {
    RefName n = splitRef(spec);
    XPLMDataRef ref = n.name.empty() ? nullptr : XPLMFindDataRef(n.name.c_str());
    if (!ref) { why = "Unknown dataref: " + spec; return false; }
    if (!XPLMCanWriteDataRef(ref)) { why = "X-Plane does not let a plugin change " + spec; return false; }
    int types = XPLMGetDataRefTypes(ref);
    if (types & (xplmType_FloatArray | xplmType_IntArray)) {
        int i = n.index < 0 ? 0 : n.index;
        if (types & xplmType_FloatArray) {
            float v = (float)value;
            XPLMSetDatavf(ref, &v, i, 1);
        } else {
            int v = (int)std::lround(value);
            XPLMSetDatavi(ref, &v, i, 1);
        }
        return true;
    }
    if (types & xplmType_Double) XPLMSetDatad(ref, value);
    else if (types & xplmType_Float) XPLMSetDataf(ref, (float)value);
    else if (types & xplmType_Int) XPLMSetDatai(ref, (int)std::lround(value));
    else { why = spec + " is not a number or a list of numbers"; return false; }
    return true;
}

SimData readSim() {
    SimData d;
    d.com1 = geti(refs.com1);
    d.com1s = geti(refs.com1s);
    d.com2 = geti(refs.com2);
    d.com2s = geti(refs.com2s);
    d.nav1 = geti(refs.nav1);
    d.nav1s = geti(refs.nav1s);
    d.nav2 = geti(refs.nav2);
    d.nav2s = geti(refs.nav2s);
    d.adf1 = geti(refs.adf1);
    d.adf1s = geti(refs.adf1s);
    d.adf2 = geti(refs.adf2);
    d.adf2s = geti(refs.adf2s);
    d.dme1 = geti(refs.dme1has) != 0;
    d.dme1nm = getf(refs.dme1nm);
    d.dme1kt = getf(refs.dme1kt);
    d.dme2 = geti(refs.dme2has) != 0;
    d.dme2nm = getf(refs.dme2nm);
    d.dme2kt = getf(refs.dme2kt);
    d.xpdrCode = geti(refs.xpdrCode);
    d.xpdrMode = geti(refs.xpdrMode);
    d.apHdg = getf(refs.apHdg);
    d.apAlt = getf(refs.apAlt);
    d.apVs = getf(refs.apVs);
    d.apIas = getf(refs.apIas);
    d.apOn = geti(refs.apOn) != 0;
    d.fdMode = geti(refs.fdMode);
    d.obs1 = getf(refs.obs1);
    d.obs2 = getf(refs.obs2);
    return d;
}

// ---- pushing the wanted screen/light state to the stick

void updateDevice(App& a, double now) {
    DeviceState s;
    Lines lines;
    bool labelFresh = now < a.labelUntil;
    a.followLayout();
    if (a.page != PageButtons) {
        // The page was hidden: show the first one the left wheel visits.
        std::vector<int> order = a.pageOrder();
        if (std::find(order.begin(), order.end(), a.page) == order.end()) a.setPage(order[0]);
    }
    if (a.testMode)
        lines = a.testLines;
    else if (a.engine.clutch() && a.pickIndex >= 0 && a.pickIndex < (int)a.pickKeys.size()) {
        lines.l[0] = "PICK PROFILE";
        lines.l[1] = App::profileTitle(a.pickKeys[a.pickIndex]);
        lines.l[2] = "> use   < auto";
    } else if (a.page == PageButtons || a.engine.clutch())
        lines = formatButtonsPage(a.profile.name, labelFresh ? a.label : "", a.engine.mode(), a.engine.shift());
    else if (a.page == PageBrightness)
        lines = formatBrightnessPage(a.profile.mfdBrightness, a.profile.ledBrightness, a.profile.brightScreen.active(),
                                     a.profile.brightLights.active(), a.cursor);
    else if (a.page >= kPageCount)
        lines = a.userPageLines(now);
    else
        lines = formatSimPage(a.page, readSim(), a.cursor, a.profile.radioSide);
    for (int i = 0; i < kMfdLines; ++i) s.lines[i] = a.screen.l[i] = mfdText(lines.l[i]);
    a.lightColors(s.leds, now);
    s.shift = a.engine.shift();
    s.mfdBrightness = a.profile.mfdBrightness;
    s.ledBrightness = a.profile.ledBrightness;
    s.blink = a.engine.clutch();
    // Brightness and master switches follow datarefs (test mode shows the plain settings).
    DatarefReader read = [now](const std::string& ref, double& v) { return readDataref(ref, now, v); };
    if (!a.testMode) {
        s.mfdBrightness = followBrightness(a.profile.brightScreen, s.mfdBrightness, read);
        s.ledBrightness = followBrightness(a.profile.brightLights, s.ledBrightness, read);
    }
    if (!a.testMode && !masterOn(a.profile.masterScreen, read)) {
        for (int i = 0; i < kMfdLines; ++i) s.lines[i] = a.screen.l[i] = "";
        s.shift = false;
        s.mfdBrightness = 0;
    }
    if (!a.testMode && !masterOn(a.profile.masterLights, read)) {
        for (int l = 0; l < kLedCount; ++l) s.leds[l] = Color::Off;
        s.ledBrightness = 0;
        s.blink = false;
    }
    for (int c = 0; c < 3; ++c) s.h12[c] = a.profile.clock12h[c];
    s.dateFormat = a.profile.dateFormat;
    if (refs.zuluH && refs.zuluM) {
        ClockTime gmt;
        gmt.hour = geti(refs.zuluH);
        gmt.minute = geti(refs.zuluM);
        gmt.day = refs.day ? geti(refs.day) : 1;
        gmt.month = refs.month ? geti(refs.month) : 1;
        time_t t = time(nullptr);
        struct tm tmv;
        gmtime_r(&t, &tmv);
        gmt.year2 = tmv.tm_year % 100;
        localtime_r(&t, &tmv);
        bool simKnown = refs.localH && refs.localM;
        int simLocal = simKnown ? localMinusGmt(geti(refs.localH), geti(refs.localM), gmt.hour, gmt.minute) : 0;
        int first = clock1FromGmt(a.profile, (int)(tmv.tm_gmtoff / 60), simKnown, simLocal);
        ClockTime c1 = shiftClock(gmt, first);
        s.clockValid = true;
        s.hour = c1.hour;
        s.minute = c1.minute;
        s.day = c1.day;
        s.month = c1.month;
        s.year2 = c1.year2;
        for (int c = 0; c < 2; ++c) s.clockOffset[c] = clockOffsetFromFirst(a.profile.clockOffset[c], first);
    } else {
        for (int c = 0; c < 2; ++c) s.clockOffset[c] = a.profile.clockOffset[c];
    }
    a.device.setState(s);
}

// After a press on the stick, watches X-Plane's own list of pressed joystick buttons to learn which of its
// slots that button is (read-only: nothing is written to X-Plane).
void learnSlots(App& a, double now) {
    if (!a.learner.pending(now)) return;
    XPLMDataRef ref = XPLMFindDataRef("sim/joystick/joystick_button_values");
    if (!ref) return;
    std::vector<int> v(kXPlaneButtonSlots, 0);
    int n = XPLMGetDatavi(ref, v.data(), 0, kXPlaneButtonSlots);
    std::set<int> active;
    for (int i = 0; i < n && i < kXPlaneButtonSlots; ++i)
        if (v[i]) active.insert(i);
    if (a.learner.update(active, now)) a.saveButtonMap();
}

float flightLoop(float elapsed, float, int, void*) {
    App& a = *gApp;
    double now = a.now();
    if (now >= a.nextOffCheck) {
        a.nextOffCheck = now + 1.0;
        bool off = pluginOff(a.store.dir);
        if (off != a.off) a.setOff(off);
    }
    std::vector<ButtonEvent> events;
    a.input.drain(events);
    if (a.off) {   // switched off in the app: whatever the stick does is X-Plane's alone
        std::vector<AxisEvent> moves;
        a.input.drainAxes(moves);
        return -1.0f;
    }
    for (const ButtonEvent& e : events) {
        if (e.down && e.button >= kPov1Up && e.button < kPov1Up + 4 && a.engine.clutch()) continue;   // with the clutch held POV 1 picks the profile
        if (e.down && e.button >= 1 && e.button <= kMaxButton) {
            a.lastPressed = e.button;
            ++a.pressSerial;
            if (!a.profile.isReserved(e.button)) a.learner.pressed(e.button, now);
        }
        if (a.testMode) {
            if (e.down && e.button >= 1 && e.button <= kMaxButton) {
                a.testLines.l[0] = "Button " + std::to_string(e.button);
                a.testLines.l[1] = buttonName(e.button);
                a.testLines.l[2] = "";
            }
            // Only the dial, pinkie and clutch are followed; nothing is acted on.
            if (!a.profile.isReserved(e.button)) continue;
        }
        a.engine.buttonEvent(e.button, e.down, now);
    }
    learnSlots(a, now);
    a.engine.tick(now);
    if (!a.engine.clutch()) a.pickIndex = -1;

    if (now >= a.nextSlowCheck) {
        a.nextSlowCheck = now + 1.0;
        a.lock.tryHold(a.store.dir);   // the menu bar app leaves the stick alone while we hold this
        long long stamp = a.store.stamp(a.profileKey());
        if (stamp && stamp != a.loadedStamp) a.reload();   // changed outside X-Plane (menu bar app, text editor)
        if (x52::calibrationStamp(a.store.dir) != a.calibrationStamp) a.loadCalibration();   // calibrated in the app
        a.readAxisAssignments();
    }

    bool axisNews = false;
    std::vector<AxisEvent> moves;
    a.input.drainAxes(moves);
    for (const AxisEvent& m : moves) {
        if (m.usagePage != 0x01 || m.usage != 0x39) continue;
        int dir = hatDirection(m.value, m.min, m.max);
        if (dir != a.lastHat && dir >= 0 && !a.testMode && a.engine.clutch()) {
            a.hatMoved(dir);
            axisNews = true;
        }
        a.lastHat = dir;
    }
    a.trackAxes(now);
    a.applyAxes(elapsed);
    a.applySliderFlaps();
    a.applyDials(now);
    if (a.testMode) {
        for (const AxisEvent& m : moves) {
            auto key = std::make_pair(m.usagePage, m.usage);
            auto it = a.axisShown.find(key);
            if (it == a.axisShown.end()) {
                a.axisShown[key] = m.value;   // where the control rests is not news
                continue;
            }
            if (!controlMoved(m.usagePage, m.usage, it->second, m.value, m.min, m.max)) continue;
            it->second = m.value;
            a.testLines.l[0] = controlName(m.usagePage, m.usage);
            a.testLines.l[1] = controlValueText(m.usagePage, m.usage, m.value, m.min, m.max);
            a.testLines.l[2] = "";
            axisNews = true;
        }
    }

    if (now >= a.nextDeviceUpdate || !events.empty() || axisNews) {
        a.nextDeviceUpdate = now + 0.05;   // fast enough for the 2 Hz alarm blink
        updateDevice(a, now);
    }
    return -1.0f;   // every frame
}

// ---- plugin commands and menu

XPLMCommandRef cmdPageNext, cmdPagePrev, cmdWindow, cmdReload, cmdTest, cmdUp, cmdDown, cmdSelect, cmdUndo, cmdRedo;
XPLMMenuID menu = nullptr;
int menuItem = -1;

int onCommand(XPLMCommandRef cmd, XPLMCommandPhase phase, void*) {
    if (phase != xplm_CommandBegin) return 1;
    App& a = *gApp;
    if (cmd == cmdPageNext) a.stepPage(1);
    else if (cmd == cmdPagePrev) a.stepPage(-1);
    else if (cmd == cmdUp) a.wheel(Wheel::Up);
    else if (cmd == cmdDown) a.wheel(Wheel::Down);
    else if (cmd == cmdSelect) a.wheel(Wheel::Select);
    else if (cmd == cmdWindow) uiToggle();
    else if (cmd == cmdReload) a.reload();
    else if (cmd == cmdTest) a.setTestMode(!a.testMode);
    else if (cmd == cmdUndo) a.undo();
    else if (cmd == cmdRedo) a.redo();
    a.nextDeviceUpdate = 0;   // show the new page at once
    return 1;
}

void onMenu(void*, void* item) {
    intptr_t which = (intptr_t)item;
    if (which == 1) uiToggle();
    else if (which == 2) gApp->reload();
    else if (which == 3) gApp->setTestMode(!gApp->testMode);
    else if (which == 4) gApp->undo();
    else if (which == 5) gApp->redo();
}

std::string currentAircraft() {
    char file[512] = "", path[1024] = "";
    XPLMGetNthAircraftModel(0, file, path);
    std::string name = file;
    size_t dot = name.rfind(".acf");
    if (dot != std::string::npos && dot + 4 == name.size()) name.erase(dot);
    return name;
}

}  // namespace

// ---------------------------------------------------------------- App

App::App() : engine(*this) {}

double App::now() const {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

std::string App::profileKey() const {
    if (!manualKey.empty()) return manualKey;
    std::string base = usingAircraftProfile ? Store::keyForAircraft(aircraft) : std::string("default");
    std::string copy = store.choiceFor(base);   // the user's copy of a ready-made profile stands in for it
    return !copy.empty() && store.exists(copy) ? copy : base;
}

std::string datarefReport(const std::string& spec) {
    RefName n = splitRef(spec);
    if (n.name.empty()) return "";
    XPLMDataRef ref = XPLMFindDataRef(n.name.c_str());
    if (!ref) return "Not found in X-Plane. Check the spelling; an add-on aircraft only has its own datarefs while it is loaded.";
    int types = XPLMGetDataRefTypes(ref);
    bool isArray = (types & (xplmType_FloatArray | xplmType_IntArray)) != 0;
    if (n.index >= 0 && !isArray) return "Found, but it is a single value: remove the [" + std::to_string(n.index) + "].";
    double v = 0;
    if (!readDataref(spec, 1e18, v)) {
        if (isArray) return "Found, but it has no item " + std::to_string(n.index < 0 ? 0 : n.index) + ".";
        return "Found, but it is text or data, not a number.";
    }
    char buf[64];
    snprintf(buf, sizeof buf, "%g", v);
    return std::string("Found. Value now: ") + buf + (isArray && n.index < 0 ? "  (a list: showing item 0, add [n] for another)" : "");
}

std::string App::profileTitle(const std::string& key) {
    return key.compare(0, 9, "aircraft-") == 0 ? key.substr(9) : key;
}

void App::setPage(int p) {
    page = p;
    cursor = PageCursor();   // a new page starts with the cursor on its first value, not editing
    nav.reset();
}

int App::pageTotal() const { return kPageCount + (int)profile.pages.topLevel().size(); }

// Radio pages, then the user's pages, Buttons and Brightness last; or the order the profile sets, without its hidden pages.
std::vector<int> App::pageOrder() const { return wheelOrder(profile, engine.mode()); }

void App::showFirstPage() { setPage(pageOrder()[0]); }

// A cursor position means a different value in the other layout (classic item 3 is the NAV1 swap, side item 3 is
// COM2's standby), so a layout change must not carry it over.
void App::followLayout() {
    if (profile.radioSide == cursorSide) return;
    cursorSide = profile.radioSide;
    cursor = PageCursor();
}

void App::stepPage(int direction) {
    std::vector<int> order = pageOrder();
    int n = (int)order.size(), at = 0;
    for (int i = 0; i < n; ++i)
        if (order[i] == page) at = i;
    setPage(order[((at + (direction < 0 ? -1 : 1)) % n + n) % n]);
}

const std::string* App::userPageId(int p) const {
    std::vector<int> top = profile.pages.topLevel();
    int k = p - kPageCount;
    return k >= 0 && k < (int)top.size() ? &profile.pages.pages[top[k]].id : nullptr;
}

std::string App::pageTitle(int p) const {
    if (p < kPageCount) return pageName(p);
    const std::string* id = userPageId(p);
    int i = id ? profile.pages.find(*id) : -1;
    return i >= 0 ? profile.pages.pages[i].title : std::string("(page removed)");
}

Lines App::userPageLines(double now) {
    const std::string* id = userPageId(page);
    if (!id) {
        setPage(PageComNav1);   // the page is gone (the profile changed): back to the first radio page
        return formatSimPage(page, readSim(), cursor, profile.radioSide);
    }
    return formatUserPage(profile.pages, *id, nav, [now](const std::string& ref, double& v) { return readDataref(ref, now, v); });
}

namespace {
XPLMDataRef cachedRef(const char* name) {
    static std::map<std::string, XPLMDataRef> cache;
    auto it = cache.find(name);
    if (it != cache.end() && it->second) return it->second;
    XPLMDataRef r = XPLMFindDataRef(name);
    cache[name] = r;
    return r;
}
const std::pair<int, int> kFlightAxes[] = {{0x01, 0x30}, {0x01, 0x31}, {0x01, 0x35}, {0x01, 0x32}};
}  // namespace

void App::trackAxes(double t) {
    for (const AxisEvent& e : input.latestAxes()) {
        auto key = std::make_pair(e.usagePage, e.usage);
        auto it = axisLive.find(key);
        if (it == axisLive.end()) {   // where a control rests is not news
            axisLive[key] = {e.value, e.min, e.max, e.value, 0};
            continue;
        }
        AxisLive& l = it->second;
        if (e.usagePage == 0x01) calibrator.feed(e.usage, e.value, e.min, e.max);
        const AxisCurve* dial = dialCurve(profile.axes, e.usage);
        l.value = dial && e.usagePage == 0x01 ? steadyValue(l.value, e.value, e.min, e.max, dial->deadzone) : e.value;
        l.min = e.min;
        l.max = e.max;
        if (controlMoved(e.usagePage, e.usage, l.reference, e.value, e.min, e.max)) {
            l.reference = e.value;
            l.changedAt = t;
            axisMoved = key;
            axisMovedAt = t;
        }
    }
}

void App::applyAxes(double dt) {
    bool wantJoy = profile.axes.overrideXPlane, wantThr = profile.axes.pluginThrottle;
    if (!wantJoy && !wantThr) {
        if (axesOverridden) releaseAxes();
        return;
    }
    if (!wantJoy && joyOverridden) {   // only the throttle now: the stick goes back to X-Plane
        if (XPLMDataRef r = cachedRef("sim/operation/override/override_joystick")) XPLMSetDatai(r, 0);
        joyOverridden = false;
    }
    if (!wantJoy) stickSent.clear();
    // The damper stays out of the way on the ground (taxi, take-off roll) and while the autopilot flies.
    auto readInt = [](const char* n) { XPLMDataRef r = cachedRef(n); return r ? XPLMGetDatai(r) : 0; };
    bool damp = wantJoy && profile.axes.damping > 0 && !readInt("sim/flightmodel/failures/onground_any") &&
                !readInt("sim/cockpit2/autopilot/servos_on");
    bool joy = false, thr = false;
    for (const auto& key : kFlightAxes) {
        const AxisTarget* t = axisTarget(key.first, key.second);
        auto it = axisLive.find(key);
        if (!t || (it == axisLive.end() && !t->centred)) continue;   // an unseen throttle stays X-Plane's
        if (t->centred ? !wantJoy : !wantThr) continue;
        double v = 0.0;
        if (it != axisLive.end()) {
            const AxisLive& l = it->second;
            auto cal = calibration.find(t->usage);
            double x = t->centred ? normalizeStick(l.value, l.min, l.max, cal != calibration.end() ? &cal->second : nullptr)
                                  : normalizeAxis(l.value, l.min, l.max, false);
            v = shapeAxis(x, axisCurve(profile.axes, t->usage), t->centred);
        }
        if (t->centred) {
            // Smoothing works on the pilot's input (what is kept here); the damper is added after it, so its
            // nudges never feed back into the smoothing. A first frame takes the stick as it is: no ramp.
            auto sent = stickSent.find(t->usage);
            if (sent != stickSent.end()) v = smoothStick(sent->second, v, dt, profile.axes.smoothingMs);
            stickSent[t->usage] = v;
            const char* rate = t->usage == 0x31 ? "sim/flightmodel/position/Q" : t->usage == 0x30 ? "sim/flightmodel/position/P" : nullptr;
            if (damp && rate)
                if (XPLMDataRef r = cachedRef(rate)) v = dampStick(v, XPLMGetDataf(r), profile.axes.damping);
            stickOut[t->usage] = v;
        }
        if (XPLMDataRef r = cachedRef(t->dataref)) {
            if (t->count > 1) {
                std::vector<float> all(t->count, (float)v);   // every engine gets the same throttle
                XPLMSetDatavf(r, all.data(), 0, t->count);
                // The handle too: the cockpit levers follow, and aircraft whose own systems read the handle
                // (FADEC jets, autothrottles) see the same power.
                if (XPLMDataRef h = cachedRef("sim/cockpit2/engine/actuators/throttle_ratio")) XPLMSetDatavf(h, all.data(), 0, t->count);
            } else {
                XPLMSetDataf(r, (float)v);
            }
        }
        (t->centred ? joy : thr) = true;
    }
    if (joy) if (XPLMDataRef r = cachedRef("sim/operation/override/override_joystick")) XPLMSetDatai(r, 1);
    if (thr) if (XPLMDataRef r = cachedRef("sim/operation/override/override_throttles")) XPLMSetDatai(r, 1);
    joyOverridden = joyOverridden || joy;
    axesOverridden = axesOverridden || joy || thr;
}

// The slider sets the flap handle at the aircraft's detents, written only when the stop changes, so flap
// commands from buttons or the mouse still work in between. Nothing happens until the slider has moved.
void App::applySliderFlaps() {
    if (!profile.axes.sliderFlaps) return;
    auto it = axisLive.find({0x01, 0x36});
    if (it == axisLive.end() || it->second.changedAt <= 0) return;
    const AxisLive& l = it->second;
    double x = normalizeAxis(l.value, l.min, l.max, false);   // high reading: flaps deployed (unless inverted)
    if (profile.axes.slider.invert) x = 1.0 - x;
    XPLMDataRef det = cachedRef("sim/aircraft/controls/acf_flap_detents");
    double handle = flapHandleFor(x, det ? XPLMGetDatai(det) : 0);
    if (std::fabs(handle - flapWritten) < 1e-4) return;
    if (XPLMDataRef r = cachedRef("sim/cockpit2/controls/flap_handle_request_ratio")) XPLMSetDataf(r, (float)handle);
    flapWritten = handle;
}

void App::setOff(bool o) {
    off = o;
    if (off) {
        engine.releaseAll();
        releaseAxes();
        device.stop();
        lock.release();   // the menu bar app may drive the stick's screen and lights
    } else {
        lock.tryHold(store.dir);
        device.start();
        reload();
        nextDeviceUpdate = 0;
    }
}

void App::readAxisAssignments() {
    xpAxisUse.clear();
    XPLMDataRef r = cachedRef("sim/joystick/joystick_axis_assignments");
    if (!r) return;
    xpAxisUse.assign(500, 0);
    int n = XPLMGetDatavi(r, xpAxisUse.data(), 0, (int)xpAxisUse.size());
    xpAxisUse.resize(std::max(0, std::min(n, (int)xpAxisUse.size())));
}

bool App::xplaneDrives(int assignment) const {
    return std::find(xpAxisUse.begin(), xpAxisUse.end(), assignment) != xpAxisUse.end();
}

bool App::xplaneDrives(const LeverJob& job) const {
    for (int a : job.xpAxis)
        if (a && xplaneDrives(a)) return true;
    return false;
}

void App::resetDials() {
    speedBrakeWritten = -9;
    zoomAnchor.clear();
    leverState.clear();
    leverNotes.clear();
    dialsResetPending = true;
}

void App::setDialUse(int usage, DialUse use) {
    if (DialUse* u = dialUse(profile.axes, usage)) *u = use;
    resetDials();
    profileChanged();
}

void App::applyDials(double now) {
    if (dialsResetPending) {
        dialsResetAt = now;
        dialsResetPending = false;
    }
    for (int usage : {0x33, 0x34}) {
        DialUse use = *dialUse(profile.axes, usage);
        auto it = axisLive.find({0x01, usage});
        if (use == DialUse::None || it == axisLive.end()) continue;
        if (const LeverJob* job = leverJob(use)) {
            applyLever(usage, *job, it->second, now);
            continue;
        }
        leverState.erase(usage);
        leverNotes.erase(usage);
        if (xplaneDrives(use == DialUse::SpeedBrake ? kXpAxisSpeedbrakes : kXpAxisViewZoom)) {
            zoomAnchor.erase(usage);   // counted afresh once X-Plane lets go of it
            continue;
        }
        const AxisLive& l = it->second;
        double x = normalizeAxis(l.value, l.min, l.max, false);
        if (dialCurve(profile.axes, usage)->invert) x = 1.0 - x;
        if (use == DialUse::SpeedBrake) {
            if (l.changedAt <= dialsResetAt) continue;   // not turned since the aircraft or profile changed
            double h = speedBrakeFor(x);
            if (std::fabs(h - speedBrakeWritten) < 1e-4) continue;
            if (XPLMDataRef r = cachedRef("sim/cockpit2/controls/speedbrake_ratio")) XPLMSetDataf(r, (float)h);
            speedBrakeWritten = h;
        } else {
            auto a = zoomAnchor.find(usage);
            if (a == zoomAnchor.end()) {
                zoomAnchor[usage] = x;   // where the dial is now is not a turn
                continue;
            }
            for (int c = zoomClicks(a->second, x, profile.axes.zoomSteps); c != 0; c += c > 0 ? -1 : 1)
                commandOnce(c > 0 ? "sim/general/zoom_in" : "sim/general/zoom_out");
        }
    }
}

bool App::leverRange(const LeverJob& job, double& lo, double& hi) {
    lo = job.lo;
    hi = job.hi;
    if (job.limLow && job.limHigh) {
        XPLMDataRef l = cachedRef(job.limLow), h = cachedRef(job.limHigh);
        if (l && h) {
            lo = job.limAbsolute ? (double)XPLMGetDataf(l) : -std::fabs((double)XPLMGetDataf(l));
            hi = job.limAbsolute ? (double)XPLMGetDataf(h) : std::fabs((double)XPLMGetDataf(h));
        } else if (job.limAbsolute) {
            return false;   // the job's own units need the aircraft's limits
        }
    }
    return hi - lo > 1e-6;
}

std::string App::leverNote(int usage) const {
    auto it = leverNotes.find(usage);
    return it == leverNotes.end() ? "" : it->second;
}

namespace {
std::string lowerName(const char* n) {
    std::string s = n;
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
}  // namespace

void App::applyLever(int usage, const LeverJob& job, const AxisLive& l, double now) {
    std::string& note = leverNotes[usage];
    LeverPickup& st = leverState[usage];
    note.clear();
    if (xplaneDrives(job)) {
        st = LeverPickup();
        return;
    }
    XPLMDataRef r = cachedRef(job.dataref);
    if (!r) return;
    double lo, hi;
    if (!leverRange(job, lo, hi)) {
        st = LeverPickup();
        note = "This aircraft has no " + lowerName(job.name) + ".";
        return;
    }
    bool lower = usage == 0x33;
    int from = lower ? profile.axes.lowerFrom : profile.axes.upperFrom;
    int to = lower ? profile.axes.lowerTo : profile.axes.upperTo;
    double x = normalizeAxis(l.value, l.min, l.max, false);
    if (dialCurve(profile.axes, usage)->invert) x = 1.0 - x;
    double target = leverValue(x, lo, hi, from, to);
    float v0 = 0;
    if (job.perEngine) XPLMGetDatavf(r, &v0, 0, 1);
    else v0 = XPLMGetDataf(r);
    double sim = v0;
    double tol = 0.01 * (hi - lo);
    if (from == to) {   // one value for every position: set it when the dial is turned, then leave it until turned again
        if (st.held && std::fabs(sim - st.lastWritten) > tol) {
            st.held = false;
            st.waitingSince = now;
        }
        if (st.held) return;
        if (l.changedAt <= std::max(dialsResetAt, st.waitingSince)) {
            note = "Turn the dial to set " + lowerName(job.name) + " to " + std::to_string(from) + " %.";
            return;
        }
        st.held = true;
        st.lastWritten = target;
    } else {
        bool wasHeld = st.held;
        double before = st.lastWritten;
        if (!leverMayWrite(st, target, sim, tol)) {
            // Where the dial takes over: X-Plane's value as a share of this dial's turn.
            double span = (hi - lo) * (to - from) / 100.0;
            double at = std::fabs(span) > 1e-9 ? (sim - lo - (hi - lo) * from / 100.0) / span : -1;
            if (dialCurve(profile.axes, usage)->invert && at >= -0.005 && at <= 1.005) at = 1.0 - at;   // as the bar shows the dial
            if (at < -0.005 || at > 1.005)
                note = "X-Plane's " + lowerName(job.name) + " is outside this dial's From/To.";
            else
                note = "Turn the dial to about " + std::to_string((int)std::lround(at * 100)) + " % of its turn to take over.";
            return;
        }
        if (wasHeld && std::fabs(target - before) < 1e-6) return;   // the dial did not move
    }
    if (!job.perEngine) {
        XPLMSetDataf(r, (float)target);
        return;
    }
    int n = 1;
    if (XPLMDataRef e = cachedRef("sim/aircraft/engine/acf_num_engines")) n = XPLMGetDatai(e);
    n = std::max(1, std::min(n, 16));
    std::vector<float> all((size_t)n, (float)target);
    XPLMSetDatavf(r, all.data(), 0, n);
}

void App::releaseAxes() {
    for (const char* n : {"sim/operation/override/override_joystick", "sim/operation/override/override_throttles"})
        if (XPLMDataRef r = cachedRef(n)) XPLMSetDatai(r, 0);
    axesOverridden = false;
    joyOverridden = false;
    stickSent.clear();
    stickOut.clear();
}

std::vector<std::string> App::pitchCheck() {
    auto get = [](const char* n) { XPLMDataRef r = cachedRef(n); return r ? XPLMGetDataf(r) : 0.0f; };
    char a[200], b[200];
    double in = stickSent.count(0x31) ? stickSent[0x31] : 0.0, out = stickOut.count(0x31) ? stickOut[0x31] : 0.0;
    snprintf(a, sizeof a, "Pitch sent: %+.3f  (stick %+.3f, damper %+.3f)", out, in, out - in);
    snprintf(b, sizeof b, "X-Plane: yoke %+.3f, elevator trim %+.3f, pitch rate %+.1f deg/s",
             get("sim/cockpit2/controls/yoke_pitch_ratio"), get("sim/cockpit2/controls/elevator_trim"),
             get("sim/flightmodel/position/Q"));
    return {a, b};
}

void App::loadCalibration() {
    calibration = x52::loadCalibration(store.dir);
    calibrationStamp = x52::calibrationStamp(store.dir);
}

void App::saveCalibration() {
    x52::saveCalibration(store.dir, calibration);
    calibrationStamp = x52::calibrationStamp(store.dir);
}

void App::startCalibration() {
    calibrator.start();
    calibrationNote = "Move the stick and the twist to every limit, let go, then press Done.";
}

bool App::finishCalibration() {
    std::map<int, long> rest;
    for (const auto& kv : axisLive)
        if (kv.first.first == 0x01) rest[kv.first.second] = kv.second.value;
    std::string err;
    if (!calibrator.finish(rest, calibration, &err)) {
        calibrationNote = err;
        return false;
    }
    saveCalibration();
    calibrationNote = calibration.count(0x35) ? "Stick and twist calibrated." : "Stick calibrated (the twist keeps its full range).";
    return true;
}

void App::cancelCalibration() {
    calibrator.cancel();
    calibrationNote.clear();
}

void App::resetCalibration() {
    calibrator.cancel();
    calibration.clear();
    saveCalibration();
    calibrationNote = "Calibration cleared: the stick's full range is used.";
}

void App::wheel(Wheel w) { wheelAt(w, now()); }

void App::wheelAt(Wheel w, double t) {
    if (testMode || engine.clutch()) return;
    followLayout();
    // A turn in the same direction soon after the last click is a fast spin: the change is repeated.
    double gap = w == lastWheelDir && w != Wheel::Select ? t - lastWheelTime : 1e9;
    lastWheelTime = t;
    lastWheelDir = w;
    if (page >= kPageCount) {
        if (const std::string* id = userPageId(page)) pageTreeInput(profile.pages, *id, nav, w);
        return;
    }
    if (page == PageBrightness) {
        int screenBefore = profile.mfdBrightness;
        if (brightnessInput(cursor, w, profile.mfdBrightness, profile.ledBrightness, profile.brightScreen.active(),
                            profile.brightLights.active())) {
            profileChanged();   // saved at once; the stick follows on the next refresh
            // The aircraft's instrument panel dims with the stick's screen (only from this page, so the cockpit's
            // own brightness knob is never fought).
            if (profile.mfdBrightness != screenBefore)
                if (XPLMDataRef r = cachedRef("sim/cockpit2/switches/instrument_brightness_ratio")) {
                    float v = (float)(brightnessPercent(profile.mfdBrightness) / 100.0);
                    XPLMSetDatavf(r, &v, 0, 1);
                }
        }
        return;
    }
    for (const std::string& c : pageInput(page, cursor, w, profile.com833, dialRepeat(gap, profile.dial), profile.radioSide)) commandOnce(c);
}

void App::hatMoved(int direction) {
    if (direction == 0 || direction == 2) {
        pickKeys = store.list();
        if (pickKeys.empty()) return;
        int n = (int)pickKeys.size();
        if (pickIndex < 0) {
            pickIndex = 0;
            for (int i = 0; i < n; ++i)
                if (pickKeys[i] == profileKey()) pickIndex = i;
        } else {
            pickIndex = (pickIndex + (direction == 2 ? 1 : n - 1)) % n;
        }
    } else if (direction == 1) {
        if (pickIndex < 0 || pickIndex >= (int)pickKeys.size()) return;
        manualKey = pickKeys[pickIndex];
        store.notePicked(manualKey);
        pickIndex = -1;
        reload();
        showFirstPage();
        showLabel("Profile in use");
    } else {
        manualKey.clear();
        pickIndex = -1;
        loadForAircraft(aircraft);   // back to the automatic choice
        showLabel("Automatic");
    }
}

void App::chooseProfile(const std::string& key) {
    pickIndex = -1;
    if (key.empty()) {
        manualKey.clear();
        loadForAircraft(aircraft);
    } else if (store.exists(key)) {
        manualKey = key;
        store.notePicked(key);
        reload();
        showFirstPage();
    }
}

// What every light shows: an alarm, else the live rule, else the mode's colour (the sim must answer for the first two).
void App::lightColors(Color out[kLedCount], double now) {
    for (int l = 0; l < kLedCount; ++l) {
        Color mode = profile.ledAt(engine.mode(), engine.shift(), l);
        if (testMode) {
            out[l] = mode;
            continue;
        }
        out[l] = shownColor(profile.alarms, profile.rules, l, mode, now,
                            [now](const std::string& ref, double& v) { return readDataref(ref, now, v); });
    }
}

void App::profileChanged() {
    engine.setProfile(profile);
    if (profile.standard) {   // a ready-made profile is never written: the change lasts for this flight
        notice = "Ready-made profile: this change lasts for this flight only. Save as my copy keeps it.";
        return;
    }
    if (saveBlocked) {
        notice = "Not saved: the profile file has a mistake. Fix or delete it, then Plugins > X52 Pro > Reload.";
        return;
    }
    std::string err;
    if (!store.save(profileKey(), profile, &err)) {
        notice = "Could not save: " + err;
        logLine(notice);
    }
    loadedStamp = store.stamp(profileKey());
    trackHistory();
}

void App::trackHistory() {
    std::string key = profileKey(), text = serialize(profile);
    if (key != historyKey) {
        historyKey = key;
        history.reset(text);
    } else if (!profile.standard) {
        history.record(text, now());
    }
}

// Puts a remembered state of the profile back: written to the file like any edit.
static bool applyHistory(App& a, const std::string& text) {
    Profile p;
    std::string err;
    if (!parse(text, p, &err)) return false;
    if (a.saveBlocked) {
        a.notice = "Not saved: the profile file has a mistake. Fix or delete it, then Plugins > X52 Pro > Reload.";
        return false;
    }
    if (!a.store.save(a.profileKey(), p, &err)) {
        a.notice = "Could not undo: " + err;
        return false;
    }
    a.profile = p;
    a.engine.setProfile(a.profile);
    a.loadedStamp = a.store.stamp(a.profileKey());
    a.history.adopt(serialize(a.profile));
    return true;
}

bool App::undo() {
    std::string text;
    return canUndo() && history.undo(text) && applyHistory(*this, text);
}

bool App::redo() {
    std::string text;
    return canRedo() && history.redo(text) && applyHistory(*this, text);
}

// The aircraft's description as X-Plane keeps it ("Cessna Citation X"); the app uses it to find the aircraft's
// joystick settings, since X-Plane stores those per aircraft.
void App::saveAircraftDescription() {
    char buf[512] = {0};
    XPLMDataRef ref = XPLMFindDataRef("sim/aircraft/view/acf_descrip");
    if (!ref) return;
    int n = XPLMGetDatab(ref, buf, 0, (int)sizeof buf - 1);
    buf[n > 0 ? n : 0] = 0;
    std::string d(buf);
    std::ofstream f(store.dir + "/xplane-aircraft.txt");
    f << d << "\n";
}

void App::loadForAircraft(const std::string& acfName) {
    saveAircraftDescription();
    if (acfName != aircraft) manualKey.clear();   // a profile picked on the stick lasts until the aircraft changes
    flapWritten = -1;
    aircraft = acfName;
    std::string key = Store::keyForAircraft(aircraft);
    usingAircraftProfile = !aircraft.empty() && store.exists(key);
    reload();
    showFirstPage();   // a new aircraft or profile starts on the first page; a file reload keeps the page
}

void App::reload() {
    resetDials();
    std::string err;
    Profile p;
    std::string key = profileKey();
    saveBlocked = false;
    loadedStamp = store.stamp(key);
    if (store.exists(key)) {
        if (store.load(key, p, &err)) {
            if (p.version < 2 && !p.standard) {
                // An older file: reading it added the wheel bindings; write them down once.
                p.version = 2;
                store.save(key, p, &err);
                loadedStamp = store.stamp(key);
            }
            profile = p;
            notice.clear();
        } else {
            // Keep the unreadable file as it is and run on defaults until it is fixed.
            notice = "Profile file has a mistake (" + err + ")";
            logLine(notice + " in " + store.pathFor(key));
            saveBlocked = true;
            profile = defaultProfile();
            engine.setProfile(profile);
            return;
        }
    } else {
        profile = defaultProfile();
        if (usingAircraftProfile) profile.name = aircraft;
        store.save(key, profile, &err);
        loadedStamp = store.stamp(key);
    }
    engine.setProfile(profile);
    logLine("using profile '" + profile.name + "' (" + store.pathFor(key) + ")");
    trackHistory();
}

void App::makeAircraftProfile() {
    manualKey.clear();
    if (aircraft.empty() || usingAircraftProfile) return;
    usingAircraftProfile = true;
    profile.name = aircraft;   // starts as a copy of the default profile
    profile.standard = false;
    profile.basedOn.clear();
    profileChanged();
}

void App::dropAircraftProfile() {
    manualKey.clear();
    if (!usingAircraftProfile) return;
    std::string base = Store::keyForAircraft(aircraft), key = profileKey();
    if (key != base) {   // the user's copy of the ready-made profile: set it aside, the ready-made one comes back
        store.trash(key);
        store.setChoice(base, "");
        reload();
        return;
    }
    if (profile.standard) return;   // a ready-made profile is never deleted
    store.trash(key);   // kept aside, so the window can offer "Undo"
    usingAircraftProfile = false;
    reload();
}

std::string App::saveCopy() {
    std::string from = profileKey();
    std::string key = store.freeKey((from == "default" ? std::string("Default") : profileTitle(from)) + " mine");
    Profile p = profile;   // with the changes made in this flight
    p.standard = false;
    if (p.basedOn.empty() && store.isStandard(from)) p.basedOn = from;
    p.name = key;
    std::string err;
    if (!store.save(key, p, &err)) {
        notice = "Could not save: " + err;
        return "";
    }
    if (!manualKey.empty()) manualKey = key;
    store.notePicked(key);
    reload();
    notice = "Saved as your profile '" + key + "'" + (p.basedOn.empty() ? "." : ", used from now on instead of the ready-made one.");
    return key;
}

bool App::canUndoDrop() const { return !aircraft.empty() && !usingAircraftProfile && store.hasTrash(Store::keyForAircraft(aircraft)); }

void App::undoDropAircraftProfile() {
    if (!canUndoDrop() || !store.restore(Store::keyForAircraft(aircraft))) return;
    manualKey.clear();
    usingAircraftProfile = true;
    reload();
}

void App::loadCommandList(const std::string& path) {
    commands.clear();
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        size_t end = line.find_first_of(" \t");
        CommandInfo c;
        c.name = line.substr(0, end);
        if (c.name.empty() || c.name[0] == '#') continue;
        size_t d = end == std::string::npos ? std::string::npos : line.find_first_not_of(" \t", end);
        if (d != std::string::npos) c.description = line.substr(d);
        while (!c.description.empty() && (c.description.back() == '\r' || c.description.back() == ' '))
            c.description.pop_back();
        c.haystack = lower(c.name + " " + c.description);
        commands.push_back(c);
    }
    for (const char* own : {"x52pro/mfd_page_next", "x52pro/mfd_page_prev", "x52pro/mfd_up", "x52pro/mfd_down",
                            "x52pro/mfd_select", "x52pro/toggle_window", "x52pro/toggle_test"}) {
        CommandInfo c;
        c.name = own;
        c.description = "X52 Pro plugin";
        c.haystack = lower(c.name + " " + c.description);
        commands.push_back(c);
    }
}

void* App::findCommand(const std::string& name) {
    auto it = commandCache_.find(name);
    if (it != commandCache_.end() && it->second) return it->second;
    // Not cached when missing: add-on aircraft register their commands late.
    void* ref = XPLMFindCommand(name.c_str());
    if (ref) commandCache_[name] = ref;
    else notice = "Unknown command: " + name;
    return ref;
}

void App::commandBegin(const std::string& name) {
    if (void* c = findCommand(name)) XPLMCommandBegin((XPLMCommandRef)c);
}
void App::commandEnd(const std::string& name) {
    if (void* c = findCommand(name)) XPLMCommandEnd((XPLMCommandRef)c);
}
void App::commandOnce(const std::string& name) {
    if (void* c = findCommand(name)) XPLMCommandOnce((XPLMCommandRef)c);
}

void App::loadButtonMap() {
    std::ifstream f(store.dir + "/xplane-map.txt");
    std::stringstream ss;
    ss << f.rdbuf();
    learner.map() = parseButtonMap(ss.str());
}

void App::saveButtonMap() {
    std::ofstream f(store.dir + "/xplane-map.txt");
    f << serializeButtonMap(learner.map());
}

void App::datarefStep(const Step& s) {
    if (s.text.empty()) return;   // still being filled in
    double cur = 0, out = 0;
    bool have = readDataref(s.text, now(), cur);
    if (!datarefStepValue(s, have, cur, out)) {
        notice = "Cannot read " + s.text + " to add to it";
        return;
    }
    std::string why;
    if (!writeDataref(s.text, out, why)) notice = why;
}

void App::key(const std::string& combo, bool down) {
    KeyCombo k;
    if (!parseKey(combo, k)) {
        notice = "Unknown key: " + combo;
        return;
    }
    if (!keysAllowed()) {
        if (!askedKeyAccess) {
            askedKeyAccess = true;
            requestKeyAccess();
        }
        notice = "Keystrokes need permission: System Settings > Privacy & Security > Accessibility > X-Plane";
        return;
    }
    sendKey(k, down);
}
void App::keyDown(const std::string& combo) { key(combo, true); }
void App::keyUp(const std::string& combo) { key(combo, false); }

void App::setTestMode(bool on) {
    testMode = on;
    engine.releaseAll();   // nothing stays held across the switch
    input.setWantAxes(on);
    axisShown.clear();
    testLines.l[0] = "TEST MODE";
    testLines.l[1] = "move or press";
    testLines.l[2] = "any control";
    nextDeviceUpdate = 0;
}

void App::showLabel(const std::string& text) {
    label = text;
    labelUntil = now() + 3.0;
}

// The mode dial: a mode with an order of its own (Profile::modeWheel) starts on its first page. When both modes
// visit the same pages in the same order (every profile without per-mode orders), the page stays.
void App::layerChanged(int mode, bool) {
    if (mode == pageMode) return;
    bool differs = wheelOrder(profile, mode) != wheelOrder(profile, pageMode);
    pageMode = mode;
    if (differs) showFirstPage();
}

}  // namespace x52

// ---------------------------------------------------------------- X-Plane entry points

using namespace x52;

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
    strcpy(outName, "X52 Pro " X52_VERSION);
    strcpy(outSig, "serbar.x52pro");
    strcpy(outDesc, "Version " X52_VERSION ". Modes, pinkie shift, macros, keystrokes, screen and lights for the X52 Pro HOTAS.");
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

    logLine("version " X52_VERSION);
    gApp = new App();

    char buf[1024] = "";
    XPLMGetPrefsPath(buf);   // .../Output/preferences/Set X-Plane.prf
    std::string prefs = buf;
    size_t slash = prefs.find_last_of('/');
    if (slash != std::string::npos) prefs.erase(slash);
    gApp->store.dir = prefs + "/X52Pro";

    XPLMGetSystemPath(buf);
    gApp->loadCommandList(std::string(buf) + "Resources/plugins/Commands.txt");

    cmdPageNext = XPLMCreateCommand("x52pro/mfd_page_next", "X52 Pro: next screen page");
    cmdPagePrev = XPLMCreateCommand("x52pro/mfd_page_prev", "X52 Pro: previous screen page");
    cmdWindow = XPLMCreateCommand("x52pro/toggle_window", "X52 Pro: show or hide the button programming window");
    cmdReload = XPLMCreateCommand("x52pro/reload_profile", "X52 Pro: reload the profile from disk");
    cmdTest = XPLMCreateCommand("x52pro/toggle_test", "X52 Pro: test mode (the stick's screen names what you move or press)");
    cmdUp = XPLMCreateCommand("x52pro/mfd_up", "X52 Pro: screen cursor up (or value up while editing)");
    cmdDown = XPLMCreateCommand("x52pro/mfd_down", "X52 Pro: screen cursor down (or value down while editing)");
    cmdSelect = XPLMCreateCommand("x52pro/mfd_select", "X52 Pro: screen select (swap active/standby, start or stop editing)");
    cmdUndo = XPLMCreateCommand("x52pro/undo", "X52 Pro: undo the last change to the profile");
    cmdRedo = XPLMCreateCommand("x52pro/redo", "X52 Pro: redo the change that was undone");
    for (XPLMCommandRef c : {cmdPageNext, cmdPagePrev, cmdWindow, cmdReload, cmdTest, cmdUp, cmdDown, cmdSelect, cmdUndo, cmdRedo})
        XPLMRegisterCommandHandler(c, onCommand, 1, nullptr);

    menuItem = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "X52 Pro", nullptr, 0);
    menu = XPLMCreateMenu("X52 Pro", XPLMFindPluginsMenu(), menuItem, onMenu, nullptr);
    XPLMAppendMenuItem(menu, "Program buttons...", (void*)1, 0);
    XPLMAppendMenuItem(menu, "Reload profile from disk", (void*)2, 0);
    XPLMAppendMenuItem(menu, "Test mode on/off (names controls on the stick's screen)", (void*)3, 0);
    XPLMAppendMenuItem(menu, "Undo last profile change", (void*)4, 0);
    XPLMAppendMenuItem(menu, "Redo profile change", (void*)5, 0);

    findRefs();
    return 1;
}

PLUGIN_API int XPluginEnable(void) {
    App& a = *gApp;
    a.off = false;          // the first frame reads the app's switch again
    a.nextOffCheck = 0;
    a.loadForAircraft(currentAircraft());
    a.loadButtonMap();
    a.loadCalibration();
    a.lock.tryHold(a.store.dir);
#ifndef X52_NO_HID   // the tests feed buttons themselves: a real stick plugged in would mix its own in
    a.input.start();
#endif
    a.device.start();
    uiCreate();
    XPLMRegisterFlightLoopCallback(flightLoop, -1.0f, nullptr);
    return 1;
}

PLUGIN_API void XPluginDisable(void) {
    App& a = *gApp;
    XPLMUnregisterFlightLoopCallback(flightLoop, nullptr);
    a.engine.releaseAll();
    a.releaseAxes();
    uiDestroy();
    a.input.stop();
    a.device.stop();
    a.lock.release();   // the menu bar app may drive the stick again
}

PLUGIN_API void XPluginStop(void) {
    for (XPLMCommandRef c : {cmdPageNext, cmdPagePrev, cmdWindow, cmdReload, cmdTest, cmdUp, cmdDown, cmdSelect, cmdUndo, cmdRedo})
        XPLMUnregisterCommandHandler(c, onCommand, 1, nullptr);
    if (menu) XPLMDestroyMenu(menu);
    if (menuItem >= 0) XPLMRemoveMenuItem(XPLMFindPluginsMenu(), menuItem);
    delete gApp;
    gApp = nullptr;
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID, int msg, void* param) {
    if (!gApp) return;
    if (msg == XPLM_MSG_PLANE_LOADED && (intptr_t)param == 0) {
        findRefs();
        gApp->loadForAircraft(currentAircraft());
    }
}
