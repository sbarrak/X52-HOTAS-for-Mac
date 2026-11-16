// X52 Pro plugin for X-Plane 12 (macOS, arm64).
// Glue between X-Plane (commands, datarefs, flight loop, menu) and the engine + hardware.
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>

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
    XPLMDataRef zuluH, zuluM, day, month;
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
    if (a.testMode)
        lines = a.testLines;
    else if (a.engine.clutch() && a.pickIndex >= 0 && a.pickIndex < (int)a.pickKeys.size()) {
        lines.l[0] = "PICK PROFILE";
        lines.l[1] = App::profileTitle(a.pickKeys[a.pickIndex]);
        lines.l[2] = "> use   < auto";
    } else if (a.page == PageButtons || a.engine.clutch())
        lines = formatButtonsPage(a.profile.name, labelFresh ? a.label : "", a.engine.mode(), a.engine.shift());
    else
        lines = formatSimPage(a.page, readSim(), a.cursor);
    for (int i = 0; i < kMfdLines; ++i) s.lines[i] = a.screen.l[i] = mfdText(lines.l[i]);
    a.lightColors(s.leds, now);
    s.shift = a.engine.shift();
    s.mfdBrightness = a.profile.mfdBrightness;
    s.ledBrightness = a.profile.ledBrightness;
    s.blink = a.engine.clutch();
    for (int c = 0; c < 2; ++c) s.clockOffset[c] = a.profile.clockOffset[c];
    for (int c = 0; c < 3; ++c) s.h12[c] = a.profile.clock12h[c];
    s.dateFormat = a.profile.dateFormat;
    if (refs.zuluH && refs.zuluM) {
        s.clockValid = true;
        s.hour = geti(refs.zuluH);
        s.minute = geti(refs.zuluM);
        s.day = refs.day ? geti(refs.day) : 1;
        s.month = refs.month ? geti(refs.month) : 1;
        time_t t = time(nullptr);
        struct tm tmv;
        gmtime_r(&t, &tmv);
        s.year2 = tmv.tm_year % 100;
    }
    a.device.setState(s);
}

