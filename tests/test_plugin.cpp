// Runs the real plugin against a fake X-Plane: opens the window, clicks and types in
// it, presses stick buttons, and checks the commands sent and the profile file written.
// The tests share one plugin instance and run in order, like one session in the sim.
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

#include "XPLMDefs.h"
#include "XPLMPlugin.h"
#include "XPLMUtilities.h"
#include "check.h"
#include "fake_xplm.h"
#include "../src/plugin/app.h"

extern "C" {
int XPluginStart(char*, char*, char*);
int XPluginEnable(void);
void XPluginDisable(void);
void XPluginStop(void);
void XPluginReceiveMessage(XPLMPluginID, int, void*);
}

using namespace x52;

static const char* kGear = "sim/flight_controls/landing_gear_toggle";

static std::string slurp(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
static bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }
static std::string defaultFile() { return sim().root + "/Output/preferences/X52Pro/default.x52"; }
static std::string aircraftFile() { return sim().root + "/Output/preferences/X52Pro/aircraft-Cessna_172SP.x52"; }
static void press(int button, bool down) {
    gApp->input.push(button, down);
    sim().frame();
}
static void turnDial(int from, int to) {
    gApp->input.push(from, false);
    gApp->input.push(to, true);
    sim().frame();
    std::this_thread::sleep_for(std::chrono::milliseconds(130));   // let the dial settle
    sim().frame();
}

TEST(starting_writes_a_default_profile) {
    char name[256], sig[256], desc[256];
    CHECK_EQ(XPluginStart(name, sig, desc), 1);
    CHECK_EQ(XPluginEnable(), 1);
    gApp->askedKeyAccess = true;   // never raise the macOS permission prompt from a test run
    CHECK_EQ(std::string(name), std::string("X52 Pro"));
    CHECK(has(slurp(defaultFile()), "cmd:x52pro/mfd_page_next"));
    CHECK_EQ(gApp->commands.size(), (size_t)10);   // 3 from Commands.txt + 7 of our own
}

TEST(window_opens_from_its_command_and_lists_the_buttons) {
    CHECK(!sim().windowVisible);
    XPLMCommandOnce(XPLMFindCommand("x52pro/toggle_window"));
    CHECK(sim().windowVisible);
    sim().frame();
    CHECK(sim().sees("Trigger"));
    CHECK(sim().sees("Profile: Default (all aircraft)"));
    CHECK(sim().sees("mode switch") == false);   // rows 28-30 are below the visible part of the list
}

TEST(page_wheel_command_changes_the_screen_page) {
    CHECK_EQ(gApp->page, (int)PageButtons);
    XPLMCommandOnce(XPLMFindCommand("x52pro/mfd_page_next"));
    CHECK_EQ(gApp->page, (int)PageComNav1);
    XPLMCommandOnce(XPLMFindCommand("x52pro/mfd_page_prev"));
    CHECK_EQ(gApp->page, (int)PageButtons);
}

TEST(clicking_a_button_row_selects_it) {
    CHECK(sim().clickText("Fire"));
    CHECK(sim().sees("2  Fire   -   Any mode"));
}

TEST(adding_a_wait_step_is_saved_at_once) {
    CHECK(sim().clickText("+ Wait"));
    CHECK(has(slurp(defaultFile()), "bind 2 any press \"\" wait:200"));
    CHECK(sim().sees("200 ms"));
}

TEST(command_picker_filters_as_you_type_and_adds_the_command) {
    CHECK(sim().clickText("+ Command"));
    CHECK(sim().sees("Choose a command for Fire"));
    CHECK(sim().hasKeyboardFocus);
    CHECK(sim().sees("sim/flight_controls/flaps_down"));
    sim().type("gear");
    CHECK(sim().sees(kGear));
    CHECK(!sim().sees("sim/flight_controls/flaps_down"));
    CHECK(sim().clickText(kGear));
    CHECK(!sim().hasKeyboardFocus);
    CHECK(has(slurp(defaultFile()), std::string("bind 2 any press \"\" wait:200 ; cmd:") + kGear));
}

TEST(pressing_the_stick_button_runs_the_macro_after_its_wait) {
    press(2, true);
    CHECK_EQ(sim().onces[kGear], 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(260));
    sim().frame();
    CHECK_EQ(sim().onces[kGear], 1);
    press(2, false);
    CHECK_EQ(sim().onces[kGear], 1);
}

