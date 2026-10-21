"""Positive and negative inputs for the speed record and its judgement (QLT-007 / QLT-014).

Issue #30: a trial whose stimulus did not reach the window is missing rather than a value, and a
bench with too few valid trials is "not measured" rather than "slower". Issue #36: a trial that
could not be observed at all joins that same path. Everything here is a pure function of a record
or of a scripted trial: no window is opened, no editor is started, and no clock is read.
"""

import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import random
import shutil
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
        values = record({"key-to-frame-burst-200": ([900.0] * 5, 0)})["values"]
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

    def bench(self, trials: list, document: Path | None = None) -> tuple[dict, str]:
        outcomes = iter(trials)

        def scripted(executable, environment, folder, opened):
            self.assertEqual(document, opened)
            outcome = next(outcomes)
            if isinstance(outcome, Exception):
                raise outcome
            return outcome

        written = io.StringIO()
        with mock.patch.object(speed, "keys_trial", scripted), \
                contextlib.redirect_stdout(written):
            values, parts = speed.bench_keys(Path("exe"), {}, Path("folder"), document)
        self.parts = parts
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

    def test_the_burst_over_the_large_document_is_its_own_bench(self):
        """ADR 0044 decision 6: the same trial over the 16 MiB document gives the burst alone."""
        document = Path("large.txt")
        values, _ = self.bench([self.delivered(0.9, 7.5)], document)
        self.assertEqual({"key-to-frame-burst-200-16mib": 7.5}, values)

    def test_a_missing_burst_over_the_large_document_names_its_bench(self):
        document = Path("large.txt")
        values, written = self.bench([(0.9, None, 0.0, 7)] * speed.BURST_ATTEMPTS, document)
        self.assertEqual({"key-to-frame-burst-200-16mib": None}, values)
        self.assertIn("this trial of key-to-frame-burst-200-16mib is missing", written)

    def test_a_slow_arrival_over_the_large_document_is_a_value_with_its_span(self):
        """#179: over 16 MiB the span is the reading time being measured, so it is recorded."""
        document = Path("large.txt")
        span = speed.BURST_SPAN_LIMIT_MS + 30.0
        values, written = self.bench([(0.9, 90.0, span, speed.BURST_KEYS + 2)], document)
        self.assertEqual({"key-to-frame-burst-200-16mib": 90.0}, values)
        self.assertEqual({"key-to-frame-burst-200-16mib": {speed.ARRIVAL_SEGMENT: span}},
                         self.parts)
        self.assertNotIn("not together", written)

    def test_a_slow_arrival_over_the_empty_document_is_still_missing(self):
        span = speed.BURST_SPAN_LIMIT_MS + 30.0
        values, written = self.bench([(0.9, 90.0, span, speed.BURST_KEYS + 2)]
                                     * speed.BURST_ATTEMPTS)
        self.assertIsNone(values["key-to-frame-burst-200"])
        self.assertEqual({}, self.parts)
        self.assertIn("not together; repeating the trial (3/3)", written)


class BenchTableTests(unittest.TestCase):
    """The bench names are one table: --bench, --check, --adopt and the reference all read it."""

    def test_the_table_names_the_eight_benches(self):
        self.assertEqual(("startup-first-frame", "startup-window-shown", "key-to-frame-single",
                          "key-to-frame-burst-200", "open-large-file-16mib",
                          "key-to-frame-burst-200-16mib", "key-to-frame-palette-5000",
                          "key-to-frame-single-long-line"), speed.BENCHES)

    def test_the_reference_describes_every_bench_of_the_table(self):
        """eng/prove-gates.py builds its QLT-014 proof from these descriptions (ADR 0044 decision 6)."""
        reference = json.loads((ROOT / "eng/perf-reference.json").read_text(encoding="utf-8"))
        # split の 5 本は基準値に載せない（ADR 0148 決定 14）。判定は --split-verdict の対だけ。
        self.assertEqual([*speed.BENCHES, *speed.GIANT_BENCHES], list(reference["benches"]))
        self.assertFalse(set(speed.SPLIT_BENCHES) & set(reference["benches"]))
        self.assertIn("ADR 0044", reference["benches"]["key-to-frame-burst-200-16mib"])
        self.assertIn("ADR 0075", reference["benches"][speed.LONG_LINE_BENCH])
        for name in speed.GIANT_BENCHES:
            self.assertIn("ADR 0138", reference["benches"][name])

    def test_the_real_machine_judges_the_long_line_bench_by_the_floor(self):
        """#298: adopted with --adopt --bench (gate-proofs 5-cs); QLT-014 says the floor decides."""
        reference = json.loads((ROOT / "eng/perf-reference.json").read_text(encoding="utf-8"))
        adopted = reference["machines"]["bc8a356f37c68491"]["values"][speed.LONG_LINE_BENCH]
        self.assertLessEqual(adopted["minimumMs"], adopted["medianMs"])
        self.assertLessEqual(adopted["medianMs"], adopted["maximumMs"])
        tolerance = reference["tolerance"]
        self.assertLess(adopted["medianMs"] * tolerance["percent"] / 100.0, tolerance["floorMs"])

    def test_a_machine_without_the_new_reference_is_judged_on_the_rest(self):
        """A reference adopted before the sixth bench existed compares the five it has."""
        recorded = reference_values()
        del recorded["key-to-frame-burst-200-16mib"]
        values = record({"key-to-frame-burst-200-16mib": ([900.0] * 5, 0)})["values"]
        self.assertEqual(([], []), speed.compare(REFERENCE, values, recorded))

    def test_adopting_the_new_bench_leaves_the_other_references_alone(self):
        folder = ROOT / "out/conformance"
        folder.mkdir(parents=True, exist_ok=True)
        path = folder / "speed-adopt-one.json"
        before = reference_values()
        del before["key-to-frame-burst-200-16mib"]
        path.write_text(json.dumps({"machines": {"test": {"recordedAt": "old",
                                                          "values": before}}}), encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()):
            speed.adopt_one(path, record({"key-to-frame-burst-200-16mib": ([7.0] * 5, 0)}),
                            "key-to-frame-burst-200-16mib")
        after = json.loads(path.read_text(encoding="utf-8"))["machines"]["test"]
        path.unlink()
        self.assertEqual("old", after["recordedAt"])
        self.assertEqual({"medianMs": 7.0, "minimumMs": 7.0, "maximumMs": 7.0},
                         after["values"].pop("key-to-frame-burst-200-16mib"))
        self.assertEqual(before, after["values"])


