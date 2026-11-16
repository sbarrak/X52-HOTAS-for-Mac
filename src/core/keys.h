// Keyboard combos such as "shift+f1" <-> macOS virtual key codes. Pure C++.
#pragma once
#include <string>

namespace x52 {

struct KeyCombo {
    int keycode = -1;   // macOS virtual key code (kVK_*)
    bool shift = false, ctrl = false, alt = false, cmd = false;
};

// Accepts e.g. "g", "shift+f1", "ctrl+alt+up", "kp5". Case-insensitive.
bool parseKey(const std::string& text, KeyCombo& out);
// Canonical text for a combo ("ctrl+alt+shift+cmd+name"); "" if the key code is unknown.
std::string comboText(const KeyCombo& k);
// Name for a macOS key code, or "" if unknown.
const char* keyNameForCode(int keycode);

}  // namespace x52