TEST(pressed_button_is_followed_in_the_window) {
    press(4, true);
    press(4, false);
    CHECK(sim().sees("4  B   -   Any mode"));
    CHECK(sim().clickText("Fire"));
}

TEST(removing_the_wait_leaves_a_command_that_follows_the_button) {
    CHECK(sim().clickText("x", 0, true));
    CHECK(has(slurp(defaultFile()), std::string("bind 2 any press \"\" cmd:") + kGear));
    press(2, true);
    CHECK_EQ(sim().begins[kGear], 1);
    CHECK_EQ(sim().ends[kGear], 0);
    press(2, false);
    CHECK_EQ(sim().ends[kGear], 1);
}

TEST(typing_a_name_stores_it_as_the_screen_label) {
    CHECK(sim().clickText("up to 16 characters"));
    sim().type("gear");
    CHECK(has(slurp(defaultFile()), "bind 2 any press \"gear\""));
    sim().key(0, XPLM_VK_RETURN);
    CHECK(!sim().hasKeyboardFocus);
    press(2, true);
    CHECK_EQ(gApp->label, std::string("gear"));
    press(2, false);
}

TEST(a_mode_layer_starts_empty_and_says_any_mode_applies) {
    CHECK(sim().clickText("Mode 2", 0, true));
    CHECK(sim().sees("2  Fire   -   Mode 2"));
    CHECK(sim().sees("so 'Any mode' applies: gear"));
}

TEST(recording_a_key_stores_the_combo_for_that_mode_only) {
    CHECK(sim().clickText("+ Keystroke"));
    CHECK(sim().clickText("Record", 0, true));
    CHECK(sim().sees("Press a key"));
    sim().key('G', XPLM_VK_G, xplm_ShiftFlag);
    std::string file = slurp(defaultFile());
    CHECK(has(file, "bind 2 2 press \"\" key:shift+g"));
    CHECK(has(file, std::string("bind 2 any press \"gear\" cmd:") + kGear));
    CHECK(!sim().sees("Press a key"));
}

TEST(mode_dial_switches_which_assignment_runs) {
    int before = sim().begins[kGear];
    gApp->input.push(28, true);   // the stick reports the dial's position when attached
    turnDial(28, 29);
    CHECK_EQ(gApp->engine.mode(), 2);
    press(2, true);
    press(2, false);
    CHECK_EQ(sim().begins[kGear], before);   // mode 2 has the keystroke instead
    turnDial(29, 28);
    CHECK_EQ(gApp->engine.mode(), 1);
    press(2, true);
    press(2, false);
    CHECK_EQ(sim().begins[kGear], before + 1);
}

TEST(light_colour_is_saved_per_mode) {
    CHECK(sim().clickText("Lights", 0, true));
    CHECK(sim().sees("Button A"));
    CHECK(sim().clickText("Mode 3", 0, true));
    CHECK(sim().clickText("Red", 0, true));   // first two-colour light: Button A
    std::string file = slurp(defaultFile());
    CHECK(has(file, "leds 3 = fire:green a:red b:green"));
    CHECK(has(file, "leds 1 = fire:green a:green"));
}

TEST(settings_toggle_frees_the_pinkie) {
    CHECK(sim().clickText("Settings", 0, true));
    CHECK(gApp->profile.isReserved(6));
    CHECK(sim().clickText("[x]", 0, true));
    CHECK(!gApp->profile.isReserved(6));
    CHECK(has(slurp(defaultFile()), "pinkie_shift = 0"));
    CHECK(sim().clickText("[  ]", 0, true));
    CHECK(has(slurp(defaultFile()), "pinkie_shift = 1"));
}

TEST(aircraft_profile_starts_as_a_copy_and_gets_its_own_file) {
    CHECK(sim().clickText("Make a profile for Cessna_172SP"));
    CHECK(gApp->usingAircraftProfile);
    std::string file = slurp(aircraftFile());
    CHECK(has(file, "name = Cessna_172SP"));
    CHECK(has(file, kGear));
    CHECK(sim().sees("Profile: Cessna_172SP"));
}

