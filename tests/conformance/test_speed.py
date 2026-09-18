"""Positive and negative inputs for the speed record and its judgement (QLT-007 / QLT-014).

Issue #30: a trial whose stimulus did not reach the window is missing rather than a value, and a
bench with too few valid trials is "not measured" rather than "slower". Issue #36: a trial that
could not be observed at all joins that same path. Everything here is a pure function of a record
or of a scripted trial: no window is opened, no editor is started, and no clock is read.
"""

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "eng"))
spec = importlib.util.spec_from_file_location("nib_speed", ROOT / "eng/measure-speed.py")
speed = importlib.util.module_from_spec(spec)
spec.loader.exec_module(speed)
REFERENCE = {"tolerance": {"percent": 25, "floorMs": 2}}


def marks(inputs: int) -> list:
    """A measurement file's marks: one input_received per keystroke, each answered by a frame."""
    entries = []
    for index in range(inputs):
        entries.append({"milestone": "input_received", "qpcMicroseconds": 1000 * (index + 1)})
        entries.append({"milestone": "frame_presented", "qpcMicroseconds": 1000 * (index + 1) + 500})
    return entries


def record(values: dict) -> dict:
    """A record with every bench at 100 ms, overridden by name with (samples, missing)."""
    written = {}
    for name in speed.BENCHES:
        samples, missing = values.get(name, ([100.0] * 5, 0))
        written[name] = speed.summarise({name: samples}, {name: missing})[name]
    return {"recordedAt": "test", "repetitions": 5, "values": written,
            "machine": {"fingerprint": "test", "cpu": "cpu", "gpu": "gpu", "dpi": 96}}


def reference_values() -> dict:
    return {name: {"medianMs": 100.0, "minimumMs": 100.0, "maximumMs": 100.0}
            for name in speed.BENCHES}


class SummaryTests(unittest.TestCase):
    def test_missing_trials_leave_the_median_to_the_valid_ones(self):
        summary = speed.summarise({"key-to-frame-burst-200": [2.0, 3.0, 4.0]},
                                  {"key-to-frame-burst-200": 2})["key-to-frame-burst-200"]
        self.assertEqual(3.0, summary["medianMs"])
        self.assertEqual(2, summary["missing"])
        self.assertEqual([2.0, 3.0, 4.0], summary["samples"])
        self.assertEqual(2.0, summary["minimumMs"])
        self.assertEqual(4.0, summary["maximumMs"])

    def test_a_bench_without_a_valid_trial_is_recorded_as_null(self):
        summary = speed.summarise({"key-to-frame-burst-200": []},
                                  {"key-to-frame-burst-200": 5})["key-to-frame-burst-200"]
        self.assertEqual([], summary["samples"])
        self.assertIsNone(summary["medianMs"])
        self.assertIsNone(summary["minimumMs"])
        self.assertEqual(5, summary["missing"])

    def test_a_trial_that_delivered_the_stimulus_is_recorded_without_missing(self):
        summary = speed.summarise({"key-to-frame-single": [1.0]}, {})["key-to-frame-single"]
        self.assertEqual(0, summary["missing"])
        self.assertEqual(1.0, summary["medianMs"])

    def test_describe_says_how_many_trials_are_missing(self):
        written = speed.describe(record({"key-to-frame-burst-200": ([2.0, 3.0, 4.0], 2)}))
        self.assertIn("(missing 2 of 5)", written)
        self.assertIn("key-to-frame-single: median 100.000 ms", written)
        self.assertNotIn("missing 0", written)

    def test_describe_says_a_bench_no_trial_measured(self):
        written = speed.describe(record({"key-to-frame-burst-200": ([], 5)}))
        self.assertIn("key-to-frame-burst-200: no trial delivered the stimulus (missing 5 of 5)",
                      written)


