#include "pages.h"
#include "sidepages.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace x52 {

namespace {

std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char* f, ...) {
    char buf[64];
    va_list ap;
    va_start(ap, f);
    vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

int wrap360(float deg) {
    int v = (int)lroundf(deg) % 360;
    if (v < 0) v += 360;
    return v;
}

std::string comFreq(int khz) { return fmt("%3d.%03d", khz / 1000, khz % 1000); }
std::string navFreq(int tenKhz) { return fmt("%3d.%02d", tenKhz / 100, tenKhz % 100); }

std::string dmeLine(int n, bool has, float nm, float kt) {
    if (!has) return fmt("DME%d --.-nm ---k", n);
    return fmt("DME%d%5.1fnm%4dk", n, std::min(nm, 999.9f), std::min((int)lroundf(kt), 999));
}

// What one cursor position does. A swap item acts on Select; the others are edited.
struct Item {
    bool swap = false;
    std::string select;   // swap only
    std::string up, down;
    bool accel = false;   // a fast turn repeats the change
};

Item swapItem(const std::string& cmd) {
    Item i;
    i.swap = true;
    i.select = cmd;
    return i;
}
Item editItem(const std::string& stem, const char* downWord = "down", bool accel = false) {
    Item i;
    i.up = stem + "up";
    i.down = stem + downWord;
    i.accel = accel;
    return i;
}

Item itemFor(int page, int item, bool com833) {
    switch (page) {
        case PageComNav1:
        case PageComNav2: {
            std::string n = page == PageComNav1 ? "1" : "2";
            const char* tail = com833 ? "_833" : "";
            Item fine;
            fine.up = "sim/radios/stby_com" + n + "_fine_up" + tail;
            fine.down = "sim/radios/stby_com" + n + "_fine_down" + tail;
            fine.accel = true;
            switch (item) {
                case 0: return swapItem("sim/radios/com" + n + "_standy_flip");   // X-Plane's own spelling
                case 1: return editItem("sim/radios/stby_com" + n + "_coarse_");
                case 2: return fine;
                case 3: return swapItem("sim/radios/nav" + n + "_standy_flip");
                case 4: return editItem("sim/radios/stby_nav" + n + "_coarse_");
                default: return editItem("sim/radios/stby_nav" + n + "_fine_", "down", true);
            }
        }
        case PageAdf: {
            std::string n = item < 4 ? "1" : "2";
            switch (item % 4) {
                case 0: return swapItem("sim/radios/adf" + n + "_standy_flip");
                case 1: return editItem("sim/radios/stby_adf" + n + "_hundreds_");
                case 2: return editItem("sim/radios/stby_adf" + n + "_tens_");
                default: return editItem("sim/radios/stby_adf" + n + "_ones_");
            }
        }
        case PageXpdr: {
            static const char* const digit[] = {"thousands", "hundreds", "tens", "ones"};
            if (item < 4) return editItem(std::string("sim/transponder/transponder_") + digit[item] + "_");
            return editItem("sim/transponder/transponder_", "dn");
        }
        case PageAutopilot: {
            static const char* const what[] = {"heading", "altitude", "vertical_speed", "airspeed"};
            return editItem(std::string("sim/autopilot/") + what[std::min(item, 3)] + "_", "down", true);
        }
        case PageVorHdg:
            if (item == 0) return editItem("sim/radios/obs1_", "down", true);
            if (item == 1) return editItem("sim/radios/obs2_", "down", true);
            return editItem("sim/autopilot/heading_", "down", true);
    }
    return Item();
}

// "STBY >121.500"  "STBY>>121.500"  "STBY  121.500<"  "STBY  121.500<<"
std::string standbyLine(const std::string& value, bool coarse, bool fine, bool editing, const char* fineTag = "") {
    std::string s = "STBY";
    s += coarse ? (editing ? ">>" : " >") : "  ";
    s += value;
    if (fine) s += std::string(editing ? "<<" : "<") + fineTag;
    return s;
}

std::string activeLine(const std::string& name, const std::string& value, bool selected) {
    return selected ? name + " [" + value + "]" : name + "  " + value;
}

std::string marked(const std::string& text, bool selected, bool editing) {
    return selected ? (editing ? ">>" : ">") + text : text;
}

}  // namespace

