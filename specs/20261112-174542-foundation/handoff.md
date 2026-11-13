# Handoff: Foundation

**Updated**: 2026-11-13 (US3: T048 committed) | **Spec**: [spec.md](spec.md) | **Branch**: `feat/20261112-174542-foundation`

## State

Spec, plan, tasks, Phase 1, Phase 2 and US1 (T012–T019) done and committed. The branch is on
GitHub with draft pull request #1; CI ("Build and test") passes in about 35 s. US2 tests and
pure logic (T020–T027) are committed (not pushed yet). T028–T030 (settings, identity, engine)
and T031 (wiring in `main.cpp`) are committed as `feat: add settings, window identity and
compatibility engine` (not pushed yet). T033 committed as `feat: add dialog and property
sheet hooks`. T034 committed as `feat: wire dialog facts into classification`; US2 is complete.
None of the US2 commits are pushed yet.

US1 results kept for reference: `build.py` writes in 3 ms, `--compile` about 1.2 s (SC-001
< 30 s met); `test.py` 7/7 C++ and 24/24 Python; `--install` loads the DLL in `explorer.exe`
only and Windhawk's log shows `[core] WholeUI 0.1.0 loaded`. Python 3.12 (CI's version) is
not installed locally; the scripts use nothing newer than 3.9.

## Done in the last session

- T034: `ReadIdentity` takes a `#32770`'s template from `DialogTemplateOf` (other processes
  stay Unknown). The engine reads `SheetPagesOf(topLevel)` outside its lock and keeps the
  pages in each cache entry as a shared `PageList` (sheet and its children share one list);
  `incomplete` adds one all-Unknown page (lowers the sheet to Native). `SetRuleSet` re-uses the
  stored pages; `OnSheetPagesChanged` re-reads the sheet's pages, stores them in the sheet's
  and its children's entries and bumps a `pagesEpoch`, so a classification that read pages
  before the change is not cached. One `log::Verbose` line per classification (level, own
  level, reason, rule origin, process, module, class, template, top-level, page count) and one
  per level change (`settings changed` / `sheet page added`).
- Found live: at `HCBT_CREATEWND` a child is not linked to its parent yet (`GetParent` is the
  desktop, `GA_ROOT` is the window itself), so a query during creation would cache a child as
  top-level and skip the cap. Now a `WS_CHILD` window whose top-level is itself is
  `Native`/`Failure`, not cached, with a verbose line (expected case, not a warning).
  Contract engine-api.md "Rules for callers" updated.
- Live check (temporary code, reverted: classify each dialog at creation and 300 ms later;
  test overrides `module=comctl32.dll; level=full` and `module=shell32.dll; level=visual`,
  removed): Run → `visual` (rule), template 1003; WinRAR SFX Properties sheet → own `full`,
  `lowered by page`, 7 pages; shell32 pages 1041 / 16816 → own `visual`, `capped by
  top-level` to `native`, top-level = the sheet; other pages `native` default.
- Results: `tools/test.py` 57/57 C++ and 43/43 Python; `build.py --check --compile` clean;
  the clean build is installed (no overrides, diagnostic mode on).

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

## US4 API fixed by the tests (T035)

`tests/unit/tokens_test.cpp` (15 tests, T035) and `src/tokens/tokens.h` / `tokens.cpp`
(T036, **PURE**) are written and pass (`tools/test.py` 72/72 C++, 43/43 Python;
`build.py --check --compile` clean). Committed as `feat: add design token resolver` (not
pushed). `main.cpp` does not include the tokens yet (T040).

Toolchain gotcha found here: with clang 20.1, a `const T&` parameter whose default argument is
a constexpr object always reads the default, even when another object is passed (-O0 and
-O2). The test helper takes the accent set by value; avoid that pattern in the mod too.

`src/tokens/tokens.h`, namespace `wholeui::tokens` (T036, **PURE**):

- `TokenInputs { bool dark; uint32_t accent, accentLight1..3, accentDark1..3; float textScale;
  bool variableFontAvailable; }`; `TokenInputs DefaultInputs()` (the failure defaults;
  `variableFontAvailable = true`, not checked by the tests).
- `TypeStyle { float size; float lineHeight; int weight; std::wstring gdiFace; }` with weight
  400 (Regular) / 600 (Semibold).
