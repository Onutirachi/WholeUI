"""Build the WholeUI mod file from the modular sources (research R4).

Writes build/wholeui.wh.cpp: a "generated, do not edit" notice, the Windhawk metadata,
readme and settings blocks from src/mod/, then the amalgamation of src/main.cpp. Each
`#include "..."` of a project file is replaced by that file the first time it is seen and by
a one-line comment after that; `#include <...>` lines are kept. Python standard library only.

The built-in compatibility rules come from the tables in docs/compatibility.md: they are
written to build/gen/builtin_rules.inc before the amalgamation, and src/compat/rules.cpp
includes that file (research R9).

Markers in the generated file let compiler diagnostics be mapped back to the sources:

    // ---- src/core/log.cpp ----      the next line is line 1 of src/core/log.cpp
    // ---- src/core/log.cpp:12 ----   the next line is line 12 (after an inlined include)

Usage:
    python tools/build.py              write build/wholeui.wh.cpp
    python tools/build.py --check      also build twice and compare the bytes (FR-003)
    python tools/build.py --compile    also compile it with Windhawk's flags (FR-004)
    python tools/build.py --install    also install it into the local Windhawk (FR-006)

The compiler is Windhawk's own (`$WINDHAWK_ROOT\\Compiler`, default from
tools/toolchain.json) when present, else the pinned toolchain from
tools/setup_toolchain.py with the headers in tools/windhawk-sdk/.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CONFIG = REPO / "tools" / "toolchain.json"
OUTPUT = Path("build") / "wholeui.wh.cpp"
DLL = Path("build") / "wholeui.dll"
COMPATIBILITY_DOC = Path("docs") / "compatibility.md"
BUILTIN_RULES = Path("build") / "gen" / "builtin_rules.inc"

NOTICE = "// Generated from src/ by tools/build.py. Do not edit: change the sources and rebuild."
MOD_BEGIN = "// ==WindhawkMod=="
MOD_END = "// ==/WindhawkMod=="

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')
MARKER_RE = re.compile(r"^// ---- (\S+?)(?::(\d+))? ----$")
METADATA_RE = re.compile(r"^//\s+@(\S+)\s+(.*?)\s*$")


class BuildError(Exception):
    """A problem in the sources or the environment; the message names the file."""


# ---------------------------------------------------------------------------------------------
# Generation


def relative(repo: Path, path: Path) -> str:
    try:
        return path.resolve().relative_to(repo.resolve()).as_posix()
    except ValueError:
        raise BuildError(f"{path}: outside the repository") from None


def read_lines(repo: Path, path: Path) -> list[str]:
    """Lines of a UTF-8 text file without BOM or line endings."""
    try:
        data = path.read_bytes()
    except OSError as error:
        raise BuildError(f"{relative(repo, path)}: {error.strerror}") from None
    if data.startswith(b"\xef\xbb\xbf"):
        data = data[3:]
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as error:
        raise BuildError(f"{relative(repo, path)}: not UTF-8 ({error.reason})") from None
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    return lines


def _header_lines(repo: Path) -> list[str]:
    path = repo / "src" / "mod" / "header.txt"
    lines = read_lines(repo, path)
    if not lines or lines[0] != MOD_BEGIN or lines[-1] != MOD_END:
        raise BuildError(
            f"{relative(repo, path)}: must start with '{MOD_BEGIN}' and end with '{MOD_END}'")
    return lines


def read_metadata(repo: Path) -> dict[str, str]:
    """The `// @key value` lines of src/mod/header.txt."""
    metadata: dict[str, str] = {}
    for line in _header_lines(repo):
        match = METADATA_RE.match(line)
        if match:
            metadata[match.group(1)] = match.group(2)
    for key in ("id", "version"):
        if key not in metadata:
            raise BuildError(f"src/mod/header.txt: missing @{key}")
    return metadata


def _comment_block(repo: Path, name: str, path: Path) -> list[str]:
    lines = read_lines(repo, path)
    while lines and not lines[-1].strip():
        lines.pop()
    for number, line in enumerate(lines, 1):
        if "*/" in line:
            raise BuildError(f"{relative(repo, path)}:{number}: '*/' would end the comment block")
    return [f"// =={name}==", "/*", *lines, "*/", f"// ==/{name}=="]


def _inline(repo: Path, path: Path, seen: set[str], out: list[str]) -> None:
    name = relative(repo, path)
    lines = read_lines(repo, path)
    out.append(f"// ---- {name} ----")
    for number, line in enumerate(lines, 1):
        if MARKER_RE.match(line):
            raise BuildError(f"{name}:{number}: line looks like a build marker; change it")
        match = INCLUDE_RE.match(line)
        if not match:
            out.append(line)
            continue
        target = (path.parent / match.group(1)).resolve()
        if not target.is_file():
            raise BuildError(f'{name}:{number}: cannot find include "{match.group(1)}"')
        key = os.path.normcase(str(target))
        if key in seen:
            out.append(f"// (already inlined: {relative(repo, target)})")
            continue
        seen.add(key)
        _inline(repo, target, seen, out)
        if number < len(lines):
            out.append(f"// ---- {name}:{number + 1} ----")


def generate(repo: Path = REPO) -> str:
    """The text of the mod file built from `repo`/src.

    Writes build/gen/builtin_rules.inc first when `repo` has docs/compatibility.md, so the
    sources can include it.
    """
    read_metadata(repo)  # validates the header
    if (repo / COMPATIBILITY_DOC).is_file():
        write_builtin_rules(repo)
    mod = repo / "src" / "mod"
    out = [NOTICE, ""]
    out += _header_lines(repo)
    out.append("")
    out += _comment_block(repo, "WindhawkModReadme", mod / "readme.md")
    out.append("")
    out += _comment_block(repo, "WindhawkModSettings", mod / "settings.yaml")
    out.append("")
    main = (repo / "src" / "main.cpp").resolve()
    _inline(repo, main, {os.path.normcase(str(main))}, out)
    return "\n".join(out) + "\n"


def encode(text: str) -> bytes:
    return text.encode("utf-8")


# ---------------------------------------------------------------------------------------------
# Built-in compatibility rules (research R9, contracts/compatibility-doc.md)

WINDOWS_HEADING = "## Windows"
SHELL_HEADING = "## Shell surfaces (always native)"
WINDOWS_COLUMNS = ["Window", "Process", "Module", "Class", "Template", "Level", "Windows build",
                   "Validated"]
SHELL_COLUMNS = ["Surface", "Process", "Class"]
LEVELS = {"native": "native", "visual": "visual", "full": "full", "to do": "native"}
NOT_NAMED = {"—", "-", ""}
CPP_LEVELS = {"native": "Level::Native", "visual": "Level::Visual", "full": "Level::Full"}


@dataclass
class CompatRule:
    """One row of a compatibility table; None means the field is not named."""
    origin: str
    process: str | None
    module: str | None
    window_class: str | None
    template: str | None
    level: str
    locked: bool
    line: int


def _display(path: Path) -> str:
    try:
        return path.resolve().relative_to(REPO).as_posix()
    except ValueError:
        return str(path)


def _cells(line: str) -> list[str] | None:
    """The cells of a table row, or None when the line is not a table row."""
    text = line.strip()
    if not text.startswith("|"):
        return None
    if not text.endswith("|") or len(text) < 2:
        return []
    return [cell.strip() for cell in text[1:-1].split("|")]


def _value(cell: str) -> str | None:
    if len(cell) >= 2 and cell.startswith("`") and cell.endswith("`"):
        cell = cell[1:-1].strip()
    return None if cell in NOT_NAMED else cell


def _table(lines: list[str], heading: str, columns: list[str],
           where: str) -> list[tuple[int, list[str]]]:
    """The data rows (line number, cells) of the table under `heading`."""
    try:
        start = lines.index(heading)
    except ValueError:
        raise BuildError(f"{where}: missing table under '{heading}'") from None
    index = start + 1
    while index < len(lines) and _cells(lines[index]) is None:
        if lines[index].startswith("#"):
            break
        index += 1
    if index >= len(lines) or _cells(lines[index]) is None:
        raise BuildError(f"{where}:{start + 1}: no table under '{heading}'")
    expected = "| " + " | ".join(columns) + " |"
    if _cells(lines[index]) != columns:
        raise BuildError(f"{where}:{index + 1}: table header must be {expected}")
    separator = _cells(lines[index + 1]) if index + 1 < len(lines) else None
    if separator is None or len(separator) != len(columns) or any(
            not re.fullmatch(r":?-+:?", cell) for cell in separator):
        raise BuildError(f"{where}:{index + 2}: separator row must follow the header {expected}")
    rows = []
    index += 2
    while index < len(lines):
        cells = _cells(lines[index])
        if cells is None:
            break
        if len(cells) != len(columns):
            raise BuildError(
                f"{where}:{index + 1}: expected {len(columns)} columns, found {len(cells)}")
        rows.append((index + 1, cells))
        index += 1
    return rows


def parse_compatibility(path: Path) -> list[CompatRule]:
    """The rules of docs/compatibility.md in document order; BuildError names the line."""
    where = _display(path)
    try:
        data = path.read_bytes()
    except OSError as error:
        raise BuildError(f"{where}: {error.strerror}") from None
    if data.startswith(b"\xef\xbb\xbf"):
        data = data[3:]
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as error:
        raise BuildError(f"{where}: not UTF-8 ({error.reason})") from None
    lines = [line.rstrip() for line in text.replace("\r\n", "\n").replace("\r", "\n").split("\n")]

    rules: list[CompatRule] = []
    for number, cells in _table(lines, WINDOWS_HEADING, WINDOWS_COLUMNS, where):
        origin, process, module, window_class, template, level = cells[:6]
        level_key = level.strip("`").strip().lower()
        if level_key not in LEVELS:
            raise BuildError(f'{where}:{number}: unknown level "{level}"')
        rules.append(CompatRule(origin, _value(process), _value(module), _value(window_class),
                                _value(template), LEVELS[level_key], False, number))
    for number, cells in _table(lines, SHELL_HEADING, SHELL_COLUMNS, where):
        origin, process, window_class = cells
        rules.append(CompatRule(origin, _value(process), None, _value(window_class), None,
                                "native", True, number))
    for rule in rules:
        if not rule.origin:
            raise BuildError(f"{where}:{rule.line}: no window name")
        if not (rule.process or rule.module or rule.window_class or rule.template):
            raise BuildError(f"{where}:{rule.line}: no identity field (name a process, module, "
                             "class or template)")
    rules.sort(key=lambda rule: rule.line)
    return rules


def _cpp_string(value: str | None) -> str:
    if value is None:
        return "nullptr"
    out = []
    for char in value:
        code = ord(char)
        if char in '"\\':
            out.append("\\" + char)
        elif 0x20 <= code < 0x7F:
            out.append(char)
        elif code <= 0xFFFF:
            out.append(f"\\u{code:04X}")
        else:
            out.append(f"\\U{code:08X}")
    return 'L"' + "".join(out) + '"'


def builtin_rules_text(path: Path) -> str:
    """The C++ initializers for the rules in `path`: one per row, in document order."""
    lines = ["// Generated by tools/build.py from the compatibility list. Do not edit.",
             "// { origin, process, module, class, template, level, locked }"]
    for rule in parse_compatibility(path):
        fields = [rule.origin, rule.process, rule.module, rule.window_class, rule.template]
        lines.append("{" + ", ".join(_cpp_string(field) for field in fields) +
                     f", {CPP_LEVELS[rule.level]}, {'true' if rule.locked else 'false'}}},")
    return "\n".join(lines) + "\n"


def write_builtin_rules(repo: Path = REPO) -> Path:
    """Writes build/gen/builtin_rules.inc from docs/compatibility.md (unchanged bytes are kept)."""
    output = repo / BUILTIN_RULES
    data = encode(builtin_rules_text(repo / COMPATIBILITY_DOC))
    if not output.is_file() or output.read_bytes() != data:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(data)
    return output


def write_output(repo: Path = REPO) -> Path:
    output = repo / OUTPUT
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(encode(generate(repo)))
    return output


def check(repo: Path = REPO) -> None:
    first, second = encode(generate(repo)), encode(generate(repo))
    if first != second:
        offset = next((i for i, (a, b) in enumerate(zip(first, second)) if a != b),
                      min(len(first), len(second)))
        raise BuildError(f"two builds differ at byte {offset}")
    print("identical")


# ---------------------------------------------------------------------------------------------
# Diagnostic mapping


def line_map(text: str) -> list[tuple[str, int] | None]:
    """For each generated line (index 0 = line 1), its source file and line, if any."""
    result: list[tuple[str, int] | None] = []
    current: tuple[str, int] | None = None
    for content in text.split("\n"):
        match = MARKER_RE.match(content)
        if match:
            result.append(None)
            current = (match.group(1), int(match.group(2) or 1))
            continue
        result.append(current)
        if current:
            current = (current[0], current[1] + 1)
    return result


def map_line(text: str, line: int) -> tuple[str, int] | None:
    table = line_map(text)
    return table[line - 1] if 1 <= line <= len(table) else None


def remap_diagnostics(output: str, text: str, generated_name: str = OUTPUT.name) -> str:
    """Rewrite `<path>/wholeui.wh.cpp:<line>` in compiler output to `src/<path>:<line>`."""
    table = line_map(text)
    pattern = re.compile(
        r"(?:[A-Za-z]:)?(?:[^\s:\"'<>|]*[\\/])?" + re.escape(generated_name) + r":(\d+)")

    def replace(match: re.Match[str]) -> str:
        line = int(match.group(1))
        source = table[line - 1] if 1 <= line <= len(table) else None
        return f"{source[0]}:{source[1]}" if source else match.group(0)

    return pattern.sub(replace, output)


# ---------------------------------------------------------------------------------------------
# Toolchain, compile and install


def load_config() -> dict:
    return json.loads(CONFIG.read_text(encoding="utf-8"))


def windhawk_root(config: dict) -> Path:
    return Path(os.environ.get("WINDHAWK_ROOT") or config["windhawk"]["default_root"])


@dataclass
class Toolchain:
    description: str
    clang: Path
    # Folder holding windhawk_api.h when the compiler does not have it built in.
    sdk_include: Path | None


def find_toolchain(config: dict) -> Toolchain:
    """Windhawk's compiler if installed, else the pinned one from setup_toolchain.py."""
    root = windhawk_root(config)
    clang = root / "Compiler" / "bin" / "clang++.exe"
    if clang.is_file():
        return Toolchain(f"Windhawk's compiler in {root}", clang, None)
    pinned = REPO / "build" / "toolchain" / config["llvm_mingw"]["archive_root"]
    clang = pinned / "bin" / "clang++.exe"
    if clang.is_file() and (pinned / ".wholeui-complete").is_file():
        return Toolchain(f"pinned llvm-mingw in {relative(REPO, pinned)}", clang,
                         REPO / config["windhawk"]["sdk_include"])
    raise BuildError(
        f"no compiler: none in {root} (set WINDHAWK_ROOT) and none pinned "
        "(run python tools/setup_toolchain.py)")


