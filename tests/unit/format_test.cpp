// Tests for the diagnostic text in src/diagnostic/format.h against
// contracts/compatibility-doc.md "Diagnostic overlay text" and "Clipboard" (FR-021, FR-021b).

#include "../../src/diagnostic/format.h"

#include <cstdint>
#include <optional>
#include <string>

#include "compat_print.h"
#include "harness.h"

using wholeui::compat::Field;
using wholeui::compat::Level;
using wholeui::compat::Reason;
using wholeui::compat::RuleSource;
using wholeui::compat::TemplateField;
using wholeui::compat::WindowClassification;
using wholeui::compat::WindowIdentity;
using wholeui::diagnostic::BuildText;
using wholeui::diagnostic::ClipboardText;
using wholeui::diagnostic::OverlayText;
using wholeui::diagnostic::OverrideLine;
using wholeui::diagnostic::TableRow;

namespace {

// UTF-8 copy of a wide string (BMP only), so CHECK_EQ can print it.
std::string Utf8(const std::wstring& text) {
    std::string out;
    for (wchar_t c : text) {
        auto u = static_cast<std::uint32_t>(c);
        if (u < 0x80) {
            out.push_back(static_cast<char>(u));
        } else if (u < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (u >> 6)));
            out.push_back(static_cast<char>(0x80 | (u & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (u >> 12)));
            out.push_back(static_cast<char>(0x80 | ((u >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (u & 0x3F)));
        }
    }
    return out;
}

WindowIdentity Window(const wchar_t* process, const wchar_t* module, const wchar_t* windowClass,
                      TemplateField templateId = TemplateField::NotADialog()) {
    return WindowIdentity{Field::Of(process), Field::Of(module), Field::Of(windowClass),
                          std::move(templateId), false};
}

const WindowIdentity kExplorer = Window(L"explorer.exe", L"explorer.exe", L"CabinetWClass");

WindowClassification Shown(Level level, Reason reason, std::wstring origin = {},
                           RuleSource source = RuleSource::BuiltIn) {
    WindowClassification c;
    c.level = level;
    c.ownLevel = level;
    c.reason = reason;
    c.ruleSource = source;
    c.ruleOrigin = std::move(origin);
    return c;
}

// The Level line's value (the fourth line, after its tab).
std::string LevelLine(const std::wstring& overlay) {
    std::string text = Utf8(overlay);
    size_t start = 0;
    for (int line = 0; line < 3; line++) {
        start = text.find("\r\n", start) + 2;
    }
    size_t end = text.find("\r\n", start);
    std::string value = text.substr(start, end - start);
    return value.substr(value.find('\t') + 1);
}

}  // namespace

// --- Overlay ---------------------------------------------------------------------------------

TEST(format_overlay_in_process_top_level) {
    std::wstring text = OverlayText(kExplorer, kExplorer,
                                    Shown(Level::Native, Reason::Rule, L"File Explorer"), true);
    CHECK_EQ(Utf8(text),
             "Window\tCabinetWClass  ·  explorer.exe  ·  template not a dialog\r\n"
             "Top-level\tCabinetWClass  ·  explorer.exe  ·  template not a dialog\r\n"
             "Process\texplorer.exe\r\n"
             "Level\tnative  ·  rule  ·  File Explorer\r\n"
             "Ctrl+Alt+Shift+C copies");
}

TEST(format_overlay_child_and_dialog_template) {
    WindowIdentity sheet = Window(L"explorer.exe", L"shell32.dll", L"#32770",
                                  TemplateField::Of(L"1040"));
    WindowIdentity button = Window(L"explorer.exe", L"shell32.dll", L"Button");
    std::string text = Utf8(OverlayText(button, sheet,
                                        Shown(Level::Native, Reason::Default), true));
    CHECK_EQ(text.substr(0, text.find("\r\n")),
             "Window\tButton  ·  shell32.dll  ·  template not a dialog");
    CHECK(text.find("Top-level\t#32770  ·  shell32.dll  ·  template 1040\r\n") !=
          std::string::npos);
}

TEST(format_overlay_other_process) {
    WindowIdentity notepad = Window(L"notepad.exe", L"notepad.exe", L"Notepad");
    std::string text = Utf8(OverlayText(notepad, notepad,
                                        Shown(Level::Native, Reason::Default), false));
    CHECK(text.find("Process\tnotepad.exe  (not running WholeUI)\r\n") != std::string::npos);
}

TEST(format_overlay_in_process_has_no_not_running_note) {
    std::string text = Utf8(OverlayText(kExplorer, kExplorer,
                                        Shown(Level::Native, Reason::Default), true));
    CHECK(text.find("not running") == std::string::npos);
}

TEST(format_overlay_unknown_fields) {
    WindowIdentity unreadable{Field::Unknown(), Field::Unknown(), Field::Unknown(),
                              TemplateField::Unknown(), false};
    std::string text = Utf8(OverlayText(unreadable, unreadable,
                                        Shown(Level::Native, Reason::Default), false));
    CHECK(text.find("Window\tunknown  ·  unknown  ·  template unknown\r\n") !=
          std::string::npos);
    CHECK(text.find("Process\tunknown  (not running WholeUI)\r\n") != std::string::npos);
}

TEST(format_overlay_known_but_empty_field_is_unknown) {
    WindowIdentity empty = Window(L"", L"", L"", TemplateField::Of(L""));
    std::string text = Utf8(OverlayText(empty, empty, Shown(Level::Native, Reason::Default),
                                        true));
    CHECK(text.find("Window\tunknown  ·  unknown  ·  template unknown\r\n") !=
          std::string::npos);
}

TEST(format_overlay_reason_texts) {
    auto level = [](WindowClassification c) {
        return LevelLine(OverlayText(kExplorer, kExplorer, c, true));
    };
    CHECK_EQ(level(Shown(Level::Native, Reason::Default)), "native  ·  default");
    CHECK_EQ(level(Shown(Level::Visual, Reason::Rule, L"File Explorer")),
             "visual  ·  rule  ·  File Explorer");
    CHECK_EQ(level(Shown(Level::Full, Reason::Rule, L"class=CabinetWClass; level=full",
                         RuleSource::Override)),
             "full  ·  override  ·  class=CabinetWClass; level=full");
    CHECK_EQ(level(Shown(Level::Native, Reason::Locked, L"Taskbar")),
             "native  ·  shell surface  ·  Taskbar");
    CHECK_EQ(level(Shown(Level::Visual, Reason::CappedByTopLevel, L"Buttons")),
             "visual  ·  capped by top-level  ·  Buttons");
    CHECK_EQ(level(Shown(Level::Native, Reason::Failure)), "native  ·  failure");

    WindowClassification lowered = Shown(Level::Native, Reason::LoweredByPage, L"Properties");
    lowered.pageModule = Field::Of(L"rarext.dll");
    CHECK_EQ(level(lowered), "native  ·  lowered by page rarext.dll  ·  Properties");

    lowered.pageModule = Field::Unknown();
    CHECK_EQ(level(lowered), "native  ·  lowered by page unknown  ·  Properties");
}

// --- Clipboard -------------------------------------------------------------------------------

TEST(format_override_line_all_fields) {
    WindowIdentity dialog = Window(L"explorer.exe", L"shell32.dll", L"#32770",
                                   TemplateField::Of(L"1003"));
    CHECK_EQ(Utf8(OverrideLine(dialog, Level::Visual)),
             "process=explorer.exe; module=shell32.dll; class=#32770; template=1003; "
             "level=visual");
}

TEST(format_override_line_leaves_out_not_a_dialog) {
    CHECK_EQ(Utf8(OverrideLine(kExplorer, Level::Native)),
             "process=explorer.exe; module=explorer.exe; class=CabinetWClass; level=native");
}

TEST(format_override_line_leaves_out_unknown) {
    WindowIdentity partial{Field::Of(L"notepad.exe"), Field::Unknown(), Field::Of(L"Notepad"),
                           TemplateField::Unknown(), false};
    CHECK_EQ(Utf8(OverrideLine(partial, Level::Full)),
             "process=notepad.exe; class=Notepad; level=full");
}

TEST(format_table_row) {
    CHECK_EQ(Utf8(TableRow(kExplorer, Level::Native, L"26300.1234")),
             "| <name> | explorer.exe | explorer.exe | CabinetWClass | — | native | "
             "26300.1234 | — |");
}

TEST(format_table_row_unknown_fields_and_build_are_dashes) {
    WindowIdentity partial{Field::Of(L"notepad.exe"), Field::Unknown(), Field::Of(L"Notepad"),
                           TemplateField::Unknown(), false};
    CHECK_EQ(Utf8(TableRow(partial, Level::Visual, L"")),
             "| <name> | notepad.exe | — | Notepad | — | visual | — | — |");
}

TEST(format_table_row_with_template) {
    WindowIdentity dialog = Window(L"explorer.exe", L"shell32.dll", L"#32770",
                                   TemplateField::Of(L"1003"));
    CHECK_EQ(Utf8(TableRow(dialog, Level::Full, L"26300.1")),
             "| <name> | explorer.exe | shell32.dll | #32770 | 1003 | full | 26300.1 | — |");
}

TEST(format_build_text) {
    CHECK_EQ(Utf8(BuildText(std::wstring(L"26300"), 1234u)), "26300.1234");
    CHECK_EQ(Utf8(BuildText(std::wstring(L"26300"), 0u)), "26300.0");
    CHECK_EQ(Utf8(BuildText(std::nullopt, 1234u)), "");
    CHECK_EQ(Utf8(BuildText(std::wstring(L"26300"), std::nullopt)), "");
    CHECK_EQ(Utf8(BuildText(std::wstring(L""), 1234u)), "");
}

TEST(format_clipboard_is_two_lines_separated_by_crlf) {
    CHECK_EQ(Utf8(ClipboardText(kExplorer, Level::Native, L"26300.1234")),
             "process=explorer.exe; module=explorer.exe; class=CabinetWClass; level=native\r\n"
             "| <name> | explorer.exe | explorer.exe | CabinetWClass | — | native | "
             "26300.1234 | — |");
}
