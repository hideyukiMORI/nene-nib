"""Run the conformance suite, failing when discovery finds no tests (QLT-007)."""

from pathlib import Path
import unittest

suite = unittest.defaultTestLoader.discover(str(Path(__file__).resolve().parents[1] / "tests/conformance"))
if suite.countTestCases() == 0:
    raise SystemExit("QLT-007: no conformance tests were discovered")
result = unittest.TextTestRunner(verbosity=2).run(suite)
raise SystemExit(0 if result.wasSuccessful() else 1)
