#include "keysend.h"

#include <ApplicationServices/ApplicationServices.h>

namespace x52 {

bool keysAllowed() { return CGPreflightPostEventAccess(); }

void requestKeyAccess() { CGRequestPostEventAccess(); }

void sendKey(const KeyCombo& k, bool down) {
    if (k.keycode < 0) return;
    CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    CGEventRef ev = CGEventCreateKeyboardEvent(src, (CGKeyCode)k.keycode, down);
    if (ev) {
        CGEventFlags flags = 0;
        if (k.shift) flags |= kCGEventFlagMaskShift;
        if (k.ctrl) flags |= kCGEventFlagMaskControl;
        if (k.alt) flags |= kCGEventFlagMaskAlternate;
        if (k.cmd) flags |= kCGEventFlagMaskCommand;
        CGEventSetFlags(ev, flags);
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
    }
    if (src) CFRelease(src);
}

}  // namespace x52
