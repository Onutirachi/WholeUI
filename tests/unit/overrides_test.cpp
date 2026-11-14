// Tests for the override parser in src/settings/overrides.h against contracts/settings.md
// (FR-015, FR-017, SC-007).

#include "../../src/settings/overrides.h"

#include <optional>
#include <string>
#include <vector>

#include "compat_print.h"
#include "harness.h"

using wholeui::compat::Level;
using wholeui::compat::Rule;
using wholeui::compat::RuleSource;
using wholeui::settings::NamesShellSurface;
using wholeui::settings::OverrideResult;
using wholeui::settings::ParseOverride;

namespace {

// Narrow copy of an ASCII wide string, so CHECK_EQ can print it.
std::string Narrow(const std::wstring& text) {
    std::string out;
    for (wchar_t c : text) {
        out.push_back(c < 128 ? static_cast<char>(c) : '?');
    }
    return out;
}

std::string Value(const std::optional<std::wstring>& field) {
    return field ? Narrow(*field) : std::string("(not named)");
}

// Parses text that must be valid and returns its rule (an empty rule when it is not).
Rule Valid(const wchar_t* text) {
    OverrideResult result = ParseOverride(0, text);
    CHECK(result.rule.has_value());
    CHECK(!result.error.has_value());
    return result.rule.value_or(Rule{});
}

// Parses text that must be invalid and returns the reason (empty when it is valid).
std::string Reason(const wchar_t* text) {
    OverrideResult result = ParseOverride(3, text);
    CHECK(!result.rule.has_value());
    CHECK(result.error.has_value());
    if (!result.error) {
        return "";
    }
    CHECK_EQ(result.error->index, 3);
    CHECK(result.error->text == text);
    return Narrow(result.error->reason);
}

}  // namespace

// --- Valid entries -------------------------------------------------------------------------

TEST(overrides_example_process_class) {
    Rule rule = Valid(L"process=explorer.exe; class=CabinetWClass; level=visual");
    CHECK_EQ(Value(rule.process), "explorer.exe");
    CHECK_EQ(Value(rule.module), "(not named)");
    CHECK_EQ(Value(rule.windowClass), "CabinetWClass");
    CHECK_EQ(Value(rule.templateId), "(not named)");
    CHECK_EQ(rule.level, Level::Visual);
    CHECK(rule.source == RuleSource::Override);
    CHECK(!rule.locked);
    CHECK_EQ(Narrow(rule.origin), "process=explorer.exe; class=CabinetWClass; level=visual");
}

TEST(overrides_example_module) {
    Rule rule = Valid(L"module=sysdm.cpl; level=native");
    CHECK_EQ(Value(rule.process), "(not named)");
    CHECK_EQ(Value(rule.module), "sysdm.cpl");
    CHECK_EQ(rule.level, Level::Native);
}

TEST(overrides_example_all_fields) {
    Rule rule = Valid(L"module=shell32.dll; class=#32770; template=1040; level=full");
    CHECK_EQ(Value(rule.module), "shell32.dll");
    CHECK_EQ(Value(rule.windowClass), "#32770");
    CHECK_EQ(Value(rule.templateId), "1040");
    CHECK_EQ(rule.level, Level::Full);
}

TEST(overrides_whitespace_is_trimmed) {
    Rule rule = Valid(L"  process =  explorer.exe ;class= CabinetWClass\t;\tlevel = full  ");
    CHECK_EQ(Value(rule.process), "explorer.exe");
    CHECK_EQ(Value(rule.windowClass), "CabinetWClass");
    CHECK_EQ(rule.level, Level::Full);
    // The origin is the entry as typed, without the surrounding whitespace.
    CHECK_EQ(Narrow(rule.origin), "process =  explorer.exe ;class= CabinetWClass\t;\tlevel = full");
}

TEST(overrides_level_and_names_are_case_insensitive) {
    CHECK_EQ(Valid(L"class=A; level=VISUAL").level, Level::Visual);
    CHECK_EQ(Valid(L"class=A; level=Full").level, Level::Full);
    CHECK_EQ(Valid(L"class=A; level=nAtIvE").level, Level::Native);
    Rule rule = Valid(L"Process=explorer.exe; CLASS=CabinetWClass; Level=full");
    CHECK_EQ(Value(rule.process), "explorer.exe");
    CHECK_EQ(Value(rule.windowClass), "CabinetWClass");
}

TEST(overrides_field_order_does_not_matter) {
    Rule rule = Valid(L"level=full; template=PropPage; process=explorer.exe");
    CHECK_EQ(Value(rule.process), "explorer.exe");
    CHECK_EQ(Value(rule.templateId), "PropPage");
    CHECK_EQ(rule.level, Level::Full);
}

TEST(overrides_value_may_contain_equals_sign) {
    // Only the first "=" separates name and value.
    CHECK_EQ(Value(Valid(L"class=A=B; level=full").windowClass), "A=B");
}

TEST(overrides_trailing_semicolon_is_accepted) {
    CHECK_EQ(Valid(L"class=A; level=full;").level, Level::Full);
    CHECK_EQ(Valid(L"class=A; level=full ;  ").level, Level::Full);
}

// --- Skipped entries -----------------------------------------------------------------------

TEST(overrides_empty_entry_is_skipped_silently) {
    for (const wchar_t* text : {L"", L"   ", L"\t"}) {
        OverrideResult result = ParseOverride(0, text);
        CHECK(!result.rule.has_value());
        CHECK(!result.error.has_value());
    }
}

