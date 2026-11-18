// Showing and moving around the user pages (see pagetree.h) on the stick's screen. Pure C++.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "mfd.h"
#include "pagetree.h"
#include "pages.h"
#include "profile.h"

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

// The left wheel. Pages are numbered as in the plugin: the built-in ones 0..kPageCount-1 (mfd.h), the user's
// top-level pages kPageCount, kPageCount+1, ... in file order.
const char* builtinPageKey(int page);   // "com1", "com2", "adf", "dme", "xpdr", "ap", "vorhdg", "buttons", "brightness"
struct WheelEntry {
    std::string token;   // a built-in page's key, or "@<id>" for a user page
    int page;
    bool hidden;
};
// Every page the left wheel can reach, in its order, hidden ones included: the profile's wheelOrder, then the pages
// it does not name in their usual order (radio pages, the user's pages, Buttons, Brightness), placed before Buttons
// and Brightness when the list names those. A page in wheelHidden is listed but skipped.
std::vector<WheelEntry> wheelEntries(const Profile& p);
std::vector<int> wheelOrder(const Profile& p);   // the pages the left wheel visits (never empty)

}  // namespace x52
