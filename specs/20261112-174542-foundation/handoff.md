# Handoff: Foundation

**Updated**: 2026-11-12 | **Spec**: [spec.md](spec.md) | **Branch**: `feat/20261112-174542-foundation`

## State

Spec approved and committed (`56a6b59`). Plan written: `plan.md`, `research.md`,
`data-model.md`, `contracts/` (engine API, settings, compatibility doc format) and
`quickstart.md`. Not committed yet: waiting for the maintainer's review.

## Done in the last session

- Ran `/speckit-plan`. Inspected the local Windhawk (2.0.0-alpha.6, portable at
  `D:\Programas\Windhawk`): flags from its editor workspace and core DLL, `windhawk-cli` for
  local install, localization syntax from installed mods.

## Decisions

- Build: Python stdlib script `tools/build.py` makes an amalgamation from `src/main.cpp`;
  `--check`, `--compile`, `--install` (via `windhawk-cli mod install --file`) (R4).
- CI and tests: pinned llvm-mingw with Windhawk's flags in `tools/toolchain.json`; fallback is
  compiling through `windhawk-cli` in CI (R1). In-repo C++ test harness, no dependency (R5).
- `@architecture x86-64`, `@include explorer.exe` (R2).
- Localization: `@name:pt-BR`, `$name:pt-BR` in settings; readme holds both languages (R3).
- Engine classifies lazily on first query; creation hooks only record dialog template IDs
  and property sheet pages (R6, R8). Built-in rules generated from `docs/compatibility.md`,
  which gets Class/Template columns and a locked "Shell surfaces" table (R9).
- Overrides are strings: `process=…; module=…; class=…; template=…; level=…` (R10).
- Tokens read from WinRT `UISettings` (accent shades, app mode, text scale) (R11).
- Diagnostic: single instance via mutex, 50 ms polling, click-through D2D layered overlay,
  copy shortcut Ctrl+Alt+Shift+C (fixed) (R14).

- Verified after the plan commit (R1, R11): Windhawk's toolchain is clang 20.1.3,
  mingw-w64 13, UCRT; `windows.ui.viewmanagement.h` is available. The maintainer installed
  Windhawk's dev tools from the UI (`windhawk-cli` cannot). CI uses the public llvm-mingw
  release on LLVM 20.1.3; Windhawk headers and `windhawk.lib` go to `tools/windhawk-sdk/` if
  their license allows (task checks), else a `.def`-generated import lib.

## Open questions

- Still to verify: `$name:pt-BR` honored in settings (R3, acceptance step 10); property
  sheet page capture on a live system (R8, spike branch); license of Windhawk's headers and
  import library (R1, setup task).

## Next step

Plan approved and committed (`f249425`); research update for R1/R11 to be committed.
Run `/speckit-tasks`.
