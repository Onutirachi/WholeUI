// Tests for the pure classifier in src/compat/rules.h against data-model.md "Resolution order"
// (FR-011 to FR-013a, FR-015, FR-018, SC-007) and the SC-004 timing budget.

#include "../../src/compat/rules.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "compat_print.h"
#include "harness.h"

using wholeui::compat::BuiltInRules;
using wholeui::compat::Classification;
using wholeui::compat::Classify;
using wholeui::compat::CopyOf;
using wholeui::compat::Field;
using wholeui::compat::Level;
using wholeui::compat::Reason;
using wholeui::compat::Rule;
using wholeui::compat::RuleSet;
using wholeui::compat::RuleSource;
using wholeui::compat::TemplateField;
using wholeui::compat::WindowClassification;
using wholeui::compat::WindowIdentity;

namespace {

// A window identity with every field known; tests change fields as needed.
WindowIdentity Window(const wchar_t* process, const wchar_t* module, const wchar_t* windowClass,
                      TemplateField templateId = TemplateField::NotADialog()) {
    WindowIdentity id;
    id.process = Field::Of(process);
    id.module = Field::Of(module);
    id.windowClass = Field::Of(windowClass);
    id.templateId = templateId;
    return id;
}

const WindowIdentity kExplorer = Window(L"explorer.exe", L"explorerframe.dll", L"CabinetWClass");

struct RuleFields {
    const wchar_t* process = nullptr;
    const wchar_t* module = nullptr;
    const wchar_t* windowClass = nullptr;
    const wchar_t* templateId = nullptr;
};

Rule MakeRule(RuleFields fields, Level level, RuleSource source, bool locked = false) {
    Rule rule;
    if (fields.process) rule.process = fields.process;
    if (fields.module) rule.module = fields.module;
    if (fields.windowClass) rule.windowClass = fields.windowClass;
    if (fields.templateId) rule.templateId = fields.templateId;
    rule.level = level;
    rule.source = source;
    rule.locked = locked;
    rule.origin = source == RuleSource::BuiltIn ? L"built-in" : L"override";
    return rule;
}

Rule BuiltIn(RuleFields fields, Level level) {
    return MakeRule(fields, level, RuleSource::BuiltIn);
}

Rule Override(RuleFields fields, Level level) {
    return MakeRule(fields, level, RuleSource::Override);
}

Rule Locked(RuleFields fields) {
    return MakeRule(fields, Level::Native, RuleSource::BuiltIn, /*locked=*/true);
}

RuleSet Rules(std::vector<Rule> builtIns, std::vector<Rule> overrides = {}) {
    RuleSet set;
    set.builtIns = std::move(builtIns);
    set.overrides = std::move(overrides);
    set.version = 1;
    return set;
}

// Classifies a top-level window with no pages.
Classification TopLevel(const WindowIdentity& window, const RuleSet& rules) {
    return Classify(window, {}, nullptr, rules);
}

}  // namespace

// --- Step 3: own level ---------------------------------------------------------------------

TEST(rules_no_match_is_native_default) {
    RuleSet rules = Rules({BuiltIn({.windowClass = L"SomethingElse"}, Level::Full)});
    Classification c = TopLevel(kExplorer, rules);
    CHECK_EQ(c.level, Level::Native);
    CHECK_EQ(c.ownLevel, Level::Native);
    CHECK_EQ(c.reason, Reason::Default);
    CHECK(c.rule == nullptr);
}

TEST(rules_empty_rule_set_is_native_default) {
    Classification c = TopLevel(kExplorer, Rules({}));
    CHECK_EQ(c.level, Level::Native);
    CHECK_EQ(c.reason, Reason::Default);
}

TEST(rules_single_match_gives_its_level) {
    RuleSet rules = Rules({BuiltIn({.windowClass = L"CabinetWClass"}, Level::Full)});
    Classification c = TopLevel(kExplorer, rules);
    CHECK_EQ(c.level, Level::Full);
    CHECK_EQ(c.ownLevel, Level::Full);
    CHECK_EQ(c.reason, Reason::Rule);
    CHECK(c.rule == &rules.builtIns[0]);
}

