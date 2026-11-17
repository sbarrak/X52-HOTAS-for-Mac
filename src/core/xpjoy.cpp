#include "xpjoy.h"

#include <sstream>

namespace x52 {

std::map<int, std::string> parseButtonAssignments(const std::string& text) {
    std::map<int, std::string> out;
    std::stringstream ss(text);
    std::string line;
    const std::string key = "_joy_BUTN_use";
    while (std::getline(ss, line)) {
        if (line.compare(0, key.size(), key) != 0) continue;
        size_t sp = line.find(' ', key.size());
        if (sp == std::string::npos) continue;
        char* end = nullptr;
        std::string num = line.substr(key.size(), sp - key.size());
        long slot = strtol(num.c_str(), &end, 10);
        if (num.empty() || *end || slot < 0 || slot >= kXPlaneButtonSlots) continue;
        std::string name = line.substr(sp + 1);
        while (!name.empty() && (name.back() == '\r' || name.back() == ' ')) name.pop_back();
        out[(int)slot] = name;
    }
    return out;
}

int xplaneButtonNumber(int slot) {
    int inDevice = slot % 160 - 8;
    return slot < 0 || inDevice < 0 ? -1 : inDevice;
}

std::string controlProfileKey(const std::string& d) {
    std::string out;
    for (char c : d) {
        if (c == ' ') out += "___";
        else if (c != '\r' && c != '\n') out += c;
    }
    return out;
}

std::string controlProfileFor(const std::string& misc, const std::string& description) {
    std::string key = controlProfileKey(description);
    if (key.empty()) return "";
    std::stringstream ss(misc);
    std::string line;
    const std::string head = "P acf_profile " + key + " ";
    while (std::getline(ss, line)) {
        if (line.compare(0, head.size(), head) != 0) continue;
        std::string name = line.substr(head.size());
        while (!name.empty() && (name.back() == '\r' || name.back() == ' ')) name.pop_back();
        return name;
    }
    return "";
}

std::map<int, int> parseButtonMap(const std::string& text) {
    std::map<int, int> out;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        std::stringstream ls(line);
        int b = 0, s = 0;
        if (ls >> b >> s && b >= 1 && b <= 39 && s >= 0 && s < kXPlaneButtonSlots) out[b] = s;
    }
    return out;
}

std::string serializeButtonMap(const std::map<int, int>& m) {
    std::ostringstream o;
    o << "# Which X-Plane joystick slot each stick button is, learned by the plugin from presses: <button> <slot>\n";
    for (const auto& kv : m) o << kv.first << " " << kv.second << "\n";
    return o.str();
}

std::map<int, int> completeButtonMap(const std::map<int, int>& learned) {
    std::map<int, int> votes;   // difference slot - button -> how many learned buttons say so
    for (const auto& kv : learned) ++votes[kv.second - kv.first];
    int offset = 7, best = 0;
    for (const auto& v : votes)
        if (v.second > best) {
            best = v.second;
            offset = v.first;
        }
    std::map<int, int> out;
    for (int b = 1; b <= 39; ++b) {
        auto it = learned.find(b);
        out[b] = it != learned.end() ? it->second : b + offset;
    }
    return out;
}

void ButtonLearner::pressed(int button, double now) {
    button_ = button;
    until_ = now + 0.6;
}

bool ButtonLearner::update(const std::set<int>& active, double now) {
    if (!button_) return false;
    if (now >= until_) {
        button_ = 0;
        return false;
    }
    std::set<int> candidates;
    for (int s : active) {
        bool otherButtons = false;
        for (const auto& kv : map_)
            if (kv.second == s && kv.first != button_) otherButtons = true;
        if (!otherButtons) candidates.insert(s);
    }
    if (candidates.size() != 1) return false;   // nothing yet, or more than one: wait
    int slot = *candidates.begin();
    auto it = map_.find(button_);
    bool changed = it == map_.end() || it->second != slot;
    map_[button_] = slot;
    button_ = 0;
    return changed;
}

}  // namespace x52