TEST(aircraft_profile_is_picked_up_again_when_that_aircraft_loads) {
    sim().aircraftFile = "Baron_58.acf";
    XPluginReceiveMessage(0, XPLM_MSG_PLANE_LOADED, nullptr);
    CHECK(!gApp->usingAircraftProfile);
    CHECK_EQ(gApp->profile.name, std::string("default"));
    sim().aircraftFile = "Cessna_172SP.acf";
    XPluginReceiveMessage(0, XPLM_MSG_PLANE_LOADED, nullptr);
    CHECK(gApp->usingAircraftProfile);
    CHECK_EQ(gApp->profile.name, std::string("Cessna_172SP"));
}

TEST(a_profile_file_with_a_mistake_is_never_overwritten) {
    {
        std::ofstream f(aircraftFile(), std::ios::trunc);
        f << "x52pro-profile 1\nbind banana any press \"x\" cmd:y\n";
    }
    gApp->reload();
    sim().frame();
    CHECK(sim().sees("Profile file has a mistake (line 2"));
    CHECK(sim().clickText("Buttons", 0, true));
    CHECK(sim().clickText("Trigger", 0, true));
    CHECK(sim().clickText("+ Wait"));
    CHECK(has(slurp(aircraftFile()), "bind banana"));
    CHECK(sim().sees("Not saved"));
}

TEST(unknown_command_is_reported_not_crashed_on) {
    unlink(aircraftFile().c_str());
    gApp->loadForAircraft("Cessna_172SP");
    Binding& b = gApp->profile.edit(3, kLayerAny);
    b.press.push_back({StepKind::Command, "addon/not/loaded", 0});
    gApp->profileChanged();
    press(3, true);
    press(3, false);
    CHECK(has(gApp->notice, "Unknown command: addon/not/loaded"));
}

TEST(test_mode_names_buttons_on_the_screen_without_running_them) {
    Binding& b = gApp->profile.edit(4, kLayerAny);
    b.press.push_back({StepKind::Command, kGear, 0});
    gApp->profileChanged();
    int before = sim().begins[kGear];
    XPLMCommandOnce(XPLMFindCommand("x52pro/toggle_test"));
    CHECK(gApp->testMode);
    sim().frame();
    CHECK_EQ(gApp->screen.l[0], std::string("TEST MODE"));
    press(4, true);
    CHECK_EQ(sim().begins[kGear], before);
    CHECK_EQ(gApp->screen.l[0], std::string("Button 4"));
    CHECK_EQ(gApp->screen.l[1], std::string("B"));
    press(4, false);
}

TEST(test_mode_names_moving_controls_with_their_position) {
    gApp->input.pushAxis({0x01, 0x32, 0, 0, 255});     // where the throttle rests
    sim().frame();
    CHECK_EQ(gApp->screen.l[0], std::string("Button 4"));   // resting position is not news
    gApp->input.pushAxis({0x01, 0x32, 128, 0, 255});
    sim().frame();
    CHECK_EQ(gApp->screen.l[0], std::string("Throttle"));
    CHECK_EQ(gApp->screen.l[1], std::string("50 %"));
    gApp->input.pushAxis({0x01, 0x34, 10, 0, 255});
    gApp->input.pushAxis({0x01, 0x34, 255, 0, 255});
    sim().frame();
    CHECK_EQ(gApp->screen.l[0], std::string("Upper rotary (E)"));
    CHECK_EQ(gApp->screen.l[1], std::string("100 %"));
}

TEST(window_mirrors_what_the_stick_screen_shows) {
    CHECK(sim().clickText("Settings", 0, true));
    CHECK(sim().sees("Upper rotary (E)"));
    CHECK(sim().sees("100 %"));
}

TEST(leaving_test_mode_gives_the_buttons_back) {
    int before = sim().begins[kGear];
    CHECK(sim().clickText("[x]", 2, true));   // third checkbox on the Settings tab: test mode
    CHECK(!gApp->testMode);
    press(4, true);
    press(4, false);
    CHECK_EQ(sim().begins[kGear], before + 1);
    CHECK(sim().clickText("Buttons", 0, true));
}

