# WholeUI — Compatibility List

Every window WholeUI may touch, with its level and the validation record. Unknown windows are
`native`. See constitution principle II.

`tools/build.py` turns the two tables below into the mod's built-in rules, so keep their
format (specs/20261112-174542-foundation/contracts/compatibility-doc.md): `—` means "not
named", and every row names at least one of Process, Module, Class or Template. A row the
script cannot read fails the build with its line number.

Levels: `native` (untouched), `visual` (appearance only), `full` (dark mode, backdrop, and
geometry where WholeUI draws). `to do` ships as `native`.

## Windows

| Window | Process | Module | Class | Template | Level | Windows build | Validated |
|---|---|---|---|---|---|---|---|
| File Explorer | explorer.exe | — | CabinetWClass | — | to do | — | — |
| System Properties | — | sysdm.cpl | — | — | native | — | — |

## Shell surfaces (always native)

Top-level windows of the Windows shell. WholeUI never changes them or their child windows, and
overrides cannot change that (FR-015). Start is a separate process
(`StartMenuExperienceHost.exe`), where the mod does not run.

| Surface | Process | Class |
|---|---|---|
| Taskbar | explorer.exe | Shell_TrayWnd |
| Taskbar on other monitors | explorer.exe | Shell_SecondaryTrayWnd |
| Notification area overflow | explorer.exe | NotifyIconOverflowWindow |
| Notification area overflow (XAML) | explorer.exe | TopLevelWindowForOverflowXamlIsland |
| Shell XAML host | explorer.exe | XamlExplorerHostIslandWindow |
| Desktop | explorer.exe | Progman |
| Desktop wallpaper | explorer.exe | WorkerW |
| Input switcher | explorer.exe | Shell_InputSwitchTopLevelWindow |
