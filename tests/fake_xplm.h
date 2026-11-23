// A stand-in for X-Plane, just big enough to run the plugin in a unit test.
#pragma once
#include <map>
#include <string>
#include <vector>

struct DrawnText {
    int x, y;
    std::string text;
};

struct FakeSim {
    std::string root;                       // temp folder playing the X-Plane folder
    std::string aircraftFile = "Cessna_172SP.acf";
    std::vector<DrawnText> drawn;           // text drawn in the last frame
    std::map<std::string, int> begins, ends, onces;
    std::map<std::string, double> datarefs;   // XPLMFindDataRef succeeds for these
    std::vector<std::string> knownCommands; // XPLMFindCommand succeeds for these
    std::string acfDescrip;                 // sim/aircraft/view/acf_descrip
    std::vector<int> joyValues;             // what sim/joystick/joystick_button_values reports (slot by slot)
    std::vector<int> axisAssignments;       // what sim/joystick/joystick_axis_assignments reports (axis by axis)
    bool windowVisible = false;
    bool hasKeyboardFocus = false;

    void frame();                           // run one flight loop + draw the window
    bool clickText(const std::string& text, int nth = 0, bool exact = false);
    bool hoverText(const std::string& text, int nth = 0, bool exact = false);   // puts the mouse on a drawn text   // click on drawn text
    bool sees(const std::string& text) const;
    void type(const std::string& s);
    void key(char ch, int vkey, int flags = 0);
    void sendMessagePlaneLoaded();
};
FakeSim& sim();
