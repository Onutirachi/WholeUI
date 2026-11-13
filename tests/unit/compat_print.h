// Lets CHECK_EQ print compatibility levels and reasons when a check fails. Shared by the test
// files so every translation unit sees the same definitions.

#ifndef WHOLEUI_TESTS_COMPAT_PRINT_H
#define WHOLEUI_TESTS_COMPAT_PRINT_H

#include <ostream>

#include "../../src/compat/rules.h"

namespace wholeui::compat {

inline std::ostream& operator<<(std::ostream& os, Level level) {
    switch (level) {
        case Level::Native: return os << "Native";
        case Level::Visual: return os << "Visual";
        case Level::Full: return os << "Full";
    }
    return os << "Level(" << static_cast<int>(level) << ")";
}

inline std::ostream& operator<<(std::ostream& os, Reason reason) {
    switch (reason) {
        case Reason::Default: return os << "Default";
        case Reason::Rule: return os << "Rule";
        case Reason::Locked: return os << "Locked";
        case Reason::CappedByTopLevel: return os << "CappedByTopLevel";
        case Reason::LoweredByPage: return os << "LoweredByPage";
        case Reason::Failure: return os << "Failure";
    }
    return os << "Reason(" << static_cast<int>(reason) << ")";
}

}  // namespace wholeui::compat

#endif  // WHOLEUI_TESTS_COMPAT_PRINT_H