def compile_command(config: dict, toolchain: Toolchain, metadata: dict[str, str]) -> list[str]:
    values = {
        "{mod_id}": metadata["id"],
        "{mod_version}": metadata["version"],
        "{windhawk_version_hex}": config["windhawk"]["version_hex"],
        "{windhawk_lib}": config["windhawk"]["sdk_lib"],
        "{source}": OUTPUT.as_posix(),
        "{output}": DLL.as_posix(),
    }
    command = [str(toolchain.clang)]
    for flag in config["compile_flags"]:
        for placeholder, value in values.items():
            flag = flag.replace(placeholder, value)
        command.append(flag)
    command += config["warning_flags"]
    if toolchain.sdk_include:
        command += ["-I", str(toolchain.sdk_include)]
    # Windhawk appends the mod's own @compilerOptions last.
    command += shlex.split(metadata.get("compilerOptions", ""))
    return command


def compile_mod() -> None:
    config = load_config()
    toolchain = find_toolchain(config)
    print(f"compiling with {toolchain.description}", flush=True)
    command = compile_command(config, toolchain, read_metadata(REPO))
    start = time.perf_counter()
    result = subprocess.run(command, cwd=REPO, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            encoding="utf-8", errors="replace")
    text = (REPO / OUTPUT).read_text(encoding="utf-8")
    if result.stdout:
        print(remap_diagnostics(result.stdout, text), end="")
    if result.returncode != 0:
        raise BuildError(f"compile failed (exit code {result.returncode})")
    print(f"compiled {DLL.as_posix()} in {time.perf_counter() - start:.1f} s")


def install_mod() -> None:
    cli = windhawk_root(load_config()) / "windhawk-cli.exe"
    if not cli.is_file():
        raise BuildError(f"{cli} not found (set WINDHAWK_ROOT)")
    result = subprocess.run([str(cli), "mod", "install", "--file", str(REPO / OUTPUT)])
    if result.returncode != 0:
        raise BuildError(f"windhawk-cli failed (exit code {result.returncode})")


def main() -> None:
    parser = argparse.ArgumentParser(description="Build build/wholeui.wh.cpp from src/.")
    parser.add_argument("--check", action="store_true", help="build twice and compare bytes")
    parser.add_argument("--compile", action="store_true", help="compile with Windhawk's flags")
    parser.add_argument("--install", action="store_true", help="install into local Windhawk")
    args = parser.parse_args()
    try:
        start = time.perf_counter()
        output = write_output(REPO)
        print(f"wrote {relative(REPO, output)} in {(time.perf_counter() - start) * 1000:.0f} ms",
              flush=True)
        if args.check:
            check(REPO)
        if args.compile:
            compile_mod()
        if args.install:
            install_mod()
    except BuildError as error:
        sys.stdout.flush()
        print(f"build: {error}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
