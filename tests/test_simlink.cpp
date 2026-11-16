#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <thread>

#include "check.h"
#include "../src/mac/simlink.h"

using namespace x52;

TEST(rref_request_is_the_documented_413_bytes) {
    std::string p = buildRref(10, 7, "sim/x/y");
    CHECK_EQ(p.size(), (size_t)413);
    CHECK(p.compare(0, 5, std::string("RREF\0", 5)) == 0);
    int hz, id;
    memcpy(&hz, p.data() + 5, 4);
    memcpy(&id, p.data() + 9, 4);
    CHECK_EQ(hz, 10);
    CHECK_EQ(id, 7);
    CHECK_EQ(std::string(p.data() + 13), std::string("sim/x/y"));
}

TEST(rref_answer_is_split_into_id_and_value) {
    std::string a("RREF,", 5);
    int id = 3;
    float v = 0.75f;
    a.append((const char*)&id, 4);
    a.append((const char*)&v, 4);
    std::vector<std::pair<int, float>> out;
    CHECK(parseRref(a.data(), (int)a.size(), out));
    CHECK_EQ(out.size(), (size_t)1);
    CHECK_EQ(out[0].first, 3);
    CHECK(out[0].second == 0.75f);
    CHECK(!parseRref("XXXX,", 5, out));
}

// A made-up X-Plane on a spare port: answers every subscription once with a value.
TEST(link_subscribes_and_reads_the_answer_then_goes_quiet) {
    int srv = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = 0;
    CHECK(bind(srv, (sockaddr*)&a, sizeof a) == 0);
    socklen_t len = sizeof a;
    getsockname(srv, (sockaddr*)&a, &len);
    timeval tv{1, 0};
    setsockopt(srv, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    SimLink link(ntohs(a.sin_port));
    link.want({"sim/brake"}, 100.0);

    char buf[1024];
    sockaddr_in from{};
    socklen_t fl = sizeof from;
    ssize_t n = recvfrom(srv, buf, sizeof buf, 0, (sockaddr*)&from, &fl);
    CHECK_EQ(n, (ssize_t)413);
    int id;
    memcpy(&id, buf + 9, 4);
    std::string ans("RREF,", 5);
    float v = 1.0f;
    ans.append((const char*)&id, 4);
    ans.append((const char*)&v, 4);
    sendto(srv, ans.data(), ans.size(), 0, (sockaddr*)&from, fl);

    double got = -1;
    for (int i = 0; i < 50 && !link.value("sim/brake", 100.0, got); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        link.poll(100.0);
    }
    CHECK(link.value("sim/brake", 100.5, got));
    CHECK(got == 1.0);
    CHECK(!link.value("sim/brake", 103.0, got));   // silent for 3 s: X-Plane is gone
    CHECK(!link.value("sim/other", 100.0, got));
    close(srv);
}

int main() { return runAllTests(); }
