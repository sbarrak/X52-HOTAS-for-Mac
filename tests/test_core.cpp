#include <dirent.h>
#include <fstream>
#include <map>
#include <sstream>
#include "check.h"
#include <cmath>
#include "../src/core/axes.h"
#include "../src/core/catalog.h"
#include "../src/core/xpjoy.h"
#include "../src/core/pagetree.h"
#include "../src/core/pageview.h"
#include "../src/core/engine.h"
#include "../src/core/history.h"
#include "../src/core/update.h"
#include "../src/core/keys.h"
#include "../src/core/mfd.h"
#include "../src/core/pages.h"
#include "../src/core/profile.h"

using namespace x52;

static std::ostream& operator<<(std::ostream& o, const UsbCmd& c) {
    char buf[32];
    snprintf(buf, sizeof buf, "%02x:%04x", c.index, c.value);
    return o << buf;
}
static std::string dump(const std::vector<UsbCmd>& v) {
    std::string s;
    for (auto& c : v) {
        char buf[32];
        snprintf(buf, sizeof buf, "%02x:%04x ", c.index, c.value);
        s += buf;
    }
    return s;
}
static Action act(const std::string& text) {
    Action a;
    std::string err;
    if (!actionFromText(text, a, &err)) std::cout << "    bad action in test: " << err << "\n";
    return a;
}

// ---------------------------------------------------------------- profile

TEST(action_text_round_trips_commands_keys_and_waits) {
    Action a;
    std::string err;
    CHECK(actionFromText("cmd:sim/flight_controls/flaps_down ; wait:250 ; key:shift+f1", a, &err));
    CHECK_EQ(a.size(), (size_t)3);
    if (a.size() == 3) {
        CHECK(a[0].kind == StepKind::Command);
        CHECK_EQ(a[0].text, std::string("sim/flight_controls/flaps_down"));
        CHECK(a[1].kind == StepKind::Wait);
        CHECK_EQ(a[1].ms, 250);
        CHECK(a[2].kind == StepKind::Key);
        CHECK_EQ(a[2].text, std::string("shift+f1"));
    }
    CHECK_EQ(actionToText(a), std::string("cmd:sim/flight_controls/flaps_down ; wait:250 ; key:shift+f1"));
}

TEST(action_text_rejects_unknown_step_kind) {
    Action a;
    std::string err;
    CHECK(!actionFromText("dance:now", a, &err));
    CHECK(!err.empty());
}

TEST(find_prefers_exact_layer_over_any) {
    Profile p;
    p.edit(3, kLayerAny).press = act("cmd:any");
    p.edit(3, 2).press = act("cmd:mode2");
    const Binding* b2 = p.find(3, 2);
    const Binding* b1 = p.find(3, 1);
    CHECK(b2 && b2->press[0].text == "mode2");
    CHECK(b1 && b1->press[0].text == "any");
    CHECK(p.find(4, 1) == nullptr);
}

TEST(mode_pinkie_and_clutch_buttons_are_reserved) {
    Profile p;
    CHECK(p.isReserved(28));
    CHECK(p.isReserved(29));
    CHECK(p.isReserved(30));
    CHECK(p.isReserved(6));
    CHECK(p.isReserved(31));
    CHECK(!p.isReserved(3));
    p.pinkieShift = false;
    p.clutchSafe = false;
    CHECK(!p.isReserved(6));
    CHECK(!p.isReserved(31));
}

TEST(profile_survives_save_and_load) {
    Profile p;
    p.name = "Cessna 172";
    p.pinkieShift = false;
    p.holdMs = 650;
    p.mfdBrightness = 40;
    p.ledBrightness = 90;
    p.leds[1][LedA] = Color::Red;
    p.leds[2][LedPov] = Color::Amber;
    p.leds[0][LedFire] = Color::Off;
    Binding& b = p.edit(3, 5);
    b.label = "Gear \"down\" now";
    b.press = act("cmd:sim/flight_controls/landing_gear_down");
    b.holdLabel = "Gear up";
    b.hold = act("cmd:sim/flight_controls/landing_gear_up ; wait:100 ; key:ctrl+g");
    p.edit(7, kLayerAny).press = act("key:space");

    Profile q;
    std::string err;
    CHECK(parse(serialize(p), q, &err));
    CHECK_EQ(err, std::string(""));
    CHECK_EQ(q.name, std::string("Cessna 172"));
    CHECK(!q.pinkieShift);
    CHECK_EQ(q.holdMs, 650);
    CHECK_EQ(q.mfdBrightness, 40);
    CHECK_EQ(q.ledBrightness, 90);
    CHECK(q.leds[1][LedA] == Color::Red);
    CHECK(q.leds[2][LedPov] == Color::Amber);
    CHECK(q.leds[0][LedFire] == Color::Off);
    CHECK(q.leds[0][LedB] == Color::Green);
    const Binding* r = q.exact(3, 5);
    CHECK(r != nullptr);
    if (r) {
        CHECK_EQ(r->label, std::string("Gear 'down' now"));   // quotes cannot be stored
        CHECK_EQ(actionToText(r->press), std::string("cmd:sim/flight_controls/landing_gear_down"));
        CHECK_EQ(r->holdLabel, std::string("Gear up"));
        CHECK_EQ(actionToText(r->hold),
                 std::string("cmd:sim/flight_controls/landing_gear_up ; wait:100 ; key:ctrl+g"));
    }
    const Binding* s = q.exact(7, kLayerAny);
    CHECK(s && actionToText(s->press) == "key:space");
    CHECK_EQ(serialize(q), serialize(p));
}

TEST(parse_reports_the_bad_line) {
    Profile q;
    std::string err;
    CHECK(!parse("x52pro-profile 1\nname = a\nbind banana any press \"x\" cmd:y\n", q, &err));
    CHECK(err.find("line 3") != std::string::npos);
}

TEST(parse_ignores_comments_and_blank_lines) {
    Profile q;
    std::string err;
    CHECK(parse("# my profile\n\nx52pro-profile 1\nname = t\n  # indented comment\nhold_ms = 500\n", q, &err));
    CHECK_EQ(q.holdMs, 500);
}

TEST(prune_drops_bindings_without_actions) {
    Profile p;
    p.edit(3, 1).label = "nothing";
    p.edit(4, 1).press = act("cmd:x");
    p.prune();
    CHECK_EQ(p.bindings.size(), (size_t)1);
    CHECK(p.exact(3, 1) == nullptr);
}

TEST(default_profile_pages_the_screen_with_the_page_wheel) {
    Profile p = defaultProfile();
    const Binding* up = p.find(35, 1);
    const Binding* dn = p.find(36, 6);
    CHECK(up && actionToText(up->press) == "cmd:x52pro/mfd_page_prev");
    CHECK(dn && actionToText(dn->press) == "cmd:x52pro/mfd_page_next");
}

TEST(button_names_follow_the_x52_pro_layout) {
    CHECK_EQ(std::string(buttonName(1)), std::string("Trigger"));
    CHECK_EQ(std::string(buttonName(6)), std::string("Pinkie"));
    CHECK_EQ(std::string(buttonName(15)), std::string("Trigger 2nd stage"));
    CHECK_EQ(std::string(buttonName(28)), std::string("Mode 1"));
    CHECK_EQ(std::string(buttonName(31)), std::string("Clutch (i)"));
    CHECK_EQ(std::string(buttonName(39)), std::string("MFD Select"));
    CHECK_EQ(std::string(buttonName(40)), std::string("POV 1 up"));
    CHECK_EQ(std::string(buttonName(43)), std::string("POV 1 left"));
    CHECK_EQ(std::string(buttonName(44)), std::string("Mouse nub up"));
    CHECK_EQ(std::string(buttonName(51)), std::string("Mouse nub left (far)"));
    CHECK_EQ(std::string(buttonName(52)), std::string(""));
}

// ---------------------------------------------------------------- screen + lights

TEST(line_is_cleared_then_written_two_characters_per_request) {
    CHECK_EQ(dump(packLine(0, "AB")), std::string("d9:0000 d1:4241 "));
}

TEST(odd_length_line_is_padded_with_a_space) {
    CHECK_EQ(dump(packLine(1, "ABC")), std::string("da:0000 d2:4241 d2:2043 "));
}

TEST(third_line_uses_its_own_addresses) {
    CHECK_EQ(dump(packLine(2, "Hi")), std::string("dc:0000 d4:6948 "));
}

TEST(empty_line_is_only_cleared) {
    CHECK_EQ(dump(packLine(0, "   ")), std::string("d9:0000 "));
}

TEST(line_longer_than_the_screen_is_cut_to_16) {
    CHECK_EQ(packLine(0, "0123456789ABCDEFGHIJ").size(), (size_t)9);
    CHECK_EQ(mfdText("0123456789ABCDEFGHIJ"), std::string("0123456789ABCDEF"));
}

TEST(characters_the_screen_cannot_show_become_question_marks) {
    CHECK_EQ(mfdText("a\xC3\xA9\tb"), std::string("a???b"));
}

TEST(two_colour_light_sets_red_and_green_halves) {
    std::vector<UsbCmd> v;
    packLed(LedA, Color::Amber, v);
    CHECK_EQ(dump(v), std::string("b8:0201 b8:0301 "));
    v.clear();
    packLed(LedA, Color::Red, v);
    CHECK_EQ(dump(v), std::string("b8:0201 b8:0300 "));
    v.clear();
    packLed(LedClutch, Color::Green, v);
    CHECK_EQ(dump(v), std::string("b8:1200 b8:1301 "));
    v.clear();
    packLed(LedPov, Color::Off, v);
    CHECK_EQ(dump(v), std::string("b8:1000 b8:1100 "));
}

TEST(single_colour_light_is_on_or_off) {
    std::vector<UsbCmd> v;
    packLed(LedFire, Color::Green, v);
    packLed(LedThrottle, Color::Off, v);
    CHECK_EQ(dump(v), std::string("b8:0101 b8:1400 "));
}

TEST(brightness_shift_and_clock_requests) {
    CHECK_EQ(packMfdBrightness(100), (UsbCmd{0xb1, 100}));
    CHECK_EQ(packLedBrightness(300), (UsbCmd{0xb2, 127}));
    CHECK_EQ(packShift(true), (UsbCmd{0xfd, 0x51}));
    CHECK_EQ(packShift(false), (UsbCmd{0xfd, 0x50}));
    CHECK_EQ(packClock(13, 45, true), (UsbCmd{0xc0, (uint16_t)(0x8000 | (13 << 8) | 45)}));
    CHECK_EQ(packDate(16, 11, 26, 0)[0], (UsbCmd{0xc4, (uint16_t)((11 << 8) | 16)}));
    CHECK_EQ(packDate(16, 11, 26, 0)[1], (UsbCmd{0xc8, 26}));
}

TEST(com_nav_page_shows_active_and_standby) {
    SimData d;
    d.com1 = 118000;
    d.com1s = 121500;
    d.nav1 = 11050;
    d.nav1s = 11390;
    Lines p = formatSimPage(PageComNav1, d);
    CHECK_EQ(p.l[0], std::string("COM1 [118.000]"));
    CHECK_EQ(p.l[1], std::string("STBY  121.500"));
    CHECK_EQ(p.l[2], std::string("N110.50  113.90v"));
}

TEST(second_com_nav_page_uses_radio_two) {
    SimData d;
    d.com2 = 127275;
    d.com2s = 135000;
    d.nav2 = 10800;
    d.nav2s = 11795;
    Lines p = formatSimPage(PageComNav2, d);
    PageCursor nav;
    nav.item = 4;
    p = formatSimPage(PageComNav2, d, nav);
    CHECK_EQ(p.l[0], std::string("NAV2  108.00"));
    CHECK_EQ(p.l[1], std::string("STBY >117.95"));
    CHECK_EQ(p.l[2], std::string("C127.275 135.000"));
}

TEST(transponder_page_shows_code_and_mode) {
    SimData d;
    d.xpdrCode = 1200;
    d.xpdrMode = 3;
    Lines p = formatSimPage(PageXpdr, d);
    CHECK_EQ(p.l[0], std::string("XPDR 1200"));
    CHECK_EQ(p.l[1], std::string("     ^"));
    CHECK_EQ(p.l[2], std::string("MODE  ALT"));
}

TEST(autopilot_page_fits_sixteen_columns) {
    SimData d;
    d.apHdg = 269.6f;
    d.apAlt = 45000;
    d.apVs = -1500;
    d.apIas = 250;
    d.apOn = true;
    Lines p = formatSimPage(PageAutopilot, d);
    CHECK_EQ(p.l[0], std::string("HDG 270 ALT45000"));
    CHECK_EQ(p.l[1], std::string("VS -1500 IAS 250"));
    CHECK_EQ(p.l[2], std::string(">HDG 270      AP"));
}

TEST(dme_page_shows_dashes_without_a_signal) {
    SimData d;
    d.dme1 = true;
    d.dme1nm = 12.34f;
    d.dme1kt = 120;
    Lines p = formatSimPage(PageDme, d);
    CHECK_EQ(p.l[0], std::string("DME1 12.3nm 120k"));
    CHECK_EQ(p.l[1], std::string("DME2 --.-nm ---k"));
}

TEST(every_page_line_fits_the_screen) {
    SimData d;
    d.com1 = d.com1s = d.com2 = d.com2s = 136975;
    d.nav1 = d.nav1s = d.nav2 = d.nav2s = 11795;
    d.adf1 = d.adf1s = d.adf2 = d.adf2s = 1799;
    d.dme1 = d.dme2 = true;
    d.dme1nm = d.dme2nm = 399.9f;
    d.dme1kt = d.dme2kt = 999;
    d.xpdrCode = 7777;
    d.apHdg = 359;
    d.apAlt = 51000;
    d.apVs = -9900;
    d.apIas = 399;
    d.obs1 = d.obs2 = 359;
    for (int page = PageComNav1; page < PageButtons; ++page)
        for (int item = 0; item < std::max(1, pageItemCount(page)); ++item)
            for (bool editing : {false, true}) {
                PageCursor c;
                c.item = item;
                c.editing = editing;
                Lines p = formatSimPage(page, d, c);
                for (int i = 0; i < kMfdLines; ++i) CHECK(p.l[i].size() <= (size_t)kMfdCols);
            }
}

TEST(buttons_page_shows_profile_label_and_mode) {
    Lines p = formatButtonsPage("Cessna 172 Skyhawk SP", "Gear down", 2, true);
    CHECK_EQ(p.l[0], std::string("Cessna 172 Skyha"));
    CHECK_EQ(p.l[1], std::string("Gear down"));
    CHECK_EQ(p.l[2], std::string("Mode 2 + Pinkie"));
}

// ---------------------------------------------------------------- keys

TEST(key_combo_with_modifiers_parses) {
    KeyCombo k;
    CHECK(parseKey("Shift+F1", k));
    CHECK_EQ(k.keycode, 122);
    CHECK(k.shift && !k.ctrl && !k.alt && !k.cmd);
    CHECK(parseKey("ctrl+alt+g", k));
    CHECK_EQ(k.keycode, 5);
    CHECK(k.ctrl && k.alt && !k.shift);
    CHECK(parseKey("space", k));
    CHECK_EQ(k.keycode, 49);
    CHECK(parseKey("cmd+up", k));
    CHECK_EQ(k.keycode, 126);
    CHECK(k.cmd);
}

TEST(unknown_key_is_rejected) {
    KeyCombo k;
    CHECK(!parseKey("shift+banana", k));
    CHECK(!parseKey("", k));
    CHECK(!parseKey("shift+", k));
}

TEST(combo_text_is_canonical) {
    KeyCombo k;
    CHECK(parseKey("SHIFT+ctrl+F12", k));
    CHECK_EQ(comboText(k), std::string("ctrl+shift+f12"));
}

// ---------------------------------------------------------------- engine

struct Rec : Host {
    std::vector<std::string> log;
    void commandBegin(const std::string& n) override { log.push_back("begin:" + n); }
    void commandEnd(const std::string& n) override { log.push_back("end:" + n); }
    void commandOnce(const std::string& n) override { log.push_back("once:" + n); }
    void keyDown(const std::string& c) override { log.push_back("kd:" + c); }
    void keyUp(const std::string& c) override { log.push_back("ku:" + c); }
    void showLabel(const std::string&) override {}
    void layerChanged(int, bool) override {}
    std::string str() {
        std::string s;
        for (auto& l : log) s += l + " ";
        log.clear();
        return s;
    }
};
struct LabelRec : Rec {
    std::vector<std::string> labels;
    std::vector<std::string> layers;
    void showLabel(const std::string& t) override { labels.push_back(t); }
    void layerChanged(int m, bool s) override { layers.push_back(std::to_string(m) + (s ? "s" : "")); }
};

TEST(single_command_is_held_for_as_long_as_the_button) {
    Rec h;
    Engine e(h);
    Profile p;
    p.edit(3, kLayerAny).press = act("cmd:brakes");
    e.setProfile(p);
    e.buttonEvent(3, true, 0.0);
    CHECK_EQ(h.str(), std::string("begin:brakes "));
    e.tick(5.0);
    CHECK_EQ(h.str(), std::string(""));
    e.buttonEvent(3, false, 5.1);
    CHECK_EQ(h.str(), std::string("end:brakes "));
}

TEST(single_key_is_held_for_as_long_as_the_button) {
    Rec h;
    Engine e(h);
    Profile p;
    p.edit(3, kLayerAny).press = act("key:shift+f1");
    e.setProfile(p);
    e.buttonEvent(3, true, 0.0);
    e.buttonEvent(3, false, 0.2);
    CHECK_EQ(h.str(), std::string("kd:shift+f1 ku:shift+f1 "));
}

TEST(unassigned_button_shows_its_standard_name) {
    LabelRec h;
    Engine e(h);
    e.setProfile(Profile());
    e.buttonEvent(3, true, 0.0);
    e.buttonEvent(3, false, 0.1);
    CHECK_EQ(h.str(), std::string(""));
    CHECK_EQ(h.labels.size(), (size_t)1);
    if (!h.labels.empty()) CHECK_EQ(h.labels[0], std::string("A"));
}

TEST(latched_clutch_stays_on_until_pressed_again) {
    Rec h;
    Engine e(h);
    Profile p;
    p.clutchLatched = true;
    p.edit(3, kLayerAny).press = act("cmd:gear");
    e.setProfile(p);
    e.buttonEvent(31, true, 0.0);
    e.buttonEvent(31, false, 0.1);
    CHECK(e.clutch());
    e.buttonEvent(3, true, 0.2);
    e.buttonEvent(3, false, 0.3);
    CHECK_EQ(h.str(), std::string(""));
    e.setProfile(p);   // a reload must not drop the latch
    CHECK(e.clutch());
    e.buttonEvent(31, true, 0.4);
    e.buttonEvent(31, false, 0.5);
    CHECK(!e.clutch());
    e.buttonEvent(3, true, 0.6);
    CHECK_EQ(h.str(), std::string("begin:gear "));
}

