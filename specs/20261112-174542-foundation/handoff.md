# Handoff: Foundation

**Updated**: 2026-11-13 (US2: T033 dialog hooks) | **Spec**: [spec.md](spec.md) | **Branch**: `feat/20261112-174542-foundation`

## State

Spec, plan, tasks, Phase 1, Phase 2 and US1 (T012–T019) done and committed. The branch is on
GitHub with draft pull request #1; CI ("Build and test") passes in about 35 s. US2 tests and
pure logic (T020–T027) are committed (not pushed yet). T028–T030 (settings, identity, engine)
and T031 (wiring in `main.cpp`) are committed as `feat: add settings, window identity and
compatibility engine` (not pushed yet).

US1 results kept for reference: `build.py` writes in 3 ms, `--compile` about 1.2 s (SC-001
< 30 s met); `test.py` 7/7 C++ and 24/24 Python; `--install` loads the DLL in `explorer.exe`
only and Windhawk's log shows `[core] WholeUI 0.1.0 loaded`. Python 3.12 (CI's version) is
not installed locally; the scripts use nothing newer than 3.9.

## Done in the last session

- T033: `src/compat/dialog_hooks.h/.cpp` per research R8 "Revision". Public:
  `InstallDialogHooks()` (Wh_ModInit, before `ApplySettings`), `UninstallDialogHooks()`
  (Wh_ModUninit, before `ResetEngine`), `DialogTemplateOf(HWND) → TemplateField`,
  `SheetPagesOf(HWND) → SheetPages { known, incomplete, pages }` (page identities: own process,
  page module, `#32770`, template). Late page → `compat::OnSheetPagesChanged(sheet)` (new in
  engine; re-evaluates the sheet and cache entries whose top-level is the sheet, notifies;
  entries now keep `topLevelHwnd`). Contract engine-api.md updated.
- Internals live in `namespace wholeui::compat::dialogs { namespace { … } }`: the amalgamation
  shares one anonymous namespace per named namespace across all `.cpp` files. SRWLOCK guards
  moved to `src/core/lock.h` (`wholeui::SharedLock` / `ExclusiveLock`). `identity.h` now
  exposes `ModuleNameOf(HMODULE)` and `CurrentProcessName()`.
- Late comctl32: `kernelbase` `LoadLibraryExW` hook + two `GetModuleHandleW` lookups (research
  R8 "Chosen in T033").
- Live check (diagnostic mode on, verbose `[dialogs]` lines): Run → `template 1003`; Properties
  of a WinRAR SFX `.exe` → sheet with 7 pages (shell32 1041, acppage 5011, rarext
  EXTARCINFODLG, cryptui 145, aclui 103, shell32 16816, twext 102) logged before the frame
  (`1006`), and each page window got its template when its tab opened; drive C: (7 pages incl.
  dskquoui 108, ntshrui 1000) and Folder Options (shell32 29959–29961) likewise. No `page
  added later`, no warnings. A nested child dialog in a page (shell32 1410) is correctly not
  taken for a page. Not observed: unloading the mod while a sheet is open.
- Results: `tools/test.py` 57/57 C++ and 43/43 Python; `build.py --check --compile` clean.

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
- Case-insensitive matching (rules and override field names) folds **ASCII letters only**
  (`EqualsIgnoreCase` in `rules.h`), so the pure code needs no locale or Windows API. Process
  and module names are read lower-cased (T029), so this only matters for typed non-ASCII
  values. Revisit if a real window needs Unicode folding.
