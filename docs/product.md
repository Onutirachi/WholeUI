# WholeUI — Product

WholeUI replaces this set of Windhawk mods, which the maintainer used together and which
conflict with each other (reference versions in [credits](credits.md)):
File Explorer WinUI-Like Content Animation, Win32 UI Modernizer, Invisible Window Borders,
Control Panel Color Fix, GPU-accelerated explorer.exe list and grid views, Explorer Font
Changer, Translucent Windows, Windows 11 File Explorer Styler, Modernize Folder Picker Dialog
and Fix Darkmode ListViews.

Feature IDs are stable: specs refer to them, and a removed feature keeps its ID.

WholeUI is opinionated (constitution IV): everything below is always on, with one look and no
per-feature switches. Settings only choose between styles that belong to the design.

## Settings

Taste settings (each spec that implements one confirms its values and default):

- S1. Icon color: monochrome glyphs or colored icons, applied to every surface at once.
- S2. Animation speed. Animations have one fixed design, as in Windows; only the speed
  changes. They cannot be turned off, except by the system "Animation effects" setting.
- S3. Font. Default: Segoe UI Variable.
- S4. Window border: default, accent color, or invisible. Rounded corners are always kept and
  thickness is not configurable.
- S5. Backdrop material: Mica, acrylic, or a Luminosity-like effect (see the Taskbar Styler
  "Luminosity" theme as a reference).

Other settings:

- S6. Per-window compatibility overrides (F10).
- S7. Diagnostic mode (F11).

Explicitly not settings: turning off animations, GPU acceleration, WinUI styling or the
styling of validated windows; changing sizes, radii, spacing or border thickness.

## Problems to solve

- P1. The content animation stops working when the GPU file view is on: the GPU view replaces
  the native list the animation was built for.
- P2. Native legacy windows (for example `sysdm.cpl`) end up half dark and half light, with
  unreadable text, because system colors are changed globally. Excluding the process does
  not help.
- P3. Three Mica implementations cover different parts of Explorer and look different.
- P5. The GPU file view and the native navigation pane look and move differently.
- P6. Fonts must be set in two mods to cover all of Explorer.

## Features

### File Explorer

- F01. GPU-accelerated file view (list, details and grid) with smooth scrolling, touchpad
  direct manipulation, hover and selection animations, zebra rows, rounded selection,
  detailed tooltips, gestures (double-click empty space to go up, middle-click opens in a new
  tab, two-finger tap context menu) and column pre-sort and auto-fit.
- F02. Page transition animations (slide and fade, staggered) when navigating, built into the
  GPU file view and the navigation pane, plus tab transitions in property pages.
- F03. GPU-rendered navigation pane with WinUI metrics, selection pill, hover fade and the
  Fluent pin icon, matching the file view's renderer, tokens and timing.
- F04. One icon style for the navigation pane, the file view and every surface WholeUI draws:
  modern shell icons and a modern shortcut overlay. Icons are static; color is setting S1.
  Animated icons are F20.
- F05. One backdrop for the whole Explorer window (header, navigation pane, file view, status
  bar), in the material chosen in S5. The same material is used by every window WholeUI
  styles (F19).
- F06. Segoe UI Variable (WinUI type ramp) across all of Explorer.
- F07. Explorer details: accent-colored disk usage bars, tinted legacy toolbars, status bar
  width, accent colorization.

### Windows and controls

- F10. Compatibility list with per-window levels (`native`, `visual`, `full`), identified by
  process, module, class and dialog template, with user overrides in the settings.
- F11. Diagnostic mode that identifies the window under the cursor and its level.
- F12. Dark mode for Win32 windows at level `full`, scoped per window, never global.
- F13. Modern control appearance at level `visual` or above: rounded buttons, separators,
  focus rectangles, scrollbars, progress bars, tooltips, tab panes, insert marks and
  drag-and-drop visuals.
- F14. WinUI context menus with rounded corners and rounded hover highlight.
- F15. Invisible window borders that keep rounded corners.
- F16. Modern folder picker replacing the classic "Browse For Folder" dialog.
- F17. Dark list views in legacy windows.
- F18. Control Panel header and sidebar colors that match the theme.
- F19. The Explorer backdrop and WinUI styling applied to legacy windows at level `full` (for
  example `mmsys.cpl` with a translucent background, as Translucent Windows does), so as many
  windows as possible share one style.
- F20. Animated icons, written by WholeUI in the style of WinUI's AnimatedIcon: one generic
  motion for every icon first, then special motions for the most visible ones. Starts only
  after the rest of the mod is finished and validated.

## Future ideas (out of scope)

- Taskbar, Start menu, notification center and quick settings. Existing mods cover them well
  today; WholeUI must coexist with them (constitution, technology constraints) and keep the
  door open to adding them later.

## Reference settings

The maintainer's settings for the replaced mods, exported from Windhawk on 2026-11-12, are
the starting point for WholeUI's fixed design values (they become values, not settings). They will be revalidated with WholeUI running. Notable
choices: dark mode on, WinUI context menus with round corners and a 4 px hover radius, WinUI
metrics and a top-aligned pill in the navigation pane, monochrome GPU glyph icons, outline
pin icon, GPU file view rows of 32 px, accent selection at 60% opacity with 8 px (list) and
16 px (grid) radius, content animation of 450 ms over 140 px with fade, motion and stagger.