TEST(unassigned_button_does_nothing) {
    Rec h;
    Engine e(h);
    e.setProfile(Profile());
    e.buttonEvent(3, true, 0.0);
    e.buttonEvent(3, false, 0.1);
    CHECK_EQ(h.str(), std::string(""));
}

TEST(mode_switch_selects_the_layer) {
    LabelRec h;
    Engine e(h);
    Profile p;
    p.edit(3, 1).press = act("cmd:one");
    p.edit(3, 2).press = act("cmd:two");
    e.setProfile(p);
    CHECK_EQ(e.mode(), 1);
    e.buttonEvent(28, false, 0.0);
    e.buttonEvent(29, true, 0.0);
    e.tick(0.2);
    CHECK_EQ(e.mode(), 2);
    CHECK_EQ(e.layer(), 2);
    e.buttonEvent(3, true, 0.3);
    e.buttonEvent(3, false, 0.4);
    CHECK_EQ(h.str(), std::string("begin:two end:two "));
    CHECK_EQ(h.layers.size(), (size_t)1);
    if (!h.layers.empty()) CHECK_EQ(h.layers[0], std::string("2"));
}

TEST(pinkie_shifts_to_the_upper_layers) {
    LabelRec h;
    Engine e(h);
    Profile p;
    p.edit(3, 1).press = act("cmd:plain");
    p.edit(3, 4).press = act("cmd:shifted");
    e.setProfile(p);
    e.buttonEvent(6, true, 0.0);
    CHECK(e.shift());
    CHECK_EQ(e.layer(), 4);
    e.buttonEvent(3, true, 0.1);
    e.buttonEvent(3, false, 0.2);
    e.buttonEvent(6, false, 0.3);
    CHECK(!e.shift());
    e.buttonEvent(3, true, 0.4);
    e.buttonEvent(3, false, 0.5);
    CHECK_EQ(h.str(), std::string("begin:shifted end:shifted begin:plain end:plain "));
}

TEST(pinkie_is_an_ordinary_button_when_shift_is_turned_off) {
    Rec h;
    Engine e(h);
    Profile p;
    p.pinkieShift = false;
    p.edit(6, kLayerAny).press = act("cmd:pinkie");
    e.setProfile(p);
    e.buttonEvent(6, true, 0.0);
    CHECK(!e.shift());
    CHECK_EQ(h.str(), std::string("begin:pinkie "));
}

TEST(release_ends_what_the_press_started_even_if_the_layer_changed) {
    Rec h;
    Engine e(h);
    Profile p;
    p.edit(3, 1).press = act("cmd:one");
    p.edit(3, 4).press = act("cmd:shifted");
    e.setProfile(p);
    e.buttonEvent(3, true, 0.0);
    e.buttonEvent(6, true, 0.1);
    e.buttonEvent(3, false, 0.2);
    CHECK_EQ(h.str(), std::string("begin:one end:one "));
}

TEST(macro_runs_its_steps_in_order_and_honours_waits) {
    Rec h;
    Engine e(h);
    Profile p;
    p.edit(3, kLayerAny).press = act("cmd:a ; wait:200 ; cmd:b ; key:g ; wait:100 ; cmd:c");
    e.setProfile(p);
    e.buttonEvent(3, true, 1.0);
    CHECK_EQ(h.str(), std::string("once:a "));
    e.buttonEvent(3, false, 1.05);
    e.tick(1.1);
    CHECK_EQ(h.str(), std::string(""));
    e.tick(1.21);
    CHECK_EQ(h.str(), std::string("once:b kd:g ku:g "));
    e.tick(1.25);
    CHECK_EQ(h.str(), std::string(""));
    e.tick(1.32);
    CHECK_EQ(h.str(), std::string("once:c "));
    e.tick(9.0);
    CHECK_EQ(h.str(), std::string(""));
}

TEST(short_tap_runs_the_press_action_when_a_hold_action_exists) {
    Rec h;
    Engine e(h);
    Profile p;
    Binding& b = p.edit(3, kLayerAny);
    b.press = act("cmd:tap");
    b.hold = act("cmd:long");
    e.setProfile(p);
    e.buttonEvent(3, true, 0.0);
    e.tick(0.1);
    CHECK_EQ(h.str(), std::string(""));
    e.buttonEvent(3, false, 0.2);
    CHECK_EQ(h.str(), std::string("once:tap "));
}

TEST(long_hold_runs_the_hold_action_and_not_the_tap) {
    Rec h;
    Engine e(h);
    Profile p;
    Binding& b = p.edit(3, kLayerAny);
    b.press = act("cmd:tap");
    b.hold = act("cmd:long");
    e.setProfile(p);
    e.buttonEvent(3, true, 0.0);
    e.tick(0.39);
    CHECK_EQ(h.str(), std::string(""));
    e.tick(0.41);
    CHECK_EQ(h.str(), std::string("begin:long "));
    e.tick(0.8);
    CHECK_EQ(h.str(), std::string(""));
    e.buttonEvent(3, false, 1.0);
    CHECK_EQ(h.str(), std::string("end:long "));
}

TEST(hold_only_binding_ignores_a_short_tap) {
    Rec h;
    Engine e(h);
    Profile p;
    p.edit(3, kLayerAny).hold = act("cmd:long");
    e.setProfile(p);
    e.buttonEvent(3, true, 0.0);
    e.buttonEvent(3, false, 0.1);
    e.tick(1.0);
    CHECK_EQ(h.str(), std::string(""));
}

TEST(clutch_shows_the_label_without_running_the_action) {
    LabelRec h;
    Engine e(h);
    Profile p;
    Binding& b = p.edit(3, kLayerAny);
    b.label = "Gear";
    b.press = act("cmd:gear");
    e.setProfile(p);
    e.buttonEvent(31, true, 0.0);
    CHECK(e.clutch());
    e.buttonEvent(3, true, 0.1);
    e.buttonEvent(3, false, 0.2);
    CHECK_EQ(h.str(), std::string(""));
    CHECK_EQ(h.labels.size(), (size_t)1);
    if (!h.labels.empty()) CHECK_EQ(h.labels[0], std::string("Gear"));
    e.buttonEvent(31, false, 0.3);
    e.buttonEvent(3, true, 0.4);
    CHECK_EQ(h.str(), std::string("begin:gear "));
}

TEST(label_falls_back_to_the_button_name) {
    LabelRec h;
    Engine e(h);
    Profile p;
    p.edit(3, kLayerAny).press = act("cmd:x");
    e.setProfile(p);
    e.buttonEvent(3, true, 0.0);
    CHECK_EQ(h.labels.size(), (size_t)1);
    if (!h.labels.empty()) CHECK_EQ(h.labels[0], std::string("A"));
}

TEST(changing_profile_releases_held_commands) {
    Rec h;
    Engine e(h);
    Profile p;
    p.edit(3, kLayerAny).press = act("cmd:brakes");
    e.setProfile(p);
    e.buttonEvent(3, true, 0.0);
    h.str();
    e.setProfile(Profile());
    CHECK_EQ(h.str(), std::string("end:brakes "));
    e.buttonEvent(3, false, 0.5);
    CHECK_EQ(h.str(), std::string(""));
}

TEST(mode_and_shift_survive_a_profile_change) {
    Rec h;
    Engine e(h);
    e.setProfile(Profile());
    e.buttonEvent(30, true, 0.0);
    e.tick(0.2);
    e.setProfile(Profile());
    CHECK_EQ(e.mode(), 3);
}

TEST(brief_blip_of_another_dial_position_does_not_change_the_mode) {
    LabelRec h;
    Engine e(h);
    Profile p;
    p.edit(3, 1).press = act("cmd:one");
    p.edit(3, 2).press = act("cmd:two");
    e.setProfile(p);
    e.buttonEvent(28, true, 0.0);
    e.tick(1.0);
    e.buttonEvent(28, false, 2.00);   // worn dial contact: flickers to position 2 for 30 ms
    e.buttonEvent(29, true, 2.00);
    e.tick(2.02);
    e.buttonEvent(29, false, 2.03);
    e.buttonEvent(28, true, 2.03);
    e.tick(3.0);
    CHECK_EQ(e.mode(), 1);
    CHECK_EQ(h.layers.size(), (size_t)0);
    e.buttonEvent(3, true, 3.1);
    CHECK_EQ(h.str(), std::string("begin:one "));
}

TEST(mode_changes_once_the_dial_has_settled) {
    Rec h;
    Engine e(h);
    e.setProfile(Profile());
    e.buttonEvent(29, true, 0.0);
    e.tick(0.05);
    CHECK_EQ(e.mode(), 1);
    e.tick(0.11);
    CHECK_EQ(e.mode(), 2);
}

TEST(settle_time_is_saved_in_the_profile) {
    Profile p;
    CHECK_EQ(p.modeSettleMs, 100);
    p.modeSettleMs = 250;
    Profile q;
    std::string err;
    CHECK(parse(serialize(p), q, &err));
    CHECK_EQ(q.modeSettleMs, 250);
}

TEST(moving_controls_are_named_as_found_on_the_real_stick) {
    CHECK_EQ(std::string(controlName(0x01, 0x32)), std::string("Throttle"));
    CHECK_EQ(std::string(controlName(0x01, 0x33)), std::string("Lower rotary (i)"));
    CHECK_EQ(std::string(controlName(0x01, 0x34)), std::string("Upper rotary (E)"));
    CHECK_EQ(std::string(controlName(0x01, 0x36)), std::string("Slider"));
    CHECK_EQ(std::string(controlName(0x01, 0x39)), std::string("POV 1 (hat switch)"));
    CHECK_EQ(std::string(controlName(0x05, 0x24)), std::string("Mouse nub L/R"));
    CHECK_EQ(std::string(controlName(0x7f, 0x01)), std::string("Unknown control"));
}

TEST(axis_position_is_shown_as_a_percentage) {
    CHECK_EQ(controlValueText(0x01, 0x32, 0, 0, 255), std::string("0 %"));
    CHECK_EQ(controlValueText(0x01, 0x32, 128, 0, 255), std::string("50 %"));
    CHECK_EQ(controlValueText(0x01, 0x30, 1023, 0, 1023), std::string("100 %"));
}

TEST(hat_position_is_shown_as_a_direction) {
    CHECK_EQ(controlValueText(0x01, 0x39, 1, 1, 8), std::string("up"));
    CHECK_EQ(controlValueText(0x01, 0x39, 3, 1, 8), std::string("right"));
    CHECK_EQ(controlValueText(0x01, 0x39, 8, 1, 8), std::string("up-left"));
    CHECK_EQ(controlValueText(0x01, 0x39, 0, 1, 8), std::string("centre"));
    CHECK_EQ(controlValueText(0x01, 0x39, 15, 1, 8), std::string("centre"));
}

TEST(small_axis_jitter_is_not_worth_showing) {
    CHECK(!controlMoved(0x01, 0x32, 100, 102, 0, 255));
    CHECK(controlMoved(0x01, 0x32, 100, 106, 0, 255));
    CHECK(controlMoved(0x01, 0x39, 1, 2, 1, 8));
    CHECK(controlMoved(0x05, 0x24, 8, 9, 0, 15));
}

// ---- wheel cursor on the radio pages

static std::string cmds(const std::vector<std::string>& v) {
    std::string s;
    for (auto& c : v) s += c + " ";
    return s;
}

TEST(turning_the_wheel_only_moves_the_cursor_until_select_is_pressed) {
    PageCursor c;
    for (int i = 0; i < 20; ++i) CHECK_EQ(cmds(pageInput(PageComNav1, c, Wheel::Down)), std::string(""));
    for (int i = 0; i < 20; ++i) CHECK_EQ(cmds(pageInput(PageXpdr, c, Wheel::Up)), std::string(""));
}

TEST(select_on_the_active_frequency_swaps_it_with_standby) {
    PageCursor c;
    CHECK_EQ(cmds(pageInput(PageComNav1, c, Wheel::Select)), std::string("sim/radios/com1_standy_flip "));
    CHECK(!c.editing);
    c.item = 3;
    CHECK_EQ(cmds(pageInput(PageComNav2, c, Wheel::Select)), std::string("sim/radios/nav2_standy_flip "));
}

TEST(standby_frequency_is_edited_in_two_parts_as_in_the_manual) {
    SimData d;
    d.com1 = 118000;
    d.com1s = 121500;
    PageCursor c;
    pageInput(PageComNav1, c, Wheel::Down);
    CHECK_EQ(formatSimPage(PageComNav1, d, c).l[1], std::string("STBY >121.500"));
    pageInput(PageComNav1, c, Wheel::Select);
    CHECK_EQ(formatSimPage(PageComNav1, d, c).l[1], std::string("STBY>>121.500"));
    CHECK_EQ(cmds(pageInput(PageComNav1, c, Wheel::Up)), std::string("sim/radios/stby_com1_coarse_up "));
    CHECK_EQ(cmds(pageInput(PageComNav1, c, Wheel::Down)), std::string("sim/radios/stby_com1_coarse_down "));
    pageInput(PageComNav1, c, Wheel::Select);
    CHECK(!c.editing);
    pageInput(PageComNav1, c, Wheel::Down);
    CHECK_EQ(formatSimPage(PageComNav1, d, c).l[1], std::string("STBY  121.500<"));
    pageInput(PageComNav1, c, Wheel::Select);
    CHECK_EQ(formatSimPage(PageComNav1, d, c).l[1], std::string("STBY  121.500<<"));
    CHECK_EQ(cmds(pageInput(PageComNav1, c, Wheel::Up)), std::string("sim/radios/stby_com1_fine_up "));
    CHECK_EQ(cmds(pageInput(PageComNav1, c, Wheel::Up, true)), std::string("sim/radios/stby_com1_fine_up_833 "));
}

TEST(cursor_wraps_round_the_page) {
    PageCursor c;
    pageInput(PageComNav1, c, Wheel::Up);
    CHECK_EQ(c.item, 5);
    pageInput(PageComNav1, c, Wheel::Down);
    CHECK_EQ(c.item, 0);
}

TEST(pages_with_nothing_to_change_ignore_the_wheel) {
    for (int page : {(int)PageButtons, (int)PageDme}) {
        PageCursor c;
        for (Wheel w : {Wheel::Up, Wheel::Down, Wheel::Select}) CHECK_EQ(cmds(pageInput(page, c, w)), std::string(""));
        CHECK(!c.editing);
        CHECK_EQ(c.item, 0);
    }
}

TEST(a_cursor_left_over_from_a_longer_page_is_brought_back_in_range) {
    PageCursor c;
    c.item = 7;
    c.editing = true;
    CHECK_EQ(cmds(pageInput(PageVorHdg, c, Wheel::Up)), std::string("sim/autopilot/heading_up "));
    CHECK_EQ(c.item, 2);
}

TEST(transponder_digits_and_mode_are_edited_one_at_a_time) {
    SimData d;
    d.xpdrCode = 1200;
    d.xpdrMode = 1;
    PageCursor c;
    c.item = 2;
    pageInput(PageXpdr, c, Wheel::Select);
    CHECK_EQ(formatSimPage(PageXpdr, d, c).l[1], std::string("       ^   EDIT"));
    CHECK_EQ(cmds(pageInput(PageXpdr, c, Wheel::Up)), std::string("sim/transponder/transponder_tens_up "));
    c.item = 4;
    CHECK_EQ(formatSimPage(PageXpdr, d, c).l[2], std::string("MODE>>STBY"));
    CHECK_EQ(cmds(pageInput(PageXpdr, c, Wheel::Down)), std::string("sim/transponder/transponder_dn "));
}

TEST(autopilot_values_are_edited_from_the_third_line) {
    SimData d;
    d.apAlt = 5000;
    PageCursor c;
    c.item = 1;
    c.editing = true;
    CHECK_EQ(formatSimPage(PageAutopilot, d, c).l[2], std::string(">>ALT 5000    --"));
    CHECK_EQ(cmds(pageInput(PageAutopilot, c, Wheel::Up)), std::string("sim/autopilot/altitude_up "));
    c.item = 2;
    CHECK_EQ(cmds(pageInput(PageAutopilot, c, Wheel::Down)), std::string("sim/autopilot/vertical_speed_down "));
}

TEST(adf_tens_and_ones_are_marked_with_their_step) {
    SimData d;
    d.adf2 = 365;
    d.adf2s = 410;
    PageCursor c;
    c.item = 6;
    Lines p = formatSimPage(PageAdf, d, c);
    CHECK_EQ(p.l[0], std::string("ADF2   365"));
    CHECK_EQ(p.l[1], std::string("STBY   410<10"));
    c.editing = true;
    CHECK_EQ(cmds(pageInput(PageAdf, c, Wheel::Up)), std::string("sim/radios/stby_adf2_tens_up "));
}

// ---- clock, date, flashing

TEST(date_order_decides_which_number_goes_where) {
    CHECK_EQ(packDate(16, 11, 26, 1)[0], (UsbCmd{0xc4, (uint16_t)((16 << 8) | 11)}));
    CHECK_EQ(packDate(16, 11, 26, 1)[1], (UsbCmd{0xc8, 26}));
    CHECK_EQ(packDate(16, 11, 26, 2)[0], (UsbCmd{0xc4, (uint16_t)((11 << 8) | 26)}));
    CHECK_EQ(packDate(16, 11, 26, 2)[1], (UsbCmd{0xc8, 16}));
}

TEST(second_and_third_clock_are_offsets_from_the_first) {
    CHECK_EQ(packClockOffset(2, 60, true), (UsbCmd{0xc1, (uint16_t)(0x8000 | 60)}));
    CHECK_EQ(packClockOffset(3, -180, false), (UsbCmd{0xc2, (uint16_t)(0x400 | 180)}));
    CHECK_EQ(packClockOffset(2, 0, false), (UsbCmd{0xc1, 0}));
}

TEST(clock_1_moves_away_from_gmt_and_carries_into_the_date) {
    ClockTime g;
    g.hour = 23, g.minute = 30, g.day = 31, g.month = 12, g.year2 = 26;
    ClockTime t = shiftClock(g, 90);                 // past midnight on New Year's Eve
    CHECK_EQ(t.hour, 1);
    CHECK_EQ(t.minute, 0);
    CHECK_EQ(t.day, 1);
    CHECK_EQ(t.month, 1);
    CHECK_EQ(t.year2, 27);
    g.hour = 2, g.minute = 0, g.day = 1, g.month = 3, g.year2 = 28;   // 2028 is a leap year
    t = shiftClock(g, -180);
    CHECK_EQ(t.hour, 23);
    CHECK_EQ(t.day, 29);
    CHECK_EQ(t.month, 2);
    t = shiftClock(g, 0);
    CHECK_EQ(t.hour, 2);
    CHECK_EQ(t.day, 1);
}

