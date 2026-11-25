#include "manual.h"

namespace x52 {

namespace {
std::string yesNo(bool b) { return b ? "yes" : "no"; }

// What the manual says a button does in one layer: its label, and what a hold does when it has one.
std::string cellText(const Profile& p, int button, int layer) {
    const Binding* b = p.find(button, layer);
    if (!b || b->empty()) return "";
    std::string text = !b->label.empty() ? b->label : actionToText(b->press.empty() ? b->hold : b->press);
    if (!b->hold.empty() && !b->press.empty()) {
        std::string hold = !b->holdLabel.empty() ? b->holdLabel : actionToText(b->hold);
        text += " (hold: " + hold + ")";
    }
    return text;
}
}  // namespace

std::string ManualData::value(const std::string& tag) const {
    auto it = values.find(tag);
    return it == values.end() ? std::string() : it->second;
}

const char* buttonDevice(int button) {
    if (button < 1 || button > kMaxButton) return "";
    // Stick: trigger, fire, A, B, C, pinkie; the three toggles and the trigger's second stage; hat 2; the main hat.
    if (button <= 6 || (button >= 9 && button <= 15) || (button >= 20 && button <= 23) ||
        (button >= kPov1Up && button < kPov1Up + 4))
        return "stick";
    return "throttle";
}

ManualData buildManualData(const Profile& p, const std::string& appVersion) {
    ManualData d;
    d.values["manual.schema"] = std::to_string(kManualSchema);
    d.values["app.version"] = appVersion;
    d.values["profile.name"] = p.name;
    d.values["profile.based_on"] = p.basedOn;
    d.values["profile.standard"] = yesNo(p.standard);
    d.values["profile.pinkie_shift"] = yesNo(p.pinkieShift);
    d.values["profile.shift_button"] = buttonName(p.shiftButton);
    for (int m = 1; m <= 3; ++m) d.values["mode." + std::to_string(m) + ".name"] = p.modeNames[m - 1];

    for (int button = 1; button <= kMaxButton; ++button) {
        ManualRow row;
        row["button.number"] = std::to_string(button);
        row["button.name"] = buttonName(button);
        bool usedPlain = false, usedPinkie = false;
        for (int m = 1; m <= 3; ++m) {
            std::string plain = cellText(p, button, m), pinkie = cellText(p, button, m + 3);
            row["button.m" + std::to_string(m)] = plain;
            row["button.p" + std::to_string(m)] = pinkie;
            usedPlain = usedPlain || !plain.empty();
            usedPinkie = usedPinkie || !pinkie.empty();
        }
        row["button.used"] = yesNo(usedPlain || usedPinkie);
        row["button.used_plain"] = yesNo(usedPlain);
        row["button.used_pinkie"] = yesNo(usedPinkie);
        d.lists[std::string(buttonDevice(button)) + "_buttons"].push_back(row);
    }
    return d;
}

}  // namespace x52