class PaletteReadingTests(unittest.TestCase):
    def test_the_last_input_is_measured_after_two_or_three_opening_marks_and_warmup(self):
        for opening in (2, 3):
            with self.subTest(opening=opening):
                entries = marks(opening + 2)
                entries[-1]["qpcMicroseconds"] += 700
                self.assertEqual(1.2, speed.measured_palette(entries))

    def test_fewer_than_three_inputs_are_missing(self):
        for count in range(3):
            with self.subTest(count=count):
                self.assertIsNone(speed.measured_palette(marks(count)))

    def test_a_frame_before_the_last_input_does_not_answer_it(self):
        self.assertIsNone(speed.measured_palette(marks(4)[:-1]))

    def test_only_the_first_frame_after_the_last_input_is_used(self):
        entries = marks(4)
        entries[-1]["qpcMicroseconds"] += 1700
        entries.append({"milestone": "frame_presented", "qpcMicroseconds": 9000})
        self.assertEqual(2.2, speed.measured_palette(entries))


class PaletteTrialTests(unittest.TestCase):
    """Script every OS boundary, including the clock: never start a process or send real input."""

    def trial(self, chord=True, changed=False, observed=None, wait_error=None, capture_error=None):
        process = mock.Mock()
        process.pid = 456
        process.wait.side_effect = wait_error
        boundaries = {name: mock.Mock() for name in
                      ("start", "report_path", "raise_window", "take_foreground", "covered_by",
                       "press_chord", "write_text", "close", "dismiss_dialog", "stop",
                       "observed_marks", "time", "capture_png", "stamp")}
        boundaries["stamp"].return_value = "test"
        boundaries["capture_png"].side_effect = capture_error
        order = mock.Mock()
        order.attach_mock(boundaries["write_text"], "write_text")
        order.attach_mock(boundaries["capture_png"], "capture_png")
        boundaries["start"].return_value = process, 123, []
        boundaries["report_path"].return_value = Path("marks-palette.json")
        boundaries["take_foreground"].return_value = True
        boundaries["covered_by"].return_value = None
        boundaries["press_chord"].return_value = chord
        boundaries["dismiss_dialog"].return_value = changed
        boundaries["observed_marks"].return_value = marks(4)
        boundaries["observed_marks"].side_effect = observed
        with mock.patch.multiple(speed, **boundaries), contextlib.redirect_stdout(io.StringIO()):
            values, parts = speed.bench_palette(Path("exe"), {}, Path("folder"), Path("f0000.txt"))
        self.assertEqual({}, parts)
        boundaries["stop"].assert_called_once_with(process)
        self.assertEqual(int(capture_error is None), boundaries["close"].call_count)
        self.order = order
        return values[speed.PALETTE_BENCH], boundaries

    def test_success_posts_warmup_then_the_measured_character(self):
        value, calls = self.trial()
        self.assertEqual(0.5, value)
        calls["start"].assert_called_once_with(
            Path("exe"), {}, ["--measure", "marks-palette.json", "f0000.txt"])
        calls["press_chord"].assert_called_once_with(123, speed.VK_CONTROL, ord("P"))
        self.assertEqual([mock.call(123, "f"), mock.call(123, "0")],
                         calls["write_text"].call_args_list)
        self.assertEqual([mock.call.write_text(123, "f"),
                          mock.call.capture_png(123, Path("folder/palette-test-456.png")),
                          mock.call.write_text(123, "0")], self.order.mock_calls)
        self.assertEqual([mock.call(speed.SETTLE_SECONDS), mock.call(speed.PALETTE_WAIT_SECONDS),
                          mock.call(speed.WARMUP_SECONDS), mock.call(speed.SINGLE_KEY_SECONDS)],
                         calls["time"].sleep.call_args_list)

    def test_failed_ctrl_p_sends_no_characters_and_is_missing(self):
        value, calls = self.trial(chord=False)
        self.assertIsNone(value)
        calls["write_text"].assert_not_called()
        calls["observed_marks"].assert_not_called()
        calls["capture_png"].assert_not_called()

    def test_unsaved_confirmation_invalidates_the_trial(self):
        value, calls = self.trial(changed=True)
        self.assertIsNone(value)
        calls["observed_marks"].assert_not_called()

    def test_an_unfinished_report_is_missing(self):
        value, _ = self.trial(observed=speed.TrialNotObserved("unfinished report"))
        self.assertIsNone(value)

    def test_an_unfinished_close_is_missing(self):
        value, _ = self.trial(wait_error=speed.subprocess.TimeoutExpired("exe", 15))
        self.assertIsNone(value)

    def test_failed_or_covered_capture_is_missing_and_sends_no_measured_character(self):
        errors = (speed.WindowCovered(9, "cover", 8, (1, 1)), OSError("write failed"),
                  AssertionError("capture failed"))
        for error in errors:
            with self.subTest(error=type(error).__name__):
                value, calls = self.trial(capture_error=error)
                self.assertIsNone(value)
                calls["write_text"].assert_called_once_with(123, "f")
                calls["observed_marks"].assert_not_called()


