// Installing everything the app carries (plugin, ready-made profiles, guides) and taking it all out again.
// Plain POSIX, no X-Plane code, so it can be tested against a made-up home folder.
#pragma once
#include <string>
#include <vector>

namespace x52 {

struct SetupPaths {
    std::string home;        // the user's home folder
    std::string resources;   // the app's Contents/Resources (X52Pro.xpl, profiles/, guides/)
};

std::string supportDir(const SetupPaths& p);   // <home>/Library/Application Support/X52Pro
std::string guidesDir(const SetupPaths& p);    // <home>/Documents/X52 Pro
// <X-Plane>/Output/preferences/X52Pro; the support folder when there is no X-Plane ("").
std::string profilesDir(const SetupPaths& p, const std::string& xplaneDir);

struct InstallChoice {
    bool plugin = true, profiles = true, guides = true;
};
struct Report {
    std::vector<std::string> done, kept, failed;   // one path or sentence each
};

// Writes what is chosen. A profile that is not ready-made is never overwritten (it is listed in `kept`).
// Notes what it did in the install record.
Report install(const SetupPaths& p, const std::string& xplaneDir, const InstallChoice& choice);

int bundledProfileCount(const SetupPaths& p);

struct InstallRecord {
    InstallChoice chosen{false, false, false};   // what was ticked the last time
    std::vector<std::string> xplanes, files;     // every X-Plane folder and file ever written
};
bool readRecord(const SetupPaths& p, InstallRecord& out);   // false: setup never finished

// True when setup never finished, or something that was chosen is missing or older than the app's copy.
bool setupNeeded(const SetupPaths& p, const std::string& xplaneDir);

}  // namespace x52