TEST(clock_2_and_3_keep_their_meaning_when_clock_1_is_not_gmt) {
    CHECK_EQ(clockOffsetFromFirst(60, 0), 60);       // clock 1 GMT: as before
    CHECK_EQ(clockOffsetFromFirst(0, 60), -60);      // clock 2 GMT, clock 1 one hour ahead
    CHECK_EQ(clockOffsetFromFirst(-720, 840), -120); // 26 hours behind shows the same as 2 hours behind, and fits the stick
    CHECK_EQ(clockOffsetFromFirst(840, -720), 120);
    CHECK(clockOffsetFromFirst(840, -300) >= -720 && clockOffsetFromFirst(840, -300) < 720);
    CHECK_EQ(localMinusGmt(1, 0, 23, 0), 120);       // local 01:00, GMT 23:00 the day before
    CHECK_EQ(localMinusGmt(20, 30, 1, 0), -270);
}

TEST(clock_1_source_picks_how_far_it_is_from_gmt) {
    Profile p;
    CHECK_EQ(clock1FromGmt(p, 60, true, 120), 0);    // GMT, the usual
    p.clock1Source = ClockSimLocal;
    CHECK_EQ(clock1FromGmt(p, 60, true, 120), 120);
    CHECK_EQ(clock1FromGmt(p, 60, false, 0), 60);    // no simulator: the Mac's local time
    p.clock1Source = ClockMacLocal;
    CHECK_EQ(clock1FromGmt(p, 60, true, 120), 60);
    p.clock1Source = ClockFixed;
    p.clock1Offset = -300;
    CHECK_EQ(clock1FromGmt(p, 60, true, 120), -300);
}

TEST(clock_1_source_survives_save_and_load_and_older_files_stay_gmt) {
    Profile p;
    p.clock1Source = ClockFixed;
    p.clock1Offset = 330;
    Profile q;
    std::string err;
    CHECK(parse(serialize(p), q, &err));
    CHECK_EQ(q.clock1Source, (int)ClockFixed);
    CHECK_EQ(q.clock1Offset, 330);
    Profile r;
    CHECK(parse("x52pro-profile 3\nclock2_offset = 60\n", r, &err));
    CHECK_EQ(r.clock1Source, (int)ClockGmt);
    CHECK(parse("x52pro-profile 3\nclock1_source = 9\nclock1_offset = 99999\n", r, &err));
    CHECK_EQ(r.clock1Source, (int)ClockFixed);
    CHECK_EQ(r.clock1Offset, kClockOffsetMax);
}

TEST(flashing_the_clutch_lights_has_its_own_request) {
    CHECK_EQ(packBlink(true), (UsbCmd{0xb4, 0x51}));
    CHECK_EQ(packBlink(false), (UsbCmd{0xb4, 0x50}));
}

TEST(clock_and_clutch_settings_survive_save_and_load) {
    Profile p;
    p.clutchLatched = true;
    p.clockOffset[0] = -180;
    p.clockOffset[1] = 330;
    p.clock12h[1] = true;
    p.dateFormat = 2;
    p.com833 = true;
    Profile q;
    std::string err;
    CHECK(parse(serialize(p), q, &err));
    CHECK(q.clutchLatched);
    CHECK_EQ(q.clockOffset[0], -180);
    CHECK_EQ(q.clockOffset[1], 330);
    CHECK(!q.clock12h[0]);
    CHECK(q.clock12h[1]);
    CHECK_EQ(q.dateFormat, 2);
    CHECK(q.com833);
    CHECK_EQ(q.version, 10);
}

TEST(out_of_range_clock_settings_are_pulled_into_range) {
    Profile q;
    std::string err;
    CHECK(parse("x52pro-profile 2\nclock2_offset = 99999\nclock3_offset = -99999\ndate_format = 7\n", q, &err));
    CHECK_EQ(q.clockOffset[0], kClockOffsetMax);
    CHECK_EQ(q.clockOffset[1], kClockOffsetMin);
    CHECK_EQ(q.dateFormat, 2);
}

TEST(profile_from_the_older_plugin_gains_the_wheel_bindings_once) {
    Profile q;
    std::string err;
    CHECK(parse("x52pro-profile 1\nname = old\nbind 39 any press \"Mine\" cmd:my/command\n", q, &err));
    CHECK_EQ(q.version, 1);
    CHECK(!q.clutchLatched);
    CHECK(q.exact(37, kLayerAny) != nullptr);   // added on reading, whoever saves first
    CHECK(q.exact(38, kLayerAny) != nullptr);
    CHECK_EQ(actionToText(q.exact(39, kLayerAny)->press), std::string("cmd:my/command"));   // theirs is kept
    CHECK(!addWheelBindings(q));
    Profile again;
    CHECK(parse(serialize(q), again, &err));
    CHECK_EQ(again.version, 10);
}

TEST(profile_from_before_pov_1_gains_the_look_around_bindings_once) {
    Profile q;
    std::string err;
    CHECK(parse("x52pro-profile 4\nname = mine\nbind 41 any press \"Mine\" cmd:my/command\n", q, &err));
    CHECK_EQ(q.version, 4);
    CHECK_EQ(actionToText(q.exact(40, kLayerAny)->press), std::string("cmd:sim/general/rot_up"));
    CHECK_EQ(actionToText(q.exact(42, kLayerAny)->press), std::string("cmd:sim/general/rot_down"));
    CHECK_EQ(actionToText(q.exact(43, kLayerAny)->press), std::string("cmd:sim/general/rot_left"));
    CHECK_EQ(actionToText(q.exact(41, kLayerAny)->press), std::string("cmd:my/command"));   // theirs is kept
    CHECK(!addLookBindings(q));
    Profile again;
    CHECK(parse(serialize(q), again, &err));
    CHECK_EQ(again.version, 10);
    CHECK(defaultProfile().exact(40, kLayerAny) != nullptr);
    // a current profile whose owner removed the look-around binding keeps it removed
    q.edit(40, kLayerAny).press.clear();
    q.prune();
    CHECK(parse(serialize(q), again, &err));
    CHECK(again.find(40, 1) == nullptr);
}

TEST(profile_from_before_the_nub_gains_the_pan_bindings_once) {
    Profile q;
    std::string err;
    CHECK(parse("x52pro-profile 5\nname = mine\nbind 45 any press \"Mine\" cmd:my/command\n", q, &err));
    CHECK_EQ(q.version, 5);
    CHECK_EQ(actionToText(q.exact(kNubUp, kLayerAny)->press), std::string("cmd:sim/general/up"));
    CHECK_EQ(actionToText(q.exact(kNubUp + 2, kLayerAny)->press), std::string("cmd:sim/general/down"));
    CHECK_EQ(actionToText(q.exact(kNubUp + 4, kLayerAny)->press), std::string("cmd:sim/general/up_fast"));
    CHECK_EQ(actionToText(q.exact(kNubUp + 7, kLayerAny)->press), std::string("cmd:sim/general/left_fast"));
    CHECK_EQ(actionToText(q.exact(kNubUp + 1, kLayerAny)->press), std::string("cmd:my/command"));   // theirs is kept
    CHECK(!addNubBindings(q));
    Profile again;
    CHECK(parse(serialize(q), again, &err));
    CHECK_EQ(again.version, 10);
    CHECK(defaultProfile().exact(kNubUp, kLayerAny) != nullptr);
}

TEST(profile_from_1_2_6_with_the_look_keys_is_switched_to_commands_unless_changed) {
    Profile q;
    std::string err;
    CHECK(parse("x52pro-profile 5\nname = m\nbind 40 any press \"Look up\" key:r\nbind 41 any press \"Look right\" key:e\n"
                "bind 42 any press \"Mine\" key:f\n", q, &err));
    CHECK_EQ(actionToText(q.exact(40, kLayerAny)->press), std::string("cmd:sim/general/rot_up"));
    CHECK_EQ(actionToText(q.exact(41, kLayerAny)->press), std::string("cmd:sim/general/rot_right"));
    CHECK_EQ(actionToText(q.exact(42, kLayerAny)->press), std::string("key:f"));   // the user's own label: left alone
}

TEST(the_nub_presses_the_arrows_part_way_and_shift_arrows_all_the_way) {
    // bits 0..3: up right down left; bits 4..7: the same, far
    CHECK_EQ(nubButtonMask(0, 0, 0), 0);
    CHECK_EQ(nubButtonMask(0, -0.5, 0), 1);          // up
    CHECK_EQ(nubButtonMask(0, 0.5, 0), 4);           // down
    CHECK_EQ(nubButtonMask(0.5, 0, 0), 2);           // right
    CHECK_EQ(nubButtonMask(-0.5, 0, 0), 8);          // left
    CHECK_EQ(nubButtonMask(0.5, -0.5, 0), 3);        // up-right
    CHECK_EQ(nubButtonMask(0, -1.0, 0), 16);         // far up: the fast one only
    CHECK_EQ(nubButtonMask(1.0, -0.5, 0), 32 | 1);   // far right, part way up
    CHECK_EQ(nubButtonMask(0.2, 0.2, 0), 0);         // inside the dead zone
    CHECK_EQ(nubButtonMask(0.35, 0, 0), 0);          // not yet
    CHECK_EQ(nubButtonMask(0.35, 0, 2), 2);          // but held while it eases back (hysteresis)
    CHECK_EQ(nubButtonMask(0.8, 0, 32), 32);         // far stays far a little longer
    CHECK_EQ(nubAxisFraction(128, 0, 255) < 0.01 && nubAxisFraction(128, 0, 255) > -0.01, true);
    CHECK(nubAxisFraction(0, 0, 255) < -0.99);
    CHECK(nubAxisFraction(255, 0, 255) > 0.99);
}

TEST(pov_1_hat_value_gives_the_direction_buttons_it_holds) {
    CHECK_EQ(hatButtonMask(1, 1, 8), 1);    // up
    CHECK_EQ(hatButtonMask(2, 1, 8), 3);    // up-right = up + right
    CHECK_EQ(hatButtonMask(3, 1, 8), 2);    // right
    CHECK_EQ(hatButtonMask(5, 1, 8), 4);    // down
    CHECK_EQ(hatButtonMask(6, 1, 8), 12);   // down-left
    CHECK_EQ(hatButtonMask(7, 1, 8), 8);    // left
    CHECK_EQ(hatButtonMask(8, 1, 8), 9);    // up-left
    CHECK_EQ(hatButtonMask(0, 1, 8), 0);    // centred
    CHECK_EQ(hatButtonMask(15, 1, 8), 0);
    CHECK_EQ(hatButtonMask(0, 0, 7), 1);    // a hat that counts from 0
}

TEST(removing_a_wheel_binding_from_a_current_profile_is_respected) {
    Profile p = defaultProfile();
    p.edit(39, kLayerAny).press.clear();
    p.prune();
    Profile q;
    std::string err;
    CHECK(parse(serialize(p), q, &err));
    CHECK(q.find(39, 1) == nullptr);
}

TEST(hat_gives_a_direction_only_when_pushed_straight) {
    CHECK_EQ(hatDirection(1, 1, 8), 0);
    CHECK_EQ(hatDirection(3, 1, 8), 1);
    CHECK_EQ(hatDirection(5, 1, 8), 2);
    CHECK_EQ(hatDirection(7, 1, 8), 3);
    CHECK_EQ(hatDirection(2, 1, 8), -1);
    CHECK_EQ(hatDirection(0, 1, 8), -1);
    CHECK_EQ(hatDirection(15, 1, 8), -1);
}

TEST(light_rules_survive_save_and_load) {
    Profile p;
    LightRule brake = rulePreset(0).rule;
    brake.led = LedE;
    LightRule gear;
    gear.led = LedFire;
    gear.dataref = "sim/x/gear[2]";
    gear.op = Compare::LessEq;
    gear.value = -1.5;
    gear.thenColor = kRuleMode;
    gear.elseColor = (int)Color::Off;
    LightRule second = brake;
    second.led = LedD;
    second.negate = true;
    second.dataref = "sim/y";
    second.join = Join::Or;
    second.second.dataref = "sim/z[1]";
    second.second.op = Compare::NotEqual;
    second.second.value = 3;
    second.second.negate = true;
    LightRule blank;            // added but no dataref typed yet
    blank.led = LedA;
    p.rules = {brake, gear, second, blank};
    std::string text = serialize(p);
    CHECK(text.find("rule e > 0.5 red green sim/flightmodel/controls/parkbrake") != std::string::npos);
    CHECK(text.find("rule a > 0.5 red green -") != std::string::npos);
    CHECK(text.find("rule d > 0.5 red green not sim/y or not != 3 sim/z[1]") != std::string::npos);
    Profile q;
    std::string err;
    CHECK(parse(text, q, &err));
    CHECK_EQ(q.rules.size(), (size_t)4);
    CHECK(q.rules[0] == brake);
    CHECK(q.rules[1] == gear);
    CHECK(q.rules[2] == second);
    CHECK(q.rules[3] == blank);
}

TEST(the_first_rule_with_a_colour_wins_for_a_light) {
    LightRule brake = rulePreset(0).rule;   // red above 0.5 else green
    brake.led = LedE;
    LightRule gear;
    gear.led = LedE;
    gear.dataref = "gear";
    gear.op = Compare::Equal;
    gear.value = 1;
    gear.thenColor = (int)Color::Amber;
    gear.elseColor = kRuleMode;             // nothing to say when the gear is up
    double brakeV = 1, gearV = 0;
    DatarefReader read = [&](const std::string& r, double& v) {
        if (r == "gear") { v = gearV; return true; }
        v = brakeV;
        return true;
    };
    Color mode = Color::Off;
    CHECK(liveColor({brake, gear}, LedE, mode, read) == Color::Red);     // brake is first and has a colour
    gearV = 1;
    CHECK(liveColor({brake, gear}, LedE, mode, read) == Color::Red);     // brake still wins: it is first
    CHECK(liveColor({gear, brake}, LedE, mode, read) == Color::Amber);   // order matters
    gearV = 0;
    CHECK(liveColor({gear, brake}, LedE, mode, read) == Color::Red);     // gear has nothing to say, brake speaks
    CHECK(liveColor({brake, gear}, LedA, mode, read) == Color::Off);     // other lights untouched
    DatarefReader none = [](const std::string&, double&) { return false; };
    CHECK(liveColor({brake, gear}, LedE, Color::Amber, none) == Color::Amber);   // unreadable: mode colour
    CHECK(liveColor({}, LedE, Color::Red, read) == Color::Red);
}

TEST(pinkie_light_colours_are_saved_and_older_files_copy_the_mode_colours) {
    Profile p;
    p.leds[1][LedA] = Color::Red;
    p.ledsPinkie[1][LedA] = Color::Amber;
    Profile q;
    std::string err;
    CHECK(parse(serialize(p), q, &err));
    CHECK(q.ledAt(2, false, LedA) == Color::Red);
    CHECK(q.ledAt(2, true, LedA) == Color::Amber);
    // a file written before the pinkie lights existed: the pinkie changes nothing
    Profile old;
    CHECK(parse("x52pro-profile 2\nleds 2 = a:red b:off\n", old, &err));
    CHECK(old.ledAt(2, true, LedA) == Color::Red);
    CHECK(old.ledAt(2, true, LedB) == Color::Off);
    CHECK_EQ(std::string(modeLabel(2, true)), std::string("Pinkie + 2"));
    CHECK_EQ(std::string(modeLabel(3, false)), std::string("Mode 3"));
}

TEST(a_light_rule_with_a_mistake_is_rejected_with_its_line) {
    Profile q;
    std::string err;
    CHECK(!parse("rule e >> 1 red green sim/x\n", q, &err));
    CHECK(err.find("line 1") == 0);
    CHECK(!parse("rule e > 1 purple green sim/x\n", q, &err));
    CHECK(!parse("rule zz > 1 red green sim/x\n", q, &err));
    CHECK(!parse("rule e > one red green sim/x\n", q, &err));
    CHECK(!parse("rule e > 1 red green\n", q, &err));
}

TEST(a_light_rule_picks_its_colour_from_the_value) {
    LightRule r = rulePreset(0).rule;   // parking brake: red above 0.5, else green
    CHECK(ruleColor(r, LedE, true, 1.0, Color::Off) == Color::Red);
    CHECK(ruleColor(r, LedE, true, 0.0, Color::Off) == Color::Green);
    CHECK(ruleColor(r, LedE, false, 1.0, Color::Amber) == Color::Amber);   // unreadable: the mode's colour
    r.elseColor = kRuleMode;
    CHECK(ruleColor(r, LedE, true, 0.0, Color::Amber) == Color::Amber);
    CHECK(ruleColor(LightRule(), LedE, true, 1.0, Color::Amber) == Color::Amber);   // no rule
    CHECK(ruleColor(rulePreset(0).rule, LedFire, true, 1.0, Color::Off) == Color::Green);   // single colour: just lit
}

TEST(every_comparison_works) {
    CHECK(compareValues(Compare::Greater, 2, 1) && !compareValues(Compare::Greater, 1, 1));
    CHECK(compareValues(Compare::GreaterEq, 1, 1));
    CHECK(compareValues(Compare::Less, 0, 1) && !compareValues(Compare::Less, 1, 1));
    CHECK(compareValues(Compare::LessEq, 1, 1));
    CHECK(compareValues(Compare::Equal, 1, 1) && !compareValues(Compare::Equal, 1, 2));
    CHECK(compareValues(Compare::NotEqual, 1, 2));
}

int main() { return runAllTests(); }


// ---- alarms, second conditions, ordering

namespace {
DatarefReader values(std::map<std::string, double> m) {
    return [m](const std::string& r, double& v) {
        auto it = m.find(r);
        if (it == m.end()) return false;
        v = it->second;
        return true;
    };
}
Alarm alarmOn(const char* ref, int led, Color c, Effect e = Effect::Steady) {
    Alarm a;
    a.name = ref;
    a.dataref = ref;
    a.op = Compare::Greater;
    a.value = 0;
    a.led = led;
    a.color = (int)c;
    a.effect = e;
    return a;
}
}  // namespace

TEST(two_conditions_join_with_and_or_and_not) {
    Trigger t;
    t.dataref = "a";
    t.op = Compare::Greater;
    t.value = 0;
    CHECK(evaluate(t, values({{"a", 1}})) == TriggerState::True);
    CHECK(evaluate(t, values({{"a", 0}})) == TriggerState::False);
    CHECK(evaluate(t, values({})) == TriggerState::Unknown);
    t.negate = true;
    CHECK(evaluate(t, values({{"a", 1}})) == TriggerState::False);
    t.negate = false;
    t.join = Join::And;
    t.second.dataref = "b";
    t.second.op = Compare::Greater;
    t.second.value = 0;
    CHECK(evaluate(t, values({{"a", 1}, {"b", 1}})) == TriggerState::True);
    CHECK(evaluate(t, values({{"a", 1}, {"b", 0}})) == TriggerState::False);
    CHECK(evaluate(t, values({{"a", 1}})) == TriggerState::Unknown);   // b cannot be read
    t.join = Join::Or;
    CHECK(evaluate(t, values({{"a", 1}, {"b", 0}})) == TriggerState::True);
    CHECK(evaluate(t, values({{"a", 0}, {"b", 0}})) == TriggerState::False);
    t.second.negate = true;   // a or not b
    CHECK(evaluate(t, values({{"a", 0}, {"b", 0}})) == TriggerState::True);
    t.join = Join::None;      // a second condition without a join is ignored
    CHECK(evaluate(t, values({{"a", 0}, {"b", 0}})) == TriggerState::False);
}

