#include "sidepages.h"

#include <algorithm>
#include <cstdio>

namespace x52 {

namespace {

enum Kind { Com, Nav, Adf };

Kind kindOf(int page) { return page == PageComNav2 ? Nav : page == PageAdf ? Adf : Com; }
int partCount(Kind k) { return k == Adf ? 3 : 2; }
const char* word(Kind k) { return k == Com ? "com" : k == Nav ? "nav" : "adf"; }
const char* label(Kind k) { return k == Com ? "COM" : k == Nav ? "NAV" : "ADF"; }

std::string value(Kind k, int v) {
    char buf[32];
    if (k == Com) snprintf(buf, sizeof buf, "%3d.%03d", v / 1000, v % 1000);
    else if (k == Nav) snprintf(buf, sizeof buf, "%3d.%02d", v / 100, v % 100);
    else snprintf(buf, sizeof buf, "%4d", v);
    return buf;
}

std::string padded(std::string s, size_t width) {
    if (s.size() < width) s += std::string(width - s.size(), ' ');
    return s;
}

PageCursor clamped(int page, PageCursor c) {
    c.item = std::max(0, std::min(3, c.item));
    c.part = std::max(0, std::min(partCount(kindOf(page)) - 1, c.part));
    if (c.item % 2 == 0) c.editing = false;   // only a standby is edited
    return c;
}

}  // namespace

bool isSidePage(int page) { return page == PageComNav1 || page == PageComNav2 || page == PageAdf; }

Lines formatSidePage(int page, const SimData& d, const PageCursor& cur) {
    Kind k = kindOf(page);
    PageCursor c = clamped(page, cur);
    int act[2], stby[2];
    if (k == Com) act[0] = d.com1, act[1] = d.com2, stby[0] = d.com1s, stby[1] = d.com2s;
    else if (k == Nav) act[0] = d.nav1, act[1] = d.nav2, stby[0] = d.nav1s, stby[1] = d.nav2s;
    else act[0] = d.adf1, act[1] = d.adf2, stby[0] = d.adf1s, stby[1] = d.adf2s;
    std::string name[2] = {std::string(label(k)) + "1", std::string(label(k)) + "2"};
    Lines p;
    if (c.editing) {
        // One unit, centred. The numbers stay in the same columns whichever part the arrow is on.
        int u = c.item / 2;
        size_t at = k == Adf ? 6 : 5;   // first column of the value
        p.l[0] = std::string(6, ' ') + name[u];
        p.l[1] = std::string(at, ' ') + value(k, act[u]);
        std::string s = value(k, stby[u]);
        if (c.part == 0) p.l[2] = std::string(at - 1, ' ') + ">" + s;
        else p.l[2] = std::string(at, ' ') + s + (k != Adf ? "<" : c.part == 1 ? "<10" : "<1");
    } else {
        // Column 0: unit 1's arrow, 1-7: its value; column 8: unit 2's arrow, 9-15: its value.
        p.l[0] = (c.item == 0 ? "[" : " ") + name[0] + (c.item == 0 ? "]" : " ") + "  " + (c.item == 2 ? "[" : " ") + name[1] +
                 (c.item == 2 ? "]" : " ");
        p.l[1] = " " + padded(value(k, act[0]), 7) + " " + value(k, act[1]);
        p.l[2] = (c.item == 1 ? ">" : " ") + padded(value(k, stby[0]), 7) + (c.item == 3 ? ">" : " ") + value(k, stby[1]);
    }
    for (auto& l : p.l) l = mfdText(l);
    return p;
}

std::vector<std::string> sidePageInput(int page, PageCursor& c, Wheel w, bool com833, int repeat) {
    Kind k = kindOf(page);
    c = clamped(page, c);
    std::string n = std::to_string(c.item / 2 + 1);
    if (!c.editing) {
        if (w == Wheel::Select) {
            if (c.item % 2 == 0) return {std::string("sim/radios/") + word(k) + n + "_standy_flip"};   // X-Plane's spelling
            c.editing = true;
            c.part = 0;
            return {};
        }
        c.item = (c.item + (w == Wheel::Down ? 1 : 3)) % 4;
        return {};
    }
    if (w == Wheel::Select) {
        if (++c.part >= partCount(k)) {
            c.editing = false;
            c.part = 0;
        }
        return {};
    }
    bool up = w == Wheel::Up;
    std::string stem = std::string("sim/radios/stby_") + word(k) + n + "_";
    std::string cmd;
    bool accel = false;
    if (k == Adf) {
        static const char* const digit[] = {"hundreds_", "tens_", "ones_"};
        cmd = stem + digit[c.part] + (up ? "up" : "down");
    } else if (c.part == 0) {
        cmd = stem + "coarse_" + (up ? "up" : "down");
    } else {
        cmd = stem + "fine_" + (up ? "up" : "down") + (k == Com && com833 ? "_833" : "");
        accel = true;
    }
    return std::vector<std::string>(accel ? std::max(1, repeat) : 1, cmd);
}

}  // namespace x52
