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

// The names in a folder, sorted, without "." and "..". `opened` says whether the folder could be listed.
std::vector<std::string> namesIn(const std::string& dir, bool* opened = nullptr) {
    std::vector<std::string> out;
    DIR* d = opendir(dir.c_str());
    if (opened) *opened = d != nullptr;
    if (d) {
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

enum class Kind { Missing, File, Folder, Other };
// What is at `path` itself: a symbolic link is Other, whatever it points to.
Kind kindOf(const std::string& path) {
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) return Kind::Missing;
    return S_ISREG(st.st_mode) ? Kind::File : S_ISDIR(st.st_mode) ? Kind::Folder : Kind::Other;
}

// The names in one of our folders. A folder that is not there, is a link, or cannot be read gives nothing
// (the last two are noted in the plan). A folder that can be looked into is pruned afterwards.
std::vector<std::string> openOurFolder(const std::string& dir, UninstallPlan& plan) {
    Kind kind = kindOf(dir);
    if (kind == Kind::Missing) return {};
    if (kind != Kind::Folder) {
        addOnce(plan.unknown, dir);
        return {};
    }
    // Opening it is the only sure test: macOS can refuse (Documents) a folder whose permissions allow it.
    bool opened = false;
    std::vector<std::string> names = namesIn(dir, &opened);
    if (!opened || access(dir.c_str(), X_OK) != 0) {
        addOnce(plan.unreadable, dir);
        return {};
    }
    addOnce(plan.folders, dir);
    return names;
}

void planPlugin(const std::string& xp, UninstallPlan& plan) {
    const std::string dir = xp + "/Resources/plugins/X52Pro";
    for (const std::string& n : openOurFolder(dir, plan)) {
        const std::string path = dir + "/" + n;
        if (n == "mac_x64" && kindOf(path) == Kind::Folder) {
            for (const std::string& m : openOurFolder(path, plan)) {
                bool ours = (m == "X52Pro.xpl" || m == "X52Pro.xpl.new" || m == ".DS_Store") && kindOf(path + "/" + m) == Kind::File;
                addOnce(ours ? plan.remove : plan.unknown, path + "/" + m);
            }
        } else {
            addOnce(n == ".DS_Store" ? plan.remove : plan.unknown, path);
        }
    }
}

void planProfiles(const std::string& dir, UninstallPlan& plan) {
    static const char* const generated[] = {".DS_Store", ".plugin-active", "plugin-off", "xplane-aircraft.txt",
                                            "xplane-map.txt", "install-record.txt.new"};
    for (const std::string& n : openOurFolder(dir, plan)) {
        const std::string path = dir + "/" + n;
        std::vector<std::string>* list = &plan.unknown;
        if (kindOf(path) != Kind::File) {
        } else if (n == "install-record.txt") {
            list = &plan.record;
        } else if (std::find(std::begin(generated), std::end(generated), n) != std::end(generated) || endsWith(n, ".tmp") ||
                   endsWith(n, ".x52.new")) {
            list = &plan.remove;
        } else if (endsWith(n, ".x52") || endsWith(n, ".x52.deleted")) {
            list = isReadyMadeFile(path) ? &plan.remove : &plan.custom;
        } else if (n == "profile-choices.txt" || n == "stick-calibration.txt") {
            list = &plan.custom;
        }
        addOnce(*list, path);
    }
}

void planGuides(const SetupPaths& p, const InstallRecord& rec, UninstallPlan& plan) {
    const std::string dir = guidesDir(p);
    std::vector<std::string> shipped = bundledGuides(p);
    for (const std::string& n : openOurFolder(dir, plan)) {
        const std::string path = dir + "/" + n;
        bool ours = kindOf(path) == Kind::File &&
                    (n == ".DS_Store" || std::find(shipped.begin(), shipped.end(), n) != shipped.end() ||
                     std::find(rec.files.begin(), rec.files.end(), path) != rec.files.end());
        addOnce(ours ? plan.remove : plan.unknown, path);
    }
}

// The app's own caches: everything in them goes (a link is removed itself, never followed).
void planCache(const std::string& path, UninstallPlan& plan) {
    Kind kind = kindOf(path);
    if (kind == Kind::Missing) return;
    if (kind != Kind::Folder) {
        addOnce(plan.remove, path);
        return;
    }
    for (const std::string& n : openOurFolder(path, plan)) planCache(path + "/" + n, plan);
}

// The same folder under another name (a link, an alias, another spelling of an accent) counts once.
void addFolderOnce(std::vector<std::string>& v, const std::string& dir) {
    struct stat a, b;
    if (stat(dir.c_str(), &a) == 0)
        for (const std::string& other : v)
            if (stat(other.c_str(), &b) == 0 && a.st_dev == b.st_dev && a.st_ino == b.st_ino) return;
    addOnce(v, dir);
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
                // Anything there that is not a ready-made file of its own (a link too) is the user's.
                if (lstat(to.c_str(), &st) == 0 && (!S_ISREG(st.st_mode) || !isReadyMadeFile(to)))
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
                if (S_ISREG(st.st_mode) && isReadyMadeFile(to) && !sameBytes(p.resources + "/profiles/" + n, to)) return true;
            }
    }
    if (rec.chosen.guides)
        for (const std::string& n : bundledGuides(p))
            if (!sameBytes(p.resources + "/guides/" + n, guidesDir(p) + "/" + n)) return true;
    return false;
}

UninstallPlan planUninstall(const SetupPaths& p, const std::vector<std::string>& foundXPlanes) {
    UninstallPlan plan;
    InstallRecord rec;
    readRecord(p, rec);
    std::vector<std::string> xplanes;
    // The recorded spelling first: guides and files are noted under it.
    for (const std::string& x : rec.xplanes) addFolderOnce(xplanes, x);
    for (const std::string& x : foundXPlanes) addFolderOnce(xplanes, withoutSlash(x));
    for (const std::string& x : xplanes) {
        if (!isXPlaneDir(x)) {
            plan.missing.push_back(x);
            continue;
        }
        planPlugin(x, plan);
        planProfiles(profilesDir(p, x), plan);
    }
    planProfiles(supportDir(p), plan);
    planGuides(p, rec, plan);
    const std::string library = p.home + "/Library";
    planCache(library + "/Caches/serbar.x52pro.app", plan);
    for (const std::string& n : namesIn(library + "/HTTPStorages"))
        if (n.compare(0, 17, "serbar.x52pro.app") == 0) planCache(library + "/HTTPStorages/" + n, plan);
    planCache(library + "/Saved Application State/serbar.x52pro.app.savedState", plan);
    // A folder inside another is longer, so longest first empties them from the inside.
    std::stable_sort(plan.folders.begin(), plan.folders.end(),
                     [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
    return plan;
}

Report removeFiles(const UninstallPlan& plan) {
    Report r;
    for (const std::string& f : plan.remove)
        if (unlink(f.c_str()) == 0 || errno == ENOENT)
            r.done.push_back(f);
        else
            r.failed.push_back(f + ": " + strerror(errno));
    for (const std::string& d : plan.unreadable) r.failed.push_back(d + ": could not be read");
    return r;
}

Report removeRecord(const UninstallPlan& plan) {
    UninstallPlan only;
    only.remove = plan.record;
    return removeFiles(only);
}

void pruneFolders(const UninstallPlan& plan) {
    for (const std::string& d : plan.folders) rmdir(d.c_str());
}

}  // namespace x52
