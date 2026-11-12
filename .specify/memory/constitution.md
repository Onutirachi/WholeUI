<!--
Sync Impact Report
- Version: none -> 1.0.0 (initial ratification)
- Added sections: Core Principles I-VII (IV: opinionated and lean; VI: original code, upstream as reference), Technology and Product Constraints, Licensing,
  Development Workflow, Governance.
- Templates: plan-template.md, spec-template.md, tasks-template.md need no change.
- Source: maintainer decisions, 2026-11-12.
-->

# WholeUI Constitution

WholeUI is a single Windhawk mod that gives Windows 11 one coherent, fluid interface. It
replaces a stack of separate mods (GPU-accelerated file views, content animations, Mica and
translucency, fonts, Win32 control modernization, dark mode fixes, borderless windows, modern
folder picker) that each patch part of the screen and fight each other. File Explorer comes
first; other windows are added one at a time as they are validated.

## Core Principles

### I. Never Worse Than Native

- A window is either fully handled by WholeUI or left exactly as Windows draws it. A window
  MUST NOT end up half styled (for example dark controls with light ones, or dark text on a
  dark background).
- WholeUI MUST NOT change system-wide state that other programs read: no `SetSysColors`, no
  global `SystemParametersInfo` changes, no theme or registry changes outside the mod's own
  settings. Colors, fonts and metrics are changed only for the windows WholeUI handles,
  through hooks that check which window is asking.
- Geometry (sizes, heights, spacing, font size) is changed only on surfaces WholeUI lays out
  and draws itself. On legacy dialogs and controls laid out by their owner, only appearance
  changes (colors, corners, hover and focus states, font face at the same size).
- Every feature fails safe: if a hook, symbol or Windows version check fails, that feature
  turns off and the original behavior remains. A failure is logged, never shown as a broken
  window.
- Unloading or disabling the mod MUST restore every window it touched without a restart of
  the affected process where Windows allows it.

Rationale: the mods WholeUI replaces broke native windows such as `sysdm.cpl` because they
changed global colors and metrics; an exclusion list that cannot exclude is not an exclusion.

### II. The Compatibility List Is the Source of Truth

- Every window WholeUI may touch is matched by stable identity: the process image, the module
  that created the window or dialog, the window class and, for dialogs, the template ID.
  Window titles are never used, because they change with the system language.
- Each entry has a level: `native` (untouched), `visual` (appearance only, per principle I)
  or `full` (dark mode, backdrop, geometry where WholeUI draws). Unknown windows default to
  `native`. Promoting a window to a higher level requires the maintainer's validation on a
  running system.
- The list lives in `docs/compatibility.md` with the level, the Windows build tested and the
  date of validation. Built-in defaults in the mod match that document. Users can override a
  level in the mod settings.
- WholeUI ships a diagnostic mode that identifies the window under the cursor (process,
  module, class, template ID, current level) so a broken window can be reported precisely.

Rationale: validating windows one by one is the only reliable way to modernize Win32, and it
needs a precise identity and a record.

### III. One Design System

- All visual values come from one set of design tokens based on WinUI / Fluent metrics:
  font (Segoe UI Variable, falling back to Segoe UI), type sizes, corner radii, spacing,
  control heights, selection and focus indicators, accent color, light and dark palettes.
  Features read tokens; they do not define their own values.
- Surfaces that sit side by side (the file view, the navigation pane, the header area) MUST
  use the same renderer, tokens and animation timing, so they look and move as one.
- Icons follow one style everywhere: the same icon set and treatment in the navigation pane,
  the file view and every other surface WholeUI draws. There is no per-surface icon choice.
- The same backdrop material and WinUI styling are applied to as many windows as possible, so legacy
  windows (for example `mmsys.cpl`) look like the rest of the system once they are validated
  at level `full`.
- The system accent color, light/dark mode and text scaling settings are followed.

Rationale: the previous setup mixed a GPU file view with a non-GPU navigation pane and three
different Mica implementations, and the seams were visible.

### IV. Opinionated and Lean

- WholeUI has one look and one behavior. The WinUI design (metrics, rounded corners,
  typography, icons, backdrop), GPU acceleration of Explorer and the animations are the
  product, not options: they cannot be turned off one by one.
- Settings are for taste, not for turning the product off. A setting may choose between
  styles that are all part of the design (icon color, animation speed, font, window border
  style, backdrop material) or handle a compatibility exception. Animations have one fixed
  design, as in Windows itself; only their speed is a setting. A setting MUST NOT disable
  the WinUI styling, the GPU acceleration, the animations or the styling of validated
  windows, and MUST NOT change WinUI metrics (sizes, radii, spacing, border thickness).
- Every setting MUST be justified in the spec that adds it, with its default. When in doubt,
  there is no setting.
- Features of the replaced mods are reproduced only when they serve the intended look and
  behavior. Variants, alternative styles and toggles that exist in those mods are dropped.
- Less code is a goal: a feature that can be removed without losing the intended experience
  is removed.

Rationale: the replaced mods add up to more than 100,000 lines, mostly options and variants;
a single opinionated design is smaller, more consistent and easier to keep working.

### V. Fluid, Not Fragile

- Rendering of WholeUI-drawn surfaces is GPU-accelerated and MUST keep input responsive:
  scrolling, hover, selection and navigation never wait on animations, thumbnails or file
  system calls.
- Animations follow WinUI timing and easing. They are turned off only by the system
  "Animation effects" setting; WholeUI has no switch of its own.
