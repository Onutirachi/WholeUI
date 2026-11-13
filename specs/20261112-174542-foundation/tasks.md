---

description: "Task list for the Foundation spec"
---

# Tasks: Foundation

**Input**: Design documents from `specs/20261112-174542-foundation/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md),
[data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: requested by the spec (FR-005, SC-007). Pure logic gets C++ unit tests in
`tests/unit/`; the build script gets Python `unittest` tests in `tests/tools/`. Within a story,
tests are written first and must fail before the code they cover exists.

**Organization**: tasks are grouped by user story. US1 (build and CI) is the base every other
story is built and checked with; US2, US3 and US4 are then independent except where noted.

**Rules that apply to every task** (constitution, AGENTS.md):

- No implementation task starts before the maintainer explicitly confirms implementation.
- Nothing is committed before the maintainer validates it; nothing is pushed unless asked.
- No code is copied or translated from other mods (constitution VI).
- No window is changed and no system-wide state is touched (FR-008). Every failure → `native`
  and a log line (FR-018).
- Files marked **PURE** include only the C++ standard library (no `windows.h`, no Windhawk
  API) so they compile into both the mod and the unit tests.
- `build/wholeui.wh.cpp` is never edited by hand.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on an unfinished task)
- **[Story]**: US1 to US4 from [spec.md](spec.md)

---

## Phase 1: Setup (toolchain and repository layout)

**Purpose**: the pinned compiler and Windhawk files that CI and local checks need (research R1).

- [X] T001 Create the source and tool folders from plan.md "Project Structure" (`src/mod/`, `src/core/`, `src/settings/`, `src/compat/`, `src/tokens/`, `src/diagnostic/`, `tests/unit/`, `tests/tools/`, `tools/`, `.github/workflows/`); `build/` stays git-ignored (already in `.gitignore`)
- [X] T002 Check the license of Windhawk's `windhawk_api.h`, `windhawk_api_internal.h`, `windhawk_utils.h`, `windhawk_tool_mod.h` and `windhawk.lib` in the Windhawk repository. If GPL-3.0-compatible, copy them from `D:\Programas\Windhawk\Compiler\include\` and `Engine\2.0.0_1\64\windhawk.lib` into `tools/windhawk-sdk/` with `tools/windhawk-sdk/README.md` naming the Windhawk version (2.0.0-alpha.6) and license; otherwise write `tools/windhawk-sdk/windhawk.def` listing the `Wh_*` exports and record the header tag to fetch. Record the outcome in research.md R1 (replaces the open question in handoff.md)
- [X] T003 Pick the public llvm-mingw release built on LLVM 20.1.3 (done: `20250417` msvcrt-i686, the one Windhawk uses — research R1 "Setup findings"), confirm `clang --version` prints `20.1.3`, and write `tools/toolchain.json`: release URL, SHA-256, target `x86_64-w64-mingw32`, the full Windhawk flag list from research.md R1 (editor flags + `-Wno-pragma-pack -Wno-pragma-system-header-outside-header` + compile-only flags `-DWH_MOD_ID=L"wholeui"`, `-DWH_MOD_VERSION`, `-mguard=cf-nochecks`, `-Wno-unneeded-internal-declaration`, `-Wno-deprecated-declarations`, `-Wl,--export-all-symbols`, `-shared`), and the mod libraries (`-lruntimeobject -lole32 -ld2d1 -ldwrite -lgdi32 -lcomctl32 -lpsapi`; later moved to `@compilerOptions` only, see T015)
- [X] T004 Write `tools/setup_toolchain.py` (Python stdlib only): read `tools/toolchain.json`, download the archive into `build/toolchain/`, verify SHA-256, extract, skip when already present, and (if T002 chose the `.def` route) generate `windhawk.lib` with `llvm-dlltool` and fetch the headers at the recorded tag

---

## Phase 2: Foundational (shared by all stories)

**Purpose**: the minimal mod and test skeleton every story builds on.

**⚠️ CRITICAL**: no user story work starts before this phase is complete.

- [X] T005 [P] Write `src/mod/header.txt`: the `// ==WindhawkMod==` block with `@id wholeui`, `@name WholeUI`, `@name:pt-BR`, `@description` and `@description:pt-BR` (research R3), `@version 0.1.0`, `@author`, `@github`, `@include explorer.exe`, `@architecture x86-64` (R2), `@compilerOptions` with the mod libraries from T003, and `@license GPL-3.0-only` if Windhawk accepts that key (otherwise state it in the readme)
- [X] T006 [P] Write `src/mod/readme.md`: what WholeUI is, that this version changes no window, how diagnostic mode and overrides work (including that Ctrl+Alt+Shift+C also reaches the app under the cursor); English first, then a "Português (Brasil)" section with the same content (research R3)
- [X] T007 [P] Write `src/mod/settings.yaml` exactly as in [contracts/settings.md](contracts/settings.md): `overrides: [""]` and `diagnosticMode: false`, each with `$name`, `$name:pt-BR`, `$description`, `$description:pt-BR`
- [X] T008 [P] Implement `src/core/log.h` / `src/core/log.cpp` per [contracts/engine-api.md](contracts/engine-api.md) "Logging": `wholeui::log::Info`, `Warn`, `Verbose(const wchar_t* fmt, ...)`, each forwarding to `Wh_Log` with an `[area]` prefix; `Verbose` writes only while a flag `log::SetVerbose(bool)` is on (set by diagnostic mode, FR-028)
- [X] T009 [P] Implement `src/core/subscriptions.h` (**PURE**): `SubscriptionId` and a small thread-safe listener list template (add, remove, snapshot-then-call outside the lock) used by the engine (FR-019) and the tokens (contracts/engine-api.md), with `tests/unit/subscriptions_test.cpp` (add, remove, remove from inside a callback, callbacks run outside the lock on a snapshot); the test runs once T017 exists
- [X] T010 Write `src/main.cpp`: includes the modules, `Wh_ModInit` logs "WholeUI <version> loaded" and returns `TRUE`, `Wh_ModSettingsChanged` and `Wh_ModUninit` are empty for now; the order of `#include "…"` lines is the amalgamation order (plan "Structure Decision")
- [X] T011 [P] Write `tests/unit/harness.h` (about 80 lines, research R5): `TEST(name)` registration, `CHECK(expr)`, `CHECK_EQ(a, b)`, failure prints file, line and test name, and `tests/unit/main.cpp` that runs every registered test, prints a summary and exits non-zero on any failure

