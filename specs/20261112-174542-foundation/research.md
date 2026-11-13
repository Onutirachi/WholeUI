# Research: Foundation

**Spec**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md) | **Date**: 2026-11-12

Each item records the decision, why, and what else was considered. Items marked
**Verify** are decided but depend on a fact that can only be confirmed on the maintainer's
machine or in CI; the first tasks confirm them and update this file.

No other mod's code was read for this research. Facts about Windhawk come from the
maintainer's local installation (`D:\Programas\Windhawk`, version 2.0.0-alpha.6), its
command-line help, its editor workspace and the public Windhawk wiki.

## R1. Windhawk version, compiler and flags (FR-004)

**Findings**

- The maintainer runs Windhawk **2.0.0-alpha.6**, portable, engine `Engine\2.0.0`, UI
  language `pt-BR`, `AlwaysCompileModsLocally=0`. Its `Compiler` folder is empty: installed
  mods were downloaded precompiled, so the bundled toolchain has not been fetched yet.
- The editor workspace (`AppData\EditorWorkspace\compile_flags.txt`) lists the flags Windhawk
  gives the language server, which mirror the compile flags:
  `-x c++ -std=c++23 -target x86_64-w64-mingw32 -DUNICODE -D_UNICODE -DWINVER=0x0A00
  -D_WIN32_WINNT=0x0A00 -D_WIN32_IE=0x0A00 -DNTDDI_VERSION=0x0A000008
  -D__USE_MINGW_ANSI_STDIO=0 -DWH_MOD -include windhawk_api.h -Wall -Wextra
  -Wno-unused-parameter -Wno-missing-field-initializers -Wno-cast-function-type-mismatch`.
- Strings in `windhawk-core.dll` show the extra compile-only flags: `-DWH_MOD_ID=L"…"`,
  `-DWH_MOD_VERSION=L"…"`, `-DWH_WINDHAWK_VERSION=…`, `-mguard=cf-nochecks`,
  `-Wno-unneeded-internal-declaration`, `-Wno-deprecated-declarations`,
  `-Wl,--export-all-symbols`, linking `windhawk.lib`, targets `i686`, `x86_64` and `aarch64`
  `-w64-mingw32`, and precompiled header support. The optimization level is not visible as a
  literal string.
- `windhawk-cli.exe mod install --file <path>` compiles a local `.wh.cpp` with Windhawk's own
  toolchain and flags and installs it; `--disabled` installs without enabling.

**Decision**

**Verified 2026-11-12** (the maintainer installed Windhawk's developer tools from the UI; a
probe mod was compiled with `windhawk-cli mod install --file --disabled`, then removed):

- `windhawk-cli` does not download the compiler: with an empty `Compiler` folder it fails
  ("failed to run bin\clang++.exe"). The download is a UI action ("install dev tools").
- Toolchain: **clang 20.1.3** (llvm-project `923a5c4f83d2`), **mingw-w64 13.0.0**, standard
  llvm-mingw layout (`-rtlib=compiler-rt -unwindlib=libunwind -stdlib=libc++
  -fuse-ld=lld`).
- Windhawk files: `Compiler\include\windhawk_api.h`, `windhawk_api_internal.h`,
  `windhawk_utils.h`, `windhawk_tool_mod.h`; import library `Engine\2.0.0_1\64\windhawk.lib`.
- `Compiler\compile_flags.txt` adds `-Wno-pragma-pack` and
  `-Wno-pragma-system-header-outside-header` to the editor flags listed above.

**Decision**

- CI compiles the generated file with the **public llvm-mingw release built on LLVM 20.1.3
  (UCRT, x86_64 host)**, with the flags above plus the mod's `@compilerOptions`, for target
  `x86_64-w64-mingw32` only (see R2). *Superseded by the setup findings below: Windhawk uses
  the msvcrt, i686-host release.*
