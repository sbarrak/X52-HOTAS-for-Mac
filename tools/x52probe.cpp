// x52probe: checks the X52 hardware without X-Plane.
//   x52probe                 list the stick's controls, then show button presses for 30 s
//   x52probe --watch 60      same, watching for 60 s
//   x52probe --text A B C    write three lines on the stick's screen
//   x52probe --leds red      set every two-colour light (off|green|amber|red)
//   x52probe --identify      show each pressed button's name on the stick's screen (10 min)
//   x52probe --clear         blank the screen
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "../src/core/mfd.h"
#include "../src/core/profile.h"
#include "../src/mac/x52device.h"
#include "../src/mac/x52input.h"

using namespace x52;

static int sendAndReport(const std::vector<UsbCmd>& cmds, const char* what) {
    std::string err;
    int pid = 0;
    if (!X52Device::sendNow(cmds, &err, &pid)) {
        printf("%s: FAILED (%s)\n", what, err.c_str());
        return 1;
    }
    printf("%s: ok, %zu USB requests accepted by product 0x%04x%s\n", what, cmds.size(), pid,
           pid == kProductX52Pro ? " (X52 Pro)" : " (X52)");
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 2 && !strcmp(argv[1], "--text")) {
        std::vector<UsbCmd> cmds;
        for (int i = 0; i < kMfdLines; ++i) {
            std::vector<UsbCmd> l = packLine(i, i + 2 < argc ? argv[i + 2] : "");
            cmds.insert(cmds.end(), l.begin(), l.end());
        }
        return sendAndReport(cmds, "screen text");
    }
    if (argc >= 2 && !strcmp(argv[1], "--clear")) {
        std::vector<UsbCmd> cmds;
        for (int i = 0; i < kMfdLines; ++i) {
            std::vector<UsbCmd> l = packLine(i, "");
            cmds.insert(cmds.end(), l.begin(), l.end());
        }
        return sendAndReport(cmds, "screen clear");
    }
    if (argc >= 3 && !strcmp(argv[1], "--leds")) {
        Color c = Color::Off;
        bool known = false;
        for (int i = 0; i < 4; ++i)
            if (!strcmp(argv[2], colorName((Color)i))) {
                c = (Color)i;
                known = true;
            }
        if (!known) {
            printf("colour must be off, green, amber or red\n");
            return 2;
        }
        std::vector<UsbCmd> cmds;
        for (int l = 0; l < kLedCount; ++l) packLed(l, ledIsSingleColor(l) ? Color::Green : c, cmds);
        return sendAndReport(cmds, "lights");
    }

    int seconds = 30;
    bool identify = false;
    if (argc >= 3 && !strcmp(argv[1], "--watch")) seconds = atoi(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "--identify")) {
        identify = true;
        seconds = argc >= 3 ? atoi(argv[2]) : 600;
    }

    X52Input input;
    input.setWantAxes(identify);
    X52Device screen;
    DeviceState shown;
    for (int l = 0; l < kLedCount; ++l) shown.leds[l] = Color::Green;
    struct Seen {
        long lo, hi, lastShown;
    };
    std::map<std::pair<int, int>, Seen> axes;
    if (identify) {
        shown.lines[0] = "MOVE OR PRESS";
        shown.lines[1] = "any control";
        screen.start();
        screen.setState(shown);
    }
    input.start();
    for (int i = 0; i < 20 && !input.connected(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!input.connected()) {
        printf("No X52 / X52 Pro found. Plug it in and run this again.\n");
        return 1;
    }
    printf("Found: %s\n", input.productName().c_str());
    std::map<std::string, int> counts;
    int maxButton = 0;
    for (const HidElementInfo& e : input.elements()) {
        ++counts[e.kind];
        if (e.kind == "button" && e.usage > maxButton) maxButton = e.usage;
        if (e.kind != "button")
            printf("  %-5s page 0x%02x usage 0x%02x  range %ld..%ld\n", e.kind.c_str(), e.usagePage, e.usage, e.min, e.max);
    }
    printf("  buttons: %d (numbered 1..%d)   axes: %d   hats: %d\n", counts["button"], maxButton, counts["axis"],
           counts["hat"]);

    std::vector<ButtonEvent> ev;
    input.drain(ev);
    printf("Held right now:");
    for (const ButtonEvent& e : ev)
        if (e.down) printf(" %d (%s)", e.button, buttonName(e.button));
    printf("\n\nPress buttons for the next %d seconds...\n", seconds);
    fflush(stdout);

    for (int t = 0; t < seconds * 20; ++t) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ev.clear();
        input.drain(ev);
        for (const ButtonEvent& e : ev) {
            printf("  %7.2fs  button %2d %-4s  expected: %s\n", t * 0.05, e.button, e.down ? "DOWN" : "up",
                   buttonName(e.button));
            if (identify && e.down) {
                // Show the name on the stick's own screen so no terminal is needed.
                shown.lines[0] = "Button " + std::to_string(e.button);
                shown.lines[1] = buttonName(e.button);
                shown.lines[2] = "";
                screen.setState(shown);
            }
            fflush(stdout);
        }
        if (identify) {
            std::vector<AxisEvent> av;
            input.drainAxes(av);
            for (const AxisEvent& e : av) {
                auto key = std::make_pair(e.usagePage, e.usage);
                auto it = axes.find(key);
                if (it == axes.end()) {
                    // The first report is just where the control rests; show it only once it moves.
                    axes[key] = {e.value, e.value, e.value};
                    continue;
                }
                Seen& sn = it->second;
                sn.lo = std::min(sn.lo, e.value);
                sn.hi = std::max(sn.hi, e.value);
                if (!controlMoved(e.usagePage, e.usage, sn.lastShown, e.value, e.min, e.max)) continue;
                sn.lastShown = e.value;
                shown.lines[0] = controlName(e.usagePage, e.usage);
                shown.lines[1] = controlValueText(e.usagePage, e.usage, e.value, e.min, e.max);
                shown.lines[2] = "";
                printf("  %7.2fs  %-18s %s\n", t * 0.05, shown.lines[0].c_str(), shown.lines[1].c_str());
                fflush(stdout);
                screen.setState(shown);
            }
        }
        if (!input.connected()) {
            printf("Stick disconnected.\n");
            break;
        }
    }
    if (identify) {
        printf("\nMoving controls seen (lowest..highest raw value reached):\n");
        for (auto& kv : axes)
            printf("  %-18s page 0x%02x usage 0x%02x  %ld..%ld%s\n", controlName(kv.first.first, kv.first.second),
                   kv.first.first, kv.first.second, kv.second.lo, kv.second.hi,
                   kv.second.lo == kv.second.hi ? "   (never moved)" : "");
        screen.stop();
    }
    input.stop();
    return 0;
}
