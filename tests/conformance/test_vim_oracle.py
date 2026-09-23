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

    # 整形の正本（Issue #98）。形を決めるのはこの 1 つの関数で、CNF-011 は同じ関数を呼ぶ。
    def test_canonical_form_is_one_fixture_per_line_in_one_key_order(self):
        fixtures = [{"name": "plain", "text": "ab", "keys": "x", "settings": []},
                    {"name": "full", "text": "あ\tb", "keys": "y", "settings": ["set expandtab"],
                     "viewport": {"column": 1, "line": 2, "first_visible": 1, "visible_lines": 3}}]
        self.assertEqual(
            '[\n'
            '  {"name":"plain","text":"ab","keys":"x"},\n'
            '  {"name":"full","text":"あ\\tb","keys":"y","settings":["set expandtab"],'
            '"viewport":{"visible_lines":3,"first_visible":1,"line":2,"column":1}}\n'
            ']\n'.encode("utf-8"),
            vim_oracle.canonical_fixtures_json(fixtures))

    # マクロのレジスタ（ADR 0046 の決定 6）。欄は settings と viewport の間で、在るときだけ書く。
    def test_canonical_form_puts_the_register_between_settings_and_viewport(self):
        fixture = {"name": "macro", "text": "ab", "keys": "@a",
                   "viewport": {"visible_lines": 3, "first_visible": 1, "line": 1, "column": 1},
                   "register": {"keys": "x", "name": "a"}, "settings": ["set expandtab"]}
        self.assertEqual(
            '[\n  {"name":"macro","text":"ab","keys":"@a","settings":["set expandtab"],'
            '"register":{"name":"a","keys":"x"},'
            '"viewport":{"visible_lines":3,"first_visible":1,"line":1,"column":1}}\n]\n'
            .encode("utf-8"),
            vim_oracle.canonical_fixtures_json([fixture]))

    # 録画は :normal! の中で動かないので、q を NORMAL / VISUAL の命令として打つ fixture は黙って
    # 測らずに拒む（#190）。検索の入力行・挿入文字・文字引数・レジスタ名の q は文字なので通る。
    def test_canonical_form_rejects_a_recording_q(self):
        for keys in ["\"aqa", "viwqa", "xqb", "dq", "gq"]:
            with self.subTest(keys=keys), self.assertRaises(ValueError):
                vim_oracle.canonical_fixtures_json([{"name": "a", "text": "b", "keys": keys}])
        for fixture in [{"name": "a", "text": "b", "keys": "qaxq@a"},
                        {"name": "a", "text": "b", "keys": "@a",
                         "register": {"name": "a", "keys": "qbq"}},
                        {"name": "a", "text": "b", "keys": "@a",
                         "register": {"name": "A", "keys": "x"}},
                        {"name": "a", "text": "b", "keys": "@a",
                         "register": {"name": "a", "keys": ""}},
                        {"name": "a", "text": "b", "keys": "@a", "register": {"name": "a"}}]:
            with self.subTest(fixture=fixture), self.assertRaises(ValueError):
                vim_oracle.canonical_fixtures_json([fixture])
        vim_oracle.canonical_fixtures_json([{"name": "a", "text": "b", "keys": "d/qux<CR>"},
                                            {"name": "c", "text": "b", "keys": "?q<CR>@a",
                                             "register": {"name": "a", "keys": "/q<CR>x"}}])
        passing = ["fqx", "rq", "iq<Esc>", "cwq<Esc>x", "\"qyy", "diq", "vrq", "vcq<Esc>"]
        vim_oracle.canonical_fixtures_json(
            [{"name": f"p{index}", "text": "b", "keys": keys} for index, keys in enumerate(passing)]
            + [{"name": "q", "text": "b", "keys": "@q", "register": {"name": "q", "keys": "x"}}])

    def test_probe_script_lets_the_register_before_the_keys(self):
        script = vim_oracle.probe_script([], "@a", None, {"name": "a", "keys": "iab<Esc>"})
        lines = script.splitlines()
        let = lines.index('let @a = "iab\\<Esc>"')
        self.assertLess(let, next(index for index, line in enumerate(lines)
                                  if line.startswith('execute "normal! "')))
        self.assertNotIn("let @", vim_oracle.probe_script([], "x"))

    def test_probe_script_puts_a_count_before_a_leading_space(self):
        # :help :normal: the Ex line eats a leading space, so the oracle writes `1 ` (ADR 0049).
        for keys in (" x", "<Space>x"):
            self.assertIn('execute "normal! " . "1 x"', vim_oracle.probe_script([], keys))
        self.assertIn('execute "normal! " . "d x"', vim_oracle.probe_script([], "d<Space>x"))

    def test_header_row_carries_the_register_only_when_there_is_one(self):
        record = self.record({"name": "macro", "text": "ab", "keys": "@a"}, "b")
        plain = vim_oracle.fixture_row({**record, "viewport": None, "macro": None})
        self.assertTrue(plain.endswith(', "", "", std::nullopt},'))
        macro = vim_oracle.fixture_row({**record, "viewport": None,
                                        "macro": {"name": "a", "keys": "x"}})
        self.assertTrue(macro.endswith(', std::nullopt, VimMacroFixture{\'a\', "x"}},'))

    def test_canonical_form_is_idempotent_and_ends_with_one_newline(self):
        content = vim_oracle.canonical_fixtures_json(
            [{"name": "one", "text": "a", "keys": "x"}, {"name": "two", "text": "b", "keys": "y"}])
        self.assertTrue(content.endswith(b"}\n]\n"))
        self.assertEqual(content, vim_oracle.canonical_fixtures_json(
            json.loads(content.decode("utf-8"))))
        self.assertEqual(b"[]\n", vim_oracle.canonical_fixtures_json([]))

    def test_canonical_form_rejects_unknown_missing_and_wrong_shapes(self):
        for fixtures in [{}, [[]], [{"name": "a", "text": "b", "keys": "c", "note": "d"}],
                         [{"name": "a", "text": "b"}],
                         [{"name": "a", "text": "b", "keys": "c", "viewport": {"line": 1}}]]:
            with self.subTest(fixtures=fixtures), self.assertRaises(ValueError):
                vim_oracle.canonical_fixtures_json(fixtures)

    def reformat_inputs(self):
        """A reuse ref written in the old shape (indent 2 and an empty ``settings``)."""
        fixtures = [{"name": "old-kept", "text": "old", "keys": "h", "settings": []},
                    {"name": "second", "text": "new", "keys": "l", "settings": []}]
        old_content = (json.dumps(fixtures, indent=2) + "\n").encode("utf-8")
        records = [self.record(fixtures[0], "old expected"),
                   self.record(fixtures[1], "second expected")]
        version = "VIM 9.1 test"
        old_header = vim_oracle.header(records, version, hashlib.sha256(old_content).hexdigest())
        canonical = vim_oracle.canonical_fixtures_json(fixtures)
        return fixtures, canonical, old_content, old_header, version, records

    def test_reformatting_reuses_every_row_and_writes_only_the_new_digest(self):
        fixtures, canonical, old_content, old_header, version, records = self.reformat_inputs()
        rendered, reused_version = vim_oracle.reformatted_header(
            json.loads(canonical.decode("utf-8")), canonical, old_content, old_header)
        self.assertEqual(version, reused_version)
        for record in records:
            self.assertIn(vim_oracle.fixture_row(record), rendered)
        digest = hashlib.sha256(canonical).hexdigest()
        self.assertEqual(old_header.replace(hashlib.sha256(old_content).hexdigest(), digest),
                         rendered)

    def test_reformatting_refuses_a_changed_input_or_a_foreign_oracle(self):
        fixtures, canonical, old_content, old_header, version, _ = self.reformat_inputs()
        changed = vim_oracle.canonical_fixtures_json([dict(fixtures[0], text="changed"),
                                                     fixtures[1]])
        with self.assertRaisesRegex(ValueError, "change fixture inputs"):
            vim_oracle.reformatted_header(json.loads(changed.decode("utf-8")), changed,
                                          old_content, old_header)
        foreign = old_header.replace(str(vim_oracle.VIM), r"C:\Other\vim.exe")
        with self.assertRaises(ValueError):
            vim_oracle.reformatted_header(json.loads(canonical.decode("utf-8")), canonical,
                                          old_content, foreign)

    def test_writing_the_canonical_fixtures_replaces_the_file_only_when_it_differs(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "fixtures.json"
            source.write_bytes(b'[\n  {\n    "name": "a",\n    "text": "b",\n'
                               b'    "keys": "x",\n    "settings": []\n  }\n]')
            written = vim_oracle.write_canonical_fixtures(source)
            self.assertEqual(b'[\n  {"name":"a","text":"b","keys":"x"}\n]\n', written)
            self.assertEqual(written, source.read_bytes())
            self.assertEqual(written, vim_oracle.write_canonical_fixtures(source))
            self.assertEqual([source.name], [p.name for p in Path(directory).iterdir()])

    def test_run_vim_keeps_a_literal_cr_out_of_the_line_split(self):
        """The report is read as bytes, so a CR Vim wrote as a character stays one (ADR 0036)."""
        report = b"a\rb\r\nc\ncursor=1,2\nregtype=v\nreg=x\ry\n"

        def fake_run(command, **kwargs):
            (Path(kwargs["cwd"]) / "out.txt").write_bytes(report)
            return subprocess.CompletedProcess(command, 0, b"", b"")

        with tempfile.TemporaryDirectory() as directory, \
                patch.object(vim_oracle.subprocess, "run", fake_run):
            body, line, column, register, kind, viewport = vim_oracle.run_vim(
                Path(directory), "a\rb\r\nc", "h", [])
        self.assertEqual("a\rb\r\nc", body)
        self.assertEqual((1, 2), (line, column))
        self.assertEqual(("x\ry", "v", None), (register, kind, viewport))

    def test_literal_cr_text_is_accepted_only_when_vim_and_we_read_it_the_same_way(self):
        vim_oracle.check_literal_cr("plain", "abcd\nefgh")
        vim_oracle.check_literal_cr("inside-a-line", "ab\rcd\nefgh")
        vim_oracle.check_literal_cr("at-a-later-line-end", "abc\nde\r\nfgh")
        with self.assertRaisesRegex(ValueError, "Vim reads the text as dos"):
            vim_oracle.check_literal_cr("all-lines", "ab\r\ncd\r")
        with self.assertRaisesRegex(ValueError, "Vim reads the text as dos"):
            vim_oracle.check_literal_cr("one-line", "ab\r")
        with self.assertRaisesRegex(ValueError, "detect_line_ending reads the"):
            vim_oracle.check_literal_cr("first-line", "a\r\nbc\ndef")

    def test_measure_refuses_a_literal_cr_text_before_starting_vim(self):
        run = Mock()
        with tempfile.TemporaryDirectory() as directory, \
                patch.object(vim_oracle.subprocess, "run", run):
            with self.assertRaises(ValueError):
                vim_oracle.measure(Path(directory),
                                   {"name": "crlf", "text": "ab\r\ncd", "keys": "h"})
        run.assert_not_called()

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
