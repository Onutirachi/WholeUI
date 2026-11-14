// Minimal unit test harness (research R5): no dependency beyond the standard library.
//
//   TEST(name) { CHECK(expr); CHECK_EQ(actual, expected); }
//
// A failed check prints file, line, test name and the expression (and both values for
// CHECK_EQ when they can be printed), then the test continues. tests/unit/main.cpp runs
// every registered test and exits non-zero if any check failed.

#ifndef WHOLEUI_TESTS_HARNESS_H
#define WHOLEUI_TESTS_HARNESS_H

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace harness {

struct Test {
    const char* name;
    void (*fn)();
};

inline std::vector<Test>& Registry() {
    static std::vector<Test> tests;
    return tests;
}

struct State {
    const char* currentTest = "";
    int failedChecks = 0;
    bool currentFailed = false;
};

inline State& Current() {
    static State state;
    return state;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) { Registry().push_back(Test{name, fn}); }
};

inline void Fail(const char* file, int line, const std::string& message) {
    State& s = Current();
    s.failedChecks++;
    s.currentFailed = true;
    std::fprintf(stderr, "%s:%d: FAILED in %s: %s\n", file, line, s.currentTest,
                 message.c_str());
}

template <typename T>
std::string Describe(const T& value) {
    if constexpr (requires(std::ostream& os) { os << value; }) {
        std::ostringstream os;
        os << value;
        return os.str();
    } else {
        return "(not printable)";
    }
}

template <typename A, typename B>
void CheckEq(const A& actual, const B& expected, const char* actualText,
             const char* expectedText, const char* file, int line) {
    if (!(actual == expected)) {
        Fail(file, line,
             std::string("CHECK_EQ(") + actualText + ", " + expectedText + "): " +
                 Describe(actual) + " != " + Describe(expected));
    }
}

}  // namespace harness

#define HARNESS_CONCAT_(a, b) a##b
#define HARNESS_CONCAT(a, b) HARNESS_CONCAT_(a, b)

#define TEST(name)                                                              \
    static void HARNESS_CONCAT(test_, name)();                                  \
    static const ::harness::Registrar HARNESS_CONCAT(registrar_, name)(         \
        #name, &HARNESS_CONCAT(test_, name));                                   \
    static void HARNESS_CONCAT(test_, name)()

#define CHECK(expr)                                                             \
    do {                                                                        \
        if (!(expr)) {                                                          \
            ::harness::Fail(__FILE__, __LINE__, "CHECK(" #expr ")");            \
        }                                                                       \
    } while (0)

#define CHECK_EQ(actual, expected) \
    ::harness::CheckEq((actual), (expected), #actual, #expected, __FILE__, __LINE__)

#endif  // WHOLEUI_TESTS_HARNESS_H
