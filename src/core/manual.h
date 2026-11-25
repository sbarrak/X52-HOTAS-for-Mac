// Manual data: everything a manual template may show about one profile, as plain text values and lists of
// rows. No Word, no files, no X-Plane: the template filler (a later step) only reads this. The field names
// and the schema number are the contract with the templates; see docs/manual-data.md.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "profile.h"

namespace x52 {

constexpr int kManualSchema = 1;   // goes up only when a field is renamed or removed; new fields do not change it

typedef std::map<std::string, std::string> ManualRow;

struct ManualData {
    std::map<std::string, std::string> values;                    // "profile.name" -> "Cessna 172"
    std::map<std::string, std::vector<ManualRow>> lists;          // "stick_buttons" -> one row per button
    // "" when the field does not exist.
    std::string value(const std::string& tag) const;
};

// The data of one profile. appVersion is the version of the program that exports (the VERSION file).
ManualData buildManualData(const Profile& p, const std::string& appVersion);

// The lists a template may repeat rows of, and the fields of their rows, are in docs/manual-data.md.
// Which device a button belongs to: "stick" or "throttle" ("" when out of range).
const char* buttonDevice(int button);

}  // namespace x52