int pageItemCount(int page, bool side) {
    if (side && isSidePage(page)) return 4;
    switch (page) {
        case PageComNav1:
        case PageComNav2: return 6;
        case PageAdf: return 8;
        case PageXpdr: return 5;
        case PageAutopilot: return 4;
        case PageVorHdg: return 3;
        default: return 0;
    }
}

namespace {
int percentOf(int v) { return (v * 100 + 63) / 127; }
int valueOf(int pct) { return std::max(0, std::min(127, (pct * 127 + 50) / 100)); }
std::string brightnessLine(const char* label, int v, bool linked, bool onCursor, bool editing) {
    std::string right = linked ? "dataref" : std::string(onCursor ? (editing ? ">>" : ">") : "") + std::to_string(percentOf(v)) + "%";
    std::string l = label;
    return l + std::string(l.size() + right.size() < (size_t)kMfdCols ? kMfdCols - l.size() - right.size() : 1, ' ') + right;
}
}  // namespace

Lines formatBrightnessPage(int screen, int lights, bool screenLinked, bool lightsLinked, const PageCursor& c) {
    Lines out;
    out.l[0] = "BRIGHTNESS";
    out.l[1] = brightnessLine("Screen", screen, screenLinked, c.item == 0, c.editing);
    out.l[2] = brightnessLine("Lights", lights, lightsLinked, c.item == 1, c.editing);
    return out;
}

bool brightnessInput(PageCursor& c, Wheel w, int& screen, int& lights, bool screenLinked, bool lightsLinked) {
    c.item = std::max(0, std::min(1, c.item));
    bool linked = c.item == 0 ? screenLinked : lightsLinked;
    if (w == Wheel::Select) {
        c.editing = !c.editing && !linked;
        return false;
    }
    if (!c.editing) {
        c.item = std::max(0, std::min(1, c.item + (w == Wheel::Down ? 1 : -1)));
        return false;
    }
    int& v = c.item == 0 ? screen : lights;
    int pct = percentOf(v) + (w == Wheel::Up ? 5 : -5);
    pct = std::max(c.item == 0 ? 10 : 0, std::min(100, pct));
    int nv = valueOf(pct);
    bool changed = nv != v;
    v = nv;
    return changed;
}

