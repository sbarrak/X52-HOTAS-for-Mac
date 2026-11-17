#include <map>
#include "check.h"
#include "../src/core/engine.h"
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
    CHECK_EQ(std::string(buttonName(40)), std::string(""));
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
    CHECK_EQ(p.l[2], std::string("N110.50  113.90"));
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
    CHECK_EQ(std::string(controlName(0x01, 0x39)), std::string("POV hat"));
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
    CHECK_EQ(q.version, 3);
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
    CHECK_EQ(again.version, 3);
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
    CHECK(text.find("rule e > 0.5 red green sim/cockpit2/controls/parking_brake_ratio") != std::string::npos);
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