class SelectedMeasurementTests(unittest.TestCase):
    def scripted_measurement(self, bench):
        document = mock.Mock()
        document.name = "large.txt"
        document.stat.return_value.st_size = 123
        boundaries = {name: mock.Mock() for name in
                      ("become_dpi_aware", "prepare", "large_document", "palette_document",
                       "long_line_document", "stamp", "machine", "bench_startup", "bench_keys",
                       "bench_large_file", "bench_palette", "bench_long_line")}
        boundaries["prepare"].return_value = Path("exe"), {}, Path("folder")
        boundaries["large_document"].return_value = document
        boundaries["palette_document"].return_value = Path("f0000.txt")
        boundaries["long_line_document"].return_value = Path("long-japanese.txt")
        boundaries["stamp"].return_value = "test"
        boundaries["machine"].return_value = record({})["machine"]
        boundaries["bench_startup"].return_value = {
            "startup-first-frame": 1.0, "startup-window-shown": 2.0}, {}
        boundaries["bench_keys"].side_effect = lambda *args: (
            ({"key-to-frame-single": 3.0, speed.EMPTY_BURST_BENCH: 4.0}, {}) if len(args) == 3
            else ({speed.LARGE_BURST_BENCH: 6.0}, {}))
        boundaries["bench_large_file"].return_value = {"open-large-file-16mib": 5.0}, {}
        boundaries["bench_palette"].return_value = {speed.PALETTE_BENCH: 7.0}, {}
        boundaries["bench_long_line"].return_value = {speed.LONG_LINE_BENCH: 8.0}, {}
        with mock.patch.multiple(speed, **boundaries):
            written = speed.measure(Path("build"), 1, bench)
        return written, boundaries

    def test_each_selection_runs_only_its_shared_stimulus_and_records_only_that_value(self):
        groups = ("bench_startup", "bench_startup", "bench_keys", "bench_keys",
                  "bench_large_file", "bench_keys", "bench_palette", "bench_long_line")
        for index, (name, group) in enumerate(zip(speed.BENCHES, groups)):
            with self.subTest(bench=name):
                written, calls = self.scripted_measurement(name)
                self.assertEqual({name}, set(written["values"]))
                self.assertEqual(index + 1.0, written["values"][name]["medianMs"])
                self.assertEqual(0, written["values"][name]["missing"])
                for candidate in set(groups):
                    self.assertEqual(int(candidate == group), calls[candidate].call_count)
                is_large = name in ("open-large-file-16mib", speed.LARGE_BURST_BENCH)
                self.assertEqual(int(is_large), calls["large_document"].call_count)
                self.assertEqual(is_large, "document" in written)
                self.assertEqual(int(name == speed.PALETTE_BENCH),
                                 calls["palette_document"].call_count)
                self.assertEqual(int(name == speed.LONG_LINE_BENCH),
                                 calls["long_line_document"].call_count)
                self.assertEqual(set(speed.BREAKDOWN_BENCHES).intersection({name}),
                                 set(written["breakdown"]))

    def test_no_selection_runs_all_trials_and_records_eight_values(self):
        written, calls = self.scripted_measurement(None)
        self.assertEqual(list(speed.BENCHES), list(written["values"]))
        self.assertEqual(2, calls["bench_keys"].call_count)
        for name in ("bench_startup", "bench_large_file", "bench_palette", "bench_long_line"):
            calls[name].assert_called_once()
        calls["bench_long_line"].assert_called_once_with(
            Path("exe"), {}, Path("folder"), Path("long-japanese.txt"))

    def test_gather_forwards_the_selection_to_measure(self):
        arguments = mock.Mock(values=None, executable=Path("build"), repetitions=5,
                              bench=speed.PALETTE_BENCH, grid_answer="yes")
        written = speed.selected_record(record({}), speed.PALETTE_BENCH)
        with mock.patch.object(speed, "measure", return_value=written) as measure, \
                mock.patch.object(speed, "write_record", return_value=Path("record.json")), \
                contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(written, speed.gather(arguments))
        measure.assert_called_once_with(Path("build"), 5, speed.PALETTE_BENCH, True)


class PartialRecordTests(unittest.TestCase):
    def test_an_old_six_bench_record_is_described_and_compared_without_fabricating_the_seventh(self):
        written = record({})
        del written["values"][speed.PALETTE_BENCH]
        del written["values"][speed.LONG_LINE_BENCH]
        self.assertNotIn(speed.PALETTE_BENCH, speed.describe(written))
        self.assertNotIn(speed.LONG_LINE_BENCH, speed.describe(written))
        self.assertIs(written, speed.selected_record(written, None))
        self.assertEqual(([], []), speed.compare(REFERENCE, written["values"], reference_values()))
        self.assertEqual(6, len(speed.compared_benches(written["values"], reference_values())))

    def test_a_selection_keeps_only_that_value_and_breakdown_without_mutating_the_source(self):
        written = record({})
        written["breakdown"] = {"startup-first-frame": {"origin": 1}, speed.LARGE_BURST_BENCH: {}}
        selected = speed.selected_record(written, "startup-first-frame")
        self.assertEqual({"startup-first-frame"}, set(selected["values"]))
        self.assertEqual({"startup-first-frame": {"origin": 1}}, selected["breakdown"])
        self.assertEqual(8, len(written["values"]))
        self.assertEqual(2, len(written["breakdown"]))

    def test_a_requested_value_absent_from_the_record_is_rejected_explicitly(self):
        written = record({})
        del written["values"][speed.PALETTE_BENCH]
        with self.assertRaisesRegex(SystemExit, "absent from this record"):
            speed.selected_record(written, speed.PALETTE_BENCH)

    def test_a_partial_record_cannot_replace_the_machine_references(self):
        path = mock.Mock()
        path.read_text.return_value = json.dumps({"machines": {"test": {"values": reference_values()}}})
        written = speed.selected_record(record({}), speed.PALETTE_BENCH)
        with self.assertRaisesRegex(SystemExit, "requires a complete record"):
            speed.adopt(path, written)
        path.write_text.assert_not_called()

    def test_adopting_only_the_palette_preserves_all_six_references_and_timestamp(self):
        before = reference_values()
        del before[speed.PALETTE_BENCH]
        path = mock.Mock()
        path.read_text.return_value = json.dumps({"machines": {"test": {"recordedAt": "old",
                                                                     "values": before}}})
        written = speed.selected_record(record({speed.PALETTE_BENCH: ([7.0] * 5, 0)}),
                                        speed.PALETTE_BENCH)
        with contextlib.redirect_stdout(io.StringIO()):
            speed.adopt_one(path, written, speed.PALETTE_BENCH)
        after = json.loads(path.write_text.call_args.args[0])["machines"]["test"]
        self.assertEqual("old", after["recordedAt"])
        self.assertEqual(7.0, after["values"].pop(speed.PALETTE_BENCH)["medianMs"])
        self.assertEqual(before, after["values"])

    def test_a_missing_trial_is_unmeasurable_even_without_a_reference(self):
        values = speed.selected_record(record({speed.PALETTE_BENCH: ([], 5)}),
                                       speed.PALETTE_BENCH)["values"]
        findings, unmeasurable = speed.compare(REFERENCE, values, {})
        self.assertEqual([], findings)
        self.assertEqual(1, len(unmeasurable))
        self.assertEqual(2, speed.exit_code(findings, unmeasurable))
        self.assertEqual((), speed.compared_benches(values, reference_values()))

    def test_a_valid_value_without_a_reference_is_recorded_only(self):
        values = speed.selected_record(record({}), speed.PALETTE_BENCH)["values"]
        self.assertEqual(([], []), speed.compare(REFERENCE, values, {}))
        self.assertEqual((), speed.compared_benches(values, {}))

    def test_check_without_machine_reference_still_reports_missing_and_returns_two(self):
        path = mock.Mock()
        path.read_text.return_value = json.dumps(dict(REFERENCE, machines={}))
        arguments = mock.Mock(reference=path)
        for missing, expected in ((False, 0), (True, 2)):
            with self.subTest(missing=missing):
                written = speed.selected_record(record({speed.PALETTE_BENCH: ([], 5)}
                                                       if missing else {}), speed.PALETTE_BENCH)
                output = io.StringIO()
                with mock.patch.object(speed, "gather", return_value=written), \
                        contextlib.redirect_stdout(output):
                    self.assertEqual(expected, speed.check(arguments))
                self.assertIn("no reference for test", output.getvalue())
                self.assertIn("0 benches checked", output.getvalue())

    def test_check_reports_a_valid_palette_without_a_reference_as_recorded_only(self):
        path = mock.Mock()
        references = reference_values()
        del references[speed.PALETTE_BENCH]
        path.read_text.return_value = json.dumps(dict(REFERENCE, machines={"test": {"values": references}}))
        arguments = mock.Mock(reference=path)
        written = speed.selected_record(record({}), speed.PALETTE_BENCH)
        output = io.StringIO()
        with mock.patch.object(speed, "gather", return_value=written), \
                contextlib.redirect_stdout(output):
            self.assertEqual(0, speed.check(arguments))
        self.assertIn(f"no reference for {speed.PALETTE_BENCH} on test; recorded only", output.getvalue())
        self.assertIn("0 benches checked", output.getvalue())


