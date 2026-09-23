"""Measure the six speed benches and compare them with this machine's reference (QLT-014).

ADR 0011: the editor marks milestones (Issue #19 added the eight startup stages to the original
input_received / frame_presented), the Win32 timing adapter turns them into a measurement file when
the process is started with --measure <out.json>, and this script starts the editor, drives it
through eng/window_driver.py, and reads the file back.

  startup-first-frame       process creation -> first frame_presented
  startup-window-shown      process creation -> first window_shown (ADR 0013: the window is shown
                            before the device is created, so this is what "startup" feels like)
  key-to-frame-single       one WM_CHAR -> the next frame_presented
  key-to-frame-burst-200    200 WM_CHAR posted at once -> the frame that finishes them
  open-large-file-16mib     the same startup measurement with a 16 MiB, 200,000 line argument
  key-to-frame-burst-200-16mib
                            the same 200 WM_CHAR after that 16 MiB document is open -> the frame
                            that finishes them (ADR 0044 decision 6: the burst over an empty
                            document never sees a large piece table or a long add buffer)

--adopt writes the medians of every bench into this machine's reference; --adopt --bench <name>
writes that one key and leaves the other reference values and recordedAt untouched, which is how a
new bench joins a machine that already has references (ADR 0013 decision 3).

--record also writes and prints the startup breakdown of the two startup benches: the segment that
ends at each startup milestone, plus the "origin" segment (process creation -> the timing adapter's
bind, which the loader, the CRT, CoInitializeEx and the arguments live in). The breakdown is
recorded and shown only; BENCHES and the reference values are untouched (Issue #19).

The exe under measurement is the Release one (build-release/NeNeNib.exe). The Debug exe carries
ASan and UBSan (eng/targets.cmake instruments the Debug configuration only), and an instrumented
run is not the speed of the product that ADR 0006 gates. eng/verify-window.py keeps driving the
Debug exe, because running the sanitizers against the real window is the point there.

A trial whose stimulus did not reach the window as specified is not a value: the 200 keystrokes
are repeated BURST_ATTEMPTS times and, when none of them arrives complete and together, that trial
of key-to-frame-burst-200 is recorded as missing instead (Issue #30). A trial that could not be
observed at all -- the unsaved confirmation never came up, or the editor left no finished
measurement file -- is the same kind of thing and joins the same repetition through
TrialNotObserved, instead of ending the run with a traceback the gate reads as a regression
(Issue #36). "values" carries the count in "missing", the median comes from the valid trials alone,
and a bench with fewer than MIN_VALID_SAMPLES of them is not judged at all. --check then leaves
with 1 for a regression and with 2 for a bench that could not be measured, which are different
things and say so in different words; eng/check.ps1 fails on both.

Every bench runs five times; the value is the median and the spread is recorded with it. References
live in eng/perf-reference.json per machine fingerprint (CPU name, display adapter, system DPI),
because the same numbers on another machine mean nothing (ADR 0006); the shared CI runners are
several hosts whose CPU generations differ by more than the tolerance, so they get one entry per
fingerprint as well (ADR 0016). A machine without a reference records its numbers and passes, and
says in one line which host was not judged rather than passing in silence; so does a machine that
cannot put a window on a desktop. Widening a reference is an ADR decision, not a repair.

Python standard library and ctypes only.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import statistics
import time
import winreg

from window_driver import (become_dpi_aware, close, covered_by, dismiss_dialog, IDNO,
                           post_together, raise_window, start, stop, take_foreground, user,
                           WindowUnavailable, write_text)

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "eng/perf-reference.json"
# 製品の速さを測るので、サニタイザの掛かっていない Release の exe を使う（ADR 0006 / 0011）。
RELEASE_EXECUTABLE = "build-release/NeNeNib.exe"
OUTPUT = ROOT / "out/speed"
REPETITIONS = 5
SETTLE_SECONDS = 0.6
SINGLE_KEY_SECONDS = 0.3
WARMUP_SECONDS = 0.5
BURST_KEYS = 200
BURST_SECONDS = 4.0
# 200 打鍵が「まとめて」届いたと言える幅。これを越えた試行は刺激が仕様どおりでないので測り直す。
# 空の文書の burst だけに当てる。16 MiB の本文では到着の幅が鍵の処理時間そのものになる（#179）。
BURST_SPAN_LIMIT_MS = 50.0
BURST_ATTEMPTS = 3
# 5 試行のうちこれだけ有効な値が無いベンチは「遅くなった」ではなく「測れなかった」（Issue #30）。
MIN_VALID_SAMPLES = 3
EXIT_SECONDS = 15.0
LARGE_LINES = 200_000
LARGE_LINE_BYTES = 84
LARGE_SECONDS = 60.0
MICROSECONDS_PER_MILLISECOND = 1000.0
DISPLAY_ADAPTERS = r"SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\0000"
CENTRAL_PROCESSOR = r"HARDWARE\DESCRIPTION\System\CentralProcessor\0"
BENCHES = ("startup-first-frame", "startup-window-shown", "key-to-frame-single",
           "key-to-frame-burst-200", "open-large-file-16mib", "key-to-frame-burst-200-16mib")
# 打鍵のベンチは 1 本の試行の経路を共有し、開く本文だけが違う（ADR 0044 決定 6）。
# 16 MiB の試行の 1 打鍵は暖機と同じ扱いで、値にするのは 200 打鍵だけ。
EMPTY_BURST_BENCH = "key-to-frame-burst-200"
LARGE_BURST_BENCH = "key-to-frame-burst-200-16mib"
# 16 MiB の試行の到着の幅は欠測の理由にしない代わりに、内訳と同じ形で記録に残す（#179）。
ARRIVAL_SEGMENT = "keysArrivalSpan"
# 起動の節目の正典順（Issue #19 / core::Milestone と同じ綴り）。区間名は「到達する節目の名前」で、
# 最初の区間 "origin" だけはプロセス生成 -> bind（loader・CRT・COM・引数）を指す。
ORIGIN_SEGMENT = "origin"
STARTUP_MILESTONES = ("document_opened", "window_created", "backdrop_applied", "window_shown",
                      "device_created", "swap_chain_created", "composition_bound",
                      "context_created", "text_formats_created", "frame_presented")
# 内訳を出すのは起動のベンチだけ。打鍵のベンチは起動の節目を測る刺激ではない。
STARTUP_BENCHES = ("startup-first-frame", "open-large-file-16mib")
# 記録の breakdown には 16 MiB の打鍵の到着の幅（ARRIVAL_SEGMENT）も同じ形で載せる（#179）。
BREAKDOWN_BENCHES = (*STARTUP_BENCHES, LARGE_BURST_BENCH)


class TrialNotObserved(RuntimeError):
    """One trial of the keystroke bench was not observed; the caller repeats it or records missing.

    WindowUnavailable means this machine could not put a window on a desktop, which the gate records
    and passes (ADR 0011 decision 4). A trial whose unsaved confirmation never came up, or whose
    measurement file is absent or unfinished, says nothing about the machine, so it never borrows
    that meaning: it is one trial that delivered no observation (Issue #36).
    """


def registry_text(key: str, name: str, fallback: str) -> str:
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, key) as handle:
            value, _ = winreg.QueryValueEx(handle, name)
    except OSError:
        return fallback
    return str(value).strip()


def machine() -> dict:
    """CPU name, display adapter and system DPI; the exe never reports its own environment."""
    cpu = registry_text(CENTRAL_PROCESSOR, "ProcessorNameString", "unknown processor")
    gpu = registry_text(DISPLAY_ADAPTERS, "DriverDesc", "unknown display adapter")
    dpi = int(user.GetDpiForSystem())
    digest = hashlib.sha256("|".join([cpu, gpu, str(dpi)]).encode("utf-8")).hexdigest()[:16]
    return {"fingerprint": digest, "cpu": cpu, "gpu": gpu, "dpi": dpi}


def stamp() -> str:
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H-%M-%SZ")


def prepare(build: Path) -> tuple[Path, dict, Path]:
    """A private copy of the exe, an isolated profile, and the folder the reports go into."""
    OUTPUT.mkdir(parents=True, exist_ok=True)
    executable = OUTPUT / "NeNeNib.exe"
    shutil.copy2(build, executable)
    profile = OUTPUT / "profile"
    profile.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ, LOCALAPPDATA=str(profile), APPDATA=str(profile))
    return executable, environment, OUTPUT


def large_document(folder: Path) -> Path:
    """200,000 CRLF lines of fixed ASCII, rebuilt only when it is missing or the wrong size."""
    path = folder / "large.txt"
    expected = LARGE_LINES * LARGE_LINE_BYTES
    if path.is_file() and path.stat().st_size == expected:
        return path
    filler = "nenenib speed sample line for the open-large-file bench "
    with path.open("w", encoding="utf-8", newline="") as handle:
        for index in range(LARGE_LINES):
            handle.write((f"{index:06d} " + filler + filler)[:LARGE_LINE_BYTES - 2] + "\r\n")
    return path


def report_path(folder: Path, name: str) -> Path:
    path = folder / f"marks-{name}.json"
    path.unlink(missing_ok=True)
    return path


def read_report(path: Path) -> dict:
    if not path.is_file():
        raise WindowUnavailable(f"the editor wrote no measurement file at {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def marks_of(text: str) -> list | None:
    """A measurement file's marks, or None when the text is not a finished measurement file.

    The keystroke bench reads its file this way rather than through read_report, because a trial
    that ends in its own failure is killed by stop(), which leaves either nothing at all or a file
    that stops in the middle of an object. That is one unobserved trial, not a machine without a
    window, so it must not become WindowUnavailable (Issue #36).
    """
    try:
        report = json.loads(text)
    except json.JSONDecodeError:
        return None
    marks = report.get("marks") if isinstance(report, dict) else None
    return marks if isinstance(marks, list) else None


def observed_marks(path: Path) -> list:
    """The marks of a trial's measurement file; an absent or unfinished one ends the trial."""
    marks = marks_of(path.read_text(encoding="utf-8")) if path.is_file() else None
    if marks is None:
        raise TrialNotObserved(f"the editor left no finished measurement file ({path.name})")
    return marks


def first_reading(marks: list, milestone: str) -> int:
    """The first qpcMicroseconds of that milestone; device lost re-marks, so later ones are noise."""
    for entry in marks:
        if entry["milestone"] == milestone:
            return int(entry["qpcMicroseconds"])
    raise AssertionError(f"the measured exe never marked {milestone}"
                         " (an exe built before Issue #19?)")


def breakdown(report: dict) -> dict:
    """One run's startup segments in ms, in the canonical order, each ending at its milestone."""
    marks = report["marks"]
    segments = {ORIGIN_SEGMENT: float(report["processCreationToOriginMs"])}
    previous = 0
    for milestone in STARTUP_MILESTONES:
        reading = first_reading(marks, milestone)
        segments[milestone] = (reading - previous) / MICROSECONDS_PER_MILLISECOND
        previous = reading
    return segments


def frame_after(marks: list, index: int) -> int | None:
    """The first frame presented after that input, or None when the run ended before one was."""
    for entry in marks[index + 1:]:
        if entry["milestone"] == "frame_presented":
            return int(entry["qpcMicroseconds"])
    return None


def input_indexes(marks: list) -> list[int]:
    return [index for index, entry in enumerate(marks) if entry["milestone"] == "input_received"]


def bench_startup(executable: Path, environment: dict, folder: Path) -> tuple[dict, dict]:
    report = report_path(folder, "startup")
    process, window, _ = start(executable, environment, ["--measure", str(report)])
    try:
        close(window)
        process.wait(timeout=EXIT_SECONDS)
    finally:
        stop(process)
    measured = read_report(report)
    # 窓が見えるまで（ADR 0013 の決定 3）。原点までの区間に window_shown の読みを足す。
    shown = (float(measured["processCreationToOriginMs"])
             + first_reading(measured["marks"], "window_shown") / MICROSECONDS_PER_MILLISECOND)
    return ({"startup-first-frame": float(measured["processCreationToFirstFrameMs"]),
             "startup-window-shown": shown},
            {"startup-first-frame": breakdown(measured)})


def bench_keys(executable: Path, environment: dict, folder: Path,
               document: Path | None = None) -> tuple[dict, dict]:
    """One keystroke, then 200 posted at once: Windows holds WM_PAINT until the queue drains.

    With a document the same trial runs after that document was opened by the startup argument,
    and only its burst becomes a value, as key-to-frame-burst-200-16mib (ADR 0044 decision 6).

    A trial only measures coalescing if the 200 messages really were in the queue together. When
    the posting process is starved the editor reads them as they arrive and the number becomes the
    poster's speed, which shows up as a burst whose inputs span hundreds of milliseconds; a window
    that is covered or starved can end up measuring fewer of them than were posted. Both are a
    stimulus that was not delivered as specified, so the trial is repeated rather than recorded,
    and when BURST_ATTEMPTS of them fail in a row the burst of that trial is missing rather than a
    value (Issue #30). Over the 16 MiB document the span is the editor's own reading time, so
    only the count and the frame decide there and the span is recorded (Issue #179). The single
    keystroke is a stimulus of its own: it stays a value whenever that keystroke and the frame
    that answered it were measured.

    A trial can also deliver no observation at all (TrialNotObserved), and that is repeated in the
    same place and for the same reason; a trial that ended there measured neither keystroke, so its
    single is None too and the record says "missing" rather than carrying an older trial's number
    (Issue #36).
    """
    name = EMPTY_BURST_BENCH if document is None else LARGE_BURST_BENCH
    single = None
    for attempt in range(BURST_ATTEMPTS):
        try:
            single, burst, span, delivered = keys_trial(executable, environment, folder, document)
        except TrialNotObserved as unobserved:
            single, reason = None, str(unobserved)
        else:
            reason = burst_failure(burst, span, delivered, name)
            if reason is None:
                return keys_values(name, single, burst), keys_parts(name, span)
        print(f"Speed: {reason}; repeating the trial ({attempt + 1}/{BURST_ATTEMPTS})")
    print(f"Speed: no trial delivered the stimulus as specified in {BURST_ATTEMPTS} attempts;"
          f" this trial of {name} is missing")
    return keys_values(name, single, None), {}


def keys_values(name: str, single: float | None, burst: float | None) -> dict:
    """The values of one keystroke trial; the single keystroke is a bench of the empty one only."""
    if name == LARGE_BURST_BENCH:
        return {LARGE_BURST_BENCH: burst}
    return {"key-to-frame-single": single, EMPTY_BURST_BENCH: burst}


def keys_parts(name: str, span: float) -> dict:
    """How long the 200 keystrokes of the 16 MiB trial took to arrive; nothing for the empty one."""
    if name == LARGE_BURST_BENCH:
        return {LARGE_BURST_BENCH: {ARRIVAL_SEGMENT: span}}
    return {}


def burst_failure(burst: float | None, span: float, delivered: int,
                  name: str = EMPTY_BURST_BENCH) -> str | None:
    """Why this trial's burst is not a value, in one phrase, or None when the stimulus arrived.

    The keystrokes are posted while the window thread is held, so they are in the queue together
    and the span of their input_received marks is how long the editor took to read them. Over the
    empty document that stays within BURST_SPAN_LIMIT_MS and a wider span means the poster was
    starved; over the 16 MiB document that span is the cost being measured, so it is recorded and
    never a reason for the trial to be missing (Issue #179).
    """
    if delivered < BURST_KEYS + 2:
        return f"only {delivered} of {BURST_KEYS + 2} keystrokes were measured by the window"
    if burst is None:
        return "no frame was presented after the 200 keystrokes"
    if name == EMPTY_BURST_BENCH and span > BURST_SPAN_LIMIT_MS:
        return f"the 200 keystrokes reached the window over {span:.1f} ms, not together"
    return None


def measured_single(marks: list, inputs: list) -> float | None:
    """inputs[1] and the frame that answered it in ms, or None when the pair was not measured."""
    if len(inputs) < 2:
        return None
    frame = frame_after(marks, inputs[1])
    if frame is None:
        return None
    return (frame - int(marks[inputs[1]]["qpcMicroseconds"])) / MICROSECONDS_PER_MILLISECOND


def keys_trial(executable: Path, environment: dict, folder: Path,
               document: Path | None = None) -> tuple[float | None, float | None, float, int]:
    """One trial: single ms, 200 keystroke ms, how long the 200 took to arrive, keystrokes measured.

    Either value is None when its own stimulus did not reach the window, and the caller repeats or
    records the trial as missing (Issue #30); the count is what the caller says in that line.
    TrialNotObserved leaves instead when the trial produced nothing to read at all (Issue #36).
    """
    if document is None:
        report = report_path(folder, "keys")
        process, window, _ = start(executable, environment, ["--measure", str(report)])
    else:
        report = report_path(folder, "keys-large")
        process, window, _ = start(executable, environment,
                                   ["--measure", str(report), str(document)], seconds=LARGE_SECONDS)
    try:
        # 隠れた窓は合成器が間引き、待機可能オブジェクトが何十 vsync も signal されないことがある
        # （打鍵 → 描画に 800 ms の外れ値が出る）。最前面に出し、取れるなら前景にもする。
        raise_window(window)
        if not take_foreground(window):
            print("Speed: the bench window did not take the foreground")
        # 覆われた窓は合成器に間引かれ、刺激が届いても値にならない。判定はしないが印は残す（#30）。
        covering = covered_by(window)
        if covering is not None:
            print(f"Speed: another window covers the bench window ({covering})")
        time.sleep(SETTLE_SECONDS)
        # 最初の 1 打鍵は捨てる。交換鎖が定常になる前の 1 枚は合成器の都合で何十 vsync も遅れる。
        write_text(window, "w")
        time.sleep(WARMUP_SECONDS)
        write_text(window, "a")
        time.sleep(SINGLE_KEY_SECONDS)
        if not post_together(window, lambda: write_text(window, "b" * BURST_KEYS)):
            print("Speed: the window thread could not be held; the burst was read as it was posted")
        time.sleep(BURST_SECONDS)
        close(window)
        # 本文を打ち替えたので閉じるときに未保存の確認が出る（ADR 0010 の決定 10）。
        # 痩せた机では 2.5 秒で出ないことがある。その試行は観測できていないので測り直す（#36）。
        if not dismiss_dialog(process, IDNO):
            raise TrialNotObserved("the unsaved confirmation never came up")
        process.wait(timeout=EXIT_SECONDS)
    finally:
        stop(process)
    marks = observed_marks(report)
    inputs = input_indexes(marks)
    single = measured_single(marks, inputs)
    if len(inputs) < BURST_KEYS + 2:
        return single, None, 0.0, len(inputs)
    # inputs[0] は捨てる暖機の 1 打鍵。測るのは inputs[1] の 1 打鍵と、そのあとの 200 打鍵ちょうど
    # （実キーが紛れ込んでも数え間違えないように、後ろからではなく先頭から 200 を取る）。
    keys = inputs[2:2 + BURST_KEYS]
    started = int(marks[keys[0]]["qpcMicroseconds"])
    finished = frame_after(marks, keys[-1])
    span = (int(marks[keys[-1]]["qpcMicroseconds"]) - started) / MICROSECONDS_PER_MILLISECOND
    if finished is None:
        return single, None, span, len(inputs)
    return single, (finished - started) / MICROSECONDS_PER_MILLISECOND, span, len(inputs)


def bench_large_file(executable: Path, environment: dict, folder: Path,
                     document: Path) -> tuple[dict, dict]:
    report = report_path(folder, "large")
    process, window, _ = start(executable, environment,
                               ["--measure", str(report), str(document)], seconds=LARGE_SECONDS)
    try:
        close(window)
        process.wait(timeout=EXIT_SECONDS)
    finally:
        stop(process)
    measured = read_report(report)
    return ({"open-large-file-16mib": float(measured["processCreationToFirstFrameMs"])},
            {"open-large-file-16mib": breakdown(measured)})


def spread(samples: dict) -> dict:
    """Median and range per startup segment; the samples themselves stay out (5 x 10 numbers)."""
    return {name: {"medianMs": round(statistics.median(values), 4),
                   "minimumMs": round(min(values), 4),
                   "maximumMs": round(max(values), 4)}
            for name, values in samples.items()}


def summarise(samples: dict, missing: dict) -> dict:
    """The valid trials of each bench, with the number of trials that delivered no stimulus.

    A bench with no valid trial keeps its place with samples [] and a null median, because the
    record must say "not measured" rather than leave the reader to notice an absence (Issue #30).
    """
    summary = {}
    for name, values in samples.items():
        gone = missing.get(name, 0)
        if not values:
            summary[name] = {"samples": [], "medianMs": None, "minimumMs": None,
                             "maximumMs": None, "missing": gone}
            continue
        summary[name] = {"samples": [round(value, 4) for value in values],
                         "medianMs": round(statistics.median(values), 4),
                         "minimumMs": round(min(values), 4),
                         "maximumMs": round(max(values), 4),
                         "missing": gone}
    return summary


def judged_samples(measured: dict) -> int | None:
    """How many trials of that bench became values, or None when the record lists no trials.

    A record written before Issue #30 lists its samples and has no "missing" key, so it reads as
    five valid trials. The small records eng/prove-gates.py writes to test the reference itself
    carry medians alone; those say nothing about delivery and are judged on the median.
    """
    samples = measured.get("samples")
    return None if samples is None else len(samples)


def measure(build: Path, repetitions: int) -> dict:
    become_dpi_aware()
    executable, environment, folder = prepare(build)
    document = large_document(folder)
    samples: dict = {name: [] for name in BENCHES}
    # 刺激が届かなかった試行は値にせず数える（Issue #30）。中央値は残った試行から出す。
    missing: dict = {name: 0 for name in BENCHES}
    # 起動の内訳は区間ごとに 5 回ぶん貯めて、値と同じように中央値を出す（Issue #19）。
    # 16 MiB の 200 打鍵の到着の幅も同じ形で残す（#179）。
    segments: dict = {name: {} for name in BREAKDOWN_BENCHES}
    for _ in range(repetitions):
        for values, parts in (bench_startup(executable, environment, folder),
                              bench_keys(executable, environment, folder),
                              bench_large_file(executable, environment, folder, document),
                              bench_keys(executable, environment, folder, document)):
            for name, value in values.items():
                if value is None:
                    missing[name] += 1
                else:
                    samples[name].append(value)
            for name, run in parts.items():
                for segment, value in run.items():
                    segments[name].setdefault(segment, []).append(value)
    return {"recordedAt": stamp(), "repetitions": repetitions, "machine": machine(),
            "document": {"path": document.name, "bytes": document.stat().st_size,
                         "lines": LARGE_LINES},
            "values": summarise(samples, missing),
            "breakdown": {name: spread(segments[name]) for name in BREAKDOWN_BENCHES}}


def write_record(record: dict) -> Path:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / f"{record['recordedAt']}.json"
    path.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    return path


def describe(record: dict) -> str:
    lines = [f"Speed: {record['repetitions']} runs on {record['machine']['fingerprint']}"
             f" ({record['machine']['cpu']} / {record['machine']['gpu']}"
             f" / {record['machine']['dpi']} dpi)"]
    for name in BENCHES:
        value = record["values"][name]
        gone = value.get("missing", 0)
        note = f" (missing {gone} of {len(value['samples']) + gone})" if gone else ""
        if value["medianMs"] is None:
            lines.append(f"  {name}: no trial delivered the stimulus{note}")
            continue
        lines.append(f"  {name}: median {value['medianMs']:.3f} ms"
                     f" (min {value['minimumMs']:.3f}, max {value['maximumMs']:.3f})"
                     f" samples {value['samples']}{note}")
    # 内訳は CI のログからしか読めないので、必ず 1 行で出す（Issue #19）。
    for name in STARTUP_BENCHES:
        parts = record.get("breakdown", {}).get(name)
        if parts is None:
            continue
        written = " | ".join(f"{segment} {parts[segment]['medianMs']:.1f}"
                             for segment in (ORIGIN_SEGMENT, *STARTUP_MILESTONES)
                             if segment in parts)
        lines.append(f"  {name} breakdown (median ms): {written}")
    return "\n".join(lines)


def compare(reference: dict, values: dict, recorded: dict) -> tuple[list[str], list[str]]:
    """The regressions, and separately the benches too few trials measured to judge (Issue #30).

    A median of one or two trials is not the bench, it is whatever the two trials that survived
    happened to be, so it is never called a regression. The caller turns the two lists into two
    exit codes, because "slower" and "not measured" are repaired in different places.
    """
    tolerance = reference["tolerance"]
    findings, unmeasurable = [], []
    for name in BENCHES:
        against = recorded.get(name)
        if against is None:
            continue
        valid = judged_samples(values[name])
        if valid is not None and valid < MIN_VALID_SAMPLES:
            trials = valid + values[name].get("missing", 0)
            unmeasurable.append(f"QLT-014: {name}: only {valid} of {trials} trials delivered the"
                                " stimulus; not judged")
            continue
        allowed = against["medianMs"] * (1.0 + tolerance["percent"] / 100.0)
        measured = values[name]["medianMs"]
        if measured > allowed and measured - against["medianMs"] > tolerance["floorMs"]:
            findings.append(f"QLT-014: {name}: {measured:.3f} ms exceeds {allowed:.3f} ms"
                            f" (reference {against['medianMs']:.3f} ms"
                            f" + {tolerance['percent']}% or {tolerance['floorMs']} ms)")
    return findings, unmeasurable


def adopt_one(reference_path: Path, record: dict, name: str) -> None:
    """Write one bench into a machine that already has references; nothing else is touched.

    Adding a bench must not move the four medians that were adopted earlier, because a reference
    that moves stops being the thing the gate compares against (ADR 0011 decision 5 / ADR 0013).
    """
    reference = json.loads(reference_path.read_text(encoding="utf-8"))
    identity = record["machine"]
    recorded = reference["machines"].get(identity["fingerprint"])
    if recorded is None:
        raise SystemExit(f"Speed: {identity['fingerprint']} has no reference yet;"
                         " adopt every bench once before adopting one of them")
    measured = record["values"][name]
    if measured["medianMs"] is None:
        raise SystemExit(f"Speed: {name} has no valid trial in this record; a reference is never"
                         " adopted from a bench that could not be measured")
    recorded["values"][name] = {"medianMs": measured["medianMs"],
                                "minimumMs": measured["minimumMs"],
                                "maximumMs": measured["maximumMs"]}
    reference_path.write_text(json.dumps(reference, ensure_ascii=False, indent=2) + "\n",
                              encoding="utf-8")
    print(f"Speed: adopted {name} for {identity['fingerprint']} into {reference_path}")


def adopt(reference_path: Path, record: dict) -> None:
    reference = json.loads(reference_path.read_text(encoding="utf-8"))
    identity = record["machine"]
    unmeasured = [name for name in BENCHES if record["values"][name]["medianMs"] is None]
    if unmeasured:
        raise SystemExit(f"Speed: {', '.join(unmeasured)} had no valid trial in this record;"
                         " a reference is never adopted from a bench that could not be measured")
    reference["machines"][identity["fingerprint"]] = {
        "cpu": identity["cpu"], "gpu": identity["gpu"], "dpi": identity["dpi"],
        "recordedAt": record["recordedAt"], "repetitions": record["repetitions"],
        "values": {name: {"medianMs": record["values"][name]["medianMs"],
                          "minimumMs": record["values"][name]["minimumMs"],
                          "maximumMs": record["values"][name]["maximumMs"]}
                   for name in BENCHES},
    }
    reference_path.write_text(json.dumps(reference, ensure_ascii=False, indent=2) + "\n",
                              encoding="utf-8")
    print(f"Speed: adopted {identity['fingerprint']} into {reference_path}")


def gather(arguments) -> dict:
    """The values to judge: a record made now, or one that was measured earlier."""
    if arguments.values:
        return json.loads(Path(arguments.values).read_text(encoding="utf-8"))
    record = measure(arguments.executable, arguments.repetitions)
    print(describe(record))
    print(f"Speed: recorded {write_record(record)}")
    return record


def check(arguments) -> int:
    reference_path = arguments.reference or REFERENCE
    reference = json.loads(reference_path.read_text(encoding="utf-8"))
    try:
        record = gather(arguments)
    except WindowUnavailable as unavailable:
        # 表示のある対話セッションが無い機械（CI の可能性）は記録して通す（ADR 0011 の決定 4）。
        print(f"Speed: window unavailable; recorded only ({unavailable})")
        return 0
    recorded = reference["machines"].get(record["machine"]["fingerprint"])
    if recorded is None:
        # 基準値の無い host は記録だけで通るが、どの host を判定しなかったかを言う（ADR 0016 の決定 3）。
        identity = record["machine"]
        # ダッシュは ASCII の -- で書く。この行は施主の cp932 の机でも CI でも出るが、U+2014 は
        # cp932 に無く（0x815C は U+2015）、print が UnicodeEncodeError で落ちてゲートが
        # 「退行」と言ってしまう。ファイル内の他の出力と docstring も同じ綴りを使っている。
        print(f"Speed: no reference for {identity['fingerprint']} ({identity['cpu']});"
              " recorded only -- QLT-014 is not judged on this host")
        return 0
    findings, unmeasurable = compare(reference, record["values"], recorded["values"])
    for line in [*findings, *unmeasurable]:
        print(line)
    print(f"Speed: {len(BENCHES)} benches checked, {len(findings)} regression(s),"
          f" {len(unmeasurable)} unmeasurable")
    return exit_code(findings, unmeasurable)


def exit_code(findings: list, unmeasurable: list) -> int:
    """1 is a regression, 2 is a bench the machine could not measure, 0 is neither (Issue #30)."""
    if findings:
        return 1
    return 2 if unmeasurable else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--record", action="store_true", help="measure and write out/speed")
    parser.add_argument("--adopt", action="store_true", help="write the medians into the reference")
    parser.add_argument("--bench", choices=BENCHES, help="with --adopt: write this one bench only")
    parser.add_argument("--check", action="store_true", help="compare with this machine's reference")
    parser.add_argument("--executable", type=Path, default=ROOT / RELEASE_EXECUTABLE)
    parser.add_argument("--reference", type=Path, help="a reference file other than the canonical one")
    parser.add_argument("--values", type=Path, help="judge an earlier record instead of measuring")
    parser.add_argument("--repetitions", type=int, default=REPETITIONS)
    arguments = parser.parse_args()
    if not (arguments.record or arguments.adopt or arguments.check):
        parser.error("one of --record, --adopt or --check is required")
    if arguments.check:
        return check(arguments)
    record = gather(arguments)
    if arguments.adopt and arguments.bench is not None:
        adopt_one(arguments.reference or REFERENCE, record, arguments.bench)
    elif arguments.adopt:
        adopt(arguments.reference or REFERENCE, record)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