**Checkpoint**: a skeleton mod and test runner exist; nothing builds them yet.

---

## Phase 3: User Story 1 - Build and check the mod from modular sources (Priority: P1) 🎯 MVP

**Goal**: one command makes `build/wholeui.wh.cpp` from the modules; CI compiles it with
Windhawk's compiler and flags and runs the unit tests on every pull request.

**Independent Test**: on a clean checkout run `python tools/build.py`, install with
`--install`, see Windhawk load it into `explorer.exe` with no visible change; open a pull
request with a deliberate compile error and see CI fail naming the source file.

### Tests for User Story 1

- [X] T012 [US1] Write `tests/tools/test_build.py` (`unittest`) for the amalgamation, against fixture sources in `tests/tools/fixtures/amalgamation/`: project `#include "…"` inlined once and dropped on repeat, `#include <…>` kept, `// ---- src/<path> ----` marker before each inlined file, metadata/readme/settings blocks first between Windhawk's markers, "generated, do not edit" header, LF endings, UTF-8 without BOM, two runs byte-identical (FR-002, FR-003); a missing include fails with the including file and line; a compiler diagnostic at a line of the generated file is mapped back to `src/<path>:<line>` through the markers (US1 scenario 3)

### Implementation for User Story 1

- [X] T013 [US1] Implement generation in `tools/build.py` (Python stdlib only, research R4): read `src/mod/header.txt`, `readme.md`, `settings.yaml`, write the "generated from src/ by tools/build.py — do not edit" comment, the `==WindhawkMod==`, `==WindhawkModReadme==` and `==WindhawkModSettings==` blocks, then the amalgamation from `src/main.cpp`; output `build/wholeui.wh.cpp`; no timestamps; make T012 pass
- [X] T014 [US1] Add `--check` to `tools/build.py`: build twice into memory, compare bytes, print "identical" or fail with the first differing offset (FR-003, SC-001)
- [X] T015 [US1] Add `--compile` to `tools/build.py`: compile `build/wholeui.wh.cpp` into `build/wholeui.dll` with the flags from `tools/toolchain.json` followed by the mod's `@compilerOptions` from `src/mod/header.txt`, as Windhawk does (the libraries live only there; `mod_libraries` was removed from `toolchain.json`); compiler from `$WINDHAWK_ROOT\Compiler` (default `D:\Programas\Windhawk`) when present, else `build/toolchain/`; Windhawk headers and lib from the same place or `tools/windhawk-sdk/`; rewrite every `build/wholeui.wh.cpp:<line>` in the compiler output to `src/<path>:<line>` using the markers (the generated file itself stays free of `#line`), so CI names the failing source file; non-zero exit on failure (US1 scenario 3, SC-002)
- [X] T016 [US1] Add `--install` to `tools/build.py`: run `<WINDHAWK_ROOT>\windhawk-cli.exe mod install --file build/wholeui.wh.cpp`; path from `WINDHAWK_ROOT` or the default portable path, never hard-coded in sources (FR-006)
- [X] T017 [US1] Write `tools/test.py`: compile `tests/unit/*.cpp` plus the **PURE** sources under `src/` (each `.cpp` whose first line starts with `// PURE:`) with the pinned toolchain and the Windhawk warning flags (without `-include windhawk_api.h` and `-DWH_MOD`) into `build/tests/unit_tests.exe`, run it, then run `python -m unittest discover tests/tools`; exit non-zero if either fails, naming the failing test (FR-005)
- [X] T018 [US1] Write `.github/workflows/ci.yml`: on `pull_request` and push to `main`, `windows-latest`, setup Python 3.12, cache `build/toolchain/` by the SHA in `tools/toolchain.json`, run `python tools/setup_toolchain.py`, `python tools/build.py --check`, `python tools/build.py --compile`, `python tools/test.py`; timeout 10 minutes (FR-004, SC-002)
- [X] T019 [US1] Run the whole chain locally (`build.py`, `--check`, `--compile`, `test.py`, `--install`), confirm in Windhawk that the mod loads into `explorer.exe` only and logs its startup line, and note the build time (SC-001 < 30 s) in handoff.md; with the maintainer, add branch protection on `main` requiring the CI job and check that a throwaway pull request with a deliberate compile error fails naming the source file (FR-004, US1 scenario 3)

