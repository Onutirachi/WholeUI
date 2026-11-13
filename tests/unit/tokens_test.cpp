// Tests for the pure token resolver in src/tokens/tokens.h against data-model.md "Design token
// set" (FR-025, FR-026, SC-007): light and dark values, accent-derived colors for three
// accents, the type ramp at text scales 1.00, 1.50 and 2.25, font faces and fallback, and the
// defaults used when a system read fails.

#include "../../src/tokens/tokens.h"

#include <cstdint>
#include <string>

#include "harness.h"

using wholeui::tokens::DefaultInputs;
using wholeui::tokens::Resolve;
using wholeui::tokens::TokenInputs;
using wholeui::tokens::Tokens;
using wholeui::tokens::TypeStyle;

namespace {

constexpr int kRegular = 400;
constexpr int kSemibold = 600;

// The three accents of SC-007. Every shade differs from the others, so a token that reads the
// wrong shade fails. The default set is Windows' own (data-model.md); green and red are test
// values shaped like Windows shades.
struct AccentSet {
    uint32_t accent, light1, light2, light3, dark1, dark2, dark3;
};

constexpr AccentSet kBlue = {0xFF0078D4, 0xFF429CE3, 0xFF76B9ED, 0xFF99EBFF,
                             0xFF005A9E, 0xFF004275, 0xFF002642};
constexpr AccentSet kGreen = {0xFF107C10, 0xFF3C9A3C, 0xFF6CB86C, 0xFF9FD89F,
                              0xFF0B5A0B, 0xFF064006, 0xFF022802};
constexpr AccentSet kRed = {0xFFC42B1C, 0xFFD9554A, 0xFFE8837A, 0xFFF4B3AD,
                            0xFF9A1F14, 0xFF70150D, 0xFF480C06};

// `accent` is taken by value on purpose: with clang 20.1, a `const AccentSet&` parameter whose
// default is a constexpr object always reads the default, even when another object is passed.
TokenInputs Inputs(bool dark, AccentSet accent = kBlue, float textScale = 1.0f,
                   bool variableFont = true) {
    TokenInputs in;
    in.dark = dark;
    in.accent = accent.accent;
    in.accentLight1 = accent.light1;
    in.accentLight2 = accent.light2;
    in.accentLight3 = accent.light3;
    in.accentDark1 = accent.dark1;
    in.accentDark2 = accent.dark2;
    in.accentDark3 = accent.dark3;
    in.textScale = textScale;
    in.variableFontAvailable = variableFont;
    return in;
}

// The color with its alpha replaced; alpha is opacity x 255 rounded to nearest
// (0.90 -> 0xE6, 0.60 -> 0x99).
constexpr uint32_t WithAlpha(uint32_t argb, uint32_t alpha) {
    return (alpha << 24) | (argb & 0x00FFFFFF);
}

void CheckStyle(const TypeStyle& style, float size, float lineHeight, int weight) {
    CHECK_EQ(style.size, size);
    CHECK_EQ(style.lineHeight, lineHeight);
    CHECK_EQ(style.weight, weight);
}

// The whole type ramp at one text scale (data-model.md "Typography"; sizes are unrounded).
void CheckRamp(const Tokens& t, float scale) {
    CheckStyle(t.caption, 12 * scale, 16 * scale, kRegular);
    CheckStyle(t.body, 14 * scale, 20 * scale, kRegular);
    CheckStyle(t.bodyStrong, 14 * scale, 20 * scale, kSemibold);
    CheckStyle(t.subtitle, 20 * scale, 28 * scale, kSemibold);
    CheckStyle(t.title, 28 * scale, 36 * scale, kSemibold);
    CheckStyle(t.titleLarge, 40 * scale, 52 * scale, kSemibold);
    CheckStyle(t.display, 68 * scale, 92 * scale, kSemibold);
}

// Geometry is the same in every mode and at every text scale (FR-026).
void CheckGeometry(const Tokens& t) {
    CHECK_EQ(t.radiusControl, 4);
    CHECK_EQ(t.radiusOverlay, 8);
    CHECK_EQ(t.radiusMenuHover, 4);
    CHECK_EQ(t.radiusListSelection, 8);
    CHECK_EQ(t.radiusGridSelection, 16);
    CHECK_EQ(t.spacing.xs, 4);
    CHECK_EQ(t.spacing.s, 8);
    CHECK_EQ(t.spacing.m, 12);
    CHECK_EQ(t.spacing.l, 16);
    CHECK_EQ(t.spacing.xl, 24);
    CHECK_EQ(t.spacing.xxl, 32);
    CHECK_EQ(t.heightControl, 32);
    CHECK_EQ(t.heightControlCompact, 24);
    CHECK_EQ(t.heightListRow, 32);
    CHECK_EQ(t.heightMenuItem, 32);
    CHECK_EQ(t.heightNavItem, 36);
    CHECK_EQ(t.focusOuter, 2);
    CHECK_EQ(t.focusInner, 1);
    CHECK_EQ(t.selectionAccentOpacity, 0.60f);
}

void CheckAccentColors(const Tokens& t, bool dark, const AccentSet& a) {
    if (dark) {
        CHECK_EQ(t.accentFill, a.light2);
        CHECK_EQ(t.accentFillHover, WithAlpha(a.light2, 0xE6));
        CHECK_EQ(t.accentText, a.light3);
    } else {
        CHECK_EQ(t.accentFill, a.dark1);
        CHECK_EQ(t.accentFillHover, WithAlpha(a.dark1, 0xE6));
        CHECK_EQ(t.accentText, a.dark2);
    }
    CHECK_EQ(t.selectionFill, WithAlpha(a.accent, 0x99));
}

}  // namespace

