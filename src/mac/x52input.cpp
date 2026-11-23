#include "x52input.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>

#include "../core/mfd.h"
#include "../core/profile.h"
#include "x52device.h"

namespace x52 {

namespace {

CFDictionaryRef matchDict(int vendor, int product) {
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                         &kCFTypeDictionaryValueCallBacks);
    CFNumberRef v = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vendor);
    CFNumberRef p = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &product);
    CFDictionarySetValue(d, CFSTR(kIOHIDVendorIDKey), v);
    CFDictionarySetValue(d, CFSTR(kIOHIDProductIDKey), p);
    CFRelease(v);
    CFRelease(p);
    return d;
}

void onValue(void* ctx, IOReturn, void*, IOHIDValueRef value) {
    IOHIDElementRef el = IOHIDValueGetElement(value);
    X52Input* self = static_cast<X52Input*>(ctx);
    if (IOHIDElementGetUsagePage(el) != kHIDPage_Button) {
        self->pushAxis({(int)IOHIDElementGetUsagePage(el), (int)IOHIDElementGetUsage(el),
                        (long)IOHIDValueGetIntegerValue(value), (long)IOHIDElementGetLogicalMin(el),
                        (long)IOHIDElementGetLogicalMax(el)});
        return;
    }
    self->push((int)IOHIDElementGetUsage(el), IOHIDValueGetIntegerValue(value) != 0);
}

void onAttach(void* ctx, IOReturn, void*, IOHIDDeviceRef device) {
    X52Input* self = static_cast<X52Input*>(ctx);
    std::string name = "X52";
    CFTypeRef prod = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
    if (prod && CFGetTypeID(prod) == CFStringGetTypeID()) {
        char buf[128];
        if (CFStringGetCString((CFStringRef)prod, buf, sizeof buf, kCFStringEncodingUTF8)) name = buf;
    }
    std::vector<HidElementInfo> infos;
    CFArrayRef els = IOHIDDeviceCopyMatchingElements(device, nullptr, kIOHIDOptionsTypeNone);
    if (els) {
        for (CFIndex i = 0; i < CFArrayGetCount(els); ++i) {
            IOHIDElementRef el = (IOHIDElementRef)CFArrayGetValueAtIndex(els, i);
            IOHIDElementType type = IOHIDElementGetType(el);
            if (type != kIOHIDElementTypeInput_Button && type != kIOHIDElementTypeInput_Misc &&
                type != kIOHIDElementTypeInput_Axis)
                continue;
            HidElementInfo info;
            info.usagePage = (int)IOHIDElementGetUsagePage(el);
            info.usage = (int)IOHIDElementGetUsage(el);
            info.min = IOHIDElementGetLogicalMin(el);
            info.max = IOHIDElementGetLogicalMax(el);
            if (info.usagePage == kHIDPage_Button) info.kind = "button";
            else if (info.usagePage == kHIDPage_GenericDesktop && info.usage == kHIDUsage_GD_Hatswitch) info.kind = "hat";
            else if (info.usagePage == kHIDPage_GenericDesktop) info.kind = "axis";
            else info.kind = "other";
            infos.push_back(info);
            // Report where every switch currently sits (the mode dial is a held button).
            if (info.usagePage == kHIDPage_Button) {
                IOHIDValueRef v = nullptr;
                if (IOHIDDeviceGetValue(device, el, &v) == kIOReturnSuccess && v)
                    self->push(info.usage, IOHIDValueGetIntegerValue(v) != 0);
            } else if (info.kind == "axis") {
                // Where each axis rests at the start: no event comes until it moves.
                IOHIDValueRef v = nullptr;
                if (IOHIDDeviceGetValue(device, el, &v) == kIOReturnSuccess && v)
                    self->pushAxis({info.usagePage, info.usage, (long)IOHIDValueGetIntegerValue(v), info.min, info.max});
            }
        }
        CFRelease(els);
    }
    self->setConnected(true, name, infos);
}

void onDetach(void* ctx, IOReturn, void*, IOHIDDeviceRef) {
    X52Input* self = static_cast<X52Input*>(ctx);
    // Nothing can still be held on a stick that is gone.
    for (int b = 1; b <= 64; ++b) self->push(b, false);
    self->releaseHat();
    self->setConnected(false, "", {});
}

}  // namespace

X52Input::X52Input() {}
X52Input::~X52Input() { stop(); }

void X52Input::push(int button, bool down) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back({button, down});
}

void X52Input::releaseHat() {
    std::lock_guard<std::mutex> lock(mutex_);
    hatMask_ = 0;
    nubMask_ = 0;
    nubX_ = nubY_ = 0;
}