**Checkpoint**: US1 complete; CI blocks merges into `main`.

---

## Phase 4: User Story 2 - Every window gets a level from the compatibility list (Priority: P1)

**Goal**: `compat::Classify(hwnd)` returns `native`/`visual`/`full` from process, module,
class and template, with built-in rules generated from `docs/compatibility.md`, locked shell
surfaces, user overrides (S6) and fail-safe `native`.

**Independent Test**: unit tests cover every precedence rule; on the live system the mod log
(or diagnostic mode, US3) shows the level of File Explorer, the taskbar, an unknown window and
an overridden window, and an override change applies without restarting `explorer.exe`.

### Tests for User Story 2

- [X] T020 [P] [US2] Write `tests/unit/rules_test.cpp` against the `classify` contract in [data-model.md](data-model.md) "Resolution order": no match → `Native`/`Default`; more named fields wins; equal specificity → `Override` beats `BuiltIn`; remaining tie → lower level; an `Unknown` field never matches a rule naming it; `NotADialog` never matches a template rule; case-insensitive process/module/class/template; locked rule on the top-level → `Native`/`Locked` even with a higher override; child capped by top-level → `CappedByTopLevel`; sheet with a lower page → `LoweredByPage`; failure → `Native`/`Failure` (FR-011 to FR-013a, FR-015, FR-018, SC-007)
- [X] T021 [P] [US2] Add a timing test to `tests/unit/rules_test.cpp`: 200 overrides + the built-ins, average `classify` time over 10,000 calls printed (not asserted), SC-004 budget < 1 ms
- [X] T022 [P] [US2] Write `tests/unit/overrides_test.cpp` for every case in [contracts/settings.md](contracts/settings.md): the three valid examples; empty string skipped silently; each error reason verbatim — `missing level`, `unknown level`, `no identity field`, `unknown field "<name>"`, `duplicate field "<name>"`, `empty value for "<name>"`, `path not allowed in "<name>"`, `malformed field "<text>"`; whitespace trimming; case-insensitive `level`; one bad entry does not drop the others (FR-017)
- [X] T023 [P] [US2] Add compatibility-doc parser tests to `tests/tools/test_build.py` with fixtures in `tests/tools/fixtures/compat/`: `## Windows` and `## Shell surfaces (always native)` tables per [contracts/compatibility-doc.md](contracts/compatibility-doc.md); `—` and `-` mean not named; backticks stripped; `to do` → `native`; shell rows → locked `native`; a row naming no identity field, an unknown level or a wrong column count fails with the line number; generated `build/gen/builtin_rules.inc` is byte-identical across runs and in document order