- Windhawk's headers and `windhawk.lib` are not published as a separate download. The setup
  task checks their license in the Windhawk repository; if it allows (Windhawk is
  GPL-3.0, compatible with WholeUI), the four headers and the import library are copied into
  `tools/windhawk-sdk/` with a note naming their Windhawk version. Otherwise CI generates the
  import library from a `.def` listing the `Wh_*` exports and fetches the headers from the
  Windhawk repository at the matching tag.

**Setup findings (T002, T003; 2026-11-12)**

- Windhawk's compiler is **llvm-mingw 20250417, msvcrt variant, i686 host**
  (`llvm-mingw-20250417-msvcrt-i686.zip`), not UCRT/x86_64: `__MSVCRT_VERSION__` is `0x600`,
  and `ramensoftware/windhawk-dependencies` (release v1.6) states that `windhawk-compiler-1.6`
  is a byte-identical subset of that archive. Compared file by file: all compiler binaries
  match. Windhawk 2.0's installer then (a) renames the runtime DLL imports in
  `libc++.dll.a` / `libunwind.dll.a` to `libc++.whl` / `libunwind.whl`, (b) removes a duplicate
  `IReference<boolean>` block from `windows.foundation.h`, (c) adds C++/WinRT (`include/winrt`),
  WIL (`include/wil`) and the four Windhawk headers. None of these affect a compile check of
  WholeUI, which uses the WinRT ABI headers and neither C++/WinRT nor WIL; the DLL CI builds is
  never shipped. If WholeUI ever uses C++/WinRT or WIL, CI must add them.
- The real compile flags come from `build_compile_args` in windhawk-core
  (`src/windhawk-core/core/src/services/compiler/flags.rs`, public on `main`): `-std=c++23 -O2
  -ffp-exception-behavior=maytrap -mguard=cf-nochecks -shared -s -DUNICODE -D_UNICODE
  -Wno-unneeded-internal-declaration -Wno-deprecated-declarations`, the Windows version
  defines, `-D__USE_MINGW_ANSI_STDIO=0 -DWH_MOD -DWH_MOD_ID -DWH_MOD_VERSION
  -DWH_WINDHAWK_VERSION=0x<maj><min><patch>00`, `windhawk.lib`, `-include windhawk_api.h
  -target x86_64-w64-mingw32 -Wl,--export-all-symbols`, then the mod's options. `-Wall
  -Wextra` and the other warning flags are the editor set only; WholeUI's check adds them
  on top (`warning_flags` in `tools/toolchain.json`).
- License: Windhawk is GPL-3.0. The four headers ship with Windhawk (installer, developer
  tools) and are not in the repository's source tree; `windhawk.lib` is the engine's import
  library (23 `InternalWh_*` exports, a subset of `src/windhawk/engine/_exports.def`). Copied
  unchanged into `tools/windhawk-sdk/` with version, source and hashes; no `.def` route.
- `tools/toolchain.json` pins the archive (URL, SHA-256 `f38c6a8c…3090`, size) and the flags;
  `tools/setup_toolchain.py` fetches it. A probe mod compiled with these flags, the SDK copy
  and the mod libraries, with no warnings. (T015 later dropped `mod_libraries` from
  `toolchain.json`: `--compile` appends the header's `@compilerOptions`, as Windhawk does.)
- Locally, `build.py --compile` uses `$WINDHAWK_ROOT\Compiler` directly, so the maintainer's
  check uses Windhawk's own files.

**Alternatives considered**

- Compile with MSVC or a system clang in CI: rejected, different compiler and headers
  (FR-004 requires Windhawk's).
- Compile through `windhawk-cli` in CI: rejected, the CLI cannot install the compiler, so
  CI would still need the toolchain itself.

## R2. Target architecture

**Decision**: `@architecture x86-64` and `@include explorer.exe`. The mod is compiled and
validated for x64 `explorer.exe` only.

**Rationale**: the maintainer's machine is x64; FR-007 limits the mod to `explorer.exe`,
which is a 64-bit process on x64 Windows. ARM64 is not tested by anyone on the project, and
constitution I prefers "not loaded" to "loaded untested".

**Alternatives**: no `@architecture` (compiles i686, x86_64 and, on Windhawk 2.0, aarch64):
rejected until someone can validate ARM64.

## R3. Localization (FR-009)

**Findings**: installed mods use `// @name:pt-BR`, `// @description:pt-BR` (also `:pt`,
`:es`, `:zh-CN`). `windhawk-core.dll` contains the messages "Metadata parameter can't be
localized" and "$options and its language variants must be marked together", which show that
setting fields (`$name`, `$description`, `$options`) accept language variants. No evidence was
found of a localized readme block.