TEST(a_master_switch_is_off_only_while_its_condition_is_false) {
    Trigger t;
    CHECK(masterOn(t, values({})));                       // nothing chosen: always on
    t.dataref = "avionics";
    t.op = Compare::Greater;
    t.value = 0.5;
    CHECK(masterOn(t, values({{"avionics", 1}})));
    CHECK(!masterOn(t, values({{"avionics", 0}})));
    CHECK(masterOn(t, values({})));                       // unreadable: stays on, a typo never blanks the stick
    t.join = Join::And;
    t.second.dataref = "battery";
    t.second.op = Compare::Greater;
    t.second.value = 0.5;
    CHECK(masterOn(t, values({{"avionics", 1}, {"battery", 1}})));
    CHECK(!masterOn(t, values({{"avionics", 1}, {"battery", 0}})));
}

TEST(x_plane_command_and_dataref_lists_can_be_loaded_and_searched) {
    const char* tmp = getenv("TMPDIR");
    std::string dir = std::string(tmp ? tmp : "/tmp") + "/x52catalog";
    std::string mk = "mkdir -p '" + dir + "'";
    CHECK(system(mk.c_str()) == 0);
    {
        std::ofstream c(dir + "/Commands.txt");
        c << "2 1210 Mon Nov  9 16:33:29 2026\n\nsim/flight_controls/landing_gear_toggle   Toggle landing gear\n"
             "sim/lights/landing_lights_toggle   Landing lights toggle\n"
             "sim/flight_controls/flaps_down   Flaps down a notch\n";
        std::ofstream d(dir + "/DataRefs.txt");
        d << "2 1210 Mon Nov  9 16:33:29 2026\n\nsim/cockpit2/switches/avionics_power_on\tint\ty\tboolean\tAvionics master\t\n"
             "sim/flightmodel/position/elevation\tdouble\tn\tmeters\tElevation\t\n";
    }
    std::vector<CatalogEntry> cmds = loadCommandCatalog(dir + "/Commands.txt");
    std::vector<CatalogEntry> refs = loadDatarefCatalog(dir + "/DataRefs.txt");
    CHECK_EQ(cmds.size(), (size_t)3);                       // the header line and the blank line are skipped
    CHECK_EQ(refs.size(), (size_t)2);
    CHECK(refs[0].writable && !refs[1].writable);
    CHECK(refs[0].info.find("int, boolean: Avionics master") == 0);
    auto found = searchCatalog(cmds, "landing", 10);
    CHECK_EQ(found.size(), (size_t)2);
    found = searchCatalog(cmds, "TOGGLE gear", 10);          // every word, any case, name or description
    CHECK_EQ(found.size(), (size_t)1);
    CHECK_EQ(found[0]->name, std::string("sim/flight_controls/landing_gear_toggle"));
    found = searchCatalog(cmds, "sim/flight_controls", 10);  // names that start with the query come first
    CHECK_EQ(found.size(), (size_t)2);
    CHECK_EQ(searchCatalog(cmds, "", 10).size(), (size_t)0);
    CHECK_EQ(searchCatalog(cmds, "landing", 1).size(), (size_t)1);
    CHECK_EQ(loadCommandCatalog(dir + "/missing.txt").size(), (size_t)0);
}

TEST(each_light_belongs_to_its_control) {
    CHECK_EQ(buttonLed(2), 0);     // Fire
    CHECK_EQ(buttonLed(3), 1);     // A
    CHECK_EQ(buttonLed(9), 5);     // toggle 1, both positions
    CHECK_EQ(buttonLed(10), 5);
    CHECK_EQ(buttonLed(14), 7);    // toggle 3
    CHECK_EQ(buttonLed(22), 8);    // the stick's hat 2
    CHECK_EQ(buttonLed(31), 9);    // clutch
    CHECK_EQ(buttonLed(1), -1);    // trigger, C, pinkie and the rest have no light
    CHECK_EQ(buttonLed(5), -1);
    CHECK_EQ(buttonLed(0), -1);
    CHECK_EQ(buttonLed(99), -1);
    for (int b = 1; b <= 51; ++b) CHECK(buttonLed(b) < kLedCount);
    CHECK_EQ(std::string(ledName(buttonLed(2))), std::string("fire"));
    CHECK_EQ(std::string(ledName(buttonLed(8))), std::string("e"));
}

TEST(aircraft_commands_and_datarefs_are_read_from_their_xlua_scripts) {
    const char* tmp = getenv("TMPDIR");
    std::string xp = std::string(tmp ? tmp : "/tmp") + "/x52aircraft";
    std::string mk = "rm -rf '" + xp + "' && mkdir -p '" + xp + "/Aircraft/Laminar Research/Grumman F-14 Tomcat/plugins/xlua/scripts/sys' '" + xp +
                     "/Aircraft/Other/Plain/plugins'";
    CHECK(system(mk.c_str()) == 0);
    {
        std::ofstream f(xp + "/Aircraft/Laminar Research/Grumman F-14 Tomcat/plugins/xlua/scripts/sys/sys.lua");
        f << "X = create_command(\"laminar/f14/fltc/cs_wpn_select_switch1\", \"Weapon Select Switch 1\", handler)\n"
             "Y = create_command('laminar/f14/fltc/cs_bomb_release_button', 'Bomb Release', handler2)\n"
             "Z = create_command(\"laminar/f14/fltc/cs_wpn_select_switch1\", \"duplicate\", handler)\n"
             "W = create_dataref(\"laminar/f14/fltc/bomb_pos\", \"number\")\n"
             "V = create_dataref(\"laminar/f14/fltc/sweep\", \"number\", sweep_handler)\n"
             "local not_a_call = my_create_command(\"a/b\", \"c\", d)\n"
             "create_command(variable, \"no literal name\", h)\n";
    }
    std::vector<CatalogEntry> cmds, refs;
    cmds.push_back(CatalogEntry{"sim/lights/landing_lights_toggle", "Landing lights", true, "landing"});
    loadAircraftCatalog(xp, cmds, refs);
    CHECK_EQ(cmds.size(), (size_t)3);                         // ours + the two F-14 commands, the duplicate and the look-alikes left out
    CHECK_EQ(cmds[1].name, std::string("laminar/f14/fltc/cs_wpn_select_switch1"));
    CHECK_EQ(cmds[1].info, std::string("Grumman F-14 Tomcat: Weapon Select Switch 1"));
    CHECK_EQ(refs.size(), (size_t)2);
    CHECK(!refs[0].writable && refs[1].writable);             // a handler makes a dataref writable
    auto found = searchCatalog(cmds, "weapon select", 10);
    CHECK_EQ(found.size(), (size_t)1);
    found = searchCatalog(cmds, "f-14", 10);                  // the aircraft's name is searchable
    CHECK_EQ(found.size(), (size_t)2);
    // DataRefTool's lists of the last run add what the scripts do not show, once per name.
    CHECK(system(("mkdir -p '" + xp + "/Output/preferences'").c_str()) == 0);
    {
        std::ofstream c(xp + "/Output/preferences/drt_last_run_commandrefs.txt");
        c << "laminar/f14/fltc/cs_wpn_select_switch1\nsome/plugin/compiled_command\n\nnot a name\n";
        std::ofstream d(xp + "/Output/preferences/drt_last_run_datarefs.txt");
        d << "some/plugin/compiled_dataref\n";
    }
    cmds.clear();
    refs.clear();
    loadAircraftCatalog(xp, cmds, refs);
    CHECK_EQ(cmds.size(), (size_t)3);   // two F-14 commands from the scripts + one only DataRefTool knows
    CHECK_EQ(cmds[2].name, std::string("some/plugin/compiled_command"));
    CHECK_EQ(refs.size(), (size_t)3);
    std::vector<CatalogEntry> none, none2;
    loadAircraftCatalog(xp + "/missing", none, none2);
    CHECK_EQ(none.size(), (size_t)0);
}

TEST(x_plane_joystick_slots_are_read_and_learned_from_presses) {
    auto a = parseButtonAssignments("I\n10 Version\n_joy_BUTN_use10 sim/flight_controls/brakes_toggle_regular\n"
                                    "_joy_BUTN_use11 sim/none/none\r\n_joy_BUTN_use9999 sim/x\n_joy_axis0 1\n");
    CHECK_EQ(a.size(), (size_t)2);                            // the slot out of range and other lines are ignored
    CHECK_EQ(a[10], std::string("sim/flight_controls/brakes_toggle_regular"));
    CHECK_EQ(a[11], std::string("sim/none/none"));
    std::map<int, int> m = {{3, 12}, {39, 150}};
    CHECK(parseButtonMap(serializeButtonMap(m)) == m);
    CHECK(parseButtonMap("# x\n0 5\n40 5\n7 -1\n7 9\n").size() == 1);

    ButtonLearner l;
    l.pressed(7, 10.0);
    CHECK(l.pending(10.1));
    CHECK(!l.update({}, 10.05));                              // nothing on yet: keep waiting
    CHECK(l.update({14}, 10.1) && l.map()[7] == 14);          // exactly one slot turned on
    CHECK(!l.pending(10.2));
    l.pressed(8, 11.0);
    CHECK(!l.update({14, 20, 21}, 11.05));                    // 14 is button 7's; 20 and 21 are ambiguous
    CHECK(l.update({14, 20}, 11.1) && l.map()[8] == 20);      // now only 20 is left
    l.pressed(8, 12.0);
    CHECK(!l.update({20}, 12.1));                             // same answer again: nothing changed
    l.pressed(9, 13.0);
    CHECK(!l.update({30, 31}, 13.1));                         // ambiguous, and then too late
    CHECK(!l.update({30}, 13.7) && l.map().count(9) == 0);
}

TEST(x_plane_numbers_buttons_after_the_hat_and_keeps_joystick_settings_per_aircraft) {
    CHECK_EQ(xplaneButtonNumber(10), 2);        // slot 10 is what X-Plane's screen calls Btn 2
    CHECK_EQ(xplaneButtonNumber(8), 0);
    CHECK_EQ(xplaneButtonNumber(7), -1);        // a hat direction
    CHECK_EQ(xplaneButtonNumber(168), 0);       // the next device starts again
    CHECK_EQ(controlProfileKey("Cessna Citation X"), std::string("Cessna___Citation___X"));
    std::string misc = "P acf_profile Boeing___737-800 A330\nP acf_profile Cessna___Citation___X Cesna___X\n";
    CHECK_EQ(controlProfileFor(misc, "Cessna Citation X"), std::string("Cesna___X"));
    CHECK_EQ(controlProfileFor(misc, "Unknown plane"), std::string(""));
    CHECK_EQ(controlProfileFor(misc, ""), std::string(""));
}

TEST(the_brightness_page_shows_percent_and_follows_the_wheel) {
    PageCursor c;
    Lines p = formatBrightnessPage(127, 64, false, false, c);
    CHECK_EQ(p.l[0], std::string("BRIGHTNESS"));
    CHECK_EQ(p.l[1], std::string("Screen     >100%"));
    CHECK_EQ(p.l[2], std::string("Lights       50%"));
    int screen = 127, lights = 64;
    CHECK(!brightnessInput(c, Wheel::Down, screen, lights, false, false));   // turning alone moves the cursor
    CHECK_EQ(c.item, 1);
    CHECK(!brightnessInput(c, Wheel::Select, screen, lights, false, false));
    CHECK(c.editing);
    CHECK(brightnessInput(c, Wheel::Up, screen, lights, false, false));
    CHECK_EQ(lights, 70);
    CHECK_EQ(formatBrightnessPage(screen, lights, false, false, c).l[2], std::string("Lights     >>55%"));
    for (int i = 0; i < 30; ++i) brightnessInput(c, Wheel::Up, screen, lights, false, false);
    CHECK_EQ(lights, 127);                                                   // stops at 100 %
    CHECK(!brightnessInput(c, Wheel::Up, screen, lights, false, false));
    brightnessInput(c, Wheel::Select, screen, lights, false, false);
    c.item = 0;
    brightnessInput(c, Wheel::Select, screen, lights, false, false);
    for (int i = 0; i < 30; ++i) brightnessInput(c, Wheel::Down, screen, lights, false, false);
    CHECK_EQ(formatBrightnessPage(screen, lights, false, false, c).l[1], std::string("Screen     >>10%"));   // never fully off
    PageCursor l;
    l.item = 0;
    CHECK(!brightnessInput(l, Wheel::Select, screen, lights, true, false));  // a linked brightness is not changed here
    CHECK(!l.editing);
    CHECK_EQ(formatBrightnessPage(screen, lights, true, false, l).l[1], std::string("Screen   dataref"));
}

TEST(the_slots_of_buttons_never_pressed_follow_from_the_ones_that_were) {
    std::map<int, int> none = completeButtonMap({});
    CHECK_EQ(none.size(), (size_t)39);
    CHECK_EQ(none[1], 8);                                        // the stick's first button is slot 8, right after the hat
    CHECK_EQ(none[39], 46);
    std::map<int, int> got = completeButtonMap({{3, 10}, {4, 11}, {35, 42}, {36, 43}});
    CHECK_EQ(got[10], 17);                                       // slot = button + 7, as learned
    std::map<int, int> odd = completeButtonMap({{3, 13}, {4, 14}, {5, 99}});   // one that disagrees is outvoted ...
    CHECK_EQ(odd[10], 17 + 3);
    CHECK_EQ(odd[5], 99);                                        // ... but what was learned always wins for its own button
}

static PageTree samplePages() {
    PageTree t;
    std::string err;
    bool ok = parsePageLine("fuel \"Fuel\"", t, err) &&
              parseRowLine("fuel text \"FUEL\"", t, err) &&
              parseRowLine("fuel value \"Left\" \"%.0f kg\" sim/fuel/left", t, err) &&
              parseRowLine("fuel value \"Right\" \"%.1f\" sim/fuel/right[1]", t, err) &&
              parseRowLine("fuel link \"Details\" fueldetail", t, err) &&
              parsePageLine("fueldetail \"Fuel details\" in fuel", t, err) &&
              parseRowLine("fueldetail value \"Flow\" \"%d\" sim/fuel/flow", t, err) &&
              parsePageLine("empty \"Nothing here\"", t, err);
    if (!ok) fprintf(stderr, "samplePages: %s\n", err.c_str());
    return t;
}

TEST(user_pages_are_saved_in_the_profile_and_read_back) {
    Profile p = defaultProfile();
    p.pages = samplePages();
    CHECK_EQ(p.pages.pages.size(), (size_t)3);
    std::string text = serialize(p);
    CHECK(text.find("page fuel \"Fuel\"\n") != std::string::npos);
    CHECK(text.find("page fueldetail \"Fuel details\" in fuel\n") != std::string::npos);
    CHECK(text.find("row fuel value \"Left\" \"%.0f kg\" sim/fuel/left\n") != std::string::npos);
    Profile q;
    std::string err;
    CHECK(parse(text, q, &err));
    CHECK(q.pages == p.pages);
    CHECK_EQ(q.pages.topLevel().size(), (size_t)2);              // fuel and empty; the detail page is only reached by a link
    CHECK(serialize(defaultProfile()).find("\npage ") == std::string::npos);   // no pages: nothing written
    PageTree t;
    CHECK(!parseRowLine("nowhere text \"x\"", t, err));          // a row needs its page defined above
    CHECK(!parsePageLine("bad id \"x\"", t, err));
    CHECK(parsePageLine("a \"A\"", t, err));
    CHECK(!parsePageLine("a \"again\"", t, err));
    CHECK(!parsePageLine("b \"B\" within a", t, err));
    CHECK(!parseRowLine("a value \"x\" \"%g\"", t, err));       // a value row needs its dataref too
}

TEST(value_formats_show_one_number_safely) {
    CHECK_EQ(formatPageValue("%.0f kg", 245.6), std::string("246 kg"));
    CHECK_EQ(formatPageValue("%d", 3.4), std::string("3"));
    CHECK_EQ(formatPageValue("%5.1f%%", 12.34), std::string(" 12.3%"));
    CHECK_EQ(formatPageValue("%g", 0.5), std::string("0.5"));
    CHECK_EQ(formatPageValue("%s", 1), std::string("1"));          // not allowed: the number, plainly
    CHECK_EQ(formatPageValue("%n", 1), std::string("1"));
    CHECK_EQ(formatPageValue("%d %d", 7), std::string("7"));       // only one conversion
    CHECK_EQ(formatPageValue("no number", 7), std::string("no number"));
}

TEST(user_pages_show_rows_with_a_cursor_and_open_sub_pages) {
    PageTree t = samplePages();
    PageNav nav;
    DatarefReader read = [](const std::string& r, double& v) {
        if (r == "sim/fuel/left") { v = 245.4; return true; }
        if (r == "sim/fuel/flow") { v = 12; return true; }
        return false;                                         // the right tank cannot be read
    };
    Lines p = formatUserPage(t, "fuel", nav, read);
    CHECK_EQ(p.l[0], std::string(">FUEL"));                    // the cursor starts on the first row
    CHECK_EQ(p.l[1], std::string(" Left     245 kg"));        // the value is right-aligned in 16 columns
    CHECK_EQ(p.l[2], std::string(" Right        --"));      // unreadable: --
    pageTreeInput(t, "fuel", nav, Wheel::Down);
    pageTreeInput(t, "fuel", nav, Wheel::Down);
    pageTreeInput(t, "fuel", nav, Wheel::Down);                // the Details link is the fourth row
    p = formatUserPage(t, "fuel", nav, read);
    CHECK_EQ(p.l[0], std::string(" Left     245 kg"));        // the window scrolled to keep the cursor in view
    CHECK_EQ(p.l[1], std::string(" Right        --"));
    CHECK_EQ(p.l[2], std::string(">Details       >"));
    pageTreeInput(t, "fuel", nav, Wheel::Select);              // opens the sub-page
    p = formatUserPage(t, "fuel", nav, read);
    CHECK_EQ(p.l[0], std::string(">< Back"));
    CHECK_EQ(p.l[1], std::string(" Flow         12"));
    pageTreeInput(t, "fuel", nav, Wheel::Down);
    pageTreeInput(t, "fuel", nav, Wheel::Select);              // a value row does nothing
    CHECK_EQ(nav.stack.size(), (size_t)1);
    pageTreeInput(t, "fuel", nav, Wheel::Up);
    pageTreeInput(t, "fuel", nav, Wheel::Select);              // Back
    CHECK(nav.stack.empty());
    p = formatUserPage(t, "fuel", nav, read);
    CHECK_EQ(p.l[2], std::string(">Details       >"));       // back where it was, cursor on the link
    nav.reset();
    p = formatUserPage(t, "empty", nav, read);
    CHECK_EQ(p.l[0], std::string("Nothing here"));            // a page without rows names itself
    CHECK_EQ(formatUserPage(t, "missing", nav, read).l[0], std::string(""));
    PageTree loop;
    std::string err;
    CHECK(parsePageLine("a \"A\"", loop, err) && parseRowLine("a link \"Again\" a", loop, err));
    PageNav n2;
    for (int i = 0; i < 20; ++i) pageTreeInput(loop, "a", n2, Wheel::Select);   // a page linking to itself cannot trap the wheel
    CHECK(n2.stack.size() <= 8);
}

