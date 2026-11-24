#include "catalog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
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

// The quoted string ('..' or "..") that starts at s[i] after blanks; moves i past it. False when there is none.
bool quoted(const std::string& s, size_t& i, std::string& out) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    if (i >= s.size() || (s[i] != '"' && s[i] != '\'')) return false;
    char q = s[i];
    size_t end = s.find(q, i + 1);
    if (end == std::string::npos || s.find('\n', i) < end) return false;
    out = s.substr(i + 1, end - i - 1);
    i = end + 1;
    return true;
}

// Every call of `fn(` in the script with a quoted name and a quoted second argument; `more` tells if a third followed.
void scanCalls(const std::string& text, const std::string& fn, std::vector<std::vector<std::string>>& found) {
    for (size_t at = text.find(fn + "("); at != std::string::npos; at = text.find(fn + "(", at + 1)) {
        if (at > 0 && (isalnum((unsigned char)text[at - 1]) || text[at - 1] == '_')) continue;
        size_t i = at + fn.size() + 1;
        std::string name, second;
        if (!quoted(text, i, name) || name.find('/') == std::string::npos) continue;
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
        if (i >= text.size() || text[i] != ',') continue;
        ++i;
        if (!quoted(text, i, second)) continue;
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
        found.push_back({name, second, i < text.size() && text[i] == ',' ? "more" : ""});
    }
}

void scanScripts(const std::filesystem::path& dir, const std::string& aircraft, std::vector<CatalogEntry>& commands,
                 std::vector<CatalogEntry>& datarefs, std::set<std::string>& seenCmd, std::set<std::string>& seenRef) {
    namespace fs = std::filesystem;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec) || it->path().extension() != ".lua") continue;
        if (it->file_size(ec) > 8u * 1024 * 1024) continue;
        std::ifstream f(it->path());
        std::stringstream ss;
        ss << f.rdbuf();
        std::string text = ss.str();
        std::vector<std::vector<std::string>> found;
        scanCalls(text, "create_command", found);
        for (auto& r : found) {
            if (!seenCmd.insert(r[0]).second) continue;
            CatalogEntry e;
            e.name = r[0];
            e.info = aircraft + ": " + r[1];
            e.haystack = lowerOf(e.name + " " + e.info);
            commands.push_back(e);
        }
        found.clear();
        scanCalls(text, "create_dataref", found);
        for (auto& r : found) {
            if (!seenRef.insert(r[0]).second) continue;
            CatalogEntry e;
            e.name = r[0];
            e.writable = !r[2].empty();
            e.info = r[1] + ": " + aircraft;
            e.haystack = lowerOf(e.name + " " + aircraft);
            datarefs.push_back(e);
        }
    }
}

// Looks for plugins/xlua/scripts in the aircraft folders (Aircraft/<group>/<aircraft>, up to four levels down).
void findAircraft(const std::filesystem::path& dir, int depth, std::vector<CatalogEntry>& commands, std::vector<CatalogEntry>& datarefs,
                  std::set<std::string>& seenCmd, std::set<std::string>& seenRef) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path scripts = dir / "plugins" / "xlua" / "scripts";
    if (fs::is_directory(scripts, ec)) {
        scanScripts(scripts, dir.filename().string(), commands, datarefs, seenCmd, seenRef);
        return;
    }
    if (depth >= 4) return;
    std::vector<fs::path> subs;
    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_directory(ec) && it->path().filename().string()[0] != '.') subs.push_back(it->path());
    std::sort(subs.begin(), subs.end());
    for (const fs::path& p : subs) findAircraft(p, depth + 1, commands, datarefs, seenCmd, seenRef);
}

// DataRefTool lists every command and dataref X-Plane had registered when it last ran, one name per line, with
// no descriptions: it also has what the scripts do not show (compiled plugins, other scripting systems).
void addLastRun(const std::string& path, const std::string& info, std::vector<CatalogEntry>& out, std::set<std::string>& seen) {
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        std::string name = trimmed(line);
        if (name.find('/') == std::string::npos || name.find(' ') != std::string::npos) continue;
        if (!seen.insert(name).second) continue;
        CatalogEntry e;
        e.name = name;
        e.info = info;
        e.haystack = lowerOf(name + " " + info);
        out.push_back(e);
    }
}
}  // namespace

void loadAircraftCatalog(const std::string& xplaneDir, std::vector<CatalogEntry>& commands, std::vector<CatalogEntry>& datarefs) {
    if (xplaneDir.empty()) return;
    std::set<std::string> seenCmd, seenRef;
    for (const CatalogEntry& e : commands) seenCmd.insert(e.name);
    for (const CatalogEntry& e : datarefs) seenRef.insert(e.name);
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path root = fs::path(xplaneDir) / "Aircraft";
    if (!fs::is_directory(root, ec)) {
        addLastRun(xplaneDir + "/Output/preferences/drt_last_run_commandrefs.txt", "seen in X-Plane's last run", commands, seenCmd);
        addLastRun(xplaneDir + "/Output/preferences/drt_last_run_datarefs.txt", "seen in X-Plane's last run", datarefs, seenRef);
        return;
    }
    std::vector<fs::path> subs;
    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_directory(ec) && it->path().filename().string()[0] != '.') subs.push_back(it->path());
    std::sort(subs.begin(), subs.end());
    for (const fs::path& p : subs) findAircraft(p, 1, commands, datarefs, seenCmd, seenRef);
    addLastRun(xplaneDir + "/Output/preferences/drt_last_run_commandrefs.txt", "seen in X-Plane's last run", commands, seenCmd);
    addLastRun(xplaneDir + "/Output/preferences/drt_last_run_datarefs.txt", "seen in X-Plane's last run", datarefs, seenRef);
}

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