TEST(rules_every_named_field_must_match) {
    // Process matches, class does not: the rule does not apply.
    RuleSet rules = Rules(
        {BuiltIn({.process = L"explorer.exe", .windowClass = L"Other"}, Level::Full)});
    CHECK_EQ(TopLevel(kExplorer, rules).reason, Reason::Default);
}

TEST(rules_more_named_fields_wins) {
    RuleSet rules = Rules({
        BuiltIn({.process = L"explorer.exe"}, Level::Visual),
        BuiltIn({.process = L"explorer.exe", .windowClass = L"CabinetWClass"}, Level::Full),
        BuiltIn({.module = L"explorerframe.dll"}, Level::Native),
    });
    Classification c = TopLevel(kExplorer, rules);
    CHECK_EQ(c.level, Level::Full);
    CHECK(c.rule == &rules.builtIns[1]);
}

TEST(rules_more_named_fields_wins_over_override) {
    // A less specific override does not beat a more specific built-in.
    RuleSet rules = Rules(
        {BuiltIn({.process = L"explorer.exe", .windowClass = L"CabinetWClass"}, Level::Visual)},
        {Override({.process = L"explorer.exe"}, Level::Full)});
    Classification c = TopLevel(kExplorer, rules);
    CHECK_EQ(c.level, Level::Visual);
    CHECK(c.rule == &rules.builtIns[0]);
}

TEST(rules_equal_specificity_override_beats_builtin) {
    RuleSet rules = Rules({BuiltIn({.windowClass = L"CabinetWClass"}, Level::Native)},
                          {Override({.windowClass = L"CabinetWClass"}, Level::Full)});
    Classification c = TopLevel(kExplorer, rules);
    CHECK_EQ(c.level, Level::Full);
    CHECK_EQ(c.reason, Reason::Rule);
    CHECK(c.rule == &rules.overrides[0]);

    // An override may also lower a built-in.
    RuleSet lowering = Rules({BuiltIn({.windowClass = L"CabinetWClass"}, Level::Full)},
                             {Override({.process = L"explorer.exe"}, Level::Native)});
    CHECK_EQ(TopLevel(kExplorer, lowering).level, Level::Native);
}

TEST(rules_remaining_tie_resolves_to_lower_level) {
    // Two built-ins of equal specificity.
    RuleSet builtIns = Rules({
        BuiltIn({.windowClass = L"CabinetWClass"}, Level::Full),
        BuiltIn({.process = L"explorer.exe"}, Level::Visual),
    });
    Classification c = TopLevel(kExplorer, builtIns);
    CHECK_EQ(c.level, Level::Visual);
    CHECK(c.rule == &builtIns.builtIns[1]);

    // Two overrides of equal specificity, in either order.
    RuleSet overrides = Rules({}, {
        Override({.process = L"explorer.exe"}, Level::Native),
        Override({.windowClass = L"CabinetWClass"}, Level::Full),
    });
    CHECK_EQ(TopLevel(kExplorer, overrides).level, Level::Native);
    RuleSet reversed = Rules({}, {
        Override({.windowClass = L"CabinetWClass"}, Level::Full),
        Override({.process = L"explorer.exe"}, Level::Native),
    });
    CHECK_EQ(TopLevel(kExplorer, reversed).level, Level::Native);
}

TEST(rules_unknown_field_never_matches_a_rule_naming_it) {
    WindowIdentity window = kExplorer;
    window.module = Field::Unknown();
    RuleSet rules = Rules({
        BuiltIn({.module = L"explorerframe.dll", .windowClass = L"CabinetWClass"}, Level::Full),
        BuiltIn({.windowClass = L"CabinetWClass"}, Level::Visual),
    });
    // The two-field rule names the unknown module, so only the one-field rule matches.
    Classification c = TopLevel(window, rules);
    CHECK_EQ(c.level, Level::Visual);
    CHECK(c.rule == &rules.builtIns[1]);

    // An unknown field is not a failure: the window is classified normally.
    window.process = Field::Unknown();
    window.windowClass = Field::Unknown();
    window.templateId = TemplateField::Unknown();
    Classification none = TopLevel(window, rules);
    CHECK_EQ(none.level, Level::Native);
    CHECK_EQ(none.reason, Reason::Default);
}

