# Feature Specification: Foundation

**Feature Branch**: `feat/20261112-174542-foundation`

**Created**: 2026-11-12

**Status**: Approved

**Input**: User description: "Foundation for WholeUI (roadmap item 1, features F10 and F11): source layout split into modules by area; a build script that generates the single wholeui.wh.cpp Windhawk loads (never edited by hand); CI that compiles the generated file with the same compiler and flags Windhawk uses, plus unit tests for pure logic; the design tokens (WinUI/Fluent metrics: Segoe UI Variable with Segoe UI fallback, type ramp, corner radii, spacing, control heights, selection and focus indicators, accent color, light and dark palettes, following system accent, light/dark mode and text scaling); the compatibility list engine (F10) that matches windows by process image, creating module, window class and dialog template ID (never titles), with levels native/visual/full, unknown windows default to native, built-in defaults matching docs/compatibility.md, user overrides in mod settings (S6), fail-safe behavior; and the diagnostic mode (F11, setting S7) that identifies the window under the cursor (process, module, class, template ID, current level) so broken windows can be reported precisely. The mod id is wholeui, injects only into explorer.exe at this stage, and must not change any window's appearance yet: this spec delivers the infrastructure other specs build on. Mod name, description, readme and settings in English and Portuguese (Brazil)."

## Clarifications

### Session 2026-11-12

- Q: How does diagnostic mode show its information? → A: An overlay next to the cursor plus a
  keyboard shortcut that copies the identity as a ready-to-paste override and
  compatibility-list line.
- Q: Which processes can diagnostic mode identify? → A: Every process; fields that cannot be
  read from outside a process show "unknown".
- Q: Which level applies when a dialog hosts pages from another module with a different
  level? → A: The whole window takes the lowest level among host and pages.

## Context

This is the first WholeUI spec. It delivers no visible styling: after it, every window looks
exactly as Windows draws it. It delivers what every later spec depends on: a way to build and
check the mod, one set of design values, a reliable answer to "may WholeUI touch this window,
and how much?", and a way to identify any window precisely when something looks wrong.

Actors:

- **Maintainer**: writes the mod, validates windows and decides their levels.
- **User**: installs the mod in Windhawk, may override a window's level and reports broken
  windows.
- **Later specs**: consume the tokens and the compatibility decision instead of defining their
  own.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Build and check the mod from modular sources (Priority: P1)

The maintainer edits the mod as several source modules split by area, runs one build command
and gets the single mod file Windhawk loads. Every pull request is checked automatically: the
generated file must compile with the same compiler and options Windhawk uses, and the unit
tests for pure logic must pass.

**Why this priority**: nothing else can be written, tested or shipped without it, and the
constitution forbids editing the generated file by hand.

**Independent Test**: run the build on a clean checkout, load the generated file in Windhawk
on a Windows 11 machine and see it compile and load into `explorer.exe` with no change to any
window; open a pull request with a deliberate compile error and see the check fail.

**Acceptance Scenarios**:

1. **Given** a clean checkout, **When** the maintainer runs the build command, **Then** one
   mod file with id `wholeui` is produced, its header states it was generated and must not be
   edited, and Windhawk compiles and loads it.
2. **Given** the same sources, **When** the build runs twice, **Then** both outputs are
   byte-identical.
3. **Given** a pull request, **When** a source change breaks compilation or a unit test,
   **Then** the automatic check fails and names the failing file or test.
4. **Given** the mod is loaded, **When** the maintainer uses File Explorer, the taskbar and
   other `explorer.exe` windows, **Then** nothing looks or behaves differently from Windows
   without the mod.

---

### User Story 2 - Every window gets a level from the compatibility list (Priority: P1)

Whenever a window appears in a process WholeUI runs in, WholeUI decides its level (`native`,
`visual` or `full`) from its stable identity: process image, the module that created it, its
window class and, for dialogs, the dialog template ID. Titles are never used. The decision
comes from the built-in list, which matches `docs/compatibility.md`, and from the user's
overrides in the mod settings. Anything not matched is `native`.

