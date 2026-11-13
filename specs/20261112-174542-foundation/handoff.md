# Handoff: Foundation

**Updated**: 2026-11-12 (US1, awaiting validation) | **Spec**: [spec.md](spec.md) | **Branch**: `feat/20261112-174542-foundation`

## State

Spec, plan, tasks, Phase 1 and Phase 2 validated and committed. US1 T012–T018 implemented
and checked locally, **not committed** (awaiting the maintainer's validation). T019 is partly
done: the parts that need the maintainer are listed under "Next step".

## Done in the last session

- `tests/tools/test_build.py` (24 tests) with fixtures in `tests/tools/fixtures/amalgamation/`;
  `tools/build.py` (generate, `--check`, `--compile`, `--install`); `tools/test.py`;
  `.github/workflows/ci.yml`. Implementation notes in research.md R4.
- `--compile` appends the header's `@compilerOptions` (as Windhawk does); `mod_libraries`
  removed from `tools/toolchain.json` (tasks T003/T015, plan, research R1 updated).
- Local run (T019): `build.py` writes the file in 3 ms; `--check` identical; `--compile` in
  about 1.2 s with both Windhawk's compiler and the pinned one (`WINDHAWK_ROOT` pointed
  elsewhere); `test.py`: 7/7 C++ and 24/24 Python, with both compilers. SC-001 (< 30 s) met.
- A deliberate error in `src/core/log.cpp` line 20 compiled to
  `src/core/log.cpp:20:56: error: expected expression` (pinned toolchain).
- `--install`: installed as `local@wholeui` 0.1.0, enabled; the DLL is loaded in
  `explorer.exe` only (checked through process modules, non-elevated processes).
- Windhawk's log (mod logging on) shows
  `explorer.exe [WH] [local@wholeui] [200:Write]: [core] WholeUI 0.1.0 loaded`; the
  `[line:function]` prefix is a line of the generated file.
- `@author` changed back to `Onutirachi` (maintainer's choice).
- Python 3.12 (CI's version) is not installed locally; the scripts use nothing newer than 3.9.

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

Finish T019 with the maintainer:

1. Validate and commit US1 (T012–T018).
2. Push the branch, open a pull request, check that CI passes; add branch protection on
   `main` requiring the "Build and test" job.
3. Throwaway pull request with a deliberate compile error: CI fails naming the source file.

Then US2 (T020–T034).
