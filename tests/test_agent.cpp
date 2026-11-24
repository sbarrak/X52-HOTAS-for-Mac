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
#include "../src/app/bridge.h"

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
    CHECK(has(file, "x52pro-profile 10"));
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

TEST(a_master_switch_can_be_tried_on_the_stick_without_the_sim) {
    std::string dir = freshDir("master");
    Agent ag;
    ag.open(dir, "default");
    CHECK(ag.change([](Profile& p) {
        p.masterScreen.dataref = "sim/cockpit2/switches/avionics_power_on";
        p.masterLights.dataref = "sim/cockpit/electrical/battery_on";
    }));
    CHECK_EQ(ag.triedMaster(), -1);
    ag.tryMaster(1);
    CHECK_EQ(ag.triedMaster(), 1);
    ag.tryMaster(-1);
    CHECK_EQ(ag.triedMaster(), -1);
    // Saved and read back by a second agent (the plugin reads the same file).
    Agent other;
    other.open(dir, "default");
    CHECK_EQ(other.profile().masterScreen.dataref, std::string("sim/cockpit2/switches/avionics_power_on"));
    CHECK_EQ(other.profile().masterLights.dataref, std::string("sim/cockpit/electrical/battery_on"));
}

TEST(brightness_link_can_be_tried_and_is_saved) {
    std::string dir = freshDir("bright");
    Agent ag;
    ag.open(dir, "default");
    CHECK(ag.change([](Profile& p) {
        p.brightLights.dataref = "sim/cockpit2/electrical/instrument_brightness_ratio[0]";
        p.brightLights.outLow = 20;
    }));
    CHECK_EQ(ag.triedBright(), -1);
    ag.tryBright(1, 0);
    CHECK_EQ(ag.triedBright(), 1);
    ag.tryBright(-1, 0);
    CHECK_EQ(ag.triedBright(), -1);
    Agent other;
    other.open(dir, "default");
    CHECK_EQ(other.profile().brightLights.outLow, 20);
    CHECK(!other.profile().brightScreen.active());
}

TEST(what_x_plane_has_on_the_buttons_can_be_imported_without_touching_anything_already_programmed) {
    std::string xp = freshDir("import_xp");
    for (const char* sub : {"/Output", "/Output/preferences", "/Output/preferences/control profiles", "/Resources", "/Resources/plugins"})
        mkdir((xp + sub).c_str(), 0755);
    {
        std::ofstream prf(xp + "/Output/preferences/X-Plane Joystick Settings.prf");
        // Stick button n is slot 7 + n: button 3 (A) = slot 10, 4 (B) = 11, 7 (D) = 14, 28 (mode dial) = 35.
        prf << "_joy_BUTN_use10 sim/FMS/CDU_popup\n_joy_BUTN_use11 sim/none/none\n"
               "_joy_BUTN_use14 sim/flight_controls/brakes_toggle_regular\n_joy_BUTN_use35 sim/flight_controls/gear_toggle\n";
        std::ofstream mp(xp + "/Output/preferences/X52Pro/xplane-map.txt");
    }
    mkdir((xp + "/Output/preferences/X52Pro").c_str(), 0755);
    {
        std::ofstream mp(xp + "/Output/preferences/X52Pro/xplane-map.txt");
        mp << "3 10\n4 11\n";
    }
    {
        std::ofstream cp(xp + "/Output/preferences/control profiles/Cesna X - Joysticks.prf");
        cp << "_joy_BUTN_use10 sim/FMS/CDU_popup\n_joy_BUTN_use11 sim/none/none\n_joy_BUTN_use35 sim/flight_controls/gear_toggle\n";
    }
    x52_start(xp.c_str(), "", "default");
    std::string list = x52_xplane_profiles();
    CHECK(has(list, "Cesna X\n") && has(list, "(settings in use)\n"));
    CHECK_EQ(std::string(x52_import_suggest("Cesna X")), std::string("Cesna X"));
    int n = x52_import_profile("Cesna X", "Cesna X");
    CHECK_EQ(n, 1);                                              // A only: B is Do nothing, 28 is the mode dial
    CHECK_EQ(std::string(x52_key()), std::string("Cesna X"));    // switched to the new profile
    CHECK_EQ(std::string(x52_step_text(3, 0, 0, 0)), std::string("sim/FMS/CDU_popup"));
    CHECK_EQ(x52_bind_exists(28, 0), 0);
    CHECK(has(slurp(xp + "/Output/preferences/X52Pro/Cesna X.x52"), "bind 3 any press \"\" cmd:sim/FMS/CDU_popup"));
    CHECK_EQ(x52_import_profile("Cesna X", "Cesna X"), -1);      // the name is taken: nothing is overwritten
    CHECK(has(x52_import_message(), "already exists"));
    CHECK_EQ(x52_import_profile("Cesna X", "default"), -1);
    CHECK_EQ(std::string(x52_import_suggest("Cesna X")), std::string("Cesna X 2"));   // the next free name
    CHECK_EQ(x52_import_profile("Cesna X", ""), -1);
    CHECK_EQ(x52_import_profile("Nobody", "Other"), -1);         // no such X-Plane profile
    CHECK_EQ(x52_import_profile("(settings in use)", "From settings"), 2);   // the main settings file: A and D
    x52_stop();
}

