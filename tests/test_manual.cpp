// Tests of the manual data (src/core/manual.cpp): what a template may show about a profile.
#include <fstream>
#include <set>
#include <sstream>

#include "../src/core/docxfill.h"
#include "../src/core/manual.h"
#include "../src/core/zipfile.h"
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

// ---------------------------------------------------------------- zip and the template filler
namespace {
const char* kNs = "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"";

std::string docWith(const std::string& body) { return std::string("<w:document ") + kNs + "><w:body>" + body + "</w:body></w:document>"; }

// An inline control: <w:sdt> inside a paragraph, tagged, with one formatted run (bold) as its placeholder.
std::string inlineControl(const std::string& tag) {
    return "<w:sdt><w:sdtPr><w:tag w:val=\"" + tag + "\"/></w:sdtPr><w:sdtContent><w:r><w:rPr><w:b/></w:rPr><w:t>placeholder</w:t></w:r></w:sdtContent></w:sdt>";
}

std::string mkDocx(const std::string& documentXml, int schema = 0, const std::string& header = "") {
    std::vector<ZipEntry> e;
    e.push_back({"[Content_Types].xml", "<Types/>"});
    e.push_back({"word/document.xml", documentXml});
    if (!header.empty()) e.push_back({"word/header1.xml", header});
    e.push_back({"word/styles.xml", "<w:styles/>"});
    if (schema)
        e.push_back({"docProps/custom.xml", "<Properties><property fmtid=\"x\" pid=\"2\" name=\"X52TemplateSchema\"><vt:i4>" + std::to_string(schema) + "</vt:i4></property></Properties>"});
    return writeZip(e);
}

std::string partOf(const std::string& docx, const std::string& name) {
    std::vector<ZipEntry> e;
    std::string err;
    if (!readZip(docx, e, &err)) return "<unreadable: " + err + ">";
    for (const ZipEntry& x : e)
        if (x.name == name) return x.data;
    return "<missing>";
}

size_t countOf(const std::string& s, const std::string& what) {
    size_t n = 0;
    for (size_t p = s.find(what); p != std::string::npos; p = s.find(what, p + what.size())) ++n;
    return n;
}

ManualData sample() {
    Profile p;
    p.name = "Fish & Chips <1>";
    return buildManualData(p, "1.5.0");
}
}  // namespace

TEST(zip_writes_and_reads_back_text_binary_and_empty_files) {
    std::string binary;
    for (int i = 0; i < 3000; ++i) binary += (char)((i * 7919 + i / 3) & 0xff);
    std::vector<ZipEntry> in = {{"a.txt", std::string(5000, 'x')}, {"dir/b.bin", binary}, {"empty", ""}};
    std::vector<ZipEntry> out;
    std::string err;
    CHECK(readZip(writeZip(in), out, &err));
    CHECK_EQ(out.size(), (size_t)3);
    for (size_t i = 0; i < out.size() && i < in.size(); ++i) {
        CHECK_EQ(out[i].name, in[i].name);
        CHECK(out[i].data == in[i].data);
    }
}

TEST(zip_rejects_what_is_not_a_zip) {
    std::vector<ZipEntry> out;
    std::string err;
    CHECK(!readZip("hello, this is not a zip file at all", out, &err));
    CHECK(!err.empty());
    std::string good = writeZip({{"a", std::string(100, 'a')}});
    good[good.size() / 2] ^= 0x55;   // damage the middle
    CHECK(!readZip(good, out, &err));
}

TEST(zip_reads_the_real_manual_file) {
    std::ifstream f("docs/manual/X52-Pro-Manual.docx", std::ios::binary);
    if (!f) return;   // not in this checkout
    std::stringstream ss;
    ss << f.rdbuf();
    std::vector<ZipEntry> out;
    std::string err;
    CHECK(readZip(ss.str(), out, &err));
    bool found = false;
    for (const ZipEntry& e : out) found = found || (e.name == "word/document.xml" && e.data.find("<w:document") != std::string::npos);
    CHECK(found);
}