class LongLineDocumentTests(unittest.TestCase):
    """#298: the document is #291's fixture byte for byte; no window is opened here."""

    def test_the_constants_give_the_fixture_of_issue_291(self):
        line = (speed.LONG_LINE_SENTENCE * speed.LONG_LINE_REPEATS).encode("utf-8")
        self.assertEqual(2070, len(speed.LONG_LINE_SENTENCE * speed.LONG_LINE_REPEATS))
        self.assertEqual(6210, len(line))
        body = b"".join(f"{index:06d} ".encode("ascii") + line + b"\r\n"
                        for index in range(speed.LONG_LINE_LINES))
        self.assertEqual(6_219_000, len(body))
        self.assertFalse(body.startswith(b"\xef\xbb\xbf"))
        self.assertEqual(speed.LONG_LINE_SHA256, speed.hashlib.sha256(body).hexdigest())

    def test_a_document_of_another_digest_is_not_measured(self):
        folder = ROOT / "out/conformance/speed-long-line"
        folder.mkdir(parents=True, exist_ok=True)
        try:
            with mock.patch.object(speed, "LONG_LINE_LINES", 2), \
                    self.assertRaisesRegex(SystemExit, "is not measured"):
                speed.long_line_document(folder)
            written = (folder / "long-japanese.txt").read_bytes()
            line = (speed.LONG_LINE_SENTENCE * speed.LONG_LINE_REPEATS).encode("utf-8")
            self.assertEqual(b"000000 " + line + b"\r\n000001 " + line + b"\r\n", written)
        finally:
            (folder / "long-japanese.txt").unlink(missing_ok=True)
            folder.rmdir()


class LongLineTrialTests(unittest.TestCase):
    """bench_long_line against a scripted keys_trial; nothing is started (#298)."""

    def bench(self, trials: list) -> tuple[dict, dict, str, list]:
        outcomes = iter(trials)
        opened = []

        def scripted(executable, environment, folder, document):
            opened.append(document)
            outcome = next(outcomes)
            if isinstance(outcome, Exception):
                raise outcome
            return outcome

        written = io.StringIO()
        with mock.patch.object(speed, "keys_trial", scripted), \
                contextlib.redirect_stdout(written):
            values, parts = speed.bench_long_line(Path("exe"), {}, Path("folder"),
                                                  Path("long-japanese.txt"))
        return values, parts, written.getvalue(), opened

    def test_only_the_single_keystroke_of_the_shared_trial_is_the_value(self):
        values, parts, _, opened = self.bench([(7.3, 900.0, 300.0, speed.BURST_KEYS + 2)])
        self.assertEqual({speed.LONG_LINE_BENCH: 7.3}, values)
        self.assertEqual({}, parts)
        self.assertEqual([Path("long-japanese.txt")], opened)

    def test_a_failed_burst_does_not_repeat_or_drop_the_single(self):
        values, _, written, opened = self.bench([(7.3, None, 0.0, 7)])
        self.assertEqual({speed.LONG_LINE_BENCH: 7.3}, values)
        self.assertEqual(1, len(opened))
        self.assertNotIn("repeating", written)

    def test_an_unanswered_single_is_missing(self):
        values, _, _, _ = self.bench([(None, None, 0.0, 1)])
        self.assertEqual({speed.LONG_LINE_BENCH: None}, values)

    def test_an_unobserved_trial_is_repeated_and_then_missing(self):
        unobserved = speed.TrialNotObserved("the unsaved confirmation never came up")
        values, _, written, opened = self.bench([unobserved] * speed.BURST_ATTEMPTS)
        self.assertEqual({speed.LONG_LINE_BENCH: None}, values)
        self.assertEqual(speed.BURST_ATTEMPTS, len(opened))
        self.assertIn("repeating the trial (1/3)", written)
        self.assertIn(f"this trial of {speed.LONG_LINE_BENCH} is missing", written)

    def test_a_trial_after_an_unobserved_one_is_the_value(self):
        values, _, _, _ = self.bench([speed.TrialNotObserved("no finished measurement file"),
                                      (7.1, None, 0.0, 7)])
        self.assertEqual({speed.LONG_LINE_BENCH: 7.1}, values)

    def test_check_records_a_valid_long_line_without_a_reference_and_passes(self):
        path = mock.Mock()
        references = reference_values()
        del references[speed.LONG_LINE_BENCH]
        path.read_text.return_value = json.dumps(dict(REFERENCE,
                                                      machines={"test": {"values": references}}))
        written = speed.selected_record(record({}), speed.LONG_LINE_BENCH)
        output = io.StringIO()
        with mock.patch.object(speed, "gather", return_value=written), \
                contextlib.redirect_stdout(output):
            self.assertEqual(0, speed.check(mock.Mock(reference=path)))
        self.assertIn(f"no reference for {speed.LONG_LINE_BENCH} on test; recorded only",
                      output.getvalue())
        self.assertIn("0 benches checked", output.getvalue())

    def test_the_floor_decides_the_long_line_tolerance(self):
        """25 % of 7.27 ms is 1.82 ms, under the 2 ms floor: 9.2 passes and 9.3 is a regression."""
        recorded = {speed.LONG_LINE_BENCH: {"medianMs": 7.27}}
        for measured, regressions in ((9.2, 0), (9.3, 1)):
            with self.subTest(measured=measured):
                values = {speed.LONG_LINE_BENCH: {"medianMs": measured}}
                findings, _ = speed.compare(REFERENCE, values, recorded)
                self.assertEqual(regressions, len(findings))


