// Runs the real plugin against a fake X-Plane: opens the window, clicks and types in
// it, presses stick buttons, and checks the commands sent and the profile file written.
// The tests share one plugin instance and run in order, like one session in the sim.
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

#include "../src/core/version.h"
#include "XPLMDefs.h"
#include "XPLMPlugin.h"
#include "XPLMUtilities.h"
#include <cmath>
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
    CHECK_EQ(std::string(name), std::string("X52 Pro ") + X52_VERSION);
    CHECK(has(std::string(desc), "Version " X52_VERSION));
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
    CHECK_EQ(gApp->page, (int)PageComNav1);
    XPLMCommandOnce(XPLMFindCommand("x52pro/mfd_page_next"));
    CHECK_EQ(gApp->page, (int)PageComNav2);
    XPLMCommandOnce(XPLMFindCommand("x52pro/mfd_page_prev"));
    CHECK_EQ(gApp->page, (int)PageComNav1);
    XPLMCommandOnce(XPLMFindCommand("x52pro/mfd_page_prev"));
    CHECK_EQ(gApp->page, (int)PageBrightness);   // the Brightness page is last, so previous wraps to it
    XPLMCommandOnce(XPLMFindCommand("x52pro/mfd_page_prev"));
    CHECK_EQ(gApp->page, (int)PageButtons);      // and the Buttons page comes just before it
    gApp->setPage(PageComNav1);
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

TEST(a_fast_turn_of_the_right_wheel_jumps_and_a_slow_one_steps_by_one) {
    sim().knownCommands.push_back("sim/autopilot/heading_up");
    sim().knownCommands.push_back("sim/autopilot/heading_down");
    gApp->setPage(PageAutopilot);
    gApp->cursor.item = 0;   // HDG
    const double t = gApp->now() + 5000;
    gApp->wheelAt(Wheel::Select, t);   // start changing
    CHECK(gApp->cursor.editing);
    int up = sim().onces["sim/autopilot/heading_up"];
    gApp->wheelAt(Wheel::Up, t + 1.0);    // first click after a rest: one degree
    CHECK_EQ(sim().onces["sim/autopilot/heading_up"], up + 1);
    gApp->wheelAt(Wheel::Up, t + 1.4);    // slow turn: still one
    CHECK_EQ(sim().onces["sim/autopilot/heading_up"], up + 2);
    gApp->wheelAt(Wheel::Up, t + 1.42);   // 20 ms later: a fast spin jumps 10
    CHECK_EQ(sim().onces["sim/autopilot/heading_up"], up + 12);
    int down = sim().onces["sim/autopilot/heading_down"];
    gApp->wheelAt(Wheel::Down, t + 1.43);   // reversing restarts at one step
    CHECK_EQ(sim().onces["sim/autopilot/heading_down"], down + 1);
    gApp->profile.dial.on = false;
    gApp->wheelAt(Wheel::Up, t + 1.44);
    gApp->wheelAt(Wheel::Up, t + 1.45);
    CHECK_EQ(sim().onces["sim/autopilot/heading_up"], up + 14);
    gApp->profile.dial.on = true;
    gApp->wheelAt(Wheel::Select, t + 3.0);   // done editing
    gApp->setPage(PageButtons);
}

static bool nearTo(double a, double b) { return std::fabs(a - b) < 1e-3; }

TEST(flight_axes_stay_with_x_plane_unless_the_override_is_on) {
    auto& d = sim().datarefs;
    d["sim/joystick/yoke_roll_ratio"] = 0.7;
    d["sim/operation/override/override_joystick"] = 0;
    gApp->profile.axes = AxisSettings();
    CHECK(!gApp->profile.axes.overrideXPlane);
    gApp->input.pushAxis({0x01, 0x30, 1023, 0, 1023});
    sim().frame();
    CHECK(nearTo(d["sim/joystick/yoke_roll_ratio"], 0.7));
    CHECK_EQ((int)d["sim/operation/override/override_joystick"], 0);
}

TEST(with_the_override_on_the_plugin_writes_shaped_axes_and_hands_them_back_when_it_is_switched_off) {
    auto& d = sim().datarefs;
    const char* roll = "sim/joystick/yoke_roll_ratio";
    const char* pitch = "sim/joystick/yoke_pitch_ratio";
    const char* ovr = "sim/operation/override/override_joystick";
    const char* thr = "sim/flightmodel/engine/ENGN_thro_use";
    const char* ovrThr = "sim/operation/override/override_throttles";
    for (const char* n : {roll, pitch, "sim/joystick/yoke_heading_ratio", ovr, thr, ovrThr}) d[n] = 0.0;
    gApp->profile.axes = AxisSettings();
    gApp->profile.axes.overrideXPlane = true;
    gApp->profile.axes.pluginThrottle = true;
    gApp->profile.axes.roll.deadzone = 10;
    gApp->profile.axes.pitch.invert = true;
    gApp->profile.axes.pitch.deadzone = 0;

    gApp->input.pushAxis({0x01, 0x30, 560, 0, 1023});    // a little right of centre: inside the dead zone
    gApp->input.pushAxis({0x01, 0x31, 0, 0, 1023});      // pitch fully one way, inverted
    sim().frame();
    CHECK(nearTo(d[roll], 0.0));
    CHECK(nearTo(d[pitch], 1.0));
    CHECK_EQ((int)d[ovr], 1);

    gApp->input.pushAxis({0x01, 0x30, 1023, 0, 1023});
    gApp->input.pushAxis({0x01, 0x32, 255, 0, 255});
    sim().frame();
    CHECK(nearTo(d[roll], 1.0));
    CHECK(nearTo(d[thr], 1.0));
    CHECK_EQ((int)d[ovrThr], 1);

    gApp->profile.axes.overrideXPlane = false;
    gApp->profile.axes.pluginThrottle = false;
    sim().frame();
    CHECK_EQ((int)d[ovr], 0);
    CHECK_EQ((int)d[ovrThr], 0);
    d[roll] = 0.25;   // X-Plane's own reading is left alone again
    sim().frame();
    CHECK(nearTo(d[roll], 0.25));
}

