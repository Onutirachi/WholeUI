// WholeUI: the root of the amalgamation. tools/build.py inlines each project include below,
// in this order, into build/wholeui.wh.cpp (research R4). Never edit that file by hand.
//
// Quoted includes are relative to the including file. A module's .cpp follows its .h.

#include "core/log.h"
#include "core/log.cpp"
#include "core/lock.h"
#include "core/subscriptions.h"
#include "core/com.h"
#include "core/worker.h"
#include "core/worker.cpp"
#include "compat/rules.h"
#include "compat/rules.cpp"
#include "settings/overrides.h"
#include "settings/overrides.cpp"
#include "settings/settings.h"
#include "settings/settings.cpp"
#include "compat/identity.h"
#include "compat/identity.cpp"
#include "compat/dialog_hooks.h"
#include "compat/engine.h"
#include "compat/engine.cpp"
#include "compat/dialog_hooks.cpp"
#include "tokens/tokens.h"
#include "tokens/tokens.cpp"
#include "tokens/system.h"
#include "tokens/system.cpp"
#include "tokens/runtime.h"
#include "tokens/runtime.cpp"
#include "diagnostic/format.h"
#include "diagnostic/format.cpp"
#include "diagnostic/overlay.h"
#include "diagnostic/overlay.cpp"

namespace {

// Reads the settings, hands the new rule set to the engine and turns diagnostic mode on or
// off (FR-020). On failure (out of memory) the engine keeps the rule set it has, which
// before the first success means every window stays Native (FR-018), and diagnostic mode
// keeps its current state.
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
        if (loaded.diagnosticMode) {
            diagnostic::Start();
        } else {
            diagnostic::Stop();
        }
    } catch (...) {
        log::Warn(L"settings", L"%s: reading settings failed: exception; rules unchanged", when);
    }
}

}  // namespace

BOOL Wh_ModInit() {
    wholeui::log::Info(L"core", L"WholeUI %s loaded", WH_MOD_VERSION);
    // A worker that does not start leaves the tokens on their defaults and the diagnostic
    // overlay off; window classification does not need it, so the mod still loads.
    wholeui::worker::Start();
    wholeui::tokens::system::Start(wholeui::tokens::OnSystemInputs, nullptr);
    wholeui::compat::InstallDialogHooks();
    ApplySettings(L"loaded");
    return TRUE;
}

void Wh_ModSettingsChanged() {
    ApplySettings(L"changed");
}

void Wh_ModUninit() {
    wholeui::compat::UninstallDialogHooks();
    // diagnostic::Stop and system::Stop post their release to the worker; worker::Stop runs
    // them and waits (FR-029).
    wholeui::diagnostic::Stop();
    wholeui::tokens::system::Stop();
    wholeui::worker::Stop();
    wholeui::tokens::ResetTokens();
    wholeui::compat::ResetEngine();
}