class ComparisonTests(unittest.TestCase):
    def judge(self, values: dict) -> tuple:
        return speed.compare(REFERENCE, record(values)["values"], reference_values())

    def test_two_valid_trials_are_unmeasurable_and_not_a_regression(self):
        findings, unmeasurable = self.judge({"key-to-frame-burst-200": ([900.0, 900.0], 3)})
        self.assertEqual([], findings)
        self.assertEqual(["QLT-014: key-to-frame-burst-200: only 2 of 5 trials delivered the"
                          " stimulus; not judged"], unmeasurable)

    def test_three_valid_trials_over_the_tolerance_are_a_regression(self):
        findings, unmeasurable = self.judge({"key-to-frame-burst-200": ([900.0] * 3, 2)})
        self.assertEqual([], unmeasurable)
        self.assertEqual(1, len(findings))
        self.assertIn("QLT-014: key-to-frame-burst-200: 900.000 ms exceeds 125.000 ms", findings[0])

    def test_three_valid_trials_within_the_tolerance_pass(self):
        findings, unmeasurable = self.judge({"key-to-frame-burst-200": ([110.0] * 3, 2)})
        self.assertEqual(([], []), (findings, unmeasurable))

    def test_a_bench_without_a_reference_is_neither(self):
        recorded = reference_values()
        del recorded["key-to-frame-burst-200"]
        values = record({"key-to-frame-burst-200": ([], 5)})["values"]
        self.assertEqual(([], []), speed.compare(REFERENCE, values, recorded))

    def test_a_record_written_before_missing_existed_is_read(self):
        values = record({})["values"]
        for name in speed.BENCHES:
            del values[name]["missing"]
        findings, unmeasurable = speed.compare(REFERENCE, values, reference_values())
        self.assertEqual(([], []), (findings, unmeasurable))
        written = dict(record({}), values=values)
        self.assertIn("samples [100.0", speed.describe(written))
        self.assertNotIn("missing", speed.describe(written))

    def test_a_record_of_medians_alone_is_judged_on_them(self):
        """eng/prove-gates.py writes the reference's own counterexample as medians with no trials."""
        values = {name: {"medianMs": 900.0} for name in speed.BENCHES}
        findings, unmeasurable = speed.compare(REFERENCE, values, reference_values())
        self.assertEqual([], unmeasurable)
        self.assertEqual(len(speed.BENCHES), len(findings))

    def test_a_single_keystroke_bench_without_a_trial_is_unmeasurable(self):
        """Issue #36: an unobserved trial leaves key-to-frame-single missing the same way."""
        findings, unmeasurable = self.judge({"key-to-frame-single": ([], 5)})
        self.assertEqual([], findings)
        self.assertEqual(["QLT-014: key-to-frame-single: only 0 of 5 trials delivered the"
                          " stimulus; not judged"], unmeasurable)

    def test_an_old_record_with_too_few_samples_is_unmeasurable(self):
        values = record({"key-to-frame-burst-200": ([2.0, 3.0], 0)})["values"]
        del values["key-to-frame-burst-200"]["missing"]
        _, unmeasurable = speed.compare(REFERENCE, values, reference_values())
        self.assertEqual(["QLT-014: key-to-frame-burst-200: only 2 of 2 trials delivered the"
                          " stimulus; not judged"], unmeasurable)


class ExitCodeTests(unittest.TestCase):
    def test_nothing_found_leaves_with_zero(self):
        self.assertEqual(0, speed.exit_code([], []))

    def test_a_regression_leaves_with_one(self):
        self.assertEqual(1, speed.exit_code(["QLT-014: slower"], []))

    def test_an_unmeasurable_bench_leaves_with_two(self):
        self.assertEqual(2, speed.exit_code([], ["QLT-014: not judged"]))

    def test_a_regression_outranks_an_unmeasurable_bench(self):
        self.assertEqual(1, speed.exit_code(["QLT-014: slower"], ["QLT-014: not judged"]))


class TrialTests(unittest.TestCase):
    def test_a_delivered_burst_is_a_value(self):
        self.assertIsNone(speed.burst_failure(2.0, 3.0, speed.BURST_KEYS + 2))

    def test_too_few_keystrokes_are_a_failed_stimulus(self):
        reason = speed.burst_failure(None, 0.0, 7)
        self.assertEqual("only 7 of 202 keystrokes were measured by the window", reason)

    def test_a_burst_that_arrived_apart_is_a_failed_stimulus(self):
        reason = speed.burst_failure(2.0, speed.BURST_SPAN_LIMIT_MS + 0.1, speed.BURST_KEYS + 2)
        self.assertIn("not together", reason)

    def test_a_burst_without_a_frame_is_a_failed_stimulus(self):
        reason = speed.burst_failure(None, 3.0, speed.BURST_KEYS + 2)
        self.assertEqual("no frame was presented after the 200 keystrokes", reason)

    def test_the_single_keystroke_is_a_value_when_its_frame_was_measured(self):
        entries = marks(3)
        self.assertEqual(0.5, speed.measured_single(entries, speed.input_indexes(entries)))

    def test_the_single_keystroke_is_missing_when_no_keystroke_followed_the_warmup(self):
        entries = marks(1)
        self.assertIsNone(speed.measured_single(entries, speed.input_indexes(entries)))

    def test_the_single_keystroke_is_missing_when_no_frame_answered_it(self):
        entries = [{"milestone": "input_received", "qpcMicroseconds": 1000},
                   {"milestone": "input_received", "qpcMicroseconds": 2000}]
        self.assertIsNone(speed.measured_single(entries, speed.input_indexes(entries)))


