// Tests of the manual data (src/core/manual.cpp): what a template may show about a profile.
#include <set>

#include "../src/core/manual.h"
#include "check.h"

using namespace x52;

namespace {
const ManualRow* rowOf(const ManualData& d, const std::string& list, const std::string& button) {
    auto it = d.lists.find(list);
    if (it == d.lists.end()) return nullptr;
    for (const ManualRow& r : it->second)
        if (r.at("button.name") == button) return &r;
    return nullptr;
}
Binding& bindAs(Profile& p, int button, int layer, const std::string& label, const std::string& command) {
    Binding& b = p.edit(button, layer);
    b.label = label;
    Action a;
    std::string err;
    actionFromText("cmd:" + command, a, &err);
    b.press = a;
    return b;
}
}  // namespace

TEST(manual_values_name_the_profile_the_modes_and_the_schema) {
    Profile p;
    p.name = "My Cessna";
    p.modeNames[0] = "Takeoff";
    p.modeNames[1] = "Cruise";
    p.modeNames[2] = "Landing";
    ManualData d = buildManualData(p, "1.5.0");
    CHECK_EQ(d.value("manual.schema"), std::to_string(kManualSchema));
    CHECK_EQ(d.value("app.version"), std::string("1.5.0"));
    CHECK_EQ(d.value("profile.name"), std::string("My Cessna"));
    CHECK_EQ(d.value("mode.1.name"), std::string("Takeoff"));
    CHECK_EQ(d.value("mode.2.name"), std::string("Cruise"));
    CHECK_EQ(d.value("mode.3.name"), std::string("Landing"));
    CHECK_EQ(d.value("no.such.field"), std::string(""));
}

TEST(manual_values_say_whether_the_pinkie_is_a_shift_and_what_a_ready_made_profile_is) {
    Profile p;
    p.pinkieShift = true;
    p.shiftButton = 6;
    p.standard = true;
    p.basedOn = "default";
    ManualData d = buildManualData(p, "1.5.0");
    CHECK_EQ(d.value("profile.pinkie_shift"), std::string("yes"));
    CHECK_EQ(d.value("profile.shift_button"), std::string("Pinkie"));
    CHECK_EQ(d.value("profile.standard"), std::string("yes"));
    CHECK_EQ(d.value("profile.based_on"), std::string("default"));
    p.pinkieShift = false;
    p.standard = false;
    d = buildManualData(p, "1.5.0");
    CHECK_EQ(d.value("profile.pinkie_shift"), std::string("no"));
    CHECK_EQ(d.value("profile.standard"), std::string("no"));
}

TEST(manual_buttons_belong_to_the_stick_or_the_throttle) {
    CHECK_EQ(std::string(buttonDevice(1)), std::string("stick"));      // trigger
    CHECK_EQ(std::string(buttonDevice(9)), std::string("stick"));      // toggle 1 up
    CHECK_EQ(std::string(buttonDevice(20)), std::string("stick"));     // hat 2
    CHECK_EQ(std::string(buttonDevice(40)), std::string("stick"));     // main hat
    CHECK_EQ(std::string(buttonDevice(7)), std::string("throttle"));   // D
    CHECK_EQ(std::string(buttonDevice(24)), std::string("throttle"));  // throttle hat
    CHECK_EQ(std::string(buttonDevice(31)), std::string("throttle"));  // clutch
    CHECK_EQ(std::string(buttonDevice(35)), std::string("throttle"));  // MFD
    CHECK_EQ(std::string(buttonDevice(44)), std::string("throttle"));  // mouse nub
    CHECK_EQ(std::string(buttonDevice(0)), std::string(""));
    CHECK_EQ(std::string(buttonDevice(kMaxButton + 1)), std::string(""));
}

