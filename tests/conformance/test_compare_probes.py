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


if __name__ == "__main__":
    unittest.main()