class MeasurementFileTests(unittest.TestCase):
    """A trial's own reading of its measurement file: absent or unfinished is not a machine (#36)."""

    def test_a_finished_measurement_file_gives_its_marks(self):
        entries = marks(2)
        written = json.dumps({"processCreationToFirstFrameMs": 1.0, "marks": entries})
        self.assertEqual(entries, speed.marks_of(written))

    def test_a_file_that_was_never_written_is_not_observed(self):
        self.assertIsNone(speed.marks_of(""))

    def test_a_file_that_stops_in_the_middle_is_not_observed(self):
        self.assertIsNone(speed.marks_of('{"marks": [{"milestone": "input_rece'))

    def test_a_json_without_marks_is_not_observed(self):
        self.assertIsNone(speed.marks_of('{"processCreationToFirstFrameMs": 1.0}'))

    def test_a_json_whose_marks_are_not_a_list_is_not_observed(self):
        self.assertIsNone(speed.marks_of('{"marks": null}'))

    def test_a_json_that_is_not_an_object_is_not_observed(self):
        self.assertIsNone(speed.marks_of("[]"))


class RepeatedTrialTests(unittest.TestCase):
    """bench_keys against scripted trials; keys_trial is replaced, so nothing is started (#36)."""

    def bench(self, trials: list) -> tuple[dict, str]:
        outcomes = iter(trials)

        def scripted(executable, environment, folder):
            outcome = next(outcomes)
            if isinstance(outcome, Exception):
                raise outcome
            return outcome

        written = io.StringIO()
        with mock.patch.object(speed, "keys_trial", scripted), \
                contextlib.redirect_stdout(written):
            values, _ = speed.bench_keys(Path("exe"), {}, Path("folder"))
        return values, written.getvalue()

    def delivered(self, single: float, burst: float) -> tuple:
        return (single, burst, 1.0, speed.BURST_KEYS + 2)

    def test_three_unobserved_trials_leave_both_keystroke_benches_missing(self):
        unobserved = speed.TrialNotObserved("the unsaved confirmation never came up")
        values, written = self.bench([unobserved] * speed.BURST_ATTEMPTS)
        self.assertIsNone(values["key-to-frame-burst-200"])
        self.assertIsNone(values["key-to-frame-single"])
        self.assertIn("Speed: the unsaved confirmation never came up; repeating the trial (1/3)",
                      written)
        self.assertIn("this trial of key-to-frame-burst-200 is missing", written)

    def test_a_trial_after_an_unobserved_one_is_the_value(self):
        values, written = self.bench([speed.TrialNotObserved("the editor left no finished"
                                                             " measurement file (marks-keys.json)"),
                                      self.delivered(0.9, 2.5)])
        self.assertEqual({"key-to-frame-single": 0.9, "key-to-frame-burst-200": 2.5}, values)
        self.assertIn("no finished measurement file (marks-keys.json); repeating the trial (1/3)",
                      written)
        self.assertNotIn("missing", written)

    def test_an_unobserved_last_trial_does_not_keep_an_earlier_single(self):
        values, _ = self.bench([(0.9, None, 0.0, 7), (0.8, None, 0.0, 7),
                                speed.TrialNotObserved("the unsaved confirmation never came up")])
        self.assertIsNone(values["key-to-frame-single"])
        self.assertIsNone(values["key-to-frame-burst-200"])

    def test_a_single_keystroke_of_a_failed_burst_is_still_a_value(self):
        values, _ = self.bench([(0.9, None, 0.0, 7)] * speed.BURST_ATTEMPTS)
        self.assertEqual(0.9, values["key-to-frame-single"])
        self.assertIsNone(values["key-to-frame-burst-200"])


if __name__ == "__main__":
    unittest.main()
