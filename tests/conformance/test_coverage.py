"""Positive and negative inputs for branch coverage validation (QLT-007/QLT-009)."""

import importlib.util
from pathlib import Path
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


if __name__ == "__main__":
    unittest.main()