TEST(right_wheel_and_its_button_tune_the_radio_from_the_stick) {
    sim().knownCommands.push_back("sim/radios/com1_standy_flip");
    sim().knownCommands.push_back("sim/radios/stby_com1_coarse_up");
    gApp->setPage(PageComNav1);
    press(37, true);   // turning alone only moves the cursor
    press(37, false);
    press(38, true);
    press(38, false);
    CHECK_EQ(sim().onces["sim/radios/stby_com1_coarse_up"], 0);
    press(39, true);   // on the active frequency: swap
    press(39, false);
    CHECK_EQ(sim().onces["sim/radios/com1_standy_flip"], 1);
    press(38, true);   // down to the standby's whole number
    press(38, false);
    press(39, true);
    press(39, false);
    CHECK_EQ(gApp->screen.l[1], std::string("STBY>>  0.000"));
    press(37, true);
    press(37, false);
    CHECK_EQ(sim().onces["sim/radios/stby_com1_coarse_up"], 1);
    XPLMCommandOnce(XPLMFindCommand("x52pro/mfd_page_next"));
    CHECK(!gApp->cursor.editing);   // a new page never starts in editing
    gApp->setPage(PageButtons);
}

TEST(clutch_flashes_and_the_pov_hat_picks_a_profile) {
    {
        Profile other;
        other.name = "combat";
        std::string err;
        CHECK(gApp->store.save("combat", other, &err));
    }
    auto hat = [](long v) {
        gApp->input.pushAxis({0x01, 0x39, v, 1, 8});
        sim().frame();
    };
    hat(5);   // without the clutch the hat is X-Plane's
    CHECK_EQ(gApp->pickIndex, -1);
    hat(0);
    press(31, true);
    CHECK(gApp->engine.clutch());
    hat(5);
    CHECK_EQ(gApp->screen.l[0], std::string("PICK PROFILE"));
    for (int i = 0; i < 4 && gApp->screen.l[1] != "combat"; ++i) {
        hat(0);
        hat(5);
    }
    CHECK_EQ(gApp->screen.l[1], std::string("combat"));
    hat(0);
    hat(3);   // right: use it
    CHECK_EQ(gApp->profile.name, std::string("combat"));
    CHECK_EQ(gApp->profileKey(), std::string("combat"));
    hat(0);
    hat(7);   // left: back to automatic
    CHECK_EQ(gApp->profileKey(), std::string("default"));
    press(31, false);
    CHECK(!gApp->engine.clutch());
    hat(0);
}

TEST(a_profile_changed_outside_x_plane_is_picked_up) {
    CHECK(gApp->lock.held());
    CHECK(StickLock::heldByOther(gApp->store.dir));
    Profile p;
    std::string err;
    CHECK(gApp->store.load("default", p, &err));
    p.leds[0][LedA] = Color::Red;
    p.clockOffset[0] = 60;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK(gApp->store.save("default", p, &err));
    gApp->nextSlowCheck = 0;
    sim().frame();
    CHECK(gApp->profile.leds[0][LedA] == Color::Red);
    CHECK_EQ(gApp->profile.clockOffset[0], 60);
    long long stamp = gApp->loadedStamp;
    gApp->nextSlowCheck = 0;
    sim().frame();
    CHECK_EQ(gApp->loadedStamp, stamp);   // and not reloaded again
}

TEST(a_profile_from_the_older_plugin_gets_the_wheel_bindings) {
    {
        std::ofstream f(defaultFile(), std::ios::trunc);
        f << "x52pro-profile 1\nname = default\nbind 3 any press \"Gear\" cmd:" << kGear << "\n";
    }
    gApp->reload();
    CHECK(has(slurp(defaultFile()), "cmd:x52pro/mfd_select"));
    CHECK(has(slurp(defaultFile()), "x52pro-profile 2"));
    CHECK(has(slurp(defaultFile()), std::string("cmd:") + kGear));
}

