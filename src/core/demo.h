// The Demo button's show: what the stick's screen and lights look like at each moment. About twelve seconds:
// a welcome while the lights come on one by one, the three colours, a short flash, a list scrolling down and
// back up, and a finale that fades in. Pure C++ so it can be unit tested.
#pragma once
#include "mfd.h"

namespace x52 {

constexpr double kDemoSeconds = 12.0;

struct DemoFrame {
    Lines lines;
    Color leds[kLedCount] = {};
    int mfdBrightness = 127;
    int ledBrightness = 127;
};

// The frame `t` seconds after the show started (0 <= t < kDemoSeconds).
DemoFrame demoFrame(double t);

}  // namespace x52
