// PURE: C++ standard library only (compiled into the mod and the unit tests).

#include "tokens.h"

namespace wholeui::tokens {

namespace {

constexpr int kRegular = 400;
constexpr int kSemibold = 600;

// The Text face below this scaled size, the Display face from it (research R12).
constexpr float kDisplayFaceFromPx = 20.0f;

constexpr float kSelectionAccentOpacity = 0.60f;
constexpr float kAccentFillHoverOpacity = 0.90f;

// Keeps the color and sets its alpha to opacity x 255, rounded to nearest.
std::uint32_t WithOpacity(std::uint32_t argb, float opacity) {
    std::uint32_t alpha = static_cast<std::uint32_t>(opacity * 255.0f + 0.5f);
    return (alpha << 24) | (argb & 0x00FFFFFFu);
}

TypeStyle Style(const TokenInputs& in, float size, float lineHeight, int weight) {
    TypeStyle style;
    style.size = size * in.textScale;
    style.lineHeight = lineHeight * in.textScale;
    style.weight = weight;
    if (!in.variableFontAvailable) {
        style.gdiFace = L"Segoe UI";
    } else if (style.size < kDisplayFaceFromPx) {
        style.gdiFace = L"Segoe UI Variable Text";
    } else {
        style.gdiFace = L"Segoe UI Variable Display";
    }
    return style;
}

}  // namespace

TokenInputs DefaultInputs() {
    TokenInputs in;
    in.dark = false;
    in.accent = 0xFF0078D4;
    in.accentLight1 = 0xFF429CE3;
    in.accentLight2 = 0xFF76B9ED;
    in.accentLight3 = 0xFF99EBFF;
    in.accentDark1 = 0xFF005A9E;
    in.accentDark2 = 0xFF004275;
    in.accentDark3 = 0xFF002642;
    in.textScale = 1.0f;
    in.variableFontAvailable = true;
    return in;
}

Tokens Resolve(const TokenInputs& in) {
    Tokens t;
    t.dark = in.dark;

    t.fontFamily = in.variableFontAvailable ? L"Segoe UI Variable" : L"Segoe UI";
    t.caption = Style(in, 12, 16, kRegular);
    t.body = Style(in, 14, 20, kRegular);
    t.bodyStrong = Style(in, 14, 20, kSemibold);
    t.subtitle = Style(in, 20, 28, kSemibold);
    t.title = Style(in, 28, 36, kSemibold);
    t.titleLarge = Style(in, 40, 52, kSemibold);
    t.display = Style(in, 68, 92, kSemibold);

    t.radiusControl = 4;
    t.radiusOverlay = 8;
    t.radiusMenuHover = 4;
    t.radiusListSelection = 8;
    t.radiusGridSelection = 16;
    t.spacing = Spacing{4, 8, 12, 16, 24, 32};
    t.heightControl = 32;
    t.heightControlCompact = 24;
    t.heightListRow = 32;
    t.heightMenuItem = 32;
    t.heightNavItem = 36;
    t.focusOuter = 2;
    t.focusInner = 1;
    t.selectionAccentOpacity = kSelectionAccentOpacity;

    if (in.dark) {
        t.textPrimary = 0xFFFFFFFF;
        t.textSecondary = 0xC5FFFFFF;
        t.textTertiary = 0x87FFFFFF;
        t.textDisabled = 0x5DFFFFFF;
        t.fillControl = 0x0FFFFFFF;
        t.fillControlHover = 0x15FFFFFF;
        t.fillControlPressed = 0x08FFFFFF;
        t.fillSubtleHover = 0x0FFFFFFF;
        t.fillSubtlePressed = 0x0AFFFFFF;
        t.fillCard = 0x0DFFFFFF;
        t.backgroundBase = 0xFF202020;
        t.strokeControl = 0x12FFFFFF;
        t.strokeDivider = 0x15FFFFFF;
        t.accentFill = in.accentLight2;
        t.accentText = in.accentLight3;
        t.textOnAccent = 0xFF000000;
        t.focusStrokeOuter = 0xFFFFFFFF;
        t.focusStrokeInner = 0xB3000000;
    } else {
        t.textPrimary = 0xE4000000;
        t.textSecondary = 0x9E000000;
        t.textTertiary = 0x72000000;
        t.textDisabled = 0x5C000000;
        t.fillControl = 0xB3FFFFFF;
        t.fillControlHover = 0x80F9F9F9;
        t.fillControlPressed = 0x4DF9F9F9;
        t.fillSubtleHover = 0x09000000;
        t.fillSubtlePressed = 0x06000000;
        t.fillCard = 0xB3FFFFFF;
        t.backgroundBase = 0xFFF3F3F3;
        t.strokeControl = 0x0F000000;
        t.strokeDivider = 0x14000000;
        t.accentFill = in.accentDark1;
        t.accentText = in.accentDark2;
        t.textOnAccent = 0xFFFFFFFF;
        t.focusStrokeOuter = 0xE4000000;
        t.focusStrokeInner = 0xB3FFFFFF;
    }
    t.accentFillHover = WithOpacity(t.accentFill, kAccentFillHoverOpacity);
    t.selectionFill = WithOpacity(in.accent, kSelectionAccentOpacity);
    return t;
}

}  // namespace wholeui::tokens