**Decision**

- Name and description: `@name:pt-BR`, `@description:pt-BR`.
- Settings: `$name:pt-BR` and `$description:pt-BR` beside every `$name` / `$description`.
- Readme: one readme containing English first and a "Português (Brasil)" section after it.
  This is the gap the spec's assumptions allow for: Windhawk shows one readme, so both
  languages are in it.
- **Verify** at acceptance step 7: with Windhawk in pt-BR, name, description and settings show
  in Portuguese; with Windhawk in another language, English shows. If `$name:pt-BR` is not
  honored by the maintainer's version, the setting stays English and this item is updated.

**Alternatives**: separate pt-BR mod: rejected (one mod, id `wholeui`).

## R4. Build script (FR-001 to FR-003, FR-006)

**Decision**: one Python 3 script, standard library only, `tools/build.py`. It produces
`build/wholeui.wh.cpp` as an **amalgamation**: starting from `src/main.cpp`, each
`#include "…"` of a project file is replaced by that file's content the first time it is
seen (and dropped after), while `#include <…>` lines are kept. Metadata, readme and settings
come from `src/mod/` and are written first between Windhawk's markers. The built-in
compatibility rules are generated from `docs/compatibility.md` (R9). The output uses LF line
endings, UTF-8 without BOM, no timestamps, and each inlined file is preceded by a
`// ---- src/<path> ----` line, so builds are byte-identical (FR-003) and errors point to a
source file. `--install` runs `windhawk-cli mod install --file build/wholeui.wh.cpp`
(FR-006); the path to `windhawk-cli.exe` comes from `WINDHAWK_ROOT` or the default portable
path, never hard-coded in the source tree.

**Implementation notes (T012–T018)**

- A repeated project include is replaced by `// (already inlined: src/<path>)`, so line
  counts stay intact. After an inlined file the including file resumes with
  `// ---- src/<path>:<line> ----` (the next line is `<line>` of that file). These two marker
  forms map every generated line back to a source line; a source line that looks like a
  marker fails the build. Compiler output has `wholeui.wh.cpp:<line>` rewritten to
  `src/<path>:<line>`; the code excerpt clang prints still shows the generated line number.
- Sources are read as UTF-8 with any BOM dropped and CRLF turned into LF; a `*/` in the readme
  or settings fails the build (it would end the comment block).
- `--compile` uses Windhawk's compiler in `$WINDHAWK_ROOT\Compiler` when present (its
  headers are built in), else the pinned one with `-I tools/windhawk-sdk/include`; the import
  library is always `tools/windhawk-sdk/64/windhawk.lib` (the engine folder name varies per
  install). Flags come from `toolchain.json`, then `warning_flags`, then the header's
  `@compilerOptions`.
- `windhawk-cli mod install --file` installs the mod as **`local@wholeui`** (local installs
  get the `local@` prefix).
- `tools/test.py` derives the test flags from `compile_flags` (dropping the DLL-only ones and
  every `-DWH_*`) and links statically, so the test executable needs no runtime DLLs.
- `tests/tools/test_build.py` loads `tools/build.py` by path: `import build` can resolve to
  PyPA's `build` package.

**Rationale**: Python 3.14 is installed on the maintainer's machine and on GitHub runners,
and `.gitignore` already anticipates Python tools. An amalgamation keeps normal
`#include`-based sources, so the same files compile in unit tests and in the IDE.

**Alternatives**: PowerShell (5.1 encoding defaults make byte-identical output fragile);
Node (another runtime to install); a manifest-ordered concatenation (order kept by hand,
easy to break; includes already describe the order).

## R5. Unit tests (FR-005, SC-007)

