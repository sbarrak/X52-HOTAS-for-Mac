// X-Plane's lists of commands and datarefs (Resources/plugins/Commands.txt and DataRefs.txt),
// so the menu bar app can search them without X-Plane running. Pure C++.
#pragma once
#include <string>
#include <vector>

namespace x52 {

struct CatalogEntry {
    std::string name;
    std::string info;        // description (commands) or "type, units: description" (datarefs)
    bool writable = true;    // datarefs only
    std::string haystack;    // lower-case name + info, for searching
};

// Files that are missing or unreadable give an empty list.
std::vector<CatalogEntry> loadCommandCatalog(const std::string& path);
std::vector<CatalogEntry> loadDatarefCatalog(const std::string& path);

// Add-on aircraft define their own commands and datarefs, which are in neither list. The Laminar aircraft (F-14,
// A330, ...) do it in xlua scripts, so read create_command("name", "description", ...) and create_dataref("name",
// "type", ...) from Aircraft/**/plugins/xlua/scripts/**/*.lua of the X-Plane folder and append them (once per name,
// with the aircraft's folder name in the info). Then add the names in DataRefTool's Output/preferences/
// drt_last_run_commandrefs.txt and drt_last_run_datarefs.txt (what X-Plane had registered when it last ran; no
// descriptions), which also covers aircraft with compiled plugins or other scripting.
void loadAircraftCatalog(const std::string& xplaneDir, std::vector<CatalogEntry>& commands, std::vector<CatalogEntry>& datarefs);

// Entries whose name or description contains every word of `query` (any case), best first:
// names that start with the query, then names that contain it, then description matches.
// At most `max` entries; an empty query gives none.
std::vector<const CatalogEntry*> searchCatalog(const std::vector<CatalogEntry>& list, const std::string& query, int max);

}  // namespace x52
