// Installing and removing everything, against a made-up home folder and X-Plane.
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "check.h"
#include "../src/mac/setup.h"

using namespace x52;

static std::string root;
static void mk(const std::string& p) {
    for (size_t i = 1; i <= p.size(); ++i)
        if (i == p.size() || p[i] == '/') mkdir(p.substr(0, i).c_str(), 0755);
}
static void put(const std::string& path, const std::string& text) {
    mk(path.substr(0, path.rfind('/')));
    std::ofstream(path, std::ios::binary) << text;
}
static std::string get(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
static bool there(const std::string& path) {
    struct stat st;
    return lstat(path.c_str(), &st) == 0;
}
static bool has(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }
static std::string ready(const char* mark) { return std::string("x52pro-profile 3\nstandard = 1\n# ") + mark + "\n"; }
static std::string mine(const char* mark) { return std::string("x52pro-profile 3\n# ") + mark + "\n"; }

// A Mac with an X-Plane in it and the app's resources: plugin, two ready-made profiles, two guides.
struct World {
    std::string d, xp, prof, guides;
    SetupPaths p;
    World(const char* name, const std::string& xplaneName = "X-Plane 12") {
        d = root + "/" + name;
        xp = d + "/" + xplaneName;
        mk(xp + "/Resources/plugins");
        mk(xp + "/Output/preferences");
        mk(d + "/home");
        p = {d + "/home", d + "/res"};
        put(p.resources + "/X52Pro.xpl", "plugin one");
        put(p.resources + "/profiles/default.x52", ready("default one"));
        put(p.resources + "/profiles/aircraft-b738.x52", ready("737 one"));
        put(p.resources + "/guides/Manual.docx", "manual");
        put(p.resources + "/guides/737 map.pdf", "map");
        prof = profilesDir(p, xp);
        guides = guidesDir(p);
    }
    std::string xpl() const { return xp + "/Resources/plugins/X52Pro/mac_x64/X52Pro.xpl"; }
};

TEST(fresh_install_writes_plugin_profiles_and_guides) {
    World w("fresh");
    CHECK(setupNeeded(w.p, w.xp));
    Report r = install(w.p, w.xp, InstallChoice{});
    CHECK(r.failed.empty());
    CHECK(r.kept.empty());
    CHECK_EQ(get(w.xpl()), std::string("plugin one"));
    CHECK_EQ(w.prof, w.xp + "/Output/preferences/X52Pro");
    CHECK_EQ(get(w.prof + "/default.x52"), ready("default one"));
    CHECK_EQ(get(w.guides + "/737 map.pdf"), std::string("map"));
    CHECK_EQ(w.guides, w.d + "/home/Documents/X52 Pro");
    InstallRecord rec;
    CHECK(readRecord(w.p, rec));
    CHECK(has(rec.xplanes, w.xp));
    CHECK(has(rec.files, w.prof + "/default.x52"));
    CHECK(has(rec.files, w.guides + "/Manual.docx"));
    CHECK(has(rec.files, w.xpl()));
    CHECK(!setupNeeded(w.p, w.xp));
}

TEST(a_users_profile_with_a_ready_made_name_is_kept) {
    World w("userfile");
    put(w.prof + "/default.x52", mine("hands off"));
    Report r = install(w.p, w.xp, InstallChoice{});
    CHECK_EQ(get(w.prof + "/default.x52"), mine("hands off"));
    CHECK(has(r.kept, w.prof + "/default.x52"));
    CHECK(r.failed.empty());
    InstallRecord rec;
    CHECK(readRecord(w.p, rec));
    CHECK(!has(rec.files, w.prof + "/default.x52"));
    CHECK(!setupNeeded(w.p, w.xp));
}

TEST(an_older_ready_made_profile_is_updated) {
    World w("update");
    install(w.p, w.xp, InstallChoice{});
    put(w.p.resources + "/profiles/default.x52", ready("default two"));
    CHECK(setupNeeded(w.p, w.xp));
    Report r = install(w.p, w.xp, InstallChoice{});
    CHECK(r.failed.empty());
    CHECK_EQ(get(w.prof + "/default.x52"), ready("default two"));
    CHECK(!setupNeeded(w.p, w.xp));
}

TEST(a_newer_plugin_or_guide_asks_for_setup) {
    World w("newer");
    install(w.p, w.xp, InstallChoice{});
    put(w.p.resources + "/X52Pro.xpl", "plugin two");
    CHECK(setupNeeded(w.p, w.xp));
    install(w.p, w.xp, InstallChoice{});
    put(w.p.resources + "/guides/Manual.docx", "manual two");
    CHECK(setupNeeded(w.p, w.xp));
}

TEST(only_ready_made_files_count_as_bundled_profiles) {
    World w("bundled");
    put(w.p.resources + "/profiles/someone.x52", mine("not ready-made"));
    CHECK_EQ(bundledProfileCount(w.p), 2);
    install(w.p, w.xp, InstallChoice{});
    CHECK(!there(w.prof + "/someone.x52"));
}

TEST(unticked_items_are_not_written_or_nagged_about) {
    World w("unticked");
    Report r = install(w.p, w.xp, InstallChoice{true, false, false});
    CHECK(r.failed.empty());
    CHECK(there(w.xpl()));
    CHECK(!there(w.prof));
    CHECK(!there(w.guides));
    CHECK(!setupNeeded(w.p, w.xp));
}

TEST(a_folder_that_cannot_be_written_fails_alone) {
    World w("readonly");
    chmod((w.xp + "/Output/preferences").c_str(), 0555);
    Report r = install(w.p, w.xp, InstallChoice{});
    chmod((w.xp + "/Output/preferences").c_str(), 0755);
    CHECK(!r.failed.empty());
    CHECK(there(w.xpl()));
    CHECK(there(w.guides + "/Manual.docx"));
    CHECK(!there(w.prof));
    CHECK(setupNeeded(w.p, w.xp));
}

TEST(guides_install_without_x_plane) {
    World w("noxp");
    Report r = install(w.p, "", InstallChoice{});
    CHECK_EQ((int)r.failed.size(), 2);   // plugin and profiles
    CHECK(there(w.guides + "/Manual.docx"));
    CHECK(!there(supportDir(w.p) + "/default.x52"));
}

TEST(odd_paths_survive_the_record) {
    World w("odd", "V\xc3\xb4os de teste/X-Plane 12");
    install(w.p, w.xp + "/", InstallChoice{});
    InstallRecord rec;
    CHECK(readRecord(w.p, rec));
    CHECK_EQ((int)rec.xplanes.size(), 1);
    CHECK_EQ(rec.xplanes[0], w.xp);
    install(w.p, w.xp, InstallChoice{});   // a second install adds nothing twice
    CHECK(readRecord(w.p, rec));
    CHECK_EQ((int)rec.xplanes.size(), 1);
    CHECK_EQ((int)std::count(rec.files.begin(), rec.files.end(), w.xpl()), 1);
}

int main() {
    char tmpl[] = "/tmp/x52setupXXXXXX";
    root = mkdtemp(tmpl);
    int rc = runAllTests();
    system(("chmod -R u+w '" + root + "'; rm -rf '" + root + "'").c_str());
    return rc;
}