TEST(rules_unknown_template_never_matches_a_template_rule) {
    WindowIdentity dialog = Window(L"explorer.exe", L"shell32.dll", L"#32770",
                                   TemplateField::Unknown());
    RuleSet rules = Rules({BuiltIn({.templateId = L"1040"}, Level::Full)});
    CHECK_EQ(TopLevel(dialog, rules).reason, Reason::Default);
}

TEST(rules_not_a_dialog_never_matches_a_template_rule) {
    RuleSet rules = Rules({BuiltIn({.windowClass = L"CabinetWClass", .templateId = L"1040"},
                                   Level::Full)});
    CHECK_EQ(TopLevel(kExplorer, rules).reason, Reason::Default);
}

TEST(rules_template_rule_matches_a_dialog) {
    WindowIdentity dialog = Window(L"explorer.exe", L"shell32.dll", L"#32770",
                                   TemplateField::Of(L"1040"));
    RuleSet rules = Rules({BuiltIn({.module = L"shell32.dll", .templateId = L"1040"},
                                   Level::Visual)});
    Classification c = TopLevel(dialog, rules);
    CHECK_EQ(c.level, Level::Visual);
    CHECK_EQ(c.reason, Reason::Rule);
}

TEST(rules_matching_is_case_insensitive) {
    WindowIdentity dialog = Window(L"Explorer.EXE", L"SHELL32.dll", L"#32770",
                                   TemplateField::Of(L"PropSheetPage"));
    RuleSet byProcess = Rules({BuiltIn({.process = L"explorer.exe"}, Level::Full)});
    CHECK_EQ(TopLevel(dialog, byProcess).level, Level::Full);
    RuleSet byModule = Rules({BuiltIn({.module = L"Shell32.DLL"}, Level::Full)});
    CHECK_EQ(TopLevel(dialog, byModule).level, Level::Full);
    RuleSet byTemplate = Rules({BuiltIn({.templateId = L"PROPSHEETPAGE"}, Level::Full)});
    CHECK_EQ(TopLevel(dialog, byTemplate).level, Level::Full);

    WindowIdentity explorer = kExplorer;
    RuleSet byClass = Rules({BuiltIn({.windowClass = L"cabinetwclass"}, Level::Full)});
    CHECK_EQ(TopLevel(explorer, byClass).level, Level::Full);
}

// --- Step 2: locked shell surfaces (FR-015) ------------------------------------------------

TEST(rules_locked_top_level_is_native_even_with_higher_override) {
    WindowIdentity taskbar = Window(L"explorer.exe", L"explorer.exe", L"Shell_TrayWnd");
    RuleSet rules = Rules(
        {Locked({.process = L"explorer.exe", .windowClass = L"Shell_TrayWnd"})},
        {Override({.process = L"explorer.exe", .module = L"explorer.exe",
                   .windowClass = L"Shell_TrayWnd"},
                  Level::Full)});
    Classification c = TopLevel(taskbar, rules);
    CHECK_EQ(c.level, Level::Native);
    CHECK_EQ(c.reason, Reason::Locked);
    CHECK(c.rule == &rules.builtIns[0]);
}

TEST(rules_child_of_locked_top_level_is_native_locked) {
    WindowIdentity taskbar = Window(L"explorer.exe", L"explorer.exe", L"Shell_TrayWnd");
    WindowIdentity child = Window(L"explorer.exe", L"explorer.exe", L"ReBarWindow32");
    RuleSet rules = Rules({Locked({.process = L"explorer.exe", .windowClass = L"Shell_TrayWnd"})},
                          {Override({.windowClass = L"ReBarWindow32"}, Level::Full)});
    Classification c = Classify(child, {}, &taskbar, rules);
    CHECK_EQ(c.level, Level::Native);
    CHECK_EQ(c.reason, Reason::Locked);
}

