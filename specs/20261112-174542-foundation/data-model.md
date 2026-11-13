# Data Model: Foundation

**Spec**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md)

All entities live in the mod's memory; nothing is persisted except what Windhawk stores for
the mod's settings. Types named here are the ones the code uses.

## Level

`enum class Level : uint8_t { Native = 0, Visual = 1, Full = 2 }`. Ordered: `Native < Visual <
Full`. "Lowest" means `std::min`.

## WindowIdentity

| Field | Type | Notes |
|---|---|---|
| `process` | `Field` | Image file name, lower-case, no path (`explorer.exe`) |
| `module` | `Field` | Creating module file name, lower-case, no path (`shell32.dll`) |
| `windowClass` | `Field` | Class name as reported; compared case-insensitively |
| `templateId` | `TemplateField` | Numeric ID (`1040`) or resource name; or `NotADialog` |

`Field` is either a known value or `Unknown`. `TemplateField` is a known value, `Unknown` or
`NotADialog`. **An `Unknown` field never matches a rule that names it** (spec edge cases).

Identity also carries, for classification only: `isTopLevel` (bool) and the identity of the
top-level window (`GetAncestor(GA_ROOT)`), and, for a property sheet, the identities of its
known pages.

## Rule

| Field | Type | Notes |
|---|---|---|
| `process`, `module`, `windowClass`, `templateId` | optional string | At least one present |
| `level` | `Level` | |
| `source` | `BuiltIn` / `Override` | |
| `locked` | bool | Built-in shell surfaces only (FR-015) |
| `origin` | string | For built-ins: the window name from `docs/compatibility.md`; for overrides: the text as typed. Shown by diagnostic mode |

Derived: `specificity` = number of named fields (1 to 4).

**Validation (overrides)**: see [contracts/settings.md](contracts/settings.md). An invalid
entry produces an `OverrideError { index, text, reason }`, is logged, and is not a rule.

**Matching**: a rule matches an identity when every field it names equals the identity's
field (case-insensitive) and that field is not `Unknown`.

## RuleSet

Immutable snapshot: built-in rules (generated at build time) + parsed overrides + a version
number. Replaced as a whole when settings change.

## Classification

| Field | Type | Notes |
|---|---|---|
| `level` | `Level` | Effective level |
| `reason` | enum | `Default` (no rule), `Rule`, `Locked` (shell surface), `CappedByTopLevel` (FR-013), `LoweredByPage` (FR-013a), `Failure` (FR-018) |
| `rule` | `const Rule*` or none | The winning rule, if any |
| `ownLevel` | `Level` | Level before the caps, shown by diagnostic mode |
| `page` | page identity or none | `LoweredByPage` only: the first page with the lowest level; its module is shown by diagnostic mode (`lowered by page <module>`) |

### Resolution order (pure function `classify(identity, ruleSet)`)

1. Any field read failed in a way that is not a normal `Unknown` (exception, invalid
   window) → `Native`, reason `Failure`.
2. Top-level identity matches a `locked` rule → `Native`, reason `Locked`. Nothing else is
   consulted.
3. Own level: among matching rules, highest `specificity`; at equal specificity `Override`
   over `BuiltIn`; remaining tie → lowest `level`. No match → `Native`, reason `Default`.
