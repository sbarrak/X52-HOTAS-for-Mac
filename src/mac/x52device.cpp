#include "x52device.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>

#include <chrono>

namespace x52 {

namespace {

typedef IOUSBDeviceInterface182 UsbDev;

const int kProducts[] = {kProductX52Pro, kProductX52a, kProductX52b};

void setNumber(CFMutableDictionaryRef d, CFStringRef key, int v) {
    CFNumberRef n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &v);
    CFDictionarySetValue(d, key, n);
    CFRelease(n);
}

UsbDev** findDevice(int* productOut) {
    for (const char* cls : {"IOUSBHostDevice", "IOUSBDevice"}) {
        for (int pid : kProducts) {
            CFMutableDictionaryRef match = IOServiceMatching(cls);
            if (!match) continue;
            setNumber(match, CFSTR(kUSBVendorID), kVendorSaitek);
            setNumber(match, CFSTR(kUSBProductID), pid);
            io_iterator_t it = 0;
            if (IOServiceGetMatchingServices(kIOMainPortDefault, match, &it) != KERN_SUCCESS) continue;
            io_service_t svc = IOIteratorNext(it);
            IOObjectRelease(it);
            if (!svc) continue;
            IOCFPlugInInterface** plug = nullptr;
            SInt32 score = 0;
            kern_return_t kr = IOCreatePlugInInterfaceForService(svc, kIOUSBDeviceUserClientTypeID,
                                                                 kIOCFPlugInInterfaceID, &plug, &score);
            IOObjectRelease(svc);
            if (kr != KERN_SUCCESS || !plug) continue;
            UsbDev** dev = nullptr;
            (*plug)->QueryInterface(plug, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID182), (LPVOID*)&dev);
            (*plug)->Release(plug);
            if (!dev) continue;
            if (productOut) *productOut = pid;
            return dev;
        }
    }
    return nullptr;
}

std::string ioError(IOReturn r) {
    char buf[64];
    snprintf(buf, sizeof buf, "USB error 0x%08x", (unsigned)r);
    return buf;
}

// Vendor request on the default control pipe; this does not need the device opened,
// so macOS's own joystick driver (and X-Plane) keep working alongside.
IOReturn sendOne(UsbDev** dev, const UsbCmd& c) {
    IOUSBDevRequestTO req;
    memset(&req, 0, sizeof req);
    req.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);
    req.bRequest = 0x91;
    req.wValue = c.value;
    req.wIndex = c.index;
    req.wLength = 0;
    req.pData = nullptr;
    req.noDataTimeout = 1000;
    req.completionTimeout = 1000;
    IOReturn r = kIOReturnError;
    for (int attempt = 0; attempt < 3; ++attempt) {
        r = (*dev)->DeviceRequestTO(dev, &req);
        if (r == kIOReturnSuccess) return r;
        if (r == kIOReturnNotOpen) {
            if ((*dev)->USBDeviceOpen(dev) != kIOReturnSuccess) return r;
            continue;
        }
        if (r == kIOReturnNoDevice || r == kIOReturnNotAttached) return r;
    }
    return r;
}

void closeDevice(UsbDev** dev) {
    if (!dev) return;
    (*dev)->USBDeviceClose(dev);
    (*dev)->Release(dev);
}

}  // namespace

X52Device::X52Device() {}
X52Device::~X52Device() { stop(); }

std::string X52Device::lastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

bool X52Device::sendNow(const std::vector<UsbCmd>& cmds, std::string* error, int* productId) {
    UsbDev** dev = findDevice(productId);
    if (!dev) {
        if (error) *error = "no X52 found on USB";
        return false;
    }
    bool ok = true;
    for (const UsbCmd& c : cmds) {
        IOReturn r = sendOne(dev, c);
        if (r != kIOReturnSuccess) {
            if (error) *error = ioError(r);
            ok = false;
            break;
        }
    }
    closeDevice(dev);
    return ok;
}