TEST(rules_locked_rule_needs_every_field_to_match) {
    // Same class in another process is not a shell surface.
    WindowIdentity other = Window(L"other.exe", L"other.exe", L"Shell_TrayWnd");
    RuleSet rules = Rules({Locked({.process = L"explorer.exe", .windowClass = L"Shell_TrayWnd"})},
                          {Override({.process = L"other.exe"}, Level::Full)});
    Classification c = TopLevel(other, rules);
    CHECK_EQ(c.level, Level::Full);
    CHECK_EQ(c.reason, Reason::Rule);
}

// --- Step 4: child capped by its top-level (FR-013) ----------------------------------------

TEST(rules_child_capped_by_top_level) {
    WindowIdentity child = Window(L"explorer.exe", L"explorerframe.dll", L"DirectUIHWND");
    RuleSet rules = Rules({
        BuiltIn({.windowClass = L"CabinetWClass"}, Level::Visual),
        BuiltIn({.windowClass = L"DirectUIHWND"}, Level::Full),
    });
    Classification c = Classify(child, {}, &kExplorer, rules);
    CHECK_EQ(c.level, Level::Visual);
    CHECK_EQ(c.ownLevel, Level::Full);
    CHECK_EQ(c.reason, Reason::CappedByTopLevel);
    CHECK(c.rule == &rules.builtIns[1]);
}

TEST(rules_child_capped_by_unmatched_top_level) {
    WindowIdentity child = Window(L"explorer.exe", L"explorerframe.dll", L"DirectUIHWND");
    RuleSet rules = Rules({BuiltIn({.windowClass = L"DirectUIHWND"}, Level::Full)});
    Classification c = Classify(child, {}, &kExplorer, rules);
    CHECK_EQ(c.level, Level::Native);
    CHECK_EQ(c.ownLevel, Level::Full);
    CHECK_EQ(c.reason, Reason::CappedByTopLevel);
}

TEST(rules_child_lower_than_top_level_keeps_own_level) {
    WindowIdentity child = Window(L"explorer.exe", L"explorerframe.dll", L"DirectUIHWND");
    RuleSet rules = Rules({
        BuiltIn({.windowClass = L"CabinetWClass"}, Level::Full),
        BuiltIn({.windowClass = L"DirectUIHWND"}, Level::Visual),
    });
    Classification c = Classify(child, {}, &kExplorer, rules);
    CHECK_EQ(c.level, Level::Visual);
    CHECK_EQ(c.reason, Reason::Rule);

    // A child with no rule is native by default, not capped.
    WindowIdentity plain = Window(L"explorer.exe", L"explorerframe.dll", L"Plain");
    Classification d = Classify(plain, {}, &kExplorer, rules);
    CHECK_EQ(d.level, Level::Native);
    CHECK_EQ(d.reason, Reason::Default);
}

// --- Step 4: hosted pages in the cap chain (FR-013a) ---------------------------------------
//
// A property sheet: the frame is `comctl32.dll` template 1006 (research R8 "Consequence for
// rules"); pages are child `#32770` windows with their own module and template. `pages` are
// the hosted pages between a window and its top-level window, nearest first.

namespace {

const WindowIdentity kFrame =
    Window(L"explorer.exe", L"comctl32.dll", L"#32770", TemplateField::Of(L"1006"));
const WindowIdentity kGeneralPage =
    Window(L"explorer.exe", L"shell32.dll", L"#32770", TemplateField::Of(L"1041"));
const WindowIdentity kThirdPartyPage =
    Window(L"explorer.exe", L"thirdparty.dll", L"#32770", TemplateField::Of(L"100"));
const WindowIdentity kButton = Window(L"explorer.exe", L"shell32.dll", L"Button");

constexpr RuleFields kFrameRule = {.module = L"comctl32.dll", .templateId = L"1006"};

}  // namespace

TEST(rules_frame_is_not_lowered_by_its_pages) {
    // The frame is classified on its own identity; its pages do not lower it.
    RuleSet rules = Rules({BuiltIn(kFrameRule, Level::Full)});
    Classification c = TopLevel(kFrame, rules);
    CHECK_EQ(c.level, Level::Full);
    CHECK_EQ(c.reason, Reason::Rule);
}