### Implementation for User Story 2

- [X] T024 [US2] Rewrite the table in `docs/compatibility.md` to the format in [contracts/compatibility-doc.md](contracts/compatibility-doc.md): `## Windows` with separate Class and Template columns (File Explorer: `explorer.exe`, class `CabinetWClass`, `to do`; System Properties: module `sysdm.cpl`, `native`) and `## Shell surfaces (always native)` with the eight rows from research R9 (`Shell_TrayWnd`, `Shell_SecondaryTrayWnd`, `NotifyIconOverflowWindow`, `TopLevelWindowForOverflowXamlIsland`, `XamlExplorerHostIslandWindow`, `Progman`, `WorkerW`, `Shell_InputSwitchTopLevelWindow`, process `explorer.exe`)
- [X] T025 [US2] Implement the compatibility-doc parser and `build/gen/builtin_rules.inc` generation in `tools/build.py`; `src/compat/rules.cpp` includes `builtin_rules.inc`, and the amalgamation and `tools/test.py` resolve it from `build/gen/` (generated before compiling); make T023 pass
- [X] T026 [US2] Implement `src/compat/rules.h` / `src/compat/rules.cpp` (**PURE**): `Level`, `Field` (value or `Unknown`), `TemplateField` (value, `Unknown`, `NotADialog`), `WindowIdentity`, `Rule` (`process`, `module`, `windowClass`, `templateId` optional, at least one present; `level`; `source` `BuiltIn`/`Override`; `locked`; `origin`), `RuleSet` (immutable: built-ins + overrides + `version`), `Reason`, `Classification`, `classify(identity, topLevel, pages, ruleSet)` following data-model.md steps 1–5, and `BuiltInRules()` from the generated include; make T020 and T021 pass
- [X] T027 [US2] Implement `src/settings/overrides.h` / `src/settings/overrides.cpp` (**PURE**): `ParseOverride(index, text) → Rule | OverrideError { index, text, reason }` per the grammar in contracts/settings.md; flag overrides whose identity matches a locked rule so the caller logs `override <index> has no effect: shell surface`; make T022 pass
- [X] T028 [US2] Implement `src/settings/settings.h` / `src/settings/settings.cpp`: read `overrides[i]` with `Wh_GetStringSetting` until 16 empty results in a row (contracts/settings.md), parse each, log `override <index> ignored: <reason>: "<text>"` with `log::Warn`, read `diagnosticMode` with `Wh_GetIntSetting`, build a new `RuleSet` with an incremented version
- [X] T029 [US2] Implement `src/compat/identity.h` / `src/compat/identity.cpp` per research R7: process image (in-process `GetModuleFileNameW(nullptr)`; other process `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)` + `QueryFullProcessImageNameW`), creating module (`GWLP_HINSTANCE` → `GetModuleFileNameW`, or `GetModuleFileNameExW` with `PROCESS_QUERY_INFORMATION | PROCESS_VM_READ`), class (`GetClassNameW`), template `NotADialog` when the class is not `#32770` and `Unknown` otherwise (T034 replaces `Unknown` with the template from the dialog facts store); file names lower-cased without path; any failed read → `Unknown`; top-level via `GetAncestor(GA_ROOT)`; never reads titles (FR-010)
- [X] T030 [US2] Implement `src/compat/engine.h` / `src/compat/engine.cpp` per [contracts/engine-api.md](contracts/engine-api.md): `Classify`, `LevelOf`, `SubscribeLevelChanged`, `Unsubscribe`; `SRWLOCK`-guarded `RuleSet` snapshot and cache `HWND → { Classification, threadId, classAtom, ruleSetVersion }`; recompute when thread ID or class atom differ (HWND reuse); sweep dead windows with `IsWindow` when the cache passes 4,096 entries or on a settings change; `SetRuleSet(newSet)` re-classifies cached windows and calls listeners for each changed level after releasing the lock; every exception or failed read → `Native`/`Failure` with one `log::Warn` (research R6, R13, FR-018, FR-019)
- [X] T031 [US2] Wire settings into `src/main.cpp`: `Wh_ModInit` builds the first `RuleSet`, logs the counts of built-in and override rules and sets `log::SetVerbose(diagnosticMode)` so per-window lines can be checked before the overlay exists (FR-028); `Wh_ModSettingsChanged` rebuilds it, updates `log::SetVerbose` and calls `engine::SetRuleSet` (US2 scenario 4, FR-016); `Wh_ModUninit` clears the engine cache and listeners (FR-029)
- [X] T032 [US2] Spike on branch `spike/20261112-dialog-capture` (not merged, research R8 **Verify**): hook `CreateDialogParamW`, `DialogBoxParamW`, their `Indirect` variants, `CreatePropertySheetPageW` and `PropertySheetW` with a thread-local `WH_CBT` hook; open a file's Properties with a third-party shell extension page and a `shell32.dll` dialog; confirm pages are known before the sheet is shown and template IDs are captured; record findings in research.md R8 and the decision in handoff.md
- [X] T033 [US2] Implement `src/compat/dialog_hooks.h` / `src/compat/dialog_hooks.cpp` from the spike findings (written fresh, research R8 "Revision"): hooks set with `Wh_SetFunctionHook` in `Wh_ModInit` on `user32` `CreateDialogParamW`, `DialogBoxParamW`, `CreateDialogIndirectParamW`, `DialogBoxIndirectParamW`, `kernelbase` `FindResourceExW` / `FindResourceW`, and `comctl32` `CreatePropertySheetPageW` / `PropertySheetW` (hooked when `comctl32.dll` loads if it is not loaded at init); `RT_DIALOG` lookups remembered in a thread-local list of the last 16 `{module, name}`, cleared when the outermost dialog call on the thread returns; the CBT hook lives only on the calling thread for the duration of the call and records `{HWND → module, template}` for the first `#32770` with the same `HINSTANCE`, the template being the call's argument (non-indirect) or the most recent remembered lookup for that module (indirect), else unknown; pending sheet pages from `PROPSHEETHEADERW` (handles or inline `PROPSHEETPAGEW`) attached to the sheet `HWND`, which is the first `#32770` created on the thread during `PropertySheetW` whatever call it is nested in; a later child `#32770` page with a new module lowers the sheet and notifies listeners through the engine; nothing about the window is changed (FR-008, FR-013a)
- [X] T034 [US2] Wire the dialog facts store into `src/compat/identity.cpp` and `src/compat/engine.cpp` (template IDs and sheet pages feed `classify`), and log one `log::Verbose` line per classification so the level can be checked from the log while diagnostic mode is on (verbose switch wired in T031)