TEST(fill_replaces_an_inline_control_with_text_in_its_own_format) {
    std::string doc = docWith("<w:p><w:r><w:t>Profile: </w:t></w:r>" + inlineControl("profile.name") + "</w:p>");
    FillResult r = fillDocx(mkDocx(doc), sample());
    CHECK(r.ok);
    std::string xml = partOf(r.bytes, "word/document.xml");
    CHECK(xml.find("Fish &amp; Chips &lt;1&gt;") != std::string::npos);   // escaped
    CHECK(xml.find("<w:b/>") != std::string::npos);                          // the control's bold kept
    CHECK(xml.find("<w:sdt") == std::string::npos);                          // the control is gone
    CHECK(xml.find("placeholder") == std::string::npos);
    CHECK(xml.find("Profile: ") != std::string::npos);
    CHECK(r.unknownTags.empty());
}

TEST(fill_keeps_the_paragraph_format_of_a_control_that_holds_a_paragraph) {
    std::string doc = docWith(
        "<w:sdt><w:sdtPr><w:tag w:val=\"mode.2.name\"/></w:sdtPr><w:sdtContent><w:p><w:pPr><w:jc w:val=\"center\"/></w:pPr>"
        "<w:r><w:rPr><w:sz w:val=\"40\"/></w:rPr><w:t>x</w:t></w:r></w:p></w:sdtContent></w:sdt>");
    Profile p;
    p.modeNames[1] = "Cruise";
    FillResult r = fillDocx(mkDocx(doc), buildManualData(p, "1.5.0"));
    CHECK(r.ok);
    std::string xml = partOf(r.bytes, "word/document.xml");
    CHECK(xml.find("<w:p><w:pPr><w:jc w:val=\"center\"/></w:pPr><w:r><w:rPr><w:sz w:val=\"40\"/></w:rPr><w:t xml:space=\"preserve\">Cruise</w:t></w:r></w:p>") != std::string::npos);
    CHECK(xml.find("<w:sdt") == std::string::npos);
}

TEST(fill_repeats_a_table_row_for_every_entry_of_the_list) {
    std::string row = "<w:tr><w:tc><w:p>" + inlineControl("button.number") + "</w:p></w:tc><w:tc><w:p>" + inlineControl("button.name") + "</w:p></w:tc>"
                      "<w:tc><w:p>" + inlineControl("button.m1") + "</w:p></w:tc></w:tr>";
    std::string doc = docWith("<w:tbl><w:tr><w:tc><w:p><w:r><w:t>header</w:t></w:r></w:p></w:tc></w:tr>"
                              "<w:sdt><w:sdtPr><w:tag w:val=\"repeat:stick_buttons\"/></w:sdtPr><w:sdtContent>" + row + "</w:sdtContent></w:sdt></w:tbl>");
    Profile p;
    bindAs(p, 1, 1, "Fire guns", "sim/weapons/fire_guns");
    ManualData d = buildManualData(p, "1.5.0");
    FillResult r = fillDocx(mkDocx(doc), d);
    CHECK(r.ok);
    std::string xml = partOf(r.bytes, "word/document.xml");
    CHECK_EQ(countOf(xml, "<w:tr>"), d.lists.at("stick_buttons").size() + 1);   // plus the header row
    CHECK(xml.find("<w:sdt") == std::string::npos);
    CHECK(xml.find(">Trigger</w:t>") != std::string::npos);
    CHECK(xml.find(">Fire guns</w:t>") != std::string::npos);
    CHECK(xml.find("header") != std::string::npos);
}

TEST(fill_turns_new_lines_into_line_breaks) {
    ManualData d;
    d.values["note"] = "one\ntwo";
    FillResult r = fillDocx(mkDocx(docWith("<w:p>" + inlineControl("note") + "</w:p>")), d);
    CHECK(r.ok);
    CHECK(partOf(r.bytes, "word/document.xml").find("one</w:t><w:br/><w:t xml:space=\"preserve\">two") != std::string::npos);
}

