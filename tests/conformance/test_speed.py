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

    def test_the_table_names_the_seven_benches(self):
        self.assertEqual(("startup-first-frame", "startup-window-shown", "key-to-frame-single",
                          "key-to-frame-burst-200", "open-large-file-16mib",
                          "key-to-frame-burst-200-16mib", "key-to-frame-palette-5000"), speed.BENCHES)

    def test_the_reference_describes_every_bench_of_the_table(self):
        """eng/prove-gates.py builds its QLT-014 proof from these descriptions (ADR 0044 decision 6)."""
        reference = json.loads((ROOT / "eng/perf-reference.json").read_text(encoding="utf-8"))
        self.assertEqual(list(speed.BENCHES), list(reference["benches"]))
        self.assertIn("ADR 0044", reference["benches"]["key-to-frame-burst-200-16mib"])

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
                       "stamp", "machine", "bench_startup", "bench_keys", "bench_large_file",
                       "bench_palette")}
        boundaries["prepare"].return_value = Path("exe"), {}, Path("folder")
        boundaries["large_document"].return_value = document
        boundaries["palette_document"].return_value = Path("f0000.txt")
        boundaries["stamp"].return_value = "test"
        boundaries["machine"].return_value = record({})["machine"]
        boundaries["bench_startup"].return_value = {
            "startup-first-frame": 1.0, "startup-window-shown": 2.0}, {}
        boundaries["bench_keys"].side_effect = lambda *args: (
            ({"key-to-frame-single": 3.0, speed.EMPTY_BURST_BENCH: 4.0}, {}) if len(args) == 3
            else ({speed.LARGE_BURST_BENCH: 6.0}, {}))
        boundaries["bench_large_file"].return_value = {"open-large-file-16mib": 5.0}, {}
        boundaries["bench_palette"].return_value = {speed.PALETTE_BENCH: 7.0}, {}
        with mock.patch.multiple(speed, **boundaries):
            written = speed.measure(Path("build"), 1, bench)
        return written, boundaries

    def test_each_selection_runs_only_its_shared_stimulus_and_records_only_that_value(self):
        groups = ("bench_startup", "bench_startup", "bench_keys", "bench_keys",
                  "bench_large_file", "bench_keys", "bench_palette")
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
                self.assertEqual(set(speed.BREAKDOWN_BENCHES).intersection({name}),
                                 set(written["breakdown"]))

    def test_no_selection_runs_all_trials_and_records_seven_values(self):
        written, calls = self.scripted_measurement(None)
        self.assertEqual(list(speed.BENCHES), list(written["values"]))
        self.assertEqual(2, calls["bench_keys"].call_count)
        for name in ("bench_startup", "bench_large_file", "bench_palette"):
            calls[name].assert_called_once()

    def test_gather_forwards_the_selection_to_measure(self):
        arguments = mock.Mock(values=None, executable=Path("build"), repetitions=5,
                              bench=speed.PALETTE_BENCH)
        written = speed.selected_record(record({}), speed.PALETTE_BENCH)
        with mock.patch.object(speed, "measure", return_value=written) as measure, \
                mock.patch.object(speed, "write_record", return_value=Path("record.json")), \
                contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(written, speed.gather(arguments))
        measure.assert_called_once_with(Path("build"), 5, speed.PALETTE_BENCH)


class PartialRecordTests(unittest.TestCase):
    def test_an_old_six_bench_record_is_described_and_compared_without_fabricating_the_seventh(self):
        written = record({})
        del written["values"][speed.PALETTE_BENCH]
        self.assertNotIn(speed.PALETTE_BENCH, speed.describe(written))
        self.assertIs(written, speed.selected_record(written, None))
        self.assertEqual(([], []), speed.compare(REFERENCE, written["values"], reference_values()))
        self.assertEqual(6, len(speed.compared_benches(written["values"], reference_values())))

    def test_a_selection_keeps_only_that_value_and_breakdown_without_mutating_the_source(self):
        written = record({})
        written["breakdown"] = {"startup-first-frame": {"origin": 1}, speed.LARGE_BURST_BENCH: {}}
        selected = speed.selected_record(written, "startup-first-frame")
        self.assertEqual({"startup-first-frame"}, set(selected["values"]))
        self.assertEqual({"startup-first-frame": {"origin": 1}}, selected["breakdown"])
        self.assertEqual(7, len(written["values"]))
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


if __name__ == "__main__":
    unittest.main()
