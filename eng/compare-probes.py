"""Compare the same window-free workloads in a fixed ABBA order (ADR 0082 / #329)."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import statistics
import subprocess
import sys

WORKLOADS = ("controller-open-utf8-16mib", "buffer-from-utf8-16mib",
             "controller-insert-200", "display-line-long",
             "controller-insert-200-after-delete-1mib", "controller-insert-200-after-delete-16mib",
             "utf8-validate-ascii-16mib", "utf8-validate-japanese-6mib",
             "controller-vim-insert-200-register-1mib", "controller-vim-insert-200-register-16mib",
             "controller-vim-record-insert-200", "controller-vim-record-insert-2000",
             "buffer-line-text-scattered-crlf-4096",
             "buffer-line-text-scattered-lf-4096",
             "buffer-position-long-utf8-57344",
             "buffer-position-scattered-utf8-57344",
             "palette-listed-name-5000",
             "palette-listed-location-5000",
             "palette-append-narrow-5000-to-50",
             "palette-caret-left-5000",
             "codepage-to-utf8-cp932-japanese-16mib",
             "utf16-to-utf8-japanese-8m-units",
             "utf16-to-utf8-ascii-8m-units",
             "utf16-to-utf8-supplementary-8m-units",
             "controller-save-utf8-16mib",
             "controller-save-utf8-bom-16mib",
             "buffer-erase-scattered-head-4096",
             "buffer-erase-scattered-middle-4096",
             "buffer-erase-scattered-tail-4096",
             "buffer-erase-scattered-all-4096",
             "buffer-erase-single-middle-4096",
             "buffer-offset-long-head-57344",
             "buffer-offset-long-middle-57344",
             "buffer-offset-long-end-57344",
             "buffer-offset-scattered-middle-57344",
             "search-forward-head-many-4096",
             "search-forward-middle-many-4096",
             "search-forward-tail-many-4096",
             "search-backward-head-many-4096",
             "search-backward-middle-many-4096",
             "search-backward-tail-many-4096",
             "pattern-star-miss-1024",
             "pattern-star-miss-2048",
             "pattern-star-miss-4096",
             "pattern-multistar-miss-32",
             "pattern-greedy-hit-4096",
             "pattern-literal-tail-4102",
             "pattern-literal-long-4096")
ORDER = ("before", "after", "after", "before")


class ProbeNotObserved(ValueError):
    """The requested work has no complete, trustworthy pair of observations."""


def unique_object(pairs: list) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise ProbeNotObserved(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(text: str):
    def reject_constant(value):
        raise ProbeNotObserved(f"nonfinite JSON constant: {value}")
    return json.loads(text, object_pairs_hook=unique_object, parse_constant=reject_constant)


def fixed_input(workload: str) -> bytes:
    if workload in (*WORKLOADS[:2], *WORKLOADS[4:7]):
        filler = "nenenib speed sample line for the open-large-file bench "
        lines = (1024 * 1024) // 84 if workload.endswith("delete-1mib") else 200000
        return "".join((f"{line:06d} " + filler + filler)[:82] + "\r\n"
                       for line in range(lines)).encode("utf-8")
    if workload == "controller-insert-200":
        return b"a" * 200
    if workload == "display-line-long":
        return ("000000 " + "日本語の長い行と分割表示の文字組みを測定する。" * 90).encode("utf-8")
    if workload == "utf8-validate-japanese-6mib":
        return (fixed_input("display-line-long") + b"\r\n") * 1000
    if workload == "controller-vim-insert-200-register-1mib":
        return b"r" * 1048576
    if workload == "controller-vim-insert-200-register-16mib":
        return b"r" * 16777216
    if workload == "controller-vim-record-insert-200":
        return b"x" * 200
    if workload == "controller-vim-record-insert-2000":
        return b"x" * 2000
    if workload in WORKLOADS[24:26]:
        return fixed_input("controller-open-utf8-16mib")
    if workload in WORKLOADS[26:31]:
        return (b"a" * 78 + b"\r\n") * 4096
    if workload in WORKLOADS[31:35]:
        return "a日本語🖋".encode("utf-8") * 4096
    if workload in WORKLOADS[35:41]:
        return "a日本語🖋 ".encode("utf-8") * 4096
    if workload in WORKLOADS[41:46]:
        return b"a" * (1024, 2048, 4096, 32, 4096)[WORKLOADS.index(workload) - 41]
    if workload == "pattern-literal-tail-4102":
        return b"x" * 4096 + b"needle"
    if workload == "pattern-literal-long-4096":
        return b"a" * 4096
    return scoped_fixed_input(workload)


def scoped_palette_input(query: str, detail: str) -> bytes:
    rows = [f"files\t{query}\n"]
    for index in range(5000):
        if detail:
            label = f"ENTRY_{index:05d}_ABCDEFGHIJKLMNOPQRSTUV.txt"
        elif index < 50:
            label = f"QX_Y_{index:05d}_file.txt"
        elif index < 500:
            label = f"QX_N_{index:05d}_file.txt"
        else:
            label = f"AA_N_{index:05d}_file.txt"
        rows.append(f"folder\topen\tprobe-{index:05d}\t{label}\t{detail}\n")
    return "".join(rows).encode("utf-8")


def scoped_fixed_input(workload: str) -> bytes:
    if workload == "buffer-line-text-scattered-crlf-4096":
        return (b"a" * 78 + b"\r\n") * 4096
    if workload == "buffer-line-text-scattered-lf-4096":
        return (b"a" * 78 + b"\n") * 4096
    if workload in ("buffer-position-long-utf8-57344", "buffer-position-scattered-utf8-57344"):
        return "a日本語🖋".encode("utf-8") * 4096
    if workload in ("palette-listed-name-5000", "palette-listed-location-5000"):
        query = "entry" if workload == "palette-listed-name-5000" else "zroot"
        return scoped_palette_input(query, r"D:\ZROOT\LONG_DIRECTORY_COMPONENT\GROUP_00")
    if workload in ("palette-append-narrow-5000-to-50", "palette-caret-left-5000"):
        return scoped_palette_input("qx", "")
    if workload == "codepage-to-utf8-cp932-japanese-16mib":
        return b"\x93\xfa\x96\x7b" * 4194304
    if workload == "utf16-to-utf8-japanese-8m-units":
        return b"\xe5\x65\x2c\x67" * 4194304
    if workload == "utf16-to-utf8-ascii-8m-units":
        return b"a\0" * 8388608
    if workload == "utf16-to-utf8-supplementary-8m-units":
        return b"\x3d\xd8\x8b\xdd" * 4194304
    raise ProbeNotObserved("unknown workload")


def fnv1a64(data: bytes) -> str:
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return str(value)


def positive_integer(value, label: str) -> int:
    if type(value) is not int or value <= 0:
        raise ProbeNotObserved(f"{label} must be a positive integer")
    return value


def samples_from(report: dict, iterations: int) -> list[int]:
    if not isinstance(report, dict):
        raise ProbeNotObserved("timing report must be an object")
    positive_integer(report.get("qpcFrequency"), "qpcFrequency")
    marks = report.get("marks")
    if not isinstance(marks, list) or len(marks) != iterations * 2:
        raise ProbeNotObserved("missing or extra marks")
    samples = []
    previous = -1
    for index in range(iterations):
        pair = marks[index * 2:index * 2 + 2]
        values = []
        for mark, expected in zip(pair, ("probe_started", "probe_finished")):
            if not isinstance(mark, dict) or mark.get("milestone") != expected:
                raise ProbeNotObserved("unpaired or unexpected milestone")
            value = mark.get("qpcMicroseconds")
            if type(value) is not int or value < 0 or value < previous:
                raise ProbeNotObserved("invalid or reversed timestamp")
            previous = value
            values.append(value)
        duration = values[1] - values[0]
        if duration <= 0:
            raise ProbeNotObserved("sample is below the timing resolution")
        samples.append(duration)
    return samples


def metadata_from(text: str, request: dict, expected_input: dict) -> dict:
    metadata = load_json(text)
    if not isinstance(metadata, dict):
        raise ProbeNotObserved("probe metadata must be an object")
    expected = {"schema": 1, "workload": request["workload"],
                "iterations": request["iterations"], "warmup": 1, **expected_input}
    if any(type(metadata.get(key)) is not type(value) or metadata.get(key) != value
           for key, value in expected.items()):
        raise ProbeNotObserved("workload, iterations, warmup or input differs")
    if metadata.get("commit") != request["commit"] or metadata.get("dirty") != 0:
        raise ProbeNotObserved("built commit differs or build source was dirty")
    if metadata.get("configuration") != "Release":
        raise ProbeNotObserved("comparison requires the development target's Release build")
    if not isinstance(metadata.get("compilerVersion"), str) or not metadata["compilerVersion"]:
        raise ProbeNotObserved("compiler version is missing")
    if not re.fullmatch(r"[0-9a-f]{64}", str(metadata.get("toolchainSha256", ""))):
        raise ProbeNotObserved("toolchain manifest hash is missing")
    if not re.fullmatch(r"[0-9]+", str(metadata.get("outputChecksum", ""))):
        raise ProbeNotObserved("output checksum is missing")
    return metadata


def summarize(runs: list[dict], blocks: int) -> dict:
    ratios = []
    for block in range(blocks):
        group = runs[block * 4:block * 4 + 4]
        for left, right in ((group[0], group[1]), (group[3], group[2])):
            ratios.extend(after / before for before, after in zip(left["microseconds"],
                                                                   right["microseconds"]))
    values = {side: [value for run in runs if run["side"] == side
                     for value in run["microseconds"]] for side in ("before", "after")}
    return {"pairedRatiosAfterOverBefore": ratios,
            "ratioMedian": statistics.median(ratios), "ratioRange": [min(ratios), max(ratios)],
            "microseconds": {side: {"median": statistics.median(samples),
                                    "range": [min(samples), max(samples)]}
                             for side, samples in values.items()}}


def execute_trial(command: list[str], timeout: float):
    options = {"creationflags": subprocess.CREATE_NO_WINDOW} if sys.platform == "win32" else {}
    return subprocess.run(command, capture_output=True, text=True, encoding="utf-8",
                          errors="replace", timeout=timeout, check=False, **options)


def compare(options, runner=execute_trial) -> tuple[dict, int]:
    output = options.output.resolve()
    private = output.with_suffix(output.suffix + ".runs")
    if output.exists() or private.exists():
        raise ProbeNotObserved("output or private trial directory already exists; preserve earlier results")
    output.parent.mkdir(parents=True, exist_ok=True)
    private.mkdir()
    data = fixed_input(options.workload)
    expected_input = {"inputBytes": len(data), "inputHashAlgorithm": "fnv1a64", "inputHash": fnv1a64(data)}
    executables = {side: getattr(options, side).resolve() for side in ("before", "after")}
    report = {"schema": 1, "purpose": "local workload comparison; not QLT-014",
              "workload": options.workload, "iterations": options.iterations, "blocks": options.blocks,
              "warmupPerProcess": 1, "warmupMeasured": False, "timeoutSeconds": options.timeout,
              "input": {**expected_input, "sha256": hashlib.sha256(data).hexdigest()},
              "plannedOrder": list(ORDER) * options.blocks, "executables": {}, "runs": [],
              "status": "not-observed"}
    if options.workload.endswith(("delete-1mib", "delete-16mib")):
        report["input"]["sizeLabel"] = "approximate MiB; inputBytes is authoritative"
    try:
        for side, path in executables.items():
            report["executables"][side] = {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                                           "ref": getattr(options, side + "_ref"),
                                           "commit": getattr(options, side + "_commit")}
        for index, side in enumerate(report["plannedOrder"]):
            request = {"workload": options.workload, "iterations": options.iterations,
                       "commit": report["executables"][side]["commit"]}
            marks_path = private / f"{index:03d}-{side}.json"
            command = [str(executables[side]), options.workload, str(options.iterations), str(marks_path)]
            run = {"index": index, "side": side, "command": command, "timingReport": str(marks_path)}
            report["runs"].append(run)
            result = runner(command, options.timeout)
            run.update(returncode=result.returncode, stdout=result.stdout, stderr=result.stderr)
            if result.returncode != 0:
                raise ProbeNotObserved(f"{side} process failed: {result.returncode}")
            run["metadata"] = metadata_from(result.stdout, request, expected_input)
            run["microseconds"] = samples_from(load_json(marks_path.read_text(encoding="utf-8")), options.iterations)
            first = report["runs"][0]["metadata"]
            if any(run["metadata"][key] != first[key] for key in ("outputChecksum", "compilerVersion", "toolchainSha256")):
                raise ProbeNotObserved("output checksum or toolchain differs between trials")
        if any(hashlib.sha256(path.read_bytes()).hexdigest() != report["executables"][side]["sha256"]
               for side, path in executables.items()):
            raise ProbeNotObserved("an executable changed during comparison")
        report.update(status="observed", summary=summarize(report["runs"], options.blocks))
    except (OSError, ValueError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
        report["failure"] = f"{type(error).__name__}: {error}"
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return report, 0 if report["status"] == "observed" else 2


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for side in ("before", "after"):
        parser.add_argument("--" + side, type=Path, required=True)
        parser.add_argument("--" + side + "-ref", required=True)
        parser.add_argument("--" + side + "-commit", required=True)
    parser.add_argument("--workload", choices=WORKLOADS, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--iterations", type=int, default=20)
    parser.add_argument("--blocks", type=int, default=3)
    parser.add_argument("--timeout", type=float, default=60)
    options = parser.parse_args(argv)
    if not 1 <= options.iterations <= 2048 or not 1 <= options.blocks <= 100:
        parser.error("iterations must be 1..2048 and blocks 1..100")
    if not math.isfinite(options.timeout) or options.timeout <= 0:
        parser.error("timeout must be finite and positive")
    for side in ("before", "after"):
        if not re.fullmatch(r"[0-9a-f]{40}", getattr(options, side + "_commit")):
            parser.error(side + "-commit must be the full lowercase commit SHA")
    return options


def main(argv=None) -> int:
    try:
        report, code = compare(arguments(argv))
    except (OSError, ValueError) as error:
        print(f"Probes: not observed: {error}", file=sys.stderr)
        return 2
    if code:
        print(f"Probes: not observed: {report['failure']}", file=sys.stderr)
    else:
        print(f"Probes: observed; after/before median {report['summary']['ratioMedian']:.6f}; "
              "local comparison only, no acceptance threshold")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
