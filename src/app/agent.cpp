#include <cstdio>
#include "agent.h"

#include <algorithm>

#include <sys/stat.h>

#include <chrono>
#include <ctime>

namespace x52 {

Agent::Agent() : engine_(*this) {}
Agent::~Agent() { stop(); }

double Agent::now() const {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

std::string Agent::chooseDir(const std::string& xplaneDir, const std::string& fallbackDir) {
    struct stat st;
    std::string prefs = xplaneDir + "/Output/preferences";
    if (!xplaneDir.empty() && stat(prefs.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return prefs + "/X52Pro";
    return fallbackDir;
}

void Agent::open(const std::string& dir, const std::string& key) {
    store_.dir = dir;
    key_ = key.empty() ? "default" : key;
    if (key_ != "default" && !store_.exists(key_)) key_ = "default";
    reload();
    calibrator_.cancel();
    calibrationNote_.clear();
    loadCalibration();
}

void Agent::loadCalibration() {
    calibration_ = x52::loadCalibration(store_.dir);
    calibrationStamp_ = calibrationStamp(store_.dir);
}

void Agent::startCalibration() {
    calibrator_.start();
    calibrationNote_ = "Move the stick and the twist to every limit, let go, then press Done.";
}

bool Agent::finishCalibration() {
    std::map<int, long> rest;
    for (const AxisEvent& e : input_.latestAxes())
        if (e.usagePage == 0x01) rest[e.usage] = e.value;
    std::string err;
    std::map<int, AxisCalibration> result;
    if (!calibrator_.finish(rest, result, &err)) {
        calibrationNote_ = err;
        return false;
    }
    calibration_ = result;
    if (!saveCalibration(store_.dir, calibration_)) {
        calibrationNote_ = "Could not save the calibration in " + store_.dir + ".";
        return false;
    }
    calibrationStamp_ = calibrationStamp(store_.dir);
    calibrationNote_ = calibration_.count(0x35) ? "Stick and twist calibrated." : "Stick calibrated (the twist keeps its full range).";
    return true;
}

std::vector<Agent::CalibrationLive> Agent::calibrationLive() const {
    std::vector<CalibrationLive> out;
    for (int usage : {0x30, 0x31, 0x35})
        for (const AxisEvent& e : input_.latestAxes()) {
            if (e.usagePage != 0x01 || e.usage != usage) continue;
            CalibrationLive l{usage, e.value, e.value, e.value, e.min, e.max};
            long lo, hi, mn, mx;
            if (calibrator_.reached(usage, lo, hi, mn, mx)) {
                l.lo = std::min(lo, e.value);
                l.hi = std::max(hi, e.value);
            }
            out.push_back(l);
        }
    return out;
}

void Agent::cancelCalibration() {
    calibrator_.cancel();
    calibrationNote_.clear();
}

void Agent::resetCalibration() {
    calibrator_.cancel();
    calibration_.clear();
    saveCalibration(store_.dir, calibration_);
    calibrationStamp_ = calibrationStamp(store_.dir);
    calibrationNote_ = "Calibration cleared: the stick's full range is used.";
}

void Agent::reload() {
    stamp_ = store_.stamp(key_);
    Profile p;
    std::string err;
    if (!store_.exists(key_)) {
        p = defaultProfile();
        error_.clear();
    } else if (store_.load(key_, p, &err)) {
        error_.clear();
    } else {
        // Run on what we had (or defaults) and say so; change() will refuse to save.
        error_ = "The profile file has a mistake (" + err + "). Nothing is saved until it is fixed.";
        ++revision_;
        return;
    }
    profile_ = p;
    engine_.setProfile(profile_);
    ++revision_;
    observeHistory();
}

void Agent::observeHistory() {
    std::string text = serialize(profile_);
    if (key_ != historyKey_) {
        historyKey_ = key_;
        history_.reset(text);
    } else if (!profile_.standard) {
        history_.record(text, now());
    }
}

// Puts a remembered state back: written to the file like any edit, then read as the profile in use.
bool Agent::applyHistory(const std::string& text) {
    Profile p;
    std::string err;
    if (!parse(text, p, &err) || !store_.save(key_, p, &err)) {
        error_ = "Could not undo: " + err;
        ++revision_;
        return false;
    }
    error_.clear();
    stamp_ = store_.stamp(key_);
    profile_ = p;
    engine_.setProfile(profile_);
    history_.adopt(serialize(profile_));
    ++revision_;
    nextSlow_ = 0;
    return true;
}

bool Agent::undo() {
    std::string text;
    if (!canUndo() || !history_.undo(text)) return false;
    return applyHistory(text);
}

bool Agent::redo() {
    std::string text;
    if (!canRedo() || !history_.redo(text)) return false;
    return applyHistory(text);
}

bool Agent::change(const std::function<void(Profile&)>& edit) {
    if (profile_.standard || store_.isStandard(key_)) {
        pending_.push_back(edit);   // a ready-made profile is never written: this goes into a copy, once it has a name
        copyAsked_ = true;
        ++revision_;
        return false;
    }
    Profile p;
    std::string err;
    if (store_.exists(key_)) {
        if (!store_.load(key_, p, &err)) {
            error_ = "Not saved: the profile file has a mistake (" + err + ").";
            ++revision_;
            return false;
        }
    } else {
        p = defaultProfile();
    }
    edit(p);
    if (!store_.save(key_, p, &err)) {
        error_ = "Could not save: " + err;
        ++revision_;
        return false;
    }
    error_.clear();
    stamp_ = store_.stamp(key_);
    profile_ = p;
    engine_.setProfile(profile_);
    ++revision_;
    nextSlow_ = 0;
    observeHistory();
    return true;
}

bool Agent::createProfile(const std::string& key, const Profile& p, std::string* error) {
    if (key.empty() || key == "default" || store_.exists(key)) {
        if (error) *error = "A profile called '" + key + "' already exists.";
        return false;
    }
    if (!store_.save(key, p, error)) return false;
    select(key);
    return true;
}

bool Agent::createBlank(const std::string& name, std::string* error) {
    size_t a = name.find_first_not_of(' '), b = name.find_last_not_of(' ');
    std::string wanted = a == std::string::npos ? "" : name.substr(a, b - a + 1);
    if (wanted.empty()) {
        if (error) *error = "Type a name for the new profile.";
        return false;
    }
    std::string safe = store_.freeKey(wanted);   // file-name safe and not taken
    if (safe != wanted) {
        if (error) *error = "'" + wanted + "' is taken, or has characters a file name cannot have. Try '" + safe + "'.";
        return false;
    }
    Profile p = defaultProfile();   // only the screen controls on the wheel and the page buttons
    p.name = wanted;
    p.axes = profile_.axes;
    return createProfile(wanted, p, error);
}

bool Agent::deleteProfile(const std::string& key, std::string* error) {
    if (key.empty() || key == "default") {
        if (error) *error = "Default is never deleted.";
        return false;
    }
    if (!store_.exists(key)) {
        if (error) *error = "There is no profile called '" + key + "'.";
        return false;
    }
    if (store_.isStandard(key)) {
        if (error) *error = "'" + key + "' is a ready-made profile: it is never deleted, so you can always go back to it.";
        return false;
    }
    Profile p;
    std::string basedOn = store_.load(key, p, nullptr) ? p.basedOn : "";
    if (!basedOn.empty() && store_.choiceFor(basedOn) == key) store_.setChoice(basedOn, "");
    if (!store_.trash(key)) {
        if (error) *error = "Could not delete '" + key + "'.";
        return false;
    }
    if (key == key_) {
        cancelCopy();
        key_ = !basedOn.empty() && store_.exists(basedOn) ? basedOn : "default";
        store_.notePicked(key_);
        reload();
    }
    ++revision_;
    return true;
}

bool Agent::restoreProfile(const std::string& key) {
    if (!store_.restore(key)) return false;
    select(key);
    ++revision_;
    return true;
}

void Agent::select(const std::string& key) {
    if (key.empty() || key == key_ || !store_.exists(key)) return;
    cancelCopy();
    key_ = key;
    store_.notePicked(key);
    reload();
}

std::string Agent::suggestCopyName() const {
    std::string title = key_ == "default" ? "Default" : key_.compare(0, 9, "aircraft-") == 0 ? key_.substr(9) : key_;
    return store_.freeKey(title + " mine");
}

bool Agent::saveCopy(const std::string& name, std::string* error) {
    size_t a = name.find_first_not_of(' '), b = name.find_last_not_of(' ');
    std::string wanted = a == std::string::npos ? "" : name.substr(a, b - a + 1);
    if (wanted.empty()) {
        if (error) *error = "Type a name for the new profile.";
        return false;
    }
    std::string safe = store_.freeKey(wanted);   // file-name safe and not taken
    if (safe != wanted) {
        if (error) *error = "'" + wanted + "' is taken, or has characters a file name cannot have. Try '" + safe + "'.";
        return false;
    }
    Profile p;
    std::string err;
    if (!store_.exists(key_) || !store_.load(key_, p, &err)) p = profile_;
    for (const auto& e : pending_) e(p);
    p.standard = false;
    if (p.basedOn.empty()) p.basedOn = store_.isStandard(key_) ? key_ : "";
    p.name = wanted;
    pending_.clear();
    copyAsked_ = false;
    if (!store_.save(wanted, p, error)) return false;
    select(wanted);
    return true;
}

void Agent::cancelCopy() {
    pending_.clear();
    if (copyAsked_) ++revision_;
    copyAsked_ = false;
}

std::vector<std::string> Agent::profiles() const {
    std::vector<std::string> keys = store_.list();
    bool hasDefault = false;
    for (const std::string& k : keys) hasDefault |= k == "default";
    if (!hasDefault) keys.insert(keys.begin(), "default");
    return keys;
}

void Agent::start() {
    if (started_) return;
    started_ = true;
    input_.start();
    nextSlow_ = 0;
}

void Agent::stop() {
    if (!started_) return;
    started_ = false;
    input_.stop();
    if (deviceRunning_) device_.stop(!pluginActive_);
    deviceRunning_ = false;
}

void Agent::showLabel(const std::string& text) {
    label_ = text;
    labelUntil_ = now() + 3.0;
}

void Agent::setSimPort(int port) {
    simPort_ = port;
    link_.reset();
}

Color Agent::lightColor(int led, double t) {
    bool previewing = preview_ && t < previewUntil_;
    int mode = previewing ? preview_ : engine_.mode();
    Color modeColor = profile_.ledAt(mode, previewing ? previewPinkie_ : engine_.shift(), led);
    // A colour being previewed in another mode is shown as it is, not as the rules would.
    if (previewing) return modeColor;
    if (tryIndex_ >= 0 && t < tryUntil_ && tryIndex_ < (int)profile_.alarms.size() && profile_.alarms[tryIndex_].led == led)
        return alarmColorAt(profile_.alarms[tryIndex_], t);   // being looked at, whatever the sim says
    if (!link_) return modeColor;
    SimLink* link = link_.get();
    return shownColor(profile_.alarms, profile_.rules, led, modeColor, t,
                      [link, t](const std::string& ref, double& v) { return link->value(ref, t, v); });
}

std::string Agent::liveStatus(const std::string& ref) {
    if (ref.empty()) return "";
    if (pluginActive_) return "X-Plane is running with the plugin: it reads this dataref inside the sim.";
    double v = 0;
    if (link_ && link_->value(ref, now(), v)) {
        char buf[64];
        snprintf(buf, sizeof buf, "%g", v);
        return std::string("X-Plane answers: ") + buf;
    }
    return "No answer from X-Plane. Is it running, with Settings > Network > accept incoming connections on, and is the name right?";
}

bool Agent::peek(const std::string& ref, double& out) const {
    return link_ && !ref.empty() && link_->value(ref, now(), out);
}

// Live lights: ask X-Plane for what the rules need (the plugin does this itself when it runs).
void Agent::updateLive(double t) {
    std::vector<Trigger> masters = {profile_.masterScreen, profile_.masterLights};
    std::vector<std::string> refs = datarefsUsed(profile_.alarms, profile_.rules, masters);
    for (const BrightnessLink* b : {&profile_.brightScreen, &profile_.brightLights})
        if (b->active() && std::find(refs.begin(), refs.end(), b->dataref) == refs.end()) refs.push_back(b->dataref);
    if (watchPages_)
        for (const UserPage& pg : profile_.pages.pages)
            for (const PageRow& r : pg.rows)
                if (r.kind == RowKind::Value && !r.dataref.empty() && std::find(refs.begin(), refs.end(), r.dataref) == refs.end())
                    refs.push_back(r.dataref);
    if (refs.empty()) {
        link_.reset();
        return;
    }
    if (!link_) link_.reset(new SimLink(simPort_));
    link_->want(refs, t);
    link_->poll(t);
}

void Agent::tryAlarm(int index) {
    tryIndex_ = index >= 0 && index < (int)profile_.alarms.size() ? index : -1;
    tryUntil_ = now() + 6.0;
}

int Agent::triedAlarm() const { return tryIndex_ >= 0 && now() < tryUntil_ ? tryIndex_ : -1; }

void Agent::tryBright(int which, int level) {
    tryBright_ = which == 0 || which == 1 ? which : -1;
    tryBrightLevel_ = level ? 1 : 0;
    tryBrightUntil_ = now() + 5.0;
}

int Agent::triedBright() const { return tryBright_ >= 0 && now() < tryBrightUntil_ ? tryBright_ : -1; }

void Agent::tryMaster(int which) {
    tryMaster_ = which == 0 || which == 1 ? which : -1;
    tryMasterUntil_ = now() + 5.0;
}

int Agent::triedMaster() const { return tryMaster_ >= 0 && now() < tryMasterUntil_ ? tryMaster_ : -1; }

void Agent::previewMode(int mode, bool pinkie) {
    preview_ = mode >= 1 && mode <= 3 ? mode : 0;
    previewPinkie_ = pinkie;
    previewUntil_ = now() + 4.0;
}

void Agent::setTest(bool on) {
    input_.setWantAxes(on);
    if (!on) moved_.clear();
}

void Agent::setText(const std::string& l0, const std::string& l1, const std::string& l2) {
    haveText_ = true;
    text_.l[0] = l0;
    text_.l[1] = l1;
    text_.l[2] = l2;
}

void Agent::clearText() { haveText_ = false; }

std::vector<Agent::AxisView> Agent::axes() const {
    std::vector<AxisView> out;
    for (const AxisEvent& e : input_.latestAxes())
    {
        long v = e.value;
        auto it = axisSteady_.find({e.usagePage, e.usage});
        if (it != axisSteady_.end()) v = it->second;
        out.push_back({e.usagePage, e.usage, v, e.min, e.max, axisMoved_ == std::make_pair(e.usagePage, e.usage)});
    }
    return out;
}

std::string Agent::held() const {
    std::string s;
    for (int b = 1; b <= kMaxButton; ++b)
        if (engine_.isDown(b) && !(b >= 20 && b <= 23) && b < kPov1Up)   // POV 1 is listed below from the hat itself
            s += (s.empty() ? "" : ", ") + std::to_string(b) + " " + buttonName(b);
    // POV 2 is four buttons: two together are a diagonal
    std::string d = povButtonsText(engine_.isDown(20), engine_.isDown(21), engine_.isDown(22), engine_.isDown(23));
    if (!d.empty()) s += (s.empty() ? "" : ", ") + std::string("POV 2: ") + d;
    for (const AxisEvent& e : input_.latestAxes()) {   // POV 1 is a hat switch, not buttons
        if (e.usagePage != 0x01 || e.usage != 0x39) continue;
        std::string d = controlValueText(e.usagePage, e.usage, e.value, e.min, e.max);
        if (d != "centre") s += (s.empty() ? "" : ", ") + std::string(controlName(e.usagePage, e.usage)) + ": " + d;
    }
    return s;
}

void Agent::tick() {
    if (!started_) return;
    double t = now();

    std::vector<ButtonEvent> events;
    input_.drain(events);
    for (const ButtonEvent& e : events) engine_.buttonEvent(e.button, e.down, t);
    engine_.tick(t);

    for (const AxisEvent& e : input_.latestAxes()) {
        auto key = std::make_pair(e.usagePage, e.usage);
        if (e.usagePage == 0x01) calibrator_.feed(e.usage, e.value, e.min, e.max);
        if (AxisCurve* dial = e.usagePage == 0x01 ? dialCurve(profile_.axes, e.usage) : nullptr) {
            auto st = axisSteady_.find(key);
            axisSteady_[key] = st == axisSteady_.end() ? e.value : steadyValue(st->second, e.value, e.min, e.max, dial->deadzone);
        }
        auto it = axisRef_.find(key);
        if (it == axisRef_.end()) {   // where a control rests is not news
            axisRef_[key] = e.value;
        } else if (controlMoved(e.usagePage, e.usage, it->second, e.value, e.min, e.max)) {
            it->second = e.value;
            axisMoved_ = key;
        }
    }

    std::vector<AxisEvent> moves;
    input_.drainAxes(moves);
    for (const AxisEvent& m : moves)
        moved_ = std::string(controlName(m.usagePage, m.usage)) + ": " +
                 controlValueText(m.usagePage, m.usage, m.value, m.min, m.max);

    if (t >= nextSlow_) {
        nextSlow_ = t + 1.0;
        long long s = store_.stamp(key_);
        if (s != stamp_) reload();   // changed by the plugin's window or by hand
        if (calibrationStamp(store_.dir) != calibrationStamp_) loadCalibration();   // calibrated in X-Plane
        // X-Plane's plugin drives the stick while it runs; step aside without touching it.
        bool busy = StickLock::heldByOther(store_.dir);
        if (busy && deviceRunning_) {
            device_.stop(false);
            deviceRunning_ = false;
        } else if (!busy && !deviceRunning_) {
            device_.start();   // sends everything afresh
            deviceRunning_ = true;
        }
        pluginActive_ = busy;
    }
    if (deviceRunning_) {
        updateLive(t);
        push(t);
    } else if (watchPages_) {
        updateLive(t);   // the plugin drives the stick, but the pages editor's preview still wants live numbers
    } else if (link_) {
        link_.reset();   // the plugin took over: stop asking
    }
}

void Agent::push(double t) {
    DeviceState s;
    Lines lines;
    if (haveText_) lines = text_;
    else lines = formatButtonsPage(profile_.name, t < labelUntil_ ? label_ : "", engine_.mode(), engine_.shift());
    int mode = preview_ && t < previewUntil_ ? preview_ : engine_.mode();
    for (int i = 0; i < kMfdLines; ++i) s.lines[i] = screen_.l[i] = mfdText(lines.l[i]);
    (void)mode;
    for (int l = 0; l < kLedCount; ++l) s.leds[l] = lightColor(l, t);
    s.shift = engine_.shift();
    s.blink = engine_.clutch();
    s.mfdBrightness = profile_.mfdBrightness;
    s.ledBrightness = profile_.ledBrightness;
    // Brightness and master switches (what X-Plane says over UDP, or the Try buttons).
    SimLink* link = link_.get();
    DatarefReader read;
    if (link) read = [link, t](const std::string& ref, double& v) { return link->value(ref, t, v); };
    s.mfdBrightness = followBrightness(profile_.brightScreen, s.mfdBrightness, read);
    s.ledBrightness = followBrightness(profile_.brightLights, s.ledBrightness, read);
    if (t < tryBrightUntil_ && tryBright_ >= 0) {
        const BrightnessLink& b = tryBright_ ? profile_.brightLights : profile_.brightScreen;
        (tryBright_ ? s.ledBrightness : s.mfdBrightness) = tryBrightLevel_ ? b.outHigh : b.outLow;
    }
    bool trying = t < tryMasterUntil_;
    if ((trying && tryMaster_ == 0) || !masterOn(profile_.masterScreen, read)) {
        for (int i = 0; i < kMfdLines; ++i) s.lines[i] = screen_.l[i] = "";
        s.shift = false;
        s.mfdBrightness = 0;
    }
    if ((trying && tryMaster_ == 1) || !masterOn(profile_.masterLights, read)) {
        for (int l = 0; l < kLedCount; ++l) s.leds[l] = Color::Off;
        s.ledBrightness = 0;
        s.blink = false;
    }
    time_t tt = time(nullptr);
    struct tm g, local;
    gmtime_r(&tt, &g);
    localtime_r(&tt, &local);
    ClockTime gmt;
    gmt.hour = g.tm_hour;
    gmt.minute = g.tm_min;
    gmt.day = g.tm_mday;
    gmt.month = g.tm_mon + 1;
    gmt.year2 = g.tm_year % 100;
    int first = clock1FromGmt(profile_, (int)(local.tm_gmtoff / 60), false, 0);   // no simulator time here
    ClockTime c1 = shiftClock(gmt, first);
    s.clockValid = true;
    s.hour = c1.hour;
    s.minute = c1.minute;
    s.day = c1.day;
    s.month = c1.month;
    s.year2 = c1.year2;
    for (int c = 0; c < 2; ++c) s.clockOffset[c] = clockOffsetFromFirst(profile_.clockOffset[c], first);
    for (int c = 0; c < 3; ++c) s.h12[c] = profile_.clock12h[c];
    s.dateFormat = profile_.dateFormat;
    device_.setState(s);
}

}  // namespace x52
