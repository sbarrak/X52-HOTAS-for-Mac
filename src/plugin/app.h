// Shared state of the plugin: the current profile, the engine, the hardware and what
// the programming window needs to know.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "../core/engine.h"
#include "../core/mfd.h"
#include "../core/pages.h"
#include "../core/pageview.h"
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
    std::string pageTitle(int p) const;
    const std::string* userPageId(int p) const;   // id of a user page's top-level page, or nullptr for a built-in one
    Lines userPageLines(double now);
    void setPage(int p);
    void wheel(Wheel w);         // right wheel turned or pressed

    // Picking a profile on the stick: clutch engaged + POV hat (up/down scroll, right use, left automatic).
    std::string manualKey;       // profile picked on the stick; empty = default / per-aircraft
    std::vector<std::string> pickKeys;
    int pickIndex = -1;          // -1: not picking
    int lastHat = -1;
    void hatMoved(int direction);
    static std::string profileTitle(const std::string& key);

    StickLock lock;              // held while the plugin drives the stick
    long long loadedStamp = 0;   // the profile file as last read or written
    double nextSlowCheck = 0;
    bool testMode = false;       // the stick's screen names whatever is moved or pressed; buttons do nothing
    Lines testLines;
    Lines screen;                // what the stick's screen shows right now (mirrored in the window)
    void setTestMode(bool on);
    double nextDeviceUpdate = 0; // set to 0 to refresh the stick's screen on the next frame
    std::string label;           // last button label, shown on the screen
    double labelUntil = 0;
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
    void loadForAircraft(const std::string& acfName);
    void makeAircraftProfile();
    void dropAircraftProfile();        // sets the aircraft's profile aside and goes back to Default
    bool canUndoDrop() const;          // the loaded aircraft has a profile that was set aside
    void undoDropAircraftProfile();
    void reload();
    std::string profileKey() const;
    void loadCommandList(const std::string& path);
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
public:
    std::map<std::pair<int, int>, long> axisShown;   // test mode: last value shown per moving control
};

extern App* gApp;

// plugin.cpp: one line saying what a dataref name resolves to right now ("" for an empty name).
std::string datarefReport(const std::string& spec);

// ui.cpp
#ifdef X52_NO_GL
extern std::string uiTestClipboard;   // unit tests: stands in for the Mac clipboard
#endif
void uiCreate();
void uiDestroy();
void uiToggle();

}  // namespace x52
