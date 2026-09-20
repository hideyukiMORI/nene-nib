import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch


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

    @staticmethod
    def record(fixture, expected):
        return {**fixture, "expected": expected, "line": 1, "column": 1,
                "register": "", "register_kind": ""}

    def reuse_inputs(self):
        fixtures = [
            {"name": "old-kept", "text": "old", "keys": "h", "settings": []},
            {"name": "new-selected", "text": "new", "keys": "l", "settings": []},
        ]
        content = (json.dumps(fixtures) + "\n").encode("utf-8")
        records = [self.record(fixtures[0], "old expected"),
                   self.record(fixtures[1], "previous expected")]
        version = "VIM 9.1 test"
        old_header = vim_oracle.header(records, version, hashlib.sha256(content).hexdigest())
        source = Path(vim_oracle.__file__).read_text(encoding="utf-8")
        return fixtures, content, old_header, source, version

    def test_partial_regeneration_measures_only_selected_and_keeps_old_row(self):
        fixtures, content, old_header, source, version = self.reuse_inputs()
        measured = self.record(fixtures[1], "new expected")
        measure = Mock(return_value=measured)
        with tempfile.TemporaryDirectory() as directory, patch.object(vim_oracle, "measure", measure):
            rendered = vim_oracle.partial_header(
                fixtures, content, ["new-"], content, old_header, source, source,
                version, Path(directory))
        measure.assert_called_once_with(Path(directory), fixtures[1])
        old_row = vim_oracle.fixture_row(self.record(fixtures[0], "old expected"))
        self.assertIn(old_row, rendered)
        self.assertIn(vim_oracle.fixture_row(measured), rendered)

    def test_partial_regeneration_rejects_changed_non_selected_input(self):
        fixtures, content, old_header, source, version = self.reuse_inputs()
        changed = [dict(fixtures[0], text="changed"), fixtures[1]]
        changed_content = (json.dumps(changed) + "\n").encode("utf-8")
        with tempfile.TemporaryDirectory() as directory, self.assertRaisesRegex(
                ValueError, "non-selected fixture input changed"):
            vim_oracle.partial_header(
                changed, changed_content, ["new-"], content, old_header, source, source,
                version, Path(directory))

    def test_partial_regeneration_rejects_reuse_metadata_mismatch(self):
        fixtures, content, old_header, source, version = self.reuse_inputs()
        rows = [line for line in old_header.splitlines() if line.startswith("    {")]
        cases = {
            "SHA": old_header.replace(hashlib.sha256(content).hexdigest(), "0" * 64),
            "count": old_header.replace("/ 2 fixtures", "/ 3 fixtures"),
            "version": old_header.replace(version, "VIM 9.0 old"),
            "path": old_header.replace(str(vim_oracle.VIM), r"C:\Other\vim.exe"),
            "settings": old_header.replace("set nocompatible / set backspace=indent,eol,start",
                                           "set nocompatible"),
            "row order": old_header.replace("\n".join(rows), "\n".join(reversed(rows))),
        }
        for name, mismatched_header in cases.items():
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory, \
                    self.assertRaises(ValueError):
                vim_oracle.partial_header(
                    fixtures, content, ["new-"], content, mismatched_header, source, source,
                    version, Path(directory))

    def test_partial_regeneration_rejects_measurement_code_change(self):
        fixtures, content, old_header, source, version = self.reuse_inputs()
        changed_source = source.replace("ORACLE_TIMEOUT_SECONDS = 30",
                                        "ORACLE_TIMEOUT_SECONDS = 31", 1)
        with tempfile.TemporaryDirectory() as directory, self.assertRaisesRegex(
                ValueError, "measurement code changed"):
            vim_oracle.partial_header(
                fixtures, content, ["new-"], content, old_header, source, changed_source,
                version, Path(directory))

    def test_measurement_source_allows_comments_docstrings_and_reuse_imports_only(self):
        source = Path(vim_oracle.__file__).read_text(encoding="utf-8")
        comments_changed = source.replace(
            '"""The first line of `vim --version`, which names the release and its patch level."""',
            '"""Changed documentation does not change measurement."""', 1)
        comments_changed = comments_changed.replace("def vim_version() -> str:",
                                                    "# A changed comment.\ndef vim_version() -> str:", 1)
        self.assertTrue(vim_oracle.measurement_sources_match(source, comments_changed))
        self.assertFalse(vim_oracle.measurement_sources_match(
            source, source.replace("import subprocess", "import subprocess as process", 1)))

    def test_measurement_source_rejects_function_body_name_and_order_changes(self):
        source = ("import subprocess\nVALUE = 1\n\n"
                  "def first():\n    return VALUE\n\n"
                  "def second():\n    return first()\n\n"
                  "def literal(value):\n    return value\n")
        cases = [
            source.replace("return VALUE", "return VALUE + 1", 1),
            source.replace("def first", "def renamed", 1),
            source.replace("def first():\n    return VALUE\n\n"
                           "def second():\n    return first()",
                           "def second():\n    return first()\n\n"
                           "def first():\n    return VALUE", 1),
        ]
        for changed in cases:
            with self.subTest(changed=changed):
                self.assertFalse(vim_oracle.measurement_sources_match(source, changed))

    def test_only_rejects_empty_and_unknown_prefix(self):
        fixtures = [{"name": "known-case", "text": "x", "keys": "h", "settings": []},
                    {"name": "second-case", "text": "y", "keys": "l", "settings": []}]
        self.assertEqual({"known-case", "second-case"},
                         vim_oracle.selected_names(fixtures, ["known-", "second-"]))
        with self.assertRaisesRegex(ValueError, "must not be empty"):
            vim_oracle.selected_names(fixtures, [""])
        with self.assertRaisesRegex(ValueError, "matched no fixture"):
            vim_oracle.selected_names(fixtures, ["unknown"])


if __name__ == "__main__":
    unittest.main()