- `Spacing { int xs, s, m, l, xl, xxl; }` (4, 8, 12, 16, 24, 32).
- `Tokens`: `bool dark`; `std::wstring fontFamily`; `TypeStyle caption, body, bodyStrong,
  subtitle, title, titleLarge, display`; `int radiusControl, radiusOverlay, radiusMenuHover,
  radiusListSelection, radiusGridSelection`; `Spacing spacing`; `int heightControl,
  heightControlCompact, heightListRow, heightMenuItem, heightNavItem, focusOuter,
  focusInner`; `float selectionAccentOpacity`; every color of data-model.md as `uint32_t`
  ARGB with the same name.
- `Tokens Resolve(const TokenInputs&)`.

Interpretations the tests encode, recorded in data-model.md: "X at N %" keeps X's RGB and
sets alpha to round(N % × 255) (90 % → `E6`, 60 % → `99`); the 20 px Text/Display split uses
the **scaled** size (at 150 % `body` is 21 px → Display).

## T037 worker thread (2026-11-13)

`src/core/worker.h` / `worker.cpp` (not PURE), namespace `wholeui::worker`: `Start()`,
`Stop()`, `Post(std::function<void()>)`, `IsWorkerThread()`, all `noexcept`.

- One thread, `RoInitialize(RO_INIT_SINGLETHREADED)`, a message loop and a **message-only
  window** (`WholeUI.Worker`, `HWND_MESSAGE`) that receives the wake and quit messages
  (a thread message would be lost in a modal loop on the worker).
- Tasks sit in a `std::deque` under an `SRWLOCK`; one wake message at a time
  (`wakePending`). Tasks run FIFO, outside the lock; a throwing task is logged
  (`log::Warn`) and skipped.
- `Start` does not wait for the thread (no wait inside `Wh_ModInit`); tasks posted before the
  window exists run as soon as it does.
- `Stop` closes the queue, posts quit, waits `INFINITE`; every task posted before `Stop` runs
  first (a final drain after the loop catches tasks whose wake reached a destroyed window), so
  T038 can post its unregister/release and rely on `Stop`. `Stop` on the worker thread is
  ignored with a warning.
- Wired in `main.cpp` already (`Start` first in `Wh_ModInit`; `Stop` after the dialog hooks
  are removed, before `ResetEngine`); T040 adds the token system around it.
- Live check (temporary code, reverted): three posted tasks ran on the worker, a throwing one
  was logged and skipped; two `--install` reloads stopped the old worker (`[worker] stopped`)
  with no hang. `[worker] started` is Verbose and is written before settings turn verbose on,
  so it does not appear at load.
- Results: `build.py --check --compile` clean; `tools/test.py` 72/72 C++, 43/43 Python; the
  clean build is installed.

## T038 system reads (2026-11-13)

`src/tokens/system.h` / `system.cpp` (not PURE), namespace `wholeui::tokens::system`:
`using InputsFn = void (*)(const TokenInputs&, void* context)`; `bool Start(InputsFn,
void*)`; `void Stop()`, both `noexcept`. Included in `main.cpp` but **not called yet**
(T040 wires `Start`/`Stop`; `Stop` must run before `worker::Stop`).

- `Start` posts one task: create `UISettings` (`WindowsCreateStringReference` →
  `RoGetActivationFactory` → `ActivateInstance`, QI `IUISettings3` for colors and
  `IUISettings2` for text scale), register both events, then read and call `InputsFn` on the
  worker. Each event posts one re-read (`g_rereadPending` coalesces events queued together).
- Handler: one agile object (`ITypedEventHandler<UISettings*, IInspectable*>` +
  `IAgileObject`) registered for both events; it only posts to the worker.
- `Stop` posts: remove both registrations, release handler and settings, then wait up to
  1 s (10 ms steps) for the live-handler count to reach 0 (an event in flight on another
  thread), else one `log::Warn`.
- Failures: each read falls back separately with one `log::Warn`; the accent and its six
  shades are one input (any failure → all seven default); text scale outside 1.00–2.25 is
  clamped; failed font check → assumed present. Recorded in data-model.md.
- Dark = not `IsColorLight(Background)` (`5G + 2R + B > 1024`).
- Live check (temporary logging callback, reverted): load read `dark=1`, accent `0078D4`
  with system shades `0091F8 4CC2FF 99EBFF / 0067C0 003E92 001A68` (they differ from the
  documented failure defaults, as expected), scale 1.00, font present, on the worker.
  Maintainer toggled light/dark, accent (orange `F7630C` and back) and text size 150 % and
  back: every change arrived with correct values. Each change gives **2–3 identical reads**
  (Windows raises the event several times, too far apart to coalesce): **T039 should skip
  the swap/notify/Info log when the inputs equal the current ones.** Reinstall unloaded
  cleanly (`[worker] stopped`, no handler-wait warning).
