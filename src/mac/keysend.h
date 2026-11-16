// Typing keyboard keys on behalf of a joystick button (macOS Quartz events).
#pragma once
#include "../core/keys.h"

namespace x52 {

// macOS only lets an app type keys once the user has allowed it under
// System Settings > Privacy & Security > Accessibility.
bool keysAllowed();
void requestKeyAccess();   // shows the system prompt (once)
void sendKey(const KeyCombo& k, bool down);

}  // namespace x52