TEST(rules_page_capped_by_top_level) {
    RuleSet rules = Rules({
        BuiltIn(kFrameRule, Level::Visual),
        BuiltIn({.module = L"shell32.dll", .templateId = L"1041"}, Level::Full),
    });
    Classification c = Classify(kGeneralPage, {}, &kFrame, rules);
    CHECK_EQ(c.level, Level::Visual);
    CHECK_EQ(c.ownLevel, Level::Full);
    CHECK_EQ(c.reason, Reason::CappedByTopLevel);
    CHECK(c.rule == &rules.builtIns[1]);
}

TEST(rules_control_capped_by_page) {
    std::vector<WindowIdentity> pages = {kThirdPartyPage};
    RuleSet rules = Rules({
        BuiltIn(kFrameRule, Level::Full),
        BuiltIn({.module = L"thirdparty.dll"}, Level::Visual),
        BuiltIn({.windowClass = L"Button"}, Level::Full),
    });
    Classification c = Classify(kButton, pages, &kFrame, rules);
    CHECK_EQ(c.level, Level::Visual);
    CHECK_EQ(c.ownLevel, Level::Full);
    CHECK_EQ(c.reason, Reason::CappedByPage);
    CHECK(c.rule == &rules.builtIns[2]);
}

TEST(rules_native_page_in_full_frame_stays_native) {
    // No rule names the third-party page: it stays as Windows draws it, as a whole, while the
    // frame and the other pages keep their level.
    std::vector<WindowIdentity> pages = {kThirdPartyPage};
    RuleSet rules = Rules({
        BuiltIn(kFrameRule, Level::Full),
        BuiltIn({.module = L"shell32.dll"}, Level::Full),
    });
    Classification page = Classify(kThirdPartyPage, {}, &kFrame, rules);
    CHECK_EQ(page.level, Level::Native);
    CHECK_EQ(page.reason, Reason::Default);

    Classification frame = TopLevel(kFrame, rules);
    CHECK_EQ(frame.level, Level::Full);
    CHECK_EQ(frame.reason, Reason::Rule);

    Classification other = Classify(kGeneralPage, {}, &kFrame, rules);
    CHECK_EQ(other.level, Level::Full);
    CHECK_EQ(other.reason, Reason::Rule);

    // A control inside the native page is native too, even with a rule of its own.
    Classification button = Classify(kButton, pages, &kFrame, rules);
    CHECK_EQ(button.level, Level::Native);
    CHECK_EQ(button.ownLevel, Level::Full);
    CHECK_EQ(button.reason, Reason::CappedByPage);
}

TEST(rules_control_in_full_page_inside_visual_frame_gets_visual) {
    // The page is capped by the frame, and the control by the page's effective level.
    std::vector<WindowIdentity> pages = {kGeneralPage};
    RuleSet rules = Rules({
        BuiltIn(kFrameRule, Level::Visual),
        BuiltIn({.module = L"shell32.dll"}, Level::Full),
    });
    Classification c = Classify(kButton, pages, &kFrame, rules);
    CHECK_EQ(c.level, Level::Visual);
    CHECK_EQ(c.ownLevel, Level::Full);
    CHECK_EQ(c.reason, Reason::CappedByPage);
}

TEST(rules_control_lower_than_page_keeps_own_level) {
    std::vector<WindowIdentity> pages = {kGeneralPage};
    RuleSet rules = Rules({
        BuiltIn(kFrameRule, Level::Full),
        BuiltIn({.module = L"shell32.dll", .templateId = L"1041"}, Level::Full),
        BuiltIn({.windowClass = L"Button"}, Level::Visual),
    });
    Classification c = Classify(kButton, pages, &kFrame, rules);
    CHECK_EQ(c.level, Level::Visual);
    CHECK_EQ(c.reason, Reason::Rule);

    // A control with no rule is native by default, not capped.
    WindowIdentity plain = Window(L"explorer.exe", L"shell32.dll", L"Static");
    Classification d = Classify(plain, pages, &kFrame, rules);
    CHECK_EQ(d.level, Level::Native);
    CHECK_EQ(d.reason, Reason::Default);
}