TEST(fill_empties_an_unknown_tag_and_says_which) {
    FillResult r = fillDocx(mkDocx(docWith("<w:p>" + inlineControl("not.a.field") + inlineControl("not.a.field") + "</w:p>")), sample());
    CHECK(r.ok);
    CHECK_EQ(r.unknownTags.size(), (size_t)1);
    CHECK_EQ(r.unknownTags.empty() ? std::string() : r.unknownTags[0], std::string("not.a.field"));
    std::string xml = partOf(r.bytes, "word/document.xml");
    CHECK(xml.find("placeholder") == std::string::npos);
    CHECK(xml.find("<w:sdt") == std::string::npos);
}

TEST(fill_leaves_other_content_controls_alone) {
    std::string other = "<w:sdt><w:sdtPr><w:alias w:val=\"mine\"/></w:sdtPr><w:sdtContent><w:r><w:t>keep me</w:t></w:r></w:sdtContent></w:sdt>";
    FillResult r = fillDocx(mkDocx(docWith("<w:p>" + other + "</w:p>")), sample());
    CHECK(r.ok);
    CHECK(partOf(r.bytes, "word/document.xml").find(other) != std::string::npos);
}

TEST(fill_also_fills_headers_and_keeps_every_other_part) {
    std::string header = std::string("<w:hdr ") + kNs + "><w:p>" + inlineControl("profile.name") + "</w:p></w:hdr>";
    std::string in = mkDocx(docWith("<w:p/>"), 0, header);
    FillResult r = fillDocx(in, sample());
    CHECK(r.ok);
    CHECK(partOf(r.bytes, "word/header1.xml").find("Fish &amp; Chips") != std::string::npos);
    CHECK_EQ(partOf(r.bytes, "word/styles.xml"), std::string("<w:styles/>"));
    std::vector<ZipEntry> e;
    std::string err;
    CHECK(readZip(r.bytes, e, &err));
    CHECK_EQ(e.empty() ? std::string() : e[0].name, std::string("[Content_Types].xml"));   // Word wants it first
}

TEST(fill_refuses_a_template_that_needs_a_newer_schema) {
    CHECK_EQ(templateSchema(mkDocx(docWith("<w:p/>"), 7)), 7);
    CHECK_EQ(templateSchema(mkDocx(docWith("<w:p/>"))), 0);
    FillResult r = fillDocx(mkDocx(docWith("<w:p/>"), kManualSchema + 1), sample());
    CHECK(!r.ok);
    CHECK(r.error.find("newer version") != std::string::npos);
    CHECK(fillDocx(mkDocx(docWith("<w:p/>"), kManualSchema), sample()).ok);
}

TEST(fill_refuses_a_file_that_is_not_a_word_document) {
    FillResult r = fillDocx("plain text", sample());
    CHECK(!r.ok);
    CHECK(!r.error.empty());
    FillResult s = fillDocx(writeZip({{"readme.txt", "hi"}}), sample());
    CHECK(!s.ok);
}

TEST(manual_rows_say_whether_the_plain_or_the_pinkie_layers_use_the_button) {
    Profile p;
    bindAs(p, 2, 1, "Fire guns", "sim/weapons/fire_guns");       // Fire: plain only
    bindAs(p, 1, 5, "Trim", "sim/flight_controls/pitch_trim_up");   // Trigger: pinkie only
    ManualData d = buildManualData(p, "1.5.0");
    CHECK_EQ(rowOf(d, "stick_buttons", "Fire")->at("button.used_plain"), std::string("yes"));
    CHECK_EQ(rowOf(d, "stick_buttons", "Fire")->at("button.used_pinkie"), std::string("no"));
    CHECK_EQ(rowOf(d, "stick_buttons", "Trigger")->at("button.used_plain"), std::string("no"));
    CHECK_EQ(rowOf(d, "stick_buttons", "Trigger")->at("button.used_pinkie"), std::string("yes"));
    CHECK_EQ(rowOf(d, "stick_buttons", "C")->at("button.used"), std::string("no"));
}