TEST(the_throttle_moves_the_cockpit_levers_too_and_the_slider_sets_the_flaps_at_their_detents) {
    auto& d = sim().datarefs;
    const char* handle = "sim/cockpit2/engine/actuators/throttle_ratio";
    const char* flaps = "sim/cockpit2/controls/flap_handle_request_ratio";
    const char* detents = "sim/aircraft/controls/acf_flap_detents";
    for (const char* n : {"sim/flightmodel/engine/ENGN_thro_use", handle, "sim/operation/override/override_throttles", flaps})
        d[n] = 0.0;
    d[detents] = 4;
    gApp->flapWritten = -1;
    gApp->profile.axes = AxisSettings();
    gApp->profile.axes.pluginThrottle = true;
    gApp->profile.axes.throttle.invert = true;                // the X52 lever reads 0 at full power
    gApp->profile.axes.throttle.deadzone = 0;
    gApp->input.pushAxis({0x01, 0x32, 200, 0, 255});
    sim().frame();
    gApp->input.pushAxis({0x01, 0x32, 0, 0, 255});           // full forward
    sim().frame();
    CHECK(nearTo(d["sim/flightmodel/engine/ENGN_thro_use"], 1.0));
    CHECK(nearTo(d[handle], 1.0));                            // the levers in the cockpit follow

    gApp->profile.axes.sliderFlaps = true;
    gApp->input.pushAxis({0x01, 0x36, 0, 0, 255});            // where the slider rests (towards you): nothing yet
    sim().frame();
    CHECK(nearTo(d[flaps], 0.0));
    gApp->input.pushAxis({0x01, 0x36, 140, 0, 255});          // a bit past half way: the nearest of 4 stops
    sim().frame();
    CHECK(nearTo(d[flaps], 0.5));
    d[flaps] = 0.25;                                          // a flap button in between is left alone...
    sim().frame();
    CHECK(nearTo(d[flaps], 0.25));
    gApp->input.pushAxis({0x01, 0x36, 255, 0, 255});          // ...until the slider reaches another stop: fully away
    sim().frame();
    CHECK(nearTo(d[flaps], 1.0));
    gApp->input.pushAxis({0x01, 0x36, 5, 0, 255});            // back towards you: flaps up
    sim().frame();
    CHECK(nearTo(d[flaps], 0.0));
    gApp->profile.axes.slider.invert = true;                  // Invert flaps: towards you is deployed
    gApp->input.pushAxis({0x01, 0x36, 0, 0, 255});
    sim().frame();
    CHECK(nearTo(d[flaps], 1.0));
    gApp->profile.axes = AxisSettings();
    sim().frame();
}

TEST(the_plugin_can_drive_the_throttle_alone_and_leave_the_stick_to_x_plane) {
    auto& d = sim().datarefs;
    const char* thr = "sim/flightmodel/engine/ENGN_thro_use";
    const char* handle = "sim/cockpit2/engine/actuators/throttle_ratio";
    const char* roll = "sim/joystick/yoke_roll_ratio";
    for (const char* n : {thr, handle, roll, "sim/operation/override/override_throttles", "sim/operation/override/override_joystick"}) d[n] = 0.0;
    gApp->releaseAxes();
    gApp->profile.axes = AxisSettings();
    gApp->profile.axes.pluginThrottle = true;
    gApp->profile.axes.throttle.invert = true;
    gApp->profile.axes.throttle.deadzone = 5;
    gApp->input.pushAxis({0x01, 0x32, 255, 0, 255});          // lever back: idle
    gApp->input.pushAxis({0x01, 0x30, 1023, 0, 1023});
    sim().frame();
    gApp->input.pushAxis({0x01, 0x32, 0, 0, 255});            // full forward
    gApp->input.pushAxis({0x01, 0x30, 900, 0, 1023});
    d[roll] = 0.3;                                            // X-Plane's own stick reading
    sim().frame();
    CHECK(nearTo(d[thr], 1.0));
    CHECK(nearTo(d[handle], 1.0));
    CHECK_EQ((int)d["sim/operation/override/override_throttles"], 1);
    CHECK_EQ((int)d["sim/operation/override/override_joystick"], 0);   // the stick stays X-Plane's
    CHECK(nearTo(d[roll], 0.3));
    gApp->input.pushAxis({0x01, 0x32, 128, 0, 255});          // half way
    sim().frame();
    CHECK(d[thr] > 0.4 && d[thr] < 0.6);
    gApp->profile.axes = AxisSettings();
    sim().frame();
    CHECK_EQ((int)d["sim/operation/override/override_throttles"], 0);   // handed back
}

