# Windhawk SDK files

Copies of the files Windhawk gives every mod at compile time, so CI can compile WholeUI
without installing Windhawk. Local builds use the files of the installed Windhawk instead
(`$WINDHAWK_ROOT`, see `tools/build.py`).

| File | Source in a Windhawk install | SHA-256 |
|---|---|---|
| `include/windhawk_api.h` | `Compiler\include\` | `66a50a89b81bc437c26ce3374774f1c545448e9f5bcf2580001653050b6dd4be` |
| `include/windhawk_api_internal.h` | `Compiler\include\` | `3bf72ba5e844a1edf297cf14b7e949fbcb8ee5f00c5737f93dd22dda8c905b27` |
| `include/windhawk_utils.h` | `Compiler\include\` | `52bba0adeed2da31ff33882b48f51b72be72615acabc7bb0b4405c553c97f9ae` |
| `include/windhawk_tool_mod.h` | `Compiler\include\` | `72681861fb9d6000cc4a76c4d5250eb8b25719e6aa06b2f1bdc55a4147efe8dc` |
| `64/windhawk.lib` | `Engine\2.0.0_1\64\` | `1b8bc8f6bc59d155f0dbd4474a38bec759f3a5dfa9f039039e199b601899f168` |

- **Windhawk version**: 2.0.0-alpha.6 (developer tools installed from the Windhawk UI).
- **Copyright**: Ramen Software. **License**: GPL-3.0, the license of Windhawk
  (<https://github.com/ramensoftware/windhawk>). The headers are part of the Windhawk
  distribution (added by its installer with the developer tools) and are not in the
  repository's source tree; `windhawk.lib` is the import library of the engine, whose export
  list is `src/windhawk/engine/_exports.def` in that repository.
- The files are unchanged; `.gitattributes` keeps the headers' CRLF line endings so the
  hashes above stay valid.

## Updating

When the maintainer moves to a new Windhawk release, copy the five files again from that
install, update the version and hashes here and `windhawk.version` in `tools/toolchain.json`.
