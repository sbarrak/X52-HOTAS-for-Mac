// Finding X-Plane and installing the plugin, against a made-up home folder and disks.
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>

#include "check.h"
#include "../src/mac/xplane.h"

using namespace x52;

static std::string root;
static void mk(const std::string& p) {
    for (size_t i = 1; i <= p.size(); ++i)
        if (i == p.size() || p[i] == '/') mkdir(p.substr(0, i).c_str(), 0755);
}
static void put(const std::string& path, const std::string& text) {
    std::ofstream(path, std::ios::binary) << text;
}
static std::string fresh(const char* name) {
    std::string d = root + "/" + name;
    mk(d + "/home/Library/Preferences");
    mk(d + "/apps");
    mk(d + "/disks");
    return d;
}
static void fakeInstall(const std::string& dir) { mk(dir + "/Resources/plugins"); }
static SearchRoots roots(const std::string& d) { return {d + "/home", d + "/apps", d + "/disks"}; }

TEST(nothing_found_on_a_mac_without_x_plane) {
    CHECK(findXPlane(roots(fresh("none"))).empty());
}

TEST(the_install_list_x_plane_keeps_comes_first) {
    std::string d = fresh("list");
    fakeInstall(d + "/apps/X-Plane 12");
    fakeInstall(d + "/odd place/X-Plane 12");
    put(d + "/home/Library/Preferences/x-plane_install_12.txt", d + "/odd place/X-Plane 12/\n");
    std::vector<std::string> f = findXPlane(roots(d));
    CHECK_EQ((int)f.size(), 2);
    CHECK_EQ(f[0], d + "/odd place/X-Plane 12");   // trailing slash removed
    CHECK_EQ(f[1], d + "/apps/X-Plane 12");
}

TEST(a_stale_entry_that_is_not_x_plane_is_ignored) {
    std::string d = fresh("stale");
    put(d + "/home/Library/Preferences/x-plane_install_12.txt", d + "/gone/X-Plane 12\n");
    CHECK(findXPlane(roots(d)).empty());
}

TEST(steam_libraries_on_other_disks_are_found) {
    std::string d = fresh("steam");
    mk(d + "/home/Library/Application Support/Steam/steamapps");
    put(d + "/home/Library/Application Support/Steam/steamapps/libraryfolders.vdf",
        "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"" + d + "/bigdisk/Library\"\n\t}\n}\n");
    fakeInstall(d + "/bigdisk/Library/steamapps/common/X-Plane 12");
    std::vector<std::string> f = findXPlane(roots(d));
    CHECK_EQ((int)f.size(), 1);
    CHECK_EQ(f[0], d + "/bigdisk/Library/steamapps/common/X-Plane 12");
}

TEST(usual_places_and_other_disks_are_searched) {
    std::string d = fresh("usual");
    fakeInstall(d + "/home/Desktop/X-Plane 12");
    fakeInstall(d + "/disks/Flight/X-Plane 12");
    fakeInstall(d + "/disks/Games/SteamLibrary/steamapps/common/X-Plane 12");
    fakeInstall(d + "/disks/Games/Deep/Down/X-Plane 12");   // too deep: not searched
    CHECK_EQ((int)findXPlane(roots(d)).size(), 3);
}

TEST(plugin_install_reports_state_and_swaps_the_file) {
    std::string d = fresh("plugin");
    std::string xp = d + "/apps/X-Plane 12";
    fakeInstall(xp);
    put(d + "/bundled.xpl", "version one");
    CHECK(pluginState(xp, d + "/bundled.xpl") == PluginState::NotInstalled);
    std::string err;
    CHECK(installPlugin(xp, d + "/bundled.xpl", &err));
    CHECK(pluginState(xp, d + "/bundled.xpl") == PluginState::UpToDate);
    put(d + "/bundled.xpl", "version two");
    CHECK(pluginState(xp, d + "/bundled.xpl") == PluginState::Outdated);
    CHECK(installPlugin(xp, d + "/bundled.xpl", &err));
    CHECK(pluginState(xp, d + "/bundled.xpl") == PluginState::UpToDate);
    CHECK(access((xp + "/Resources/plugins/X52Pro/mac_x64/X52Pro.xpl.new").c_str(), F_OK) != 0);
}

TEST(plugin_install_refuses_a_folder_that_is_not_x_plane) {
    std::string d = fresh("refuse");
    put(d + "/bundled.xpl", "x");
    std::string err;
    CHECK(!installPlugin(d + "/apps", d + "/bundled.xpl", &err));
    CHECK(!err.empty());
}

int main() {
    char tmpl[] = "/tmp/x52xplaneXXXXXX";
    root = mkdtemp(tmpl);
    return runAllTests();
}
