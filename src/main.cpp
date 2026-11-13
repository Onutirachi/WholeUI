// WholeUI: the root of the amalgamation. tools/build.py inlines each project include below,
// in this order, into build/wholeui.wh.cpp (research R4). Never edit that file by hand.
//
// Quoted includes are relative to the including file. A module's .cpp follows its .h.

#include "core/log.h"
#include "core/log.cpp"
#include "core/subscriptions.h"
#include "compat/rules.h"
#include "compat/rules.cpp"
#include "settings/overrides.h"
#include "settings/overrides.cpp"
#include "settings/settings.h"
#include "settings/settings.cpp"
#include "compat/identity.h"
#include "compat/identity.cpp"
#include "compat/engine.h"
#include "compat/engine.cpp"

namespace {

// Reads the settings and hands the new rule set to the engine. On failure (out of memory)
// the engine keeps the rule set it has, which before the first success means every window
// stays Native (FR-018).
void ApplySettings(const wchar_t* when) {
    using namespace wholeui;
    try {
        settings::Settings loaded = settings::Load();
        log::SetVerbose(loaded.diagnosticMode);
        log::Info(L"settings", L"%s: %u built-in rules, %u overrides, diagnostic mode %s", when,
                  static_cast<unsigned>(loaded.rules.builtIns.size()),
                  static_cast<unsigned>(loaded.rules.overrides.size()),
                  loaded.diagnosticMode ? L"on" : L"off");
        compat::SetRuleSet(std::move(loaded.rules));
    } catch (...) {
        log::Warn(L"settings", L"%s: reading settings failed: exception; rules unchanged", when);
    }
}

}  // namespace

BOOL Wh_ModInit() {
    wholeui::log::Info(L"core", L"WholeUI %s loaded", WH_MOD_VERSION);
    ApplySettings(L"loaded");
    return TRUE;
}

void Wh_ModSettingsChanged() {
    ApplySettings(L"changed");
}

void Wh_ModUninit() {
    wholeui::compat::ResetEngine();
}