**Checkpoint**: US2 complete; unit tests pass and the log reports levels.

---

## Phase 5: User Story 4 - One set of design tokens (Priority: P2)

Done before US3 because the diagnostic overlay draws with the tokens (plan order 6 → 7).

**Goal**: one read-only token set from WinUI values that follows the system app mode, accent
and text size, and re-resolves without a restart.

**Independent Test**: unit tests resolve tokens for light and dark, three accents and text
scales 100 %, 150 %, 225 %; on the live system the log shows mode, accent and scale changing
when the system settings change.

### Tests for User Story 4

- [x] T035 [P] [US4] Write `tests/unit/tokens_test.cpp`: every value in [data-model.md](data-model.md) "Design token set" for light and dark; accent-derived colors (`accentFill`, `accentFillHover`, `accentText`, `selectionFill` at 0.60) for three accents (`#0078D4` default shades, a green and a red set of shades); type sizes and line heights × 1.00, 1.50 and 2.25 with radii, spacing, heights and focus thicknesses unchanged (FR-026); font family "Segoe UI Variable" with GDI faces "Segoe UI Variable Text" below 20 px and "Segoe UI Variable Display" from 20 px, and "Segoe UI" everywhere when `variableFontAvailable` is false; the failure defaults (light, `#0078D4`, `#429CE3 #76B9ED #99EBFF`, `#005A9E #004275 #002642`, 1.0) (SC-007)