void X52Device::start() {
    if (thread_.joinable()) return;
    quit_ = false;
    leave_ = false;
    thread_ = std::thread([this] { run(); });
}

void X52Device::stop(bool blank) {
    if (!thread_.joinable()) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (blank) {
            desired_.lines[0] = desired_.lines[1] = desired_.lines[2] = "";
            desired_.shift = false;
            desired_.blink = false;
            dirty_ = true;
        } else {
            leave_ = true;
        }
        quit_ = true;
    }
    wake_.notify_all();
    thread_.join();
}

void X52Device::setState(const DeviceState& s) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        desired_ = s;
        dirty_ = true;
    }
    wake_.notify_all();
}

void X52Device::run() {
    UsbDev** dev = nullptr;
    DeviceState sent;
    bool fresh = true;   // nothing known about the device's current state

    for (;;) {
        DeviceState want;
        bool quitting;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (!dirty_ && !quit_) wake_.wait_for(lock, std::chrono::milliseconds(dev ? 2000 : 1000));
            want = desired_;
            dirty_ = false;
            quitting = quit_;
            if (leave_) break;
        }

        if (!dev) {
            int pid = 0;
            dev = findDevice(&pid);
            if (dev) {
                isPro_ = pid == kProductX52Pro;
                connected_ = true;
                fresh = true;
            }
        }

        if (dev) {
            std::vector<UsbCmd> cmds;
            if (fresh || want.mfdBrightness != sent.mfdBrightness) cmds.push_back(packMfdBrightness(want.mfdBrightness));
            if (fresh || want.ledBrightness != sent.ledBrightness) cmds.push_back(packLedBrightness(want.ledBrightness));
            for (int i = 0; i < kMfdLines; ++i)
                if (fresh || mfdText(want.lines[i]) != mfdText(sent.lines[i])) {
                    std::vector<UsbCmd> l = packLine(i, want.lines[i]);
                    cmds.insert(cmds.end(), l.begin(), l.end());
                }
            if (fresh || want.shift != sent.shift) cmds.push_back(packShift(want.shift));
            if (isPro_)
                for (int l = 0; l < kLedCount; ++l)
                    if (fresh || want.leds[l] != sent.leds[l]) packLed(l, want.leds[l], cmds);
            if (fresh || want.blink != sent.blink) cmds.push_back(packBlink(want.blink));
            if (want.clockValid) {
                bool first = fresh || !sent.clockValid;
                if (first || want.hour != sent.hour || want.minute != sent.minute || want.h12[0] != sent.h12[0])
                    cmds.push_back(packClock(want.hour, want.minute, !want.h12[0]));
                for (int c = 0; c < 2; ++c)
                    if (first || want.clockOffset[c] != sent.clockOffset[c] || want.h12[c + 1] != sent.h12[c + 1])
                        cmds.push_back(packClockOffset(c + 2, want.clockOffset[c], !want.h12[c + 1]));
                if (first || want.day != sent.day || want.month != sent.month || want.year2 != sent.year2 ||
                    want.dateFormat != sent.dateFormat) {
                    std::vector<UsbCmd> d = packDate(want.day, want.month, want.year2, want.dateFormat);
                    cmds.insert(cmds.end(), d.begin(), d.end());
                }
            }

            IOReturn r = kIOReturnSuccess;
            for (const UsbCmd& c : cmds) {
                r = sendOne(dev, c);
                if (r != kIOReturnSuccess) break;
                ++sent_;
            }
            if (r == kIOReturnSuccess) {
                sent = want;
                fresh = false;
                if (!cmds.empty()) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    lastError_.clear();
                }
            } else {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    lastError_ = ioError(r);
                }
                closeDevice(dev);
                dev = nullptr;
                connected_ = false;
            }
        }

        if (quitting) break;
    }
    closeDevice(dev);
    connected_ = false;
}

}  // namespace x52