// --- Colors ----------------------------------------------------------------------------------

TEST(tokens_light_colors) {
    Tokens t = Resolve(Inputs(/*dark=*/false));
    CHECK(!t.dark);
    CHECK_EQ(t.textPrimary, 0xE4000000u);
    CHECK_EQ(t.textSecondary, 0x9E000000u);
    CHECK_EQ(t.textTertiary, 0x72000000u);
    CHECK_EQ(t.textDisabled, 0x5C000000u);
    CHECK_EQ(t.fillControl, 0xB3FFFFFFu);
    CHECK_EQ(t.fillControlHover, 0x80F9F9F9u);
    CHECK_EQ(t.fillControlPressed, 0x4DF9F9F9u);
    CHECK_EQ(t.fillSubtleHover, 0x09000000u);
    CHECK_EQ(t.fillSubtlePressed, 0x06000000u);
    CHECK_EQ(t.fillCard, 0xB3FFFFFFu);
    CHECK_EQ(t.backgroundBase, 0xFFF3F3F3u);
    CHECK_EQ(t.strokeControl, 0x0F000000u);
    CHECK_EQ(t.strokeDivider, 0x14000000u);
    CHECK_EQ(t.textOnAccent, 0xFFFFFFFFu);
    CHECK_EQ(t.focusStrokeOuter, 0xE4000000u);
    CHECK_EQ(t.focusStrokeInner, 0xB3FFFFFFu);
}

TEST(tokens_dark_colors) {
    Tokens t = Resolve(Inputs(/*dark=*/true));
    CHECK(t.dark);
    CHECK_EQ(t.textPrimary, 0xFFFFFFFFu);
    CHECK_EQ(t.textSecondary, 0xC5FFFFFFu);
    CHECK_EQ(t.textTertiary, 0x87FFFFFFu);
    CHECK_EQ(t.textDisabled, 0x5DFFFFFFu);
    CHECK_EQ(t.fillControl, 0x0FFFFFFFu);
    CHECK_EQ(t.fillControlHover, 0x15FFFFFFu);
    CHECK_EQ(t.fillControlPressed, 0x08FFFFFFu);
    CHECK_EQ(t.fillSubtleHover, 0x0FFFFFFFu);
    CHECK_EQ(t.fillSubtlePressed, 0x0AFFFFFFu);
    CHECK_EQ(t.fillCard, 0x0DFFFFFFu);
    CHECK_EQ(t.backgroundBase, 0xFF202020u);
    CHECK_EQ(t.strokeControl, 0x12FFFFFFu);
    CHECK_EQ(t.strokeDivider, 0x15FFFFFFu);
    CHECK_EQ(t.textOnAccent, 0xFF000000u);
    CHECK_EQ(t.focusStrokeOuter, 0xFFFFFFFFu);
    CHECK_EQ(t.focusStrokeInner, 0xB3000000u);
}

TEST(tokens_accent_colors_light) {
    for (const AccentSet* a : {&kBlue, &kGreen, &kRed}) {
        CheckAccentColors(Resolve(Inputs(/*dark=*/false, *a)), /*dark=*/false, *a);
    }
}

TEST(tokens_accent_colors_dark) {
    for (const AccentSet* a : {&kBlue, &kGreen, &kRed}) {
        CheckAccentColors(Resolve(Inputs(/*dark=*/true, *a)), /*dark=*/true, *a);
    }
}

TEST(tokens_accent_does_not_change_neutral_colors) {
    Tokens blue = Resolve(Inputs(/*dark=*/false, kBlue));
    Tokens red = Resolve(Inputs(/*dark=*/false, kRed));
    CHECK_EQ(red.textPrimary, blue.textPrimary);
    CHECK_EQ(red.fillControl, blue.fillControl);
    CHECK_EQ(red.backgroundBase, blue.backgroundBase);
    CHECK_EQ(red.textOnAccent, blue.textOnAccent);
    CHECK_EQ(red.focusStrokeOuter, blue.focusStrokeOuter);
}

// --- Type ramp and text scale (FR-026) -------------------------------------------------------

