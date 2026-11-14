// PURE: C++ standard library only (compiled into the mod and the unit tests).
//
// Design tokens (data-model.md "Design token set"): the inputs read from the system and the
// pure resolver that turns them into every visual value later specs use (FR-024 to FR-027).
// The system reads, the current snapshot and change notifications live elsewhere
// (tokens/system.h, tokens/runtime.h).

#ifndef WHOLEUI_TOKENS_TOKENS_H
#define WHOLEUI_TOKENS_TOKENS_H

#include <cstdint>
#include <string>

namespace wholeui::tokens {

// What the resolver needs from the system. Colors are ARGB (0xAARRGGBB).
struct TokenInputs {
    bool dark = false;
    std::uint32_t accent = 0;
    std::uint32_t accentLight1 = 0;
    std::uint32_t accentLight2 = 0;
    std::uint32_t accentLight3 = 0;
    std::uint32_t accentDark1 = 0;
    std::uint32_t accentDark2 = 0;
    std::uint32_t accentDark3 = 0;
    float textScale = 1.0f;  // 1.00 to 2.25
    bool variableFontAvailable = false;

    // Exact comparison: the runtime skips a read equal to the current inputs (Windows raises
    // one change several times).
    friend bool operator==(const TokenInputs&, const TokenInputs&) = default;
};

// The inputs used when a system read fails: light mode, Windows' default accent and shades,
// text at 100 %, and the variable font assumed present (Windows 11 ships it).
TokenInputs DefaultInputs();

// One step of the type ramp. Size and line height are px at 96 DPI, already multiplied by
// the text scale (unrounded). Weight is a font weight (400 Regular, 600 Semibold).
struct TypeStyle {
    float size = 0;
    float lineHeight = 0;
    int weight = 0;
    std::wstring gdiFace;  // the face name to use with GDI at this size
};

struct Spacing {
    int xs = 0, s = 0, m = 0, l = 0, xl = 0, xxl = 0;
};

// Every resolved token. Geometry is px at 96 DPI and never scales with text size (FR-026).
struct Tokens {
    bool dark = false;

    // Typography.
    std::wstring fontFamily;
    TypeStyle caption, body, bodyStrong, subtitle, title, titleLarge, display;

    // Geometry.
    int radiusControl = 0;
    int radiusOverlay = 0;
    int radiusMenuHover = 0;
    int radiusListSelection = 0;
    int radiusGridSelection = 0;
    Spacing spacing;
    int heightControl = 0;
    int heightControlCompact = 0;
    int heightListRow = 0;
    int heightMenuItem = 0;
    int heightNavItem = 0;
    int focusOuter = 0;
    int focusInner = 0;
    float selectionAccentOpacity = 0;

    // Colors (ARGB).
    std::uint32_t textPrimary = 0;
    std::uint32_t textSecondary = 0;
    std::uint32_t textTertiary = 0;
    std::uint32_t textDisabled = 0;
    std::uint32_t fillControl = 0;
    std::uint32_t fillControlHover = 0;
    std::uint32_t fillControlPressed = 0;
    std::uint32_t fillSubtleHover = 0;
    std::uint32_t fillSubtlePressed = 0;
    std::uint32_t fillCard = 0;
    std::uint32_t backgroundBase = 0;
    std::uint32_t strokeControl = 0;
    std::uint32_t strokeDivider = 0;
    std::uint32_t accentFill = 0;
    std::uint32_t accentFillHover = 0;
    std::uint32_t accentText = 0;
    std::uint32_t textOnAccent = 0;
    std::uint32_t selectionFill = 0;
    std::uint32_t focusStrokeOuter = 0;
    std::uint32_t focusStrokeInner = 0;
};

// Pure: the same inputs always give the same tokens.
Tokens Resolve(const TokenInputs& in);

}  // namespace wholeui::tokens

#endif  // WHOLEUI_TOKENS_TOKENS_H
