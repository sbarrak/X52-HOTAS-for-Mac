#include "store.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

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

bool Store::remove(const std::string& key) const { return unlink(pathFor(key).c_str()) == 0; }

bool Store::trash(const std::string& key) const {
    return rename(pathFor(key).c_str(), (pathFor(key) + ".deleted").c_str()) == 0;
}

bool Store::hasTrash(const std::string& key) const { return access((pathFor(key) + ".deleted").c_str(), R_OK) == 0; }

bool Store::restore(const std::string& key) const {
    if (!hasTrash(key) || exists(key)) return false;
    return rename((pathFor(key) + ".deleted").c_str(), pathFor(key).c_str()) == 0;
}

}  // namespace x52
