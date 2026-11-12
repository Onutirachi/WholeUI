# Handoff: Foundation

**Updated**: 2026-11-12 | **Spec**: [spec.md](spec.md) | **Branch**: `feat/20261112-174542-foundation`

## State

Spec (`56a6b59`), plan (`066fa0a`) and tasks approved and committed. `/speckit-analyze` ran:
no critical issues; its findings were applied to `tasks.md` and `plan.md` before the commit.
Phase 1 (T001–T004) validated and committed (`build: pin Windhawk toolchain, flags and SDK files`).

## Done in the last session

- Ran `/speckit-tasks` (54 tasks) and `/speckit-analyze`. Applied: verbose log switch wired
  with the settings in T031 so US2 can be checked before the overlay; `--compile` maps compiler
  errors back to `src/<path>:<line>` (no `#line` in the generated file); branch protection
  folded into T019; tests for `subscriptions.h`; `src/tokens/runtime.*` named; the copy
  shortcut is observed, not swallowed (readme says so).
- Phase 1: source folders; `tools/windhawk-sdk/` (Windhawk 2.0.0-alpha.6 headers and
  `windhawk.lib`, GPL-3.0, byte-identical, hashes in its README); `tools/toolchain.json`;
  `tools/setup_toolchain.py` (download or reuse, SHA-256, in-place extraction with a completion
  marker, `clang --version` check). R1 corrected: Windhawk uses llvm-mingw 20250417
  **msvcrt-i686** (not UCRT/x86_64) with three installer patches; real compile flags taken from
  windhawk-core `flags.rs` (`-O2 -ffp-exception-behavior=maytrap -s`, no `-Wall`). A probe mod
  compiled cleanly with the pinned toolchain and flags.

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
  mingw-w64 13, llvm-mingw 20250417 msvcrt-i686; `windows.ui.viewmanagement.h` is available.
  The maintainer installed Windhawk's dev tools from the UI (`windhawk-cli` cannot). CI uses
  that same public release, pinned in `tools/toolchain.json`; Windhawk headers and
  `windhawk.lib` are vendored in `tools/windhawk-sdk/` (GPL-3.0, no `.def` route).

## Open questions

- Still to verify: `$name:pt-BR` honored in settings (R3, acceptance step 10); property
  sheet page capture on a live system (R8, spike branch).

## Next step

Phase 2 (T005–T011), then US1 (T012–T019), stopping after T019 for validation.