TEST(live_lights_tab_adds_opens_edits_and_removes_rules) {
    CHECK(sim().clickText("Live lights", 0, true));
    CHECK(sim().sees("No live lights yet. Press Add rule."));
    CHECK(sim().clickText("Add rule", 0, true));
    CHECK_EQ(gApp->profile.rules.size(), (size_t)1);
    CHECK(sim().sees("Button E"));                      // the new rule's header
    CHECK(sim().clickText("Parking brake: red on, green off", 0, true));   // it opened when added
    CHECK(has(slurp(defaultFile()), "rule e > 0.5 red green sim/cockpit2/controls/parking_brake_ratio"));
    CHECK(sim().clickText(">", 0, true));               // the comparison button opens a list
    CHECK(sim().sees("!="));
    CHECK(sim().clickText(">=", 0, true));
    CHECK(has(slurp(defaultFile()), "rule e >= 0.5 red green"));
    CHECK(!sim().sees("!="));                           // and it closed again
    CHECK(sim().clickText("Add rule", 0, true));        // a second rule, the first stays
    CHECK_EQ(gApp->profile.rules.size(), (size_t)2);
    CHECK(has(slurp(defaultFile()), "rule e >= 0.5 red green sim/cockpit2/controls/parking_brake_ratio"));
    CHECK(has(slurp(defaultFile()), "rule e > 0.5 red green -"));
    CHECK(sim().clickText("[  ]", 0, true));            // tick the first rule
    CHECK(sim().clickText("Remove selected", 0, true));
    CHECK_EQ(gApp->profile.rules.size(), (size_t)1);
    CHECK(!has(slurp(defaultFile()), "parking_brake_ratio"));
    CHECK(sim().clickText("Select all", 0, true));
    CHECK(sim().clickText("Remove selected", 0, true));
    CHECK(gApp->profile.rules.empty());
    CHECK(sim().clickText("Buttons", 0, true));
}

TEST(pinkie_held_has_its_own_light_colours) {
    CHECK(sim().clickText("Lights", 0, true));
    CHECK(sim().clickText("Pinkie held: off", 0, true));
    CHECK(sim().sees("Pinkie + 2"));
    CHECK(sim().clickText("Pinkie + 2", 0, true));
    CHECK(sim().clickText("Red", 0, true));             // Button A
    std::string file = slurp(defaultFile());
    CHECK(has(file, "ledsp 2 = fire:green a:red b:green"));
    CHECK(has(file, "leds 2 = fire:green a:green"));    // the plain colours are untouched
    CHECK(sim().clickText("Pinkie held: on", 0, true));
    CHECK(sim().clickText("Buttons", 0, true));
}

TEST(live_light_follows_a_dataref_and_falls_back_to_the_mode_colour) {
    const std::string brake = "sim/cockpit2/controls/parking_brake_ratio";
    gApp->profile.rules.clear();
    LightRule brakeRule = rulePreset(0).rule;
    brakeRule.led = LedE;
    gApp->profile.rules.push_back(brakeRule);
    Color out[kLedCount];
    gApp->lightColors(out, 100.0);   // the sim does not know the dataref yet
    CHECK(out[LedE] == gApp->profile.leds[gApp->engine.mode() - 1][LedE]);
    sim().datarefs[brake] = 1.0;
    gApp->lightColors(out, 200.0);   // found on the next look (not before 5 s)
    CHECK(out[LedE] == Color::Red);
    sim().datarefs[brake] = 0.0;
    gApp->lightColors(out, 200.1);
    CHECK(out[LedE] == Color::Green);
    gApp->testMode = true;           // test mode shows plain mode colours
    gApp->lightColors(out, 200.2);
    CHECK(out[LedE] == gApp->profile.leds[gApp->engine.mode() - 1][LedE]);
    gApp->testMode = false;
    gApp->profile.rules.clear();
}

TEST(text_fields_have_a_cursor_selection_and_clipboard) {
    gApp->profile.rules.clear();
    CHECK(sim().clickText("Live lights", 0, true));
    CHECK(sim().clickText("Add rule", 0, true));
    CHECK(sim().clickText("name[3] for one item"));          // the dataref field
    sim().type("sim/foo/bar");
    CHECK_EQ(gApp->profile.rules[0].dataref, std::string("sim/foo/bar"));
    for (int i = 0; i < 3; ++i) sim().key(0, XPLM_VK_LEFT);
    sim().type("x");                                          // typed at the cursor, not at the end
    CHECK_EQ(gApp->profile.rules[0].dataref, std::string("sim/foo/xbar"));
    sim().key(0, XPLM_VK_HOME, xplm_ShiftFlag);               // select back to the start
    sim().key(0, XPLM_VK_DELETE);                             // delete removes the selection
    CHECK_EQ(gApp->profile.rules[0].dataref, std::string("bar"));
    sim().key(0, XPLM_VK_A, xplm_ControlFlag);                // Command+A, Command+C
    sim().key(0, XPLM_VK_C, xplm_ControlFlag);
    CHECK_EQ(uiTestClipboard, std::string("bar"));
    sim().key(0, XPLM_VK_RIGHT);                              // an arrow drops the selection at its end
    sim().key(0, XPLM_VK_V, xplm_ControlFlag);                // Command+V
    CHECK_EQ(gApp->profile.rules[0].dataref, std::string("barbar"));
    sim().key(0, XPLM_VK_A, xplm_ControlFlag);
    sim().key(0, XPLM_VK_X, xplm_ControlFlag);                // Command+X cuts
    CHECK_EQ(gApp->profile.rules[0].dataref, std::string(""));
    CHECK_EQ(uiTestClipboard, std::string("barbar"));
    uiTestClipboard = "a b\n";                                 // blanks can never be part of a dataref
    sim().key(0, XPLM_VK_V, xplm_ControlFlag);
    CHECK_EQ(gApp->profile.rules[0].dataref, std::string("ab"));
    sim().key(0, XPLM_VK_BACK);
    CHECK_EQ(gApp->profile.rules[0].dataref, std::string("a"));
    sim().key(0, XPLM_VK_RETURN);
    gApp->profile.rules.clear();
    CHECK(sim().clickText("Buttons", 0, true));
}