**Why this priority**: this is the safety mechanism behind constitution principles I and II.
Every later feature asks this engine before touching a window, so it must exist and be correct
before any styling is written.

**Independent Test**: with diagnostic mode (Story 3) or the mod log, check the level reported
for File Explorer, the taskbar, an unknown window and a window covered by a user override.

**Acceptance Scenarios**:

1. **Given** a window that matches no rule, **When** it is classified, **Then** its level is
   `native`.
2. **Given** a built-in rule for a window, **When** the user adds an override for the same
   identity, **Then** the override's level is used.
3. **Given** two rules that both match a window, **When** it is classified, **Then** the more
   specific rule wins (the one that names more identity fields), and a user override always
   wins over a built-in rule of equal or lower specificity.
4. **Given** the user changes an override in the settings, **When** the settings are saved,
   **Then** the new level applies to existing and new windows without restarting
   `explorer.exe`.
5. **Given** a malformed override (unknown level, no identity field), **When** settings are
   read, **Then** that entry is ignored, the reason is logged, and all other entries still
   apply.
6. **Given** a shell surface hosted in `explorer.exe` (taskbar, Start, notification area),
   **When** it is classified, **Then** its level is `native` regardless of overrides, because
   shell surfaces are out of scope (constitution, scope).

---

### User Story 3 - Identify the window under the cursor (Priority: P2)

A user or the maintainer turns on diagnostic mode in the mod settings and points at a window.
WholeUI shows the window's identity (process image, creating module, window class, dialog
template ID when it is a dialog) and the level WholeUI assigns to it, along with the rule that
produced the level, so a broken window can be reported or added to the compatibility list
precisely. The information is shown in an overlay next to the cursor, and a keyboard shortcut
copies the identity to the clipboard as a ready-to-paste override and compatibility-list line.
Windows of every process can be identified, not only those WholeUI runs in, so the maintainer
can prepare entries for windows before they are in scope.

**Why this priority**: it is how windows get onto the compatibility list and how users report
problems, but the engine works without it.

**Independent Test**: turn diagnostic mode on, point at File Explorer, the taskbar and a
dialog, and compare the reported identity with what a window inspection tool shows; turn it
off and confirm that nothing of it remains.

**Acceptance Scenarios**:

1. **Given** diagnostic mode is on, **When** the cursor rests on a window, **Then** its process
   image, creating module, class, template ID (or "not a dialog") and level are shown within
   the time budget in SC-005.
2. **Given** diagnostic mode is on, **When** the cursor is over a child control, **Then** both
   the control and the top-level window it belongs to are identified, with the level that
   applies.
3. **Given** a field cannot be determined, **When** the window is identified, **Then** that
   field reads "unknown" instead of a guessed value.
4. **Given** diagnostic mode is on, **When** it is turned off or the mod is disabled, **Then**
   everything it showed disappears at once and no trace remains.
5. **Given** the cursor is over a window of a process WholeUI does not run in, **When** it is
   identified, **Then** it is shown like any other window (for example `sysdm.cpl` or
   `mmsys.cpl`), with "unknown" for fields that cannot be read from outside that process, and
   the level is the one WholeUI would assign if it ran there.
6. **Given** diagnostic mode is on and a window is identified, **When** the user presses the
   copy shortcut, **Then** the clipboard holds the identity as a ready-to-paste override for
   the mod settings and as a line for `docs/compatibility.md`.

---

### User Story 4 - One set of design tokens (Priority: P2)

Every visual value later specs need comes from one set of design tokens based on WinUI /
Fluent: font family (Segoe UI Variable, falling back to Segoe UI), type ramp, corner radii,
spacing, control heights, selection and focus indicators, accent color and its shades, and
light and dark palettes. The tokens follow the system accent color, the light/dark app mode
and the system text size setting, and update when those change.

**Why this priority**: no surface uses the tokens yet, but defining them now keeps later specs
from inventing their own values (constitution III).