TEST(dataref_steps_are_written_and_read_as_text) {
    Action a;
    std::string err;
    CHECK(actionFromText("set:sim/a=1 ; toggle:sim/b ; toggle:sim/c=2,5 ; add:sim/d=-0.5 ; add:sim/e[3]=1,0,10 ; wait:100", a, &err));
    CHECK_EQ(a.size(), (size_t)6);
    CHECK(a[0].kind == StepKind::Dataref && a[0].op == DatarefOp::Set && a[0].text == "sim/a" && a[0].value == 1);
    CHECK(a[1].op == DatarefOp::Toggle && a[1].value == 0 && a[1].value2 == 1);
    CHECK(a[2].op == DatarefOp::Toggle && a[2].value == 2 && a[2].value2 == 5);
    CHECK(a[3].op == DatarefOp::Add && a[3].value == -0.5 && a[3].lo == a[3].hi);
    CHECK(a[4].text == "sim/e[3]" && a[4].lo == 0 && a[4].hi == 10);
    Action b;
    CHECK(actionFromText(actionToText(a), b, &err));
    CHECK(a == b);                                          // survives a round trip
    CHECK(!actionFromText("set:sim/a", b, &err));           // set needs a value
    CHECK(!actionFromText("set:sim/a=x", b, &err));
    CHECK(!actionFromText("add:sim/a=1,2", b, &err));       // add takes 1 or 3 numbers
    CHECK(!actionFromText("toggle:sim/a=1", b, &err));
    CHECK(actionFromText("cmd: ; key: ; set:=0 ; toggle: ; add:=1", b, &err));   // blank steps still being filled in
    CHECK_EQ(b.size(), (size_t)5);
    Action c;
    CHECK(actionFromText(actionToText(b), c, &err) && b == c);
    double out = 0;
    Step t;
    t.kind = StepKind::Dataref;
    t.op = DatarefOp::Toggle;
    CHECK(datarefStepValue(t, true, 0, out) && out == 1);
    CHECK(datarefStepValue(t, true, 1, out) && out == 0);
    CHECK(datarefStepValue(t, false, 0, out) && out == 0);  // unknown: goes to the first value
    t.op = DatarefOp::Add;
    t.value = 2;
    CHECK(!datarefStepValue(t, false, 0, out));             // cannot add to what it cannot read
    t.lo = 0;
    t.hi = 5;
    CHECK(datarefStepValue(t, true, 4, out) && out == 5);
}

TEST(a_dataref_step_runs_once_on_press_and_inside_macros) {
    struct DataHost : Rec {
        std::vector<std::string> sets;
        void datarefStep(const Step& s) override { sets.push_back(s.text); }
    } h;
    Profile p = defaultProfile();
    Action a;
    CHECK(actionFromText("set:sim/a=1", a, nullptr));
    p.edit(10, kLayerAny).press = a;
    CHECK(actionFromText("cmd:sim/x ; set:sim/b=2", a, nullptr));
    p.edit(11, kLayerAny).press = a;
    Engine e(h);
    e.setProfile(p);
    e.buttonEvent(10, true, 0);
    CHECK_EQ(h.sets.size(), (size_t)1);        // once, on press
    e.buttonEvent(10, false, 0.1);
    CHECK_EQ(h.sets.size(), (size_t)1);        // nothing on release
    e.buttonEvent(11, true, 1);
    e.buttonEvent(11, false, 1.1);
    CHECK_EQ(h.sets.size(), (size_t)2);
    CHECK_EQ(h.str(), std::string("once:sim/x "));
}

TEST(brightness_follows_a_dataref_between_two_points) {
    BrightnessLink b;
    CHECK_EQ(followBrightness(b, 99, values({{"dial", 1}})), 99);   // none chosen: the fixed value
    b.dataref = "dial";
    b.inLow = 0;
    b.inHigh = 1;
    b.outLow = 10;
    b.outHigh = 110;
    CHECK_EQ(followBrightness(b, 99, values({{"dial", 0}})), 10);
    CHECK_EQ(followBrightness(b, 99, values({{"dial", 1}})), 110);
    CHECK_EQ(followBrightness(b, 99, values({{"dial", 0.5}})), 60);
    CHECK_EQ(followBrightness(b, 99, values({{"dial", 7}})), 110);   // beyond the range: clamped
    CHECK_EQ(followBrightness(b, 99, values({{"dial", -3}})), 10);
    CHECK_EQ(followBrightness(b, 99, values({})), 99);               // unreadable: the fixed value
    b.inLow = 1;
    b.inHigh = 0;                                                    // reversed range works too
    CHECK_EQ(followBrightness(b, 99, values({{"dial", 1}})), 10);
}

TEST(brightness_links_are_saved_in_the_profile_and_read_back) {
    Profile p = defaultProfile();
    p.brightScreen.dataref = "sim/cockpit2/electrical/instrument_brightness_ratio[0]";
    p.brightScreen.inLow = 0.1;
    p.brightScreen.inHigh = 0.9;
    p.brightScreen.outLow = 5;
    p.brightScreen.outHigh = 100;
    std::string text = serialize(p);
    CHECK(text.find("brightness screen 0.1 0.9 5 100 sim/cockpit2/electrical/instrument_brightness_ratio[0]") != std::string::npos);
    Profile q;
    std::string err;
    CHECK(parse(text, q, &err));
    CHECK(q.brightScreen == p.brightScreen);
    CHECK(!q.brightLights.active());
    CHECK(q.mfdBrightness == p.mfdBrightness);                       // the fixed slider is untouched
    CHECK(!parse("x52pro-profile 3\nbrightness lights a b 1 2 x\n", q, &err));
}

TEST(master_switches_are_saved_in_the_profile_and_read_back) {
    Profile p = defaultProfile();
    p.masterScreen.dataref = "sim/cockpit2/switches/avionics_power_on";
    p.masterScreen.op = Compare::Equal;
    p.masterScreen.value = 1;
    p.masterLights.dataref = "sim/battery";
    p.masterLights.negate = true;
    p.masterLights.op = Compare::Less;
    p.masterLights.value = 1;
    p.masterLights.join = Join::Or;
    p.masterLights.second.dataref = "sim/gen";
    p.masterLights.second.op = Compare::Greater;
    p.masterLights.second.value = 0;
    std::string text = serialize(p);
    CHECK(text.find("master screen == 1 sim/cockpit2/switches/avionics_power_on") != std::string::npos);
    Profile q;
    std::string err;
    CHECK(parse(text, q, &err));
    CHECK(q.masterScreen == p.masterScreen);
    CHECK(q.masterLights == p.masterLights);
    Profile none = defaultProfile();
    CHECK(serialize(none).find("\nmaster ") == std::string::npos);   // none chosen: no line written
    CHECK(!parse("x52pro-profile 3\nmaster toes > 1 x\n", q, &err));
    CHECK(datarefsUsed({}, {}, {p.masterScreen, p.masterLights}).size() == 3);
}

TEST(an_alarm_that_is_not_triggered_leaves_the_light_to_the_next_one) {
    std::vector<Alarm> alarms = {alarmOn("stall", LedPov, Color::Red), alarmOn("gear", LedPov, Color::Amber)};
    CHECK_EQ(activeAlarm(alarms, LedPov, values({{"stall", 1}, {"gear", 1}})), 0);   // both: the first wins
    CHECK_EQ(activeAlarm(alarms, LedPov, values({{"stall", 0}, {"gear", 1}})), 1);   // not stalling: gear speaks
    CHECK_EQ(activeAlarm(alarms, LedPov, values({{"stall", 0}, {"gear", 0}})), -1);
    CHECK_EQ(activeAlarm(alarms, LedPov, values({{"gear", 1}})), 1);                 // stall unreadable: skipped
    CHECK_EQ(activeAlarm(alarms, LedA, values({{"stall", 1}})), -1);                 // other light
}

TEST(alarms_beat_live_rules_which_beat_the_mode_colour) {
    LightRule rule;
    rule.led = LedPov;
    rule.dataref = "brake";
    rule.thenColor = (int)Color::Green;
    rule.elseColor = (int)Color::Off;
    rule.op = Compare::Greater;
    rule.value = 0;
    std::vector<Alarm> alarms = {alarmOn("stall", LedPov, Color::Red)};
    CHECK(shownColor(alarms, {rule}, LedPov, Color::Amber, 0, values({{"stall", 1}, {"brake", 1}})) == Color::Red);
    CHECK(shownColor(alarms, {rule}, LedPov, Color::Amber, 0, values({{"stall", 0}, {"brake", 1}})) == Color::Green);
    CHECK(shownColor(alarms, {rule}, LedPov, Color::Amber, 0, values({})) == Color::Amber);
    CHECK(shownColor({}, {}, LedPov, Color::Amber, 0, values({})) == Color::Amber);
}

TEST(blinking_alarms_follow_their_speed) {
    Alarm a = alarmOn("x", LedA, Color::Red, Effect::Blink);
    a.fast = false;
    CHECK(alarmColorAt(a, 0.0) == Color::Red);
    CHECK(alarmColorAt(a, 0.49) == Color::Red);
    CHECK(alarmColorAt(a, 0.51) == Color::Off);
    CHECK(alarmColorAt(a, 1.2) == Color::Red);
    a.fast = true;
    CHECK(alarmColorAt(a, 0.1) == Color::Red);
    CHECK(alarmColorAt(a, 0.3) == Color::Off);
    CHECK(alarmColorAt(a, 0.6) == Color::Red);
    a.effect = Effect::Alternate;
    a.color2 = (int)Color::Amber;
    CHECK(alarmColorAt(a, 0.1) == Color::Red);
    CHECK(alarmColorAt(a, 0.3) == Color::Amber);
    a.effect = Effect::Steady;
    CHECK(alarmColorAt(a, 0.3) == Color::Red);
    Alarm fire = alarmOn("x", LedFire, Color::Red, Effect::Blink);   // single-colour lights are on or off
    CHECK(alarmColorAt(fire, 0.1) == Color::Green);
    CHECK(alarmColorAt(fire, 0.6) == Color::Off);
}

TEST(alarms_survive_save_and_load) {
    Profile p;
    Alarm a = alarmPreset(0).alarm;
    Alarm b = alarmOn("gear", LedPov, Color::Amber, Effect::Alternate);
    b.name = "Gear and \"fast\"";
    b.fast = true;
    b.negate = true;
    b.join = Join::And;
    b.second.dataref = "sim/q[2]";
    b.second.op = Compare::LessEq;
    b.second.value = 1.5;
    Alarm blank;
    p.alarms = {a, b, blank};
    std::string text = serialize(p);
    CHECK(text.find("alarm pov blink fast red amber \"Stall\" > 0 sim/cockpit2/annunciators/stall_warning") != std::string::npos);
    Profile q;
    std::string err;
    CHECK(parse(text, q, &err));
    CHECK_EQ(q.alarms.size(), (size_t)3);
    CHECK(q.alarms[0] == a);
    b.name = "Gear and 'fast'";   // quotes cannot be stored in a name
    CHECK(q.alarms[1] == b);
    CHECK(q.alarms[2] == blank);
}

TEST(an_alarm_with_a_mistake_is_rejected_with_its_line) {
    Profile q;
    std::string err;
    CHECK(!parse("alarm pov blink fast red amber Stall > 0 x\n", q, &err));          // no quotes
    CHECK(err.find("line 1") == 0);
    CHECK(!parse("alarm zz blink fast red amber \"S\" > 0 x\n", q, &err));
    CHECK(!parse("alarm pov wobble fast red amber \"S\" > 0 x\n", q, &err));
    CHECK(!parse("alarm pov blink quick red amber \"S\" > 0 x\n", q, &err));
    CHECK(!parse("alarm pov blink fast mode amber \"S\" > 0 x\n", q, &err));
    CHECK(!parse("alarm pov blink fast red amber \"S\" >> 0 x\n", q, &err));
    CHECK(!parse("alarm pov blink fast red amber \"S\" > 0 x maybe > 1 y\n", q, &err));
}

TEST(rules_from_before_version_3_are_reversed_once_so_they_behave_the_same) {
    // Old files: the last rule for a light won. Reversed, the first one wins, which is the same light.
    Profile q;
    std::string err;
    CHECK(parse("x52pro-profile 2\nrule e > 0 red off first\nrule e > 0 amber mode second\n", q, &err));
    CHECK_EQ(q.rules.size(), (size_t)2);
    CHECK_EQ(q.rules[0].dataref, std::string("second"));
    CHECK_EQ(q.rules[1].dataref, std::string("first"));
    CHECK(liveColor(q.rules, LedE, Color::Green, values({{"first", 1}, {"second", 1}})) == Color::Amber);   // as before
    Profile n;
    CHECK(parse("x52pro-profile 3\nrule e > 0 red off first\nrule e > 0 amber mode second\n", n, &err));
    CHECK_EQ(n.rules[0].dataref, std::string("first"));   // a current file keeps its order
}

TEST(one_rule_per_light_is_checked_and_a_free_light_is_found) {
    std::vector<LightRule> rules(2);
    rules[0].led = LedE;
    rules[1].led = LedA;
    CHECK_EQ(ruleUsingLed(rules, LedE, 1), 0);
    CHECK_EQ(ruleUsingLed(rules, LedE, 0), -1);   // the rule itself does not count
    CHECK_EQ(ruleUsingLed(rules, LedB, 0), -1);
    CHECK_EQ(firstFreeLed(rules), (int)LedFire);
    rules.assign(kLedCount, LightRule());
    for (int l = 0; l < kLedCount; ++l) rules[l].led = l;
    CHECK_EQ(firstFreeLed(rules), -1);
}

TEST(the_alarm_templates_are_complete) {
    CHECK(alarmPresetCount() >= 5);
    for (int i = 0; i < alarmPresetCount(); ++i) {
        const Alarm& a = alarmPreset(i).alarm;
        CHECK(a.active());
        CHECK(!a.name.empty());
        CHECK(a.dataref.compare(0, 4, "sim/") == 0);
    }
}

// ---------------------------------------------------------------- dial acceleration

static std::string joinCmds(const std::vector<std::string>& v) {
    std::string s;
    for (auto& c : v) s += c + " ";
    return s;
}

TEST(a_slow_dial_turn_repeats_nothing_and_a_fast_one_repeats_by_the_factor) {
    DialAccel d;   // defaults: slow from 150 ms, fast below 60 ms, factor 10
    CHECK_EQ(dialRepeat(10.0, d), 1);    // first click after a rest
    CHECK_EQ(dialRepeat(0.30, d), 1);
    CHECK_EQ(dialRepeat(0.15, d), 1);
    CHECK_EQ(dialRepeat(0.06, d), 10);
    CHECK_EQ(dialRepeat(0.01, d), 10);
    int mid = dialRepeat(0.10, d);       // in between it ramps up smoothly
    CHECK(mid > 1 && mid < 10);
    CHECK(dialRepeat(0.12, d) <= dialRepeat(0.08, d));
}

TEST(dial_acceleration_can_be_switched_off) {
    DialAccel d;
    d.on = false;
    CHECK_EQ(dialRepeat(0.01, d), 1);
    DialAccel one;
    one.factor = 1;
    CHECK_EQ(dialRepeat(0.01, one), 1);
}

TEST(fast_turns_repeat_the_command_on_heading_altitude_speed_and_fine_tuning_only) {
    PageCursor c;
    c.item = 0;   // HDG
    pageInput(PageAutopilot, c, Wheel::Select);
    CHECK(c.editing);
    CHECK_EQ(joinCmds(pageInput(PageAutopilot, c, Wheel::Up, false, 1)), std::string("sim/autopilot/heading_up "));
    std::string ten;
    for (int i = 0; i < 10; ++i) ten += "sim/autopilot/heading_up ";
    CHECK_EQ(joinCmds(pageInput(PageAutopilot, c, Wheel::Up, false, 10)), ten);

    PageCursor f;   // COM1 fine: item 2
    f.item = 2;
    pageInput(PageComNav1, f, Wheel::Select);
    CHECK_EQ(pageInput(PageComNav1, f, Wheel::Down, false, 4).size(), (size_t)4);

    PageCursor k;   // COM1 coarse (MHz) never jumps
    k.item = 1;
    pageInput(PageComNav1, k, Wheel::Select);
    CHECK_EQ(pageInput(PageComNav1, k, Wheel::Up, false, 10).size(), (size_t)1);

    PageCursor x;   // transponder digit wraps around: no repeat
    x.item = 0;
    pageInput(PageXpdr, x, Wheel::Select);
    CHECK_EQ(pageInput(PageXpdr, x, Wheel::Up, false, 10).size(), (size_t)1);
}

TEST(moving_the_cursor_never_repeats) {
    PageCursor c;
    c.item = 0;
    pageInput(PageAutopilot, c, Wheel::Down, false, 10);
    CHECK_EQ(c.item, 1);
}

TEST(dial_acceleration_is_saved_in_the_profile) {
    Profile p;
    CHECK(p.dial.on);
    CHECK_EQ(p.dial.factor, 10);
    p.dial.on = false;
    p.dial.slowMs = 200;
    p.dial.fastMs = 40;
    p.dial.factor = 5;
    Profile q;
    std::string err;
    CHECK(parse(serialize(p), q, &err));
    CHECK(!q.dial.on);
    CHECK_EQ(q.dial.slowMs, 200);
    CHECK_EQ(q.dial.fastMs, 40);
    CHECK_EQ(q.dial.factor, 5);
}

// ---------------------------------------------------------------- flight axes

static bool near(double a, double b) { return std::fabs(a - b) < 1e-4; }

TEST(raw_stick_values_become_minus_one_to_one_or_zero_to_one) {
    CHECK(near(normalizeAxis(0, 0, 1023, true), -1.0));
    CHECK(near(normalizeAxis(1023, 0, 1023, true), 1.0));
    CHECK(near(normalizeAxis(511, 0, 1022, true), 0.0));
    CHECK(near(normalizeAxis(0, 0, 255, false), 0.0));
    CHECK(near(normalizeAxis(255, 0, 255, false), 1.0));
    CHECK(near(normalizeAxis(5000, 0, 255, false), 1.0));   // never outside the range
    CHECK(near(normalizeAxis(7, 3, 3, true), 0.0));         // a broken range does not divide by zero
}

