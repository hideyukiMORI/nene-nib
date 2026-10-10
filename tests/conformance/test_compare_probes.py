"""Focused protocol and fixed-order checks for the local comparison tool (#329)."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("compare_probes", ROOT / "eng/compare-probes.py")
PROBES = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PROBES)
COMMIT = "a" * 40


class ProbeComparisonTests(unittest.TestCase):
    def setUp(self):
        directory = ROOT / "out/probe-tool-tests"
        directory.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=directory)
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        before = self.folder / "before.exe"
        after = self.folder / "after.exe"
        before.write_bytes(b"before executable")
        after.write_bytes(b"after executable")
        self.options = argparse.Namespace(before=before, after=after, before_ref="before-ref",
                                          after_ref="after-ref", before_commit=COMMIT,
                                          after_commit=COMMIT, workload="display-line-long",
                                          iterations=2, blocks=1, timeout=1.0,
                                          output=self.folder / "comparison.json")

    def runner(self, command, timeout):
        self.assertEqual(timeout, 1.0)
        iterations = int(command[2])
        data = PROBES.fixed_input(command[1])
        metadata = {"schema": 1, "workload": command[1], "iterations": iterations, "warmup": 1,
                    "inputBytes": len(data), "inputHashAlgorithm": "fnv1a64",
                    "inputHash": PROBES.fnv1a64(data), "outputChecksum": "123",
                    "commit": COMMIT, "dirty": 0, "configuration": "Release",
                    "compilerVersion": "19.1.5", "toolchainSha256": "b" * 64}
        marks = [{"milestone": name, "qpcMicroseconds": index * 100 + offset}
                 for index in range(iterations)
                 for name, offset in (("probe_started", 10), ("probe_finished", 20))]
        Path(command[3]).write_text(json.dumps({"qpcFrequency": 1000000, "marks": marks}), encoding="utf-8")
        return subprocess.CompletedProcess(command, 0, json.dumps(metadata), "")

    def test_fixed_abba_preserves_every_sample(self):
        report, code = PROBES.compare(self.options, self.runner)
        self.assertEqual(code, 0)
        self.assertEqual([run["side"] for run in report["runs"]], ["before", "after", "after", "before"])
        self.assertEqual(report["summary"]["pairedRatiosAfterOverBefore"], [1.0] * 4)
        self.assertEqual([run["microseconds"] for run in report["runs"]], [[10, 10]] * 4)
        self.assertFalse(report["warmupMeasured"])
        self.assertNotEqual(report["executables"]["before"]["sha256"], report["executables"]["after"]["sha256"])
        with self.assertRaises(PROBES.ProbeNotObserved):
            PROBES.compare(self.options, self.runner)

    def test_one_side_failure_is_not_a_comparison(self):
        def failed(command, timeout):
            if Path(command[0]).name == "after.exe":
                return subprocess.CompletedProcess(command, 1, "", "wrong result")
            return self.runner(command, timeout)
        report, code = PROBES.compare(self.options, failed)
        self.assertEqual(code, 2)
        self.assertNotIn("summary", report)
        self.assertEqual(report["runs"][-1]["stderr"], "wrong result")
        self.assertEqual(len(report["plannedOrder"]), 4)

    def test_checksum_mismatch_is_not_a_comparison(self):
        def changed(command, timeout):
            result = self.runner(command, timeout)
            if Path(command[0]).name == "after.exe":
                data = json.loads(result.stdout)
                data["outputChecksum"] = "124"
                result.stdout = json.dumps(data)
            return result
        report, code = PROBES.compare(self.options, changed)
        self.assertEqual(code, 2)
        self.assertIn("checksum", report["failure"])
        self.assertNotIn("summary", report)

    def test_timeout_preserves_missing_trial(self):
        def timed_out(command, timeout):
            raise subprocess.TimeoutExpired(command, timeout)
        report, code = PROBES.compare(self.options, timed_out)
        self.assertEqual(code, 2)
        self.assertIn("TimeoutExpired", report["failure"])
        self.assertEqual(len(report["runs"]), 1)

    def test_broken_timing_reports_are_rejected(self):
        valid = [{"milestone": "probe_started", "qpcMicroseconds": 10},
                 {"milestone": "probe_finished", "qpcMicroseconds": 20}]
        cases = ([], {"qpcFrequency": 0, "marks": valid},
                 {"qpcFrequency": 1, "marks": valid[:1]},
                 {"qpcFrequency": 1, "marks": valid[::-1]},
                 {"qpcFrequency": 1, "marks": [valid[0], {**valid[1], "qpcMicroseconds": 10}]},
                 {"qpcFrequency": 1, "marks": [{**valid[0], "qpcMicroseconds": True}, valid[1]]})
        for report in cases:
            with self.subTest(report=report), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.samples_from(report, 1)
        self.assertEqual(PROBES.samples_from({"qpcFrequency": 1, "marks": valid}, 1), [10])

    def test_invalid_json_and_metadata_are_rejected(self):
        for text in ('{"marks":[],"marks":[]}', '{"value":NaN}', '[1,2]'):
            with self.subTest(text=text), self.assertRaises((PROBES.ProbeNotObserved, TypeError)):
                PROBES.metadata_from(text, {"workload": "display-line-long", "iterations": 2, "commit": COMMIT}, {})
        result = self.runner([str(self.options.before), self.options.workload, "2", str(self.folder / "marks.json")], 1.0)
        original = json.loads(result.stdout)
        expected = {key: original[key] for key in ("inputBytes", "inputHashAlgorithm", "inputHash")}
        for key, value in (("configuration", "Debug"), ("commit", "c" * 40), ("dirty", 1),
                           ("iterations", 3), ("inputHash", "0"), ("warmup", True)):
            with self.subTest(key=key), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.metadata_from(json.dumps({**original, key: value}),
                                     {"workload": self.options.workload, "iterations": 2, "commit": COMMIT}, expected)

    def test_fixed_deleted_input_sizes(self):
        self.assertEqual(len(PROBES.fixed_input("controller-insert-200-after-delete-1mib")), 1048572)
        self.assertEqual(len(PROBES.fixed_input("controller-insert-200-after-delete-16mib")), 16800000)

    def test_fixed_utf8_validation_counts(self):
        ascii_input = PROBES.fixed_input("utf8-validate-ascii-16mib")
        japanese = PROBES.fixed_input("utf8-validate-japanese-6mib")
        self.assertEqual(len(ascii_input), 16800000)
        self.assertEqual(len(ascii_input.decode("utf-8")), 16800000)
        self.assertEqual(len(japanese), 6219000)
        self.assertEqual(len(japanese.decode("utf-8")), 2079000)

    def test_stage_two_fixed_vim_inputs(self):
        cases = (
            ("controller-vim-insert-200-register-1mib", b"r", 1048576,
             "1f763ea478ec75459ed5b2b86463a21ebffe4c3ce8604d1e8c8ca7018f091ab1", "17443442650570171173"),
            ("controller-vim-insert-200-register-16mib", b"r", 16777216,
             "d578db1458827855ca402ad0a78c5bb8003d9302409d547f814f0a690c10dadd", "7866584971205550885"),
            ("controller-vim-record-insert-200", b"x", 200,
             "aa20c23e3201834050679e1d88941b9a6fed0557c9a705cb2c315e2e63fd486d", "7989751381043797061"),
            ("controller-vim-record-insert-2000", b"x", 2000,
             "5c0e0ea421571c300b5df6aec0a118b5c3dc02e0683a546341d5efc689df2f58", "5526198219355535973"))
        for workload, byte, count, sha256, fnv in cases:
            with self.subTest(workload=workload):
                self.assertIn(workload, PROBES.WORKLOADS)
                data = PROBES.fixed_input(workload)
                self.assertEqual(data, byte * count)
                self.assertEqual(len(data), count)
                self.assertEqual(hashlib.sha256(data).hexdigest(), sha256)
                self.assertEqual(PROBES.fnv1a64(data), fnv)


    def test_stage_five_pattern_inputs(self):
        cases = (("pattern-star-miss-1024", b"a" * 1024),
                 ("pattern-star-miss-2048", b"a" * 2048),
                 ("pattern-star-miss-4096", b"a" * 4096),
                 ("pattern-multistar-miss-32", b"a" * 32),
                 ("pattern-greedy-hit-4096", b"a" * 4096),
                 ("pattern-literal-tail-4102", b"x" * 4096 + b"needle"),
                 ("pattern-literal-long-4096", b"a" * 4096))
        self.assertEqual(tuple(name for name, _ in cases), PROBES.WORKLOADS[41:48])
        self.assertEqual(len(PROBES.WORKLOADS[:48]), 48)
        self.assertEqual(len(set(PROBES.WORKLOADS[:48])), 48)
        for name, expected in cases:
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), expected)
        for name in ("pattern-star-miss-1023", "pattern-literal-tail-4101", "pattern-greedy-hit-4095"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_stage_six_frame_inputs(self):
        cases = (("frame-search-dense-ascii-8192", b"a x " * 8192, "2381364893216809765"),
                 ("frame-search-dense-mixed-4096", "日a\t🖋\x01 ".encode("utf-8") * 4096,
                  "10068659747010089765"),
                 ("frame-search-sparse-tail-32768", b"x" * 32767 + b"a", "6746773668288547386"),
                 ("frame-selection-ascii-32768", b"a x " * 8192, "2381364893216809765"),
                 ("frame-search-visual-ascii-8192", b"a x " * 8192, "2381364893216809765"))
        self.assertEqual(tuple(name for name, _, _ in cases), PROBES.WORKLOADS[48:53])
        self.assertEqual(len(PROBES.WORKLOADS[:53]), 53)
        self.assertEqual(len(set(PROBES.WORKLOADS[:53])), 53)
        for name, expected, fnv in cases:
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), expected)
                self.assertEqual(PROBES.fnv1a64(expected), fnv)
        for name in ("frame-search-dense-ascii-8191", "frame-search-sparse-tail-32767"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_frame_rows_inputs_and_registry(self):
        unit = bytes.fromhex("61 e6 97 a5 09 f0 9f 96 8b 01 20 72 6f 77")
        cases = (("frame-rows-empty-64", b"", 0, "14695981039346656037"),
                 ("frame-rows-short-64", unit + b"\r\ntail", 20, "17662855059157275171"),
                 ("frame-rows-30-64", (unit + b"\r\n") * 119 + b"tail", 1908,
                  "11632098944160318499"),
                 ("frame-rows-120-64", (unit + b"\r\n") * 119 + b"tail", 1908,
                  "11632098944160318499"))
        self.assertEqual(tuple(name for name, _, _, _ in cases), PROBES.WORKLOADS[53:57])
        self.assertEqual(len(PROBES.WORKLOADS[:57]), 57)
        self.assertEqual(len(set(PROBES.WORKLOADS[:57])), 57)
        for name, expected, size, fnv in cases:
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), expected)
                self.assertEqual(len(expected), size)
                self.assertEqual(PROBES.fnv1a64(expected), fnv)

    def test_frame_document_inputs_and_registry(self):
        cases = (
            ('frame-document-short-saved-256', b'C:\\nib-probe\\frame-document.txt', 31, '3165114504819470625'),
            ('frame-document-long-saved-256', b'C:\\nib-probe\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.txt', 241, '3259355916674798828'),
            ('frame-document-long-failed-256', b'C:\\nib-probe\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.txt', 241, '3259355916674798828'),
            ('frame-document-untitled-256', b'', 0, '14695981039346656037'),
        )
        self.assertEqual(tuple(row[0] for row in cases), PROBES.WORKLOADS[72:76])
        self.assertEqual(len(PROBES.WORKLOADS), 76)
        self.assertEqual(len(set(PROBES.WORKLOADS)), 76)
        for name, expected, size, fnv in cases:
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), expected)
                self.assertEqual(len(expected), size)
                self.assertEqual(PROBES.fnv1a64(expected), fnv)

    def test_frame_document_invalid_names(self):
        for name in ("frame-document-long-saved-255", "frame-document-short-saved-64",
                     "frame-document-unknown-256"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_preview_caret_inputs_and_registry(self):
        cases = (
            ("preview-caret-long-30-64", 3674, "3943064766360993406"),
            ("preview-caret-long-120-64", 3674, "3943064766360993406"),
            ("preview-caret-short-30-64", 605, "5659432768607348982"),
            ("preview-caret-confirmed-30-64", 3674, "3943064766360993406"),
            ("preview-caret-unsearched-30-64", 3674, "3943064766360993406"),
            ("preview-caret-absent-30-64", 3674, "3943064766360993406"),
            ("preview-caret-disabled-30-64", 3674, "3943064766360993406"),
        )
        self.assertEqual(tuple(name for name, _, _ in cases), PROBES.WORKLOADS[65:72])
        self.assertEqual(len(PROBES.WORKLOADS[:72]), 72)
        self.assertEqual(len(set(PROBES.WORKLOADS[:72])), 72)
        for name, size, fnv in cases:
            count = 1 if name == "preview-caret-short-30-64" else 1024
            expected = bytes.fromhex("e6 97 a5") * count + bytes.fromhex("7a 09 f0 9f 96 8b 01")
            expected += b"\r\nz x" * 119
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), expected)
                self.assertEqual(len(expected), size)
                self.assertEqual(PROBES.fnv1a64(expected), fnv)

    def test_preview_caret_invalid_names(self):
        for name in ("preview-caret-long-31-64", "preview-caret-short-30-63",
                     "preview-caret-disabled-30-65"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_search_snapshot_inputs_and_registry(self):
        cases = (
            ("search-snapshot-frame-short-64", 1, "12638187200555641996"),
            ("search-snapshot-frame-long-64", 256, "18242136092491110437"),
            ("search-snapshot-repeat-long-64", 256, "18242136092491110437"),
            ("search-snapshot-retained-long-200", 256, "18242136092491110437"),
            ("search-snapshot-retained-short-200", 1, "12638187200555641996"),
            ("search-snapshot-unsearched-200", 0, "14695981039346656037"),
            ("search-snapshot-commit-short-64", 1, "12638187200555641996"),
            ("search-snapshot-typing-frame-short-64", 1, "12638187200555641996"),
        )
        self.assertEqual(tuple(name for name, _, _ in cases), PROBES.WORKLOADS[57:65])
        self.assertEqual(len(PROBES.WORKLOADS[:65]), 65)
        self.assertEqual(len(set(PROBES.WORKLOADS[:65])), 65)
        for name, size, fnv in cases:
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), b"a" * size)
                self.assertEqual(PROBES.fnv1a64(b"a" * size), fnv)

    def test_search_snapshot_invalid_names(self):
        for name in ("search-snapshot-frame-long-63", "search-snapshot-retained-long-201",
                     "search-snapshot-typing-frame-short-65"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_frame_rows_invalid_names(self):
        for name in ("frame-rows-empty-63", "frame-rows-short-65", "frame-rows-31-64",
                     "frame-rows-121-64"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_stage_four_registry_and_fixed_inputs(self):
        self.assertEqual(len(PROBES.WORKLOADS[:41]), 41)
        self.assertEqual(len(set(PROBES.WORKLOADS[:41])), 41)
        groups = ((PROBES.WORKLOADS[24:26], PROBES.fixed_input("controller-open-utf8-16mib"), 16800000),
                  (PROBES.WORKLOADS[26:31], (b"a" * 78 + b"\r\n") * 4096, 327680),
                  (PROBES.WORKLOADS[31:35], "a日本語🖋".encode("utf-8") * 4096, 57344),
                  (PROBES.WORKLOADS[35:41], "a日本語🖋 ".encode("utf-8") * 4096, 61440))
        for names, expected, size in groups:
            for name in names:
                with self.subTest(workload=name):
                    data = PROBES.fixed_input(name)
                    self.assertEqual(data, expected)
                    self.assertEqual(len(data), size)
        for name in ("buffer-erase-scattered-head-4095", "buffer-offset-long-head-57343",
                     "search-forward-head-many-4095", "controller-save-utf8-16mb"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_stage_three_registry_and_unknown_names(self):
        self.assertEqual(len(PROBES.WORKLOADS[:24]), 24)
        self.assertEqual(len(set(PROBES.WORKLOADS[:24])), 24)
        for workload in ("buffer-line-text-scattered-crlf-4095", "palette-listed-name-4999", "unknown"):
            with self.subTest(workload=workload), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(workload)

    def test_stage_three_buffer_inputs(self):
        crlf = PROBES.fixed_input("buffer-line-text-scattered-crlf-4096")
        lf = PROBES.fixed_input("buffer-line-text-scattered-lf-4096")
        self.assertEqual(crlf, (b"a" * 78 + b"\r\n") * 4096)
        self.assertEqual(lf, (b"a" * 78 + b"\n") * 4096)
        self.assertEqual((len(crlf), len(lf)), (327680, 323584))
        position = PROBES.fixed_input("buffer-position-long-utf8-57344")
        self.assertEqual(position, PROBES.fixed_input("buffer-position-scattered-utf8-57344"))
        self.assertEqual(position.decode("utf-8"), "a日本語🖋" * 4096)
        self.assertEqual((len(position), len(position.decode("utf-8"))), (57344, 20480))

    def test_stage_three_palette_serialization(self):
        for workload, query in (("palette-listed-name-5000", "entry"),
                                ("palette-listed-location-5000", "zroot")):
            rows = PROBES.fixed_input(workload).decode("utf-8").splitlines()
            self.assertEqual(rows[0], f"files\t{query}")
            self.assertEqual(len(rows), 5001)
            for index, row in enumerate(rows[1:]):
                self.assertEqual(row.split("\t"), ["folder", "open", f"probe-{index:05d}",
                    f"ENTRY_{index:05d}_ABCDEFGHIJKLMNOPQRSTUV.txt",
                    r"D:\ZROOT\LONG_DIRECTORY_COMPONENT\GROUP_00"])
                self.assertNotIn("Z", row.split("\t")[3])
        data = PROBES.fixed_input("palette-append-narrow-5000-to-50")
        self.assertEqual(data, PROBES.fixed_input("palette-caret-left-5000"))
        rows = data.decode("utf-8").splitlines()
        self.assertEqual(rows[0], "files\tqx")
        labels = [row.split("\t")[3] for row in rows[1:]]
        self.assertEqual(sum(label.startswith("QX_") for label in labels), 500)
        self.assertEqual(sum(label.startswith("QX_Y_") for label in labels), 50)
        self.assertEqual([row.split("\t")[2] for row in rows[1:]],
                         [f"probe-{index:05d}" for index in range(5000)])
        self.assertTrue(all(row.endswith("\t") for row in rows[1:]))

    def test_stage_three_conversion_bytes_and_units(self):
        cases = (("codepage-to-utf8-cp932-japanese-16mib", "cp932", "日本" * 4194304),
                 ("utf16-to-utf8-japanese-8m-units", "utf-16-le", "日本" * 4194304),
                 ("utf16-to-utf8-ascii-8m-units", "utf-16-le", "a" * 8388608),
                 ("utf16-to-utf8-supplementary-8m-units", "utf-16-le", "🖋" * 4194304))
        for workload, encoding, expected in cases:
            with self.subTest(workload=workload):
                data = PROBES.fixed_input(workload)
                self.assertEqual(len(data), 16777216)
                self.assertEqual(data.decode(encoding), expected)

    def test_stage_three_metadata_uses_actual_serialized_input(self):
        self.options.workload = "palette-caret-left-5000"
        result = self.runner([str(self.options.before), self.options.workload, "2",
                              str(self.folder / "new-marks.json")], 1.0)
        data = PROBES.fixed_input(self.options.workload)
        expected = {"inputBytes": len(data), "inputHashAlgorithm": "fnv1a64",
                    "inputHash": PROBES.fnv1a64(data)}
        request = {"workload": self.options.workload, "iterations": 2, "commit": COMMIT}
        self.assertEqual(PROBES.metadata_from(result.stdout, request, expected)["inputBytes"], len(data))
        metadata = json.loads(result.stdout)
        for key, value in (("inputBytes", len(data) + 1), ("inputHash", "0"),
                           ("workload", "palette-listed-name-5000")):
            with self.subTest(key=key), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.metadata_from(json.dumps({**metadata, key: value}), request, expected)


if __name__ == "__main__":
    unittest.main()