TEST(rules_nested_pages_cap_each_level) {
    // control -> inner page -> outer page -> frame.
    WindowIdentity inner =
        Window(L"explorer.exe", L"inner.dll", L"#32770", TemplateField::Of(L"200"));
    WindowIdentity outer =
        Window(L"explorer.exe", L"outer.dll", L"#32770", TemplateField::Of(L"300"));
    std::vector<WindowIdentity> controlPages = {inner, outer};
    std::vector<WindowIdentity> innerPages = {outer};
    RuleSet rules = Rules({
        BuiltIn(kFrameRule, Level::Full),
        BuiltIn({.module = L"outer.dll"}, Level::Visual),
        BuiltIn({.module = L"inner.dll"}, Level::Full),
        BuiltIn({.windowClass = L"Button"}, Level::Full),
    });

    // The outer page is not capped (the frame is full).
    Classification o = Classify(outer, {}, &kFrame, rules);
    CHECK_EQ(o.level, Level::Visual);
    CHECK_EQ(o.reason, Reason::Rule);

    // The inner page is capped by the outer page.
    Classification i = Classify(inner, innerPages, &kFrame, rules);
    CHECK_EQ(i.level, Level::Visual);
    CHECK_EQ(i.ownLevel, Level::Full);
    CHECK_EQ(i.reason, Reason::CappedByPage);

    // The control is capped through the inner page, whose effective level is visual.
    Classification c = Classify(kButton, controlPages, &kFrame, rules);
    CHECK_EQ(c.level, Level::Visual);
    CHECK_EQ(c.ownLevel, Level::Full);
    CHECK_EQ(c.reason, Reason::CappedByPage);

    // A native outer page makes everything inside it native.
    RuleSet nativeOuter = Rules({
        BuiltIn(kFrameRule, Level::Full),
        BuiltIn({.module = L"inner.dll"}, Level::Full),
        BuiltIn({.windowClass = L"Button"}, Level::Full),
    });
    CHECK_EQ(Classify(inner, innerPages, &kFrame, nativeOuter).level, Level::Native);
    Classification n = Classify(kButton, controlPages, &kFrame, nativeOuter);
    CHECK_EQ(n.level, Level::Native);
    CHECK_EQ(n.reason, Reason::CappedByPage);
}

TEST(rules_locked_top_level_wins_over_pages) {
    // Pages and controls under a shell surface are native and locked, whatever their rules.
    WindowIdentity surface = Window(L"explorer.exe", L"explorer.exe", L"Shell_TrayWnd");
    std::vector<WindowIdentity> pages = {kGeneralPage};
    RuleSet rules = Rules({Locked({.process = L"explorer.exe", .windowClass = L"Shell_TrayWnd"})},
                          {Override({.module = L"shell32.dll"}, Level::Full),
                           Override({.windowClass = L"Button"}, Level::Full)});
    Classification page = Classify(kGeneralPage, {}, &surface, rules);
    CHECK_EQ(page.level, Level::Native);
    CHECK_EQ(page.reason, Reason::Locked);
    CHECK(page.rule == &rules.builtIns[0]);

    Classification button = Classify(kButton, pages, &surface, rules);
    CHECK_EQ(button.level, Level::Native);
    CHECK_EQ(button.reason, Reason::Locked);
}

TEST(rules_copy_of_classification_keeps_rule) {
    std::vector<WindowIdentity> pages = {kThirdPartyPage};
    RuleSet rules = Rules({BuiltIn(kFrameRule, Level::Full)},
                          {Override({.windowClass = L"Button"}, Level::Full)});
    rules.overrides[0].origin = L"class=Button; level=full";
    WindowClassification copy = CopyOf(Classify(kButton, pages, &kFrame, rules));
    CHECK_EQ(copy.level, Level::Native);
    CHECK_EQ(copy.ownLevel, Level::Full);
    CHECK_EQ(copy.reason, Reason::CappedByPage);
    CHECK(copy.ruleSource == RuleSource::Override);
    CHECK(copy.ruleOrigin == L"class=Button; level=full");

    // No rule: no origin.
    WindowClassification plain = CopyOf(Classify(kThirdPartyPage, {}, &kFrame, rules));
    CHECK_EQ(plain.reason, Reason::Default);
    CHECK(plain.ruleOrigin.empty());
}