### Implementation for User Story 4

- [x] T036 [US4] Implement `src/tokens/tokens.h` / `src/tokens/tokens.cpp` (**PURE**): `TokenInputs` (`dark`, `accent`, `accentLight1..3`, `accentDark1..3` as ARGB, `textScale` 1.00 to 2.25, `variableFontAvailable`), `Tokens` with every field of data-model.md (typography ramp, geometry, colors as ARGB), `DefaultInputs()`, and `Resolve(const TokenInputs&)`; make T035 pass
- [x] T037 [US4] Implement `src/core/worker.h` / `src/core/worker.cpp` (research R13): one thread with a message loop, COM/WinRT initialized on it (`RoInitialize(RO_INIT_SINGLETHREADED)`), `worker::Post(fn)` to run work on it, started in `Wh_ModInit`, stopped in `Wh_ModUninit` by a posted quit and a wait
- [ ] T038 [US4] Implement `src/tokens/system.h` / `src/tokens/system.cpp` (research R11, R12): on the worker thread create `UISettings` through `RoGetActivationFactory` using `windows.ui.viewmanagement.h`; read `GetColorValue` for `Accent`, `AccentLight1-3`, `AccentDark1-3`, `Background` (dark when the background is dark) and `TextScaleFactor`; check "Segoe UI Variable Text" with DirectWrite `GetSystemFontCollection` → `FindFamilyName`; any failed read → that input's default and one `log::Warn`; register `ColorValuesChanged` and `TextScaleFactorChanged`, re-resolve on change; unregister and release on stop
- [ ] T039 [US4] Implement `tokens::Current()`, `SubscribeChanged`, `Unsubscribe` in `src/tokens/runtime.h` / `src/tokens/runtime.cpp` per contracts/engine-api.md: swap an immutable snapshot, then notify; log mode, accent and text scale with `log::Info` on every resolution (FR-025, FR-028)
- [ ] T040 [US4] Wire the worker and token system into `src/main.cpp` (`Wh_ModInit` starts the worker and the token system; `Wh_ModUninit` stops them before the engine is cleared) and confirm with `build.py --compile`