Lines formatSimPage(int page, const SimData& d, const PageCursor& cur, bool side) {
    if (side && isSidePage(page)) return formatSidePage(page, d, cur);
    Lines p;
    int count = pageItemCount(page);
    int item = count ? std::max(0, std::min(count - 1, cur.item)) : 0;
    bool ed = cur.editing;
    switch (page) {
        case PageComNav1:
        case PageComNav2: {
            bool one = page == PageComNav1;
            const char* n = one ? "1" : "2";
            std::string com = comFreq(one ? d.com1 : d.com2), coms = comFreq(one ? d.com1s : d.com2s);
            std::string nav = navFreq(one ? d.nav1 : d.nav2), navs = navFreq(one ? d.nav1s : d.nav2s);
            if (item < 3) {
                p.l[0] = activeLine(std::string("COM") + n, com, item == 0);
                p.l[1] = standbyLine(coms, item == 1, item == 2, ed);
                p.l[2] = "N" + nav + "  " + navs;
            } else {
                p.l[0] = activeLine(std::string("NAV") + n, nav, item == 3);
                p.l[1] = standbyLine(navs, item == 4, item == 5, ed);
                p.l[2] = "C" + com + " " + coms;
            }
            break;
        }
        case PageAdf: {
            bool first = item < 4;
            int part = item % 4;
            std::string act = fmt("%4d", first ? d.adf1 : d.adf2), stby = fmt("%4d", first ? d.adf1s : d.adf2s);
            p.l[0] = activeLine(first ? "ADF1" : "ADF2", act, part == 0);
            p.l[1] = standbyLine(stby, part == 1, part >= 2, ed, part == 2 ? "10" : "1");
            p.l[2] = fmt("ADF%d %4d %4d", first ? 2 : 1, first ? d.adf2 : d.adf1, first ? d.adf2s : d.adf1s);
            break;
        }
        case PageDme:
            p.l[0] = dmeLine(1, d.dme1, d.dme1nm, d.dme1kt);
            p.l[1] = dmeLine(2, d.dme2, d.dme2nm, d.dme2kt);
            break;
        case PageXpdr: {
            static const char* const modes[] = {"OFF", "STBY", "ON", "ALT", "TEST"};
            std::string mode = d.xpdrMode >= 0 && d.xpdrMode <= 4 ? modes[d.xpdrMode] : "?";
            p.l[0] = fmt("XPDR %04d", d.xpdrCode);
            if (item < 4) {
                p.l[1] = std::string(5 + item, ' ') + "^";   // under the digit the cursor is on
                if (ed) p.l[1] += std::string(11 - p.l[1].size(), ' ') + "EDIT";
                p.l[2] = "MODE  " + mode;
            } else {
                p.l[2] = std::string("MODE") + (ed ? ">>" : " >") + mode;
            }
            break;
        }
        case PageAutopilot: {
            p.l[0] = fmt("HDG %03d ALT%5d", wrap360(d.apHdg), (int)lroundf(d.apAlt));
            p.l[1] = fmt("VS %+5d IAS %3d", (int)lroundf(d.apVs), (int)lroundf(d.apIas));
            std::string v = item == 0 ? fmt("HDG %03d", wrap360(d.apHdg))
                          : item == 1 ? fmt("ALT %d", (int)lroundf(d.apAlt))
                          : item == 2 ? fmt("VS %+d", (int)lroundf(d.apVs))
                          : fmt("IAS %d", (int)lroundf(d.apIas));
            std::string left = marked(v, true, ed);
            const char* status = d.apOn ? "AP" : d.fdMode == 1 ? "FD" : "--";
            if (left.size() < 14) left += std::string(14 - left.size(), ' ');
            p.l[2] = left + status;
            break;
        }
        case PageVorHdg:
            p.l[0] = marked(fmt("OBS1 %03d", wrap360(d.obs1)), item == 0, ed);
            p.l[1] = marked(fmt("OBS2 %03d", wrap360(d.obs2)), item == 1, ed);
            p.l[2] = marked(fmt("HDG BUG %03d", wrap360(d.apHdg)), item == 2, ed);
            break;
        default:
            break;
    }
    for (auto& l : p.l) l = mfdText(l);
    return p;
}

int dialRepeat(double gapSeconds, const DialAccel& d) {
    if (!d.on || d.factor <= 1) return 1;
    double gap = gapSeconds * 1000.0, slow = d.slowMs, fast = std::min(d.fastMs, d.slowMs);
    if (gap >= slow) return 1;
    if (gap <= fast) return d.factor;
    double t = (slow - gap) / (slow - fast);   // 0 at the slow end, 1 at the fast end
    return 1 + (int)lround((d.factor - 1) * t);
}

std::vector<std::string> pageInput(int page, PageCursor& c, Wheel w, bool com833, int repeat, bool side) {
    if (side && isSidePage(page)) return sidePageInput(page, c, w, com833, repeat);
    int count = pageItemCount(page);
    if (count == 0) {
        c = PageCursor();
        return {};
    }
    c.item = std::max(0, std::min(count - 1, c.item));
    Item it = itemFor(page, c.item, com833);
    if (w == Wheel::Select) {
        if (it.swap) return {it.select};
        c.editing = !c.editing;
        return {};
    }
    if (c.editing && !it.swap) {
        int n = it.accel ? std::max(1, repeat) : 1;
        return std::vector<std::string>(n, w == Wheel::Up ? it.up : it.down);
    }
    c.editing = false;
    c.item = (c.item + (w == Wheel::Down ? 1 : count - 1)) % count;
    return {};
}

}  // namespace x52
