// Shared state of the plugin: the current profile, the engine, the hardware and what
// the programming window needs to know.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "../core/axes.h"
#include "../core/engine.h"
#include "../core/mfd.h"
#include "../core/pages.h"
#include "../core/pageview.h"
#include "../core/history.h"
#include "../core/profile.h"
#include "../core/xpjoy.h"
#include "../mac/x52device.h"
#include "../mac/x52input.h"
#include "store.h"

namespace x52 {

struct CommandInfo {
    std::string name;
    std::string description;
    std::string haystack;   // lower-case name + description, for searching
};

struct App : Host {
    App();

    Profile profile;
    bool usingAircraftProfile = false;
    std::string aircraft;        // loaded aircraft's file name without ".acf"
    Store store;
    Engine engine;
    X52Device device;
    X52Input input;

    int page = PageComNav1;
    PageCursor cursor;           // where the MFD's right wheel is on the current page
    // Pages: the built-in ones are 0..kPageCount-1 (see mfd.h); the user's top-level pages follow as kPageCount,
    // kPageCount+1, ... The left wheel goes through the radio pages, then the user's, and the Buttons page last.
    PageNav nav;                 // where the wheels are inside a user page
    int pageTotal() const;       // built-in pages plus the user's top-level ones
    std::vector<int> pageOrder() const;
    void stepPage(int direction);   // left wheel: +1 next, -1 previous
    void showFirstPage();           // the first page of the current mode's order, cursor reset
    int pageMode = 1;               // the mode whose order the left wheel was last following
    bool cursorSide = false;        // the radio layout the cursor was set for
    void followLayout();            // the layout changed (window, file reload): the cursor starts over
    std::string pageTitle(int p) const;
    const std::string* userPageId(int p) const;   // id of a user page's top-level page, or nullptr for a built-in one
    Lines userPageLines(double now);
    void setPage(int p);
    void wheel(Wheel w);         // right wheel turned or pressed
    void wheelAt(Wheel w, double now);   // the same at a given time (the time between clicks sets the jump size)
    double lastWheelTime = 0;
    Wheel lastWheelDir = Wheel::Select;

    // Picking a profile on the stick: clutch engaged + POV hat (up/down scroll, right use, left automatic).
    std::string manualKey;       // profile picked on the stick; empty = default / per-aircraft
    std::vector<std::string> pickKeys;
    int pickIndex = -1;          // -1: not picking
    int lastHat = -1;
    void hatMoved(int direction);
    // The profile picker in the window: "" goes back to the automatic choice (this aircraft's profile, else
    // Default), otherwise the profile with this key is used until the aircraft changes.
    void chooseProfile(const std::string& key);
    static std::string profileTitle(const std::string& key);

    StickLock lock;              // held while the plugin drives the stick
    long long loadedStamp = 0;   // the profile file as last read or written
    double nextSlowCheck = 0;
    // Switched off in the app (the plugin-off file): the flight loop only checks, once a second, whether it is on again.
    bool off = false;
    double nextOffCheck = 0;
    void setOff(bool off);
    bool testMode = false;       // the stick's screen names whatever is moved or pressed; buttons do nothing
    Lines testLines;
    Lines screen;                // what the stick's screen shows right now (mirrored in the window)
    void setTestMode(bool on);
    double nextDeviceUpdate = 0; // set to 0 to refresh the stick's screen on the next frame
    std::string label;           // last button label, shown on the screen
    double labelUntil = 0;
    std::string banner;          // the mode's name, shown on the screen after the mode dial moves
    double bannerUntil = 0;
    int lastPressed = 0;         // most recent button pressed on the stick
    long pressSerial = 0;        // bumps on every press, so the window can follow
    std::string notice;          // one-line message for the window
    bool askedKeyAccess = false;
    bool saveBlocked = false;    // the profile file on disk is unreadable; never overwrite it
    std::vector<CommandInfo> commands;
    ButtonLearner learner;       // which X-Plane joystick slot each stick button is (learned from presses)
    void saveAircraftDescription();
    void loadButtonMap();
    void saveButtonMap();

    // Called by the window after it changed `profile`: applies and saves it.
    void profileChanged();
    // Undo / redo of the edits to the profile in use (never for a ready-made one); each writes the file.
    bool canUndo() const { return !profile.standard && history.canUndo(); }
    bool canRedo() const { return !profile.standard && history.canRedo(); }
    bool undo();
    bool redo();
    History history;
    std::string historyKey;                 // the profile the history belongs to
    void trackHistory();                    // `profile` may have changed: note it (or start over for another profile)
    void loadForAircraft(const std::string& acfName);
    void makeAircraftProfile();
    // A copy of the profile in use, with this flight's changes, under a free name ("<title> mine"); a copy of a
    // ready-made profile is used instead of it from then on. Returns the new key, "" when it could not be saved.
    std::string saveCopy();
    void dropAircraftProfile();        // sets the aircraft's profile aside and goes back to Default
    bool canUndoDrop() const;          // the loaded aircraft has a profile that was set aside
    void undoDropAircraftProfile();
    void reload();
    std::string profileKey() const;
    void loadCommandList(const std::string& path, const std::string& xplaneDir = "");   // + the aircraft's own, if the folder is given
    double now() const;
    void lightColors(Color out[kLedCount], double now);   // mode colours, overridden by live rules

