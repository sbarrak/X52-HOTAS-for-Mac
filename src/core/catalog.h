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

// Entries whose name or description contains every word of `query` (any case), best first:
// names that start with the query, then names that contain it, then description matches.
// At most `max` entries; an empty query gives none.
std::vector<const CatalogEntry*> searchCatalog(const std::vector<CatalogEntry>& list, const std::string& query, int max);

}  // namespace x52
