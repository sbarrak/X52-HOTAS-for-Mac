// Update check, the part that needs no network: comparing version numbers. The app fetches the
// latest release from GitHub and asks here whether it is newer than the running version. Pure C++.
#pragma once
#include <string>

namespace x52 {

// "1.2.3", "v1.2.3" or "1.2" (a missing part is 0). Anything after the numbers ("-beta") is ignored.
// False when there is no number at all ("dev", "").
bool parseVersion(const std::string& text, int out[3]);

// True when `latest` is a higher x.y.z than `current` (every number counts, the third too). False when it is
// equal or lower, or when either version cannot be read (a development build never says an update is waiting).
bool isNewerVersion(const std::string& current, const std::string& latest);

}  // namespace x52
