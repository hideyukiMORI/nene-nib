"""Positive and negative inputs for branch coverage validation (QLT-007/QLT-009)."""

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import unittest

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / "eng"))
spec = importlib.util.spec_from_file_location("nib_coverage", root / "eng/coverage.py")
coverage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(coverage)


class CoverageTests(unittest.TestCase):
    def report(self, count=10, covered=9, name="src/core/TextBuffer.cpp"):
        return {"data": [{"files": [{"filename": str(root / name),
                "summary": {"branches": {"count": count, "covered": covered}}}]}]}

    def assess(self, report):
        return coverage.assess(report, root, {root / "src/core/TextBuffer.cpp"},
                               [root / "src/core"], 90)

    def test_boundary_passes(self):
        self.assertEqual(90, self.assess(self.report())["percent"])

    def test_below_boundary_fails(self):
        with self.assertRaisesRegex(ValueError, "QLT-009: branch coverage"):
            self.assess(self.report(100, 89))

    def test_missing_translation_unit_fails(self):
        with self.assertRaisesRegex(ValueError, "QLT-009: missing"):
            self.assess(self.report(name="src/core/Other.cpp"))

    def test_empty_branches_fail(self):
        with self.assertRaisesRegex(ValueError, "QLT-009: empty"):
            self.assess(self.report(0, 0))

    def test_invalid_counts_fail(self):
        with self.assertRaisesRegex(ValueError, "QLT-009: invalid"):
            self.assess(self.report(10, 11))

    def test_duplicate_file_fails(self):
        report = self.report()
        report["data"][0]["files"] *= 2
        with self.assertRaisesRegex(ValueError, "QLT-009: duplicate"):
            self.assess(report)

    def test_external_branches_do_not_inflate_score(self):
        report = self.report(10, 8)
        report["data"][0]["files"] += self.report(1000, 1000, "tests/unit/NibTests.cpp")["data"][0]["files"]
        with self.assertRaisesRegex(ValueError, "QLT-009: branch coverage"):
            self.assess(report)

    def test_scoped_headers_are_measured(self):
        report = self.report(10, 10)
        report["data"][0]["files"] += self.report(10, 0, "src/core/TextBuffer.hpp")["data"][0]["files"]
        with self.assertRaisesRegex(ValueError, "QLT-009: branch coverage"):
            self.assess(report)


# The shape CMake (Ninja + clang-cl) writes into build/compile_commands.json for src/core/AppIdentity.cpp,
# whose definition CMakeLists.txt gives that translation unit only (#1677). eng/coverage.py dropped it and
# QLT-009 stopped compiling from 2026-11-14 until Issue #1921; only eng/check.ps1 -Full ran the tool.
CANONICAL = (r'"C:\PROGRA~2\LLVM\bin\clang-cl.exe"  /nologo -TP -DNENENIB_APP_VERSION=\"1.1.0\" '
             r'-IC:\nib\src\core -imsvcC:\nib\third_party\md4c /DWIN32 /EHsc /Ob0 /Od /RTC1 '
             r'-clang:-std=c++23 -MT -Zi /W4 /WX /utf-8 -Wconversion -fsanitize=address -fsanitize=undefined '
             r'-fno-sanitize-recover=all /FoCMakeFiles\nenenib_core.dir\src\core\AppIdentity.cpp.obj '
             r'/FdCMakeFiles\nenenib_core.dir\nenenib_core.pdb -c -- C:\nib\src\core\AppIdentity.cpp')


class MeasurementCommandTests(unittest.TestCase):
    def test_split_keeps_an_escaped_quote_inside_a_definition(self):
        arguments = coverage.split_command(CANONICAL)
        self.assertEqual(r"C:\PROGRA~2\LLVM\bin\clang-cl.exe", arguments[0])
        self.assertIn('-DNENENIB_APP_VERSION="1.1.0"', arguments)
        self.assertEqual(["a b", "c\\\\d", 'e"', "f\\"], coverage.split_command(r'"a b" c\\d e\" f\ '))

    def test_measured_command_keeps_every_canonical_flag_of_the_translation_unit(self):
        obj = Path(r"C:\nib\out\coverage\obj\src__core__AppIdentity.cpp.obj")
        command = coverage.measured_command(coverage.split_command(CANONICAL), obj)
        for flag in ('-DNENENIB_APP_VERSION="1.1.0"', r"-IC:\nib\src\core", r"-imsvcC:\nib\third_party\md4c",
                     "-clang:-std=c++23", "-MT", "/W4", "/WX", "-Wconversion", *coverage.INSTRUMENTATION):
            self.assertIn(flag, command)
        self.assertFalse([a for a in command if "sanitize" in a or a in ("-Zi", "-c")])
        self.assertEqual([f"/Fo{obj}"], [a for a in command if a.startswith(("/Fo", "/Fd"))])
        self.assertEqual(["--", r"C:\nib\src\core\AppIdentity.cpp"], command[-2:])
        # subprocess quotes the list for CreateProcess; clang-cl must read the definition back unchanged.
        self.assertEqual(command, coverage.split_command(subprocess.list2cmdline(command)))

    def test_measured_command_refuses_a_command_without_one_source(self):
        with self.assertRaisesRegex(ValueError, "QLT-009: canonical compile command"):
            coverage.measured_command(["clang-cl", "/c", "a.cpp"], Path("a.obj"))
        with self.assertRaisesRegex(ValueError, "QLT-009: canonical compile command"):
            coverage.measured_command(["clang-cl", "--", "a.cpp", "b.cpp"], Path("a.obj"))

    def test_translation_units_are_the_test_target_and_the_libraries_it_links(self):
        def entry(file, output):
            return {"file": str(root / file), "output": output, "arguments": ["clang-cl", "--", str(root / file)]}
        commands = [entry("src/core/TextBuffer.cpp", r"CMakeFiles\nenenib_core.dir\src\core\TextBuffer.cpp.obj"),
                    entry("tests/unit/NibTests.cpp", r"CMakeFiles\nib_tests.dir\tests\unit\NibTests.cpp.obj"),
                    entry("tests/unit/TestSupport.cpp", r"CMakeFiles\nib_tests.dir\tests\unit\TestSupport.cpp.obj"),
                    entry("tests/unit/TestSupport.cpp", r"CMakeFiles\nib_vim_replay.dir\tests\unit\TestSupport.cpp.obj"),
                    entry("tests/unit/VimReplayMain.cpp", r"CMakeFiles\nib_vim_replay.dir\tests\unit\VimReplayMain.cpp.obj"),
                    entry("src/adapters/win32/Win32Worker.cpp", r"CMakeFiles\nenenib_win32.dir\Win32Worker.cpp.obj")]
        units = coverage.translation_units(commands, root, "nib_tests", [root / "src/core"])
        self.assertEqual({root / "src/core/TextBuffer.cpp", root / "tests/unit/NibTests.cpp",
                          root / "tests/unit/TestSupport.cpp"}, set(units))
        with self.assertRaisesRegex(ValueError, "two canonical compile commands"):
            coverage.translation_units(commands + commands[:1], root, "nib_tests", [root / "src/core"])

    def test_the_test_module_links_every_measured_module_and_md4c(self):
        modules = json.loads((root / "eng/architecture.json").read_text(encoding="utf-8"))["modules"]
        policy = json.loads((root / "eng/coverage-policy.json").read_text(encoding="utf-8"))
        linked = coverage.closure(modules[policy["testModule"]]["dependencies"], modules)
        self.assertLessEqual(set(policy["modules"]) | {"third_party_md4c"}, linked)


if __name__ == "__main__":
    unittest.main()
