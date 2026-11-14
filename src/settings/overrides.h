// PURE: C++ standard library only (compiled into the mod and the unit tests).
//
// Parser for one compatibility override, the string syntax in contracts/settings.md
// "Override syntax" (research R10, FR-016, FR-017):
//
//     process=explorer.exe; class=CabinetWClass; level=visual

#ifndef WHOLEUI_SETTINGS_OVERRIDES_H
#define WHOLEUI_SETTINGS_OVERRIDES_H

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "../compat/rules.h"

namespace wholeui::settings {

struct OverrideError {
    int index;           // position in the overrides array
    std::wstring text;   // the entry, outer whitespace trimmed
    std::wstring reason; // one of the reasons in contracts/settings.md "Errors"
};

// A rule, an error, or neither: an empty or whitespace-only entry is skipped silently.
struct OverrideResult {
    std::optional<compat::Rule> rule;
    std::optional<OverrideError> error;
};

// Parses entry `index` of the overrides array. A valid entry gives an Override rule whose
// origin is the entry text.
OverrideResult ParseOverride(int index, std::wstring_view text);

// True when `candidate` names every field of a locked built-in rule with equal values
// (case-insensitive): it names a shell surface and has no effect (FR-015). An override naming
// only part of a locked rule (for example only the class) is not flagged, since it may match
// windows in other processes.
bool NamesShellSurface(const compat::Rule& candidate, std::span<const compat::Rule> builtIns);

}  // namespace wholeui::settings

#endif  // WHOLEUI_SETTINGS_OVERRIDES_H
