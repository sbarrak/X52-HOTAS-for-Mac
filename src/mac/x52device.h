// X52 / X52 Pro screen and lights over USB (IOKit). Writes happen on a worker thread
// so the simulator never waits on the stick. The caller describes the state it wants;
// the worker sends only what changed and resends everything after a reconnect.
#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "../core/mfd.h"

namespace x52 {

constexpr int kVendorSaitek = 0x06a3;
constexpr int kProductX52Pro = 0x0762;
constexpr int kProductX52a = 0x0255;
constexpr int kProductX52b = 0x075c;

struct DeviceState {
    std::string lines[kMfdLines];
    Color leds[kLedCount] = {};
    bool shift = false;
    int mfdBrightness = 127;
    int ledBrightness = 127;
    bool clockValid = false;
    int hour = 0, minute = 0, day = 1, month = 1, year2 = 0;   // clock 1 (GMT)
    bool blink = false;               // clutch and POV lights flash
    int clockOffset[2] = {0, 0};      // clock 2 and 3, minutes from clock 1
    bool h12[3] = {false, false, false};
    int dateFormat = 0;
};

class X52Device {
public:
    X52Device();
    ~X52Device();
    void start();
    void stop(bool blank = true);         // blanks the screen (unless told not to), then joins the worker
    void setState(const DeviceState& s);  // thread-safe; cheap if nothing changed
    bool connected() const { return connected_; }
    bool isPro() const { return isPro_; }
    long requestsSent() const { return sent_; }
    std::string lastError() const;

    // Used by the probe tool: send raw requests synchronously on the calling thread.
    static bool sendNow(const std::vector<UsbCmd>& cmds, std::string* error, int* productId);

private:
    void run();
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    DeviceState desired_;
    bool dirty_ = false;
    bool quit_ = false;
    bool leave_ = false;   // quitting without touching the stick again
    std::atomic<bool> connected_{false};
    std::atomic<bool> isPro_{false};
    std::atomic<long> sent_{0};
    std::string lastError_;
};

}  // namespace x52
