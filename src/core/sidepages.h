// The side-by-side radio screens: both units of a kind on one screen (COM 1+2, NAV 1+2, ADF 1+2), chosen per
// profile instead of the classic radio pages (see pages.h). They use the classic pages' slots: PageComNav1 is the
// COM page, PageComNav2 the NAV page and PageAdf the ADF page. Pure C++.
//
// Browse view: line 1 the names, line 2 the active values, line 3 the standbys. The right wheel visits
// [unit 1] -> unit 1's standby -> [unit 2] -> unit 2's standby; Select on a name swaps it with its standby, Select
// on a standby opens the edit view: that unit alone, centred, where the wheel changes the standby straight away and
// Select steps MHz -> kHz (ADF: hundreds -> tens -> ones) and back to the browse view.
#pragma once
#include <string>
#include <vector>
#include "pages.h"

namespace x52 {

bool isSidePage(int page);
Lines formatSidePage(int page, const SimData& d, const PageCursor& c);
std::vector<std::string> sidePageInput(int page, PageCursor& c, Wheel w, bool com833, int repeat);

}  // namespace x52