float flightLoop(float, float, int, void*) {
    App& a = *gApp;
    double now = a.now();
    std::vector<ButtonEvent> events;
    a.input.drain(events);
    for (const ButtonEvent& e : events) {
        if (e.down && e.button >= 1 && e.button <= kMaxButton) {
            a.lastPressed = e.button;
            ++a.pressSerial;
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
    a.engine.tick(now);
    if (!a.engine.clutch()) a.pickIndex = -1;

    if (now >= a.nextSlowCheck) {
        a.nextSlowCheck = now + 1.0;
        a.lock.tryHold(a.store.dir);   // the menu bar app leaves the stick alone while we hold this
        long long stamp = a.store.stamp(a.profileKey());
        if (stamp && stamp != a.loadedStamp) a.reload();   // changed outside X-Plane (menu bar app, text editor)
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
        a.nextDeviceUpdate = now + 0.1;
        updateDevice(a, now);
    }
    return -1.0f;   // every frame
}

// ---- plugin commands and menu

XPLMCommandRef cmdPageNext, cmdPagePrev, cmdWindow, cmdReload, cmdTest, cmdUp, cmdDown, cmdSelect;
XPLMMenuID menu = nullptr;
int menuItem = -1;

int onCommand(XPLMCommandRef cmd, XPLMCommandPhase phase, void*) {
    if (phase != xplm_CommandBegin) return 1;
    App& a = *gApp;
    if (cmd == cmdPageNext) a.setPage((a.page + 1) % kPageCount);
    else if (cmd == cmdPagePrev) a.setPage((a.page + kPageCount - 1) % kPageCount);
    else if (cmd == cmdUp) a.wheel(Wheel::Up);
    else if (cmd == cmdDown) a.wheel(Wheel::Down);
    else if (cmd == cmdSelect) a.wheel(Wheel::Select);
    else if (cmd == cmdWindow) uiToggle();
    else if (cmd == cmdReload) a.reload();
    else if (cmd == cmdTest) a.setTestMode(!a.testMode);
    a.nextDeviceUpdate = 0;   // show the new page at once
    return 1;
}

void onMenu(void*, void* item) {
    intptr_t which = (intptr_t)item;
    if (which == 1) uiToggle();
    else if (which == 2) gApp->reload();
    else if (which == 3) gApp->setTestMode(!gApp->testMode);
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
    return usingAircraftProfile ? Store::keyForAircraft(aircraft) : std::string("default");
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
}

void App::wheel(Wheel w) {
    if (testMode || engine.clutch()) return;
    for (const std::string& c : pageInput(page, cursor, w, profile.com833)) commandOnce(c);
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
        pickIndex = -1;
        reload();
        showLabel("Profile in use");
    } else {
        manualKey.clear();
        pickIndex = -1;
        loadForAircraft(aircraft);   // back to the automatic choice
        showLabel("Automatic");
    }
}

// What every light shows: the mode's colour, or its live rule while the sim can answer.
void App::lightColors(Color out[kLedCount], double now) {
    for (int l = 0; l < kLedCount; ++l) {
        Color mode = profile.ledAt(engine.mode(), engine.shift(), l);
        if (testMode) {
            out[l] = mode;
            continue;
        }
        out[l] = liveColor(profile.rules, l, mode,
                           [now](const std::string& ref, double& v) { return readDataref(ref, now, v); });
    }
}

void App::profileChanged() {
    engine.setProfile(profile);
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
}

void App::loadForAircraft(const std::string& acfName) {
    if (acfName != aircraft) manualKey.clear();   // a profile picked on the stick lasts until the aircraft changes
    aircraft = acfName;
    std::string key = Store::keyForAircraft(aircraft);
    usingAircraftProfile = !aircraft.empty() && store.exists(key);
    reload();
}

void App::reload() {
    std::string err;
    Profile p;
    std::string key = profileKey();
    saveBlocked = false;
    loadedStamp = store.stamp(key);
    if (store.exists(key)) {
        if (store.load(key, p, &err)) {
            if (p.version < 2) {
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
}

void App::makeAircraftProfile() {
    manualKey.clear();
    if (aircraft.empty() || usingAircraftProfile) return;
    usingAircraftProfile = true;
    profile.name = aircraft;   // starts as a copy of the default profile
    profileChanged();
}

void App::dropAircraftProfile() {
    manualKey.clear();
    if (!usingAircraftProfile) return;
    store.trash(profileKey());   // kept aside, so the window can offer "Undo"
    usingAircraftProfile = false;
    reload();
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

void App::layerChanged(int, bool) {}

}  // namespace x52

// ---------------------------------------------------------------- X-Plane entry points

using namespace x52;

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
    strcpy(outName, "X52 Pro");
    strcpy(outSig, "serbar.x52pro");
    strcpy(outDesc, "Modes, pinkie shift, macros, keystrokes, screen and lights for the X52 Pro HOTAS.");
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

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
    for (XPLMCommandRef c : {cmdPageNext, cmdPagePrev, cmdWindow, cmdReload, cmdTest, cmdUp, cmdDown, cmdSelect})
        XPLMRegisterCommandHandler(c, onCommand, 1, nullptr);

    menuItem = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "X52 Pro", nullptr, 0);
    menu = XPLMCreateMenu("X52 Pro", XPLMFindPluginsMenu(), menuItem, onMenu, nullptr);
    XPLMAppendMenuItem(menu, "Program buttons...", (void*)1, 0);
    XPLMAppendMenuItem(menu, "Reload profile from disk", (void*)2, 0);
    XPLMAppendMenuItem(menu, "Test mode on/off (names controls on the stick's screen)", (void*)3, 0);

    findRefs();
    return 1;
}

PLUGIN_API int XPluginEnable(void) {
    App& a = *gApp;
    a.loadForAircraft(currentAircraft());
    a.lock.tryHold(a.store.dir);
    a.input.start();
    a.device.start();
    uiCreate();
    XPLMRegisterFlightLoopCallback(flightLoop, -1.0f, nullptr);
    return 1;
}

PLUGIN_API void XPluginDisable(void) {
    App& a = *gApp;
    XPLMUnregisterFlightLoopCallback(flightLoop, nullptr);
    a.engine.releaseAll();
    uiDestroy();
    a.input.stop();
    a.device.stop();
    a.lock.release();   // the menu bar app may drive the stick again
}

PLUGIN_API void XPluginStop(void) {
    for (XPLMCommandRef c : {cmdPageNext, cmdPagePrev, cmdWindow, cmdReload, cmdTest, cmdUp, cmdDown, cmdSelect})
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
