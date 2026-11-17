// Button engine: turns raw button presses into actions, applying modes, pinkie shift,
// tap-versus-hold, macros (sequences with waits) and the clutch "safe mode".
// Pure C++: everything it does to the outside world goes through Host.
#pragma once
#include <string>
#include <vector>
#include "profile.h"

namespace x52 {

struct Host {
    virtual ~Host() {}
    virtual void commandBegin(const std::string& name) = 0;
    virtual void commandEnd(const std::string& name) = 0;
    virtual void commandOnce(const std::string& name) = 0;
    virtual void keyDown(const std::string& combo) = 0;
    virtual void keyUp(const std::string& combo) = 0;
    virtual void showLabel(const std::string& text) = 0;
    virtual void layerChanged(int mode, bool shift) = 0;
    virtual void datarefStep(const Step&) {}   // a Dataref step ran: write the dataref
};

class Engine {
public:
    explicit Engine(Host& host);
    // Copies the profile. Anything currently held is released first.
    void setProfile(const Profile& p);
    void buttonEvent(int button, bool down, double now);
    void tick(double now);
    void releaseAll();

    int mode() const { return mode_; }                 // 1..3
    bool shift() const { return shift_; }
    int layer() const { return mode_ + (shift_ ? 3 : 0); }
    bool clutch() const { return clutch_; }
    bool isDown(int button) const;

private:
    enum class State { Idle, Held, PendingHold };
    struct Button {
        bool down = false;
        State state = State::Idle;
        double downTime = 0;
        Binding binding;
        Step held;          // the single step being held (Command or Key)
        bool hasHeld = false;
    };
    struct Running {
        Action steps;
        size_t next = 0;
        double resumeAt = 0;
    };

    void startHeld(Button& b, const Action& a, double now);
    void runOnce(const Action& a, double now);
    void advance(Running& r, double now);
    void endHeld(Button& b);

    Host& host_;
    Profile profile_;
    Button buttons_[kMaxButton + 1];
    std::vector<Running> running_;
    int mode_ = 1;
    int pendingMode_ = 0;       // dial position waiting to settle (0 = none)
    double pendingSince_ = 0;
    bool shift_ = false;
    bool clutch_ = false;
};

}  // namespace x52
