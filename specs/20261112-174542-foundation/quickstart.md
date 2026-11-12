# Quickstart: Foundation

How to build, test and validate this spec. Live-system steps follow the spec's
"Acceptance on a live system".

## Prerequisites

- Windows 11 x64 with Windhawk (maintainer: 2.0.0-alpha.6, portable at
  `D:\Programas\Windhawk`). Set `WINDHAWK_ROOT` if it lives elsewhere.
- Python 3.12 or later.
- The llvm-mingw toolchain pinned in `tools/toolchain.json`, for unit tests and the local
  compile check (`tools/setup_toolchain.py` downloads it into `build/toolchain/`).
- An inspection tool for SC-006 (for example Spy++ or System Informer's window view).

## Build and check (Story 1)

```powershell
python tools/build.py                 # writes build/wholeui.wh.cpp
python tools/build.py --check         # build twice, compare bytes (FR-003)
python tools/build.py --compile       # compile with Windhawk's flags (same as CI)
python tools/test.py                  # unit tests (C++) and build-script tests (Python)
python tools/build.py --install       # install into the local Windhawk (FR-006)
```

Expected: one file with `@id wholeui` and a "generated, do not edit" header; `--check` says
identical; `--compile` and `test.py` exit 0; `--install` reports the mod installed and
enabled, loaded into `explorer.exe` only.

CI (`.github/workflows/ci.yml`) runs the same `--check`, `--compile` and `test.py` on every
pull request. To see it fail, push a branch with a deliberate syntax error: the job fails and
names the source file (`// ---- src/... ----` markers map generated lines back to sources).

## Validate on the live system

| # | Step | Expected | Covers |
|---|---|---|---|
| 1 | Install; check Windhawk's mod details | Loaded in `explorer.exe` only | FR-007 |
| 2 | Use Explorer, taskbar, Start, a context menu, a Properties dialog | Nothing differs from Windows without the mod | SC-003 |
| 3 | Turn on diagnostic mode; point at Explorer, taskbar, Properties, Open/Save dialog, `sysdm.cpl` | Overlay next to the cursor within 200 ms; class, module and process match the inspection tool; `sysdm.cpl` shows "not running WholeUI" and unknown fields | US3, SC-005, SC-006 |
| 4 | Point at a button inside a dialog | Control and top-level both shown | US3-2 |
| 5 | Press Ctrl+Alt+Shift+C, paste in a text editor | One override line and one table row ([format](contracts/compatibility-doc.md)) | FR-021b |
| 6 | Paste the override as a new item, change level to `visual`, save | Overlay on Explorer shows `visual · override` without restarting Explorer; remove it → built-in level | US2-2, US2-4 |
| 7 | Add `title=Explorer; level=full`; open Windhawk's log | Entry ignored, log says why | FR-017 |
| 8 | Add `class=Shell_TrayWnd; level=full` | Taskbar stays `native · shell surface` | FR-015 |
| 9 | Toggle dark/light mode, change accent, set text size 125 % | Log shows tokens re-resolved with the new values | US4 |
| 10 | Switch Windhawk to Português (Brasil) | Name, description, settings in Portuguese; readme has a Portuguese section | FR-009, research R3 |
| 11 | Turn diagnostic mode off; then disable the mod with Explorer open | Overlay gone at once; windows keep working, no Explorer restart | FR-023, SC-008 |

Record the Windows build and the results in `handoff.md`.

## Performance checks

- SC-004: the unit test binary prints the average `classify` time over 200 overrides; it must
  be under 1 ms (expected: microseconds). Not asserted in CI (timing on shared runners is
  noisy), but reported.
- SC-005: step 3, judged by eye and with the overlay's own log line of time from rest to
  update when diagnostic mode is on.