**Independent Test**: unit tests resolve the tokens for light and dark mode, several accent
colors and several text scales, and compare them with the expected values; on a running
system the mod log (or diagnostic mode) shows the resolved mode, accent and text scale
changing when the system settings change.

**Acceptance Scenarios**:

1. **Given** the system is in dark app mode, **When** tokens are resolved, **Then** the dark
   palette is used; switching to light mode switches the palette without restarting
   `explorer.exe`.
2. **Given** the user changes the system accent color, **When** tokens are resolved again,
   **Then** the accent and its shades follow the new color.
3. **Given** the system text size is 125%, **When** the type ramp is resolved, **Then** every
   type size is scaled by 1.25 and no other metric (radii, spacing, heights) changes because
   of text size alone.
4. **Given** Segoe UI Variable is not installed, **When** the font token is resolved, **Then**
   Segoe UI is used.

---

### Edge Cases

- A window changes owner, parent or class after creation (for example a dialog whose template
  cannot be read): it keeps the level decided from the identity known at classification, and
  unreadable fields count as not matching.
- A dialog hosts pages created by another module, such as a third-party page inside a
  Properties dialog, and host and page resolve to different levels: the whole window takes the
  lowest level among the host and all its pages, so it is never half styled (FR-013a).
- A child window resolves to a higher level than its top-level window: the child never gets a
  higher level than its top-level window (constitution I, no half-styled windows).
- The settings contain hundreds of overrides: classification still meets SC-004.
- A Windows update changes a class name or template ID: the rule no longer matches and the
  window falls back to `native`, which is safe; the diagnostic mode shows the new identity.
- Reading the system accent, mode or text scale fails: the tokens fall back to the Windows
  defaults (blue accent, light mode, 100%) and the failure is logged.
- The mod is unloaded while windows are classified: all classification state and diagnostic
  display are released; no window is left changed.
- Windhawk shows the mod in a language other than English or Portuguese (Brazil): English is
  shown.

## Requirements *(mandatory)*

### Functional Requirements

**Build and checks**

- **FR-001**: The mod source MUST be split into modules by area (at least: core and logging,
  settings, design tokens, compatibility engine, diagnostic mode).
- **FR-002**: One build command MUST generate the single mod file Windhawk loads, with mod id
  `wholeui`, from the modules. The generated file MUST state in its header that it is
  generated and must not be edited by hand.
- **FR-003**: The build MUST be deterministic: the same sources produce a byte-identical file.
- **FR-004**: Every pull request MUST be checked automatically by compiling the generated file
  with the same compiler and options Windhawk uses for this mod, and by running the unit
  tests. A failure MUST block the merge.
- **FR-005**: Pure logic (token resolution, rule matching and precedence, settings parsing)
  MUST be covered by unit tests that run without a Windows desktop session.
- **FR-006**: The maintainer MUST be able to install the freshly built mod into the local
  Windhawk in one documented step.

**Mod identity and scope**

- **FR-007**: The mod MUST run only in `explorer.exe` in this spec.
- **FR-008**: The mod MUST NOT change the appearance, layout or behavior of any window in this
  spec, and MUST NOT change any system-wide state (constitution I).
- **FR-009**: The mod name, description, readme and settings MUST be available in English and
  Portuguese (Brazil), with English as the fallback.

**Compatibility engine (F10)**

- **FR-010**: The engine MUST identify a window by process image name, the module that created
  the window, the window class and, for dialogs, the dialog template ID. It MUST NOT use window
  titles or any other localized text.
- **FR-011**: The engine MUST assign each window exactly one level: `native`, `visual` or
  `full`. A window that matches no rule MUST be `native`.
- **FR-012**: A rule MAY name any subset of the identity fields (at least one). When several
  rules match, the rule naming more fields wins; at equal specificity a user override wins over
  a built-in rule; remaining ties resolve to the lower level.
- **FR-013**: A child window MUST NOT get a higher level than the top-level window it belongs
  to.