TEST(the_dead_zone_is_flat_and_the_rest_still_reaches_full_travel) {
    AxisCurve c;
    c.deadzone = 10;
    c.expo = 0;
    CHECK(near(shapeAxis(0.05, c, true), 0.0));
    CHECK(near(shapeAxis(-0.10, c, true), 0.0));
    CHECK(near(shapeAxis(0.55, c, true), 0.5));
    CHECK(near(shapeAxis(-0.55, c, true), -0.5));
    CHECK(near(shapeAxis(1.0, c, true), 1.0));
    CHECK(near(shapeAxis(-1.0, c, true), -1.0));
}

TEST(expo_softens_the_middle_and_keeps_the_ends) {
    AxisCurve c;
    c.deadzone = 0;
    c.expo = 100;
    CHECK(near(shapeAxis(0.5, c, true), 0.125));
    CHECK(near(shapeAxis(-0.5, c, true), -0.125));
    CHECK(near(shapeAxis(1.0, c, true), 1.0));
    c.expo = 50;
    CHECK(near(shapeAxis(0.5, c, true), 0.5 * 0.5 + 0.5 * 0.125));
}

TEST(an_inverted_axis_is_mirrored) {
    AxisCurve c;
    c.deadzone = 0;
    c.expo = 0;
    c.invert = true;
    CHECK(near(shapeAxis(0.25, c, true), -0.25));
    CHECK(near(shapeAxis(0.25, c, false), 0.75));
}

TEST(a_one_sided_axis_such_as_the_throttle_has_its_dead_zone_at_the_bottom) {
    AxisCurve c;
    c.deadzone = 5;
    c.expo = 0;
    CHECK(near(shapeAxis(0.03, c, false), 0.0));
    CHECK(near(shapeAxis(0.525, c, false), 0.5));
    CHECK(near(shapeAxis(1.0, c, false), 1.0));
}

TEST(each_stick_axis_knows_which_x_plane_value_it_replaces) {
    const AxisTarget* roll = axisTarget(0x01, 0x30);
    const AxisTarget* pitch = axisTarget(0x01, 0x31);
    const AxisTarget* yaw = axisTarget(0x01, 0x35);
    const AxisTarget* thr = axisTarget(0x01, 0x32);
    CHECK(roll && pitch && yaw && thr);
    if (!(roll && pitch && yaw && thr)) return;
    CHECK_EQ(std::string(roll->dataref), std::string("sim/joystick/yoke_roll_ratio"));
    CHECK_EQ(std::string(pitch->dataref), std::string("sim/joystick/yoke_pitch_ratio"));
    CHECK_EQ(std::string(yaw->dataref), std::string("sim/joystick/yoke_heading_ratio"));
    CHECK(roll->centred && pitch->centred && yaw->centred && !thr->centred);
    CHECK_EQ(std::string(roll->overrideRef), std::string("sim/operation/override/override_joystick"));
    CHECK_EQ(std::string(thr->overrideRef), std::string("sim/operation/override/override_throttles"));
    // With override_throttles on, X-Plane takes the throttles from ENGN_thro_use (one value per engine).
    CHECK_EQ(std::string(thr->dataref), std::string("sim/flightmodel/engine/ENGN_thro_use"));
    CHECK_EQ(thr->count, 16);
    CHECK_EQ(roll->count, 1);
    CHECK(axisTarget(0x01, 0x36) == nullptr);   // the slider and the rotaries are not flight axes
    CHECK(axisTarget(0x01, 0x39) == nullptr);
}

TEST(axis_settings_start_with_x_plane_in_charge_and_are_saved_in_the_profile) {
    Profile p;
    CHECK(!p.axes.overrideXPlane);
    p.axes.overrideXPlane = true;
    p.axes.roll.deadzone = 12;
    p.axes.pitch.expo = 40;
    p.axes.yaw.invert = true;
    p.axes.throttle.deadzone = 3;
    Profile q;
    std::string err;
    CHECK(parse(serialize(p), q, &err));
    CHECK(q.axes.overrideXPlane);
    CHECK_EQ(q.axes.roll.deadzone, 12);
    CHECK_EQ(q.axes.pitch.expo, 40);
    CHECK(q.axes.yaw.invert);
    CHECK_EQ(q.axes.throttle.deadzone, 3);
    Profile old;   // a profile written before the axes existed
    CHECK(parse("x52pro-profile 3\nname = old\n", old, &err));
    CHECK(!old.axes.overrideXPlane);
}

// ---------------------------------------------------------------- stick handling (smoothing, damping, calibration)

TEST(the_stick_starts_with_soft_curves_smoothing_and_a_light_damper) {
    AxisSettings s;
    CHECK_EQ(s.pitch.expo, 50);
    CHECK_EQ(s.roll.expo, 40);
    CHECK_EQ(s.yaw.expo, 30);
    CHECK_EQ(s.pitch.deadzone, 3);
    CHECK_EQ(s.throttle.expo, 0);   // the throttle stays linear
    CHECK_EQ(s.smoothingMs, 60);
    CHECK_EQ(s.damping, 30);
    Profile p;
    p.axes.smoothingMs = 120;
    p.axes.damping = 0;
    Profile q;
    std::string err;
    CHECK(parse(serialize(p), q, &err));
    CHECK_EQ(q.axes.smoothingMs, 120);
    CHECK_EQ(q.axes.damping, 0);
    CHECK(parse("x52pro-profile 4\nstick_smoothing = 9999\nstick_damping = -5\n", q, &err));
    CHECK_EQ(q.axes.smoothingMs, 200);
    CHECK_EQ(q.axes.damping, 0);
}

TEST(older_profiles_with_the_old_untouched_stick_curves_get_the_new_ones_but_chosen_curves_stay) {
    Profile q;
    std::string err;
    CHECK(parse("x52pro-profile 3\naxis_roll = 5 0 0\naxis_pitch = 5 0 1\naxis_yaw = 8 20 0\naxis_throttle = 5 0 1\n", q, &err));
    CHECK_EQ(q.axes.roll.deadzone, 3);
    CHECK_EQ(q.axes.roll.expo, 40);
    CHECK_EQ(q.axes.pitch.expo, 50);
    CHECK(q.axes.pitch.invert);                 // the inversion is kept
    CHECK_EQ(q.axes.yaw.deadzone, 8);           // set on purpose: kept
    CHECK_EQ(q.axes.yaw.expo, 20);
    CHECK_EQ(q.axes.throttle.deadzone, 5);      // the throttle is not touched
    CHECK_EQ(q.axes.throttle.expo, 0);
    CHECK(parse("x52pro-profile 4\naxis_pitch = 5 0 0\n", q, &err));   // a current file means it
    CHECK_EQ(q.axes.pitch.deadzone, 5);
    CHECK_EQ(q.axes.pitch.expo, 0);
}

TEST(smoothing_calms_small_jitter_but_big_moves_and_letting_go_reach_the_sim_at_once) {
    const double dt = 1.0 / 60;
    // A jitter of half a percent is mostly held back.
    double y = smoothStick(0.0, 0.005, dt, 60);
    CHECK(y > 0.0 && y < 0.0025);
    // Letting go of the stick from full deflection: the centre is reached in the same frame.
    CHECK(near(smoothStick(1.0, 0.0, dt, 60), 0.0));
    CHECK(near(smoothStick(-0.6, 0.0, dt, 60), 0.0));
    // Letting go from a small deflection still arrives within a few frames (well under 0.1 s).
    y = 0.15;
    for (int i = 0; i < 4; ++i) y = smoothStick(y, 0.0, dt, 60);
    CHECK(std::fabs(y) < 0.01);
    // Off, or a paused sim (no time passing): no filtering, and nothing moves while paused.
    CHECK(near(smoothStick(0.2, 0.3, dt, 0), 0.3));
    CHECK(near(smoothStick(0.2, 0.21, 0.0, 60), 0.2));
}

TEST(the_damper_only_nudges_near_the_centre_and_never_takes_over) {
    // Nose pitching up at 10 deg/s with the stick centred: a small push the other way.
    double c = dampStick(0.0, 10.0, 30);
    CHECK(c < 0.0);
    CHECK(c >= -0.045 - 1e-9);                 // at most 4.5 % of the travel at the default strength
    CHECK(dampStick(0.0, -10.0, 30) > 0.0);    // and the other way round
    CHECK(near(dampStick(0.0, 200.0, 100), -0.15));   // a violent motion: capped at 15 % even at full strength
    // With the stick held well away from the centre the pilot's input is untouched.
    CHECK(near(dampStick(0.6, 10.0, 30), 0.6));
    CHECK(near(dampStick(-1.0, -40.0, 100), -1.0));
    // Off: nothing.
    CHECK(near(dampStick(0.1, 10.0, 0), 0.1));
    // Never outside -1..1.
    CHECK(dampStick(0.99, -300.0, 100) <= 1.0);
}

TEST(a_calibrated_axis_has_its_own_centre_and_ends) {
    AxisCalibration c{100, 540, 950};
    CHECK(c.valid());
    CHECK(near(normalizeStick(540, 0, 1023, &c), 0.0));    // the stick's own centre is zero
    CHECK(near(normalizeStick(950, 0, 1023, &c), 1.0));
    CHECK(near(normalizeStick(1023, 0, 1023, &c), 1.0));   // beyond the measured end: still full
    CHECK(near(normalizeStick(100, 0, 1023, &c), -1.0));
    CHECK(near(normalizeStick(320, 0, 1023, &c), -0.5));   // each side scaled on its own
    CHECK(near(normalizeStick(745, 0, 1023, &c), 0.5));
    CHECK(near(normalizeStick(1023, 0, 1023, nullptr), 1.0));   // without a calibration: the full range
    CHECK(!(AxisCalibration{500, 500, 900}).valid());
}

TEST(calibration_measures_the_limits_and_takes_the_centre_where_the_stick_is_let_go) {
    StickCalibrator cal;
    cal.start();
    CHECK(cal.active());
    for (long v : {512L, 30L, 990L, 512L}) cal.feed(0x30, v, 0, 1023);
    for (long v : {500L, 10L, 1010L}) cal.feed(0x31, v, 0, 1023);
    for (long v : {128L, 20L, 240L}) cal.feed(0x35, v, 0, 255);
    std::map<int, AxisCalibration> out;
    std::string err;
    std::map<int, long> rest = {{0x30, 515}, {0x31, 505}, {0x35, 127}};
    CHECK(cal.finish(rest, out, &err));
    CHECK(!cal.active());
    CHECK_EQ(out[0x30].min, 30L);
    CHECK_EQ(out[0x30].max, 990L);
    CHECK_EQ(out[0x30].centre, 515L);
    CHECK_EQ(out[0x35].centre, 127L);
    // Saved and read back.
    std::map<int, AxisCalibration> back = parseCalibration(serializeCalibration(out));
    CHECK_EQ(back.size(), (size_t)3);
    CHECK_EQ(back[0x31].min, 10L);
    CHECK_EQ(back[0x31].centre, 505L);
    CHECK_EQ(back[0x31].max, 1010L);
}

TEST(calibration_is_refused_when_the_stick_was_not_moved_or_not_let_go) {
    StickCalibrator cal;
    std::map<int, AxisCalibration> out;
    std::string err;
    cal.start();
    for (long v : {500L, 450L, 560L}) cal.feed(0x31, v, 0, 1023);   // barely moved
    for (long v : {500L, 0L, 1023L}) cal.feed(0x30, v, 0, 1023);
    for (long v : {128L, 0L, 255L}) cal.feed(0x35, v, 0, 255);
    CHECK(!cal.finish({{0x30, 512}, {0x31, 510}, {0x35, 128}}, out, &err));
    CHECK(err.find("all the way") != std::string::npos);
    CHECK(out.empty());
    cal.start();
    for (long v : {500L, 0L, 1023L}) cal.feed(0x31, v, 0, 1023);
    for (long v : {500L, 0L, 1023L}) cal.feed(0x30, v, 0, 1023);
    for (long v : {128L, 0L, 255L}) cal.feed(0x35, v, 0, 255);
    CHECK(!cal.finish({{0x30, 512}, {0x31, 900}, {0x35, 128}}, out, &err));   // still pulling the stick back
    CHECK(err.find("let go") != std::string::npos);
    cal.start();
    CHECK(!cal.finish({}, out, &err));                                          // nothing reported at all
    CHECK(parseCalibration("garbage\n48 900 500 100\n").empty());               // broken lines are skipped
}

TEST(pov_1_is_the_hat_switch_and_pov_2_is_the_four_buttons) {
    CHECK_EQ(std::string(ledTitle(LedPov)), std::string("POV 2"));
    CHECK_EQ(std::string(controlName(0x01, 0x39)), std::string("POV 1 (hat switch)"));
    CHECK_EQ(std::string(buttonName(20)), std::string("POV 2 up"));
    CHECK_EQ(std::string(buttonName(23)), std::string("POV 2 left"));
}

TEST(two_pov_buttons_held_together_read_as_a_diagonal) {
    CHECK_EQ(povButtonsText(true, false, false, false), std::string("up"));
    CHECK_EQ(povButtonsText(true, true, false, false), std::string("up-right"));
    CHECK_EQ(povButtonsText(false, true, true, false), std::string("down-right"));
    CHECK_EQ(povButtonsText(false, false, true, true), std::string("down-left"));
    CHECK_EQ(povButtonsText(true, false, false, true), std::string("up-left"));
    CHECK_EQ(povButtonsText(false, false, false, false), std::string(""));
}

TEST(a_dial_holds_its_value_until_it_moves_more_than_the_steady_percentage) {
    CHECK_EQ(steadyValue(100, 103, 0, 255, 2), 100L);    // 1.2 % of travel: jitter
    CHECK_EQ(steadyValue(100, 110, 0, 255, 2), 110L);    // 3.9 %: real
    CHECK_EQ(steadyValue(100, 140, 0, 255, 0), 140L);    // 0: no filtering
    CHECK_EQ(steadyValue(254, 255, 0, 255, 5), 255L);    // the ends are always reachable
    CHECK_EQ(steadyValue(1, 0, 0, 255, 5), 0L);
}

TEST(dial_steadiness_is_saved_in_the_profile) {
    Profile p;
    CHECK_EQ(p.axes.lowerRotary.deadzone, 2);
    p.axes.lowerRotary.deadzone = 7;
    p.axes.upperRotary.deadzone = 4;
    p.axes.slider.deadzone = 0;
    Profile q;
    std::string err;
    CHECK(parse(serialize(p), q, &err));
    CHECK_EQ(q.axes.lowerRotary.deadzone, 7);
    CHECK_EQ(q.axes.upperRotary.deadzone, 4);
    CHECK_EQ(q.axes.slider.deadzone, 0);
    CHECK(dialCurve(q.axes, 0x33) == &q.axes.lowerRotary);
    CHECK(dialCurve(q.axes, 0x34) == &q.axes.upperRotary);
    CHECK(dialCurve(q.axes, 0x36) == &q.axes.slider);
    CHECK(dialCurve(q.axes, 0x30) == nullptr);
}

// ---------------------------------------------------------------- the left wheel's order

TEST(the_left_wheels_pages_can_be_reordered_and_hidden) {
    Profile p;
    std::string err;
    parsePageLine("fly \"Flight\"", p.pages, err);
    parsePageLine("sub \"Sub\" in fly", p.pages, err);
    std::vector<int> def = wheelOrder(p);
    CHECK_EQ(def.size(), (size_t)kPageCount + 1);
    CHECK_EQ(def[PageVorHdg + 1], (int)kPageCount);           // the user's page after the radio pages
    p.wheelOrder = {"@fly", "com1", "xpdr"};
    p.wheelHidden = {"adf", "dme"};
    std::vector<int> o = wheelOrder(p);
    CHECK_EQ(o[0], (int)kPageCount);
    CHECK_EQ(o[1], (int)PageComNav1);
    CHECK_EQ(o[2], (int)PageXpdr);
    CHECK_EQ(o[3], (int)PageComNav2);                         // not named: after the named ones, in the usual order
    CHECK_EQ(o[5], (int)PageVorHdg);
    CHECK_EQ(o[6], (int)PageButtons);
    p.wheelOrder = {"buttons", "@fly", "com1"};
    o = wheelOrder(p);
    CHECK_EQ(o[0], (int)PageComNav2);                         // a new page in a list that names Buttons: before it
    CHECK_EQ(o[4], (int)PageButtons);
    CHECK_EQ(o[5], (int)kPageCount);
    CHECK(std::find(o.begin(), o.end(), (int)PageAdf) == o.end());
    CHECK_EQ(o.back(), (int)PageBrightness);
    CHECK_EQ(wheelEntries(p).size(), (size_t)kPageCount + 1);   // hidden ones are still listed for the editor
    p.wheelHidden.clear();
    for (const WheelEntry& e : wheelEntries(p)) p.wheelHidden.push_back(e.token);
    CHECK_EQ(wheelOrder(p).size(), (size_t)1);                // everything hidden: one page stays
    Profile q;
    CHECK(parse(serialize(p), q, &err));
    CHECK(q.wheelOrder == p.wheelOrder);
    CHECK(q.wheelHidden == p.wheelHidden);
    CHECK(serialize(Profile()).find("wheel_") == std::string::npos);   // nothing written while unused
}


TEST(a_ready_made_profile_says_so_and_a_copy_names_where_it_came_from) {
    Profile p;
    CHECK(!p.standard);
    CHECK(serialize(p).find("standard") == std::string::npos);   // nothing written for ordinary profiles
    p.standard = true;
    p.basedOn = "aircraft-Cirrus SR22";
    Profile q;
    std::string err;
    CHECK(parse(serialize(p), q, &err));
    CHECK(q.standard);
    CHECK_EQ(q.basedOn, std::string("aircraft-Cirrus SR22"));
}