class GiantLineTests(unittest.TestCase):
    """ADR 0138: the one-line 16 MiB document and its two optional benches."""

    def test_the_document_is_one_ascii_line_of_the_fixed_digest(self):
        data = speed.giant_line_text().encode("ascii")
        self.assertEqual(speed.GIANT_LINE_BYTES, len(data))
        self.assertNotIn(b"\n", data)
        self.assertNotIn(b"\r", data)
        self.assertEqual(speed.GIANT_LINE_SHA256, hashlib.sha256(data).hexdigest())
        self.assertIsInstance(json.loads(data), dict)

    def test_the_optional_benches_are_not_in_the_default_set(self):
        self.assertEqual(("open-giant-line-16mib", "key-to-frame-single-giant-line-16mib"),
                         speed.GIANT_BENCHES)
        self.assertEqual((*speed.GIANT_BENCHES, *speed.SPLIT_BENCHES, speed.PREVIEW_BENCH), speed.OPTIONAL_BENCHES)
        self.assertFalse(set(speed.OPTIONAL_BENCHES) & set(speed.BENCHES))
        self.assertIn(speed.GIANT_OPEN_BENCH, speed.BREAKDOWN_BENCHES)

    def test_compare_never_judges_an_optional_bench(self):
        values = {name: {"medianMs": 9000.0} for name in speed.OPTIONAL_BENCHES}
        recorded = {name: {"medianMs": 1.0} for name in speed.OPTIONAL_BENCHES}
        self.assertEqual(([], []), speed.compare(REFERENCE, values, recorded))

    def scripted_measurement(self, bench):
        boundaries = {name: mock.Mock() for name in
                      ("become_dpi_aware", "prepare", "giant_line_document", "large_document",
                       "stamp", "machine", "bench_giant_open", "bench_giant_single")}
        document = mock.Mock()
        document.name = "giant-line.txt"
        document.stat.return_value.st_size = speed.GIANT_LINE_BYTES
        boundaries["prepare"].return_value = Path("exe"), {}, Path("folder")
        boundaries["giant_line_document"].return_value = document
        boundaries["stamp"].return_value = "test"
        boundaries["machine"].return_value = record({})["machine"]
        boundaries["bench_giant_open"].return_value = (
            {speed.GIANT_OPEN_BENCH: 6000.0}, {speed.GIANT_OPEN_BENCH: {"origin": 10.0}})
        boundaries["bench_giant_single"].return_value = {speed.GIANT_SINGLE_BENCH: None}, {}
        with mock.patch.multiple(speed, **boundaries):
            written = speed.measure(Path("build"), 1, bench)
        return written, boundaries

    def test_each_optional_bench_runs_only_its_own_trial_over_the_one_line_document(self):
        for name, group in zip(speed.GIANT_BENCHES, ("bench_giant_open", "bench_giant_single")):
            with self.subTest(bench=name):
                written, calls = self.scripted_measurement(name)
                self.assertEqual({name}, set(written["values"]))
                for candidate in ("bench_giant_open", "bench_giant_single"):
                    self.assertEqual(int(candidate == group), calls[candidate].call_count)
                calls["large_document"].assert_not_called()
                self.assertEqual({"path": "giant-line.txt", "bytes": speed.GIANT_LINE_BYTES,
                                  "lines": 1}, written["document"])
                self.assertEqual(set(speed.BREAKDOWN_BENCHES).intersection({name}),
                                 set(written["breakdown"]))

    def test_a_single_without_an_answer_is_recorded_missing(self):
        written, _ = self.scripted_measurement(speed.GIANT_SINGLE_BENCH)
        self.assertEqual(1, written["values"][speed.GIANT_SINGLE_BENCH]["missing"])
        self.assertIsNone(written["values"][speed.GIANT_SINGLE_BENCH]["medianMs"])

    def test_a_timed_out_single_trial_is_missing_and_not_repeated(self):
        error = speed.subprocess.TimeoutExpired("NeNeNib.exe", speed.LARGE_SECONDS)
        with mock.patch.object(speed, "keys_trial", side_effect=error) as trial, \
                contextlib.redirect_stdout(io.StringIO()):
            values, parts = speed.bench_giant_single(Path("exe"), {}, Path("folder"), Path("g"),
                                                     True)
        self.assertEqual(({speed.GIANT_SINGLE_BENCH: None}, {}), (values, parts))
        trial.assert_called_once_with(Path("exe"), {}, Path("folder"), Path("g"), burst=False,
                                      grid=True)

    def quiet_trial(self, entries):
        process = mock.Mock()
        boundaries = {name: mock.Mock() for name in
                      ("start", "report_path", "answer_question", "raise_window",
                       "take_foreground", "covered_by", "await_quiet", "write_text", "close",
                       "dismiss_dialog", "stop", "observed_marks", "time")}
        boundaries["start"].return_value = process, 123, []
        boundaries["answer_question"].return_value = True
        boundaries["take_foreground"].return_value = True
        boundaries["covered_by"].return_value = None
        boundaries["await_quiet"].return_value = True
        boundaries["dismiss_dialog"].return_value = True
        boundaries["observed_marks"].return_value = entries
        with mock.patch.multiple(speed, **boundaries), contextlib.redirect_stdout(io.StringIO()):
            result = speed.keys_trial(Path("exe"), {}, Path("folder"), Path("g"), burst=False)
        boundaries["stop"].assert_called_once_with(process)
        boundaries["answer_question"].assert_called_once_with(process, True,
                                                              seconds=speed.LARGE_SECONDS)
        self.assertEqual(["w", "a"], [call.args[1] for call in
                                      boundaries["write_text"].call_args_list])
        self.assertEqual(3, boundaries["await_quiet"].call_count)
        return result

    def test_the_trial_without_the_200_measures_the_single_after_the_warmup_frame(self):
        self.assertEqual((0.5, None, 0.0, 2), self.quiet_trial(marks(2)))

    def test_a_single_read_before_the_warmup_frame_is_missing(self):
        entries = [{"milestone": "input_received", "qpcMicroseconds": 1000},
                   {"milestone": "input_received", "qpcMicroseconds": 1200},
                   {"milestone": "frame_presented", "qpcMicroseconds": 9000}]
        self.assertEqual((None, None, 0.0, 2), self.quiet_trial(entries))


def giant_report(body: bool, wait_ms: int | None = None) -> str:
    """A measurement file of the giant line: the startup marks, a frame per answer, the body last.

    With wait_ms the question's two marks (#689) surround that wait after the first frame.
    """
    entries = [{"milestone": name, "qpcMicroseconds": 1000 * (index + 1)}
               for index, name in enumerate(speed.STARTUP_MILESTONES)
               if name != "body_presented"]
    entries.append({"milestone": "frame_presented", "qpcMicroseconds": 150000})
    if wait_ms is not None:
        entries.append({"milestone": "question_shown", "qpcMicroseconds": 150500})
        entries.append({"milestone": "question_answered",
                        "qpcMicroseconds": 150500 + 1000 * wait_ms})
    if body:
        entries.append({"milestone": "body_presented", "qpcMicroseconds": 200000})
    return json.dumps({"processCreationToFirstFrameMs": 30.0, "processCreationToOriginMs": 10.0,
                       "qpcFrequency": 10000000, "marks": entries})


