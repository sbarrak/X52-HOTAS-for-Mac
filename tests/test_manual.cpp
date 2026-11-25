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

int main() { return runAllTests(); }
