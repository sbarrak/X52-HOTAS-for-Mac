#include "store.h"
#include "../core/mfd.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace x52 {

std::string Store::pathFor(const std::string& key) const { return dir + "/" + key + ".x52"; }

bool Store::exists(const std::string& key) const { return access(pathFor(key).c_str(), R_OK) == 0; }

std::string Store::keyForAircraft(const std::string& aircraft) {
    std::string k = "aircraft-";
    for (unsigned char c : aircraft) k += (isalnum(c) || c == '-' || c == '_' || c == '.' || c == ' ') ? (char)c : '_';
    return k;
}

bool Store::load(const std::string& key, Profile& out, std::string* error) const {
    std::ifstream f(pathFor(key));
    if (!f) {
        if (error) *error = "cannot open " + pathFor(key);
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return parse(ss.str(), out, error);
}

bool Store::save(const std::string& key, const Profile& p, std::string* error) const {
    if (isStandard(key)) {
        if (error) *error = "'" + key + "' is a ready-made profile and is never changed: save the change as a new profile";
        return false;
    }
    mkdir(dir.c_str(), 0755);
    Profile copy = p;
    copy.prune();
    // Write to a temporary file first so a crash never leaves a half-written profile.
    std::string path = pathFor(key), tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::trunc);
        if (!f) {
            if (error) *error = "cannot write " + tmp;
            return false;
        }
        f << serialize(copy);
        if (!f.good()) {
            if (error) *error = "cannot write " + tmp;
            return false;
        }
    }
    if (rename(tmp.c_str(), path.c_str()) != 0) {
        if (error) *error = "cannot replace " + path;
        return false;
    }
    return true;
}

std::vector<std::string> Store::list() const {
    std::vector<std::string> keys;
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() > 4 && n[0] != '.' && n.compare(n.size() - 4, 4, ".x52") == 0) keys.push_back(n.substr(0, n.size() - 4));
        }
        closedir(d);
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

long long Store::stamp(const std::string& key) const {
    struct stat st;
    if (stat(pathFor(key).c_str(), &st) != 0) return 0;
    return (long long)st.st_mtimespec.tv_sec * 1000000000LL + st.st_mtimespec.tv_nsec + st.st_size + st.st_ino;
}

std::string calibrationPath(const std::string& dir) { return dir + "/stick-calibration.txt"; }

std::map<int, AxisCalibration> loadCalibration(const std::string& dir) {
    std::ifstream f(calibrationPath(dir));
    std::stringstream ss;
    ss << f.rdbuf();
    return parseCalibration(ss.str());
}

bool saveCalibration(const std::string& dir, const std::map<int, AxisCalibration>& cal) {
    std::string path = calibrationPath(dir);
    if (cal.empty()) {
        ::remove(path.c_str());
        return true;
    }
    mkdir(dir.c_str(), 0755);
    std::ofstream f(path);
    f << serializeCalibration(cal);
    return (bool)f;
}

long long calibrationStamp(const std::string& dir) {
    struct stat st;
    if (stat(calibrationPath(dir).c_str(), &st) != 0) return 0;
    return (long long)st.st_mtimespec.tv_sec * 1000000000LL + st.st_mtimespec.tv_nsec + st.st_size + st.st_ino;
}

std::string describeCalibration(const std::map<int, AxisCalibration>& cal) {
    std::string out;
    for (const auto& kv : cal)
        out += std::string(controlName(0x01, kv.first)) + ": " + std::to_string(kv.second.min) + " .. " +
               std::to_string(kv.second.centre) + " .. " + std::to_string(kv.second.max) + "\n";
    return out;
}

namespace {
std::string lockPath(const std::string& dir) { return dir + "/.plugin-active"; }
}

bool StickLock::tryHold(const std::string& dir) {
    if (held()) return true;
    mkdir(dir.c_str(), 0755);
    int f = open(lockPath(dir).c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0644);
    if (f < 0) return false;
    if (flock(f, LOCK_EX | LOCK_NB) != 0) {
        close(f);
        return false;
    }
    fd = f;
    return true;
}

void StickLock::release() {
    if (fd >= 0) close(fd);
    fd = -1;
}