TEST(manual_lists_every_button_once_in_the_list_of_its_device) {
    ManualData d = buildManualData(Profile(), "1.5.0");
    size_t total = d.lists.at("stick_buttons").size() + d.lists.at("throttle_buttons").size();
    CHECK_EQ(total, (size_t)kMaxButton);
    std::set<std::string> numbers;
    for (const char* list : {"stick_buttons", "throttle_buttons"})
        for (const ManualRow& r : d.lists.at(list)) {
            CHECK(numbers.insert(r.at("button.number")).second);
            CHECK_EQ(std::string(buttonDevice(std::stoi(r.at("button.number")))), std::string(list) == "stick_buttons" ? "stick" : "throttle");
        }
    CHECK(rowOf(d, "stick_buttons", "Trigger") != nullptr);
    CHECK(rowOf(d, "throttle_buttons", "Throttle hat up") != nullptr);
    CHECK(rowOf(d, "stick_buttons", "Throttle hat up") == nullptr);
}

TEST(manual_cells_show_what_each_mode_and_pinkie_layer_does) {
    Profile p;
    bindAs(p, 2, 2, "Fire guns", "sim/weapons/fire_guns");       // Fire, mode 2 only
    bindAs(p, 3, kLayerAny, "Gear", "sim/flight_controls/landing_gear_toggle");   // A, every mode
    bindAs(p, 1, 5, "Trim", "sim/flight_controls/pitch_trim_up");   // Trigger, mode 2 + pinkie
    ManualData d = buildManualData(p, "1.5.0");
    const ManualRow* fire = rowOf(d, "stick_buttons", "Fire");
    CHECK(fire != nullptr);
    CHECK_EQ(fire->at("button.m1"), std::string(""));
    CHECK_EQ(fire->at("button.m2"), std::string("Fire guns"));
    CHECK_EQ(fire->at("button.m3"), std::string(""));
    CHECK_EQ(fire->at("button.used"), std::string("yes"));
    const ManualRow* a = rowOf(d, "stick_buttons", "A");
    CHECK_EQ(a->at("button.m1"), std::string("Gear"));   // a binding for any mode shows in all three
    CHECK_EQ(a->at("button.m2"), std::string("Gear"));
    CHECK_EQ(a->at("button.m3"), std::string("Gear"));
    CHECK_EQ(a->at("button.p2"), std::string("Gear"));   // and with the pinkie held, which falls back to any mode
    const ManualRow* trig = rowOf(d, "stick_buttons", "Trigger");
    CHECK_EQ(trig->at("button.m2"), std::string(""));
    CHECK_EQ(trig->at("button.p2"), std::string("Trim"));
    CHECK_EQ(trig->at("button.p1"), std::string(""));
}

TEST(manual_cell_adds_the_hold_action_and_has_a_row_number_and_name) {
    Profile p;
    Binding& b = bindAs(p, 3, kLayerAny, "Landing light", "sim/lights/landing_lights_toggle");
    b.holdLabel = "Parking brake";
    std::string err;
    actionFromText("cmd:sim/flight_controls/brakes_toggle_max", b.hold, &err);
    ManualData d = buildManualData(p, "1.5.0");
    const ManualRow* a = rowOf(d, "stick_buttons", "A");
    CHECK_EQ(a->at("button.m1"), std::string("Landing light (hold: Parking brake)"));
    CHECK_EQ(a->at("button.number"), std::string("3"));
}

TEST(manual_cell_without_a_label_shows_the_command) {
    Profile p;
    bindAs(p, 4, 1, "", "sim/autopilot/servos_toggle");
    ManualData d = buildManualData(p, "1.5.0");
    CHECK_EQ(rowOf(d, "stick_buttons", "B")->at("button.m1"), std::string("cmd:sim/autopilot/servos_toggle"));
}

TEST(manual_unused_buttons_are_listed_and_marked_unused) {
    ManualData d = buildManualData(Profile(), "1.5.0");
    const ManualRow* c = rowOf(d, "stick_buttons", "C");
    CHECK(c != nullptr);
    CHECK_EQ(c->at("button.used"), std::string("no"));
    CHECK_EQ(c->at("button.m1"), std::string(""));
}

int main() { return runAllTests(); }
