#include "catalog.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace x52 {

namespace {
std::string lowerOf(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}
std::string trimmed(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
}  // namespace

std::vector<CatalogEntry> loadCommandCatalog(const std::string& path) {
    std::vector<CatalogEntry> out;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        size_t end = line.find_first_of(" \t");
        CatalogEntry e;
        e.name = line.substr(0, end);
        if (e.name.find('/') == std::string::npos) continue;   // blank lines and the file's version header
        if (end != std::string::npos) e.info = trimmed(line.substr(end));
        e.haystack = lowerOf(e.name + " " + e.info);
        out.push_back(e);
    }
    return out;
}

std::vector<CatalogEntry> loadDatarefCatalog(const std::string& path) {
    std::vector<CatalogEntry> out;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        std::vector<std::string> col;
        std::stringstream ss(line);
        std::string item;
        while (std::getline(ss, item, '\t')) col.push_back(trimmed(item));
        if (col.empty() || col[0].find('/') == std::string::npos) continue;
        CatalogEntry e;
        e.name = col[0];
        std::string type = col.size() > 1 ? col[1] : "";
        e.writable = col.size() > 2 ? col[2] == "y" : true;
        std::string units = col.size() > 3 ? col[3] : "";
        std::string desc = col.size() > 4 ? col[4] : "";
        e.info = type + (units.empty() || units == "-" ? "" : ", " + units) + (desc.empty() ? "" : ": " + desc);
        e.haystack = lowerOf(e.name + " " + desc);
        out.push_back(e);
    }
    return out;
}

std::vector<const CatalogEntry*> searchCatalog(const std::vector<CatalogEntry>& list, const std::string& query, int max) {
    std::vector<std::string> words;
    {
        std::stringstream ss(lowerOf(query));
        std::string w;
        while (ss >> w) words.push_back(w);
    }
    std::vector<const CatalogEntry*> out;
    if (words.empty() || max <= 0) return out;
    std::string whole = lowerOf(trimmed(query));
    std::vector<std::pair<int, const CatalogEntry*>> hits;
    for (const CatalogEntry& e : list) {
        bool all = true;
        for (const std::string& w : words) all = all && e.haystack.find(w) != std::string::npos;
        if (!all) continue;
        std::string name = lowerOf(e.name);
        int rank = name.compare(0, whole.size(), whole) == 0 ? 0 : name.find(whole) != std::string::npos ? 1 : 2;
        hits.push_back({rank, &e});
    }
    std::stable_sort(hits.begin(), hits.end(), [](const std::pair<int, const CatalogEntry*>& a, const std::pair<int, const CatalogEntry*>& b) {
        return a.first < b.first;
    });
    for (size_t i = 0; i < hits.size() && (int)i < max; ++i) out.push_back(hits[i].second);
    return out;
}

}  // namespace x52