- **FR-013a**: A window that hosts pages or child dialogs created by other modules (for example
  a Properties dialog with third-party pages) MUST take, as a whole, the lowest level among the
  host and every hosted page. The level MUST be decided before the window is first shown,
  using every page known at that point; if a page with a lower level appears later, the whole
  window drops to that level and is restored to how Windows draws it.
- **FR-014**: The built-in rules MUST match `docs/compatibility.md`. Each built-in entry in that
  document records level, Windows build tested and validation date; an entry still marked
  "to do" ships as `native`.
- **FR-015**: Shell surfaces hosted in `explorer.exe` (taskbar, Start, notification area and
  similar) MUST always be `native` and MUST NOT be raised by overrides.
- **FR-016**: Users MUST be able to add, change and remove per-window overrides in the mod
  settings (S6), each made of identity fields and a level. Changes MUST take effect without
  restarting the process.
- **FR-017**: A malformed override MUST be ignored and logged; valid entries still apply.
- **FR-018**: Every failure while identifying or classifying a window (unreadable field, failed
  lookup) MUST result in that window being treated as `native`, and MUST be logged, never shown
  to the user as an error.
- **FR-019**: Later specs MUST be able to ask the engine for a window's level and to be told
  when a window's level changes (for example after an override is edited).

**Diagnostic mode (F11)**

- **FR-020**: A setting (S7) MUST turn diagnostic mode on and off; it is off by default.
- **FR-021**: When on, diagnostic mode MUST identify the window under the cursor and its
  top-level window, in any process: process image, creating module, class, dialog template ID
  or "not a dialog", the assigned level (or the level WholeUI would assign, for processes it
  does not run in) and which rule (built-in, override or default) produced it. Fields that
  cannot be determined MUST read "unknown".
- **FR-021a**: The information MUST be shown in an overlay next to the cursor that follows it
  and never covers the inspected point.
- **FR-021b**: While diagnostic mode is on, a keyboard shortcut MUST copy the identity of the
  window under the cursor to the clipboard in two forms: an override ready to paste into the
  mod settings and a row ready to paste into `docs/compatibility.md`. The shortcut is active
  only while diagnostic mode is on.
- **FR-022**: Diagnostic mode MUST NOT change the window it inspects and MUST NOT take focus or
  intercept input meant for other windows.
- **FR-023**: Turning diagnostic mode off or unloading the mod MUST remove everything it shows
  immediately.

**Design tokens**

- **FR-024**: The mod MUST define one set of design tokens: font family with fallback, type
  ramp, corner radii, spacing, control heights, selection and focus indicators, accent color
  and shades, and light and dark palettes, with values taken from WinUI / Fluent and from the
  maintainer's reference settings in `docs/product.md`.
- **FR-025**: Tokens MUST follow the system light/dark app mode, the system accent color and
  the system text size, and MUST be re-resolved when any of them changes, without a restart.
- **FR-026**: Text size MUST scale only the type ramp; other metrics stay fixed (constitution
  IV: WinUI metrics are not configurable).
- **FR-027**: Tokens MUST be read-only to features: later specs read them and MUST NOT define
  their own values.

**Logging and lifecycle**

- **FR-028**: The mod MUST log startup, settings changes, classification failures and token
  fallbacks with enough detail to diagnose them, and MUST stay quiet in normal operation
  (no log line per window unless diagnostic mode is on).
- **FR-029**: Unloading or disabling the mod MUST release every hook, timer, window and
  resource it created, without restarting `explorer.exe`.

### Key Entities

- **Window identity**: process image name, creating module, window class, dialog template ID
  (optional), and the top-level window it belongs to.
- **Compatibility rule**: a set of identity fields (at least one), a level, and its source
  (built-in or user override). Built-in rules also carry the Windows build tested and the
  validation date, as in `docs/compatibility.md`.
- **Level**: `native` < `visual` < `full`.
- **Classification**: the level decided for one window, plus the rule that produced it.
- **Design token set**: named values (type, geometry, color) resolved for the current mode,
  accent and text scale.