// --- Step 1: failure (FR-018) --------------------------------------------------------------

TEST(rules_failed_window_is_native_failure) {
    WindowIdentity window = kExplorer;
    window.failed = true;
    RuleSet rules = Rules({BuiltIn({.windowClass = L"CabinetWClass"}, Level::Full)});
    Classification c = TopLevel(window, rules);
    CHECK_EQ(c.level, Level::Native);
    CHECK_EQ(c.ownLevel, Level::Native);
    CHECK_EQ(c.reason, Reason::Failure);
    CHECK(c.rule == nullptr);
}

TEST(rules_failed_top_level_is_native_failure) {
    WindowIdentity top = kExplorer;
    top.failed = true;
    WindowIdentity child = Window(L"explorer.exe", L"explorerframe.dll", L"DirectUIHWND");
    RuleSet rules = Rules({BuiltIn({.windowClass = L"DirectUIHWND"}, Level::Full)});
    Classification c = Classify(child, {}, &top, rules);
    CHECK_EQ(c.level, Level::Native);
    CHECK_EQ(c.reason, Reason::Failure);
}

TEST(rules_failed_page_is_native_failure) {
    // A page above the window could not be read: the chain is unknown, so the window is
    // native whatever its own rules say.
    WindowIdentity page = kGeneralPage;
    page.failed = true;
    std::vector<WindowIdentity> pages = {page};
    RuleSet rules = Rules({
        BuiltIn(kFrameRule, Level::Full),
        BuiltIn({.module = L"shell32.dll"}, Level::Full),
    });
    Classification c = Classify(kButton, pages, &kFrame, rules);
    CHECK_EQ(c.level, Level::Native);
    CHECK_EQ(c.reason, Reason::Failure);
}

// --- Built-in rules and timing (SC-004) ----------------------------------------------------

TEST(rules_builtins_have_an_identity_field_and_locked_ones_are_native) {
    const std::vector<Rule>& builtIns = BuiltInRules();
    CHECK(!builtIns.empty());
    for (const Rule& rule : builtIns) {
        CHECK(rule.process || rule.module || rule.windowClass || rule.templateId);
        CHECK(rule.source == RuleSource::BuiltIn);
        CHECK(!rule.origin.empty());
        if (rule.locked) {
            CHECK_EQ(rule.level, Level::Native);
        }
    }
}

TEST(rules_classify_timing_with_200_overrides) {
    RuleSet rules;
    rules.builtIns = BuiltInRules();
    for (int i = 0; i < 200; i++) {
        std::wstring name = L"app" + std::to_wstring(i) + L".exe";
        Rule rule = Override({}, i % 2 == 0 ? Level::Visual : Level::Full);
        rule.process = name;
        rule.windowClass = L"Window" + std::to_wstring(i);
        rules.overrides.push_back(std::move(rule));
    }
    rules.version = 1;

    // A child of a top-level window: the most work classify does without pages. The last
    // override matches the top-level, so every rule is scanned.
    WindowIdentity top = Window(L"APP199.exe", L"app199.exe", L"window199");
    WindowIdentity child = Window(L"APP199.exe", L"comctl32.dll", L"SysListView32");

    constexpr int kCalls = 10000;
    int full = 0;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kCalls; i++) {
        if (Classify(child, {}, &top, rules).ownLevel == Level::Full) {
            full++;
        }
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    double averageUs =
        std::chrono::duration<double, std::micro>(elapsed).count() / static_cast<double>(kCalls);
    std::printf("classify: %zu rules, average %.2f us per call over %d calls (SC-004 < 1000 us)\n",
                rules.builtIns.size() + rules.overrides.size(), averageUs, kCalls);
    CHECK_EQ(full, 0);  // the child matches no rule; keeps the loop from being optimized away
}
