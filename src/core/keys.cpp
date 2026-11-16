#include "keys.h"

#include <cctype>
#include <cstring>
#include <sstream>

namespace x52 {

namespace {

struct KeyName {
    const char* name;
    int code;
};

// macOS virtual key codes (Carbon kVK_*), ANSI layout.
const KeyName kKeys[] = {
    {"a", 0},      {"s", 1},      {"d", 2},      {"f", 3},      {"h", 4},       {"g", 5},      {"z", 6},
    {"x", 7},      {"c", 8},      {"v", 9},      {"b", 11},     {"q", 12},      {"w", 13},     {"e", 14},
    {"r", 15},     {"y", 16},     {"t", 17},     {"1", 18},     {"2", 19},      {"3", 20},     {"4", 21},
    {"6", 22},     {"5", 23},     {"=", 24},     {"9", 25},     {"7", 26},      {"-", 27},     {"8", 28},
    {"0", 29},     {"]", 30},     {"o", 31},     {"u", 32},     {"[", 33},      {"i", 34},     {"p", 35},
    {"return", 36}, {"l", 37},    {"j", 38},     {"'", 39},     {"k", 40},      {"semicolon", 41},
    {"\\", 42},    {",", 43},     {"/", 44},     {"n", 45},     {"m", 46},      {".", 47},     {"tab", 48},
    {"space", 49}, {"`", 50},     {"backspace", 51}, {"escape", 53},
    {"kp.", 65},   {"kp*", 67},   {"kpplus", 69}, {"kp/", 75},  {"kpenter", 76}, {"kp-", 78},
    {"kp0", 82},   {"kp1", 83},   {"kp2", 84},   {"kp3", 85},   {"kp4", 86},    {"kp5", 87},   {"kp6", 88},
    {"kp7", 89},   {"kp8", 91},   {"kp9", 92},
    {"f5", 96},    {"f6", 97},    {"f7", 98},    {"f3", 99},    {"f8", 100},    {"f9", 101},   {"f11", 103},
    {"f13", 105},  {"f16", 106},  {"f14", 107},  {"f10", 109},  {"f12", 111},   {"f15", 113},
    {"home", 115}, {"pageup", 116}, {"delete", 117}, {"f4", 118}, {"end", 119}, {"f2", 120},
    {"pagedown", 121}, {"f1", 122}, {"left", 123}, {"right", 124}, {"down", 125}, {"up", 126},
    // aliases (parse only; the first name for a code is the canonical one)
    {"enter", 36}, {"esc", 53},   {"plus", 24},  {"minus", 27}, {";", 41},
};

std::string lower(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = (char)tolower((unsigned char)c);
    return r;
}

}  // namespace

const char* keyNameForCode(int keycode) {
    for (const KeyName& k : kKeys)
        if (k.code == keycode) return k.name;
    return "";
}

bool parseKey(const std::string& text, KeyCombo& out) {
    KeyCombo k;
    std::string t = lower(text);
    // Step separator is ';' and modifier separator is '+', so the key itself is the last part.
    size_t pos = 0;
    bool haveKey = false;
    while (pos <= t.size()) {
        size_t plus = t.find('+', pos);
        std::string part = t.substr(pos, plus == std::string::npos ? std::string::npos : plus - pos);
        size_t a = part.find_first_not_of(' ');
        size_t b = part.find_last_not_of(' ');
        part = a == std::string::npos ? "" : part.substr(a, b - a + 1);
        if (part == "shift") k.shift = true;
        else if (part == "ctrl" || part == "control") k.ctrl = true;
        else if (part == "alt" || part == "option" || part == "opt") k.alt = true;
        else if (part == "cmd" || part == "command") k.cmd = true;
        else {
            if (haveKey || part.empty()) return false;
            int code = -1;
            for (const KeyName& kn : kKeys)
                if (part == kn.name) {
                    code = kn.code;
                    break;
                }
            if (code < 0) return false;
            k.keycode = code;
            haveKey = true;
        }
        if (plus == std::string::npos) break;
        pos = plus + 1;
    }
    if (!haveKey) return false;
    out = k;
    return true;
}

std::string comboText(const KeyCombo& k) {
    const char* name = keyNameForCode(k.keycode);
    if (!*name) return "";
    std::string s;
    if (k.ctrl) s += "ctrl+";
    if (k.alt) s += "alt+";
    if (k.shift) s += "shift+";
    if (k.cmd) s += "cmd+";
    return s + name;
}

}  // namespace x52