bool StickLock::heldByOther(const std::string& dir) {
    int f = open(lockPath(dir).c_str(), O_RDWR | O_CLOEXEC);
    if (f < 0) return false;   // no lock file: the plugin has never run here
    bool busy = flock(f, LOCK_EX | LOCK_NB) != 0;
    close(f);                  // also gives the lock straight back if we got it
    return busy;
}

bool pluginOff(const std::string& dir) { return access((dir + "/plugin-off").c_str(), F_OK) == 0; }

bool setPluginOff(const std::string& dir, bool off) {
    std::string path = dir + "/plugin-off";
    if (!off) return unlink(path.c_str()) == 0 || errno == ENOENT;
    mkdir(dir.c_str(), 0755);
    std::ofstream f(path);
    f << "The X-Plane plugin is switched off in the X52 Pro app. Switch it on there, or delete this file.\n";
    return (bool)f;
}

bool Store::remove(const std::string& key) const { return unlink(pathFor(key).c_str()) == 0; }

bool Store::trash(const std::string& key) const {
    return rename(pathFor(key).c_str(), (pathFor(key) + ".deleted").c_str()) == 0;
}

bool Store::hasTrash(const std::string& key) const { return access((pathFor(key) + ".deleted").c_str(), R_OK) == 0; }

bool Store::restore(const std::string& key) const {
    if (!hasTrash(key) || exists(key)) return false;
    return rename((pathFor(key) + ".deleted").c_str(), pathFor(key).c_str()) == 0;
}

bool Store::isStandard(const std::string& key) const {
    std::ifstream f(pathFor(key));
    for (std::string line; std::getline(f, line);) {
        if (line.compare(0, 8, "standard") != 0) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string v = line.substr(eq + 1);
        v.erase(0, v.find_first_not_of(" \t"));
        return !v.empty() && v[0] != '0';
    }
    return false;
}

const std::map<std::string, std::string>& Store::choices() const {
    std::string path = dir + "/profile-choices.txt";
    struct stat st;
    long long s = stat(path.c_str(), &st) == 0 ? (long long)st.st_mtimespec.tv_sec * 1000000000LL + st.st_mtimespec.tv_nsec + st.st_size : 0;
    if (s == choicesStamp_) return choices_;
    choicesStamp_ = s;
    choices_.clear();
    std::ifstream f(path);
    for (std::string line; std::getline(f, line);) {
        size_t tab = line.find('\t');
        if (line.empty() || line[0] == '#' || tab == std::string::npos) continue;
        choices_[line.substr(0, tab)] = line.substr(tab + 1);
    }
    return choices_;
}

std::string Store::choiceFor(const std::string& standardKey) const {
    const auto& c = choices();
    auto it = c.find(standardKey);
    return it == c.end() ? "" : it->second;
}

void Store::setChoice(const std::string& standardKey, const std::string& copyKey) const {
    if (standardKey.empty()) return;
    std::map<std::string, std::string> c = choices();
    if (copyKey.empty()) c.erase(standardKey);
    else c[standardKey] = copyKey;
    std::string path = dir + "/profile-choices.txt", tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::trunc);
        f << "# Your copy used instead of a ready-made profile: <ready-made profile><TAB><your copy>\n";
        for (const auto& kv : c) f << kv.first << "\t" << kv.second << "\n";
    }
    rename(tmp.c_str(), path.c_str());
    choicesStamp_ = -1;
}

void Store::notePicked(const std::string& key) const {
    if (key.empty() || !exists(key)) return;
    if (isStandard(key)) {
        setChoice(key, "");
        return;
    }
    Profile p;
    if (load(key, p, nullptr) && !p.basedOn.empty() && isStandard(p.basedOn)) setChoice(p.basedOn, key);
}

std::string Store::freeKey(const std::string& wanted) const {
    std::string base;
    for (unsigned char c : wanted) base += (isalnum(c) || c == '-' || c == '_' || c == '.' || c == ' ') ? (char)c : '_';
    size_t a = base.find_first_not_of(' '), b = base.find_last_not_of(' ');
    base = a == std::string::npos ? "My profile" : base.substr(a, b - a + 1);
    std::string key = base;
    for (int n = 2; key == "default" || exists(key) || hasTrash(key); ++n) key = base + " " + std::to_string(n);
    return key;
}

}  // namespace x52
