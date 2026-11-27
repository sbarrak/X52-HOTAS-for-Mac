#include "setup.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

#include "../plugin/store.h"
#include "xplane.h"

namespace x52 {

namespace {

std::string withoutSlash(std::string s) {
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    return s;
}

bool endsWith(const std::string& s, const std::string& end) {
    return s.size() >= end.size() && s.compare(s.size() - end.size(), end.size(), end) == 0;
}

// The names in a folder, sorted, without "." and "..".
std::vector<std::string> namesIn(const std::string& dir) {
    std::vector<std::string> out;
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d))
            if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0) out.push_back(e->d_name);
        closedir(d);
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool isFile(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

bool makeDirs(const std::string& path) {
    for (size_t i = 1; i <= path.size(); ++i)
        if (i == path.size() || path[i] == '/') {
            std::string part = path.substr(0, i);
            struct stat st;
            if (stat(part.c_str(), &st) != 0 && mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) return false;
        }
    return true;
}

// Writes next to the target and swaps it in, so a half-written file is never left under its name.
bool writeFile(const std::string& path, const std::string& bytes, std::string* error) {
    std::string dir = path.substr(0, path.rfind('/')), tmp = path + ".new";
    auto fail = [&] {
        if (error) *error = path + ": " + strerror(errno);
        unlink(tmp.c_str());
        return false;
    };
    if (!makeDirs(dir)) return fail();
    {
        std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
        o.write(bytes.data(), (std::streamsize)bytes.size());
        o.flush();
        if (!o) return fail();
    }
    if (rename(tmp.c_str(), path.c_str()) != 0) return fail();
    return true;
}

std::vector<std::string> bundledProfiles(const SetupPaths& p) {
    std::vector<std::string> out;
    for (const std::string& n : namesIn(p.resources + "/profiles"))
        if (endsWith(n, ".x52") && isReadyMadeFile(p.resources + "/profiles/" + n)) out.push_back(n);
    return out;
}

std::vector<std::string> bundledGuides(const SetupPaths& p) {
    std::vector<std::string> out;
    for (const std::string& n : namesIn(p.resources + "/guides"))
        if (n[0] != '.' && isFile(p.resources + "/guides/" + n)) out.push_back(n);
    return out;
}

std::string recordPath(const SetupPaths& p) { return supportDir(p) + "/install-record.txt"; }

void addOnce(std::vector<std::string>& v, const std::string& s) {
    if (std::find(v.begin(), v.end(), s) == v.end()) v.push_back(s);
}

// Copies one bundled file to `to` and notes how it went.
void copyInto(const std::string& from, const std::string& to, Report& r) {
    std::string bytes, error;
    if (!readFile(from, bytes))
        r.failed.push_back(from + ": missing from the app");
    else if (!writeFile(to, bytes, &error))
        r.failed.push_back(error);
    else
        r.done.push_back(to);
}

bool sameBytes(const std::string& a, const std::string& b) {
    std::string x, y;
    return readFile(a, x) && readFile(b, y) && x == y;
}

}  // namespace

std::string supportDir(const SetupPaths& p) { return p.home + "/Library/Application Support/X52Pro"; }
std::string guidesDir(const SetupPaths& p) { return p.home + "/Documents/X52 Pro"; }
std::string profilesDir(const SetupPaths& p, const std::string& xplaneDir) {
    return xplaneDir.empty() ? supportDir(p) : withoutSlash(xplaneDir) + "/Output/preferences/X52Pro";
}

int bundledProfileCount(const SetupPaths& p) { return (int)bundledProfiles(p).size(); }

bool readRecord(const SetupPaths& p, InstallRecord& out) {
    std::ifstream f(recordPath(p));
    if (!f) return false;
    out = InstallRecord{};
    for (std::string line; std::getline(f, line);) {
        size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string key = line.substr(0, tab), value = line.substr(tab + 1);
        if (key == "item" && value == "plugin") out.chosen.plugin = true;
        if (key == "item" && value == "profiles") out.chosen.profiles = true;
        if (key == "item" && value == "guides") out.chosen.guides = true;
        if (key == "xplane") addOnce(out.xplanes, value);
        if (key == "file") addOnce(out.files, value);
    }
    return true;
}

Report install(const SetupPaths& p, const std::string& xplaneDir, const InstallChoice& choice) {
    Report r;
    const std::string xp = withoutSlash(xplaneDir);
    const bool haveXPlane = isXPlaneDir(xp);

    if (choice.plugin) {
        std::string error;
        if (!haveXPlane)
            r.failed.push_back("Plugin: no X-Plane 12 folder.");
        else if (installPlugin(xp, p.resources + "/X52Pro.xpl", &error))
            r.done.push_back(xp + "/Resources/plugins/X52Pro/mac_x64/X52Pro.xpl");
        else
            r.failed.push_back("Plugin: " + error);
    }
    if (choice.profiles) {
        if (!haveXPlane)
            r.failed.push_back("Profiles: no X-Plane 12 folder.");
        else
            for (const std::string& n : bundledProfiles(p)) {
                std::string to = profilesDir(p, xp) + "/" + n;
                struct stat st;
                if (lstat(to.c_str(), &st) == 0 && !isReadyMadeFile(to))
                    r.kept.push_back(to);
                else
                    copyInto(p.resources + "/profiles/" + n, to, r);
            }
    }
    if (choice.guides)
        for (const std::string& n : bundledGuides(p)) copyInto(p.resources + "/guides/" + n, guidesDir(p) + "/" + n, r);

    InstallRecord rec;
    readRecord(p, rec);
    rec.chosen = choice;
    if (haveXPlane && (choice.plugin || choice.profiles)) addOnce(rec.xplanes, xp);
    for (const std::string& f : r.done) addOnce(rec.files, f);
    std::string text;
    if (choice.plugin) text += "item\tplugin\n";
    if (choice.profiles) text += "item\tprofiles\n";
    if (choice.guides) text += "item\tguides\n";
    for (const std::string& x : rec.xplanes) text += "xplane\t" + x + "\n";
    for (const std::string& f : rec.files) text += "file\t" + f + "\n";
    std::string error;
    if (!writeFile(recordPath(p), text, &error)) r.failed.push_back(error);
    return r;
}

bool setupNeeded(const SetupPaths& p, const std::string& xplaneDir) {
    InstallRecord rec;
    if (!readRecord(p, rec)) return true;
    const std::string xp = withoutSlash(xplaneDir);
    if (isXPlaneDir(xp)) {
        if (rec.chosen.plugin && pluginState(xp, p.resources + "/X52Pro.xpl") != PluginState::UpToDate) return true;
        if (rec.chosen.profiles)
            for (const std::string& n : bundledProfiles(p)) {
                std::string to = profilesDir(p, xp) + "/" + n;
                struct stat st;
                if (lstat(to.c_str(), &st) != 0) return true;
                if (isReadyMadeFile(to) && !sameBytes(p.resources + "/profiles/" + n, to)) return true;
            }
    }
    if (rec.chosen.guides)
        for (const std::string& n : bundledGuides(p))
            if (!sameBytes(p.resources + "/guides/" + n, guidesDir(p) + "/" + n)) return true;
    return false;
}

}  // namespace x52
