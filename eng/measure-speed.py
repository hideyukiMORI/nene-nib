"""Measure the three speed benches and compare them with this machine's reference (QLT-014).

ADR 0011: the editor marks two milestones (input_received / frame_presented), the Win32 timing
adapter turns them into a measurement file when the process is started with --measure <out.json>,
and this script starts the editor, drives it through eng/window_driver.py, and reads the file back.

  startup-first-frame       process creation -> first frame_presented
  key-to-frame-single       one WM_CHAR -> the next frame_presented
  key-to-frame-burst-200    200 WM_CHAR posted at once -> the frame that finishes them
  open-large-file-16mib     the same startup measurement with a 16 MiB, 200,000 line argument

The exe under measurement is the Release one (build-release/NeNeNib.exe). The Debug exe carries
ASan and UBSan (eng/targets.cmake instruments the Debug configuration only), and an instrumented
run is not the speed of the product that ADR 0006 gates. eng/verify-window.py keeps driving the
Debug exe, because running the sanitizers against the real window is the point there.

Every bench runs five times; the value is the median and the spread is recorded with it. References
live in eng/perf-reference.json per machine fingerprint (CPU name, display adapter, system DPI),
because the same numbers on a shared CI runner mean nothing (ADR 0006). A machine without a
reference records its numbers and passes; so does a machine that cannot put a window on a desktop.
Widening a reference is an ADR decision, not a repair.

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

from window_driver import (become_dpi_aware, close, dismiss_dialog, IDNO, post_together,
                           raise_window, start, stop, take_foreground, user, WindowUnavailable,
                           write_text)

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
BURST_SPAN_LIMIT_MS = 50.0
BURST_ATTEMPTS = 3
EXIT_SECONDS = 15.0
LARGE_LINES = 200_000
LARGE_LINE_BYTES = 84
LARGE_SECONDS = 60.0
MICROSECONDS_PER_MILLISECOND = 1000.0
DISPLAY_ADAPTERS = r"SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\0000"
CENTRAL_PROCESSOR = r"HARDWARE\DESCRIPTION\System\CentralProcessor\0"
BENCHES = ("startup-first-frame", "key-to-frame-single", "key-to-frame-burst-200",
           "open-large-file-16mib")


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


def frame_after(marks: list, index: int) -> int:
    for entry in marks[index + 1:]:
        if entry["milestone"] == "frame_presented":
            return int(entry["qpcMicroseconds"])
    raise AssertionError("no frame was presented after that input")


def input_indexes(marks: list) -> list[int]:
    return [index for index, entry in enumerate(marks) if entry["milestone"] == "input_received"]


def bench_startup(executable: Path, environment: dict, folder: Path) -> dict:
    report = report_path(folder, "startup")
    process, window, _ = start(executable, environment, ["--measure", str(report)])
    try:
        close(window)
        process.wait(timeout=EXIT_SECONDS)
    finally:
        stop(process)
    return {"startup-first-frame": float(read_report(report)["processCreationToFirstFrameMs"])}


def bench_keys(executable: Path, environment: dict, folder: Path) -> dict:
    """One keystroke, then 200 posted at once: Windows holds WM_PAINT until the queue drains.

    A trial only measures coalescing if the 200 messages really were in the queue together. When
    the posting process is starved the editor reads them as they arrive and the number becomes the
    poster's speed, which shows up as a burst whose inputs span hundreds of milliseconds. That is a
    stimulus that was not delivered as specified, so the trial is repeated rather than recorded.
    """
    for attempt in range(BURST_ATTEMPTS):
        single, burst, span = keys_trial(executable, environment, folder)
        if span <= BURST_SPAN_LIMIT_MS:
            break
        print(f"Speed: the 200 keystrokes reached the window over {span:.1f} ms,"
              f" not together; repeating the trial ({attempt + 1}/{BURST_ATTEMPTS})")
    return {"key-to-frame-single": single, "key-to-frame-burst-200": burst}


def keys_trial(executable: Path, environment: dict, folder: Path) -> tuple[float, float, float]:
    """One trial: (single keystroke ms, 200 keystroke ms, how long the 200 took to arrive)."""
    report = report_path(folder, "keys")
    process, window, _ = start(executable, environment, ["--measure", str(report)])
    try:
        # 隠れた窓は合成器が間引き、待機可能オブジェクトが何十 vsync も signal されないことがある
        # （打鍵 → 描画に 800 ms の外れ値が出る）。最前面に出し、取れるなら前景にもする。
        raise_window(window)
        if not take_foreground(window):
            print("Speed: the bench window did not take the foreground")
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
        assert dismiss_dialog(process, IDNO), "the unsaved confirmation never came up"
        process.wait(timeout=EXIT_SECONDS)
    finally:
        stop(process)
    marks = read_report(report)["marks"]
    inputs = input_indexes(marks)
    assert len(inputs) >= BURST_KEYS + 2, f"only {len(inputs)} keystrokes reached the window"
    # inputs[0] は捨てる暖機の 1 打鍵。測るのは inputs[1] の 1 打鍵と、そのあとの 200 打鍵ちょうど
    # （実キーが紛れ込んでも数え間違えないように、後ろからではなく先頭から 200 を取る）。
    keys = inputs[2:2 + BURST_KEYS]
    started = int(marks[keys[0]]["qpcMicroseconds"])
    single = frame_after(marks, inputs[1]) - int(marks[inputs[1]]["qpcMicroseconds"])
    burst = frame_after(marks, keys[-1]) - started
    span = int(marks[keys[-1]]["qpcMicroseconds"]) - started
    return (single / MICROSECONDS_PER_MILLISECOND, burst / MICROSECONDS_PER_MILLISECOND,
            span / MICROSECONDS_PER_MILLISECOND)


def bench_large_file(executable: Path, environment: dict, folder: Path, document: Path) -> dict:
    report = report_path(folder, "large")
    process, window, _ = start(executable, environment,
                               ["--measure", str(report), str(document)], seconds=LARGE_SECONDS)
    try:
        close(window)
        process.wait(timeout=EXIT_SECONDS)
    finally:
        stop(process)
    return {"open-large-file-16mib": float(read_report(report)["processCreationToFirstFrameMs"])}


def summarise(samples: dict) -> dict:
    return {name: {"samples": [round(value, 4) for value in values],
                   "medianMs": round(statistics.median(values), 4),
                   "minimumMs": round(min(values), 4),
                   "maximumMs": round(max(values), 4)}
            for name, values in samples.items()}


def measure(build: Path, repetitions: int) -> dict:
    become_dpi_aware()
    executable, environment, folder = prepare(build)
    document = large_document(folder)
    samples: dict = {name: [] for name in BENCHES}
    for _ in range(repetitions):
        for result in (bench_startup(executable, environment, folder),
                       bench_keys(executable, environment, folder),
                       bench_large_file(executable, environment, folder, document)):
            for name, value in result.items():
                samples[name].append(value)
    return {"recordedAt": stamp(), "repetitions": repetitions, "machine": machine(),
            "document": {"path": document.name, "bytes": document.stat().st_size,
                         "lines": LARGE_LINES},
            "values": summarise(samples)}


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
        lines.append(f"  {name}: median {value['medianMs']:.3f} ms"
                     f" (min {value['minimumMs']:.3f}, max {value['maximumMs']:.3f})"
                     f" samples {value['samples']}")
    return "\n".join(lines)


def compare(reference: dict, values: dict, recorded: dict) -> list[str]:
    tolerance = reference["tolerance"]
    findings = []
    for name in BENCHES:
        against = recorded.get(name)
        if against is None:
            continue
        allowed = against["medianMs"] * (1.0 + tolerance["percent"] / 100.0)
        measured = values[name]["medianMs"]
        if measured > allowed and measured - against["medianMs"] > tolerance["floorMs"]:
            findings.append(f"QLT-014: {name}: {measured:.3f} ms exceeds {allowed:.3f} ms"
                            f" (reference {against['medianMs']:.3f} ms"
                            f" + {tolerance['percent']}% or {tolerance['floorMs']} ms)")
    return findings


def adopt(reference_path: Path, record: dict) -> None:
    reference = json.loads(reference_path.read_text(encoding="utf-8"))
    identity = record["machine"]
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
        print("Speed: no reference for this machine; recorded only")
        return 0
    findings = compare(reference, record["values"], recorded["values"])
    for finding in findings:
        print(finding)
    print(f"Speed: {len(BENCHES)} benches checked, {len(findings)} regression(s)")
    return int(bool(findings))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--record", action="store_true", help="measure and write out/speed")
    parser.add_argument("--adopt", action="store_true", help="write the medians into the reference")
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
    if arguments.adopt:
        adopt(arguments.reference or REFERENCE, record)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