TEST(fill_repeat_with_a_filter_keeps_only_the_rows_that_say_yes) {
    auto rows = [](const std::string& filter) {
        std::string row = "<w:tr><w:tc><w:p>" + inlineControl("button.name") + "</w:p></w:tc></w:tr>";
        return docWith("<w:tbl><w:sdt><w:sdtPr><w:tag w:val=\"repeat:stick_buttons" + filter + "\"/></w:sdtPr><w:sdtContent>" + row + "</w:sdtContent></w:sdt></w:tbl>");
    };
    Profile p;
    bindAs(p, 2, 1, "Fire guns", "sim/weapons/fire_guns");
    bindAs(p, 1, 5, "Trim", "sim/flight_controls/pitch_trim_up");
    ManualData d = buildManualData(p, "1.5.0");
    std::string all = partOf(fillDocx(mkDocx(rows("")), d).bytes, "word/document.xml");
    std::string plain = partOf(fillDocx(mkDocx(rows(":used_plain")), d).bytes, "word/document.xml");
    std::string pinkie = partOf(fillDocx(mkDocx(rows(":used_pinkie")), d).bytes, "word/document.xml");
    CHECK_EQ(countOf(all, "<w:tr>"), d.lists.at("stick_buttons").size());
    CHECK_EQ(countOf(plain, "<w:tr>"), (size_t)1);
    CHECK(plain.find(">Fire</w:t>") != std::string::npos);
    CHECK_EQ(countOf(pinkie, "<w:tr>"), (size_t)1);
    CHECK(pinkie.find(">Trigger</w:t>") != std::string::npos);
}

// ---------------------------------------------------------------- the shipped template and the fixed assignment tables
namespace {
std::string slurpFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
}  // namespace

TEST(the_shipped_template_only_uses_tags_the_manual_data_has) {
    std::string tmpl = slurpFile("templates/X52-Manual-Template.docx");
    CHECK(tmpl.size() > 1000);
    CHECK_EQ(templateSchema(tmpl), kManualSchema);
    for (const char* profile : {"", "profiles/aircraft-F-14D.x52"}) {
        Profile p;
        if (*profile) {
            std::string err;
            CHECK(parse(slurpFile(profile), p, &err));
        }
        FillResult r = fillDocx(tmpl, buildManualData(p, "1.5.0", "some-profile"));
        CHECK(r.ok);
        std::string unknown;
        for (const std::string& u : r.unknownTags) unknown += u + " ";
        CHECK_EQ(unknown, std::string(""));
        std::vector<ZipEntry> parts;
        std::string err;
        CHECK(readZip(r.bytes, parts, &err));
        for (const ZipEntry& e : parts)
            if (e.name == "word/document.xml" || e.name == "word/header1.xml") CHECK(e.data.find("<w:sdt") == std::string::npos);
        std::string header = partOf(r.bytes, "word/header1.xml");
        CHECK(header.find("some-profile.x52") != std::string::npos);
        std::string doc = partOf(r.bytes, "word/document.xml");
        CHECK(doc.find("A tap") != std::string::npos);                          // the fixed rows are still there
        CHECK(doc.find("Roll / pitch / twist") != std::string::npos);
        CHECK(doc.find("Display and lights dashboard") != std::string::npos);
    }
}