- Results: `build.py --check --compile` clean; `tools/test.py` 72/72 C++, 43/43 Python; the
  clean build is installed.

## T039 runtime snapshot (2026-11-13)

`src/tokens/runtime.h` / `runtime.cpp` (not PURE), namespace `wholeui::tokens`: `Current()`
(`noexcept`), `SubscribeChanged`, `Unsubscribe`, and for `main.cpp` only
`OnSystemInputs(const TokenInputs&, void*)` (a `system::InputsFn`) and `ResetTokens()`.
Included in `main.cpp` but **not called yet** (T040).

- Before the first read `Current()` is `Resolve(DefaultInputs())` (function-local static).
- Swap: resolve outside the lock, push the new snapshot into a list kept until
  `ResetTokens`, publish it through an atomic pointer (`Current()` takes no lock), then one
  `log::Info` (`resolved: mode dark, accent #0078D4, text scale 1.00, font Segoe UI
  Variable`) and `Notify` with no lock held. Keeping every snapshot (changes are rare) keeps
  references from other threads valid without reference counting.
- Inputs equal to the current ones are skipped (no swap, log or notify): `TokenInputs` got a
  defaulted `operator==` (exact, floats included), with 2 new tests in `tokens_test.cpp`.
- Resolve failure (out of memory) keeps the current snapshot, one `log::Warn`.
- `ResetTokens` must run after `system::Stop` and `worker::Stop`; it clears listeners and
  snapshots, so `Current()` returns the defaults again.
- Contract engine-api.md "Design tokens" updated (where each function lives, skip rule,
  snapshot lifetime, main.cpp entries).
- Results: `build.py --check --compile` clean; `tools/test.py` 74/74 C++, 43/43 Python. Not
  installed or checked live (nothing calls it until T040).

## T040 wiring (2026-11-13)

`Wh_ModInit`: `worker::Start` → `tokens::system::Start(tokens::OnSystemInputs, nullptr)` →
dialog hooks → settings. `Wh_ModUninit`: dialog hooks off → `system::Stop` → `worker::Stop`
→ `ResetTokens` → `ResetEngine`.

- Live check: at load `[tokens] resolved: mode dark, accent #0078D4, text scale 1.00, font
  Segoe UI Variable`. App mode toggled light and back (registry `AppsUseLightTheme` plus a
  `WM_SETTINGCHANGE "ImmersiveColorSet"` broadcast, then restored): with the PID added to the
  line for a moment (reverted), each `explorer.exe` (3 running) logged **one** line per change.
  Skip rule works; no repeats.
- Watching the log: the DBWIN capture (scratch script, not in the repo) **drops lines** when
  several explorer processes write at once, so a line missing from it is not proof of
  anything. Count lines per PID, or use Windhawk's log view.
- Results: `build.py --check --compile` clean; `tools/test.py` 74/74 C++, 43/43 Python; the
  clean build is installed.

## T041–T042 diagnostic text (2026-11-13)

`src/diagnostic/format.h` / `format.cpp` (**PURE**), namespace `wholeui::diagnostic`:
`LevelText(Level)`, `OverlayText(window, topLevel, WindowClassification, inProcess)`,
`OverrideLine(topLevel, level)`, `TableRow(topLevel, level, build)`,
`BuildText(optional<wstring> currentBuild, optional<uint32_t> ubr)` (empty when either read
failed; `TableRow` writes `—`), `ClipboardText(topLevel, level, build)` (override + CRLF + row).
Tests in `tests/unit/format_test.cpp` (15). Included in `main.cpp`, not called yet.

- `lowered by page <module>` needed the page: `compat::Classification` gained
  `const WindowIdentity* page` (first page with the lowest level), and `WindowClassification`
  **moved from engine.h to rules.h** (PURE) with a new `Field pageModule`; `CopyOf(pure)` in
  rules.cpp makes the copy (the engine uses it). 2 new tests in rules_test.cpp. engine-api.md
  and data-model.md updated.