TEST(letting_go_of_the_stick_reaches_the_sim_in_the_same_frame_and_the_damper_only_nudges_in_flight) {
    auto& d = sim().datarefs;
    const char* pitch = "sim/joystick/yoke_pitch_ratio";
    const char* q = "sim/flightmodel/position/Q";
    const char* ground = "sim/flightmodel/failures/onground_any";
    const char* ap = "sim/cockpit2/autopilot/servos_on";
    for (const char* n : {pitch, "sim/joystick/yoke_roll_ratio", "sim/joystick/yoke_heading_ratio",
                          "sim/operation/override/override_joystick", q, "sim/flightmodel/position/P", ground, ap})
        d[n] = 0.0;
    gApp->releaseAxes();
    gApp->calibration.clear();
    gApp->profile.axes = AxisSettings();
    gApp->profile.axes.overrideXPlane = true;
    gApp->input.pushAxis({0x01, 0x31, 1023, 0, 1023});   // stick pulled fully back
    sim().frame();
    CHECK(nearTo(d[pitch], 1.0));
    gApp->input.pushAxis({0x01, 0x31, 512, 0, 1023});    // let go: springs to the centre
    sim().frame();
    CHECK(std::fabs(d[pitch]) < 0.01);                   // no smoothing lag on a big move

    d[q] = 10.0;                                         // airborne, nose pitching up at 10 deg/s
    sim().frame();
    CHECK(d[pitch] < 0.0 && d[pitch] >= -0.046);         // a small push against it, never more
    {   // the Axes tab's pitch check says what was sent and how much of it is the damper
        std::vector<std::string> check = gApp->pitchCheck();
        CHECK_EQ(check.size(), (size_t)2);
        CHECK(check[0].find("damper -0.0") != std::string::npos);
        CHECK(check[1].find("pitch rate +10.0 deg/s") != std::string::npos);
    }
    d[ground] = 1;                                       // on the ground: hands off
    sim().frame();
    CHECK(std::fabs(d[pitch]) < 0.01);
    d[ground] = 0;
    d[ap] = 1;                                           // the autopilot flies: hands off too
    sim().frame();
    CHECK(std::fabs(d[pitch]) < 0.01);
    d[ap] = 0;
    gApp->profile.axes.damping = 0;                      // switched off
    sim().frame();
    CHECK(std::fabs(d[pitch]) < 0.01);
    d[q] = 0.0;
    gApp->profile.axes = AxisSettings();
    sim().frame();
}

TEST(the_stick_is_calibrated_from_the_window_and_the_calibration_is_kept_beside_the_profiles) {
    auto& d = sim().datarefs;
    const char* pitch = "sim/joystick/yoke_pitch_ratio";
    for (const char* n : {pitch, "sim/operation/override/override_joystick"}) d[n] = 0.0;
    gApp->releaseAxes();
    gApp->resetCalibration();
    gApp->profile.axes = AxisSettings();
    gApp->profile.axes.overrideXPlane = true;
    gApp->profile.axes.pitch = {0, 0, false};            // linear, to read the calibration straight off
    gApp->profile.axes.smoothingMs = 0;
    gApp->profileChanged();
    CHECK(sim().clickText("Axes", 0, true));
    CHECK(sim().sees("full range is used"));
    CHECK(sim().clickText("Calibrate stick", 0, true));
    CHECK(gApp->calibrator.active());
    // This stick rests at 600 and only reaches 100..950.
    for (long v : {600L, 100L, 950L, 600L}) {
        gApp->input.pushAxis({0x01, 0x31, v, 0, 1023});
        gApp->input.pushAxis({0x01, 0x30, v == 600 ? 512 : v == 100 ? 20 : 1000, 0, 1023});
        sim().frame();
    }
    gApp->input.pushAxis({0x01, 0x30, 512, 0, 1023});
    sim().frame();
    CHECK(sim().clickText("Done", 0, true));
    CHECK(!gApp->calibrator.active());
    CHECK(sim().sees("calibrated"));
    CHECK_EQ(gApp->calibration[0x31].centre, 600L);
    CHECK(std::fabs(d[pitch]) < 0.01);                   // resting at 600 is the centre now
    gApp->input.pushAxis({0x01, 0x31, 950, 0, 1023});
    sim().frame();
    CHECK(nearTo(d[pitch], 1.0));                        // the measured end is full travel
    // Kept in its own file and read back when the plugin starts.
    std::ifstream f(gApp->store.dir + "/stick-calibration.txt");
    CHECK(f.good());
    gApp->calibration.clear();
    gApp->loadCalibration();
    CHECK_EQ(gApp->calibration[0x31].max, 950L);
    CHECK(sim().clickText("Reset", 0, true));
    CHECK(gApp->calibration.empty());
    CHECK(!std::ifstream(gApp->store.dir + "/stick-calibration.txt").good());
    // Calibrated in the app meanwhile: picked up within a second, no restart needed.
    std::map<int, AxisCalibration> fromApp = {{0x31, {40, 512, 980}}};
    CHECK(saveCalibration(gApp->store.dir, fromApp));
    gApp->nextSlowCheck = 0;
    sim().frame();
    CHECK_EQ(gApp->calibration[0x31].max, 980L);
    gApp->resetCalibration();
    gApp->profile.axes = AxisSettings();
    gApp->profileChanged();
    sim().frame();
    CHECK(sim().clickText("Buttons", 0, true));
}

TEST(the_window_can_tell_which_axis_is_moving) {
    gApp->profile.axes = AxisSettings();
    gApp->input.pushAxis({0x01, 0x30, 100, 0, 1023});
    gApp->input.pushAxis({0x01, 0x33, 100, 0, 255});
    sim().frame();
    gApp->input.pushAxis({0x01, 0x30, 102, 0, 1023});   // noise: under 2 % of the travel
    sim().frame();
    gApp->input.pushAxis({0x01, 0x33, 200, 0, 255});    // the lower rotary turned
    sim().frame();
    CHECK_EQ(gApp->axisMoved.first, 0x01);
    CHECK_EQ(gApp->axisMoved.second, 0x33);
    CHECK_EQ(std::string(controlName(0x01, 0x33)), std::string("Lower rotary (i)"));
    auto it = gApp->axisLive.find(gApp->axisMoved);
    CHECK(it != gApp->axisLive.end());
    if (it != gApp->axisLive.end()) CHECK_EQ(it->second.value, 200L);
    gApp->input.pushAxis({0x01, 0x30, 900, 0, 1023});
    sim().frame();
    CHECK_EQ(gApp->axisMoved.second, 0x30);
}

