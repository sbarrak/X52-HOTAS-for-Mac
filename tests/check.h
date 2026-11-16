// Minimal test harness: TEST(name) { CHECK(...); CHECK_EQ(a, b); }
#pragma once
#include <cstdio>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

struct TestCase {
    const char* name;
    std::function<void()> fn;
};
inline std::vector<TestCase>& allTests() {
    static std::vector<TestCase> t;
    return t;
}
inline int& failCount() {
    static int n = 0;
    return n;
}
struct TestReg {
    TestReg(const char* n, std::function<void()> f) { allTests().push_back({n, f}); }
};

#define TEST(name)                                   \
    static void test_##name();                       \
    static TestReg reg_##name(#name, test_##name);   \
    static void test_##name()

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            ++failCount();                                                 \
            std::cout << "    FAIL " << __FILE__ << ":" << __LINE__        \
                      << "  " #cond "\n";                                  \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b)                                                     \
    do {                                                                   \
        auto va_ = (a);                                                    \
        auto vb_ = (b);                                                    \
        if (!(va_ == vb_)) {                                               \
            ++failCount();                                                 \
            std::cout << "    FAIL " << __FILE__ << ":" << __LINE__        \
                      << "  " #a " == " #b "\n      got:      [" << va_    \
                      << "]\n      expected: [" << vb_ << "]\n";           \
        }                                                                  \
    } while (0)

inline int runAllTests() {
    int failedTests = 0;
    for (auto& t : allTests()) {
        int before = failCount();
        t.fn();
        bool ok = failCount() == before;
        if (!ok) ++failedTests;
        std::cout << (ok ? "  ok    " : "  FAIL  ") << t.name << "\n";
    }
    std::cout << allTests().size() - failedTests << "/" << allTests().size() << " tests passed\n";
    return failedTests ? 1 : 0;
}