    // Host
    void commandBegin(const std::string& name) override;
    void commandEnd(const std::string& name) override;
    void commandOnce(const std::string& name) override;
    void keyDown(const std::string& combo) override;
    void keyUp(const std::string& combo) override;
    void showLabel(const std::string& text) override;
    void layerChanged(int mode, bool shift) override;
    void datarefStep(const Step& s) override;

private:
    void* findCommand(const std::string& name);
    void key(const std::string& combo, bool down);
    std::map<std::string, void*> commandCache_;
    std::map<std::string, double> saved;   // what a Save step remembered, by dataref name
public:
    // Every analog control seen on the stick, for the window's live view and the axis override.
    struct AxisLive {
        long value = 0, min = 0, max = 0;
        long reference = 0;       // the value when it last counted as moving
        double changedAt = 0;
    };
    std::map<std::pair<int, int>, AxisLive> axisLive;
    std::pair<int, int> axisMoved{0, 0};   // usage page / usage of the control that moved last
    double axisMovedAt = 0;
    void trackAxes(double now);            // updates axisLive / axisMoved from the stick (every frame)
    // Writes the shaped flight axes into X-Plane while the override is on; dt is the frame time (for the smoothing).
    void applyAxes(double dt = 0);
    void releaseAxes();                    // gives the axes back to X-Plane
    std::map<int, double> stickSent;       // the stick's smoothed input per usage, as last sent (before the damper)
    std::map<int, double> stickOut;        // what was last written into X-Plane per stick usage (after the damper)
    // Two lines for the Axes tab while the plugin drives the stick: what the plugin sends for pitch, and what
    // X-Plane then uses (its yoke, the elevator trim, the pitch rate), to see where a nose-up or down comes from.
    std::vector<std::string> pitchCheck();
    // The stick's own centre and ends (stick-calibration.txt next to the profiles; it belongs to the stick, not
    // to a profile). Calibrating: start, move everything to its limits, let go, finish.
    std::map<int, AxisCalibration> calibration;
    StickCalibrator calibrator;
    std::string calibrationNote;           // what the window says about the calibration
    long long calibrationStamp = 0;        // the calibration file as last read or written (the app can write it too)
    void loadCalibration();
    void saveCalibration();
    void startCalibration();
    bool finishCalibration();              // false: not saved, calibrationNote says why (still calibrating)
    void cancelCalibration();
    void resetCalibration();
    bool axesOverridden = false;           // the plugin has set X-Plane's override flags
    bool joyOverridden = false;            // ... the stick's one (override_joystick) among them
    bool thrOverridden = false;            // ... the throttle's one (override_throttles) among them
    ThrottleHandover throttleHandover;     // X-Plane's autothrottle has the throttle, or had it and the lever must catch up
    std::string throttleNote;              // what the window says about it ("" when the lever drives)
    double flapWritten = -1;               // the flap handle the slider last set (-1: not since the aircraft loaded)
    void applySliderFlaps();
    // The throttle's dials (speed brake in steps, zoom), driven from their steady values. The speed brake is written
    // only when the dial reaches another step and only after it moved since resetDials(); zoom steps go from where the
    // dial was when it was first seen after resetDials(). A job X-Plane drives itself (its own axis assignment) is
    // left alone.
    void applyDials(double now);
    void resetDials();                     // after an aircraft or profile change, or a dial's job changed
    void setDialUse(int usage, DialUse use);   // the window's choice: saves the profile and starts the dial afresh
    bool xplaneDrives(int assignment) const;   // some X-Plane axis is assigned to this job (kXpAxis...)
    bool xplaneDrives(const LeverJob& job) const;   // ...to any of a lever job's own numbers
    void readAxisAssignments();            // X-Plane's axis assignments, read once a second
    double speedBrakeWritten = -9;         // the speed-brake handle a dial last set (-9: none since resetDials)
    std::map<int, double> zoomAnchor;      // per dial usage: the position zoom steps are counted from
    bool dialsResetPending = false;
    double dialsResetAt = 0;               // a dial must have moved after this to set the speed brake
    std::vector<int> xpAxisUse;            // X-Plane's axis assignments
    // Lever jobs (prop, mixture, trims, cowl flaps, carb heat): soft takeover per dial usage, cleared by resetDials().
    std::map<int, LeverPickup> leverState;
    std::map<int, std::string> leverNotes;  // per dial usage: what the window says (waiting, no range), "" when held
    // The job's range for this aircraft (its own limits where X-Plane has them). False when it is empty.
    bool leverRange(const LeverJob& job, double& lo, double& hi);
    std::string leverNote(int usage) const;
    void applyLever(int usage, const LeverJob& job, const AxisLive& l, double now);   // one frame of a dial on a lever job
    std::map<std::pair<int, int>, long> axisShown;   // test mode: last value shown per moving control
};

extern App* gApp;

// plugin.cpp: one line saying what a dataref name resolves to right now ("" for an empty name).
std::string datarefReport(const std::string& spec);

// ui.cpp
#ifdef X52_NO_GL
extern double uiTipDelay;           // seconds the mouse rests on a button before its tip shows (tests set it to 0)
extern std::string uiTestClipboard;   // unit tests: stands in for the Mac clipboard
#endif
void uiCreate();
void uiDestroy();
void uiToggle();

}  // namespace x52
