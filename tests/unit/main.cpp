// Runs every test registered with TEST(...) in the linked test files.

#include <cstdio>

#include "harness.h"

int main() {
    harness::State& state = harness::Current();
    int failedTests = 0;
    for (const harness::Test& test : harness::Registry()) {
        state.currentTest = test.name;
        state.currentFailed = false;
        test.fn();
        if (state.currentFailed) {
            failedTests++;
        }
    }
    int total = static_cast<int>(harness::Registry().size());
    std::printf("%d tests, %d passed, %d failed (%d failed checks)\n", total,
                total - failedTests, failedTests, state.failedChecks);
    return failedTests == 0 ? 0 : 1;
}
