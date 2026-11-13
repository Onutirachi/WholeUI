# Contract: `docs/compatibility.md` format and diagnostic output

## Table format (parsed by `tools/build.py`)

`docs/compatibility.md` keeps two tables. The build script finds them by their headings and
fails with the line number on any row it cannot parse.

### `## Windows`

```markdown
| Window | Process | Module | Class | Template | Level | Windows build | Validated |
|---|---|---|---|---|---|---|---|
| File Explorer | explorer.exe | — | CabinetWClass | — | to do | — | — |
| System Properties | — | sysdm.cpl | — | — | native | — | — |
```

- `—` (em dash) or `-` means "not named". At least one of Process, Module, Class, Template is
  named.
- Values may be wrapped in backticks; they are stripped.
- Level: `native`, `visual`, `full` or `to do` (ships as `native`).
- Windows build and Validated are free text, not used by the code; Validated is a date
  `YYYY-MM-DD` or `—`.

### `## Shell surfaces (always native)`

```markdown
| Surface | Process | Class |
|---|---|---|
| Taskbar | explorer.exe | Shell_TrayWnd |
```

Every row becomes a **locked** `native` rule matched against the top-level window.

## Generated code

The script writes `build/gen/builtin_rules.inc`, included by the amalgamation: one
initializer per row, in document order, with the window name as `origin`. Same input, same
bytes.

## Diagnostic overlay text

```text
Window        <class>  ·  <module>  ·  template <id | not a dialog | unknown>
Top-level     <class>  ·  <module>  ·  template <…>
Process       <process>  (not running WholeUI)      ← second part only for other processes
Level         <level>  ·  <reason>  ·  <rule origin>
Ctrl+Alt+Shift+C copies
```

`<reason>` is one of: `default`, `rule`, `override`, `shell surface`, `capped by top-level`,
`capped by page`, `failure`; `· <rule origin>` is left out when no rule matched. Any
unreadable (or empty) field shows `unknown`. When the window is itself top-level, the
Top-level line repeats it.

In the text built by `diagnostic/format` (`OverlayText`) each line is `<label>` TAB `<value>`
(the overlay draws with a proportional font and aligns the values on a tab stop, so the
padding above is only for reading); the hint line has no tab. Lines are separated by CRLF.

## Clipboard (Ctrl+Alt+Shift+C)

Two lines for the window under the cursor's **top-level** window (the level applies per
top-level window); fields that are `unknown` or `not a dialog` are left out of the override and
written as `—` in the table row. The level written is the current effective level.

```text
process=explorer.exe; module=explorer.exe; class=CabinetWClass; level=native
| <name> | explorer.exe | explorer.exe | CabinetWClass | — | native | <build> | — |
```

`<name>` is a placeholder the maintainer replaces; `<build>` is the current Windows build as
`<CurrentBuild>.<UBR>` (for example `26300.1234`), read with `RegGetValueW` from
`HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion`, or `—` if either read fails. The two
lines are separated by CRLF. The Validated cell is `—`. If every identity field is unknown,
the override line is only `level=<level>` (the settings parser reports it as `no identity
field`), which is accepted: such a window cannot be targeted anyway.
