# Compatibility List (fixture)

Text before the tables is ignored.

## Windows

| Window | Process | Module | Class | Template | Level | Windows build | Validated |
|---|---|---|---|---|---|---|---|
| File Explorer | explorer.exe | — | CabinetWClass | — | to do | — | — |
| System Properties | - | `sysdm.cpl` | — | — | native | 26300.1 | 2026-11-13 |
| Run dialog | `explorer.exe` | shell32.dll | `#32770` | 1003 | visual | — | — |
| Say "hi" \ back | — | — | Quoted | — | full | — | — |

## Notes

| Other | Table |
|---|---|
| is | ignored |

## Shell surfaces (always native)

| Surface | Process | Class |
|---|---|---|
| Taskbar | explorer.exe | Shell_TrayWnd |
| Desktop | `explorer.exe` | `Progman` |