// The ready-made profiles in profiles/ (built by profiles/tools/build_profiles.py): the throttle light follows its
// mode colour in every one, the plugin drives the throttle, and no light uses a dataref X-Plane 12 has replaced.
TEST(the_ready_made_profiles_light_the_throttle_and_let_the_plugin_drive_it) {
    const std::vector<std::string> helis = {"aircraft-Robinson R22 Beta II", "aircraft-Robinson R22 Beta II_mariner", "aircraft-S-76C"};
    int checked = 0;
    DIR* d = opendir("profiles");
    CHECK(d != nullptr);
    while (dirent* e = d ? readdir(d) : nullptr) {
        std::string n = e->d_name;
        if (n.size() < 4 || n.compare(n.size() - 4, 4, ".x52") != 0) continue;
        std::ifstream f("profiles/" + n);
        std::stringstream ss;
        ss << f.rdbuf();
        Profile p;
        std::string err;
        CHECK(parse(ss.str(), p, &err));
        if (!p.standard) continue;   // the user's own profiles kept in the folder
        ++checked;
        for (const LightRule& r : p.rules) CHECK(r.led != LedThrottle);
        bool heli = std::find(helis.begin(), helis.end(), n.substr(0, n.size() - 4)) != helis.end();
        // X-Plane's own axis assignment did not reach the controls: the plugin drives the stick and twist, and the
        // throttle except the helicopters' (their lever is the collective). Default, for any aircraft, is left alone.
        CHECK_EQ(p.axes.overrideXPlane, n != "default.x52");
        CHECK_EQ(p.axes.pluginThrottle, !heli && n != "default.x52");
        if (n != "default.x52") CHECK(p.axes.throttle.invert);   // ready for the override: the X52 lever reads 0 at full power
        for (const LightRule& r : p.rules) CHECK(r.dataref != "sim/cockpit2/controls/flap_ratio" && r.dataref != "sim/cockpit2/controls/parking_brake_ratio");
        bool fighter = n == "aircraft-F-4.x52";   // its three flap steps are on hat 2 (left, right) as well
        for (const Binding& bd : p.bindings)                  // the flaps are on the slider, not on B and C
            for (const Step& st : bd.press) if (!fighter) CHECK(st.text != "sim/flight_controls/flaps_up" && st.text != "sim/flight_controls/flaps_down");
        if (n == "aircraft-Cessna_CitationX.x52") {
            CHECK(p.axes.sliderFlaps);
            CHECK(!p.axes.slider.invert);                     // Invert flaps off: slider away from you deploys them
            // Modes: 1 takeoff, 2 cruise, 3 landing. One autopilot button (B) in every mode.
            CHECK_EQ(p.exact(4, kLayerAny)->press[0].text, std::string("sim/autopilot/servos_toggle"));
            CHECK(!p.exact(4, 1) && !p.exact(4, 2) && !p.exact(4, 3));
            CHECK_EQ(p.exact(5, 2)->press[0].text, std::string("sim/autopilot/altitude_hold"));   // C, cruise
            CHECK_EQ(p.exact(5, 3)->press[0].text, std::string("sim/autopilot/approach"));        // C, landing
            CHECK_EQ(p.exact(7, 3)->press[0].text, std::string("sim/engines/thrust_reverse_hold"));   // D, landing: reverse
            CHECK_EQ(p.modeNames[1], std::string("Cruise"));
            CHECK_EQ(p.modeBannerMs, 1500);
        }
        // The parking brake is a hold on A in every mode (tap: gear, or the landing light in cruise); Fire is free.
        CHECK(!p.exact(2, kLayerAny) || p.exact(2, kLayerAny)->press[0].text != "sim/flight_controls/brakes_toggle_max");
        for (int mode = 1; mode <= 3 && n.compare(0, 9, "aircraft-") == 0; ++mode) {
            const Binding* a = p.exact(3, mode);
            CHECK(a != nullptr);
            if (a) {
                CHECK_EQ(a->hold.size(), (size_t)1);
                if (!a->hold.empty()) CHECK_EQ(a->hold[0].text, std::string("sim/flight_controls/brakes_toggle_max"));
            }
        }
        if (n == "aircraft-F-14D.x52") {   // the F-14's own weapon select switch on the throttle hat
            CHECK_EQ(p.exact(1, kLayerAny)->press[0].text, std::string("sim/weapons/fire_any_armed"));      // the trigger fires the selected weapon
            CHECK_EQ(p.exact(2, kLayerAny)->press[0].text, std::string("sim/weapons/deploy_flares"));      // no bombs in the F-14
            CHECK_EQ(p.exact(24, 2)->press[0].text, std::string("laminar/f14/fltc/cs_wpn_select_switch1"));
            CHECK_EQ(p.exact(25, 2)->press[0].text, std::string("laminar/f14/fltc/cs_wpn_select_switch2"));
            CHECK_EQ(p.exact(26, 2)->press[0].text, std::string("laminar/f14/fltc/cs_wpn_select_switch3"));
            CHECK_EQ(p.exact(27, 2)->press[0].text, std::string("laminar/f14/fltc/cs_wpn_select_switch4"));
        }
        if (n == "aircraft-F-4.x52") {   // no rudder trim: flaps one step on hat 2 left and right, pitch trim stays
            CHECK_EQ(p.exact(21, kLayerAny)->press[0].text, std::string("sim/flight_controls/flaps_down"));
            CHECK_EQ(p.exact(23, kLayerAny)->press[0].text, std::string("sim/flight_controls/flaps_up"));
            CHECK_EQ(p.exact(20, kLayerAny)->press[0].text, std::string("sim/flight_controls/pitch_trim_down"));
        }
        if (n == "aircraft-F-14D.x52") {   // hat 2 is the weapon select, in every mode: up 1, right 2, down 3, left 4
            CHECK_EQ(p.exact(20, kLayerAny)->press[0].text, std::string("laminar/f14/fltc/cs_wpn_select_switch1"));
            CHECK_EQ(p.exact(21, kLayerAny)->press[0].text, std::string("laminar/f14/fltc/cs_wpn_select_switch2"));
            CHECK_EQ(p.exact(22, kLayerAny)->press[0].text, std::string("laminar/f14/fltc/cs_wpn_select_switch3"));
            CHECK_EQ(p.exact(23, kLayerAny)->press[0].text, std::string("laminar/f14/fltc/cs_wpn_select_switch4"));
        }
        if (n == "aircraft-F-4.x52") {
            CHECK_EQ(p.exact(2, kLayerAny)->press[0].text, std::string("sim/weapons/fire_air_to_ground"));   // Fire drops bombs
            CHECK_EQ(p.exact(7, 2)->press[0].text, std::string("sim/weapons/master_arm_on"));            // weapons: cruise mode
        }
    }
    if (d) closedir(d);
    CHECK_EQ(checked, 24);
}

TEST(an_older_profile_gets_takeoff_cruise_landing_and_the_parking_brake_on_a) {
    // Before mode_layout 2: mode 2 was approach and landing, mode 3 cruise; the parking brake sat on Fire.
    const std::string old =
        "x52pro-profile 8\n"
        "bind 2 any press \"Parking brake\" cmd:sim/flight_controls/brakes_toggle_max\n"
        "bind 3 any press \"Gear\" cmd:sim/flight_controls/landing_gear_toggle\n"
        "bind 4 2 press \"Flaps\" cmd:sim/flight_controls/flaps_down\n"
        "bind 4 3 press \"AP ALT hold\" cmd:sim/autopilot/altitude_hold\n"
        "bind 5 5 press \"Pinkie 2\" cmd:sim/a\n"
        "bind 5 6 press \"Pinkie 3\" cmd:sim/b\n";
    Profile p;
    std::string err;
    CHECK(parse(old, p, &err));
    CHECK_EQ(p.exact(4, 2)->press[0].text, std::string("sim/autopilot/altitude_hold"));   // cruise is mode 2 now
    CHECK_EQ(p.exact(4, 3)->press[0].text, std::string("sim/flight_controls/flaps_down"));
    CHECK_EQ(p.exact(5, 5)->press[0].text, std::string("sim/b"));                       // and with the pinkie
    CHECK_EQ(p.exact(5, 6)->press[0].text, std::string("sim/a"));
    CHECK(p.exact(2, kLayerAny) == nullptr);                                            // Fire is free
    const Binding* a = p.exact(3, kLayerAny);
    CHECK(a != nullptr);
    CHECK_EQ(a->press[0].text, std::string("sim/flight_controls/landing_gear_toggle"));
    CHECK_EQ(a->hold[0].text, std::string("sim/flight_controls/brakes_toggle_max"));
    CHECK_EQ(p.modeNames[0], std::string("Takeoff"));
    CHECK_EQ(p.modeBannerMs, 1500);
    // Written again it says so, and reading it back changes nothing (no second swap).
    Profile q;
    CHECK(parse(serialize(p), q, &err));
    CHECK_EQ(q.exact(4, 2)->press[0].text, std::string("sim/autopilot/altitude_hold"));
    CHECK_EQ(q.exact(4, 3)->press[0].text, std::string("sim/flight_controls/flaps_down"));
    // A Fire button the user changed stays, and so does an A that has a hold of its own.
    CHECK(parse("x52pro-profile 8\nbind 2 any press \"Zoom\" cmd:sim/general/zoom_in\n", q, &err));
    CHECK(q.exact(2, kLayerAny) != nullptr);
    CHECK(parse("x52pro-profile 8\nbind 2 any press \"Parking brake\" cmd:sim/flight_controls/brakes_toggle_max\n"
                "bind 3 any press \"Gear\" cmd:sim/a\nbind 3 any hold \"Mine\" cmd:sim/b\n", q, &err));
    CHECK(q.exact(2, kLayerAny) != nullptr);
    CHECK_EQ(q.exact(3, kLayerAny)->hold[0].text, std::string("sim/b"));
    // Mode names and the banner time are saved and read back; the time is kept between 0 and 10 s.
    q.modeNames[2] = "Final";
    q.modeBannerMs = 2500;
    Profile r;
    CHECK(parse(serialize(q), r, &err));
    CHECK_EQ(r.modeNames[2], std::string("Final"));
    CHECK_EQ(r.modeBannerMs, 2500);
    CHECK(parse("x52pro-profile 9\nmode_layout = 2\nmode_banner_ms = 99999\n", r, &err));
    CHECK_EQ(r.modeBannerMs, 10000);
}

TEST(the_slider_picks_the_nearest_flap_detent_and_is_saved) {
    CHECK_EQ(flapHandleFor(0.0, 4), 0.0);
    CHECK_EQ(flapHandleFor(0.55, 4), 0.5);
    CHECK_EQ(flapHandleFor(0.9, 4), 1.0);
    CHECK_EQ(flapHandleFor(0.3, 0), 0.3);                    // no detents: as it is
    CHECK_EQ(flapHandleFor(1.7, 3), 1.0);
    Profile p;
    CHECK(!p.axes.sliderFlaps);
    p.axes.sliderFlaps = true;
    p.axes.slider.invert = true;
    Profile q;
    std::string err;
    CHECK(parse(serialize(p), q, &err));
    CHECK(q.axes.sliderFlaps);
    CHECK(q.axes.slider.invert);                              // the app's switch is kept
    Profile old;                                              // an older file: axis_override alone drove the throttle too
    CHECK(parse("x52pro-profile 3\naxis_override = 1\n", old, &err));
    CHECK(old.axes.pluginThrottle);
    Profile split;
    CHECK(parse("x52pro-profile 3\naxis_override = 1\nthrottle_plugin = 0\n", split, &err));
    CHECK(split.axes.overrideXPlane && !split.axes.pluginThrottle);
}

// ---- side-by-side radio screens

TEST(side_com_page_browses_names_and_standbys) {
    SimData d;
    d.com1 = 118005;
    d.com1s = 121900;
    d.com2 = 121505;
    d.com2s = 122800;
    PageCursor c;
    Lines p = formatSimPage(PageComNav1, d, c, true);
    CHECK_EQ(p.l[0], std::string("[COM1]   COM2"));
    CHECK_EQ(p.l[1], std::string(" 118.005 121.505"));
    CHECK_EQ(p.l[2], std::string(" 121.900 122.800"));
    CHECK_EQ(cmds(pageInput(PageComNav1, c, Wheel::Select, false, 1, true)), std::string("sim/radios/com1_standy_flip "));
    pageInput(PageComNav1, c, Wheel::Down, false, 1, true);
    CHECK_EQ(formatSimPage(PageComNav1, d, c, true).l[2], std::string(">121.900 122.800"));
    pageInput(PageComNav1, c, Wheel::Down, false, 1, true);
    CHECK_EQ(formatSimPage(PageComNav1, d, c, true).l[0], std::string(" COM1   [COM2]"));
    CHECK_EQ(cmds(pageInput(PageComNav1, c, Wheel::Select, false, 1, true)), std::string("sim/radios/com2_standy_flip "));
    pageInput(PageComNav1, c, Wheel::Down, false, 1, true);
    CHECK_EQ(formatSimPage(PageComNav1, d, c, true).l[2], std::string(" 121.900>122.800"));
    pageInput(PageComNav1, c, Wheel::Down, false, 1, true);
    CHECK_EQ(c.item, 0);   // wraps
    pageInput(PageComNav1, c, Wheel::Up, false, 1, true);
    CHECK_EQ(c.item, 3);
}

TEST(side_com_edit_view_is_centred_and_steps_mhz_then_khz) {
    SimData d;
    d.com2 = 121505;
    d.com2s = 122800;
    PageCursor c;
    c.item = 3;
    pageInput(PageComNav1, c, Wheel::Select, false, 1, true);   // open the edit view
    Lines p = formatSimPage(PageComNav1, d, c, true);
    CHECK_EQ(p.l[0], std::string("      COM2"));
    CHECK_EQ(p.l[1], std::string("     121.505"));
    CHECK_EQ(p.l[2], std::string("    >122.800"));
    CHECK_EQ(cmds(pageInput(PageComNav1, c, Wheel::Up, false, 1, true)), std::string("sim/radios/stby_com2_coarse_up "));
    pageInput(PageComNav1, c, Wheel::Select, false, 1, true);   // to kHz
    CHECK_EQ(formatSimPage(PageComNav1, d, c, true).l[2], std::string("     122.800<"));
    CHECK_EQ(cmds(pageInput(PageComNav1, c, Wheel::Down, true, 3, true)),
             std::string("sim/radios/stby_com2_fine_down_833 sim/radios/stby_com2_fine_down_833 sim/radios/stby_com2_fine_down_833 "));
    CHECK_EQ(cmds(pageInput(PageComNav1, c, Wheel::Up, false, 1, true)), std::string("sim/radios/stby_com2_fine_up "));
    pageInput(PageComNav1, c, Wheel::Select, false, 1, true);   // done
    CHECK(!c.editing);
    CHECK_EQ(c.item, 3);
    CHECK_EQ(formatSimPage(PageComNav1, d, c, true).l[0], std::string(" COM1    COM2"));   // the browse view again
}

TEST(side_nav_page_uses_nav_commands) {
    SimData d;
    d.nav1 = 11050;
    d.nav1s = 11390;
    d.nav2 = 10800;
    d.nav2s = 11795;
    PageCursor c;
    Lines p = formatSimPage(PageComNav2, d, c, true);
    CHECK_EQ(p.l[0], std::string("[NAV1]   NAV2"));
    CHECK_EQ(p.l[1], std::string(" 110.50  108.00"));
    CHECK_EQ(cmds(pageInput(PageComNav2, c, Wheel::Select, false, 1, true)), std::string("sim/radios/nav1_standy_flip "));
    c.item = 1;
    pageInput(PageComNav2, c, Wheel::Select, false, 1, true);
    CHECK_EQ(formatSimPage(PageComNav2, d, c, true).l[2], std::string("    >113.90"));
    CHECK_EQ(cmds(pageInput(PageComNav2, c, Wheel::Up, false, 1, true)), std::string("sim/radios/stby_nav1_coarse_up "));
    pageInput(PageComNav2, c, Wheel::Select, false, 1, true);
    CHECK_EQ(cmds(pageInput(PageComNav2, c, Wheel::Up, false, 2, true)),
             std::string("sim/radios/stby_nav1_fine_up sim/radios/stby_nav1_fine_up "));
}

TEST(side_adf_page_edits_hundreds_tens_ones) {
    SimData d;
    d.adf1 = 365;
    d.adf1s = 379;
    d.adf2 = 410;
    d.adf2s = 520;
    PageCursor c;
    CHECK_EQ(formatSimPage(PageAdf, d, c, true).l[0], std::string("[ADF1]   ADF2"));
    CHECK_EQ(formatSimPage(PageAdf, d, c, true).l[1], std::string("  365     410"));
    c.item = 3;
    pageInput(PageAdf, c, Wheel::Select, false, 1, true);
    CHECK_EQ(formatSimPage(PageAdf, d, c, true).l[0], std::string("      ADF2"));
    CHECK_EQ(formatSimPage(PageAdf, d, c, true).l[2], std::string("     > 520"));
    CHECK_EQ(cmds(pageInput(PageAdf, c, Wheel::Up, false, 5, true)), std::string("sim/radios/stby_adf2_hundreds_up "));   // no repeat
    pageInput(PageAdf, c, Wheel::Select, false, 1, true);
    CHECK_EQ(formatSimPage(PageAdf, d, c, true).l[2], std::string("       520<10"));
    CHECK_EQ(cmds(pageInput(PageAdf, c, Wheel::Down, false, 1, true)), std::string("sim/radios/stby_adf2_tens_down "));
    pageInput(PageAdf, c, Wheel::Select, false, 1, true);
    CHECK_EQ(formatSimPage(PageAdf, d, c, true).l[2], std::string("       520<1"));
    CHECK_EQ(cmds(pageInput(PageAdf, c, Wheel::Up, false, 1, true)), std::string("sim/radios/stby_adf2_ones_up "));
    pageInput(PageAdf, c, Wheel::Select, false, 1, true);
    CHECK(!c.editing);
    CHECK_EQ(cmds(pageInput(PageAdf, c, Wheel::Select, false, 1, true)), std::string(""));   // standby: opens editing again
    CHECK(c.editing);
}

TEST(side_pages_fit_and_clamp_out_of_range_cursors) {
    SimData d;
    d.com1 = d.com1s = d.com2 = d.com2s = 136975;
    d.nav1 = d.nav1s = d.nav2 = d.nav2s = 11795;
    d.adf1 = d.adf1s = d.adf2 = d.adf2s = 1799;
    for (int page : {(int)PageComNav1, (int)PageComNav2, (int)PageAdf})
        for (int item = -1; item < 7; ++item)
            for (int part = -1; part < 4; ++part)
                for (bool ed : {false, true}) {
                    PageCursor c;
                    c.item = item;
                    c.part = part;
                    c.editing = ed;
                    Lines p = formatSimPage(page, d, c, true);
                    for (auto& l : p.l) CHECK(l.size() <= 16);
                    pageInput(page, c, Wheel::Up, false, 1, true);   // must not crash
                    CHECK(c.item >= 0 && c.item < 4);
                }
    PageCursor c;   // side state left over, the layout now classic
    c.item = 1;
    c.editing = true;
    c.part = 2;
    CHECK(formatSimPage(PageComNav1, d, c).l[0].size() <= 16);
    CHECK_EQ(pageItemCount(PageAdf, true), 4);
    CHECK_EQ(pageItemCount(PageAdf), 8);
    CHECK_EQ(pageItemCount(PageXpdr, true), 5);
    CHECK_EQ(std::string(pageName(PageComNav2, true)), std::string("NAV 1+2"));
    CHECK_EQ(std::string(pageName(PageComNav2, false)), std::string("COM2 / NAV2"));
    CHECK_EQ(std::string(pageName(PageXpdr, true)), std::string("Transponder"));
}