class GiantOpenTrialTests(unittest.TestCase):
    """#546: open-giant-line-16mib ends at body_presented, after the answer; never a real window."""

    def trial(self, text):
        process = mock.Mock()
        boundaries = {name: mock.Mock() for name in
                      ("start", "report_path", "answer_question", "await_quiet", "close", "stop")}
        order = mock.Mock()
        for name in ("answer_question", "await_quiet", "close"):
            order.attach_mock(boundaries[name], name)
        report = mock.Mock()
        report.is_file.return_value = True
        report.read_text.return_value = text
        boundaries["start"].return_value = process, 123, []
        boundaries["report_path"].return_value = report
        boundaries["answer_question"].return_value = True
        boundaries["await_quiet"].return_value = True
        with mock.patch.multiple(speed, **boundaries), contextlib.redirect_stdout(io.StringIO()):
            values, parts = speed.bench_giant_open(Path("exe"), {}, Path("folder"), Path("g"),
                                                   False)
        boundaries["stop"].assert_called_once_with(process)
        self.assertEqual([mock.call.answer_question(process, False, seconds=speed.LARGE_SECONDS),
                          mock.call.await_quiet(123),
                          mock.call.close(123)], order.mock_calls)
        return values[speed.GIANT_OPEN_BENCH], parts

    def test_the_value_is_process_creation_to_body_presented(self):
        value, parts = self.trial(giant_report(body=True))
        self.assertEqual(210.0, value)
        segments = parts[speed.GIANT_OPEN_BENCH]
        self.assertEqual(190.0, segments["body_presented"])  # 最初の frame (10 ms) から本文まで
        self.assertEqual(list((speed.ORIGIN_SEGMENT, *speed.STARTUP_MILESTONES)), list(segments))

    def test_a_run_without_body_presented_is_missing(self):
        value, parts = self.trial(giant_report(body=False))
        self.assertIsNone(value)
        self.assertEqual({}, parts)

    def test_the_wait_for_the_answer_is_taken_out_of_the_value(self):
        """#689: body_presented minus question_shown -> question_answered (the "no" answer too)."""
        value, parts = self.trial(giant_report(body=True, wait_ms=60))
        self.assertEqual(150.0, value)
        segments = parts[speed.GIANT_OPEN_BENCH]
        self.assertEqual(60.0, segments[speed.QUESTION_WAIT_SEGMENT])
        self.assertEqual(190.0, segments["body_presented"])  # 内訳の区間は節目のまま

    def test_an_exe_without_the_question_marks_keeps_body_presented(self):
        """#689: an exe built before the two marks is not missing; nothing is taken out."""
        value, parts = self.trial(giant_report(body=True))
        self.assertEqual(210.0, value)
        self.assertNotIn(speed.QUESTION_WAIT_SEGMENT, parts[speed.GIANT_OPEN_BENCH])

    def test_one_mark_alone_takes_nothing_out(self):
        report = json.loads(giant_report(body=True, wait_ms=60))
        report["marks"] = [entry for entry in report["marks"]
                           if entry["milestone"] != "question_answered"]
        value, parts = self.trial(json.dumps(report))
        self.assertEqual(210.0, value)
        self.assertNotIn(speed.QUESTION_WAIT_SEGMENT, parts[speed.GIANT_OPEN_BENCH])


class GiantOpenRecordTests(unittest.TestCase):
    """#689: the record keeps each valid trial's wait, null for an exe without the marks."""

    def test_the_record_keeps_the_wait_per_trial_and_describes_its_median(self):
        startup = {speed.ORIGIN_SEGMENT: 10.0, **{name: 1.0 for name in speed.STARTUP_MILESTONES}}
        trials = iter([
            ({speed.GIANT_OPEN_BENCH: 150.0},
             {speed.GIANT_OPEN_BENCH: {**startup, speed.QUESTION_WAIT_SEGMENT: 60.0}}),
            ({speed.GIANT_OPEN_BENCH: None}, {}),
            ({speed.GIANT_OPEN_BENCH: 210.0}, {speed.GIANT_OPEN_BENCH: dict(startup)}),
            ({speed.GIANT_OPEN_BENCH: 140.0},
             {speed.GIANT_OPEN_BENCH: {**startup, speed.QUESTION_WAIT_SEGMENT: 40.0}})])
        boundaries = {name: mock.Mock() for name in
                      ("become_dpi_aware", "prepare", "giant_line_document", "stamp", "machine")}
        boundaries["prepare"].return_value = Path("exe"), {}, Path("folder")
        giant = mock.Mock()
        giant.name = "giant-line.txt"
        giant.stat.return_value.st_size = 16_800_000
        boundaries["giant_line_document"].return_value = giant
        boundaries["stamp"].return_value = "test"
        boundaries["machine"].return_value = record({})["machine"]
        boundaries["run_benches"] = mock.Mock(side_effect=lambda *args: [next(trials)])
        with mock.patch.multiple(speed, **boundaries):
            written = speed.measure(Path("build"), 4, speed.GIANT_OPEN_BENCH)
        value = written["values"][speed.GIANT_OPEN_BENCH]
        self.assertEqual([150.0, 210.0, 140.0], value["samples"])
        self.assertEqual(1, value["missing"])
        self.assertEqual([60.0, None, 40.0], value["questionWaitMs"])
        wait = written["breakdown"][speed.GIANT_OPEN_BENCH][speed.QUESTION_WAIT_SEGMENT]
        self.assertEqual(50.0, wait["medianMs"])
        self.assertIn(f"{speed.GIANT_OPEN_BENCH} question wait (median ms, not in the value): 50.0",
                      speed.describe(written))


class MilestoneOrderTests(unittest.TestCase):
    def test_the_startup_milestones_are_core_milestone_in_its_order(self):
        """STARTUP_MILESTONES is core::Milestone from document_opened to body_presented (#546)."""
        text = (ROOT / "src/core/Milestone.hpp").read_text(encoding="utf-8")
        body = text[text.index("enum class Milestone"):]
        body = body[body.index("{") + 1:body.index("}")]
        names = [line.strip().rstrip(",") for line in body.splitlines() if line.strip()]
        startup = [name for name in names[:names.index("body_presented") + 1]
                   if name != "input_received"]
        self.assertEqual(tuple(startup), speed.STARTUP_MILESTONES)
        self.assertEqual("body_presented", speed.STARTUP_MILESTONES[-1])


