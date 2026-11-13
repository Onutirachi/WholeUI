"""Tests for tools/build.py: amalgamation, determinism and diagnostic mapping (T012).

Run with: python -m unittest discover tests/tools
"""

from __future__ import annotations

import importlib.util
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent


def _load_build():
    # Loaded by path: a plain "import build" can pick up PyPA's "build" package instead.
    spec = importlib.util.spec_from_file_location("wholeui_build", REPO / "tools" / "build.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


build = _load_build()

FIXTURES = Path(__file__).resolve().parent / "fixtures"
AMALGAMATION = FIXTURES / "amalgamation"


def line_number_of(text: str, content: str) -> int:
    """1-based line number of the only line equal to `content`."""
    matches = [i + 1 for i, line in enumerate(text.split("\n")) if line == content]
    if len(matches) != 1:
        raise AssertionError(f"expected one line {content!r}, found {len(matches)}")
    return matches[0]


class FixtureCopy:
    """A temporary copy of the amalgamation fixture that a test may change."""

    def __enter__(self) -> Path:
        self._dir = tempfile.TemporaryDirectory()
        root = Path(self._dir.name) / "repo"
        shutil.copytree(AMALGAMATION, root)
        return root

    def __exit__(self, *exc: object) -> None:
        self._dir.cleanup()


class GenerateTest(unittest.TestCase):
    def setUp(self) -> None:
        self.text = build.generate(AMALGAMATION)
        self.lines = self.text.split("\n")

    def test_project_include_inlined_once(self) -> None:
        self.assertEqual(self.lines.count("int A();"), 1)
        self.assertEqual(self.lines.count("int A() { return B(); }"), 1)
        self.assertEqual(self.lines.count("inline int B() { return 1; }"), 1)
        self.assertNotIn('#include "core/a.h"', self.text)
        self.assertNotIn('#include "a.h"', self.text)
        self.assertNotIn('#include "b.h"', self.text)

    def test_repeated_include_is_dropped(self) -> None:
        # main.cpp includes core/a.h twice and a.cpp includes it again: one copy only.
        self.assertEqual(self.lines.count("#ifndef FIXTURE_A_H"), 1)
        self.assertEqual(self.lines.count("// ---- src/core/a.h ----"), 1)

    def test_system_includes_kept(self) -> None:
        self.assertIn("#include <windows.h>", self.lines)
        self.assertIn("#include <vector>", self.lines)

    def test_marker_before_each_inlined_file(self) -> None:
        for path, first_line in [
            ("src/main.cpp", "// Fixture root."),
            ("src/core/a.h", "#ifndef FIXTURE_A_H"),
            ("src/core/a.cpp", None),  # a.cpp starts with an include that is dropped
            ("src/core/b.h", "inline int B() { return 1; }"),
        ]:
            marker = f"// ---- {path} ----"
            self.assertEqual(self.lines.count(marker), 1, marker)
            if first_line is not None:
                index = self.lines.index(marker)
                self.assertEqual(self.lines[index + 1], first_line, marker)

    def test_files_in_include_order(self) -> None:
        order = [self.lines.index(f"// ---- src/{p} ----")
                 for p in ["main.cpp", "core/a.h", "core/a.cpp", "core/b.h"]]
        self.assertEqual(order, sorted(order))

    def test_generated_notice_first(self) -> None:
        self.assertTrue(self.lines[0].startswith("// "))
        self.assertIn("tools/build.py", self.lines[0])
        self.assertIn("do not edit", self.lines[0].lower())

    def test_windhawk_blocks_before_code(self) -> None:
        markers = [
            "// ==WindhawkMod==",
            "// @id              fixture",
            "// ==/WindhawkMod==",
            "// ==WindhawkModReadme==",
            "# Fixture",
            "// ==/WindhawkModReadme==",
            "// ==WindhawkModSettings==",
            "- enabled: true",
            "// ==/WindhawkModSettings==",
            "// ---- src/main.cpp ----",
        ]
        positions = [line_number_of(self.text, m) for m in markers]
        self.assertEqual(positions, sorted(positions))

    def test_readme_and_settings_inside_comments(self) -> None:
        readme = line_number_of(self.text, "// ==WindhawkModReadme==")
        self.assertEqual(self.lines[readme], "/*")
        self.assertEqual(self.lines[line_number_of(self.text, "// ==/WindhawkModReadme==") - 2],
                         "*/")
        settings = line_number_of(self.text, "// ==WindhawkModSettings==")
        self.assertEqual(self.lines[settings], "/*")
        self.assertEqual(
            self.lines[line_number_of(self.text, "// ==/WindhawkModSettings==") - 2], "*/")

    def test_lf_endings_and_single_final_newline(self) -> None:
        self.assertNotIn("\r", self.text)
        self.assertTrue(self.text.endswith("\n"))
        self.assertFalse(self.text.endswith("\n\n"))

    def test_two_runs_identical(self) -> None:
        self.assertEqual(build.encode(build.generate(AMALGAMATION)), build.encode(self.text))

    def test_metadata(self) -> None:
        metadata = build.read_metadata(AMALGAMATION)
        self.assertEqual(metadata["id"], "fixture")
        self.assertEqual(metadata["version"], "1.2.3")
        self.assertEqual(metadata["compilerOptions"], "-lole32 -lgdi32")


class EncodingTest(unittest.TestCase):
    def test_utf8_without_bom(self) -> None:
        data = build.encode(build.generate(AMALGAMATION))
        self.assertFalse(data.startswith(b"\xef\xbb\xbf"))
        self.assertIn("Português".encode("utf-8"), data)

    def test_crlf_and_bom_sources_normalized(self) -> None:
        with FixtureCopy() as root:
            a_cpp = root / "src" / "core" / "a.cpp"
            a_cpp.write_bytes(b"\xef\xbb\xbf" + a_cpp.read_bytes().replace(b"\n", b"\r\n"))
            readme = root / "src" / "mod" / "readme.md"
            readme.write_bytes(readme.read_bytes().replace(b"\n", b"\r\n"))
            self.assertEqual(build.encode(build.generate(root)),
                             build.encode(build.generate(AMALGAMATION)))

    def test_write_output(self) -> None:
        with FixtureCopy() as root:
            output = build.write_output(root)
            self.assertEqual(output, root / "build" / "wholeui.wh.cpp")
            self.assertEqual(output.read_bytes(), build.encode(build.generate(root)))


class ErrorTest(unittest.TestCase):
    def assertBuildError(self, root: Path, *fragments: str) -> None:
        with self.assertRaises(build.BuildError) as caught:
            build.generate(root)
        for fragment in fragments:
            self.assertIn(fragment, str(caught.exception))

    def test_missing_include_names_file_and_line(self) -> None:
        with FixtureCopy() as root:
            a_cpp = root / "src" / "core" / "a.cpp"
            a_cpp.write_text('#include "a.h"\n\n#include "nope.h"\n', encoding="utf-8")
            self.assertBuildError(root, "src/core/a.cpp:3", '"nope.h"')

    def test_comment_end_in_readme_fails(self) -> None:
        with FixtureCopy() as root:
            (root / "src" / "mod" / "readme.md").write_text("a */ b\n", encoding="utf-8")
            self.assertBuildError(root, "src/mod/readme.md", "*/")

    def test_comment_end_in_settings_fails(self) -> None:
        with FixtureCopy() as root:
            (root / "src" / "mod" / "settings.yaml").write_text("- a: '*/'\n", encoding="utf-8")
            self.assertBuildError(root, "src/mod/settings.yaml", "*/")

    def test_header_without_markers_fails(self) -> None:
        with FixtureCopy() as root:
            (root / "src" / "mod" / "header.txt").write_text("// @id x\n", encoding="utf-8")
            self.assertBuildError(root, "src/mod/header.txt", "==WindhawkMod==")

    def test_header_without_version_fails(self) -> None:
        with FixtureCopy() as root:
            (root / "src" / "mod" / "header.txt").write_text(
                "// ==WindhawkMod==\n// @id x\n// ==/WindhawkMod==\n", encoding="utf-8")
            self.assertBuildError(root, "src/mod/header.txt", "@version")

    def test_marker_lookalike_in_source_fails(self) -> None:
        with FixtureCopy() as root:
            (root / "src" / "core" / "b.h").write_text(
                "// ---- src/fake.h ----\n", encoding="utf-8")
            self.assertBuildError(root, "src/core/b.h:1", "marker")


class DiagnosticMappingTest(unittest.TestCase):
    def setUp(self) -> None:
        self.text = build.generate(AMALGAMATION)

    def test_map_lines(self) -> None:
        cases = [
            ("// Fixture root.", ("src/main.cpp", 1)),
            ("#include <windows.h>", ("src/main.cpp", 2)),
            ("#ifndef FIXTURE_A_H", ("src/core/a.h", 1)),
            ("int A();", ("src/core/a.h", 4)),
            ("inline int B() { return 1; }", ("src/core/b.h", 1)),
            ("int A() { return B(); }", ("src/core/a.cpp", 4)),  # after the inlined b.h
            ("int g_main = A();", ("src/main.cpp", 7)),  # after a dropped repeat include
        ]
        for content, expected in cases:
            line = line_number_of(self.text, content)
            self.assertEqual(build.map_line(self.text, line), expected, content)

    def test_lines_before_code_not_mapped(self) -> None:
        self.assertIsNone(build.map_line(self.text, 1))
        self.assertIsNone(build.map_line(self.text, line_number_of(self.text, "# Fixture")))

    def test_remap_compiler_output(self) -> None:
        line = line_number_of(self.text, "int A() { return B(); }")
        output = (
            f"In file included from build/wholeui.wh.cpp:{line}:\n"
            f"build/wholeui.wh.cpp:{line}:5: error: expected ';'\n"
            f"D:\\work\\repo\\build\\wholeui.wh.cpp:{line}:12: warning: unused\n"
            f"D:/work/repo/build/wholeui.wh.cpp:1:1: note: in the header\n"
            "other.h:3:1: note: unrelated\n"
        )
        self.assertEqual(
            build.remap_diagnostics(output, self.text),
            "In file included from src/core/a.cpp:4:\n"
            "src/core/a.cpp:4:5: error: expected ';'\n"
            "src/core/a.cpp:4:12: warning: unused\n"
            "D:/work/repo/build/wholeui.wh.cpp:1:1: note: in the header\n"
            "other.h:3:1: note: unrelated\n",
        )


COMPAT = FIXTURES / "compat"


def rule_tuple(rule) -> tuple:
    return (rule.origin, rule.process, rule.module, rule.window_class, rule.template,
            rule.level, rule.locked)


class CompatibilityDocTest(unittest.TestCase):
    """Parser of docs/compatibility.md (contracts/compatibility-doc.md, T023)."""

    def setUp(self) -> None:
        self.rules = build.parse_compatibility(COMPAT / "valid.md")

    def test_rows_in_document_order(self) -> None:
        self.assertEqual(
            [rule.origin for rule in self.rules],
            ["File Explorer", "System Properties", "Run dialog", 'Say "hi" \\ back',
             "Taskbar", "Desktop"],
        )

    def test_windows_rows(self) -> None:
        self.assertEqual(rule_tuple(self.rules[0]),
                         ("File Explorer", "explorer.exe", None, "CabinetWClass", None,
                          "native", False))
        self.assertEqual(rule_tuple(self.rules[2]),
                         ("Run dialog", "explorer.exe", "shell32.dll", "#32770", "1003",
                          "visual", False))
        self.assertEqual(self.rules[3].level, "full")

    def test_em_dash_and_hyphen_mean_not_named(self) -> None:
        explorer, system = self.rules[0], self.rules[1]
        self.assertIsNone(explorer.module)  # —
        self.assertIsNone(system.process)   # -
        self.assertIsNone(system.window_class)
        self.assertIsNone(system.template)

    def test_backticks_stripped(self) -> None:
        self.assertEqual(self.rules[1].module, "sysdm.cpl")
        self.assertEqual(self.rules[2].process, "explorer.exe")
        self.assertEqual(self.rules[2].window_class, "#32770")
        self.assertEqual(self.rules[5].window_class, "Progman")

    def test_to_do_ships_as_native(self) -> None:
        self.assertEqual(self.rules[0].level, "native")
        self.assertFalse(self.rules[0].locked)

    def test_shell_rows_are_locked_native(self) -> None:
        self.assertEqual(rule_tuple(self.rules[4]),
                         ("Taskbar", "explorer.exe", None, "Shell_TrayWnd", None, "native", True))
        self.assertEqual(rule_tuple(self.rules[5]),
                         ("Desktop", "explorer.exe", None, "Progman", None, "native", True))

    def test_other_tables_ignored(self) -> None:
        self.assertNotIn("is", [rule.origin for rule in self.rules])


class CompatibilityDocErrorTest(unittest.TestCase):
    def assertDocError(self, name: str, *fragments: str) -> None:
        with self.assertRaises(build.BuildError) as caught:
            build.parse_compatibility(COMPAT / name)
        for fragment in fragments:
            self.assertIn(fragment, str(caught.exception))

    def test_row_without_identity_field(self) -> None:
        self.assertDocError("no_identity.md", "no_identity.md:6:", "no identity field")

    def test_shell_row_without_identity_field(self) -> None:
        self.assertDocError("shell_no_identity.md", "shell_no_identity.md:11:",
                            "no identity field")

    def test_unknown_level(self) -> None:
        self.assertDocError("unknown_level.md", "unknown_level.md:5:", "unknown level", "dark")

    def test_wrong_column_count(self) -> None:
        self.assertDocError("column_count.md", "column_count.md:12:", "columns")

    def test_wrong_header(self) -> None:
        self.assertDocError("wrong_header.md", "wrong_header.md:3:", "header")

    def test_missing_table(self) -> None:
        self.assertDocError("missing_shell.md", "## Shell surfaces (always native)")


class BuiltInRulesIncludeTest(unittest.TestCase):
    """build/gen/builtin_rules.inc generated from the parsed rules."""

    def setUp(self) -> None:
        self.text = build.builtin_rules_text(COMPAT / "valid.md")

    def test_identical_across_runs(self) -> None:
        again = build.builtin_rules_text(COMPAT / "valid.md")
        self.assertEqual(build.encode(self.text), build.encode(again))

    def test_one_entry_per_row_in_document_order(self) -> None:
        positions = [self.text.find(f'L"{origin}"')
                     for origin in ("File Explorer", "System Properties", "Run dialog",
                                    "Taskbar", "Desktop")]
        self.assertNotIn(-1, positions)
        self.assertEqual(positions, sorted(positions))

    def test_strings_escaped_for_cpp(self) -> None:
        self.assertIn('L"Say \\"hi\\" \\\\ back"', self.text)

    def test_lf_endings_and_single_final_newline(self) -> None:
        self.assertNotIn("\r", self.text)
        self.assertTrue(self.text.endswith("\n"))
        self.assertFalse(self.text.endswith("\n\n"))

    def test_write_to_build_gen(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "docs").mkdir()
            shutil.copy(COMPAT / "valid.md", root / "docs" / "compatibility.md")
            path = build.write_builtin_rules(root)
            self.assertEqual(path, root / "build" / "gen" / "builtin_rules.inc")
            self.assertEqual(path.read_bytes(), build.encode(self.text))


class RealSourcesTest(unittest.TestCase):
    """The repository's own sources build (catches a broken include in src/)."""

    def test_repository_builds(self) -> None:
        text = build.generate(REPO)
        self.assertIn("// @id              wholeui", text.split("\n"))
        self.assertIn("// ---- src/main.cpp ----", text.split("\n"))

    def test_repository_compatibility_doc_parses(self) -> None:
        rules = build.parse_compatibility(REPO / "docs" / "compatibility.md")
        by_origin = {rule.origin: rule for rule in rules}
        self.assertEqual(rule_tuple(by_origin["File Explorer"]),
                         ("File Explorer", "explorer.exe", None, "CabinetWClass", None,
                          "native", False))
        self.assertEqual(by_origin["System Properties"].module, "sysdm.cpl")
        shell = sorted(rule.window_class for rule in rules if rule.locked)
        self.assertEqual(shell, sorted([
            "Shell_TrayWnd", "Shell_SecondaryTrayWnd", "NotifyIconOverflowWindow",
            "TopLevelWindowForOverflowXamlIsland", "XamlExplorerHostIslandWindow", "Progman",
            "WorkerW", "Shell_InputSwitchTopLevelWindow"]))
        self.assertTrue(all(rule.process == "explorer.exe" for rule in rules if rule.locked))


if __name__ == "__main__":
    unittest.main()
