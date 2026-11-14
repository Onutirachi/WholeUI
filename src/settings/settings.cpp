#include "settings.h"

#include <windows.h>

#include <atomic>
#include <string>

#include "../core/log.h"
#include "overrides.h"

namespace wholeui::settings {

namespace {

// Windhawk returns "" both for an empty item and past the end of the array, so reading
// stops only after this many empty results in a row (contracts/settings.md).
constexpr int kMaxEmptyRun = 16;

std::atomic<std::uint64_t> g_lastVersion{0};

std::wstring ReadOverride(int index) {
    PCWSTR value = Wh_GetStringSetting(L"overrides[%d]", index);
    std::wstring text = value ? value : L"";
    Wh_FreeStringSetting(value);
    return text;
}

}  // namespace

Settings Load() {
    Settings settings;
    settings.rules.builtIns = compat::BuiltInRules();
    settings.rules.version = ++g_lastVersion;

    int emptyRun = 0;
    for (int index = 0; emptyRun < kMaxEmptyRun; index++) {
        std::wstring text = ReadOverride(index);
        OverrideResult result = ParseOverride(index, text);
        if (!result.rule && !result.error) {
            emptyRun++;
            continue;
        }
        emptyRun = 0;
        if (result.error) {
            log::Warn(L"settings", L"override %d ignored: %s: \"%s\"", result.error->index,
                      result.error->reason.c_str(), result.error->text.c_str());
            continue;
        }
        if (NamesShellSurface(*result.rule, settings.rules.builtIns)) {
            log::Warn(L"settings", L"override %d has no effect: shell surface", index);
        }
        settings.rules.overrides.push_back(std::move(*result.rule));
    }

    settings.diagnosticMode = Wh_GetIntSetting(L"diagnosticMode") != 0;
    return settings;
}

}  // namespace wholeui::settings
