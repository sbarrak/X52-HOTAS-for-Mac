// The radio / autopilot pages of the stick's screen, with the cursor the MFD's right
// wheel moves and its button acts on (as the manual describes for the Flight Simulator
// plug-in). Pure C++: changes come back as X-Plane command names for the caller to run.
#pragma once
#include <string>
#include <vector>
#include "dial.h"
#include "mfd.h"

namespace x52 {

struct PageCursor {
    int item = 0;          // which value on the page the cursor is on
    bool editing = false;  // the wheel changes the value instead of moving the cursor
    int part = 0;          // side-by-side edit view: which step (COM/NAV 0 MHz 1 kHz; ADF 0 hundreds 1 tens 2 ones)
};

enum class Wheel { Up, Down, Select };

// How many values the cursor can visit on a page (0: nothing to change there).
int pageItemCount(int page, bool side = false);

// The three screen lines of a page. Markers follow the manual: [ ] around an active
// frequency (Select swaps it with standby), > or < next to the part of a value the
// cursor is on, doubled (>> or <<) while the wheel is changing it.
// side: the profile shows the side-by-side radio screens (sidepages.h) on the COM, NAV and ADF slots.
Lines formatSimPage(int page, const SimData& d, const PageCursor& c = PageCursor(), bool side = false);

// One wheel click or press. Moves the cursor or toggles editing, and returns the
// X-Plane commands to run once (empty when only the cursor changed).
// com833: fine COM steps are 8.33 kHz channels instead of 25 kHz.
// repeat: how many times the change is applied; only heading, altitude, vertical speed, speed, OBS and the fine
// COM/NAV values take it (the whole-MHz and digit values would overshoot or wrap).
std::vector<std::string> pageInput(int page, PageCursor& c, Wheel w, bool com833 = false, int repeat = 1, bool side = false);

// The Brightness page: the screen's and the lights' brightness (0..127 as stored), shown in percent. The wheel
// works as on the radio pages: turn to move the cursor, press to start changing (>>), turn to change in 5 % steps,
// press again to finish. The screen never goes below 10 %, so it cannot be turned off by accident. A brightness that
// follows a dataref (Master & brightness tab) shows "dataref" and is not changed here.
Lines formatBrightnessPage(int screen, int lights, bool screenLinked, bool lightsLinked, const PageCursor& c);
bool brightnessInput(PageCursor& c, Wheel w, int& screen, int& lights, bool screenLinked, bool lightsLinked);   // true when a value changed

}  // namespace x52