SPLIT_WORK = ROOT / "out/conformance/split-verdict"


def paced_mark(milestone: str, microseconds: int) -> dict:
    return {"milestone": milestone, "qpcMicroseconds": microseconds}


def paced_marks(latencies_ms: list[float], setup_inputs: int = 0) -> list:
    """Setup inputs, the warmup and one keystroke per latency, each answered before the next."""
    marks, now = [], 1_000
    for _ in range(setup_inputs):
        marks.append(paced_mark("input_received", now))
        now += 100_000
    for latency in [5.0, *latencies_ms]:
        marks.append(paced_mark("input_received", now))
        now += int(latency * 1000)
        marks.append(paced_mark("frame_presented", now))
        now += 500_000
    return marks


def split_trial(median: float, p95: float, over: int = 0) -> dict:
    return {"count": 100, "medianMs": median, "p95Ms": p95, "medianRawMs": median, "p95RawMs": p95,
            "maximumMs": p95, "minimumMs": 0.5, "over_16_7": over, "samples": []}


def split_record(bench: str, trials: list, sha: str = "e" * 64, window=(1280, 720),
           document: str = "empty") -> dict:
    return {"bench": bench, "executableSha256": sha, "window": list(window),
            "document": {"choice": document, "path": None, "bytes": 0, "sha256": None},
            "layout": speed.SPLIT_PANE_BENCHES.get(bench, ("none", False))[0],
            "otherDocument": speed.SPLIT_PANE_BENCHES.get(bench, ("none", False))[1],
            "panesRequested": bench != speed.SPLIT_SINGLE_BENCH,
            "panesDelivered": bench != speed.SPLIT_SINGLE_BENCH, "splitKeys": [],
            "trials": trials,
            "missingReasons": ["not observed"] * sum(1 for trial in trials if trial is None)}


PANE = "split-pane-key-100-same-vertical"


class PacedSummaryTests(unittest.TestCase):
    def test_median_is_the_mean_of_the_50th_and_51st_and_p95_the_95th(self):
        values = [float(value) for value in range(1, 101)]
        random.Random(681).shuffle(values)
        summary = speed.summarise_paced(values)
        self.assertEqual(summary["count"], 100)
        self.assertEqual(summary["medianMs"], 50.5)
        self.assertEqual(summary["p95Ms"], 95.0)
        self.assertEqual(summary["maximumMs"], 100.0)
        self.assertEqual(summary["minimumMs"], 1.0)
        self.assertEqual(summary["over_16_7"], 84)
        self.assertEqual(summary["samples"], values)

    def test_over_budget_counts_strictly_above_16_7(self):
        values = [16.7] * 50 + [16.71] * 50
        self.assertEqual(speed.summarise_paced(values)["over_16_7"], 50)

    def test_anything_but_100_values_is_missing(self):
        for values in (None, [], [1.0] * 99, [1.0] * 101):
            with self.subTest(count=None if values is None else len(values)):
                self.assertIsNone(speed.summarise_paced(values))


class PacedValuesTests(unittest.TestCase):
    def test_the_last_101_inputs_are_the_warmup_and_the_100_keystrokes(self):
        latencies = [float(index % 7 + 1) for index in range(100)]
        for setup in (0, 6):
            with self.subTest(setup=setup):
                values = speed.paced_values(paced_marks(latencies, setup))
                self.assertEqual(values, latencies)

    def test_99_keystrokes_are_missing(self):
        self.assertIsNone(speed.paced_values(paced_marks([2.0] * 99)[2:]))

    def test_a_keystroke_read_before_the_previous_frame_makes_the_trial_missing(self):
        marks = paced_marks([2.0] * 100)
        # 50 番目の打鍵を、その前の打鍵の frame より前に読んだことにする。
        # 印の並びは時刻の順のままにする（打鍵 49 → 打鍵 50 → 打鍵 49 の frame）。
        frame, key = marks[2 * 50 - 1], marks[2 * 50]
        marks[2 * 50 - 1] = paced_mark("input_received", frame["qpcMicroseconds"])
        marks[2 * 50] = paced_mark("frame_presented", key["qpcMicroseconds"])
        self.assertIsNone(speed.paced_values(marks))

    def test_an_unanswered_last_keystroke_makes_the_trial_missing(self):
        self.assertIsNone(speed.paced_values(paced_marks([2.0] * 100)[:-1]))

    def test_the_input_count_must_be_the_split_inputs_and_101(self):
        latencies = [2.0] * 100
        self.assertEqual(speed.paced_outcome(paced_marks(latencies, 3), 104), latencies)
        for setup, expected in ((2, 104), (4, 104), (0, 104)):
            with self.subTest(setup=setup), self.assertRaises(speed.TrialNotObserved) as caught:
                speed.paced_outcome(paced_marks(latencies, setup), expected)
            self.assertEqual(str(caught.exception),
                             f"input count mismatch: expected {expected}, observed {setup + 101}")

    def test_a_count_that_matches_but_answers_out_of_order_is_missing(self):
        with self.assertRaises(speed.TrialNotObserved):
            speed.paced_outcome(paced_marks([2.0] * 100)[:-1], 101)

    def test_a_split_step_counts_its_key_downs_and_characters(self):
        self.assertEqual(speed.step_inputs([speed.VK_CONTROL], 1), 3)
        self.assertEqual(speed.step_inputs([speed.VK_CONTROL, speed.VK_SHIFT], 0), 3)
        self.assertEqual(speed.step_inputs([], 0), 1)


class SplitKeysTests(unittest.TestCase):
    def test_vertical_and_horizontal_and_the_other_document(self):
        self.assertEqual(speed.split_keys("vertical", False),
                         [("Ctrl+\\", [speed.VK_CONTROL], 0xDC)])
        self.assertEqual(speed.split_keys("horizontal", True),
                         [("Ctrl+Shift+\\", [speed.VK_CONTROL, speed.VK_SHIFT], 0xDC),
                          ("F6", [], 0x75), ("Ctrl+T", [speed.VK_CONTROL], 0x54),
                          ("F6", [], 0x75)])

    def test_the_five_benches_are_optional_and_never_compared(self):
        self.assertEqual(len(speed.SPLIT_BENCHES), 5)
        for name in speed.SPLIT_BENCHES:
            with self.subTest(name=name):
                self.assertIn(name, speed.OPTIONAL_BENCHES)
                self.assertNotIn(name, speed.BENCHES)
        self.assertEqual(speed.GIANT_BENCHES, (speed.GIANT_OPEN_BENCH, speed.GIANT_SINGLE_BENCH))