TEST(the_axes_tab_shows_live_values_names_the_moving_control_and_has_the_override_checkbox) {
    gApp->profile.axes = AxisSettings();
    CHECK(sim().clickText("Axes", 0, true));
    gApp->input.pushAxis({0x01, 0x34, 10, 0, 255});
    sim().frame();
    gApp->input.pushAxis({0x01, 0x34, 200, 0, 255});    // the upper rotary turned
    sim().frame();
    CHECK(sim().sees("Moving now: Upper rotary (E)"));
    CHECK(sim().sees("Stick left/right"));
    CHECK(!gApp->profile.axes.overrideXPlane);
    CHECK(sim().clickText("[  ]", 0, true));   // the first checkbox on the tab: process the axes here
    CHECK(gApp->profile.axes.overrideXPlane);
    int dz = gApp->profile.axes.roll.deadzone;
    CHECK(sim().clickText("+", 0, true));      // first "+" is the roll dead zone
    CHECK_EQ(gApp->profile.axes.roll.deadzone, dz + 1);
    gApp->profile.axes = AxisSettings();
    gApp->profileChanged();
    CHECK(sim().clickText("Buttons", 0, true));
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

TEST(pov_1_looks_around_while_pushed_and_picks_profiles_only_with_the_clutch) {
    // The default bindings send keys (Q E R F), which a test must not type into the Mac: bind commands instead.
    for (const char* c : {"sim/general/up", "sim/general/right", "sim/general/down", "sim/general/left"})
        sim().knownCommands.push_back(c);
    {
        Profile p = gApp->profile;
        const char* cmds[4] = {"sim/general/up", "sim/general/right", "sim/general/down", "sim/general/left"};
        for (int i = 0; i < 4; ++i) {
            Binding& b = p.edit(kPov1Up + i, kLayerAny);
            b.press.clear();
            b.press.push_back({StepKind::Command, cmds[i], 0});
        }
        gApp->engine.setProfile(p);
    }
    auto hat = [](long v) {
        gApp->input.pushAxis({0x01, 0x39, v, 1, 8});
        sim().frame();
    };
    hat(8);   // up-left: both directions are held
    CHECK_EQ(sim().begins["sim/general/up"], 1);
    CHECK_EQ(sim().begins["sim/general/left"], 1);
    CHECK_EQ(sim().begins["sim/general/right"], 0);
    hat(0);
    CHECK_EQ(sim().ends["sim/general/up"], 1);
    CHECK_EQ(sim().ends["sim/general/left"], 1);
    press(31, true);   // with the clutch it is the profile picker, not a look
    hat(7);
    CHECK_EQ(sim().begins["sim/general/left"], 1);
    hat(0);
    press(31, false);
    gApp->engine.setProfile(gApp->profile);
}

TEST(the_mouse_nub_pans_part_way_and_pans_fast_all_the_way) {
    // The default bindings send keys, which a test must not type into the Mac: bind commands instead.
    const char* cmds[8] = {"test/pan_up", "test/pan_right", "test/pan_down", "test/pan_left",
                           "test/fast_up", "test/fast_right", "test/fast_down", "test/fast_left"};
    for (const char* c : cmds) sim().knownCommands.push_back(c);
    {
        Profile p = gApp->profile;
        for (int i = 0; i < 8; ++i) {
            Binding& b = p.edit(kNubUp + i, kLayerAny);
            b.press.clear();
            b.press.push_back({StepKind::Command, cmds[i], 0});
        }
        gApp->engine.setProfile(p);
    }
    auto nub = [](int usage, long v) {
        gApp->input.pushAxis({0x05, usage, v, 0, 255});
        sim().frame();
    };
    nub(0x24, 128);                           // centred
    CHECK_EQ(sim().begins["test/pan_right"], 0);
    nub(0x24, 200);                           // part way right
    CHECK_EQ(sim().begins["test/pan_right"], 1);
    CHECK_EQ(sim().begins["test/fast_right"], 0);
    nub(0x24, 255);                           // all the way: the fast pan takes over
    CHECK_EQ(sim().begins["test/fast_right"], 1);
    CHECK_EQ(sim().ends["test/pan_right"], 1);
    nub(0x24, 128);
    CHECK_EQ(sim().ends["test/fast_right"], 1);
    nub(0x26, 0);                             // all the way up
    CHECK_EQ(sim().begins["test/fast_up"], 1);
    nub(0x26, 128);
    CHECK_EQ(sim().ends["test/fast_up"], 1);
    gApp->engine.setProfile(gApp->profile);
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
    CHECK(has(slurp(defaultFile()), "x52pro-profile 6"));
    CHECK(has(slurp(defaultFile()), std::string("cmd:") + kGear));
}

TEST(live_lights_tab_adds_opens_edits_and_removes_rules) {
    CHECK(sim().clickText("Live lights", 0, true));
    CHECK(sim().sees("No live lights yet. Press Add rule."));
    CHECK(sim().clickText("Add rule", 0, true));
    CHECK_EQ(gApp->profile.rules.size(), (size_t)1);
    CHECK(sim().sees("Fire button"));                   // the new rule takes the first free light
    CHECK(sim().clickText("Parking brake: red on, green off", 0, true));   // it opened when added
    CHECK(has(slurp(defaultFile()), "rule fire > 0.5 red green sim/flightmodel/controls/parkbrake"));
    CHECK(sim().clickText(">", 0, true));               // the comparison button opens a list
    CHECK(sim().sees("!="));
    CHECK(sim().clickText(">=", 0, true));
    CHECK(has(slurp(defaultFile()), "rule fire >= 0.5 red green"));
    CHECK(!sim().sees("!="));                           // and it closed again
    CHECK(sim().clickText("Add rule", 0, true));        // a second rule, the first stays
    CHECK_EQ(gApp->profile.rules.size(), (size_t)2);
    CHECK(has(slurp(defaultFile()), "rule fire >= 0.5 red green sim/flightmodel/controls/parkbrake"));
    CHECK(has(slurp(defaultFile()), "rule a > 0.5 red green -"));   // the second rule took the next free light
    CHECK(sim().clickText("[  ]", 0, true));            // tick the first rule
    CHECK(sim().clickText("Remove selected", 0, true));
    CHECK_EQ(gApp->profile.rules.size(), (size_t)1);
    CHECK(!has(slurp(defaultFile()), "controls/parkbrake"));
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
    const std::string brake = "sim/flightmodel/controls/parkbrake";
    gApp->profile.rules.clear();
    LightRule brakeRule = rulePreset(0).rule;
    brakeRule.led = LedE;
    gApp->profile.rules.push_back(brakeRule);
    Color out[kLedCount];
    const double base = gApp->now() + 1000;   // pretend times well after anything the real clock looked up earlier
    gApp->lightColors(out, base);   // the sim does not know the dataref yet
    CHECK(out[LedE] == gApp->profile.leds[gApp->engine.mode() - 1][LedE]);
    sim().datarefs[brake] = 1.0;
    gApp->lightColors(out, base + 100);   // found on the next look (not before 5 s)
    CHECK(out[LedE] == Color::Red);
    sim().datarefs[brake] = 0.0;
    gApp->lightColors(out, base + 100.1);
    CHECK(out[LedE] == Color::Green);
    gApp->testMode = true;           // test mode shows plain mode colours
    gApp->lightColors(out, base + 100.2);
    CHECK(out[LedE] == gApp->profile.leds[gApp->engine.mode() - 1][LedE]);
    gApp->testMode = false;
    gApp->profile.rules.clear();
}

TEST(master_switches_blank_the_screen_while_their_dataref_is_false) {
    const std::string power = "sim/cockpit2/switches/avionics_power_on";
    gApp->profile.masterScreen = Trigger();
    gApp->profile.masterScreen.dataref = power;
    gApp->profile.masterScreen.op = Compare::Greater;
    gApp->profile.masterScreen.value = 0.5;
    gApp->page = PageButtons;
    gApp->nextDeviceUpdate = 0;
    sim().datarefs[power] = 1.0;
    sim().frame();                       // finds the dataref
    gApp->nextDeviceUpdate = 0;
    sim().frame();
    CHECK(!gApp->screen.l[0].empty());   // power on: the page shows
    sim().datarefs[power] = 0.0;
    gApp->nextDeviceUpdate = 0;
    sim().frame();
    CHECK(gApp->screen.l[0].empty() && gApp->screen.l[1].empty() && gApp->screen.l[2].empty());
    sim().datarefs[power] = 1.0;
    gApp->nextDeviceUpdate = 0;
    sim().frame();
    CHECK(!gApp->screen.l[0].empty());   // and back
    gApp->profile.masterScreen = Trigger();
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

TEST(tab_moves_to_the_next_text_field_and_escape_cancels_the_command_search) {
    gApp->profile.rules.clear();
    CHECK(sim().clickText("Live lights", 0, true));
    CHECK(sim().clickText("Add rule", 0, true));
    CHECK(sim().clickText("name[3] for one item"));
    sim().type("sim/foo");
    sim().key(0, XPLM_VK_TAB);                                // on to the number field
    CHECK(sim().hasKeyboardFocus);
    sim().key('7', XPLM_VK_7);
    CHECK_EQ(gApp->profile.rules[0].dataref, std::string("sim/foo"));
    CHECK_EQ(gApp->profile.rules[0].value, 0.57);   // typed after the 0.5 already there
    sim().key(0, XPLM_VK_TAB, xplm_ShiftFlag);                // and back
    sim().type("x");
    CHECK_EQ(gApp->profile.rules[0].dataref, std::string("sim/foox"));
    sim().key(0, XPLM_VK_RETURN);
    gApp->profile.rules.clear();
    CHECK(sim().clickText("Buttons", 0, true));
    CHECK(sim().clickText("+ Command"));
    CHECK(sim().sees("Choose a command for"));
    sim().key(0, XPLM_VK_ESCAPE);
    CHECK(!sim().sees("Choose a command for"));
    CHECK(!sim().hasKeyboardFocus);
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

TEST(a_button_can_write_set_toggle_and_add_to_datarefs) {
    const std::string gear = "sim/test/switch", knob = "sim/test/knob", locked = "sim/test/readonly_thing";
    sim().datarefs[gear] = 0;
    sim().datarefs[knob] = 0.5;
    sim().datarefs[locked] = 3;
    Action a;
    CHECK(actionFromText("set:" + gear + "=1", a, nullptr));
    gApp->profile.edit(20, kLayerAny).press = a;
    CHECK(actionFromText("toggle:" + gear, a, nullptr));
    gApp->profile.edit(21, kLayerAny).press = a;
    CHECK(actionFromText("add:" + knob + "=0.25,0,1", a, nullptr));
    gApp->profile.edit(22, kLayerAny).press = a;
    CHECK(actionFromText("set:" + locked + "=9", a, nullptr));
    gApp->profile.edit(23, kLayerAny).press = a;
    gApp->profileChanged();
    press(20, true);
    press(20, false);
    CHECK_EQ(sim().datarefs[gear], 1.0);
    press(21, true);
    press(21, false);
    CHECK_EQ(sim().datarefs[gear], 0.0);   // toggled off
    press(21, true);
    press(21, false);
    CHECK_EQ(sim().datarefs[gear], 1.0);   // and on again
    press(22, true);
    press(22, false);
    CHECK_EQ(sim().datarefs[knob], 0.75);
    press(22, true);
    press(22, false);
    press(22, true);
    press(22, false);
    CHECK_EQ(sim().datarefs[knob], 1.0);   // kept at the top of its range
    press(23, true);
    press(23, false);
    CHECK_EQ(sim().datarefs[locked], 3.0); // read-only: untouched, and the window says so
    CHECK(has(gApp->notice, "does not let a plugin change"));
    for (int b = 20; b <= 23; ++b) gApp->profile.edit(b, kLayerAny).press.clear();
    gApp->profile.prune();
    gApp->profileChanged();
}

TEST(the_plugin_learns_which_x_plane_slot_a_stick_button_is) {
    sim().datarefs["sim/joystick/joystick_button_values"] = 0;
    sim().joyValues.assign(200, 0);
    press(20, true);                       // the stick reports button 20, X-Plane has not noticed yet
    CHECK(gApp->learner.map().count(20) == 0);
    sim().joyValues[33] = 1;               // X-Plane shows its slot 33 pressed
    sim().frame();
    CHECK_EQ(gApp->learner.map()[20], 33);
    press(20, false);
    sim().joyValues[33] = 0;
    std::string file = slurp(sim().root + "/Output/preferences/X52Pro/xplane-map.txt");
    CHECK(has(file, "20 33"));
    sim().frame();
    CHECK_EQ(gApp->learner.map()[20], 33);  // releasing teaches nothing new
    press(28, true);                        // the mode dial is a switch: not learned
    sim().joyValues[40] = 1;
    sim().frame();
    CHECK(gApp->learner.map().count(28) == 0);
    sim().joyValues.assign(200, 0);
    press(28, false);
    gApp->learner.map().clear();
}

TEST(the_plugin_saves_the_aircraft_description_for_the_app) {
    sim().datarefs["sim/aircraft/view/acf_descrip"] = 0;
    sim().acfDescrip = "Cessna Citation X";
    gApp->loadForAircraft("Cessna_CitationX");
    CHECK(has(slurp(sim().root + "/Output/preferences/X52Pro/xplane-aircraft.txt"), "Cessna Citation X"));
    gApp->loadForAircraft("Cessna_172SP");
}

TEST(the_window_has_a_profile_picker) {
    Profile other = defaultProfile();
    other.name = "Other";
    std::string err;
    CHECK(gApp->store.save("Other", other, &err));
    {   // a ready-made profile next to it, so the dropdown has both groups
        Profile ready = defaultProfile();
        ready.name = "Readymade";
        CHECK(gApp->store.save("Readymade", ready, &err));
        std::ofstream f(gApp->store.pathFor("Readymade"), std::ios::app);
        f << "standard = 1\n";
    }
    CHECK(gApp->manualKey.empty());
    CHECK(sim().clickText("Change profile...", 0, true));
    sim().frame();
    CHECK(sim().sees("Automatic (aircraft, else Default)"));
    CHECK(sim().sees("[-] Your profiles ("));                 // the user's group starts unfolded, the ready-made one folded
    CHECK(sim().sees("[+] Ready-made profiles ("));
    CHECK(!sim().sees("      Readymade"));
    CHECK(sim().clickText("[+] Ready-made profiles (", 0, false));   // unfolding keeps the list open
    CHECK(sim().sees("[-] Ready-made profiles ("));
    CHECK(sim().sees("      Readymade"));
    CHECK(sim().clickText("[-] Ready-made profiles (", 0, false));
    CHECK(!sim().sees("      Readymade"));
    CHECK(sim().clickText("      Other", 0, true));
    CHECK_EQ(gApp->manualKey, std::string("Other"));
    CHECK_EQ(gApp->profile.name, std::string("Other"));
    sim().frame();
    CHECK(sim().sees("Profile: Other (picked)"));
    CHECK(!sim().sees("Delete, use Default"));                // the aircraft's own profile is not offered for deletion now
    CHECK(sim().clickText("Change profile...", 0, true));
    sim().frame();
    CHECK(sim().clickText("Automatic (aircraft, else Default)", 0, true));
    CHECK(gApp->manualKey.empty());
    CHECK_EQ(gApp->profile.name, gApp->usingAircraftProfile ? gApp->aircraft : std::string("default"));
    gApp->store.remove("Other");
    gApp->store.remove("Readymade");
}

TEST(the_window_has_undo_and_redo_for_profile_changes) {
    gApp->manualKey.clear();
    gApp->reload();                                           // a fresh history for the profile in use
    sim().frame();
    CHECK(!gApp->canUndo());
    CHECK(sim().sees("Undo") && sim().sees("Redo"));
    int original = gApp->profile.ledBrightness;
    gApp->profile.ledBrightness = original == 50 ? 60 : 50;
    gApp->profileChanged();
    int changed = gApp->profile.ledBrightness;
    sim().frame();
    CHECK(gApp->canUndo() && !gApp->canRedo());
    CHECK(sim().clickText("Undo", 0, true));
    CHECK_EQ(gApp->profile.ledBrightness, original);
    CHECK(has(slurp(gApp->store.pathFor(gApp->profileKey())), "led_brightness = " + std::to_string(original)));
    sim().frame();
    CHECK(!gApp->canUndo() && gApp->canRedo());
    CHECK(sim().clickText("Redo", 0, true));
    CHECK_EQ(gApp->profile.ledBrightness, changed);
    sim().key('z', 'Z', xplm_ControlFlag);                    // Command+Z while the window has the keyboard
    CHECK_EQ(gApp->profile.ledBrightness, original);
    sim().key('z', 'Z', xplm_ControlFlag | xplm_ShiftFlag);   // Shift+Command+Z
    CHECK_EQ(gApp->profile.ledBrightness, changed);
    gApp->undo();
    gApp->reload();
}

TEST(the_undo_and_redo_buttons_show_a_tip_when_the_mouse_rests_on_them) {
    gApp->reload();
    gApp->history.reset(serialize(gApp->profile));            // nothing to undo or redo
    sim().frame();
    CHECK(!sim().sees("Nothing to redo"));
    double saved = uiTipDelay;
    uiTipDelay = 0;
    CHECK(sim().hoverText("Redo", 0, true));
    sim().frame();
    sim().frame();
    CHECK(sim().sees("Nothing to redo"));
    uiTipDelay = 2.0;
    sim().frame();
    CHECK(!sim().sees("Nothing to redo"));                   // a longer wait: not yet
    uiTipDelay = saved;
}

TEST(the_brightness_page_changes_and_saves_the_screen_and_light_brightness) {
    gApp->profile.pages = PageTree();
    gApp->profile.brightScreen = BrightnessLink();
    gApp->profile.brightLights = BrightnessLink();
    gApp->profile.mfdBrightness = 127;
    gApp->profile.ledBrightness = 127;
    gApp->setPage(PageBrightness);
    gApp->nextDeviceUpdate = 0;
    sim().frame();
    CHECK_EQ(gApp->screen.l[0], std::string("BRIGHTNESS"));
    CHECK_EQ(gApp->screen.l[1], std::string("Screen     >100%"));       // the cursor starts on the screen
    press(39, true);                                                   // press: start changing
    press(39, false);
    press(38, true);                                                   // turn down: 5 % less
    press(38, false);
    CHECK_EQ(gApp->profile.mfdBrightness, 121);
    gApp->nextDeviceUpdate = 0;
    sim().frame();
    CHECK_EQ(gApp->screen.l[1], std::string("Screen     >>95%"));
    CHECK(has(slurp(gApp->store.pathFor(gApp->profileKey())), "mfd_brightness = 121"));   // saved at once
    press(39, true);                                                   // press: finish
    press(39, false);
    press(38, true);                                                   // turn down: the cursor moves to the lights
    press(38, false);
    CHECK_EQ(gApp->profile.mfdBrightness, 121);                        // turning alone changed nothing
    press(39, true);
    press(39, false);
    for (int i = 0; i < 30; ++i) {
        press(38, true);
        press(38, false);
    }
    CHECK_EQ(gApp->profile.ledBrightness, 0);                          // the lights can go fully dark
    gApp->profile.mfdBrightness = 127;
    gApp->profile.ledBrightness = 127;
    gApp->profileChanged();
    gApp->setPage(PageComNav1);
}

TEST(user_pages_join_the_left_wheel_before_the_buttons_page_and_the_right_wheel_moves_inside_them) {
    sim().datarefs["sim/fuel/left"] = 245;
    PageTree t;
    std::string err;
    CHECK(parsePageLine("fuel \"Fuel\"", t, err) && parseRowLine("fuel text \"FUEL\"", t, err) &&
          parseRowLine("fuel value \"Left\" \"%.0f kg\" sim/fuel/left", t, err) &&
          parseRowLine("fuel link \"More\" more", t, err) && parsePageLine("more \"More\" in fuel", t, err) &&
          parseRowLine("more text \"DEEPER\"", t, err));
    gApp->profile.pages = t;
    gApp->setPage(PageVorHdg);
    gApp->stepPage(1);
    CHECK_EQ(gApp->page, (int)kPageCount);                    // the user's page comes right after the radio pages
    CHECK_EQ(gApp->pageTitle(gApp->page), std::string("Fuel"));
    gApp->nextDeviceUpdate = 0;
    sim().frame();
    gApp->nextDeviceUpdate = 0;
    sim().frame();
    CHECK_EQ(gApp->screen.l[0], std::string(">FUEL"));
    CHECK_EQ(gApp->screen.l[1], std::string(" Left     245 kg"));
    gApp->stepPage(1);
    CHECK_EQ(gApp->page, (int)PageButtons);                   // the Buttons page follows the user's pages
    gApp->stepPage(1);
    CHECK_EQ(gApp->page, (int)PageBrightness);                // and the Brightness page is last
    gApp->stepPage(1);
    CHECK_EQ(gApp->page, (int)PageComNav1);
    gApp->stepPage(-1);
    gApp->stepPage(-1);
    gApp->stepPage(-1);
    CHECK_EQ(gApp->page, (int)kPageCount);
    press(38, true);                                          // right wheel down, down, then select the link
    press(38, false);
    press(38, true);
    press(38, false);
    press(39, true);
    press(39, false);
    sim().frame();
    CHECK_EQ(gApp->screen.l[0], std::string(">< Back"));
    CHECK_EQ(gApp->screen.l[1], std::string(" DEEPER"));
    gApp->stepPage(1);                                        // leaving the page forgets where it was
    gApp->stepPage(-1);
    CHECK(gApp->nav.stack.empty());
    gApp->profile.pages = PageTree();                         // pages removed while one is shown: back to the radio page
    gApp->nextDeviceUpdate = 0;
    sim().frame();
    CHECK_EQ(gApp->page, (int)PageComNav1);
}

TEST(the_left_wheel_follows_the_profiles_order_and_skips_hidden_pages) {
    PageTree t;
    std::string err;
    CHECK(parsePageLine("r \"Radio\"", t, err) && parseRowLine("r text \"MINE\"", t, err));
    gApp->profile.pages = t;
    gApp->profile.wheelOrder = {"@r", "com1"};
    gApp->profile.wheelHidden = {"adf", "dme", "xpdr", "ap", "vorhdg"};
    gApp->setPage(PageAdf);                                   // a hidden page is left at once
    gApp->nextDeviceUpdate = 0;
    sim().frame();
    CHECK_EQ(gApp->page, (int)kPageCount);
    CHECK_EQ(gApp->screen.l[0], std::string(">MINE"));
    gApp->stepPage(1);
    CHECK_EQ(gApp->page, (int)PageComNav1);
    gApp->stepPage(1);
    CHECK_EQ(gApp->page, (int)PageComNav2);                   // not named: after the named ones
    gApp->stepPage(1);
    CHECK_EQ(gApp->page, (int)PageButtons);
    gApp->stepPage(1);
    CHECK_EQ(gApp->page, (int)PageBrightness);
    gApp->stepPage(1);
    CHECK_EQ(gApp->page, (int)kPageCount);                    // and round again
    gApp->profile.wheelOrder.clear();
    gApp->profile.wheelHidden.clear();
    gApp->profile.pages = PageTree();
}

TEST(a_ready_made_profile_is_never_written_and_my_copy_takes_its_place) {
    std::string dir = sim().root + "/Output/preferences/X52Pro";
    std::string file = dir + "/aircraft-Baron_58.x52";
    {
        Profile p = defaultProfile();
        p.name = "Baron_58";
        p.standard = true;
        std::ofstream f(file);
        f << serialize(p);
    }
    std::string before = slurp(file);
    gApp->loadForAircraft("Baron_58");
    CHECK(gApp->profile.standard);
    gApp->profile.mfdBrightness = 40;                         // e.g. the Brightness page on the stick
    gApp->profileChanged();
    CHECK_EQ(slurp(file), before);                            // works now, never saved
    CHECK(has(gApp->notice, "this flight only"));
    std::string err;
    CHECK(!gApp->store.save("aircraft-Baron_58", gApp->profile, &err));   // the store refuses too

    std::string copy = gApp->saveCopy();                      // keep it: a copy, used from now on
    CHECK_EQ(copy, std::string("Baron_58 mine"));
    CHECK_EQ(gApp->profileKey(), copy);
    CHECK(!gApp->profile.standard);
    CHECK_EQ(gApp->profile.mfdBrightness, 40);
    CHECK_EQ(gApp->profile.basedOn, std::string("aircraft-Baron_58"));
    CHECK(has(slurp(dir + "/profile-choices.txt"), "aircraft-Baron_58\tBaron_58 mine\n"));
    gApp->loadForAircraft("Cessna_172SP");
    gApp->loadForAircraft("Baron_58");                        // next flight: still my copy
    CHECK_EQ(gApp->profileKey(), copy);
    gApp->chooseProfile("aircraft-Baron_58");                 // picking the ready-made one goes back to it
    gApp->chooseProfile("");
    CHECK_EQ(gApp->profileKey(), std::string("aircraft-Baron_58"));
    gApp->chooseProfile(copy);                                // and my copy again
    gApp->chooseProfile("");
    CHECK_EQ(gApp->profileKey(), copy);
    gApp->dropAircraftProfile();                              // deleting my copy: the ready-made one is back
    CHECK_EQ(gApp->profileKey(), std::string("aircraft-Baron_58"));
    gApp->dropAircraftProfile();                              // the ready-made one itself is never deleted
    CHECK_EQ(slurp(file), before);
    gApp->loadForAircraft("Cessna_172SP");
}

TEST(side_layout_shows_both_coms_on_the_first_radio_page) {
    gApp->profile.radioSide = true;
    gApp->setPage(PageComNav1);
    gApp->nextDeviceUpdate = 0;
    sim().frame();
    CHECK_EQ(gApp->screen.l[0].substr(0, 6), std::string("[COM1]"));
    sim().knownCommands.push_back("sim/radios/com1_standy_flip");
    int before = sim().onces["sim/radios/com1_standy_flip"];
    gApp->wheel(Wheel::Select);   // swaps COM1
    CHECK_EQ(sim().onces["sim/radios/com1_standy_flip"], before + 1);
    gApp->profile.radioSide = false;
    gApp->nextDeviceUpdate = 0;
    sim().frame();
    CHECK_EQ(gApp->screen.l[0].substr(0, 4), std::string("COM1"));
}

TEST(the_mode_dial_opens_the_first_page_only_when_the_orders_differ) {
    CHECK_EQ(gApp->engine.mode(), 1);
    gApp->setPage(PageXpdr);
    turnDial(28, 29);
    CHECK_EQ(gApp->engine.mode(), 2);
    CHECK_EQ(gApp->page, (int)PageXpdr);   // the same order in every mode: the page stays
    gApp->profile.modeWheel[2].own = true;
    gApp->profile.modeWheel[2].order = {"ap"};
    gApp->cursor.editing = true;
    turnDial(29, 30);
    CHECK_EQ(gApp->engine.mode(), 3);
    CHECK_EQ(gApp->page, (int)PageAutopilot);
    CHECK(!gApp->cursor.editing);
    gApp->stepPage(1);
    CHECK_EQ(gApp->page, (int)PageComNav1);   // mode 3's order goes on after AP
    gApp->setPage(PageDme);
    turnDial(30, 28);
    CHECK_EQ(gApp->page, (int)PageComNav1);   // back to the shared order's first page
    gApp->profile.modeWheel[2] = WheelSet();
}

TEST(a_file_reload_keeps_the_page_and_a_new_aircraft_starts_on_the_first) {
    gApp->setPage(PageXpdr);
    gApp->reload();
    CHECK_EQ(gApp->page, (int)PageXpdr);
    gApp->loadForAircraft("Cessna_172SP");
    CHECK_EQ(gApp->page, gApp->pageOrder()[0]);
}

TEST(a_layout_flip_while_editing_starts_the_cursor_over) {
    gApp->setPage(PageComNav1);
    gApp->cursor.item = 5;                  // classic: editing NAV1 fine
    gApp->cursor.editing = true;
    gApp->profile.radioSide = true;         // ticked in the plugin window, or a file reload
    gApp->nextDeviceUpdate = 0;
    sim().frame();
    CHECK(!gApp->cursor.editing);
    CHECK_EQ(gApp->cursor.item, 0);
    CHECK_EQ(gApp->screen.l[0].substr(0, 6), std::string("[COM1]"));
    gApp->cursor.item = 3;                  // side: editing COM2 standby
    gApp->cursor.editing = true;
    gApp->profile.radioSide = false;
    sim().knownCommands.push_back("sim/radios/nav1_standy_flip");
    int before = sim().onces["sim/radios/nav1_standy_flip"];
    gApp->wheel(Wheel::Select);             // classic item 3 would swap NAV1
    CHECK_EQ(sim().onces["sim/radios/nav1_standy_flip"], before);
    CHECK_EQ(gApp->cursor.item, 0);
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
