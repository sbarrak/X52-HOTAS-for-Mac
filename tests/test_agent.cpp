// The menu bar app's saving rules, without touching a real stick: Agent::open and
// Agent::change only read and write profile files.
#include <sys/stat.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstring>
#include <fstream>
#include <sstream>

#include "check.h"
#include "../src/app/agent.h"

using namespace x52;

static std::string root;
static std::string slurp(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
static bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }
static std::string freshDir(const char* name) {
    std::string d = root + "/" + name;
    mkdir(d.c_str(), 0755);
    return d;
}

TEST(changing_a_light_keeps_button_assignments_made_in_x_plane) {
    std::string dir = freshDir("keep");
    Agent a;
    a.open(dir, "default");
    // Meanwhile the plugin's window saves a button assignment the app has never seen.
    Store s;
    s.dir = dir;
    Profile fromPlugin = defaultProfile();
    fromPlugin.edit(3, 2).press.push_back({StepKind::Command, "sim/flight_controls/landing_gear_toggle", 0});
    fromPlugin.edit(3, 2).label = "Gear";
    fromPlugin.holdMs = 650;
    std::string err;
    CHECK(s.save("default", fromPlugin, &err));
    CHECK(a.change([](Profile& p) { p.leds[1][LedA] = Color::Red; }));
    std::string file = slurp(dir + "/default.x52");
    CHECK(has(file, "bind 3 2 press \"Gear\" cmd:sim/flight_controls/landing_gear_toggle"));
    CHECK(has(file, "hold_ms = 650"));
    CHECK(has(file, "a:red"));
    CHECK(a.profile().leds[1][LedA] == Color::Red);
    CHECK_EQ(a.profile().holdMs, 650);
}

TEST(saving_an_older_file_from_the_app_still_adds_the_wheel_bindings) {
    std::string dir = freshDir("old");
    {
        std::ofstream f(dir + "/default.x52");
        f << "x52pro-profile 1\nname = default\nbind 35 any press \"Screen page -\" cmd:x52pro/mfd_page_prev\n";
    }
    Agent a;
    a.open(dir, "default");
    CHECK(a.change([](Profile& p) { p.ledBrightness = 90; }));
    std::string file = slurp(dir + "/default.x52");
    CHECK(has(file, "x52pro-profile 3"));
    CHECK(has(file, "cmd:x52pro/mfd_select"));
    CHECK(has(file, "cmd:x52pro/mfd_page_prev"));
}

TEST(first_change_without_a_file_writes_a_complete_default_profile) {
    std::string dir = freshDir("new");
    Agent a;
    a.open(dir, "default");
    CHECK_EQ(a.error(), std::string(""));
    CHECK(a.change([](Profile& p) { p.ledBrightness = 40; }));
    std::string file = slurp(dir + "/default.x52");
    CHECK(has(file, "led_brightness = 40"));
    CHECK(has(file, "cmd:x52pro/mfd_page_next"));
}

TEST(a_file_with_a_mistake_is_never_overwritten) {
    std::string dir = freshDir("broken");
    {
        std::ofstream f(dir + "/default.x52");
        f << "x52pro-profile 2\nbind banana any press \"x\" cmd:y\n";
    }
    Agent a;
    a.open(dir, "default");
    CHECK(has(a.error(), "mistake"));
    CHECK(!a.change([](Profile& p) { p.ledBrightness = 1; }));
    CHECK(has(slurp(dir + "/default.x52"), "bind banana"));
    CHECK(has(a.error(), "Not saved"));
}

TEST(profiles_folder_follows_x_plane_when_it_is_installed) {
    std::string xp = freshDir("xp");
    CHECK_EQ(Agent::chooseDir(xp, "/fallback"), std::string("/fallback"));
    mkdir((xp + "/Output").c_str(), 0755);
    mkdir((xp + "/Output/preferences").c_str(), 0755);
    CHECK_EQ(Agent::chooseDir(xp, "/fallback"), xp + "/Output/preferences/X52Pro");
    CHECK_EQ(Agent::chooseDir("", "/fallback"), std::string("/fallback"));
}