- Engine (T030): returns `WindowClassification { level, ownLevel, reason, ruleSource,
  ruleOrigin }` (a copy; rules.h's `Classification` points into the rule set). Identities are
  read **outside** the lock (module names take the loader lock); the cache entry keeps the
  window's and top-level's identities so `SetRuleSet` re-classifies without calling Windows
  under the lock (only `IsWindow` for the sweep). Failures (invalid window, unreadable
  top-level, exception) are not cached and log one `log::Warn` each. Before the first
  `SetRuleSet` every window is `Native`/`Default`. If `SetRuleSet` runs out of memory, the
  cache is cleared and listeners are not told. Contract updated.
- Overrides array (T028): Windhawk returns "" for an empty item and past the end, so reading
  stops after 16 empty results in a row (a blank line between entries does not hide the rest).
  Contract settings.md and T028 updated.
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

## US2 APIs (tests and implementation, T020–T027)

Tests and implementation go into the same commit (the tests do not compile without it).

- `tests/unit/rules_test.cpp` (T020, T021): 28 tests.
- `tests/unit/overrides_test.cpp` (T022): 21 tests.
- `tests/unit/compat_print.h`: `operator<<` for `Level` and `Reason`, shared by both test
  files (one definition, no ODR clash).
- `tests/tools/test_build.py` (T023): `CompatibilityDocTest`, `CompatibilityDocErrorTest`,
  `BuiltInRulesIncludeTest`, and `RealSourcesTest.test_repository_compatibility_doc_parses`
  (passes after T024); fixtures in `tests/tools/fixtures/compat/`.

### `src/compat/rules.h` API fixed by the tests (T026, namespace `wholeui::compat`)


- `Level`, `Reason` (`Default`, `Rule`, `Locked`, `CappedByTopLevel`, `LoweredByPage`,
  `Failure`), `RuleSource` (`BuiltIn`, `Override`), all `uint8_t` enums.
- `Field { bool known; std::wstring value; static Of(v); static Unknown(); }`;
  `TemplateField { Kind kind (Value, Unknown, NotADialog); std::wstring value; static Of(v);
  Unknown(); NotADialog(); }`.
- `WindowIdentity { Field process, module, windowClass; TemplateField templateId; bool failed; }`
  — `failed` is step 1 of the resolution order (a read that failed abnormally).
- `Rule { std::optional<std::wstring> process, module, windowClass, templateId; Level level;
  RuleSource source; bool locked; std::wstring origin; }`;
  `RuleSet { std::vector<Rule> builtIns, overrides; uint64_t version; }`.
- `Classification { Level level, ownLevel; Reason reason; const Rule* rule; }`.
- `Classify(const WindowIdentity& window, const WindowIdentity* topLevel,
  std::span<const WindowIdentity> pages, const RuleSet&) noexcept` — `topLevel == nullptr`
  means the window is itself top-level; `pages` are the pages of the top-level sheet. Named
  `Classify` (PascalCase like the rest of the code) instead of `classify` in data-model.md;
  T054 records it in the contract.
- `const std::vector<Rule>& BuiltInRules()`.

Behaviour choices the test encodes (all fail-safe, within data-model.md): a failed top-level
or page makes the window `Native`/`Failure`; a child of a locked top-level is
`Native`/`Locked`; a child's cap uses the top-level's effective level (after page lowering);
a page's level is its own level from step 3.

### `src/settings/overrides.h` API fixed by the tests (T027, namespace `wholeui::settings`)

- `OverrideError { int index; std::wstring text; std::wstring reason; }`;
  `OverrideResult { std::optional<compat::Rule> rule; std::optional<OverrideError> error; }`
  — both empty means skipped (empty or whitespace-only entry).
- `OverrideResult ParseOverride(int index, std::wstring_view text)`.
- `bool NamesShellSurface(const compat::Rule& override, std::span<const compat::Rule> builtIns)`
  — true when the override names every field of a locked rule with equal values
  (case-insensitive); `class=Shell_TrayWnd` alone is not flagged (it may match another
  process).

Parser choices the tests encode (within contracts/settings.md): field names are
case-insensitive; the rule's `origin` and the error's `text` are the entry with outer
whitespace trimmed; only the first `=` splits name and value; empty segments (a trailing `;`)
are ignored; `/`, `\` or `:` in `process` or `module` is a path; field errors are reported
left to right, then `missing level`, then `no identity field`.

### `tools/build.py` API fixed by the tests (T025)

- `parse_compatibility(path) → list` of objects with `origin`, `process`, `module`,
  `window_class`, `template` (`None` when not named), `level` (`"native"`, `"visual"`,
  `"full"`; `to do` → `"native"`) and `locked`; document order. Errors are `BuildError`
  with `<file>:<line>:` and the reason (`no identity field`, `unknown level`, column count,
  header); a missing table names its heading.
- `builtin_rules_text(path) → str` (wide C++ string literals, escaped, LF, one final
  newline); `write_builtin_rules(repo) → Path` writes `build/gen/builtin_rules.inc` from
  `docs/compatibility.md`.

## T032 spike (2026-11-13)

Done on `spike/20261112-dialog-capture` (`src/spike/dialog_capture.cpp`, logging only; never
merged). Findings and a **proposed revision of R8** are in research.md R8 "Spike findings".
Short version: pages and their modules are known from `PropertySheetW`'s header before the
sheet window exists (confirmed, including the third-party `rarext.dll` page); but every dialog
observed (Run, sheet frames, pages) is created through the **indirect** functions with a
copied template, so the template ID must come from a thread-local record of
`FindResourceExW(RT_DIALOG)` lookups matched by module. The maintainer approved the revision;
T033 in tasks.md is rewritten to follow it. Spike committed on its branch (`804a72b`).

How to watch the mod's log without the Windhawk UI: `OutputDebugString` listener (DBWIN
protocol); Windhawk writes `[WH] [local@wholeui] …` lines there. `windhawk-cli mod
disable/enable` does not reload the DLL; `build.py --install` or a settings change does.
The live check of `[settings] loaded/changed` lines passed (diagnostic mode is now on in the
local install).

## Next step

1. T034: feed `DialogTemplateOf` into `ReadIdentity` (template for `#32770`) and
   `SheetPagesOf` into the engine's `Evaluate` (pages span; `incomplete` → treat as an
   unknown page), one `log::Verbose` line per classification. Lock order: engine lock, then
   the dialog store lock (dialog_hooks never calls the engine while holding its lock).