void X52Input::pushAxis(const AxisEvent& e) {
    bool hat = e.usagePage == 0x01 && e.usage == 0x39;
    std::lock_guard<std::mutex> lock(mutex_);
    axisLatest_[{e.usagePage, e.usage}] = e;
    if (hat) {   // POV 1 is a hat switch, not buttons: make buttons 40..43 of it so profiles can bind it (a diagonal holds two)
        int mask = hatButtonMask(e.value, e.min, e.max);
        for (int i = 0; i < 4; ++i)
            if (((mask ^ hatMask_) >> i) & 1) pending_.push_back({kPov1Up + i, ((mask >> i) & 1) != 0});
        hatMask_ = mask;
    }
    if (e.usagePage == 0x05 && (e.usage == 0x24 || e.usage == 0x26)) {   // the throttle's mouse nub: buttons 44..51 (arrows; far = Shift + arrows)
        (e.usage == 0x24 ? nubX_ : nubY_) = nubAxisFraction(e.value, e.min, e.max);
        int mask = nubButtonMask(nubX_, nubY_, nubMask_);
        for (int i = 0; i < 8; ++i)
            if (((mask ^ nubMask_) >> i) & 1) pending_.push_back({kNubUp + i, ((mask >> i) & 1) != 0});
        nubMask_ = mask;
    }
    if (!wantAxes_ && !hat) return;
    axisPending_.push_back(e);
}

std::vector<AxisEvent> X52Input::latestAxes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AxisEvent> out;
    for (const auto& kv : axisLatest_) out.push_back(kv.second);
    return out;
}

void X52Input::drainAxes(std::vector<AxisEvent>& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    out.insert(out.end(), axisPending_.begin(), axisPending_.end());
    axisPending_.clear();
}

void X52Input::setConnected(bool c, const std::string& name, const std::vector<HidElementInfo>& els) {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_ = c;
    name_ = name;
    elements_ = els;
}

std::string X52Input::productName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return name_;
}

std::vector<HidElementInfo> X52Input::elements() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return elements_;
}

void X52Input::drain(std::vector<ButtonEvent>& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    out.insert(out.end(), pending_.begin(), pending_.end());
    pending_.clear();
}

void X52Input::start() {
    if (thread_.joinable()) return;
    started_ = false;
    thread_ = std::thread([this] { run(); });
    while (!started_) std::this_thread::yield();
}

void X52Input::stop() {
    if (!thread_.joinable()) return;
    CFRunLoopStop((CFRunLoopRef)runLoop_);
    thread_.join();
    runLoop_ = nullptr;
    connected_ = false;
}

void X52Input::run() {
    CFRunLoopRef loop = CFRunLoopGetCurrent();
    runLoop_ = loop;

    IOHIDManagerRef mgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    CFDictionaryRef dicts[3] = {matchDict(kVendorSaitek, kProductX52Pro), matchDict(kVendorSaitek, kProductX52a),
                                matchDict(kVendorSaitek, kProductX52b)};
    CFArrayRef arr = CFArrayCreate(kCFAllocatorDefault, (const void**)dicts, 3, &kCFTypeArrayCallBacks);
    IOHIDManagerSetDeviceMatchingMultiple(mgr, arr);
    CFRelease(arr);
    for (CFDictionaryRef d : dicts) CFRelease(d);

    IOHIDManagerRegisterDeviceMatchingCallback(mgr, onAttach, this);
    IOHIDManagerRegisterDeviceRemovalCallback(mgr, onDetach, this);
    IOHIDManagerRegisterInputValueCallback(mgr, onValue, this);
    IOHIDManagerScheduleWithRunLoop(mgr, loop, kCFRunLoopDefaultMode);
    IOHIDManagerOpen(mgr, kIOHIDOptionsTypeNone);

    // A timer keeps the run loop alive even while no stick is attached.
    CFRunLoopTimerRef keepAlive = CFRunLoopTimerCreateWithHandler(
        kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + 3600, 3600, 0, 0, ^(CFRunLoopTimerRef) {});
    CFRunLoopAddTimer(loop, keepAlive, kCFRunLoopDefaultMode);

    started_ = true;
    CFRunLoopRun();

    CFRunLoopRemoveTimer(loop, keepAlive, kCFRunLoopDefaultMode);
    CFRelease(keepAlive);
    IOHIDManagerUnscheduleFromRunLoop(mgr, loop, kCFRunLoopDefaultMode);
    IOHIDManagerClose(mgr, kIOHIDOptionsTypeNone);
    CFRelease(mgr);
}

}  // namespace x52
