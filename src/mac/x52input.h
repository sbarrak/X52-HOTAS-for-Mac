// X52 button input through macOS's HID manager. Shared (non-exclusive) access, so
// X-Plane keeps seeing the stick's axes and buttons as usual. Events are collected on
// a private thread and handed over when the owner calls drain().
#pragma once
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace x52 {

struct ButtonEvent {
    int button;   // 1-based HID button number
    bool down;
};

// A moving control (axis, hat, mouse nub). Only the probe tool uses these;
// in X-Plane the axes stay with the simulator.
struct AxisEvent {
    int usagePage, usage;
    long value, min, max;
};

struct HidElementInfo {
    std::string kind;   // "button", "axis", "hat", "other"
    int usagePage, usage;
    long min, max;
};

class X52Input {
public:
    X52Input();
    ~X52Input();
    void start();
    void stop();
    void drain(std::vector<ButtonEvent>& out);   // moves pending events into out
    void setWantAxes(bool want) { wantAxes_ = want; }
    void drainAxes(std::vector<AxisEvent>& out);
    void pushAxis(const AxisEvent& e);
    // Where every analog control sits right now (the last value the stick reported, whether or not axis events are wanted).
    std::vector<AxisEvent> latestAxes() const;
    bool connected() const { return connected_; }

    // For the probe tool; filled when a device is matched.
    std::string productName() const;
    std::vector<HidElementInfo> elements() const;

    // Internal (called from the HID thread).
    void push(int button, bool down);
    void releaseHat();   // the stick is gone: POV 1 is centred
    void setConnected(bool c, const std::string& name, const std::vector<HidElementInfo>& els);

private:
    void run();
    std::thread thread_;
    void* runLoop_ = nullptr;
    mutable std::mutex mutex_;
    std::vector<ButtonEvent> pending_;
    std::vector<AxisEvent> axisPending_;
    std::map<std::pair<int, int>, AxisEvent> axisLatest_;
    int nubMask_ = 0;   // mouse nub buttons held now (bits 1 up, 2 right, 4 down, 8 left, 16..128 the same far); under mutex_
    double nubX_ = 0, nubY_ = 0;
    int hatMask_ = 0;   // POV 1 buttons held now (bits 1 up, 2 right, 4 down, 8 left); under mutex_
    std::atomic<bool> wantAxes_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> started_{false};
    std::string name_;
    std::vector<HidElementInfo> elements_;
};

}  // namespace x52