// --- Errors --------------------------------------------------------------------------------

TEST(overrides_error_missing_level) {
    CHECK_EQ(Reason(L"process=explorer.exe"), "missing level");
}

TEST(overrides_error_unknown_level) {
    CHECK_EQ(Reason(L"process=explorer.exe; level=dark"), "unknown level");
}

TEST(overrides_error_no_identity_field) {
    CHECK_EQ(Reason(L"level=full"), "no identity field");
}

TEST(overrides_error_unknown_field) {
    CHECK_EQ(Reason(L"title=Explorer; level=full"), "unknown field \"title\"");
}

TEST(overrides_error_duplicate_field) {
    CHECK_EQ(Reason(L"class=A; class=B; level=full"), "duplicate field \"class\"");
    CHECK_EQ(Reason(L"class=A; level=full; level=native"), "duplicate field \"level\"");
    // Names differing only in case are the same field.
    CHECK_EQ(Reason(L"class=A; Class=B; level=full"), "duplicate field \"Class\"");
}

TEST(overrides_error_empty_value) {
    CHECK_EQ(Reason(L"class=; level=full"), "empty value for \"class\"");
    CHECK_EQ(Reason(L"class=   ; level=full"), "empty value for \"class\"");
    CHECK_EQ(Reason(L"class=A; level="), "empty value for \"level\"");
}

TEST(overrides_error_path_not_allowed) {
    CHECK_EQ(Reason(L"module=C:\\Windows\\shell32.dll; level=full"),
             "path not allowed in \"module\"");
    CHECK_EQ(Reason(L"process=bin/app.exe; level=full"), "path not allowed in \"process\"");
    CHECK_EQ(Reason(L"process=..\\app.exe; level=full"), "path not allowed in \"process\"");
    // Class and template values may contain these characters.
    CHECK_EQ(Value(Valid(L"class=Afx:400000:8; level=full").windowClass), "Afx:400000:8");
}

TEST(overrides_error_malformed_field) {
    CHECK_EQ(Reason(L"explorer.exe; level=full"), "malformed field \"explorer.exe\"");
    CHECK_EQ(Reason(L"class=A; =full"), "malformed field \"=full\"");
}

TEST(overrides_first_error_in_entry_is_reported) {
    // Field errors are found left to right, before the checks on the whole entry.
    CHECK_EQ(Reason(L"title=X; class=; level=dark"), "unknown field \"title\"");
    CHECK_EQ(Reason(L"class=; title=X"), "empty value for \"class\"");
    // With neither an identity field nor a level, the missing level is reported.
    CHECK_EQ(Reason(L";"), "missing level");
}

TEST(overrides_one_bad_entry_does_not_drop_the_others) {
    std::vector<const wchar_t*> entries = {
        L"process=explorer.exe; class=CabinetWClass; level=visual",
        L"title=Explorer; level=full",
        L"",
        L"module=sysdm.cpl; level=native",
    };
    int rules = 0, errors = 0, skipped = 0;
    for (size_t i = 0; i < entries.size(); i++) {
        OverrideResult result = ParseOverride(static_cast<int>(i), entries[i]);
        if (result.rule) rules++;
        if (result.error) {
            errors++;
            CHECK_EQ(result.error->index, 1);
        }
        if (!result.rule && !result.error) skipped++;
    }
    CHECK_EQ(rules, 2);
    CHECK_EQ(errors, 1);
    CHECK_EQ(skipped, 1);
}

// --- Overrides that name a shell surface (FR-015) ------------------------------------------

namespace {

std::vector<Rule> LockedTaskbar() {
    Rule taskbar;
    taskbar.process = L"explorer.exe";
    taskbar.windowClass = L"Shell_TrayWnd";
    taskbar.level = Level::Native;
    taskbar.source = RuleSource::BuiltIn;
    taskbar.locked = true;
    taskbar.origin = L"Taskbar";

    Rule explorer;  // a built-in that is not locked
    explorer.process = L"explorer.exe";
    explorer.windowClass = L"CabinetWClass";
    explorer.level = Level::Native;
    explorer.source = RuleSource::BuiltIn;
    explorer.origin = L"File Explorer";
    return {taskbar, explorer};
}

}  // namespace

TEST(overrides_naming_a_shell_surface_are_flagged) {
    std::vector<Rule> builtIns = LockedTaskbar();
    // Names every field of the locked rule (any case), with or without more fields.
    CHECK(NamesShellSurface(Valid(L"process=explorer.exe; class=Shell_TrayWnd; level=full"),
                            builtIns));
    CHECK(NamesShellSurface(
        Valid(L"process=EXPLORER.EXE; module=explorer.exe; class=shell_traywnd; level=visual"),
        builtIns));
}

TEST(overrides_not_naming_a_shell_surface_are_not_flagged) {
    std::vector<Rule> builtIns = LockedTaskbar();
    // Does not name the process: it may match the class in another process.
    CHECK(!NamesShellSurface(Valid(L"class=Shell_TrayWnd; level=full"), builtIns));
    // Another process.
    CHECK(!NamesShellSurface(Valid(L"process=other.exe; class=Shell_TrayWnd; level=full"),
                             builtIns));
    // Matches a built-in that is not locked.
    CHECK(!NamesShellSurface(Valid(L"process=explorer.exe; class=CabinetWClass; level=full"),
                             builtIns));
}