TEST(a_missing_profile_falls_back_to_default_and_lists_it) {
    std::string dir = freshDir("list");
    Agent a;
    a.open(dir, "aircraft-Gone");
    CHECK_EQ(a.key(), std::string("default"));
    CHECK_EQ(a.profiles().size(), (size_t)1);
    a.select("aircraft-Gone");
    CHECK_EQ(a.key(), std::string("default"));
}

TEST(live_light_in_the_app_follows_x_plane_over_udp) {
    std::string dir = freshDir("live");
    int srv = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(srv, (sockaddr*)&a, sizeof a);
    socklen_t len = sizeof a;
    getsockname(srv, (sockaddr*)&a, &len);
    timeval tv{1, 0};
    setsockopt(srv, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    Agent ag;
    ag.open(dir, "default");
    ag.setSimPort(ntohs(a.sin_port));
    CHECK(ag.change([](Profile& p) { LightRule r = rulePreset(0).rule; r.led = LedE; p.rules.push_back(r); }));
    Color plain = ag.profile().leds[0][LedE];
    ag.updateLive(10.0);                              // subscribes
    CHECK(ag.lightColor(LedE, 10.0) == plain);        // no answer yet: the mode colour
    char buf[1024];
    sockaddr_in from{};
    socklen_t fl = sizeof from;
    CHECK_EQ(recvfrom(srv, buf, sizeof buf, 0, (sockaddr*)&from, &fl), (ssize_t)413);
    int id;
    memcpy(&id, buf + 9, 4);
    std::string ans("RREF,", 5);
    float v = 1.0f;
    ans.append((const char*)&id, 4);
    ans.append((const char*)&v, 4);
    sendto(srv, ans.data(), ans.size(), 0, (sockaddr*)&from, fl);
    for (int i = 0; i < 50 && ag.lightColor(LedE, 10.1) == plain; ++i) {
        usleep(10000);
        ag.updateLive(10.1);
    }
    CHECK(ag.lightColor(LedE, 10.1) == Color::Red);
    CHECK(ag.lightColor(LedE, 13.0) == plain);        // X-Plane went quiet
    close(srv);
}

TEST(an_alarm_can_be_tried_on_the_stick_and_alarms_are_saved_in_order) {
    std::string dir = freshDir("alarm");
    Agent ag;
    ag.open(dir, "default");
    CHECK(ag.change([](Profile& p) {
        Alarm a = alarmPreset(0).alarm;   // stall: red blink, fast, on the POV light
        a.led = LedA;
        Alarm b = alarmPreset(1).alarm;
        p.alarms = {a, b};
    }));
    CHECK_EQ(ag.profile().alarms.size(), (size_t)2);
    CHECK_EQ(ag.profile().alarms[0].name, std::string("Stall"));
    CHECK_EQ(ag.triedAlarm(), -1);
    Color plain = ag.profile().leds[0][LedA];
    CHECK(ag.lightColor(LedA, 10.0) == plain);        // nothing triggered, nothing tried
    ag.tryAlarm(0);
    CHECK_EQ(ag.triedAlarm(), 0);
    CHECK(ag.lightColor(LedA, 10.0) == Color::Red);   // on half of the 0.5 s cycle
    CHECK(ag.lightColor(LedA, 10.3) == Color::Off);   // off the other half
    CHECK(ag.lightColor(LedB, 10.0) == ag.profile().leds[0][LedB]);   // other lights untouched
    ag.tryAlarm(-1);
    CHECK_EQ(ag.triedAlarm(), -1);
    CHECK(ag.lightColor(LedA, 10.0) == plain);
}

int main() {
    const char* scratch = getenv("TMPDIR");
    std::string base = std::string(scratch ? scratch : "/tmp") + "/x52agent.XXXXXX";
    std::vector<char> buf(base.begin(), base.end());
    buf.push_back(0);
    if (!mkdtemp(buf.data())) {
        perror("mkdtemp");
        return 2;
    }
    root = buf.data();
    return runAllTests();
}
