# WholeUI

One Windhawk mod for a coherent, fluid Windows 11 interface: a GPU-accelerated File Explorer
with smooth animations, a unified backdrop, consistent WinUI typography and icons, and
modernized Win32 windows that are validated one by one so none becomes hard to read or use.

Status: the foundation is implemented (build, compatibility engine, design tokens and
diagnostic mode); nothing visible changes yet. See [the roadmap](docs/roadmap.md).

## Build

Requires Windows 11 x64, Windhawk and Python 3.12 or later. Set `WINDHAWK_ROOT` to the
Windhawk folder (the default is in `tools/toolchain.json`). `--compile` and the tests use
Windhawk's own compiler when it is installed; otherwise run `python tools/setup_toolchain.py`
once to download the llvm-mingw toolchain pinned in `tools/toolchain.json`.

```powershell
python tools/build.py                 # writes build/wholeui.wh.cpp
python tools/build.py --check         # build twice and compare the bytes
python tools/build.py --compile       # compile with Windhawk's flags (same as CI)
python tools/test.py                  # unit tests (C++) and build-script tests (Python)
python tools/build.py --install       # install into the local Windhawk
```

The generated `.wh.cpp` is never edited by hand; change the sources in `src/` and rebuild.

## License

GPL-3.0-only. See [LICENSE](LICENSE). WholeUI is written from scratch; the mods that inspired
it are listed in [credits](docs/credits.md).
