#include "xplane.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>

namespace x52 {

namespace {

bool isDir(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string trimmed(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t')) ++a;
    s.erase(0, a);
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    return s;
}

std::vector<std::string> subdirs(const std::string& dir) {
    std::vector<std::string> out;
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d))
            if (e->d_name[0] != '.') out.push_back(dir + "/" + e->d_name);
        closedir(d);
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

std::string pluginDir(const std::string& xplane) { return xplane + "/Resources/plugins/X52Pro/mac_x64"; }

bool makeDirs(const std::string& path) {
    for (size_t i = 1; i <= path.size(); ++i)
        if (i == path.size() || path[i] == '/') {
            std::string part = path.substr(0, i);
            if (!isDir(part) && mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) return false;
        }
    return true;
}

}  // namespace

bool isXPlaneDir(const std::string& dir) { return !dir.empty() && isDir(dir + "/Resources/plugins"); }

std::vector<std::string> findXPlane(const SearchRoots& r) {
    std::vector<std::string> candidates;

    // 1. The list X-Plane keeps of its own installs, one folder per line.
    std::string list;
    if (readFile(r.home + "/Library/Preferences/x-plane_install_12.txt", list)) {
        std::istringstream in(list);
        for (std::string line; std::getline(in, line);)
            if (!trimmed(line).empty()) candidates.push_back(trimmed(line));
    }

    // 2. Steam: every library folder it knows may hold X-Plane 12.
    std::vector<std::string> steamRoots = {r.home + "/Library/Application Support/Steam"};
    std::string vdf;
    if (readFile(r.home + "/Library/Application Support/Steam/steamapps/libraryfolders.vdf", vdf)) {
        std::istringstream in(vdf);
        for (std::string line; std::getline(in, line);) {
            size_t k = line.find("\"path\"");
            if (k == std::string::npos) continue;
            size_t a = line.find('"', k + 6), b = a == std::string::npos ? a : line.find('"', a + 1);
            if (b != std::string::npos) steamRoots.push_back(line.substr(a + 1, b - a - 1));
        }
    }
    for (const std::string& s : steamRoots) candidates.push_back(s + "/steamapps/common/X-Plane 12");

    // 3. The usual places.
    const std::string name = "X-Plane 12";
    for (const std::string& base : {r.applications, r.home, r.home + "/Applications", r.home + "/Desktop",
                                    r.home + "/Documents", r.home + "/Games"}) {
        candidates.push_back(base + "/" + name);
        candidates.push_back(base + "/Steam Games/steamapps/common/" + name);
    }
    for (const std::string& vol : subdirs(r.volumes)) {
        candidates.push_back(vol + "/" + name);
        candidates.push_back(vol + "/SteamLibrary/steamapps/common/" + name);
        candidates.push_back(vol + "/Steam/steamapps/common/" + name);
        candidates.push_back(vol + "/Games/" + name);
    }

    std::vector<std::string> found;
    for (const std::string& c : candidates) {
        std::string d = trimmed(c);
        if (isXPlaneDir(d) && std::find(found.begin(), found.end(), d) == found.end()) found.push_back(d);
    }
    return found;
}

PluginState pluginState(const std::string& xplaneDir, const std::string& bundledXpl) {
    std::string installed, bundled;
    if (!readFile(pluginDir(xplaneDir) + "/X52Pro.xpl", installed)) return PluginState::NotInstalled;
    if (!readFile(bundledXpl, bundled)) return PluginState::UpToDate;   // nothing to compare with
    return installed == bundled ? PluginState::UpToDate : PluginState::Outdated;
}

bool installPlugin(const std::string& xplaneDir, const std::string& bundledXpl, std::string* error) {
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    if (!isXPlaneDir(xplaneDir)) return fail("That folder is not an X-Plane 12 install.");
    std::string bytes;
    if (!readFile(bundledXpl, bytes)) return fail("The plugin file is missing from the app.");
    std::string dir = pluginDir(xplaneDir);
    if (!makeDirs(dir)) return fail("Could not create " + dir + ": " + strerror(errno));
    std::string tmp = dir + "/X52Pro.xpl.new";
    {
        std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
        o.write(bytes.data(), (std::streamsize)bytes.size());
        if (!o) return fail("Could not write " + tmp);
    }
    chmod(tmp.c_str(), 0755);
    if (rename(tmp.c_str(), (dir + "/X52Pro.xpl").c_str()) != 0) {
        unlink(tmp.c_str());
        return fail(std::string("Could not replace the plugin: ") + strerror(errno));
    }
    return true;
}

}  // namespace x52