**Decision**: pure logic lives in files that include only the C++ standard library (no
`windows.h`, no Windhawk API). Tests are compiled by the same llvm-mingw toolchain (native on
the Windows runner) with a small in-repo test harness (`tests/unit/harness.h`, about 80
lines: test registration, `CHECK`, `CHECK_EQ`, failure with file and line, non-zero exit). The
build script's parser of `docs/compatibility.md` is tested with Python's `unittest`.

**Rationale**: no new dependency (constitution VII); the tests need no desktop session.

**Alternatives**: doctest or Catch2 (more features than needed, a vendored dependency to
justify); GoogleTest (build system needed).

## R6. Getting to know a window: lazy classification

**Decision**: the engine classifies a window **when first asked** (by a later spec, or by
diagnostic mode) and caches the result per `HWND`. The only work done at window creation is
recording facts that cannot be read later: the dialog template ID and the pages of property
sheets (R7, R8). No global window creation hook, no per-window subclassing.

**Rationale**: in this spec nobody asks, so the mod costs nothing in normal use (FR-008,
FR-028). Later specs ask at the moment they would change a window.

**Cache validity**: an entry stores the window's thread ID and class atom; a lookup that finds
a different thread or atom (HWND reused) recomputes. Entries for destroyed windows are
dropped on the next settings change or when the cache passes 4,096 entries (a sweep with
`IsWindow`). Classification state lives only in the mod's memory, so unloading frees it
(FR-029).

## R7. Identity fields and how each is read (FR-010, FR-021)

| Field | In-process (`explorer.exe`) | Other process (diagnostic only) |
|---|---|---|
| Process image | `GetModuleFileNameW(nullptr)` → file name | `GetWindowThreadProcessId` + `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)` + `QueryFullProcessImageNameW` |
| Creating module | `GetWindowLongPtrW(GWLP_HINSTANCE)` → `GetModuleFileNameW` | same `HINSTANCE` value + `OpenProcess(PROCESS_QUERY_INFORMATION \| PROCESS_VM_READ)` + `GetModuleFileNameExW`; "unknown" when access is denied (elevated or protected processes) |
| Window class | `GetClassNameW` | `GetClassNameW` (works across processes) |
| Template ID | from the dialog facts store (R8); "not a dialog" when the class is not `#32770`; "unknown" for a dialog created before the mod loaded or through an indirect template | always "unknown" for dialogs |

All are documented Win32 functions. File names are compared case-insensitively, without the
path. "Creating module" is the `HINSTANCE` the creator passed to `CreateWindowEx` /
`CreateDialog*`: for `sysdm.cpl` that is `sysdm.cpl`, which matches
`docs/compatibility.md`. A caller that passes another module's `HINSTANCE` is identified by
that module; this is the only reliable signal Windows keeps, and diagnostic mode shows what
the engine sees, so a rule can be written against it.

**Alternatives**: return address of the `CreateWindowExW` caller (needs a hook on every
window creation and stack inspection; heavier and less stable); window titles (forbidden).

## R8. Dialog template IDs and property sheet pages (FR-013a)

**Decision**

- Hooks (Windhawk `Wh_SetFunctionHook` on documented `user32` exports): `CreateDialogParamW`,
  `DialogBoxParamW`, `CreateDialogIndirectParamW`, `DialogBoxIndirectParamW`. During the call
  the hook installs a **thread-local** `WH_CBT` hook that, at the first `HCBT_CREATEWND` for a
  `#32770` window with the same `HINSTANCE`, records `{HWND → module, template}` (template
  ID only for the non-indirect variants) and removes itself. The CBT hook is installed only on
  the calling thread and only for the duration of the call; it changes nothing.
- Hooks on `comctl32` exports `CreatePropertySheetPageW` (records page handle → page
  `HINSTANCE` and `pszTemplate`) and `PropertySheetW` (records the pages listed in the header,
  either as handles or inline `PROPSHEETPAGEW`, as a thread-local "pending sheet"). When the
  sheet's top-level dialog is created on that thread during `PropertySheetW`, the pending page
  list is attached to that `HWND`. The sheet's level is the lowest of the host and all listed
  pages, decided before the sheet is shown, as FR-013a requires; pages are only created when
  their tab is first selected, which is why the list comes from the header and not from the
  page windows.
