// Fills a Word template with the manual data of a profile. The template marks its spots with Word content
// controls ("Developer > Rich text / Plain text content control", Properties > Tag):
//   - a control tagged with a single value ("profile.name", "mode.1.name") becomes that text, in the control's own
//     font and size; the control itself is removed, so the result is an ordinary document;
//   - a control tagged "repeat:<list>" that wraps a table row (or a paragraph) is repeated once per entry of the
//     list, and the controls inside it take the tags of the entry ("button.name", "button.m1", ...). "repeat:<list>:<f>"
//     keeps only the entries whose yes/no field "<f>" (such as "used_pinkie") says yes;
//   - a control with a tag the app does not know becomes empty and is reported in unknownTags (a picture that is not in
//     the icon set is reported as "icon:<name>").
// The template may also carry a custom document property X52TemplateSchema (a whole number): the lowest manual
// data schema it needs. An older app refuses it. See docs/manual-data.md.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "manual.h"

namespace x52 {

struct FillResult {
    bool ok = false;
    std::string error;                      // why not ok
    std::string bytes;                      // the finished .docx
    std::vector<std::string> unknownTags;   // tags the template uses and the data does not have (once each)
};

// The light icons (templates/light-icons): picture name ("g", "ra_flash", ...) -> PNG bytes. A value puts one in a cell
// with \x04<name>\x05; the pictures it uses are added to the Word file.
typedef std::map<std::string, std::string> IconSet;
IconSet loadIconSet(const std::string& dir);   // every <name>.png of the folder; empty when it cannot be read

FillResult fillDocx(const std::string& templateBytes, const ManualData& data, const IconSet& icons = IconSet());

// The schema a template says it needs (X52TemplateSchema); 0 when it does not say.
int templateSchema(const std::string& templateBytes);

}  // namespace x52
