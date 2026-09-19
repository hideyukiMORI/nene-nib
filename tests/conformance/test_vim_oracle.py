import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("vim_oracle", ROOT / "eng/vim-oracle.py")
vim_oracle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vim_oracle)


class VimOracleTests(unittest.TestCase):
    def fake_run(self, command, **kwargs):
        work = kwargs["cwd"]
        report = "text\ncursor=1,1\nregtype=\nreg=\n"
        if "-es" not in command:
            report += "topline=1\nscroll=5\n"
        (work / "out.txt").write_text(report, encoding="utf-8")
        self.command, self.kwargs = command, kwargs
        return subprocess.CompletedProcess(command, 0, b"", b"")

    def test_launch_mode_and_timeout_follow_viewport(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(vim_oracle.subprocess, "run", self.fake_run):
            work = Path(directory)
            vim_oracle.run_vim(work, "text", "h", [])
            self.assertIn("-es", self.command)
            self.assertNotIn("--not-a-term", self.command)
            self.assertEqual(vim_oracle.ORACLE_TIMEOUT_SECONDS, self.kwargs["timeout"])
            viewport = {"visible_lines": 10, "first_visible": 1, "line": 1, "column": 1}
            vim_oracle.run_vim(work, "text", "h", [], viewport)
            self.assertNotIn("-es", self.command)
            self.assertIn("--not-a-term", self.command)

    def test_viewport_rejects_non_integer_and_incomplete_input(self):
        fixture = {"name": "viewport", "viewport": {"visible_lines": True, "first_visible": 1,
                                                        "line": 1, "column": 1}}
        with self.assertRaises(ValueError):
            vim_oracle.viewport_of(fixture)
        with self.assertRaises(ValueError):
            vim_oracle.viewport_of({"name": "viewport", "viewport": {"visible_lines": 1}})

    def test_header_keeps_old_fixture_shape_as_nullopt(self):
        record = {"name": "old", "text": "text", "keys": "h", "expected": "text", "line": 1,
                  "column": 1, "register": "", "register_kind": ""}
        self.assertIn("std::nullopt", vim_oracle.header([record], "VIM", "digest"))


if __name__ == "__main__":
    unittest.main()
