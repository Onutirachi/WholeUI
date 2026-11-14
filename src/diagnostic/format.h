// PURE: C++ standard library only (compiled into the mod and the unit tests).
//
// The text diagnostic mode shows and copies (contracts/compatibility-doc.md "Diagnostic
// overlay text" and "Clipboard", FR-021, FR-021b). Lines are separated by CRLF.

#ifndef WHOLEUI_DIAGNOSTIC_FORMAT_H
#define WHOLEUI_DIAGNOSTIC_FORMAT_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "../compat/rules.h"

namespace wholeui::diagnostic {

// "native", "visual" or "full".
const wchar_t* LevelText(compat::Level level) noexcept;

// The overlay: Window, Top-level, Process and Level lines, then the shortcut hint. Each line
// is "<label>\t<value>" (the overlay aligns the values on a tab stop), except the hint.
// `topLevel` is the window itself when the window is top-level. `inProcess` is false for a
// window of another process (the Process line then says it is not running WholeUI).
std::wstring OverlayText(const compat::WindowIdentity& window,
                         const compat::WindowIdentity& topLevel,
                         const compat::WindowClassification& classification, bool inProcess);

// An override entry for the top-level window at `level`; unknown fields and "not a dialog"
// are left out.
std::wstring OverrideLine(const compat::WindowIdentity& topLevel, compat::Level level);

// A docs/compatibility.md "Windows" row for the top-level window, with "<name>" as the
// window name; unknown fields, "not a dialog" and an empty build are written as an em dash.
std::wstring TableRow(const compat::WindowIdentity& topLevel, compat::Level level,
                      std::wstring_view build);

// "<CurrentBuild>.<UBR>", or empty when either read failed (TableRow then writes a dash).
std::wstring BuildText(const std::optional<std::wstring>& currentBuild,
                       std::optional<std::uint32_t> ubr);

// What the copy shortcut puts on the clipboard: OverrideLine, CRLF, TableRow.
std::wstring ClipboardText(const compat::WindowIdentity& topLevel, compat::Level level,
                           std::wstring_view build);

}  // namespace wholeui::diagnostic

#endif  // WHOLEUI_DIAGNOSTIC_FORMAT_H
