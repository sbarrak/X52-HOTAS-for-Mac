// X-Plane's own joystick button assignments, read-only. X-Plane keeps one numbered slot per button
// ("_joy_BUTN_use<n> <command>" in its Joystick Settings file; sim/none/none means Do nothing), and
// the plugin works out which slot is which stick button by watching a press. Pure C++.
#pragma once
#include <map>
#include <set>
#include <string>

namespace x52 {

constexpr int kXPlaneButtonSlots = 3200;   // size of sim/joystick/joystick_button_values

// slot -> command name, from the text of "X-Plane Joystick Settings.prf".
std::map<int, std::string> parseButtonAssignments(const std::string& prfText);

// What X-Plane's own Joystick screen calls a slot: "Btn n". Each device starts with the eight directions of its
// hat (slots 0..7, shown as "Hat switch ..." rows), then Btn 0, Btn 1, ... Returns -1 for a hat direction.
// (Each device has 160 slots.)
int xplaneButtonNumber(int slot);

// X-Plane keeps one set of joystick assignments per aircraft, in "control profiles/<name> - Joysticks.prf", and
// Miscellaneous.prf says which one an aircraft uses ("P acf_profile <key> <name>", the key being the aircraft's
// description with spaces written as ___). "" when the aircraft has none (then the main settings file applies).
std::string controlProfileKey(const std::string& aircraftDescription);
std::string controlProfileFor(const std::string& miscellaneousPrfText, const std::string& aircraftDescription);

// stick button (1..39) -> X-Plane slot, as the plugin learned it. A small text file: "<button> <slot>" per line.
std::map<int, int> parseButtonMap(const std::string& text);
std::string serializeButtonMap(const std::map<int, int>& m);

// The slot of every stick button 1..39. X-Plane numbers this stick's buttons in order after the eight hat
// directions (stick button n is slot 7 + n on the first device), so the buttons the plugin has not seen pressed
// are worked out from the ones it has: the most common difference between a learned slot and its button is
// applied to the rest, and what was learned always wins.
std::map<int, int> completeButtonMap(const std::map<int, int>& learned);

// Learns which X-Plane slot a stick button is: after a press, the one slot that turns on (ignoring slots
// already known to be other buttons) is the button's. Feed it the plugin's presses and what the sim shows.
class ButtonLearner {
public:
    std::map<int, int>& map() { return map_; }
    const std::map<int, int>& map() const { return map_; }
    void pressed(int button, double now);                 // the stick reported this button going down
    bool pending(double now) const { return button_ && now < until_; }
    // The slots that are on right now. True when this learned (or corrected) a button's slot.
    bool update(const std::set<int>& activeSlots, double now);

private:
    std::map<int, int> map_;
    int button_ = 0;
    double until_ = 0;
};

}  // namespace x52