- Overlay text: each line is `<label>` TAB `<value>` (T044 sets a DirectWrite tab stop to
  align values with the proportional font); hint line last, no tab; CRLF between lines. The
  `· <rule origin>` part is left out when no rule matched; a known but empty field shows
  `unknown`; `topLevel` is the window itself for a top-level window. compatibility-doc.md
  updated.
- An all-unknown identity gives an override line of only `level=…` (accepted; recorded).
- Results: `tools/test.py` 91/91 C++, 43/43 Python; `build.py --check --compile` clean.
  Not installed (nothing calls it).

## T043 classify without a window (2026-11-13)

`compat::ClassifyIdentity(const WindowIdentity& window, const WindowIdentity* topLevel)
noexcept` in engine.h / engine.cpp: takes the current rule set under the shared lock, then
`CopyOf(Classify(window, topLevel, {}, *rules))` (the `shared_ptr` keeps the rules alive for
`CopyOf`). No cache, no listeners, no pages (other processes' sheets are never seen); Unknown
fields already match no rule in `rules.cpp`. Before the first `SetRuleSet` → `Native`/`Default`;
exception → `Native`/`Failure` with one `log::Warn`. T045 calls it for windows of other
processes, after `ReadIdentity` on the window and `TopLevelOf` (pass `nullptr` when the window
is its own top-level). Contract engine-api.md updated. Not unit-tested (engine is not PURE; the
logic is `compat::Classify`, already covered).

- Results: `build.py --check --compile` clean; `tools/test.py` 91/91 C++, 43/43 Python.

## T044 overlay window (2026-11-13)

`src/diagnostic/overlay.h` / `overlay.cpp` (not PURE), namespace
`wholeui::diagnostic::overlay`, **worker thread only** (other threads: one warn, ignored):
`bool Show(POINT cursor, const std::wstring& text)`, `void Follow(POINT cursor)`,
`void Hide()`, `void Destroy()`, all `noexcept`. Included in `main.cpp`, not called yet
(T045 calls Show/Follow from its timer; T047's Stop posts `Destroy`, which must run before
`worker::Stop`, or the window class stays registered after the DLL unloads).

- Window `WholeUI.Diagnostic`: `WS_POPUP` with `WS_EX_LAYERED | WS_EX_TRANSPARENT |
  WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST`; `WM_MOUSEACTIVATE` → `MA_NOACTIVATE`,
  `WM_NCHITTEST` → `HTTRANSPARENT`; shown with `SWP_NOACTIVATE | SWP_SHOWWINDOW`.
- Drawing: `ID2D1DCRenderTarget` (premultiplied BGRA, DPI of the cursor's monitor) bound to
  a 32-bit top-down DIB, then `UpdateLayeredWindow(ULW_ALPHA)`. Tokens read on every Show:
  `fontFamily` + `body` style, padding `spacing.m` × `spacing.s`, `radiusOverlay`,
  `backgroundBase` fill, 1 physical px `strokeControl` border, `textPrimary`, hint line in
  `textSecondary`. Grayscale text antialiasing (transparent corners rule out ClearType).
  Values align on one incremental tab stop = widest label + `spacing.l`; no wrapping.
- DPI: each call switches the thread to per-monitor-v2 for its duration; size uses
  `GetDpiForMonitor` (new link flag `-lshcore` in header.txt, plan.md updated). `Follow`
  only moves (`SetWindowPos`), and redraws the last text when the cursor reaches a monitor
  with another DPI. **T045 must read `GetCursorPos` under per-monitor-v2 awareness too.**
- Placement: cursor + 16 px (scaled); on an axis where it would leave the monitor
  (`rcMonitor`) it goes to the other side; then clamped inside the monitor.
- Failures: one `log::Warn` until a draw succeeds again; the overlay is hidden. A failed
  `EndDraw` (device lost included) drops the render target; the next Show makes a new one.
- `ComRef` moved from `tokens/system.cpp` to `src/core/com.h` (namespace `wholeui`), shared
  by both.
- Live check (temporary code, reverted: shell process only, Show with sample text and a 50 ms
  thread timer calling Follow; Destroy posted at uninit): screenshot shows the overlay 16 px
  below-right, rounded corners, border, values aligned, hint dimmer. Window rect for cursor at
  (1880,1050) → (1430,918)–(1864,1034), (1880,100) → right edge flipped only, (100,1050) →
  bottom flipped only, second monitor (-454,754) → below-right. After reinstall no
  `WholeUI.Diagnostic` window is left.
- Results: `build.py --check --compile` clean; `tools/test.py` 91/91 C++, 43/43 Python; the
  clean build is installed.

## T045 cursor tracking (2026-11-13)

In `src/diagnostic/overlay.h` / `overlay.cpp`, worker thread only: `bool StartTracking()`
and `void StopTracking()` (`noexcept`). Not called from `main.cpp` yet: T047's
`diagnostic::Start()` / `Stop()` wrap them (mutex first), T048 wires them.

- Timer: `SetTimer(overlay window, 1, 50 ms)`, handled in `WindowProc` (`WM_TIMER`), so
  `Destroy` kills it with the window. `StartTracking` creates the window (hidden) if needed;
  `StopTracking` kills the timer and hides the overlay, keeping the window.
- Each tick runs under per-monitor-v2 awareness: `GetCursorPos` (failure, e.g. secure
  desktop → hide, forget the shown window), `WindowFromPoint` (null → hide). A new window
  under the cursor → refresh at once; same window and the cursor moved → `Follow`; the cursor
  still and `now − movedAfter ≥ 100 ms` → one refresh at rest plus a `log::Verbose`
  (`overlay updated at most N ms after the cursor came to rest`). `movedAfter` is the time
  of the previous tick when the cursor was seen elsewhere, so N is an upper bound of the
  delay from rest (SC-005). Times from `QueryPerformanceCounter`.
- Refresh: `TopLevelOf` (null → window gone, hide); in this process `compat::Classify(hwnd)`
  (cached engine, pages included), otherwise `ClassifyIdentity(window, top or nullptr)`;
  identities from `ReadIdentity`; text from `OverlayText`. `bad_alloc` → the overlay's one
  warning, hidden. The shown window is remembered even when drawing failed, so it is retried
  on the next window change or rest, not on every tick.
- Live check (temporary code, reverted: `StartTracking` posted in the shell process only,
  `Destroy` posted at uninit; cursor driven with `SetCursorPos`, screenshots): taskbar →
  `Shell_TrayWnd · explorer.exe`, `native · shell surface · Taskbar`, overlay above the
  cursor (bottom edge); VS Code → `Chrome_RenderWidgetHostHWND`, top-level
  `Chrome_WidgetWin_1`, `code.exe (not running WholeUI)`, `native · default`; near the right
  edge the overlay flips left. Logged rest delays 123–127 ms (budget 200 ms).
- Module `unknown` for VS Code is correct: Electron/Chrome windows have a null
  `GWLP_HINSTANCE` (checked with a probe script).
- Results: `build.py --check --compile` clean; `tools/test.py` 91/91 C++, 43/43 Python; the
  clean build is installed.

## T046 copy shortcut (2026-11-13)

In `src/diagnostic/overlay.cpp`, in the T045 tick (no new public API; overlay.h comment
updated).

- `ShortcutPressed()` runs first in every tick (also when `GetCursorPos` fails, so the edge
  state stays right): `GetAsyncKeyState` high bit of Ctrl, Alt, Shift and C; true only on the
  tick the chord becomes held (`Tracking::shortcutDown`). Polled, never swallowed (FR-022); a
  press shorter than one 50 ms tick can be missed (accepted).
- `Copy(under)` runs after `WindowFromPoint` (null → nothing copied) and before the refresh:
  `TopLevelOf`, `ReadIdentity(topLevel)`, level = `Classify(topLevel).level` in this process or
  `ClassifyIdentity(top, nullptr).level` otherwise (the top-level's effective level, contract
  "Clipboard"); text from `ClipboardText(top, level, ReadBuild())`.
- `ReadBuild()`: `RegGetValueW` `CurrentBuild` (`RRF_RT_REG_SZ`) and `UBR`
  (`RRF_RT_REG_DWORD`) from `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion`, then
  `BuildText`. advapi32 is linked by default (no `@compilerOptions` change).
- `SetClipboardText()`: `GlobalAlloc(GMEM_MOVEABLE)` → `OpenClipboard(overlay)` →
  `EmptyClipboard` → `SetClipboardData(CF_UNICODETEXT)`; the memory is freed only if the
  clipboard did not take it. Each failure logs one `log::Warn` (e.g. another app holds the
  clipboard); success logs `copied: <text>` at verbose.
- Readme already says the shortcut is observed, not captured (T006); no change.
- Live check (temporary code, reverted: `StartTracking` in the shell process, `Destroy` at
  uninit; cursor moved and keys sent with `SetCursorPos` / `keybd_event`): secondary taskbar →
  `process=explorer.exe; module=taskbar.dll; class=Shell_SecondaryTrayWnd; level=native` and
  its row with build `26300.9711`; VS Code → `process=code.exe; class=Chrome_WidgetWin_1;
  level=native`, row with `—` (U+2014) for module, template and Validated. Holding the chord
  for 700 ms copied once (clipboard overwritten after the first copy stayed unchanged).
- Results: `build.py --check --compile` clean; `tools/test.py` 91/91 C++, 43/43 Python; the
  clean build is installed.

## T047 single instance and lifecycle (2026-11-13)

In `src/diagnostic/overlay.h` / `overlay.cpp`, namespace `wholeui::diagnostic` (outside
`overlay`): `bool Start()` and `void Stop()`, `noexcept`, **safe from any thread** (they
post to the worker, so off/on/off ends in the last state). Not called from `main.cpp` yet
(T048: `Start` after `ApplySettings` when `diagnosticMode` is on, `Stop` when off and in
`Wh_ModUninit` before `system::Stop` / `worker::Stop`).

- Worker task: `CreateMutexW(Local\WholeUI.Diagnostic)` (kept open while on) and
  `WaitForSingleObject(…, 0)`; `WAIT_OBJECT_0` or `WAIT_ABANDONED` (verbose line) → owner →
  `overlay::StartTracking` → `log::Info` `overlay shown by this process (pid N)`. Otherwise a
  1 s thread timer (`SetTimer(nullptr, …, TIMERPROC)`) retries; one verbose line `another
  process shows the overlay; waiting`. If `StartTracking` fails the mutex is released and the
  instance retries like an idle one. `CreateMutex` failure warns once and retries.
- Stop task: kill the retry timer, `StopTracking`, `Destroy` (window, class, D2D), release
  and close the mutex; `log::Info` `overlay removed` only in the owner. research.md R14
  updated (retry interval, abandoned mutex, thread safety).
- Live check (temporary `Start` after `ApplySettings` and `Stop` first in uninit, reverted):
  3 `explorer.exe`; one owner (48468), the other two waiting; reinstall → `overlay removed` in
  the owner, `[worker] stopped` everywhere, new owner. With a helper script holding the mutex
  across a reinstall, all three waited; when it exited without releasing, exactly one took
  over within 1 s with the abandoned line. One `WholeUI.Diagnostic` window (in the owner);
  none after unload.
- **Log capture**: eight DBWIN listeners from earlier sessions are still running (python
  `dbwin.py` in old scratchpads; stopping them was refused by the permission classifier).
  Each line goes to only one listener, which is why lines seemed "dropped". Merge every
  `log*.txt` in the old scratchpads (`20e21459-…`, `9df39a7d-…`) with the new one, or ask the
  maintainer to end those `python.exe` processes.
- Results: `build.py --check --compile` clean; `tools/test.py` 91/91 C++, 43/43 Python; the
  clean build is installed (overlay off: nothing calls `Start` yet).

## T048 wiring (2026-11-13)

`ApplySettings` calls `diagnostic::Start()` when `diagnosticMode` is on and
`diagnostic::Stop()` when off, after `SetRuleSet`; a failed settings read leaves the mode as
it was. `Wh_ModUninit`: dialog hooks off → `diagnostic::Stop` → `system::Stop` →
`worker::Stop` → `ResetTokens` → `ResetEngine`. US3 is complete.

- Live check: `windhawk-cli mod settings set local@wholeui diagnosticMode=0|1` changes the
  setting and the mod gets `Wh_ModSettingsChanged` (no UI needed). Load with the mode on → one
  visible `WholeUI.Diagnostic` window in one of 3 `explorer.exe`; off → none; on → one;
  off/on/off sent back to back → none; reinstall with the mode on → the old owner let go and
  another process took over, still one window, no `explorer.exe` restarted.
- Results: `build.py --check --compile` clean; `tools/test.py` 91/91 C++, 43/43 Python; the
  clean build is installed (diagnostic mode on).

## Next step

1. T048 is committed
   (`feat: wire diagnostic mode into the mod`, not pushed). T044–T047 and earlier US3/US4
   commits are also unpushed.
2. Phase 6: T049 (build, tests, SC-004 timing), then T050 live acceptance with the
   maintainer (quickstart.md steps 1–11), T051–T054.