- **Settings**: compatibility overrides (S6) and diagnostic mode (S7). The taste settings S1 to
  S5 are added by the specs that use them.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A clean checkout builds the mod file with one command in under 30 seconds, and
  two builds of the same sources are byte-identical.
- **SC-002**: The automatic check rejects 100% of pull requests whose generated file does not
  compile or whose unit tests fail, and finishes in under 10 minutes.
- **SC-003**: With the mod enabled for a full working session (File Explorer, taskbar, Start,
  context menus, dialogs), the maintainer sees no visual or behavioral difference from Windows
  without the mod, and there are zero `explorer.exe` crashes attributed to the mod.
- **SC-004**: Classifying a window takes under 1 ms on the maintainer's machine, including
  with 200 user overrides, so opening a window is not perceptibly slower.
- **SC-005**: With diagnostic mode on, the identity of the window under the cursor is shown
  within 200 ms of the cursor coming to rest, and moving the cursor stays as smooth as with
  diagnostic mode off.
- **SC-006**: For every window tested during acceptance (File Explorer, taskbar, a Properties
  dialog, a common dialog), the identity shown by diagnostic mode matches what an independent
  window inspection tool reports for process, class and module.
- **SC-007**: Unit tests cover every precedence rule in FR-012 and FR-013, malformed override
  handling in FR-017, and token resolution for light and dark mode, at least three accent
  colors and text scales of 100%, 150% and 225%.
- **SC-008**: Disabling the mod while File Explorer windows are open leaves them working and
  unchanged, with no restart of `explorer.exe`.

## Acceptance on a live system

Steps the maintainer runs on Windows 11 (build recorded in the handoff):

1. Build the mod, install it in Windhawk, confirm it loads into `explorer.exe` only.
2. Use File Explorer, the taskbar, Start, a context menu and a Properties dialog; confirm
   nothing changed (SC-003).
3. Turn diagnostic mode on; point at File Explorer, the taskbar, a Properties dialog, an
   Open/Save dialog and a window of another process (for example `sysdm.cpl`); compare the
   identity with an inspection tool (SC-006); press the copy shortcut and paste the result.
4. Add an override for File Explorer to `visual`, confirm diagnostic mode shows `visual` and
   the override as the source, without restarting `explorer.exe`; remove it and confirm the
   level returns to the built-in one.
5. Add a malformed override; confirm it is ignored and logged.
6. Switch light/dark mode, change the accent color and the text size; confirm the log shows the
   tokens re-resolved.
7. Switch Windhawk's language to Portuguese (Brazil); confirm name, description and settings are
   translated.
8. Disable the mod with Explorer windows open; confirm they stay working (SC-008).

## Assumptions

- The automatic checks run on the project's GitHub repository; the exact compiler version and
  options are taken from the Windhawk release the maintainer uses and recorded in the plan.
- The generated mod file is a build output; how it is published to the Windhawk mods
  repository is out of scope for this spec.
- "Light/dark mode" means the app mode (the setting apps follow), not the system (taskbar)
  mode, because File Explorer follows the app mode.
- "Text size" means the Windows accessibility "Text size" setting; display scaling (DPI) is
  handled by each surface and is not a token concern.
- The built-in list at this stage contains File Explorer (shipped as `native` until the
  Explorer core spec validates it), the shell surfaces of `explorer.exe` as permanent `native`,
  and `sysdm.cpl` as `native`, matching `docs/compatibility.md`.
- Token values come from WinUI / Fluent defaults and the maintainer's reference settings in
  `docs/product.md` (for example 4 px menu hover radius, 8 px list and 16 px grid selection
  radius, 60% accent selection opacity, 32 px file view rows); the plan lists them in full.
- Windhawk supports localized mod name, description, readme and settings; if part of it does
  not, the plan records the gap and English is used there.
- Validated on the maintainer's Windows 11 build; the build number is recorded at acceptance.