class SplitVerdictTests(unittest.TestCase):
    def verdict(self, single: dict, pane: dict) -> str:
        return speed.split_verdict(single, pane)["verdict"]

    def pair(self, single_median: float, pane_median: float, pane_p95: float) -> str:
        return self.verdict(split_record(speed.SPLIT_SINGLE_BENCH, [split_trial(single_median, 9.0)]),
                            split_record(PANE, [split_trial(pane_median, pane_p95)]))

    def test_25_percent_of_s_both_sides(self):
        self.assertEqual(self.pair(8.0, 10.0, 12.0), "pass")
        self.assertEqual(self.pair(8.0, 10.01, 12.0), "fail")

    def test_the_1_ms_floor_both_sides(self):
        self.assertEqual(self.pair(2.0, 3.0, 12.0), "pass")
        self.assertEqual(self.pair(2.0, 3.01, 12.0), "fail")

    def test_the_verdict_reads_the_unrounded_values_at_the_rounding_boundary(self):
        # 差が allowed（2.0）± 0.00005。4 桁に丸めた中央値はどちらも 10.0 で、生の値だけが向きを決める。
        single = speed.summarise_paced([8.0] * 100)
        for pane_value, verdict in ((10.00005, "fail"), (9.99995, "pass")):
            pane = speed.summarise_paced([pane_value] * 100)
            with self.subTest(pane=pane_value):
                self.assertEqual(pane["medianMs"], 10.0)
                self.assertEqual(self.verdict(split_record(speed.SPLIT_SINGLE_BENCH, [single]),
                                              split_record(PANE, [pane])), verdict)

    def test_p95_at_16_7_both_sides(self):
        self.assertEqual(self.pair(8.0, 8.5, 16.7), "pass")
        self.assertEqual(self.pair(8.0, 8.5, 16.71), "fail")

    def test_the_verdict_carries_both_medians_p95_difference_and_allowance(self):
        single = split_record(speed.SPLIT_SINGLE_BENCH, [split_trial(8.0, 9.0, 1)])
        result = speed.split_verdict(single, split_record(PANE, [split_trial(9.5, 15.0, 3)]))
        self.assertEqual(result["trials"], [{
            "verdict": "pass", "singleMedianMs": 8.0, "paneMedianMs": 9.5, "singleP95Ms": 9.0,
            "paneP95Ms": 15.0, "differenceMs": 1.5, "allowedMs": 2.0, "singleOver16_7": 1,
            "paneOver16_7": 3}])
        self.assertEqual(result["executableSha256"], {"single": "e" * 64, "pane": "e" * 64})

    def test_each_trial_is_judged_on_its_own(self):
        single = split_record(speed.SPLIT_SINGLE_BENCH, [split_trial(8.0, 9.0)] * 3)
        pane = split_record(PANE, [split_trial(9.0, 12.0), split_trial(11.0, 12.0), split_trial(9.0, 12.0)])
        result = speed.split_verdict(single, pane)
        self.assertEqual([entry["verdict"] for entry in result["trials"]], ["pass", "fail", "pass"])
        self.assertEqual(result["verdict"], "fail")

    def test_the_verdict_names_layout_other_document_and_both_documents(self):
        other = "split-pane-key-100-other-horizontal"
        result = speed.split_verdict(split_record(speed.SPLIT_SINGLE_BENCH, [split_trial(8.0, 9.0)]),
                                     split_record(other, [split_trial(8.5, 9.0)]))
        self.assertEqual((result["layout"], result["otherDocument"]), ("horizontal", True))
        self.assertEqual(result["document"]["single"], result["document"]["pane"])
        self.assertTrue(result["panesDelivered"])

    def test_every_trial_missing_is_unmeasurable_before_incomparable(self):
        single = split_record(speed.SPLIT_SINGLE_BENCH, [split_trial(8.0, 9.0)])
        pane = dict(split_record(PANE, [None]), window=None,
                    missingReasons=[speed.SPLIT_KEYS_NOT_DELIVERED])
        result = speed.split_verdict(single, pane)
        self.assertEqual(result["verdict"], "unmeasurable")
        self.assertEqual(result["reasons"], [speed.SPLIT_KEYS_NOT_DELIVERED])

    def test_a_missing_trial_is_unmeasurable(self):
        single = split_record(speed.SPLIT_SINGLE_BENCH, [split_trial(8.0, 9.0), split_trial(8.0, 9.0)])
        pane = split_record(PANE, [split_trial(20.0, 30.0), None])
        self.assertEqual(self.verdict(single, pane), "unmeasurable")

    def test_incomparable_records(self):
        single = split_record(speed.SPLIT_SINGLE_BENCH, [split_trial(8.0, 9.0)])
        cases = {"exe": split_record(PANE, [split_trial(8.0, 9.0)], sha="f" * 64),
                 "window": split_record(PANE, [split_trial(8.0, 9.0)], window=(1280, 721)),
                 "document": split_record(PANE, [split_trial(8.0, 9.0)], document="long-line"),
                 "trials": split_record(PANE, [split_trial(8.0, 9.0)] * 2),
                 "single as pane": split_record(speed.SPLIT_SINGLE_BENCH, [split_trial(8.0, 9.0)])}
        for name, pane in cases.items():
            with self.subTest(name=name):
                self.assertEqual(self.verdict(single, pane), "incomparable")
        self.assertEqual(self.verdict(split_record(PANE, [split_trial(8.0, 9.0)]), single), "incomparable")
        unknown = dict(single, window=None)
        self.assertEqual(self.verdict(unknown, dict(split_record(PANE, [split_trial(8.0, 9.0)]), window=None)),
                         "incomparable")

    def test_the_command_prints_one_json_and_exits_0_for_pass_only(self):
        SPLIT_WORK.mkdir(parents=True, exist_ok=True)
        try:
            single, pane = SPLIT_WORK / "s.json", SPLIT_WORK / "p.json"
            single.write_text(json.dumps(split_record(speed.SPLIT_SINGLE_BENCH, [split_trial(8.0, 9.0)])),
                              encoding="utf-8")
            for pane_median, code in ((9.0, 0), (11.0, 1)):
                pane.write_text(json.dumps(split_record(PANE, [split_trial(pane_median, 12.0)])),
                                encoding="utf-8")
                printed = io.StringIO()
                with self.subTest(pane=pane_median), contextlib.redirect_stdout(printed):
                    self.assertEqual(speed.split_verdict_command(single, pane), code)
                self.assertIn(json.loads(printed.getvalue())["verdict"], ("pass", "fail"))
        finally:
            shutil.rmtree(SPLIT_WORK, ignore_errors=True)


if __name__ == "__main__":
    unittest.main()
