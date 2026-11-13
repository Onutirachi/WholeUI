# Implementation Plan: Foundation

**Branch**: `feat/20261112-174542-foundation` | **Date**: 2026-11-12 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/20261112-174542-foundation/spec.md`

## Summary

Deliver the infrastructure every later spec builds on, with no visible change to any window:
modular C++ sources amalgamated by a Python build script into `build/wholeui.wh.cpp`; CI that
compiles it with Windhawk's toolchain and flags and runs unit tests; design tokens resolved
from WinUI values and the system mode, accent and text size (WinRT `UISettings`); a
compatibility engine that lazily classifies windows by process, creating module, class and
dialog template ID, with built-in rules generated from `docs/compatibility.md`, locked shell
surfaces and string overrides in the settings; and a diagnostic overlay, single-instance and
click-through, that identifies the window under the cursor in any process and copies a
ready-to-paste override and table row. Details and alternatives are in
[research.md](research.md).

## Technical Context

**Language/Version**: C++23 as compiled by Windhawk's bundled llvm-mingw clang (target
`x86_64-w64-mingw32`); Python 3.12+ for the build script.

**Primary Dependencies**: Windhawk API (`Wh_SetFunctionHook`, `Wh_Get*Setting`, `Wh_Log`,
`Wh_ModInit` / `Wh_ModSettingsChanged` / `Wh_ModUninit`); Win32 (`user32`, `comctl32`
hooks, `kernel32`); WinRT ABI `Windows.UI.ViewManagement.UISettings`
(`-lruntimeobject -lole32`); Direct2D and DirectWrite for the overlay (`-ld2d1 -ldwrite`);
GDI for the layered window surface (`-lgdi32`); `GetDpiForMonitor` to size the overlay
for the cursor's monitor (`-lshcore`). No third-party libraries.

**Storage**: none; Windhawk stores the two settings.

**Testing**: in-repo C++ harness for pure logic (rules, overrides parser, tokens, diagnostic
text), compiled by the pinned llvm-mingw; Python `unittest` for the build script; live
acceptance per [quickstart.md](quickstart.md).

**Target Platform**: Windows 11 x64; mod loaded into `explorer.exe` only
(`@include explorer.exe`, `@architecture x86-64`). Maintainer build: 26300 (26H2).
Windhawk 2.0.0-alpha.6.

**Project Type**: single Windhawk mod (one generated `.wh.cpp`) with build tooling.

**Performance Goals**: classification < 1 ms with 200 overrides (SC-004); diagnostic update
< 200 ms after the cursor rests, no input hooks (SC-005); build < 30 s (SC-001); CI < 10 min
(SC-002). Idle cost with diagnostic mode off: no timers, no per-window work.

**Constraints**: no visible change and no system-wide state (FR-008); fail safe to `native`
(FR-018); full release on unload (FR-029); English and pt-BR (FR-009).

**Scale/Scope**: about 10 source modules; a few hundred rules at most.

All unknowns from the handoff are resolved in research. The toolchain (clang 20.1.3,
mingw-w64 13, UCRT) and the WinRT header were verified on the maintainer's machine (R1, R11).
Two facts are confirmed during implementation (marked **Verify** in research):
`$name:pt-BR` in settings (R3) and the property sheet page capture (R8).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Status |
|---|---|---|
| I. Never worse than native | No window is changed; no system-wide calls; every failure → `native` and logged; unload releases hooks, thread, overlay, caches. The only observer hooks are scoped (CBT hook on one thread for one call). | Pass |
| II. Compatibility list | Identity = process, module, class, template; no titles. Built-in rules generated from `docs/compatibility.md`; unknown → `native`; overrides in S6; diagnostic mode S7. | Pass |
| III. One design system | One token set, read-only to features; follows accent, app mode, text size. The overlay uses the tokens. | Pass |
| IV. Opinionated and lean | Only S6 and S7, both planned in `docs/product.md`. Copy shortcut fixed, not a setting. | Pass |
| V. Fluid | No input hooks; overlay timer only while diagnostic mode is on; lazy classification. Budgets stated (SC-004, SC-005). | Pass |
| VI. Original code | No other mod's code was read; research cites Windhawk and Windows docs only. | Pass |
| VII. Spec-driven, tested | Modules by area, generated `.wh.cpp`, CI with Windhawk's compiler, unit tests for pure logic. No undocumented Windows internals (research R16); new tooling dependency is Python (stdlib) only. | Pass |
| Scope: injects only where needed | `explorer.exe` only. Diagnostic mode reads other processes' windows with documented query APIs, never injects. | Pass |
| Coexistence | Shell surfaces are locked `native`; hooks only observe. | Pass |

**Post-design re-check (after Phase 1)**: still passes. One documentation change follows from
the design and is part of this spec's tasks: `docs/compatibility.md` gets separate Class and
Template columns and a "Shell surfaces (always native)" table (research R9).

## Project Structure

### Documentation (this feature)

```text
specs/20261112-174542-foundation/
├── spec.md
├── plan.md               # this file
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── engine-api.md        # C++ interface for later specs
│   ├── settings.md          # settings block and override syntax
│   └── compatibility-doc.md # docs table format, overlay text, clipboard lines
├── checklists/requirements.md
├── handoff.md
└── tasks.md
```

### Source Code (repository root)

```text
src/
├── main.cpp                  # amalgamation root: Wh_ModInit / SettingsChanged / Uninit
├── mod/
│   ├── header.txt            # ==WindhawkMod== metadata (id, names, pt-BR, include, options)
│   ├── readme.md             # English, then Português (Brasil)
│   └── settings.yaml         # contracts/settings.md
├── core/
│   ├── log.h / log.cpp       # Wh_Log wrappers
│   ├── subscriptions.h       # PURE: thread-safe listener list (engine, tokens)
│   └── worker.h / worker.cpp # worker thread with message loop (UISettings, overlay)
├── settings/
│   ├── overrides.h / .cpp    # PURE: override string parser
│   └── settings.h / .cpp     # reads Windhawk settings, builds RuleSet
├── compat/
│   ├── rules.h / .cpp        # PURE: Level, Rule, RuleSet, classify()
│   ├── identity.h / .cpp     # reads identity of an HWND (in- and cross-process)
│   ├── dialog_hooks.h / .cpp # dialog and property sheet hooks, facts store
│   └── engine.h / .cpp       # cache, locking, listeners (contracts/engine-api.md)
├── tokens/
│   ├── tokens.h / .cpp       # PURE: TokenInputs, Tokens, Resolve()
│   ├── system.h / .cpp       # UISettings + DirectWrite font check, change events
│   └── runtime.h / .cpp      # Current() snapshot, change subscriptions
└── diagnostic/
    ├── format.h / .cpp       # PURE: overlay text, override line, table row
    └── overlay.h / .cpp      # single-instance mutex, timer, D2D layered window, clipboard

