// The mod's settings (data-model.md "Settings", contracts/settings.md): the compatibility
// overrides (S6) and diagnostic mode (S7), read through Windhawk.

#ifndef WHOLEUI_SETTINGS_SETTINGS_H
#define WHOLEUI_SETTINGS_SETTINGS_H

#include "../compat/rules.h"

namespace wholeui::settings {

struct Settings {
    compat::RuleSet rules;  // built-ins + valid overrides, with a version no earlier set had
    bool diagnosticMode = false;
};

// Reads every setting and builds a new rule set. Invalid overrides are skipped and logged
// with log::Warn (FR-017); an override naming a shell surface is kept and logged as having
// no effect (FR-015).
Settings Load();

}  // namespace wholeui::settings

#endif  // WHOLEUI_SETTINGS_SETTINGS_H