4. For a top-level window with known pages: level = min(own, each page's own level) → reason
   `LoweredByPage` when lowered.
5. For a child window: level = min(own, effective level of its top-level) → reason
   `CappedByTopLevel` when lowered.

### Cache entry

`HWND → { Classification, threadId, classAtom, ruleSetVersion }`. Valid while thread ID and
class atom are unchanged and `ruleSetVersion` is current; on a new rule set every cached
window is re-classified and listeners are told about each window whose level changed
(FR-019).

### Dialog facts store

`HWND → { module, templateId }` for dialogs seen by the creation hooks, and
`HWND (sheet) → [ { module, templateId } … ]` for property sheet pages (research R8).

## Design token set

Inputs (`TokenInputs`): `dark` (bool), `accent` + `accentLight1..3` + `accentDark1..3`
(ARGB), `textScale` (1.00 to 2.25), `variableFontAvailable` (bool). Output (`Tokens`):
resolved values below. `resolveTokens(TokenInputs) → Tokens` is a pure function.

Defaults when a system read fails: `dark = false`, accent `#0078D4`, light 1–3
`#429CE3 #76B9ED #99EBFF`, dark 1–3 `#005A9E #004275 #002642`, `textScale = 1.0`.
Each input falls back on its own, except that the accent and its six shades are one input:
if any of the seven colors cannot be read, all seven take their defaults, so the shades
always belong to the accent. A text scale outside 1.00 to 2.25 is clamped (with a warning).
A failed font check assumes the variable font is present.

### Typography

Font family: "Segoe UI Variable" if available, else "Segoe UI". GDI face: "Segoe UI Variable
Text" below 20 px, "Segoe UI Variable Display" from 20 px; "Segoe UI" when not available.
The 20 px split compares the scaled size (after `textScale`): at 150 % `body` is 21 px and
uses the Display face.

| Token | Size / line height (px at 100 %) | Weight |
|---|---|---|
| `caption` | 12 / 16 | Regular |
| `body` | 14 / 20 | Regular |
| `bodyStrong` | 14 / 20 | Semibold |
| `subtitle` | 20 / 28 | Semibold |
| `title` | 28 / 36 | Semibold |
| `titleLarge` | 40 / 52 | Semibold |
| `display` | 68 / 92 | Semibold |

Size and line height are multiplied by `textScale` (unrounded floats). Nothing else scales.

### Geometry (fixed, px at 96 DPI)

| Token | Value | Source |
|---|---|---|
| `radiusControl` | 4 | WinUI `ControlCornerRadius` |
| `radiusOverlay` | 8 | WinUI `OverlayCornerRadius` |
| `radiusMenuHover` | 4 | Maintainer reference |
| `radiusListSelection` | 8 | Maintainer reference |
| `radiusGridSelection` | 16 | Maintainer reference |
| `spacing` | 4, 8, 12, 16, 24, 32 (`xs` … `xxl`) | Fluent 4 px grid |
| `heightControl` | 32 | WinUI button / text box |
| `heightControlCompact` | 24 | WinUI compact density |
| `heightListRow` | 32 | Maintainer reference (file view rows) |
| `heightMenuItem` | 32 | WinUI menu flyout item |
| `heightNavItem` | 36 | WinUI navigation item; revalidated in the navigation pane spec |
| `focusOuter` / `focusInner` | 2 / 1 | WinUI focus visual (primary / secondary thickness) |
| `selectionAccentOpacity` | 0.60 | Maintainer reference |

### Colors (ARGB)

| Token | Light | Dark |
|---|---|---|
| `textPrimary` | `E4000000` | `FFFFFFFF` |
| `textSecondary` | `9E000000` | `C5FFFFFF` |
| `textTertiary` | `72000000` | `87FFFFFF` |
| `textDisabled` | `5C000000` | `5DFFFFFF` |
| `fillControl` | `B3FFFFFF` | `0FFFFFFF` |
| `fillControlHover` | `80F9F9F9` | `15FFFFFF` |
| `fillControlPressed` | `4DF9F9F9` | `08FFFFFF` |
| `fillSubtleHover` | `09000000` | `0FFFFFFF` |
| `fillSubtlePressed` | `06000000` | `0AFFFFFF` |
| `fillCard` | `B3FFFFFF` | `0DFFFFFF` |
| `backgroundBase` | `FFF3F3F3` | `FF202020` |
| `strokeControl` | `0F000000` | `12FFFFFF` |
| `strokeDivider` | `14000000` | `15FFFFFF` |
| `accentFill` | `accentDark1` | `accentLight2` |
| `accentFillHover` | `accentDark1` at 90 % | `accentLight2` at 90 % |
| `accentText` | `accentDark2` | `accentLight3` |
| `textOnAccent` | `FFFFFFFF` | `FF000000` |
| `selectionFill` | `accent` at `selectionAccentOpacity` | same |
| `focusStrokeOuter` | `E4000000` | `FFFFFFFF` |
| `focusStrokeInner` | `B3FFFFFF` | `B3000000` |

"`X` at N %" keeps X's color and sets its alpha to N % of 255 rounded to nearest (90 % →
`E6`, 60 % → `99`), as WinUI applies a brush opacity.

Values follow WinUI 3 theme resources; a later spec that finds a value wrong on a running
system corrects it here (and in the code), not locally.

## Settings

| Key | Type | Default | Spec |
|---|---|---|---|
| `overrides` | array of string | empty | S6, [contracts/settings.md](contracts/settings.md) |
| `diagnosticMode` | bool | `false` | S7 |

## State transitions

- **Settings changed** → parse overrides → new `RuleSet` → re-classify cached windows →
  notify listeners of changed levels → start or stop diagnostic mode.
- **System mode / accent / text scale changed** → `resolveTokens` → swap token snapshot →
  log the new mode, accent and scale → notify token listeners.
- **Late page added to a sheet** → sheet level may drop → notify listeners.
- **Unload** → stop worker thread (overlay, UISettings events) → clear caches and stores.
