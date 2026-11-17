// Speed-sensitive right wheel on the stick's screen. Pure C++.
#pragma once

namespace x52 {

// Turning the wheel fast changes a value in bigger jumps: a click that comes less than slowMs after the
// previous one repeats the change, up to `factor` times once the gap is fastMs or less (in between it ramps).
struct DialAccel {
    bool on = true;
    int slowMs = 150;
    int fastMs = 60;
    int factor = 10;
};
// How many times to run the command for a click that came gapSeconds after the previous one (1: just once).
int dialRepeat(double gapSeconds, const DialAccel& d);

}  // namespace x52