TEST(tokens_type_ramp_at_100_percent) {
    Tokens t = Resolve(Inputs(/*dark=*/false, kBlue, 1.00f));
    CheckRamp(t, 1.00f);
    CheckGeometry(t);
}

TEST(tokens_type_ramp_at_150_percent) {
    Tokens t = Resolve(Inputs(/*dark=*/false, kBlue, 1.50f));
    CheckRamp(t, 1.50f);
    CHECK_EQ(t.body.size, 21.0f);  // spot check: unrounded product
    CheckGeometry(t);
}

TEST(tokens_type_ramp_at_225_percent) {
    Tokens t = Resolve(Inputs(/*dark=*/true, kBlue, 2.25f));
    CheckRamp(t, 2.25f);
    CHECK_EQ(t.body.size, 31.5f);
    CHECK_EQ(t.caption.lineHeight, 36.0f);
    CheckGeometry(t);
}

TEST(tokens_geometry_same_in_light_and_dark) {
    CheckGeometry(Resolve(Inputs(/*dark=*/false)));
    CheckGeometry(Resolve(Inputs(/*dark=*/true)));
}

TEST(tokens_text_scale_does_not_change_colors) {
    Tokens small = Resolve(Inputs(/*dark=*/true, kGreen, 1.00f));
    Tokens large = Resolve(Inputs(/*dark=*/true, kGreen, 2.25f));
    CHECK_EQ(large.textPrimary, small.textPrimary);
    CHECK_EQ(large.accentFill, small.accentFill);
    CHECK_EQ(large.selectionFill, small.selectionFill);
}

// --- Fonts (R12) -----------------------------------------------------------------------------

TEST(tokens_variable_font_family_and_faces) {
    Tokens t = Resolve(Inputs(/*dark=*/false, kBlue, 1.00f, /*variableFont=*/true));
    CHECK(t.fontFamily == L"Segoe UI Variable");
    // Below 20 px: Text; from 20 px: Display.
    CHECK(t.caption.gdiFace == L"Segoe UI Variable Text");
    CHECK(t.body.gdiFace == L"Segoe UI Variable Text");
    CHECK(t.bodyStrong.gdiFace == L"Segoe UI Variable Text");
    CHECK(t.subtitle.gdiFace == L"Segoe UI Variable Display");
    CHECK(t.title.gdiFace == L"Segoe UI Variable Display");
    CHECK(t.titleLarge.gdiFace == L"Segoe UI Variable Display");
    CHECK(t.display.gdiFace == L"Segoe UI Variable Display");
}

TEST(tokens_face_split_uses_scaled_size) {
    // The split is on the size the text is drawn at: at 150 % body is 21 px (Display) and
    // caption is 18 px (Text).
    Tokens t = Resolve(Inputs(/*dark=*/false, kBlue, 1.50f, /*variableFont=*/true));
    CHECK(t.caption.gdiFace == L"Segoe UI Variable Text");
    CHECK(t.body.gdiFace == L"Segoe UI Variable Display");
    CHECK(t.bodyStrong.gdiFace == L"Segoe UI Variable Display");
    CHECK(t.subtitle.gdiFace == L"Segoe UI Variable Display");
}

TEST(tokens_fallback_font_is_segoe_ui_everywhere) {
    for (float scale : {1.00f, 1.50f, 2.25f}) {
        Tokens t = Resolve(Inputs(/*dark=*/false, kBlue, scale, /*variableFont=*/false));
        CHECK(t.fontFamily == L"Segoe UI");
        for (const TypeStyle* s : {&t.caption, &t.body, &t.bodyStrong, &t.subtitle, &t.title,
                                   &t.titleLarge, &t.display}) {
            CHECK(s->gdiFace == L"Segoe UI");
        }
        CheckRamp(t, scale);  // the fallback changes the face only
    }
}

// --- Failure defaults (spec edge case, R11) --------------------------------------------------

TEST(tokens_default_inputs) {
    TokenInputs in = DefaultInputs();
    CHECK(!in.dark);
    CHECK_EQ(in.accent, 0xFF0078D4u);
    CHECK_EQ(in.accentLight1, 0xFF429CE3u);
    CHECK_EQ(in.accentLight2, 0xFF76B9EDu);
    CHECK_EQ(in.accentLight3, 0xFF99EBFFu);
    CHECK_EQ(in.accentDark1, 0xFF005A9Eu);
    CHECK_EQ(in.accentDark2, 0xFF004275u);
    CHECK_EQ(in.accentDark3, 0xFF002642u);
    CHECK_EQ(in.textScale, 1.0f);
}

TEST(tokens_resolve_default_inputs) {
    Tokens t = Resolve(DefaultInputs());
    CHECK(!t.dark);
    CHECK_EQ(t.backgroundBase, 0xFFF3F3F3u);
    CheckAccentColors(t, /*dark=*/false, kBlue);
    CheckRamp(t, 1.0f);
    CheckGeometry(t);
}
