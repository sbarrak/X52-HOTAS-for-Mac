// The menu bar app's engine room: keeps the stick's lights, screen and clock set from
// the saved profile while X-Plane is not running, and edits the settings the app owns.
// No Swift and no UI here, so the saving rules can be unit tested.
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../core/engine.h"
#include "../core/history.h"
#include "../core/mfd.h"
#include "../core/profile.h"
#include "../mac/x52device.h"
#include "../mac/simlink.h"
#include "../mac/x52input.h"
#include "../plugin/store.h"

namespace x52 {

class Agent : Host {
public:
    Agent();
    ~Agent();

    // Profiles live in <xplaneDir>/Output/preferences/X52Pro when X-Plane is installed
    // there, otherwise in fallbackDir. `key` is the profile to edit and apply.
    static std::string chooseDir(const std::string& xplaneDir, const std::string& fallbackDir);
    void open(const std::string& dir, const std::string& key);   // loads the profile; no hardware yet
    void start();                                                // begins talking to the stick
    void stop();
    void tick();                                                 // call about twenty times a second (alarm blinking needs it)

    // Changes settings in the profile file: reads the file as it is now, applies `edit`,
    // writes it back. Button assignments made in X-Plane are therefore never lost. An
    // unreadable file is left alone (false, see error()).
    // On a ready-made (standard) profile nothing is saved: the edit waits (copyNeeded()) until saveCopy() puts it,
    // and any after it, into a new profile, or cancelCopy() drops them.
    bool change(const std::function<void(Profile&)>& edit);
    // Undo / redo of the edits made to the profile in use (here, in the plugin's window or by hand), as long as it
    // stays the one in use. Not for ready-made profiles: they are never changed. Both write the file.
    bool canUndo() const { return !profile_.standard && history_.canUndo(); }
    bool canRedo() const { return !profile_.standard && history_.canRedo(); }
    bool undo();
    bool redo();
    bool copyNeeded() const { return copyAsked_; }
    void askCopy() { copyAsked_ = true; ++revision_; }        // "Save as new profile" without a change waiting
    // A new profile `name` made from this one with the waiting edits applied, switched to. False (see error) when the
    // name is taken or empty. A copy of a standard profile stands in for it from then on (see Store::notePicked).
    bool saveCopy(const std::string& name, std::string* error);
    void cancelCopy();
    bool standard() const { return profile_.standard; }
    std::string suggestCopyName() const;
    void select(const std::string& key);
    // Writes a new profile file and switches to it. Never replaces a profile that exists (false, see error).
    bool createProfile(const std::string& key, const Profile& p, std::string* error);
    // A new profile `name` with nothing programmed (no buttons, light rules or alarms, plain lights and the built-in
    // pages) that keeps this profile's axis settings, so the aircraft still flies; switched to. False (see error)
    // when the name is taken or empty.
    bool createBlank(const std::string& name, std::string* error);
    // Sets a profile of the user's aside (<key>.x52.deleted, see Store::trash) so restoreProfile can bring it back.
    // Default and ready-made profiles are never deleted. If it is in use, the ready-made profile it was copied from
    // (or Default) is used instead; a copy that stood in for a ready-made profile no longer does.
    bool deleteProfile(const std::string& key, std::string* error);
    bool restoreProfile(const std::string& key);   // brings a deleted profile back and switches to it

    const Profile& profile() const { return profile_; }
    const std::string& key() const { return key_; }
    const std::string& dir() const { return store_.dir; }
    const std::string& error() const { return error_; }
    std::vector<std::string> profiles() const;
    int revision() const { return revision_; }      // bumps whenever profile() changed
    bool connected() const { return device_.connected() || input_.connected(); }
    bool isPro() const { return device_.isPro(); }
    bool pluginActive() const { return pluginActive_; }
    int mode() const { return engine_.mode(); }
    bool shift() const { return engine_.shift(); }
    const Lines& screen() const { return screen_; }
    std::string held() const;                       // names of the buttons held right now
    const std::string& moved() const { return moved_; }

