// Fills a Word template with the manual data of a profile. The template marks its spots with Word content
// controls ("Developer > Rich text / Plain text content control", Properties > Tag):
//   - a control tagged with a single value ("profile.name", "mode.1.name") becomes that text, in the control's own
//     font and size; the control itself is removed, so the result is an ordinary document;
//   - a control tagged "repeat:<list>" that wraps a table row (or a paragraph) is repeated once per entry of the
//     list, and the controls inside it take the tags of the entry ("button.name", "button.m1", ...);
//   - a control with a tag the app does not know becomes empty and is reported in unknownTags.
// The template may also carry a custom document property X52TemplateSchema (a whole number): the lowest manual
// data schema it needs. An older app refuses it. See docs/manual-data.md.
#pragma once
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

FillResult fillDocx(const std::string& templateBytes, const ManualData& data);

// The schema a template says it needs (X52TemplateSchema); 0 when it does not say.
int templateSchema(const std::string& templateBytes);

}  // namespace x52
