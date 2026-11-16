// Finding X-Plane 12 on this Mac and putting the plugin into it. Plain POSIX, no X-Plane
// code, so it can be tested against a made-up home folder.
#pragma once
#include <string>
#include <vector>

namespace x52 {

struct SearchRoots {
    std::string home;                          // the user's home folder
    std::string applications = "/Applications";
    std::string volumes = "/Volumes";          // other disks
};

// A folder is an X-Plane install if it has Resources/plugins.
bool isXPlaneDir(const std::string& dir);

// Every X-Plane 12 install found, best first, no repeats, no trailing slash. Looks at:
// the install list X-Plane itself keeps (~/Library/Preferences/x-plane_install_12.txt),
// Steam's library list, and the usual places (Applications, home, Desktop, other disks).
std::vector<std::string> findXPlane(const SearchRoots& roots);

enum class PluginState { NotInstalled, Outdated, UpToDate };

// Compares the installed X52Pro.xpl with the copy bundled in the app.
PluginState pluginState(const std::string& xplaneDir, const std::string& bundledXpl);

// Copies the bundled plugin into <xplane>/Resources/plugins/X52Pro/mac_x64, swapping the
// file in so a running X-Plane is not disturbed. False and a message on failure.
bool installPlugin(const std::string& xplaneDir, const std::string& bundledXpl, std::string* error);

}  // namespace x52