TEST(classic_radio_pages_mark_more_below_on_the_first_unit_only) {
    SimData d;
    d.adf1 = 365;
    d.adf2 = 410;
    d.adf1s = 379;
    d.adf2s = 520;
    PageCursor c;
    CHECK_EQ(formatSimPage(PageAdf, d, c).l[2], std::string("ADF2  410  520 v"));
    c.item = 3;
    CHECK_EQ(formatSimPage(PageAdf, d, c).l[2], std::string("ADF2  410  520 v"));
    c.item = 4;
    CHECK_EQ(formatSimPage(PageAdf, d, c).l[2], std::string("ADF1  365  379"));
    SimData n;
    n.com2 = 127275;
    n.com2s = 135000;
    n.nav2 = 10800;
    n.nav2s = 11795;
    PageCursor k;
    k.item = 2;
    CHECK_EQ(formatSimPage(PageComNav2, n, k).l[2], std::string("N108.00  117.95v"));
    k.item = 4;
    CHECK_EQ(formatSimPage(PageComNav2, n, k).l[2], std::string("C127.275 135.000"));   // unchanged
}

// ---- radio layout and a page order per mode

TEST(radio_layout_round_trips_and_defaults_to_classic) {
    Profile p = defaultProfile();
    CHECK(serialize(p).find("radio_layout") == std::string::npos);
    p.radioSide = true;
    CHECK(serialize(p).find("radio_layout = side\n") != std::string::npos);
    Profile q;
    std::string err;
    CHECK(parse(serialize(p), q, &err));
    CHECK(q.radioSide);
    Profile r;
    CHECK(parse("x52pro-profile 3\nradio_layout = classic\n", r, &err));
    CHECK(!r.radioSide);
}

TEST(a_mode_can_have_its_own_wheel_order) {
    Profile p = defaultProfile();
    p.modeWheel[1].own = true;
    p.modeWheel[1].order = {"ap", "com1"};
    std::vector<int> two = wheelOrder(p, 2);
    CHECK_EQ(two[0], (int)PageAutopilot);
    CHECK_EQ(two[1], (int)PageComNav1);
    CHECK(wheelOrder(p, 1) == wheelOrder(p));   // mode 1 has none: the shared order
    CHECK(wheelOrder(p, 3) == wheelOrder(p));
    CHECK(wheelOrder(p, 0) == wheelOrder(p));
    std::string text = serialize(p);
    CHECK(text.find("wheel_order.2 = ap com1\n") != std::string::npos);
    CHECK(text.find("wheel_hidden.2 =\n") != std::string::npos);
    CHECK(text.find("wheel_order.1") == std::string::npos);
    Profile q;
    std::string err;
    CHECK(parse(text, q, &err));
    CHECK(q.modeWheel[1].own && q.modeWheel[1].hidden.empty());
    CHECK(q.modeWheel[1].order == p.modeWheel[1].order);
    CHECK(!q.modeWheel[0].own && !q.modeWheel[2].own);
}

TEST(a_mode_with_only_hidden_pages_is_its_own_and_hiding_everything_keeps_buttons) {
    Profile q;
    std::string err;
    CHECK(parse("x52pro-profile 3\nmode_layout = 2\nwheel_hidden = adf\nwheel_hidden.3 = com1 com2 adf dme xpdr ap vorhdg buttons brightness\n", q, &err));
    CHECK(q.modeWheel[2].own && q.modeWheel[2].order.empty());
    std::vector<int> three = wheelOrder(q, 3);
    CHECK_EQ((int)three.size(), 1);
    CHECK_EQ(three[0], (int)PageButtons);
    std::vector<int> one = wheelOrder(q, 1);   // shared: only ADF hidden
    CHECK(std::find(one.begin(), one.end(), (int)PageAdf) == one.end());
    CHECK(std::find(one.begin(), one.end(), (int)PageDme) != one.end());
    std::vector<WheelEntry> e = wheelEntries(q, 3);
    CHECK_EQ(e[0].token, std::string("com1"));
    CHECK(e[0].hidden);
}

TEST(history_undoes_and_redoes_steps) {
    History h;
    h.reset("a");
    CHECK(!h.canUndo() && !h.canRedo());
    h.record("b", 10);
    h.record("c", 20);
    std::string t;
    CHECK(h.undo(t));
    CHECK_EQ(t, std::string("b"));
    CHECK(h.undo(t));
    CHECK_EQ(t, std::string("a"));
    CHECK(!h.undo(t));
    CHECK(h.redo(t));
    CHECK_EQ(t, std::string("b"));
    h.record("x", 30);                       // a new edit drops what could be redone
    CHECK(!h.canRedo());
    CHECK(h.undo(t));
    CHECK_EQ(t, std::string("b"));
}

TEST(history_joins_quick_edits_into_one_step) {
    History h;
    h.reset("a");
    h.record("b", 10.0);
    h.record("c", 10.4);                     // a drag: edits within a second of each other
    h.record("d", 11.0);
    std::string t;
    CHECK(h.undo(t));
    CHECK_EQ(t, std::string("a"));           // all three at once
    CHECK(!h.canUndo());
    h.record("e", 12.0);
    h.undo(t);                               // a step made by undo never joins the next edit
    h.record("f", 12.1);
    CHECK(h.undo(t));
    CHECK_EQ(t, std::string("a"));
}

TEST(history_ignores_no_change_and_is_capped) {
    History h;
    h.reset("a");
    h.record("a", 1);
    CHECK(!h.canUndo());
    for (int i = 1; i <= 80; ++i) h.record(std::to_string(i), i * 10.0);
    int steps = 0;
    std::string t;
    while (h.undo(t)) ++steps;
    CHECK_EQ(steps, (int)History::kMax);
    h.reset("z");
    CHECK(!h.canUndo());
    CHECK_EQ(h.current(), std::string("z"));
}

TEST(update_compares_versions) {
    CHECK(isNewerVersion("1.2.2", "v1.3.0"));
    CHECK(isNewerVersion("1.9.9", "2.0.0"));
    CHECK(isNewerVersion("1.2", "1.10.0"));        // numbers, not text: 10 > 2
    CHECK(isNewerVersion("1.2.2", "1.3.0-beta"));  // suffix ignored
    // The third number is for small changes: never worth an update prompt.
    CHECK(!isNewerVersion("1.2.2", "1.2.3"));
    CHECK(!isNewerVersion("1.2.2", "v1.2.10"));
    CHECK(!isNewerVersion("1.2.0", "1.2.100"));
    CHECK(!isNewerVersion("1.2.2", "1.2.2"));
    CHECK(!isNewerVersion("1.3.0", "1.2.9"));
    CHECK(!isNewerVersion("dev", "1.0.0"));        // a development build never reports an update
    CHECK(!isNewerVersion("1.0.0", ""));
    CHECK(!isNewerVersion("1.0.0", "latest"));
    int v[3];
    CHECK(parseVersion("v10.20.300", v) && v[0] == 10 && v[1] == 20 && v[2] == 300);
}

TEST(dial_functions_round_trip_through_the_profile) {
    Profile p;
    p.axes.lowerUse = DialUse::Zoom;
    p.axes.upperUse = DialUse::None;
    p.axes.lowerRotary.invert = true;
    std::string text = serialize(p);
    CHECK(text.find("rotary_lower = 2\n") != std::string::npos);
    CHECK(text.find("rotary_lower_invert = 1\n") != std::string::npos);
    CHECK(text.find("x52pro-profile 10\n") == 0);
    Profile q;
    std::string err;
    CHECK(parse(text, q, &err));
    CHECK(q.axes.lowerUse == DialUse::Zoom);
    CHECK(q.axes.upperUse == DialUse::None);
    CHECK(q.axes.lowerRotary.invert);
    CHECK(!q.axes.upperRotary.invert);
}

TEST(an_older_profile_gets_speed_brake_and_zoom_on_the_dials) {
    Profile q;
    std::string err;
    CHECK(parse("x52pro-profile 6\nname = old\n", q, &err));
    CHECK(q.axes.lowerUse == DialUse::SpeedBrake);
    CHECK(q.axes.upperUse == DialUse::Zoom);
    CHECK(!q.axes.lowerRotary.invert && !q.axes.upperRotary.invert);
}

TEST(a_dial_set_to_nothing_stays_nothing_in_an_older_file) {
    Profile q;
    std::string err;
    CHECK(parse("x52pro-profile 3\nrotary_lower = 0\nrotary_upper_invert = 1\n", q, &err));
    CHECK(q.axes.lowerUse == DialUse::None);
    CHECK(q.axes.upperUse == DialUse::Zoom);
    CHECK(q.axes.upperRotary.invert);
}

TEST(zoom_strength_is_kept_in_the_profile) {
    Profile p;
    CHECK_EQ(p.axes.zoomSteps, 96);                      // steps of X-Plane's zoom per full turn of a dial
    p.axes.zoomSteps = 200;
    std::string text = serialize(p);
    CHECK(text.find("rotary_zoom_steps = 200\n") != std::string::npos);
    Profile q;
    std::string err;
    CHECK(parse(text, q, &err));
    CHECK_EQ(q.axes.zoomSteps, 200);
    CHECK(parse("x52pro-profile 8\nrotary_zoom_steps = 5000\n", q, &err));
    CHECK_EQ(q.axes.zoomSteps, 480);
    CHECK(parse("x52pro-profile 8\nrotary_zoom_steps = 1\n", q, &err));
    CHECK_EQ(q.axes.zoomSteps, 12);
}

TEST(an_unknown_dial_function_reads_as_nothing) {
    Profile q;
    std::string err;
    CHECK(parse("x52pro-profile 8\nrotary_upper = 9\nrotary_lower = -1\n", q, &err));
    CHECK(q.axes.upperUse == DialUse::None);
    CHECK(q.axes.lowerUse == DialUse::None);
}


TEST(lever_value_covers_the_range_from_to) {
    CHECK(near(leverValue(0, -0.3, 0.5, 0, 100), -0.3));
    CHECK(near(leverValue(1, -0.3, 0.5, 0, 100), 0.5));
    CHECK(near(leverValue(0.5, 0, 1, 20, 60), 0.4));
    CHECK(near(leverValue(0.25, 0, 1, 100, 0), 0.75));   // From above To turns the other way
    CHECK(near(leverValue(0.7, 0, 1, 50, 50), 0.5));     // From = To: one value
}

TEST(a_lever_waits_until_the_dial_reaches_x_planes_value) {
    LeverPickup s;
    CHECK(!leverMayWrite(s, 0.1, 0.6, 0.02));
    CHECK(!leverMayWrite(s, 0.3, 0.6, 0.02));
    CHECK(leverMayWrite(s, 0.65, 0.6, 0.02));           // crossed X-Plane's value: taken over
    CHECK(s.held);
    CHECK(near(s.lastWritten, 0.65));
    CHECK(leverMayWrite(s, 0.7, 0.65, 0.02));           // held: follows the dial
    CHECK(!leverMayWrite(s, 0.72, 0.2, 0.02));          // a button moved it to 0.2: waiting again
    CHECK(!s.held);
    CHECK(!leverMayWrite(s, 0.21, 0.2, 0.02));          // let go: near is not enough, it must cross
    CHECK(leverMayWrite(s, 0.19, 0.2, 0.02));           // across 0.2: taken over
    LeverPickup fresh;
    CHECK(leverMayWrite(fresh, 0.21, 0.2, 0.02));       // first takeover: within 1 % of the range is enough
}

TEST(a_trim_step_smaller_than_1_percent_lets_go_of_the_dial) {
    LeverPickup s;
    CHECK(leverMayWrite(s, 0.5, 0.5, 0.02));
    CHECK(leverMayWrite(s, 0.5, 0.5, 0.02));
    CHECK(!leverMayWrite(s, 0.5, 0.505, 0.02));         // a trim button: 0.25 % of the range
    CHECK(!s.held);
    CHECK(!leverMayWrite(s, 0.49, 0.505, 0.02));        // turned away: the button's value stays
    CHECK(leverMayWrite(s, 0.51, 0.505, 0.02));         // turned across it: taken over
}

TEST(split_engine_levers_stay_split_under_the_dial) {
    LeverPickup s;
    auto out = leverEngines(s, {0.6, 0.4}, 0.62, 0, 1, 1e-4);   // engine 2 set 0.2 lower
    CHECK(near(out[0], 0.62));
    CHECK(near(out[1], 0.42));
    out = leverEngines(s, {out[0], out[1]}, 0.1, 0, 1, 1e-4);
    CHECK(near(out[0], 0.1));
    CHECK(near(out[1], 0.0));                           // held at the end of the range...
    out = leverEngines(s, {out[0], out[1]}, 0.5, 0, 1, 1e-4);
    CHECK(near(out[1], 0.3));                           // ...and split again on the way back
    out = leverEngines(s, {0.5, 0.45}, 0.55, 0, 1, 1e-4);   // the mouse moved engine 2 to 0.45
    CHECK(near(out[1], 0.5));                           // keeps its new distance
}

TEST(a_lever_with_an_empty_range_never_writes) {
    LeverPickup s;
    CHECK(!leverMayWrite(s, 0, 0, 0));
    CHECK(!leverMayWrite(s, 0, 0, 0));
}

TEST(lever_jobs_have_names_and_settings) {
    CHECK_EQ(std::string(dialUseName(DialUse::None)), std::string("Nothing"));
    CHECK_EQ(std::string(dialUseName(DialUse::RudderTrim)), std::string("Rudder trim"));
    CHECK(leverJob(DialUse::Zoom) == nullptr);
    CHECK(leverJob(DialUse::SpeedBrake) == nullptr);
    const LeverJob* j = leverJob(DialUse::CowlFlaps);
    CHECK(j != nullptr && j->perEngine);
    CHECK(leverJob(DialUse::Mixture) != nullptr && leverJob(DialUse::Mixture)->perEngine);   // one per engine: split levers
    const LeverJob* prop = leverJob(DialUse::Prop);         // the governor's speed, between the aircraft's own limits
    CHECK_EQ(std::string(prop->dataref), std::string("sim/cockpit2/engine/actuators/prop_rotation_speed_rad_sec"));
    CHECK(prop->limAbsolute);
    CHECK(!leverJob(DialUse::RudderTrim)->limAbsolute);
    CHECK_EQ(kDialUseCount, 9);
}

TEST(lever_jobs_know_x_planes_own_axis_numbers) {
    // X-Plane's joy_use list: prop 8 (prop 1-4: 24-27), mixture 9 (28-31), carb heat 10, aileron trim 18,
    // rudder trim 19, cowl flaps 39.
    auto has = [](DialUse u, int n) {
        for (int a : leverJob(u)->xpAxis)
            if (a == n) return true;
        return false;
    };
    CHECK(has(DialUse::Prop, 8) && has(DialUse::Prop, 24) && has(DialUse::Prop, 27));
    CHECK(has(DialUse::Mixture, 9) && has(DialUse::Mixture, 28) && has(DialUse::Mixture, 31));
    CHECK(has(DialUse::CarbHeat, 10));
    CHECK(has(DialUse::AileronTrim, 18));
    CHECK(has(DialUse::RudderTrim, 19));
    CHECK(has(DialUse::CowlFlaps, 39));
    CHECK(!has(DialUse::Prop, 9) && !has(DialUse::RudderTrim, 18));
}

TEST(dial_ranges_and_new_jobs_round_trip_through_the_profile) {
    Profile p;
    p.axes.lowerUse = DialUse::RudderTrim;
    p.axes.lowerFrom = 25;
    p.axes.lowerTo = 75;
    std::string text = serialize(p);
    CHECK(text.find("rotary_lower = 5\n") != std::string::npos);
    CHECK(text.find("rotary_lower_range = 25 75\n") != std::string::npos);
    CHECK(text.find("rotary_upper_range = 0 100\n") != std::string::npos);
    Profile q;
    std::string err;
    CHECK(parse(text, q, &err));
    CHECK(q.axes.lowerUse == DialUse::RudderTrim);
    CHECK_EQ(q.axes.lowerFrom, 25);
    CHECK_EQ(q.axes.lowerTo, 75);
    CHECK(parse("x52pro-profile 8\nrotary_upper_range = 120 -5\nrotary_lower = 8\n", q, &err));
    CHECK_EQ(q.axes.upperFrom, 100);
    CHECK_EQ(q.axes.upperTo, 0);
    CHECK(q.axes.lowerUse == DialUse::CarbHeat);
}

TEST(an_older_profile_gets_the_full_range) {
    Profile q;
    std::string err;
    CHECK(parse("x52pro-profile 7\nrotary_lower = 3\n", q, &err));
    CHECK(q.axes.lowerUse == DialUse::Prop);
    CHECK_EQ(q.axes.lowerFrom, 0);
    CHECK_EQ(q.axes.lowerTo, 100);
    CHECK_EQ(q.axes.upperFrom, 0);
    CHECK_EQ(q.axes.upperTo, 100);
}

TEST(speed_brake_steps) {
    CHECK_EQ(speedBrakeFor(0.0), -0.5);    // armed
    CHECK_EQ(speedBrakeFor(0.25), 0.0);    // retracted
    CHECK_EQ(speedBrakeFor(0.3), 0.0);
    CHECK_EQ(speedBrakeFor(0.6), 0.5);     // half
    CHECK_EQ(speedBrakeFor(0.99), 1.0);    // full
    CHECK_EQ(speedBrakeFor(1.0), 1.0);
}

TEST(zoom_clicks_carry_the_remainder) {
    double anchor = 0.5;
    CHECK_EQ(zoomClicks(anchor, 0.5 + 1.5 / 24), 1);
    CHECK(std::fabs(anchor - (0.5 + 1.0 / 24)) < 1e-9);
    CHECK_EQ(zoomClicks(anchor, 0.5), -1);
    CHECK(std::fabs(anchor - 0.5) < 1e-9);
    CHECK_EQ(zoomClicks(anchor, 0.5 + 0.4 / 24), 0);
    CHECK_EQ(zoomClicks(anchor, 0.0), -12);
}

TEST(an_older_fire_button_drops_bombs_unless_the_user_changed_it) {
    Profile p;
    std::string err;
    CHECK(parse("x52pro-profile 9\nmode_layout = 2\nbind 2 any press \"Fire weapon\" cmd:sim/weapons/fire_any_armed\n", p, &err));
    CHECK_EQ(p.exact(2, kLayerAny)->press[0].text, std::string("sim/weapons/fire_air_to_ground"));
    CHECK_EQ(p.exact(2, kLayerAny)->label, std::string("Drop bomb"));
    Profile q;
    CHECK(parse("x52pro-profile 9\nmode_layout = 2\nbind 2 any press \"Mine\" cmd:my/command\n", q, &err));
    CHECK_EQ(q.exact(2, kLayerAny)->press[0].text, std::string("my/command"));
    Profile r;   // a profile already at version 10 is not touched
    CHECK(parse("x52pro-profile 10\nmode_layout = 2\nbind 2 any press \"Fire weapon\" cmd:sim/weapons/fire_any_armed\n", r, &err));
    CHECK_EQ(r.exact(2, kLayerAny)->press[0].text, std::string("sim/weapons/fire_any_armed"));
}