    // Every analog control the stick reports with where it sits now; `moving` marks the one that moved last.
    struct AxisView {
        int usagePage, usage;
        long value, min, max;
        bool moving;
    };
    std::vector<AxisView> axes() const;
    void pushAxisForTest(const AxisEvent& e) { input_.pushAxis(e); }   // tests: stand in for the stick

    // What a light shows at time t: the mode's colour, or its live rule when X-Plane answers
    // over UDP (only while the plugin is not driving the stick itself).
    Color lightColor(int led, double t);
    // One line on what a live rule's dataref is doing, for the settings window.
    std::string liveStatus(const std::string& dataref);
    void updateLive(double t);                      // subscribe to / read what the live lights need
    // While the pages editor is open, also ask X-Plane for the datarefs of the user pages' value rows, so the
    // preview shows live numbers. peek: the latest value X-Plane sent for a dataref, false when none.
    void watchPages(bool on) { watchPages_ = on; }
    bool peek(const std::string& dataref, double& out) const;
    void setSimPort(int port);                      // tests: talk to a made-up X-Plane

    // Shows alarm `index` on its light for a few seconds whatever the simulator says, so its
    // colours and blinking can be looked at (the Test tab). -1 stops it.
    void tryAlarm(int index);
    int triedAlarm() const;

    // Pretends master switch `which` (0 screen, 1 lights) is off for a few seconds whatever the simulator
    // says, so it can be seen working without X-Plane. -1 stops it.
    void tryMaster(int which);
    int triedMaster() const;

    // Shows the screen (which 0) or lights (which 1) at the dim (level 0) or bright (level 1) end of their
    // brightness link for a few seconds, whatever the simulator says. which -1 stops it.
    void tryBright(int which, int level);
    int triedBright() const;

    void previewMode(int mode, bool pinkie = false); // show this mode's lights for a few seconds
    void setTest(bool on);                          // report moving controls too
    void setText(const std::string& l0, const std::string& l1, const std::string& l2);
    void clearText();

private:
    void reload();
    void push(double now);
    double now() const;
    // Host: the app never runs button actions, it only shows their names.
    void commandBegin(const std::string&) override {}
    void commandEnd(const std::string&) override {}
    void commandOnce(const std::string&) override {}
    void keyDown(const std::string&) override {}
    void keyUp(const std::string&) override {}
    void showLabel(const std::string& text) override;
    void layerChanged(int, bool) override {}

    std::unique_ptr<SimLink> link_;
    int simPort_ = kXPlaneUdpPort;
    Store store_;
    History history_;
    std::string historyKey_;                 // the profile the history belongs to
    void observeHistory();                   // the profile may have changed: note it (or start over for a new profile)
    bool applyHistory(const std::string& text);
    std::string key_ = "default";
    Profile profile_;
    Engine engine_;
    X52Device device_;
    X52Input input_;
    bool started_ = false;
    bool pluginActive_ = false;
    bool deviceRunning_ = false;
    long long stamp_ = 0;
    double nextSlow_ = 0;
    int revision_ = 0;
    std::string error_;
    std::string label_;
    double labelUntil_ = 0;
    int preview_ = 0;
    bool previewPinkie_ = false;
    double previewUntil_ = 0;
    int tryBright_ = -1;
    int tryBrightLevel_ = 0;
    double tryBrightUntil_ = 0;
    int tryMaster_ = -1;
    double tryMasterUntil_ = 0;
    int tryIndex_ = -1;
    double tryUntil_ = 0;
    bool watchPages_ = false;
    std::vector<std::function<void(Profile&)>> pending_;   // edits to a standard profile, waiting for a name
    bool copyAsked_ = false;
    bool haveText_ = false;
    Lines text_;
    Lines screen_;
    std::string moved_;
    std::map<std::pair<int, int>, long> axisRef_;   // value when each control last counted as moving
    std::pair<int, int> axisMoved_{0, 0};
    std::map<std::pair<int, int>, long> axisSteady_;   // what dials and the slider show (small flicker held back)
};

}  // namespace x52
