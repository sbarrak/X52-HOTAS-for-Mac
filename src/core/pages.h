// The radio / autopilot pages of the stick's screen, with the cursor the MFD's right
// wheel moves and its button acts on (as the manual describes for the Flight Simulator
// plug-in). Pure C++: changes come back as X-Plane command names for the caller to run.
#pragma once
#include <string>
#include <vector>
#include "mfd.h"

namespace x52 {

struct PageCursor {
    int item = 0;          // which value on the page the cursor is on
    bool editing = false;  // the wheel changes the value instead of moving the cursor
};

enum class Wheel { Up, Down, Select };

// How many values the cursor can visit on a page (0: nothing to change there).
int pageItemCount(int page);

// The three screen lines of a page. Markers follow the manual: [ ] around an active
// frequency (Select swaps it with standby), > or < next to the part of a value the
// cursor is on, doubled (>> or <<) while the wheel is changing it.
Lines formatSimPage(int page, const SimData& d, const PageCursor& c = PageCursor());

// One wheel click or press. Moves the cursor or toggles editing, and returns the
// X-Plane commands to run once (empty when only the cursor changed).
// com833: fine COM steps are 8.33 kHz channels instead of 25 kHz.
std::vector<std::string> pageInput(int page, PageCursor& c, Wheel w, bool com833 = false);

}  // namespace x52