TEST(live_rule_says_what_x_plane_makes_of_the_dataref) {
    CHECK(datarefReport("").empty());
    CHECK(has(datarefReport("sim/nope/nothing"), "Not found"));
    sim().datarefs["sim/test/value"] = 2.5;
    CHECK(has(datarefReport("sim/test/value"), "Found. Value now: 2.5"));
    CHECK(has(datarefReport(" sim/test/value "), "Found. Value now: 2.5"));   // stray blanks do not matter
    CHECK(has(datarefReport("sim/test/value[2]"), "Found"));
}

TEST(a_deleted_aircraft_profile_can_be_brought_back) {
    gApp->loadForAircraft("Cessna_172SP");
    if (!gApp->usingAircraftProfile) CHECK(sim().clickText("Make a profile for Cessna_172SP"));
    gApp->profile.name = "Cessna_172SP";
    gApp->profileChanged();
    CHECK(has(slurp(aircraftFile()), "name = Cessna_172SP"));
    CHECK(sim().clickText("Delete, use Default", 0, true));
    CHECK(sim().clickText("Really delete it?", 0, true));
    CHECK(!gApp->usingAircraftProfile);
    CHECK(access(aircraftFile().c_str(), R_OK) != 0);        // not a profile any more...
    CHECK(access((aircraftFile() + ".deleted").c_str(), R_OK) == 0);   // ...but still on disk
    CHECK(sim().sees("Undo delete"));
    CHECK(sim().clickText("Undo delete", 0, true));
    CHECK(gApp->usingAircraftProfile);
    CHECK(has(slurp(aircraftFile()), "name = Cessna_172SP"));
    CHECK(!sim().sees("Undo delete"));
}

TEST(plugin_shuts_down_cleanly) {
    XPluginDisable();
    XPluginStop();
    CHECK(gApp == nullptr);
    CHECK(!StickLock::heldByOther(sim().root + "/Output/preferences/X52Pro"));
}

int main() {
    char tmpl[] = "/tmp/x52test.XXXXXX";
    const char* scratch = getenv("TMPDIR");
    std::string base = scratch ? std::string(scratch) + "/x52test.XXXXXX" : tmpl;
    std::vector<char> buf(base.begin(), base.end());
    buf.push_back(0);
    if (!mkdtemp(buf.data())) {
        perror("mkdtemp");
        return 2;
    }
    sim().root = buf.data();
    mkdir((sim().root + "/Output").c_str(), 0755);
    mkdir((sim().root + "/Output/preferences").c_str(), 0755);
    mkdir((sim().root + "/Resources").c_str(), 0755);
    mkdir((sim().root + "/Resources/plugins").c_str(), 0755);
    {
        std::ofstream f(sim().root + "/Resources/plugins/Commands.txt");
        f << "sim/flight_controls/landing_gear_toggle            Landing gear toggle.\n"
          << "sim/flight_controls/flaps_down                     Flaps down a notch.\n"
          << "sim/autopilot/heading                              Autopilot heading-hold.\n";
    }
    sim().knownCommands = {kGear, "sim/flight_controls/flaps_down", "sim/autopilot/heading"};
    return runAllTests();
}
