// Profile files on disk: <X-Plane>/Output/preferences/X52Pro/<key>.x52
#pragma once
#include <string>
#include <vector>
#include "../core/profile.h"

namespace x52 {

struct Store {
    std::string dir;
    std::string pathFor(const std::string& key) const;
    bool exists(const std::string& key) const;
    bool load(const std::string& key, Profile& out, std::string* error) const;
    bool save(const std::string& key, const Profile& p, std::string* error) const;
    bool remove(const std::string& key) const;
    // "Deleting" a profile only sets it aside (<key>.x52.deleted, not listed as a profile), so it can be brought back.
    bool trash(const std::string& key) const;
    bool restore(const std::string& key) const;       // false if there is nothing to bring back or the profile exists again
    bool hasTrash(const std::string& key) const;
    std::vector<std::string> list() const;            // keys of every profile in the folder, sorted
    // Changes whenever the file is rewritten; 0 if it does not exist.
    long long stamp(const std::string& key) const;
    // File-name-safe version of an aircraft name.
    static std::string keyForAircraft(const std::string& aircraft);
};

// Says who is driving the stick's screen and lights. The X-Plane plugin holds this lock
// while it is enabled; the menu bar app stays quiet for as long as it is held. The
// system drops the lock by itself if the holder crashes.
struct StickLock {
    int fd = -1;
    bool held() const { return fd >= 0; }
    bool tryHold(const std::string& dir);              // true once this process holds it
    void release();
    static bool heldByOther(const std::string& dir);   // someone else holds it right now
};

}  // namespace x52
