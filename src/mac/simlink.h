// Reads datarefs from a running X-Plane over its built-in UDP interface (RREF), so the
// menu bar app can drive live lights without the plugin. Plain POSIX sockets, no X-Plane code.
#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace x52 {

constexpr int kXPlaneUdpPort = 49000;

// The RREF request for one dataref: "RREF\0", frequency (Hz, 0 = stop), id, 400-byte name.
std::string buildRref(int hz, int id, const std::string& dataref);
// X-Plane's answer: "RREF," followed by (id, float) pairs. False if it is not one.
bool parseRref(const char* data, int size, std::vector<std::pair<int, float>>& out);

class SimLink {
public:
    explicit SimLink(int port = kXPlaneUdpPort);   // talks to 127.0.0.1:port
    ~SimLink();

    // The datarefs we want. Subscribes new ones, unsubscribes dropped ones, and repeats
    // the request every few seconds so a restarted X-Plane picks it up again.
    void want(const std::vector<std::string>& datarefs, double now);
    void poll(double now);   // reads whatever X-Plane has sent
    // Latest value, false if never answered or silent for more than 2 s (X-Plane not running).
    bool value(const std::string& dataref, double now, double& out) const;
    void stop();             // unsubscribes everything

private:
    struct Item {
        int id = 0;
        float value = 0;
        double heard = -1e9;
    };
    void send(int hz, int id, const std::string& ref);
    int fd_ = -1;
    int port_;
    int nextId_ = 1;
    double lastRequest_ = -1e9;
    std::map<std::string, Item> items_;
};

}  // namespace x52
