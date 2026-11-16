#include "simlink.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

namespace x52 {

std::string buildRref(int hz, int id, const std::string& dataref) {
    std::string s("RREF\0", 5);
    s.append((const char*)&hz, 4);
    s.append((const char*)&id, 4);
    std::string name = dataref.substr(0, 399);
    name.resize(400, '\0');
    return s + name;
}

bool parseRref(const char* data, int size, std::vector<std::pair<int, float>>& out) {
    out.clear();
    if (size < 5 || memcmp(data, "RREF,", 5) != 0) return false;
    for (int i = 5; i + 8 <= size; i += 8) {
        int id;
        float v;
        memcpy(&id, data + i, 4);
        memcpy(&v, data + i + 4, 4);
        out.push_back({id, v});
    }
    return true;
}

SimLink::SimLink(int port) : port_(port) {
    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ >= 0) fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL, 0) | O_NONBLOCK);
}

SimLink::~SimLink() {
    stop();
    if (fd_ >= 0) close(fd_);
}

void SimLink::send(int hz, int id, const std::string& ref) {
    if (fd_ < 0) return;
    sockaddr_in to;
    memset(&to, 0, sizeof to);
    to.sin_family = AF_INET;
    to.sin_port = htons((uint16_t)port_);
    inet_pton(AF_INET, "127.0.0.1", &to.sin_addr);
    std::string p = buildRref(hz, id, ref);
    sendto(fd_, p.data(), p.size(), 0, (sockaddr*)&to, sizeof to);
}

void SimLink::want(const std::vector<std::string>& refs, double now) {
    for (auto it = items_.begin(); it != items_.end();) {
        bool keep = false;
        for (const std::string& r : refs) keep |= r == it->first;
        if (!keep) {
            send(0, it->second.id, it->first);
            it = items_.erase(it);
        } else {
            ++it;
        }
    }
    bool repeat = now - lastRequest_ >= 5.0;
    for (const std::string& r : refs) {
        auto it = items_.find(r);
        bool fresh = it == items_.end();
        if (fresh) {
            Item n;
            n.id = nextId_++;
            it = items_.emplace(r, n).first;
        }
        if (fresh || repeat) send(10, it->second.id, r);
    }
    if (repeat) lastRequest_ = now;
}

void SimLink::poll(double now) {
    if (fd_ < 0) return;
    char buf[2048];
    std::vector<std::pair<int, float>> got;
    for (;;) {
        ssize_t n = recv(fd_, buf, sizeof buf, 0);
        if (n <= 0) break;
        if (!parseRref(buf, (int)n, got)) continue;
        for (auto& g : got)
            for (auto& kv : items_)
                if (kv.second.id == g.first) {
                    kv.second.value = g.second;
                    kv.second.heard = now;
                }
    }
}

bool SimLink::value(const std::string& ref, double now, double& out) const {
    auto it = items_.find(ref);
    if (it == items_.end() || now - it->second.heard > 2.0) return false;
    out = it->second.value;
    return true;
}

void SimLink::stop() {
    for (auto& kv : items_) send(0, kv.second.id, kv.first);
    items_.clear();
}

}  // namespace x52
