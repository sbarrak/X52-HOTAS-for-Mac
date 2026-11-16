#include "mfd.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace x52 {

namespace {

const uint16_t kLineWrite[kMfdLines] = {0xd1, 0xd2, 0xd4};
const uint16_t kClearBit = 0x08;
const uint16_t kIdxMfdBrightness = 0xb1, kIdxLedBrightness = 0xb2, kIdxLed = 0xb8;
const uint16_t kIdxClock = 0xc0, kIdxDate = 0xc4, kIdxYear = 0xc8, kIdxShift = 0xfd;

// Wire id of each light; two-colour lights use id for red and id + 1 for green.
const int kLedId[kLedCount] = {0x01, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, 0x10, 0x12, 0x14};

std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char* f, ...) {
    char buf[64];
    va_list ap;
    va_start(ap, f);
    vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

}  // namespace

std::string mfdText(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        if ((int)r.size() >= kMfdCols) break;
        r += (c >= 0x20 && c < 0x7f) ? (char)c : '?';
    }
    while (!r.empty() && r.back() == ' ') r.pop_back();
    return r;
}

std::vector<UsbCmd> packLine(int line, const std::string& text) {
    std::vector<UsbCmd> v;
    if (line < 0 || line >= kMfdLines) return v;
    std::string t = mfdText(text);
    if (t.size() % 2) t += ' ';
    v.push_back({(uint16_t)(kLineWrite[line] | kClearBit), 0});
    for (size_t i = 0; i < t.size(); i += 2)
        v.push_back({kLineWrite[line], (uint16_t)(((unsigned char)t[i + 1] << 8) | (unsigned char)t[i])});
    return v;
}

void packLed(int led, Color c, std::vector<UsbCmd>& out) {
    if (led < 0 || led >= kLedCount) return;
    int id = kLedId[led];
    if (ledIsSingleColor(led)) {
        out.push_back({kIdxLed, (uint16_t)((id << 8) | (c != Color::Off ? 1 : 0))});
        return;
    }
    bool red = c == Color::Red || c == Color::Amber;
    bool green = c == Color::Green || c == Color::Amber;
    out.push_back({kIdxLed, (uint16_t)((id << 8) | (red ? 1 : 0))});
    out.push_back({kIdxLed, (uint16_t)(((id + 1) << 8) | (green ? 1 : 0))});
}

UsbCmd packMfdBrightness(int v) { return {kIdxMfdBrightness, (uint16_t)std::max(0, std::min(127, v))}; }
UsbCmd packLedBrightness(int v) { return {kIdxLedBrightness, (uint16_t)std::max(0, std::min(127, v))}; }
UsbCmd packShift(bool on) { return {kIdxShift, (uint16_t)(on ? 0x51 : 0x50)}; }
UsbCmd packClock(int hour, int minute, bool h24) {
    return {kIdxClock, (uint16_t)((h24 ? 0x8000 : 0) | ((hour & 0x7f) << 8) | (minute & 0xff))};
}
std::vector<UsbCmd> packDate(int day, int month, int year2, int format) {
    int first = day, second = month, third = year2;   // DD-MM-YY
    if (format == 1) first = month, second = day;
    else if (format == 2) first = year2, third = day;
    return {{kIdxDate, (uint16_t)(((second & 0xff) << 8) | (first & 0xff))}, {kIdxYear, (uint16_t)(third & 0xff)}};
}
UsbCmd packClockOffset(int clock, int minutes, bool h24) {
    bool negative = minutes < 0;
    int off = negative ? -minutes : minutes;
    // More than 1023 minutes does not fit: go round the day the other way instead.
    while (off > 1023) off -= 1440;
    if (off < 0) {
        negative = !negative;
        off = -off;
    }
    return {(uint16_t)(clock == 3 ? 0xc2 : 0xc1), (uint16_t)((h24 ? 0x8000 : 0) | (negative ? 0x400 : 0) | (off & 0x3ff))};
}
UsbCmd packBlink(bool on) { return {0xb4, (uint16_t)(on ? 0x51 : 0x50)}; }

const char* pageName(int page) {
    static const char* const names[kPageCount] = {"Buttons",     "COM1 / NAV1", "COM2 / NAV2", "ADF",
                                                  "DME",         "Transponder", "Autopilot",   "OBS / Heading"};
    return page >= 0 && page < kPageCount ? names[page] : "";
}

namespace {
bool isHat(int usagePage, int usage) { return usagePage == 0x01 && usage == 0x39; }
}  // namespace

// Usages as the X52 Pro reports them; the two rotaries were identified on a real stick.
const char* controlName(int usagePage, int usage) {
    if (usagePage == 0x01) {
        switch (usage) {
            case 0x30: return "Stick left/right";
            case 0x31: return "Stick fwd/back";
            case 0x35: return "Stick twist";
            case 0x32: return "Throttle";
            case 0x33: return "Lower rotary (i)";
            case 0x34: return "Upper rotary (E)";
            case 0x36: return "Slider";
            case 0x39: return "POV hat";
        }
    }
    if (usagePage == 0x05 && usage == 0x24) return "Mouse nub L/R";
    if (usagePage == 0x05 && usage == 0x26) return "Mouse nub U/D";
    return "Unknown control";
}

std::string controlValueText(int usagePage, int usage, long value, long min, long max) {
    if (isHat(usagePage, usage)) {
        static const char* const dirs[] = {"up", "up-right", "right", "down-right",
                                           "down", "down-left", "left", "up-left"};
        long i = value - min;
        return i >= 0 && i < 8 && value <= max ? dirs[i] : "centre";
    }
    long range = max - min;
    long pct = range > 0 ? ((value - min) * 100 + range / 2) / range : 0;
    return std::to_string(std::max(0L, std::min(100L, pct))) + " %";
}

int hatDirection(long value, long min, long max) {
    long i = value - min;
    if (i < 0 || i >= 8 || value > max || i % 2) return -1;
    return (int)(i / 2);
}

bool controlMoved(int usagePage, int usage, long shown, long value, long min, long max) {
    long step = isHat(usagePage, usage) ? 1 : std::max(1L, (max - min) / 50);   // ignore jitter under 2 %
    return labs(value - shown) >= step;
}

Lines formatButtonsPage(const std::string& profileName, const std::string& label, int mode, bool shift) {
    Lines p;
    p.l[0] = mfdText(profileName);
    p.l[1] = mfdText(label);
    p.l[2] = mfdText(fmt("Mode %d", mode) + (shift ? " + Pinkie" : ""));
    return p;
}

}  // namespace x52
