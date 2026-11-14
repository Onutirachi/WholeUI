# Contract: Mod settings

The settings block in the generated mod (Windhawk YAML). English with pt-BR variants
(research R3).

```yaml
- overrides: [""]
  $name: Compatibility overrides
  $name:pt-BR: Exceções de compatibilidade
  $description: >-
    One window per line, for example
    "process=explorer.exe; class=CabinetWClass; level=visual".
    Use diagnostic mode to copy a ready line.
  $description:pt-BR: >-
    Uma janela por linha, por exemplo
    "process=explorer.exe; class=CabinetWClass; level=visual".
    Use o modo de diagnóstico para copiar uma linha pronta.
- diagnosticMode: false
  $name: Diagnostic mode
  $name:pt-BR: Modo de diagnóstico
  $description: >-
    Shows the identity and level of the window under the cursor.
    Ctrl+Alt+Shift+C copies it.
  $description:pt-BR: >-
    Mostra a identidade e o nível da janela sob o cursor.
    Ctrl+Alt+Shift+C copia.
```

The code reads `overrides[i]` with `Wh_GetStringSetting` and `diagnosticMode` with
`Wh_GetIntSetting`. Windhawk returns an empty string both for an empty item and for an index
past the end, so reading stops after 16 empty results in a row: an empty line between
entries does not hide the entries after it.

## Override syntax

```text
override  = field *( ";" field )
field     = ws name ws "=" ws value ws
name      = "process" / "module" / "class" / "template" / "level"
value     = 1*( any character except ";" ), trimmed
```

- `level` is required, exactly once, value `native`, `visual` or `full`
  (case-insensitive).
- At least one of `process`, `module`, `class`, `template`, each at most once.
- `process` and `module` are file names without a path; compared case-insensitively.
- `class` is compared case-insensitively.
- `template` is a decimal number (`1040`) or a resource name; compared case-insensitively.
- Empty strings (the empty array item Windhawk shows by default) are skipped silently.

Examples:

```text
process=explorer.exe; class=CabinetWClass; level=visual
module=sysdm.cpl; level=native
module=shell32.dll; class=#32770; template=1040; level=full
```

## Errors (FR-017)

Each invalid entry is skipped and logged as
`override <index> ignored: <reason>: "<text>"`. Reasons:

| Reason | Example |
|---|---|
| `missing level` | `process=explorer.exe` |
| `unknown level` | `process=explorer.exe; level=dark` |
| `no identity field` | `level=full` |
| `unknown field "<name>"` | `title=Explorer; level=full` |
| `duplicate field "<name>"` | `class=A; class=B; level=full` |
| `empty value for "<name>"` | `class=; level=full` |
| `path not allowed in "<name>"` | `module=C:\Windows\shell32.dll; level=full` |
| `malformed field "<text>"` | `explorer.exe; level=full` |

An override that names a locked shell surface is accepted but has no effect (FR-015); it is
logged once as `override <index> has no effect: shell surface`.
