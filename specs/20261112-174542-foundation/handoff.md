# Handoff: Foundation

**Updated**: 2026-11-12 (Phase 2) | **Spec**: [spec.md](spec.md) | **Branch**: `feat/20261112-174542-foundation`

## State

Spec, plan, tasks, Phase 1 and Phase 2 (T005–T011) validated and committed (Phase 2:
`feat: add mod skeleton, logging, listener list and test harness`).

## Done in the last session

- Phase 2: `src/mod/header.txt`, `readme.md` (English, then pt-BR), `settings.yaml`
  (contracts/settings.md verbatim); `src/core/log.*`; `src/core/subscriptions.h`
  (`ListenerList<Args...>`); `src/main.cpp`; `tests/unit/harness.h`, `main.cpp`,
  `subscriptions_test.cpp`.
- Checked with the pinned toolchain: unit tests 7/7 pass, a deliberate failure exits 1 with
  file, line and test name; a hand-made throwaway amalgamation compiles with Windhawk's flags
  plus `warning_flags`, no warnings (the real `build.py` is T012+).
- Logging contract refined (contracts/engine-api.md): `Info/Warn/Verbose(area, fmt, ...)`,
  plus `SetVerbose`/`IsVerbose`. `Wh_Log`'s own `[line:function]` prefix points to `log.cpp`.
- Conventions: include guards (no `#pragma once`, it warns in the amalgamated main file);
  quoted includes are relative to the including file; `main.cpp` includes each `.h` then its
  `.cpp`.
- Metadata: `@author Carlos Landini`, `@github`/`@homepage` from the repository remote,
  `@license GPL-3.0-only` (installed mods use `@license`; acceptance confirms Windhawk shows it).
- `ListenerList`: a listener removed during a notification is skipped for the rest of that
  notification on the same thread (checked per call), not only from the next one.

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

US1 (T012–T019), stopping after T019 for validation.