TEST(buttons_are_programmed_from_the_app_and_found_in_x_plane_lists) {
    std::string xp = freshDir("buttons_xp");
    mkdir((xp + "/Output").c_str(), 0755);
    mkdir((xp + "/Output/preferences").c_str(), 0755);
    mkdir((xp + "/Resources").c_str(), 0755);
    mkdir((xp + "/Resources/plugins").c_str(), 0755);
    {
        std::ofstream c(xp + "/Resources/plugins/Commands.txt");
        c << "sim/flight_controls/landing_gear_toggle   Toggle landing gear\n";
        std::ofstream d(xp + "/Resources/plugins/DataRefs.txt");
        d << "sim/cockpit2/switches/avionics_power_on\tint\ty\tboolean\tAvionics master\t\n";
    }
    x52_start(xp.c_str(), "", "default");
    CHECK_EQ(x52_catalog_size(0), 1);
    CHECK_EQ(x52_catalog_size(1), 1);
    CHECK(has(x52_catalog_search(0, "gear", 5), "sim/flight_controls/landing_gear_toggle\tToggle landing gear\t1"));
    CHECK(has(x52_catalog_search(1, "avionics", 5), "avionics_power_on\tint, boolean: Avionics master\t1"));

    CHECK_EQ(x52_bind_exists(9, 2), 0);
    int s0 = x52_step_add(9, 2, 0, 0);                                   // a command on button 9, mode 2
    CHECK_EQ(s0, 0);
    x52_step_set(9, 2, 0, 0, " sim/flight_controls/landing_gear_toggle ", 0, 0, 0, 1, 0, 0);
    CHECK_EQ(std::string(x52_step_text(9, 2, 0, 0)), std::string("sim/flight_controls/landing_gear_toggle"));   // blanks removed
    int s1 = x52_step_add(9, 2, 0, 3);                                   // then a dataref step
    CHECK_EQ(s1, 1);
    x52_step_set(9, 2, 0, 1, "sim/cockpit2/switches/avionics_power_on", 0, 2, 1, 1, 0, 1);   // add 1, between 0 and 1
    x52_bind_set_label(9, 2, 0, "Gear and power please");
    CHECK_EQ(std::string(x52_bind_label(9, 2, 0)), std::string("Gear and power p"));     // 16 characters on the screen
    CHECK_EQ(x52_step_count(9, 2, 0), 2);
    CHECK_EQ(x52_step_op(9, 2, 0, 1), 2);
    CHECK_EQ(x52_step_num(9, 2, 0, 1, 3), 1.0);
    CHECK_EQ(std::string(x52_bind_summary(9, 2)), std::string("Gear and power p"));
    CHECK_EQ(x52_bind_exists(9, 0), 0);                                  // other layers are separate
    std::string file = slurp(xp + "/Output/preferences/X52Pro/default.x52");
    CHECK(has(file, "bind 9 2 press \"Gear and power p\" cmd:sim/flight_controls/landing_gear_toggle ; add:sim/cockpit2/switches/avionics_power_on=1,0,1"));
    x52_step_add(12, 0, 0, 0);
    CHECK_EQ(std::string(x52_bound_buttons()).find("9\t") != std::string::npos, true);
    CHECK(has(x52_bound_buttons(), "9\t" + std::string(x52_button_name(9)) + "\tMode 2\t-1\t\n"));   // slot not learned yet
    {
        std::ofstream prf(xp + "/Output/preferences/X-Plane Joystick Settings.prf");
        prf << "I\n10 Version\n_joy_BUTN_use14 sim/flight_controls/brakes_toggle_regular\n_joy_BUTN_use15 sim/none/none\n";
        std::ofstream mp(xp + "/Output/preferences/X52Pro/xplane-map.txt");
        mp << "# learned\n9 14\n12 15\n";
    }
    CHECK(has(x52_bound_buttons(), "9\t" + std::string(x52_button_name(9)) + "\tMode 2\t6\tsim/flight_controls/brakes_toggle_regular\n"));
    CHECK(has(x52_bound_buttons(), "12\t" + std::string(x52_button_name(12)) + "\tAny mode\t7\tsim/none/none\n"));
    // The aircraft's own joystick settings take over from the main file (found through Miscellaneous.prf).
    mkdir((xp + "/Output/preferences/control profiles").c_str(), 0755);
    {
        std::ofstream misc(xp + "/Output/preferences/Miscellaneous.prf");
        misc << "P acf_profile Cessna___Citation___X Cesna___X\n";
        std::ofstream ac(xp + "/Output/preferences/X52Pro/xplane-aircraft.txt");
        ac << "Cessna Citation X\n";
        std::ofstream cp(xp + "/Output/preferences/control profiles/Cesna___X - Joysticks.prf");
        cp << "_joy_BUTN_use14 sim/FMS/CDU_popup\n";
    }
    CHECK_EQ(std::string(x52_assignment_source()), std::string("Cesna___X - Joysticks.prf"));
    CHECK(has(x52_bound_buttons(), "\tMode 2\t6\tsim/FMS/CDU_popup\n"));
    x52_bind_clear(12, 0);
    CHECK(!has(x52_bound_buttons(), "\n12\t") && std::string(x52_bound_buttons()).compare(0, 3, "12\t") != 0);
    x52_step_move(9, 2, 0, 1, -1);                                       // the dataref step first
    CHECK_EQ(x52_step_kind(9, 2, 0, 0), 3);
    x52_step_remove(9, 2, 0, 0);
    x52_step_remove(9, 2, 0, 0);
    x52_bind_set_label(9, 2, 0, "");
    CHECK_EQ(x52_bind_exists(9, 2), 0);                                  // nothing left: the binding is gone
    CHECK(!has(slurp(xp + "/Output/preferences/X52Pro/default.x52"), "bind 9 2"));
    CHECK_EQ(x52_step_add(28, 0, 0, 0), -1);                             // the mode dial cannot be programmed
    CHECK_EQ(x52_button_reserved(28), 1);
    x52_bind_set_label(10, 0, 1, "Long");                                // a name without a step does nothing: no binding
    CHECK_EQ(x52_bind_exists(10, 0), 0);
    CHECK_EQ(x52_step_add(10, 0, 1, 1), 0);                              // a blank keystroke step, still being typed
    CHECK_EQ(x52_bind_exists(10, 0), 1);
    CHECK_EQ(x52_step_add(10, 0, 1, 3), 1);                              // and a blank dataref step
    CHECK_EQ(x52_step_count(10, 0, 1), 2);                               // both survive the save and reload in between
    x52_bind_clear(10, 0);
    CHECK_EQ(x52_bind_exists(10, 0), 0);
    x52_stop();
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

TEST(each_throttle_dials_job_is_picked_in_the_app_and_saved) {
    std::string dir = freshDir("dial_jobs");
    x52_start("", dir.c_str(), "default");
    CHECK_EQ(x52_get("rotary_lower"), 1);   // speed brake
    CHECK_EQ(x52_get("rotary_upper"), 2);   // zoom
    x52_set("rotary_upper", 0);
    x52_set("rotary_lower", 12);            // kept inside the allowed range
    x52_set("rotary_lower_invert", 1);
    CHECK_EQ(x52_get("rotary_upper"), 0);
    CHECK_EQ(x52_get("rotary_lower"), 8);   // carb heat, the last job
    CHECK_EQ(x52_get("rotary_lower_invert"), 1);
    std::string file = slurp(dir + "/default.x52");
    CHECK(has(file, "rotary_upper = 0"));
    CHECK(has(file, "rotary_lower = 8"));
    CHECK(has(file, "rotary_lower_invert = 1"));
    CHECK_EQ(x52_get("rotary_zoom_steps"), 96);
    x52_set("rotary_zoom_steps", 5000);                      // kept inside the allowed range
    CHECK_EQ(x52_get("rotary_zoom_steps"), 480);
    CHECK(has(slurp(dir + "/default.x52"), "rotary_zoom_steps = 480"));
    // The mode names and how long the screen shows them (profile keys mode_name_N and mode_banner_ms).
    CHECK_EQ(std::string(x52_mode_name(2)), std::string("Cruise"));
    x52_set_mode_name(2, "Cruise \"x\" and a very long name");
    CHECK_EQ(std::string(x52_mode_name(2)).size(), (size_t)16);
    x52_set_mode_name(3, "Final");
    CHECK(has(slurp(dir + "/default.x52"), "mode_name_3 = Final"));
    CHECK_EQ(x52_get("mode_banner_ms"), 1500);
    x52_set("mode_banner_ms", 99999);                        // kept inside 0..10000
    CHECK_EQ(x52_get("mode_banner_ms"), 10000);
    CHECK(has(slurp(dir + "/default.x52"), "mode_banner_ms = 10000"));
    CHECK_EQ(x52_get("rotary_lower_from"), 0);
    CHECK_EQ(x52_get("rotary_lower_to"), 100);
    x52_set("rotary_lower_from", 30);
    x52_set("rotary_upper_to", 150);                         // kept inside 0..100
    CHECK(has(slurp(dir + "/default.x52"), "rotary_lower_range = 30 100"));
    CHECK_EQ(x52_get("rotary_upper_to"), 100);
    CHECK_EQ(std::string(x52_dial_use_name(5)), std::string("Rudder trim"));
    CHECK_EQ(std::string(x52_dial_use_name(99)), std::string("Nothing"));
    x52_stop();
}

TEST(the_plugin_is_switched_off_and_on_from_the_app_and_it_lasts) {
    std::string dir = freshDir("plugin_switch");
    x52_start("", dir.c_str(), "default");
    CHECK_EQ(x52_plugin_enabled(), 1);
    x52_set_plugin_enabled(0);
    CHECK_EQ(x52_plugin_enabled(), 0);
    CHECK(access((dir + "/plugin-off").c_str(), F_OK) == 0);   // the plugin reads this file
    x52_stop();
    x52_start("", dir.c_str(), "default");
    CHECK_EQ(x52_plugin_enabled(), 0);                          // still off after a restart of the app
    x52_set_plugin_enabled(1);
    CHECK_EQ(x52_plugin_enabled(), 1);
    CHECK(access((dir + "/plugin-off").c_str(), F_OK) != 0);
    x52_stop();
}

TEST(axis_and_dial_settings_are_edited_from_the_app_and_saved) {
    std::string dir = freshDir("axes_settings");
    x52_start("", dir.c_str(), "default");
    CHECK_EQ(x52_get("axis_override"), 0);
    CHECK_EQ(x52_get("dial_accel"), 1);
    x52_set("axis_override", 1);
    x52_set("axis_roll_deadzone", 12);
    x52_set("axis_pitch_expo", 40);
    x52_set("axis_yaw_invert", 1);
    x52_set("axis_throttle_deadzone", 3);
    x52_set("dial_factor", 6);
    x52_set("dial_accel", 0);
    CHECK_EQ(x52_get("axis_roll_deadzone"), 12);
    x52_set("axis_roll_deadzone", 500);   // kept inside the allowed range
    CHECK_EQ(x52_get("axis_roll_deadzone"), 40);
    x52_set("axis_roll_deadzone", 12);
    std::string file = slurp(dir + "/default.x52");
    CHECK(has(file, "axis_override = 1"));
    CHECK(has(file, "axis_roll = 12 40 0"));   // the stick's default curves: roll 40, pitch 50, yaw 30
    CHECK(has(file, "axis_pitch = 3 40 0"));
    CHECK(has(file, "axis_yaw = 3 30 1"));
    CHECK(has(file, "axis_throttle = 3 0 0"));
    CHECK(has(file, "dial_factor = 6"));
    CHECK(has(file, "dial_accel = 0"));
    CHECK_EQ(x52_get("stick_smoothing"), 60);
    CHECK_EQ(x52_get("stick_damping"), 30);
    x52_set("stick_smoothing", 90);
    x52_set("stick_damping", 500);           // kept inside the allowed range
    CHECK_EQ(x52_get("stick_damping"), 100);
    file = slurp(dir + "/default.x52");
    CHECK(has(file, "stick_smoothing = 90"));
    CHECK(has(file, "stick_damping = 100"));
    x52_stop();
}

TEST(the_app_lists_every_control_and_names_the_one_that_moves) {
    std::string dir = freshDir("axes_live");
    Agent ag;
    ag.open(dir, "default");
    ag.start();
    ag.tick();
    ag.pushAxisForTest({0x01, 0x30, 100, 0, 1023});
    ag.pushAxisForTest({0x01, 0x33, 100, 0, 255});
    ag.tick();
    ag.pushAxisForTest({0x01, 0x30, 102, 0, 1023});   // noise
    ag.tick();
    ag.pushAxisForTest({0x01, 0x33, 200, 0, 255});    // the lower rotary turned
    ag.tick();
    std::vector<Agent::AxisView> v = ag.axes();
    CHECK(v.size() >= 2);   // a real stick on this Mac adds its own controls
    int movingCount = 0;
    for (const auto& a : v)
        if (a.moving) {
            ++movingCount;
            CHECK_EQ(a.usage, 0x33);
            CHECK_EQ(a.value, 200L);
        }
    CHECK_EQ(movingCount, 1);
    ag.stop();
}

TEST(the_app_calibrates_the_stick_into_the_file_the_plugin_reads) {
    std::string dir = freshDir("calibrate");
    x52_start("", dir.c_str(), "default");
    Agent ag;
    ag.open(dir, "default");
    ag.start();
    ag.tick();
    CHECK(ag.calibration().empty());
    ag.startCalibration();
    CHECK(ag.calibrating());
    CHECK(ag.calibrationNote().find("every limit") != std::string::npos);
    // Values at the middle, so a real stick on this Mac resting there cannot upset the test.
    for (long v : {512L, 20L, 1000L, 512L}) {
        ag.pushAxisForTest({0x01, 0x30, v, 0, 1023});
        ag.pushAxisForTest({0x01, 0x31, v == 20 ? 40 : v == 1000 ? 980 : 512, 0, 1023});
        ag.tick();
    }
    // The calibration window sees how far each axis went, and where it is now.
    bool sawPitch = false;
    for (const Agent::CalibrationLive& l : ag.calibrationLive())
        if (l.usage == 0x31) {
            sawPitch = true;
            CHECK_EQ(l.lo, 40L);
            CHECK_EQ(l.hi, 980L);
            CHECK_EQ(l.max, 1023L);
        }
    CHECK(sawPitch);
    CHECK(ag.finishCalibration());
    CHECK(!ag.calibrating());
    CHECK_EQ(ag.calibration().size(), (size_t)2);   // the twist was not moved: it keeps its full range
    CHECK(ag.calibrationNote().find("twist keeps") != std::string::npos);
    // The plugin's file, in the plugin's format.
    std::map<int, AxisCalibration> saved = loadCalibration(dir);
    CHECK_EQ(saved[0x31].min, 40L);
    CHECK_EQ(saved[0x31].centre, 512L);
    CHECK_EQ(saved[0x31].max, 980L);
    CHECK(has(describeCalibration(saved), "Stick fwd/back: 40 .. 512 .. 980"));
    // A calibration made elsewhere (the plugin) shows up here too.
    Agent other;
    other.open(dir, "default");
    CHECK_EQ(other.calibration().size(), (size_t)2);
    // Done without moving the stick is refused, and calibration goes on.
    ag.startCalibration();
    CHECK(!ag.finishCalibration());
    CHECK(ag.calibrating());
    CHECK(ag.calibrationNote().find("all the way") != std::string::npos);
    ag.cancelCalibration();
    CHECK(!ag.calibrating());
    CHECK_EQ(ag.calibration().size(), (size_t)2);   // the old one is kept
    ag.resetCalibration();
    CHECK(ag.calibration().empty());
    CHECK(!std::ifstream(calibrationPath(dir)).good());
    ag.stop();
    // The app's bridge: status line first, then the measured axes.
    CHECK(has(x52_calibration(), "Not calibrated"));
    x52_stop();
}

TEST(the_pov_1_hat_switch_is_listed_as_held_while_it_is_pushed) {
    std::string dir = freshDir("hat_held");
    Agent ag;
    ag.open(dir, "default");
    ag.start();
    ag.pushAxisForTest({0x01, 0x39, 0, 1, 8});   // centred
    ag.tick();
    CHECK(ag.held().find("POV 1") == std::string::npos);
    ag.pushAxisForTest({0x01, 0x39, 4, 1, 8});   // down-right
    ag.tick();
    CHECK(ag.held().find("POV 1 (hat switch): down-right") != std::string::npos);
    ag.stop();
}

TEST(user_pages_are_edited_from_the_app_and_saved) {
    std::string xp = freshDir("pages_xp");
    mkdir((xp + "/Output").c_str(), 0755);
    mkdir((xp + "/Output/preferences").c_str(), 0755);
    x52_start(xp.c_str(), "", "default");
    x52_pages_restore_default();
    CHECK_EQ(x52_page_count(), 0);

    int fuel = x52_page_add("Fuel & weight", "");
    CHECK_EQ(fuel, 0);
    CHECK_EQ(std::string(x52_page_id(fuel)), std::string("fuel-weight"));
    int fuel2 = x52_page_add("Fuel & \"weight\"", "");                    // same id once more: made unique
    CHECK_EQ(std::string(x52_page_id(fuel2)), std::string("fuel-weight-2"));
    CHECK_EQ(std::string(x52_page_title(fuel2)), std::string("Fuel & 'weight'"));
    CHECK_EQ(std::string(x52_page_id(x52_page_add("", ""))), std::string("page"));
    int tanks = x52_page_add("Tanks", "fuel-weight");                       // a sub-page
    CHECK_EQ(std::string(x52_page_parent(tanks)), std::string("fuel-weight"));
    int left = x52_page_add("Left tank", "tanks");                          // a sub-page of the sub-page
    CHECK_EQ(std::string(x52_pages_outline()), std::string("0\t0\n3\t1\n4\t2\n1\t0\n2\t0\n"));

    // A page cannot go inside itself or one of its own sub-pages, nor inside a page that does not exist.
    CHECK_EQ(x52_page_set(fuel, "Fuel", "left-tank"), 0);
    CHECK_EQ(x52_page_set(fuel, "Fuel", "nowhere"), 0);
    CHECK_EQ(std::string(x52_page_title(fuel)), std::string("Fuel & weight"));
    CHECK_EQ(x52_page_set(fuel, "Fuel", ""), 1);
    CHECK_EQ(std::string(x52_page_title(fuel)), std::string("Fuel"));
    CHECK_EQ(std::string(x52_page_id(fuel)), std::string("fuel-weight"));  // the id stays: links keep working

    // Rows: blank ones, still being filled in, survive the save and reload of every change.
    CHECK_EQ(x52_row_add(fuel, 1), 0);
    CHECK_EQ(x52_row_add(fuel, 2), 1);
    CHECK_EQ(x52_row_add(fuel, 0), 2);
    CHECK_EQ(x52_row_count(fuel), 3);
    CHECK_EQ(x52_row_kind(fuel, 1), 2);
    x52_row_set(fuel, 0, 1, "Fuel", "%.0f kg", " sim/flightmodel/weight/m_fuel_total ", "");
    CHECK_EQ(std::string(x52_row_dataref(fuel, 0)), std::string("sim/flightmodel/weight/m_fuel_total"));
    x52_row_set(fuel, 1, 2, "Tanks", "", "", "tanks");
    x52_row_set(fuel, 2, 2, "Nowhere", "", "", "no-such-page");             // a link only to a page that exists
    CHECK_EQ(std::string(x52_row_target(fuel, 2)), std::string(""));
    x52_row_set(fuel, 2, 0, "Hello", "", "", "");
    x52_row_move(fuel, 2, -1);
    CHECK_EQ(std::string(x52_row_text(fuel, 1)), std::string("Hello"));
    CHECK_EQ(std::string(x52_row_target(fuel, 2)), std::string("tanks"));
    x52_row_add(left, 1);                                                   // a blank value row on the sub-sub-page

    // The preview is what the stick shows; without X-Plane a made-up number stands in.
    CHECK_EQ(std::string(x52_page_preview(fuel, 0)), std::string(">Fuel    1234 kg"));
    CHECK_EQ(std::string(x52_page_preview(fuel, 2)), std::string(" Tanks         >"));
    CHECK_EQ(x52_page_preview_live(fuel), 0);
    CHECK_EQ(std::string(x52_page_preview(tanks, 0)), std::string(">< Back"));   // opened through its parent
    CHECK_EQ(std::string(x52_format_example("%.1f kts", 12.34)), std::string("12.3 kts"));

    // Moving: top-level pages pass each other, skipping sub-pages.
    x52_page_move(fuel2, -1);
    CHECK_EQ(std::string(x52_page_id(0)), std::string("fuel-weight-2"));
    CHECK_EQ(std::string(x52_page_id(1)), std::string("fuel-weight"));

    // What is saved reads back as the same tree.
    std::string file = slurp(xp + "/Output/preferences/X52Pro/default.x52");
    CHECK(has(file, "page tanks \"Tanks\" in fuel-weight\n"));
    CHECK(has(file, "row fuel-weight value \"Fuel\" \"%.0f kg\" sim/flightmodel/weight/m_fuel_total\n"));
    CHECK(has(file, "row left-tank value \"\" \"%g\" -\n"));
    Profile back;
    std::string err;
    CHECK(parse(file, back, &err));
    CHECK_EQ(back.pages.pages.size(), (size_t)5);
    CHECK_EQ(serializePages(back.pages), std::string(x52_pages_source()));

    // Removing a page takes its sub-pages with it and clears the links that opened them.
    x52_page_remove(x52_page_count() > 0 ? 3 : -1);                       // "tanks" (and "left-tank" inside it)
    CHECK_EQ(x52_page_count(), 3);
    CHECK_EQ(std::string(x52_row_target(1, 2)), std::string(""));
    CHECK_EQ(x52_row_kind(1, 2), 2);                                        // the row stays, pointing nowhere
    CHECK(!has(slurp(xp + "/Output/preferences/X52Pro/default.x52"), "tanks"));

    x52_pages_restore_default();
    CHECK_EQ(x52_page_count(), 0);
    CHECK(!has(slurp(xp + "/Output/preferences/X52Pro/default.x52"), "\npage "));
    x52_stop();
}

TEST(the_left_wheels_order_is_edited_from_the_app_and_saved) {
    std::string xp = freshDir("wheel_xp");
    mkdir((xp + "/Output").c_str(), 0755);
    mkdir((xp + "/Output/preferences").c_str(), 0755);
    x52_start(xp.c_str(), "", "default");
    x52_pages_restore_default();
    std::string file = xp + "/Output/preferences/X52Pro/default.x52";
    int r = x52_page_add("Radio", "");
    std::string entries = x52_wheel_entries();
    CHECK(entries.compare(0, 20, "com1\tCOM1 / NAV1\t0\t1") == 0);
    CHECK(has(entries, "@radio\tRadio\t0\t0\n"));
    CHECK(!has(slurp(file), "wheel_"));                       // the usual order is not written down
    x52_wheel_move("@radio", -1);                             // before VOR / HDG
    x52_wheel_hide("adf", 1);
    CHECK(has(x52_wheel_entries(), "adf\tADF\t1\t1\n"));
    CHECK(has(x52_wheel_entries(), "@radio\tRadio\t0\t0\nvorhdg\t"));
    CHECK(has(slurp(file), "wheel_hidden = adf\n"));
    CHECK(has(slurp(file), "wheel_order = com1 com2 adf dme xpdr ap @radio vorhdg buttons brightness\n"));
    x52_wheel_hide("adf", 0);
    CHECK(!has(slurp(file), "wheel_hidden"));
    CHECK(std::string(x52_builtin_preview("com1", 0)).size() > 0);
    CHECK_EQ(std::string(x52_builtin_preview("nonsense", 0)), std::string(""));
    x52_page_remove(r);                                       // its place in the order goes with it
    CHECK(!has(slurp(file), "@radio"));
    x52_wheel_hide("dme", 1);
    x52_pages_restore_default();                              // everything back: order and hidden pages too
    CHECK(!has(slurp(file), "wheel_"));
    x52_stop();
}

TEST(the_pages_editor_sets_the_radio_layout_and_a_mode_order) {
    std::string xp = freshDir("mode_wheel_xp");
    mkdir((xp + "/Output").c_str(), 0755);
    mkdir((xp + "/Output/preferences").c_str(), 0755);
    x52_start(xp.c_str(), "", "default");
    x52_pages_restore_default();
    std::string file = xp + "/Output/preferences/X52Pro/default.x52";
    x52_set("radio_side", 1);
    CHECK_EQ(x52_get("radio_side"), 1);
    CHECK(has(slurp(file), "radio_layout = side\n"));
    CHECK(has(x52_wheel_entries(), "com1\tCOM 1+2\t0\t1\n"));
    CHECK(has(x52_builtin_preview("com1", 0), "[COM1]"));
    x52_set("radio_side", 0);
    CHECK(!has(slurp(file), "radio_layout"));
    x52_wheel_edit_mode(2);
    CHECK_EQ(x52_wheel_mode_own(2), 0);
    x52_wheel_move("ap", -1);                                  // makes mode 2's order its own
    CHECK_EQ(x52_wheel_mode_own(2), 1);
    CHECK(has(slurp(file), "wheel_order.2 = com1 com2 adf dme ap xpdr"));
    CHECK(!has(slurp(file), "wheel_order ="));                 // the shared order is untouched
    CHECK(has(x52_wheel_entries(), "ap\tAutopilot\t0\t1\nxpdr\t"));
    int r = x52_page_add("Radio", "");
    x52_wheel_hide("@radio", 1);
    CHECK(has(slurp(file), "wheel_hidden.2 = @radio\n"));
    x52_page_remove(r);
    CHECK(!has(slurp(file), "@radio"));                        // gone from mode 2's lists too
    x52_wheel_edit_mode(0);
    CHECK(has(x52_wheel_entries(), "xpdr\tTransponder\t0\t1\nap\t"));   // every mode: the usual order
    x52_wheel_set_mode_own(3, 1);
    CHECK(has(slurp(file), "wheel_order.3 = com1"));
    x52_pages_restore_default();
    CHECK(!has(slurp(file), "wheel_"));
    x52_wheel_set_mode_own(2, 1);
    x52_wheel_set_mode_own(2, 0);
    CHECK(!has(slurp(file), "wheel_order.2"));
    x52_stop();
}

TEST(a_change_to_a_ready_made_profile_is_saved_as_a_new_profile) {
    std::string dir = freshDir("standard");
    {
        Profile p = defaultProfile();
        p.name = "Cirrus SR22";
        p.standard = true;
        Store s;
        s.dir = dir;
        std::ofstream f(dir + "/aircraft-Cirrus SR22.x52");
        f << serialize(p);
    }
    std::string before = slurp(dir + "/aircraft-Cirrus SR22.x52");
    Agent a;
    a.open(dir, "aircraft-Cirrus SR22");
    CHECK(a.standard());
    CHECK(!a.change([](Profile& p) { p.mfdBrightness = 50; }));   // waits for a name
    CHECK(!a.change([](Profile& p) { p.ledBrightness = 60; }));
    CHECK(a.copyNeeded());
    CHECK_EQ(slurp(dir + "/aircraft-Cirrus SR22.x52"), before);
    CHECK_EQ(a.suggestCopyName(), std::string("Cirrus SR22 mine"));
    std::string err;
    CHECK(!a.saveCopy("  ", &err));
    CHECK(has(err, "Type a name"));
    CHECK(!a.saveCopy("aircraft-Cirrus SR22", &err));          // taken
    CHECK(!a.saveCopy("Mine/bad", &err));                      // not a file name
    CHECK(a.saveCopy("My SR22", &err));
    CHECK_EQ(a.key(), std::string("My SR22"));
    CHECK(!a.copyNeeded());
    CHECK(!a.standard());
    CHECK_EQ(a.profile().mfdBrightness, 50);                   // both waiting changes are in the copy
    CHECK_EQ(a.profile().ledBrightness, 60);
    CHECK_EQ(a.profile().name, std::string("My SR22"));
    CHECK(has(slurp(dir + "/My SR22.x52"), "based_on = aircraft-Cirrus SR22\n"));
    CHECK(!has(slurp(dir + "/My SR22.x52"), "standard"));
    CHECK(has(slurp(dir + "/profile-choices.txt"), "aircraft-Cirrus SR22\tMy SR22\n"));   // X-Plane uses it now
    CHECK(a.change([](Profile& p) { p.mfdBrightness = 70; }));  // my own profile saves as usual
    CHECK_EQ(slurp(dir + "/aircraft-Cirrus SR22.x52"), before);
    a.select("aircraft-Cirrus SR22");                          // back to the ready-made one
    CHECK(!has(slurp(dir + "/profile-choices.txt"), "My SR22"));
    CHECK(!a.change([](Profile& p) { p.mfdBrightness = 1; }));
    a.cancelCopy();                                            // the change is dropped
    CHECK(!a.copyNeeded());
    CHECK_EQ(slurp(dir + "/aircraft-Cirrus SR22.x52"), before);
}

TEST(clock_1_can_be_set_from_the_app) {
    std::string dir = freshDir("clock1");
    x52_start("", dir.c_str(), "default");
    CHECK_EQ(x52_get("clock1_source"), 0);
    x52_set("clock1_source", 3);
    x52_set("clock1_offset", -300);
    x52_set("clock1_source", 9);   // kept inside the allowed range
    CHECK_EQ(x52_get("clock1_source"), 3);
    std::string file = slurp(dir + "/default.x52");
    CHECK(has(file, "clock1_source = 3\n"));
    CHECK(has(file, "clock1_offset = -300\n"));
    x52_stop();
}

TEST(a_profile_of_mine_can_be_deleted_and_brought_back_but_never_default_or_a_ready_made_one) {
    std::string dir = freshDir("delete");
    {
        Profile p = defaultProfile();
        p.name = "Cirrus SR22";
        p.standard = true;
        std::ofstream f(dir + "/aircraft-Cirrus SR22.x52");
        f << serialize(p);
    }
    Agent a;
    a.open(dir, "aircraft-Cirrus SR22");
    std::string err;
    CHECK(!a.deleteProfile("aircraft-Cirrus SR22", &err));   // ready-made
    CHECK(has(err, "ready-made"));
    CHECK(!a.deleteProfile("default", &err));
    CHECK(!a.deleteProfile("nothing here", &err));
    a.askCopy();
    CHECK(a.saveCopy("My SR22", &err));
    CHECK(has(slurp(dir + "/profile-choices.txt"), "aircraft-Cirrus SR22\tMy SR22\n"));
    CHECK(a.deleteProfile("My SR22", &err));                  // the one in use
    CHECK_EQ(a.key(), std::string("aircraft-Cirrus SR22"));   // back to the profile it came from
    CHECK(!has(slurp(dir + "/profile-choices.txt"), "My SR22"));   // X-Plane uses the ready-made one again
    std::vector<std::string> keys = a.profiles();
    CHECK(std::find(keys.begin(), keys.end(), "My SR22") == keys.end());
    CHECK(a.restoreProfile("My SR22"));                       // undo
    keys = a.profiles();
    CHECK(std::find(keys.begin(), keys.end(), "My SR22") != keys.end());
    CHECK_EQ(a.key(), std::string("My SR22"));
}

TEST(a_blank_profile_has_nothing_programmed_but_keeps_the_axes_and_screen_controls) {
    std::string dir = freshDir("blank");
    Agent a;
    a.open(dir, "default");
    CHECK(a.change([](Profile& p) {
        p.axes.overrideXPlane = true;
        p.axes.pluginThrottle = true;
        p.edit(1, kLayerAny).press.push_back({StepKind::Command, "sim/gear_toggle", 0});
        p.rules.push_back(LightRule());
        p.alarms.push_back(Alarm());
        p.ledAt(1, false, LedA) = Color::Red;
    }));
    std::string err;
    CHECK(!a.createBlank("  ", &err));
    CHECK(!a.createBlank("default", &err));
    CHECK(a.createBlank("Scratch", &err));
    CHECK_EQ(a.key(), std::string("Scratch"));
    const Profile& p = a.profile();
    CHECK_EQ(p.name, std::string("Scratch"));
    CHECK(p.exact(1, kLayerAny) == nullptr);
    CHECK(p.exact(37, kLayerAny) != nullptr);                // screen cursor on the wheel
    CHECK(p.rules.empty());
    CHECK(p.alarms.empty());
    CHECK(p.ledAt(1, false, LedA) == Color::Green);
    CHECK(p.axes.overrideXPlane);                            // the aircraft still flies
    CHECK(p.axes.pluginThrottle);
    CHECK(!p.standard);
    CHECK(p.basedOn.empty());
}

TEST(undo_and_redo_step_through_the_edits_of_the_profile_in_use) {
    std::string dir = freshDir("undo");
    Agent a;
    a.open(dir, "default");
    CHECK(!a.canUndo() && !a.canRedo());
    int original = a.profile().ledBrightness;
    CHECK(a.change([](Profile& p) { p.ledBrightness = 11; }));
    CHECK(a.change([](Profile& p) { p.ledBrightness = 22; }));   // right after: the same step
    CHECK(a.canUndo() && !a.canRedo());
    CHECK(a.undo());
    CHECK_EQ(a.profile().ledBrightness, original);
    CHECK(has(slurp(dir + "/default.x52"), "led_brightness = " + std::to_string(original)));   // written to the file too
    CHECK(!a.canUndo() && a.canRedo());
    CHECK(a.redo());
    CHECK_EQ(a.profile().ledBrightness, 22);
    CHECK(a.undo());
    CHECK(a.change([](Profile& p) { p.mfdBrightness = 33; }));   // a new edit drops the redo
    CHECK(!a.canRedo());
    {
        Store st;
        st.dir = dir;
        std::string e;
        CHECK(st.save("other", defaultProfile(), &e));
    }
    a.select("other");                                           // another profile: its own, empty history
    CHECK_EQ(a.key(), std::string("other"));
    CHECK(!a.canUndo());
}

TEST(a_ready_made_profile_has_nothing_to_undo) {
    std::string dir = freshDir("undo_standard");
    Profile p = defaultProfile();
    p.name = "Std";
    p.standard = true;
    {
        std::ofstream f(dir + "/aircraft-Std.x52");
        f << serialize(p);
    }
    Agent a;
    a.open(dir, "aircraft-Std");
    a.change([](Profile& q) { q.ledBrightness = 5; });
    CHECK(!a.canUndo() && !a.canRedo());
    CHECK(!a.undo());
}
