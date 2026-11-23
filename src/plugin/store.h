// Profile files on disk: <X-Plane>/Output/preferences/X52Pro/<key>.x52
#pragma once
#include <map>
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

    // Ready-made profiles (standard = 1) are never written; a change goes to a copy. Which copy stands in for a
    // standard profile (an aircraft's, or Default) is kept in <dir>/profile-choices.txt, "<standard key><TAB><copy key>".
    bool isStandard(const std::string& key) const;
    std::string choiceFor(const std::string& standardKey) const;   // "" when the standard profile itself is used
    void setChoice(const std::string& standardKey, const std::string& copyKey) const;   // "" goes back to the standard
    // Someone picked `key`: a standard profile goes back to itself, a copy of one stands in for it from now on.
    void notePicked(const std::string& key) const;
    // A free profile name made from `wanted` ("Citation X mine", "Citation X mine 2", ...), file-name safe.
    std::string freeKey(const std::string& wanted) const;

private:
    mutable long long choicesStamp_ = -1;
    mutable std::map<std::string, std::string> choices_;
    const std::map<std::string, std::string>& choices() const;
};

// The stick's calibration (see AxisCalibration), shared by the app and the plugin: <dir>/stick-calibration.txt.
// It belongs to the stick, not to a profile. Saving an empty one removes the file.
std::string calibrationPath(const std::string& dir);
std::map<int, AxisCalibration> loadCalibration(const std::string& dir);
bool saveCalibration(const std::string& dir, const std::map<int, AxisCalibration>& cal);
long long calibrationStamp(const std::string& dir);   // changes whenever the file is rewritten; 0 when there is none
// One line per calibrated axis, "Stick fwd/back: 10 .. 505 .. 1010" (measured min, centre, max).
std::string describeCalibration(const std::map<int, AxisCalibration>& cal);

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