tests/
├── unit/
│   ├── harness.h
│   ├── main.cpp
│   ├── subscriptions_test.cpp
│   ├── rules_test.cpp        # FR-012, FR-013, FR-013a, FR-015, unknown fields, SC-004 timing
│   ├── overrides_test.cpp    # syntax and every FR-017 error
│   ├── tokens_test.cpp       # light/dark, 3 accents, 100/150/225 %, font fallback
│   └── format_test.cpp       # overlay text, clipboard lines
└── tools/
    ├── test_build.py         # amalgamation, determinism, diagnostic mapping, compatibility.md parser
    └── fixtures/             # amalgamation/ and compat/ inputs

tools/
├── build.py                  # generate, --check, --compile (maps errors to src/), --install
├── test.py                   # build and run C++ tests, run Python tests
├── setup_toolchain.py        # fetch pinned llvm-mingw (+ windhawk_api.h) into build/toolchain
├── toolchain.json            # pinned compiler version, URL, SHA-256, flags
└── windhawk-sdk/             # Windhawk headers + windhawk.lib for CI, if their license allows (R1)

.github/workflows/ci.yml      # windows-latest: setup toolchain, --check, --compile, test.py

docs/compatibility.md         # new table format (contracts/compatibility-doc.md)
build/                        # git-ignored: wholeui.wh.cpp, gen/, toolchain/, tests binary
```

**Structure Decision**: one source tree split by area; files marked PURE include only the
C++ standard library and are compiled both into the mod and into the unit tests. Windows-
dependent files are thin adapters around them. The amalgamation inlines project includes in
the order `src/main.cpp` includes them.

## Implementation order (for /speckit-tasks)

1. Toolchain verification (research R1) and `tools/toolchain.json`; `build.py` with the
   header, readme, settings and an empty `Wh_ModInit`; `--check`, `--compile`, `--install`;
   CI. (Story 1, can ship alone.)
2. Pure modules with tests: rules, overrides parser, tokens, format.
3. `docs/compatibility.md` new format and generated built-in rules.
4. Identity reading, engine with cache and listeners, settings wiring.
5. Spike branch for dialog and property sheet capture (R8), then `dialog_hooks`.
6. Token system adapter (UISettings, font check), worker thread.
7. Diagnostic overlay and copy shortcut.
8. Live acceptance (quickstart), handoff with Windows build and results.

## Complexity Tracking

No constitution violations to justify.