TEST(manual_fixed_tables_show_taps_holds_hats_and_the_fixed_functions) {
    Profile p;
    Binding& a = bindAs(p, 3, kLayerAny, "Gear", "sim/flight_controls/landing_gear_toggle");
    a.holdLabel = "Parking brake";
    std::string err;
    actionFromText("cmd:sim/flight_controls/brakes_toggle_max", a.hold, &err);
    for (int i = 0; i < 4; ++i) bindAs(p, kPov1Up + i, kLayerAny, std::string("Look ") + (i == 0 ? "up" : i == 1 ? "right" : i == 2 ? "down" : "left"), "sim/view/x");
    for (int i = 0; i < 8; ++i) bindAs(p, kNubUp + i, kLayerAny, i < 4 ? "Pan up" : "Pan up fast", "sim/general/up");
    bindAs(p, 1, 2, "Gun", "sim/weapons/fire_guns");
    bindAs(p, 20, 5, "Trim", "sim/flight_controls/pitch_trim_up");
    p.axes.overrideXPlane = true;
    p.axes.pluginThrottle = true;
    p.axes.sliderFlaps = true;
    p.axes.upperUse = DialUse::Zoom;
    p.axes.lowerUse = DialUse::None;
    ManualData d = buildManualData(p, "1.5.0");
    CHECK_EQ(d.value("stick.a_tap.m1"), std::string("Gear"));              // a tap is the press only
    CHECK_EQ(d.value("stick.a_hold.m1"), std::string("Parking brake"));    // a hold is the hold only
    CHECK_EQ(d.value("stick.tr1.m2"), std::string("Gun"));
    CHECK_EQ(d.value("stick.tr1.m1"), std::string("\xE2\x80\x94"));        // nothing bound: an em dash
    CHECK_EQ(d.value("stick.h1_main_hat.m1"), std::string("Look in direction"));
    CHECK_EQ(d.value("throttle.ms_mouse_nub.m3"), std::string("Pan / far: fast pan"));
    CHECK_EQ(d.value("pinkie_stick.h2_up.m2"), std::string("Trim"));
    CHECK_EQ(d.value("pinkie_stick.h2_up.m1"), std::string("\xE2\x80\x94"));
    CHECK_EQ(d.value("stick.roll_pitch_twist.m1"), std::string("Plugin axes"));
    CHECK_EQ(d.value("throttle.ax_lever.m1"), std::string("Engine power"));
    CHECK_EQ(d.value("throttle.sl_slider.m2"), std::string("Flap detents"));
    CHECK_EQ(d.value("throttle.e_upper_ring.m1"), std::string("View zoom"));
    CHECK_EQ(d.value("throttle.i_lower_ring.m1"), std::string("X-Plane joystick setup"));
    CHECK_EQ(d.value("throttle.l_turn.m1"), std::string("Change page"));
    CHECK_EQ(d.value("throttle.r_turn_press.m3"), std::string("Cursor / select"));
    CHECK_EQ(d.value("throttle.i_press.m1"), std::string("Safe mode / picker"));
    CHECK_EQ(d.value("pinkie_throttle.l_turn.m1"), std::string("\xE2\x80\x94"));
}

TEST(manual_dashboard_values_pages_lights_rules_and_alarms) {
    Profile p;
    p.leds[0][LedA] = Color::Red;
    p.ledsPinkie[0][LedA] = Color::Amber;
    LightRule r;
    r.led = LedB;
    r.dataref = "sim/x/gear";
    r.op = Compare::Greater;
    r.value = 0.5;
    r.thenColor = (int)Color::Amber;
    r.elseColor = kRuleMode;
    p.rules.push_back(r);
    Alarm a;
    a.name = "Stall";
    a.led = LedPov;
    a.dataref = "sim/x/stall";
    a.effect = Effect::Blink;
    a.fast = true;
    p.alarms.push_back(a);
    ManualData d = buildManualData(p, "1.5.0");
    CHECK_EQ(d.value("light.a.m1.name"), std::string("red (Pinkie: amber)"));
    CHECK_EQ(d.value("light.a.m2.name"), std::string("green"));
    CHECK_EQ(d.value("light.pov.m1.name"), std::string("green"));
    CHECK(d.value("light.a.m2").find("\xE2\x97\x8F") != std::string::npos);            // a full dot
    CHECK(d.value("light.a.m2").find("2E9E57") != std::string::npos);                    // in green
    CHECK(d.value("light.a.m1").find("Pinkie") != std::string::npos);                    // and the Pinkie colour when it differs
    CHECK_EQ(stripStyle(d.value("light.a.m2")), std::string("\xE2\x97\x8F"));
    CHECK_EQ(d.lists.at("rules").size(), (size_t)3);                       // a table never has fewer than 3 rows
    CHECK_EQ(d.lists.at("rules")[0].at("rule.light"), std::string("B"));
    CHECK_EQ(d.lists.at("rules")[0].at("rule.condition"), std::string("sim/x/gear > 0.5"));
    CHECK_EQ(d.lists.at("rules")[0].at("rule.active_name"), std::string("amber"));
    CHECK_EQ(d.lists.at("rules")[0].at("rule.otherwise_name"), std::string("mode light"));
    CHECK(d.lists.at("rules")[0].at("rule.active").find("F0B429") != std::string::npos);   // an amber dot
    CHECK_EQ(d.value("alarm.1.name"), std::string("Stall"));
    CHECK_EQ(d.value("alarm.1.light"), std::string("H2 lit hat"));
    CHECK_EQ(d.value("alarm.1.rate"), std::string("Fast"));
    CHECK_EQ(stripStyle(d.value("alarm.1.m1")), std::string("\xE2\x9C\xBA"));                // flashing: rays
    CHECK_EQ(d.value("alarm.2.name"), std::string(""));                    // 8 alarm rows, always
    CHECK(!d.value("pages.1.m1").empty());                                  // the built-in pages come first
    CHECK_EQ(d.lists.at("user_pages").size(), (size_t)3);
}