- A page added later (a child `#32770` of a known sheet, created through
  `CreateDialogIndirectParamW` with an `HINSTANCE` not in the list) lowers the sheet's level if
  its module's level is lower; the engine then notifies listeners (FR-019), so the later spec
  that styled the sheet restores it.
- Hooks are set in `Wh_ModInit`; Windhawk removes them on unload. Hooking only the `W`
  variants is enough for `explorer.exe`; dialogs created through `A` variants get template
  "unknown", which is safe (rules naming a template do not match).
- **Verify** (spike task, own branch): open a file's Properties dialog with a third-party
  shell extension page and confirm that the page's module is listed before the sheet is shown,
  and that a dialog from `shell32.dll` gets its template ID.

**Spike findings** (T032, 2026-11-13, branch `spike/20261112-dialog-capture`, Windows 11
build 26300, comctl32 6.0.26100.9711). The spike hooked the functions above plus
`kernelbase` `FindResourceW` / `FindResourceExW`, and logged only. Cases: Run dialog; Properties
of a WinRAR self-extracting `.exe` (third-party page from `rarext.dll`) and of a signed `.exe`;
Properties of drive C: (Quota page from `dskquoui.dll`); Folder Options.

1. **Pages are known before the sheet window exists.** In every sheet, `shell32.dll` (or
   `explorerframe.dll` for Folder Options) creates every page through
   `CreatePropertySheetPageW` and then calls `PropertySheetW` with **handles** (never inline
   pages, never `PSH_MODELESS`). The header listed all pages, with module and template, before
   the sheet's `#32770` was created, and that window is created without `WS_VISIBLE`. The
   `rarext.dll` page was listed with template `"EXTARCINFODLG"` (a resource name, which the
   contract already allows).
2. **No dialog in these cases went through the non-indirect functions.** The Run dialog
   (`shell32.dll`) calls `DialogBoxIndirectParamW` with a template copied to private memory, so
   neither the call arguments nor a reverse search of the module's resources can name it.
   Property sheets do the same: the sheet frame is a `CreateDialogIndirectParamW` with
   `HINSTANCE` = `comctl32.dll` (resource `#1006`), and each page window is a
   `CreateDialogIndirectParamW` from `comctl32.dll` with the page's `HINSTANCE` and a copied
   template. As designed, template IDs would be "unknown" for every dialog observed.