**Checkpoint**: US4 complete; tokens resolve and follow the system.

---

## Phase 6: User Story 3 - Identify the window under the cursor (Priority: P2)

**Depends on**: US2 (identity, rules, engine) and US4 (tokens and worker).

**Goal**: with S7 on, an overlay next to the cursor shows the identity and level of the window
under it, in any process, and Ctrl+Alt+Shift+C copies an override and a table row.

**Independent Test**: turn S7 on, point at File Explorer, the taskbar, a dialog and
`sysdm.cpl`, compare with an inspection tool, copy and paste; turn S7 off and see nothing
remain.

### Tests for User Story 3

- [ ] T041 [P] [US3] Write `tests/unit/format_test.cpp` for [contracts/compatibility-doc.md](contracts/compatibility-doc.md): overlay lines `Window`, `Top-level`, `Process` (with "(not running WholeUI)" only for other processes), `Level` with each reason text (`default`, `rule`, `override`, `shell surface`, `capped by top-level`, `lowered by page <module>`, `failure`) and the rule origin, `unknown` for unreadable fields, `template not a dialog`; clipboard override line leaves out `unknown`/`not a dialog` fields; table row writes them as `—`, `<name>` placeholder, build as `<CurrentBuild>.<UBR>` or `—`; lines separated by CRLF

### Implementation for User Story 3

- [ ] T042 [US3] Implement `src/diagnostic/format.h` / `src/diagnostic/format.cpp` (**PURE**): `OverlayText(window, topLevel, classification, inProcess)`, `OverrideLine(topLevelIdentity, level)`, `TableRow(topLevelIdentity, level, build)`; make T041 pass
- [ ] T043 [US3] Add a "classify without a window" entry to `src/compat/engine.h` / `engine.cpp` for identities of other processes (rules only, `Unknown` fields not matching, no cache), used by diagnostic mode for "the level WholeUI would assign" (FR-021, US3 scenario 5)
- [ ] T044 [US3] Implement the overlay window in `src/diagnostic/overlay.h` / `src/diagnostic/overlay.cpp` (research R14), on the worker thread: layered window with `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST`, never activated, drawn with Direct2D/DirectWrite into a DIB and shown with `UpdateLayeredWindow`, using `tokens::Current()` (font, `radiusOverlay`, `backgroundBase`, `textPrimary`, `strokeControl`); placed 16 px below-right of the cursor, flipped near monitor edges so it never covers the inspected point (FR-021a, FR-022)
- [ ] T045 [US3] Add cursor tracking to `src/diagnostic/overlay.cpp`: 50 ms timer reading `GetCursorPos` + `WindowFromPoint`; refresh when the window under the cursor changes or 100 ms after the cursor stops; identify the window and its top-level with `identity` (in-process or cross-process) and classify; `log::Verbose` the time from rest to update (SC-005); no low-level input hooks
- [ ] T046 [US3] Add the copy shortcut to `src/diagnostic/overlay.cpp`: edge-triggered `GetAsyncKeyState` check of Ctrl+Alt+Shift+C in the same timer, only while S7 is on; read `CurrentBuild` and `UBR` with `RegGetValueW` from `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion`; `OpenClipboard(overlay)` + `SetClipboardData(CF_UNICODETEXT)` with the two lines from `format` (FR-021b); the keystroke is observed, not swallowed, so it still reaches the foreground app (FR-022) — the readme says so (T006)
- [ ] T047 [US3] Add single-instance and lifecycle to `src/diagnostic/overlay.cpp`: own `Local\WholeUI.Diagnostic` mutex before showing anything, other instances stay idle and retry when the owner exits; `diagnostic::Start()` / `Stop()` destroy the window, kill the timer and release the mutex at once (FR-023)
- [ ] T048 [US3] Wire diagnostic mode into `src/main.cpp`: start or stop on `Wh_ModInit` and `Wh_ModSettingsChanged` according to `diagnosticMode` (the verbose switch is already wired by T031), stop in `Wh_ModUninit` before the worker stops (FR-020, FR-029)

