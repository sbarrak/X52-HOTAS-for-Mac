// X52 Pro screen (MFD) and light protocol, plus the text pages shown on the screen.
// Pure C++ so it can be unit tested. Protocol constants follow libx52 (commands.h).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "profile.h"

namespace x52 {

constexpr int kMfdCols = 16;
constexpr int kMfdLines = 3;

// One USB vendor request: bmRequestType 0x40, bRequest 0x91, wValue = value, wIndex = index.
struct UsbCmd {
    uint16_t index = 0;
    uint16_t value = 0;
    bool operator==(const UsbCmd& o) const { return index == o.index && value == o.value; }
};

// Printable ASCII only, at most 16 characters, trailing spaces removed.
std::string mfdText(const std::string& s);
// Clear the line, then write its text two characters per request.
std::vector<UsbCmd> packLine(int line, const std::string& text);
void packLed(int led, Color c, std::vector<UsbCmd>& out);
UsbCmd packMfdBrightness(int v);   // 0..127
UsbCmd packLedBrightness(int v);   // 0..127
UsbCmd packShift(bool on);         // the SHIFT indicator on the screen
UsbCmd packClock(int hour, int minute, bool h24);
// The date and year requests for a date order (0 DD-MM-YY, 1 MM-DD-YY, 2 YY-MM-DD).
std::vector<UsbCmd> packDate(int day, int month, int year2, int format);
// Clock 2 or 3 as minutes ahead of (or behind) clock 1; the stick takes at most 1023.
UsbCmd packClockOffset(int clock, int minutes, bool h24);
UsbCmd packBlink(bool on);         // flashes the clutch and POV lights

// A time and date for clock 1.
struct ClockTime {
    int hour = 0, minute = 0, day = 1, month = 1, year2 = 0;
};
// The GMT time moved by `minutes`, the date following past midnight.
ClockTime shiftClock(const ClockTime& gmt, int minutes);
// What the stick needs for clock 2 or 3 (minutes from clock 1) when it is `fromGmt` and clock 1 is `firstFromGmt`
// minutes from GMT: the same time of day, between -720 and 719.
int clockOffsetFromFirst(int fromGmt, int firstFromGmt);
// How far a local time is from GMT, between -720 and 719 minutes (the two may be on different days).
int localMinusGmt(int localHour, int localMinute, int gmtHour, int gmtMinute);
// Clock 1's minutes from GMT for this profile. macLocal: the Mac's local time's; simLocal: the simulator's
// (simKnown false: X-Plane is not running, the Mac's is used).
int clock1FromGmt(const Profile& p, int macLocal, bool simKnown, int simLocal);

// Values read from the simulator for the radio / autopilot pages.
struct SimData {
    int com1 = 0, com1s = 0, com2 = 0, com2s = 0;   // kHz, e.g. 118000
    int nav1 = 0, nav1s = 0, nav2 = 0, nav2s = 0;   // 10 kHz, e.g. 11050
    int adf1 = 0, adf1s = 0, adf2 = 0, adf2s = 0;   // kHz
    bool dme1 = false, dme2 = false;
    float dme1nm = 0, dme1kt = 0, dme2nm = 0, dme2kt = 0;
    int xpdrCode = 0, xpdrMode = 0;                 // mode: 0 off 1 stby 2 on 3 alt 4 test
    float apHdg = 0, apAlt = 0, apVs = 0, apIas = 0;
    bool apOn = false;
    int fdMode = 0;                                 // 0 off 1 flight director 2 autopilot
    float obs1 = 0, obs2 = 0;
};

// The radio pages come first (the stick starts on COM1 / NAV1), then the user's own pages (see pagetree.h), then
// the Buttons page, and the Brightness page is the very last one.
enum Page { PageComNav1, PageComNav2, PageAdf, PageDme, PageXpdr, PageAutopilot, PageVorHdg, PageButtons, PageBrightness, kPageCount };
const char* pageName(int page);
const char* pageName(int page, bool side);   // side-by-side layout: "COM 1+2", "NAV 1+2", "ADF 1+2" for the radio slots

struct Lines {
    std::string l[kMfdLines];
};

// Moving controls (axes, hat, mouse nub) by HID usage, for the test mode.
const char* controlName(int usagePage, int usage);
std::string controlValueText(int usagePage, int usage, long value, long min, long max);
// True when the control moved far enough since `shown` to be worth displaying.
// POV hat value as a direction: 0 up, 1 right, 2 down, 3 left; -1 centred or diagonal.
int hatDirection(long value, long min, long max);
// The buttons a POV 1 value holds, as bits: 1 up, 2 right, 4 down, 8 left (a diagonal holds two); 0 when centred.
int hatButtonMask(long value, long min, long max);
// Four direction buttons (a hat made of buttons) as one word: "up", "up-right", ...; "" when none is held.
std::string povButtonsText(bool up, bool right, bool down, bool left);
bool controlMoved(int usagePage, int usage, long shown, long value, long min, long max);

Lines formatButtonsPage(const std::string& profileName, const std::string& label, int mode, bool shift);

}  // namespace x52
