#pragma once
// Minimal self-contained test harness (no external dependencies).

#include <cstdio>

namespace testfw {

using TestFn = void (*)();

struct TestCase {
    const char* name;
    TestFn fn;
};

inline TestCase* registry() {
    static TestCase tests[256];
    return tests;
}
inline int& registryCount() {
    static int count = 0;
    return count;
}
inline int& failures() {
    static int f = 0;
    return f;
}
inline int& checks() {
    static int c = 0;
    return c;
}
inline bool& currentFailed() {
    static bool b = false;
    return b;
}

struct Registrar {
    Registrar(const char* name, TestFn fn) {
        if (registryCount() < 256) registry()[registryCount()++] = {name, fn};
    }
};

inline void report(bool ok, const char* expr, const char* file, int line) {
    ++checks();
    if (!ok) {
        ++failures();
        currentFailed() = true;
        std::printf("    FAILED: %s  (%s:%d)\n", expr, file, line);
    }
}

inline int runAll() {
    int passed = 0;
    std::printf("Dungeonbound test suite: %d tests\n\n", registryCount());
    for (int i = 0; i < registryCount(); ++i) {
        currentFailed() = false;
        try {
            registry()[i].fn();
        } catch (...) {
            ++failures();
            currentFailed() = true;
            std::printf("    FAILED: unexpected exception\n");
        }
        std::printf("  [%s] %s\n", currentFailed() ? "FAIL" : " OK ", registry()[i].name);
        if (!currentFailed()) ++passed;
    }
    std::printf("\n%d/%d tests passed, %d checks, %d failed checks\n", passed, registryCount(), checks(), failures());
    return failures() == 0 ? 0 : 1;
}

}  // namespace testfw

#define TF_CONCAT2(a, b) a##b
#define TF_CONCAT(a, b) TF_CONCAT2(a, b)

#define TEST(name)                                                              \
    static void name();                                                         \
    static testfw::Registrar TF_CONCAT(registrar_, name)(#name, &name);         \
    static void name()

#define CHECK(expr) testfw::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

#define CHECK_THROWS(expr)                                                      \
    do {                                                                        \
        bool threw_ = false;                                                    \
        try {                                                                   \
            (void)(expr);                                                       \
        } catch (...) {                                                         \
            threw_ = true;                                                      \
        }                                                                       \
        testfw::report(threw_, "throws: " #expr, __FILE__, __LINE__);           \
    } while (0)
