// Showing and moving around the user pages (see pagetree.h) on the stick's screen. Pure C++.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "mfd.h"
#include "pagetree.h"
#include "pages.h"

namespace x52 {

// Where the wheels are inside the user pages: which sub-pages were opened to get here and the cursor row.
struct PageNav {
    std::vector<std::string> stack;   // sub-pages opened from the top page, innermost last
    std::vector<int> cursors;         // the cursor of each page left behind
    int cursor = 0;
    void reset() { stack.clear(); cursors.clear(); cursor = 0; }
};

// The three lines for the page with this id (a top-level one), or the sub-page open inside it. The cursor row
// is marked with ">". A Back row comes first inside a sub-page.
Lines formatUserPage(const PageTree& t, const std::string& topId, PageNav& nav, const std::function<bool(const std::string&, double&)>& read);

// One turn or press of the right wheel: moves the cursor, or opens a link / goes back.
void pageTreeInput(const PageTree& t, const std::string& topId, PageNav& nav, Wheel w);

}  // namespace x52
