#include "update.h"
#include <cctype>

namespace x52 {

bool parseVersion(const std::string& text, int out[3]) {
    out[0] = out[1] = out[2] = 0;
    size_t i = 0;
    if (i < text.size() && (text[i] == 'v' || text[i] == 'V')) ++i;
    int part = 0;
    bool any = false;
    while (part < 3) {
        if (i >= text.size() || !isdigit((unsigned char)text[i])) break;
        long n = 0;
        while (i < text.size() && isdigit((unsigned char)text[i])) {
            n = n * 10 + (text[i] - '0');
            if (n > 1000000) n = 1000000;
            ++i;
        }
        out[part++] = (int)n;
        any = true;
        if (i < text.size() && text[i] == '.') ++i;
        else break;
    }
    return any;
}

bool isNewerVersion(const std::string& current, const std::string& latest) {
    int a[3], b[3];
    if (!parseVersion(current, a) || !parseVersion(latest, b)) return false;
    // Only the first two numbers count: 1.2.3 -> 1.2.40 is a small change that nobody is asked to download.
    for (int i = 0; i < 2; ++i)
        if (a[i] != b[i]) return b[i] > a[i];
    return false;
}

}  // namespace x52