TEST(manual_alarm_dots_show_steady_flashing_and_alternating_and_use_the_mode_light) {
    Profile p;
    p.leds[1][LedA] = Color::Amber;                    // mode 2 lights A amber
    Alarm steady, alt, modeLight;
    steady.name = "S"; steady.effect = Effect::Steady; steady.color = (int)Color::Red; steady.dataref = "x";
    alt.name = "A"; alt.effect = Effect::Alternate; alt.color = (int)Color::Red; alt.color2 = (int)Color::Amber; alt.fast = true; alt.dataref = "x";
    modeLight.name = "M"; modeLight.effect = Effect::Steady; modeLight.color = kRuleMode; modeLight.led = LedA; modeLight.dataref = "x";
    p.alarms = {steady, alt, modeLight};
    ManualData d = buildManualData(p, "1.5.0");
    CHECK_EQ(stripStyle(d.value("alarm.1.m1")), std::string("\xE2\x97\x8F"));                         // steady: a full dot
    CHECK_EQ(d.value("alarm.1.rate"), std::string("Steady"));
    CHECK_EQ(stripStyle(d.value("alarm.2.m1")), std::string("\xE2\x97\x96\xE2\x97\x97"));            // alternating: two half dots
    CHECK(d.value("alarm.2.m1").find("D64545") != std::string::npos);
    CHECK(d.value("alarm.2.m1").find("F0B429") != std::string::npos);
    CHECK_EQ(d.value("alarm.2.rate"), std::string("Fast"));
    CHECK(d.value("alarm.3.m2").find("F0B429") != std::string::npos);                                // the mode's own colour in mode 2
    CHECK(d.value("alarm.3.m1").find("2E9E57") != std::string::npos);                                // green in mode 1
}

TEST(fill_colours_the_styled_parts_of_a_value_and_keeps_the_rest) {
    ManualData d;
    d.values["dot"] = "\x01" "D64545,22\x02" "X\x03" " after";
    FillResult r = fillDocx(mkDocx(docWith("<w:p>" + inlineControl("dot") + "</w:p>")), d);
    CHECK(r.ok);
    std::string xml = partOf(r.bytes, "word/document.xml");
    CHECK(xml.find("<w:color w:val=\"D64545\"/>") != std::string::npos);
    CHECK(xml.find("<w:sz w:val=\"22\"/>") != std::string::npos);
    CHECK(xml.find(">X</w:t>") != std::string::npos);
    CHECK(xml.find("> after</w:t>") != std::string::npos);
    CHECK(xml.find("\x01") == std::string::npos);
    CHECK(xml.find("<w:b/>") != std::string::npos);                       // the control's own formatting stays
}

int main() { return runAllTests(); }
