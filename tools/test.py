"""Run WholeUI's tests (FR-005): C++ unit tests, then the build script's Python tests.

The C++ tests are tests/unit/*.cpp plus every PURE source under src/ (a .cpp whose first line
starts with "// PURE:"), compiled into build/tests/unit_tests.exe with the same toolchain as
`build.py --compile` and Windhawk's flags minus everything specific to a mod DLL (no
windhawk_api.h, no WH_MOD defines, no windhawk.lib). Python standard library only.

Usage: python tools/test.py
Exits non-zero if either suite fails; the failing tests are named in the output.
"""

from __future__ import annotations

import importlib.util
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
EXE = Path("build") / "tests" / "unit_tests.exe"


def _load_build():
    # Loaded by path: a plain "import build" can pick up PyPA's "build" package instead.
    spec = importlib.util.spec_from_file_location("wholeui_build", REPO / "tools" / "build.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


build = _load_build()

# Flags of a mod DLL that a test executable does not take ("-include x", "-o x", "-x c++"
# take the next item with them).
DROP = {"-shared", "-s", "-Wl,--export-all-symbols", "{windhawk_lib}", "{source}"}
DROP_WITH_NEXT = {"-include", "-o", "-x"}


def test_flags(config: dict) -> list[str]:
    flags: list[str] = []
    skip_next = False
    for flag in config["compile_flags"]:
        if skip_next:
            skip_next = False
        elif flag in DROP_WITH_NEXT:
            skip_next = True
        elif flag not in DROP and not flag.startswith("-DWH_"):
            flags.append(flag)
    return flags + config["warning_flags"] + ["-static"]


def pure_sources() -> list[Path]:
    sources = []
    for path in sorted((REPO / "src").rglob("*.cpp")):
        with path.open(encoding="utf-8") as stream:
            if stream.readline().startswith("// PURE:"):
                sources.append(path)
    return sources


def run_unit_tests() -> bool:
    config = build.load_config()
    toolchain = build.find_toolchain(config)
    build.write_builtin_rules(REPO)  # included by src/compat/rules.cpp
    sources = sorted((REPO / "tests" / "unit").glob("*.cpp")) + pure_sources()
    (REPO / EXE).parent.mkdir(parents=True, exist_ok=True)
    command = [str(toolchain.clang), *test_flags(config),
               *(build.relative(REPO, s) for s in sources), "-o", EXE.as_posix()]
    print(f"compiling {len(sources)} files with {toolchain.description}", flush=True)
    if subprocess.run(command, cwd=REPO).returncode != 0:
        print("unit tests: compile failed", file=sys.stderr)
        return False
    if subprocess.run([str(REPO / EXE)], cwd=REPO).returncode != 0:
        print("unit tests: failed (see the failing tests above)", file=sys.stderr)
        return False
    return True


def run_tool_tests() -> bool:
    command = [sys.executable, "-m", "unittest", "discover", "-s", "tests/tools"]
    if subprocess.run(command, cwd=REPO).returncode != 0:
        print("build script tests: failed (see the failing tests above)", file=sys.stderr)
        return False
    return True


def main() -> None:
    try:
        unit_ok = run_unit_tests()
    except build.BuildError as error:
        print(f"test: {error}", file=sys.stderr)
        unit_ok = False
    tools_ok = run_tool_tests()
    sys.exit(0 if unit_ok and tools_ok else 1)


if __name__ == "__main__":
    main()