- Every spec that adds a rendered surface or animation states a performance budget (frame
  time, time to first paint after navigation, memory) and how it is measured.

Rationale: smoothness is the main reason to use the mod; a pretty but stuttering Explorer is
a regression.

### VI. Original Code, Upstream as Reference

- All WholeUI code is written for this project. No code is copied or translated from other
  mods, including small snippets.
- Other mods are studied as references: what they do, how it looks and feels, which settings
  they offer, which Windows functions, classes and internals they work with, and what went
  wrong with them. Those facts and ideas may guide the design.
- Research on another mod is recorded in prose in the spec's `research.md` (behavior,
  technique, the Windows APIs involved), never as copied code. Implementation is written from
  the spec, the plan and Windows documentation, not from the other mod's source.
- `docs/credits.md` names every mod that inspired a feature, with its author, so the
  community whose ideas WholeUI builds on is acknowledged.

Rationale: writing everything from scratch keeps the licensing simple and the code coherent,
since each upstream mod was written with different conventions and assumptions.

### VII. Spec-Driven, Tested and Readable

- Every feature starts from a spec under `specs/` with user scenarios and acceptance
  criteria, followed by a plan and tasks. No implementation code is written before the
  maintainer approves the spec.
- A feature is done only after the maintainer accepts it on a running system. Acceptance
  steps that need a live Windows desktop are listed in the spec.
- The source is split into modules by area; a build script generates the single `.wh.cpp`
  file Windhawk loads. The generated file is never edited by hand.
- CI compiles the generated file with the same compiler and flags Windhawk uses. Pure logic
  (tokens, rules matching, settings parsing, layout math) has unit tests.
- Experiments (spikes) live on their own branches. Their findings are recorded in the spec or
  plan; their code is not merged unless a spec adopts it.
- Build the smallest thing that satisfies the spec. Readable code over clever code. Every new
  dependency or Windows internal (undocumented function, symbol, class) MUST be justified in
  the plan, with the fallback when it is missing.

Rationale: the mod hooks undocumented Windows internals that change with updates; small,
documented pieces are what keep it maintainable by people and agents starting from a fresh
context.

## Technology and Product Constraints

- Platform: Windows 11, on the releases Microsoft currently supports. Each spec states the
  builds it was validated on. Windows 10 is out of scope.
- Language and toolchain: C++ as compiled by Windhawk (its bundled Clang/LLVM-MinGW), using
  the Windhawk API. Delivered as one Windhawk mod with the id `wholeui`.
- Rendering: Direct3D 11, Direct2D, DirectWrite and DirectComposition for WholeUI-drawn
  surfaces. GDI and UxTheme hooks only for appearance changes on native controls.
- Processes: the mod injects only where the compatibility list needs it, starting with
  `explorer.exe`. It never injects into `dwm.exe`, games or critical system processes.
- Scope: File Explorer and application windows (Win32 dialogs, Control Panel applets,
  common dialogs). Shell surfaces (taskbar, Start, notification center, quick settings) are
  out of scope until an amendment adds them, but no design may preclude adding them later as
  separate modules.
- Coexistence: WholeUI MUST work alongside mods for the surfaces it does not cover (for
  example taskbar and Start menu stylers). In `explorer.exe`, which also hosts the taskbar,
  every hook checks that the window belongs to WholeUI's scope before changing anything.
  Mods that WholeUI replaces are expected to be disabled; running both is not supported.
- Settings: as few as possible (principle IV). Planned so far: icon color, animation speed,
  font, window border style, backdrop material, compatibility overrides and the diagnostic
  mode. The list in `docs/product.md` is the reference.
- Internationalization: mod name, description, readme and settings in English and
  Portuguese (Brazil), English as fallback.

## Licensing

- WholeUI is licensed under GPL-3.0-only.
- Because no upstream code is reused (principle VI), the license of the mods that inspired
  WholeUI does not constrain it. Third-party libraries, if any are added, MUST be compatible
  with GPL-3.0 and justified in the plan.

## Development Workflow

- All code, specs, commits, documentation and code comments are written in English.
- Every commit MUST follow Conventional Commits. Commits MUST NOT include `Co-Authored-By`
  or any AI attribution trailer.
- Branches: `feat/<timestamp>-<slug>` for spec work; `fix/`, `docs/`, `chore/`, `refactor/`
  and `test/` for other work. Nobody commits directly to `main`.
- Nothing is committed before the maintainer validates the change, and nothing is pushed
  unless the maintainer asks. Agents never push on their own.
- Corrections made before merging are folded into the branch history (amend or fixup);
  reverts are only for changes already on `main`.
- Changes land on `main` through pull requests merged with squash, after CI passes.
- When a decision changes during implementation, the spec is updated first, then the code.
- Each spec folder is self-contained so work can continue in a fresh conversation or with a
  different agent: `spec.md`, `plan.md`, `tasks.md`, and a `handoff.md` updated at the end of
  every session with the current state, decisions taken and the next step.

## Governance

- This constitution supersedes any other practice or guideline in the project.
- Amendments are made by pull request that states the rationale and bumps the version: MAJOR
  for removing or redefining a principle, MINOR for adding a principle or materially expanding
  guidance, PATCH for clarifications and wording.
- Every plan MUST include a constitution check, and every pull request review MUST verify
  compliance. Deviations MUST be documented in the plan with a justification.

**Version**: 1.0.0 | **Ratified**: pending maintainer approval | **Last Amended**: 2026-11-12