**Checkpoint**: all four stories work; ready for live acceptance.

---

## Phase 7: Polish and live acceptance

- [ ] T049 Run `python tools/build.py --check`, `--compile` and `python tools/test.py`; record the SC-004 timing printed by T021 in handoff.md
- [ ] T050 Live acceptance: run every step of [quickstart.md](quickstart.md) "Validate on the live system" (1–11) on the maintainer's machine with the maintainer; record the Windows build, results and any failing step in handoff.md (SC-003, SC-005, SC-006, SC-008)
- [ ] T051 Confirm or correct the shell-surface classes from step 3 in `docs/compatibility.md` (research R9), and confirm `$name:pt-BR` from step 10 in research.md R3 (handoff open questions)
- [ ] T052 [P] Update `AGENTS.md` "Status" and `docs/roadmap.md` to say the foundation spec is implemented, and `README.md` with the build commands from quickstart.md
- [ ] T053 [P] Confirm `docs/credits.md` needs no entry for this spec (no mod inspired it; constitution VI) and say so in handoff.md
- [ ] T054 Update [contracts/engine-api.md](contracts/engine-api.md) with the final signatures if any changed during implementation, then update handoff.md with state, decisions and next step

---

## Dependencies and Execution Order

### Phase dependencies

- **Setup (Phase 1)**: none. T003 before T004; T002 before T004 when the `.def` route is chosen.
- **Foundational (Phase 2)**: after Setup. T010 after T008; T009's test after T011.
- **US1 (Phase 3)**: after Foundational. Every later story is built and tested through it.
- **US2 (Phase 4)**: after US1. T032 (spike) before T033; T029 before T034.
- **US4 (Phase 5)**: after US1; independent of US2.
- **US3 (Phase 6)**: after US2 and US4.
- **Polish (Phase 7)**: after all stories.

### Within each story

- Tests first, failing; then pure modules; then Windows adapters; then wiring in `src/main.cpp`.
- `src/main.cpp` and `tools/build.py` are edited by several tasks: those tasks run one at a time.

### Parallel opportunities

- Phase 2: T005, T006, T007, T008, T009, T011 (different files).
- US2: tests T020/T021, T022, T023 together; then T026 and T027 together; T024 alongside them.
- US4 can run alongside US2 once US1 is done (only `src/main.cpp` wiring is shared: T031,
  T040 one at a time).
- US3: T041 alongside T043.

## Parallel Example: User Story 2

```text
Task: "Write tests/unit/rules_test.cpp (T020, T021)"
Task: "Write tests/unit/overrides_test.cpp (T022)"
Task: "Add compatibility-doc parser tests to tests/tools/test_build.py (T023)"
# then
Task: "Implement src/compat/rules.h/.cpp (T026)"
Task: "Implement src/settings/overrides.h/.cpp (T027)"
```

## Implementation Strategy

### MVP first (User Story 1)

1. Phases 1 and 2.
2. Phase 3 (US1): the mod builds, compiles in CI and loads with no visible change.
3. **Stop and validate** with the maintainer; commit after validation.

### Incremental delivery

1. US1 → validate → commit.
2. US2 (with the spike on its own branch) → validate → commit.
3. US4 → validate → commit.
4. US3 → validate → commit.
5. Live acceptance (Phase 7) → handoff → pull request.

Suggested break points for a new conversation: after US1, after US2, after US4, before
live acceptance; update handoff.md first.
