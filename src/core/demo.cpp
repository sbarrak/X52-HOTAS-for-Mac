#include "demo.h"

#include <algorithm>
#include <string>

namespace x52 {

namespace {

// When each part starts, in seconds.
constexpr double kColours = 2.0, kFlash = 5.0, kScroll = 7.0, kFinale = 10.5;

void all(DemoFrame& f, Color c) {
    for (Color& led : f.leds) led = c;
}

}  // namespace

DemoFrame demoFrame(double t) {
    DemoFrame f;
    if (t < kColours) {   // welcome: the lights come on one after another
        f.lines = {{"", "  X52 PRO DEMO", ""}};
        for (int l = 0; l < kLedCount; ++l)
            if (t >= 0.2 + 0.13 * l) f.leds[l] = Color::Green;
    } else if (t < kFlash) {   // one second each
        int i = std::min(2, (int)(t - kColours));
        const char* names[] = {"     GREEN", "     AMBER", "      RED"};
        const Color colors[] = {Color::Green, Color::Amber, Color::Red};
        f.lines = {{"    COLOURS", "", names[i]}};
        all(f, colors[i]);
    } else if (t < kScroll) {   // twice a second: lively, and well under what is uncomfortable to look at
        bool on = (int)((t - kFlash) / 0.25) % 2 == 0;
        f.lines = {{"     ALERTS", "", on ? "  LIGHTS FLASH" : ""}};
        all(f, on ? Color::Red : Color::Off);
    } else if (t < kFinale) {   // a list: three steps down, three back up
        const char* items[] = {"RADIOS", "NAV", "HEADING", "ALTITUDE", "FLAPS", "GEAR"};
        int step = std::min(6, (int)((t - kScroll) / 0.5));
        int top = step <= 3 ? step : 6 - step;
        for (int i = 0; i < kMfdLines; ++i)
            f.lines.l[i] = "ITEM " + std::to_string(top + i + 1) + "  " + items[top + i];
        f.leds[(int)((t - kScroll) / 0.1) % kLedCount] = Color::Amber;   // one light runs round
    } else {   // everything fades in, green
        double rise = std::min(1.0, (t - kFinale) / 1.0);
        f.lines = {{"   YOUR TURN!", "", "  READY TO FLY"}};
        all(f, Color::Green);
        f.ledBrightness = (int)(10 + 117 * rise);
        f.mfdBrightness = (int)(30 + 97 * rise);
    }
    return f;
}

}  // namespace x52