3. **The template ID is recoverable from the resource lookup.** Right before each indirect
   call, the same thread calls `FindResourceExW(module, RT_DIALOG, name)` with `module` equal to
   the `HINSTANCE` later passed to the dialog function (Run: `shell32.dll #1003`; sheet frame:
   `comctl32.dll #1006`; pages: their own IDs). Other lookups can come in between (comctl32
   looks up every page's template while building the sheet), so the match must take the most
   recent lookup on the thread **for the same module**, not simply the last one.
4. **Page windows are created when their tab is first selected**, each through the exported
   `CreateDialogIndirectParamW` (so our hook sees it) with the sheet frame as parent. Every page
   window matched a page listed in the header; no page was added later in these cases.
5. **The page's module is its `HINSTANCE`, not the code that created it**: `cryptext.dll`
   creates the Digital Signatures page with `HINSTANCE` = `CRYPTUI.dll`. Rules for that page
   must name `cryptui.dll`. This matches R7 (`GWLP_HINSTANCE`).
6. **Each Properties sheet runs modally on its own thread**; the CBT hook "for the duration of
   the call" therefore lasts while the sheet is open, on that thread only.
7. **comctl32 may not be loaded at `Wh_ModInit`**: short-lived `explorer.exe` processes (the
   one started by `explorer.exe <folder>`, which hands off and exits) loaded the mod before
   `comctl32.dll`. In the main `explorer.exe` only the v6 `comctl32.dll` was loaded.

**Revision** (approved by the maintainer on 2026-11-13; it replaces the bullets of the decision above where they differ, and T033 follows it):

- Add hooks on `kernelbase` `FindResourceExW` and `FindResourceW`: when the type is
  `RT_DIALOG` and the call succeeds, remember `{module, name}` in a small thread-local list
  (last 16 lookups). Cost outside dialogs: one comparison per call.
- In the four `user32` dialog hooks, the template is the call's argument for the non-indirect
  variants; for the indirect variants it is the most recent remembered lookup whose module
  equals the call's `HINSTANCE`, or "unknown" if there is none. The list is cleared when the
  outermost dialog call on the thread returns.
- The sheet window is the **first** `#32770` created on the thread during `PropertySheetW`,
  whatever hooked call it is nested in (it is created inside `CreateDialogIndirectParamW`).
- If `comctl32.dll` is not loaded at `Wh_ModInit`, hook `PropertySheetW` and
  `CreatePropertySheetPageW` when it loads (through Windhawk's module-load notification or a
  `LoadLibraryExW` hook, chosen in T033); only the instance(s) actually loaded are hooked.

**Alternatives**: wrapping the dialog procedure to catch `WM_INITDIALOG` (changes the window's
`DWLP_DLGPROC`, even if briefly; rejected by FR-008); a global `SetWinEventHook` for object
creation (asynchronous, arrives after the window is shown).

## R9. Built-in rules from `docs/compatibility.md` (FR-014, FR-015)

**Decision**: the build script parses the table in `docs/compatibility.md` and generates the
built-in rules, so the two cannot drift. The table gets separate `Class` and `Template`
columns (today one column); `—` means "not named". `to do` ships as `native`. Shell surfaces
move to their own table, "Shell surfaces (always native)", whose rows become **locked** rules:
when the top-level window matches a locked rule, the level is `native` and overrides are not
consulted. A row that cannot be parsed fails the build with the line number. The format is in
[contracts/compatibility-doc.md](contracts/compatibility-doc.md).

Initial shell-surface rows (process `explorer.exe`), to confirm with diagnostic mode during
acceptance: `Shell_TrayWnd`, `Shell_SecondaryTrayWnd`, `NotifyIconOverflowWindow`,
`TopLevelWindowForOverflowXamlIsland`, `XamlExplorerHostIslandWindow`, `Progman`, `WorkerW`,
`Shell_InputSwitchTopLevelWindow`. Start is a separate process
(`StartMenuExperienceHost.exe`), where the mod does not run.

## R10. Overrides setting format (FR-016, FR-017, FR-021b)

**Decision**: S6 is a Windhawk array of **strings**, one override per string, in a short
`field=value; …; level=…` syntax (contract:
[contracts/settings.md](contracts/settings.md)). The diagnostic copy shortcut produces
exactly such a string, so pasting it into one new array item is the whole workflow.

**Rationale**: Windhawk's array-of-objects settings are edited field by field and cannot be
pasted as one item; FR-021b asks for a ready-to-paste override.

**Alternatives**: array of objects with `process`, `module`, `class`, `template`, `level`
(friendlier fields, but no single paste); one multi-line text (harder to edit, one error could
hide others).

## R11. System mode, accent and text scale (FR-025)

**Decision**: read all three from the documented WinRT `Windows.UI.ViewManagement.UISettings`
class: `GetColorValue(UIColorType::Accent, AccentLight1-3, AccentDark1-3)` for the accent and
its shades exactly as Windows computes them, `GetColorValue(Background)` to tell dark from
light app mode (dark when the background is dark, Microsoft's documented method),
`TextScaleFactor` for the text size. Changes arrive through `ColorValuesChanged` and
`TextScaleFactorChanged`, handled on the mod's worker thread (R13). The object is created
through the raw ABI (`RoGetActivationFactory`, link `-lruntimeobject`), with no C++/WinRT.
If any read fails, the Windows defaults apply (light, accent `#0078D4` with its default
shades, 100 %) and the failure is logged (spec edge case).

**Verified 2026-11-12**: Windhawk's toolchain ships `windows.ui.viewmanagement.h`
(`__has_include` true in a probe compiled by Windhawk), so the project uses it and declares
no WinRT interfaces itself.

**Alternatives**: registry values (`AppsUseLightTheme`, `AccentPalette`, `TextScaleFactor`)
with `RegNotifyChangeKeyValue`: simpler, but `AccentPalette` is undocumented binary data and
the shades would be guessed; kept only as an idea, not implemented.

## R12. Font availability (US4 scenario 4)

**Decision**: at token resolution the mod asks DirectWrite's system font collection for the
family "Segoe UI Variable Text" (`IDWriteFactory::GetSystemFontCollection` →
`FindFamilyName`). Present: the font family token is "Segoe UI Variable" with the GDI face
names "Segoe UI Variable Text" (type sizes below 20 px) and "Segoe UI Variable Display"
(20 px and above). Absent: "Segoe UI" everywhere. The font check is an input to the pure
resolver, so the fallback is unit-tested. The 20 px split will be revalidated in the Explorer
core spec (F06).

## R13. Threads and lifecycle (FR-023, FR-029)

**Decision**: one worker thread per mod instance, started in `Wh_ModInit` and stopped in
`Wh_ModUninit` (posted quit, then wait). It owns the WinRT `UISettings` object and its event
registrations, the diagnostic overlay window and its timer. The compatibility engine has no
thread: callers query it from their own threads. The rule set is an immutable snapshot
swapped under an `SRWLOCK` when settings change (`Wh_ModSettingsChanged`); the cache is
guarded by the same lock. Listeners (FR-019) are called after the lock is released.

## R14. Diagnostic mode (FR-020 to FR-023, SC-005)

**Decision**

- **One instance per session**: `explorer.exe` can run as several processes (separate folder
  windows). Only the instance that owns the named mutex `Local\WholeUI.Diagnostic` shows the
  overlay; the others stay idle and retry when the owner exits.
- **Cursor tracking**: a 50 ms timer reads `GetCursorPos` and `WindowFromPoint`. Identity is
  refreshed when the window under the cursor changes or 100 ms after the cursor stops, which
  meets the 200 ms budget. No low-level mouse or keyboard hook, so input latency is unchanged.
- **Overlay**: a layered, click-through window (`WS_EX_LAYERED | WS_EX_TRANSPARENT |
  WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST`), never activated, drawn with
  Direct2D/DirectWrite into a DIB and pushed with `UpdateLayeredWindow`, using token values
  (font, radius, palette). It sits 16 px below-right of the cursor and flips to the other side
  near screen edges, so it never covers the inspected point; click-through windows are not
  returned by `WindowFromPoint`.
- **Copy shortcut**: **Ctrl+Alt+Shift+C**, detected by `GetAsyncKeyState` in the same timer
  (edge-triggered). Polling does not consume the keystroke (FR-022), and it is checked only
  while diagnostic mode is on. The shortcut is fixed, not a setting.
- **Clipboard**: `OpenClipboard(overlay)` / `SetClipboardData(CF_UNICODETEXT)` with two lines
  (contract: [contracts/compatibility-doc.md](contracts/compatibility-doc.md)).
- **Other processes**: identified as in R7; the level shown is what the rules give for that
  identity, with "unknown" fields not matching.
- **Off / unload**: the window is destroyed, timer killed, mutex released at once.

## R15. Design token values (FR-024)

**Decision**: values from WinUI 3 / Fluent theme resources and the maintainer's reference
settings, listed in full in [data-model.md](data-model.md#design-token-set). Text scale
multiplies type sizes and line heights only (FR-026). Colors are stored as ARGB.

## R16. Windows internals used

None. Every Windows function in this plan is documented (Win32, WinRT, DirectWrite,
Direct2D); hooks target documented exports of `user32.dll` and `comctl32.dll`. Class names
in the built-in rules (R9) are observed values, not APIs; when Windows changes one, the rule
stops matching and the window falls back to `native`, which is safe.
