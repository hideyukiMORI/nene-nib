"""Measure the canonical core sources with LLVM; prove QLT-009 using real profiles.

The instrumented build compiles the same core / application sources and the same
unit tests with clang-cl (-fprofile-instr-generate -fcoverage-mapping). It is a
measurement build for verification, not a second product build (ADR 0003).
The unit test executable is run twice: once with --coverage-negative (which skips
most tests and must fail the threshold) and once completely (which must pass).

Issue #1 does not wire this into eng/check.ps1: there is no core / application
source to measure yet, so QLT-009 stays planned. The self-tests in
tests/conformance/test_coverage.py run from the gate, and Phase 3 wires the rest.
Python standard library only; ported from NeNe Loupe eng/coverage.py.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess

from conformance import inventory


def assess(report: dict, root: Path, required: set[Path], scopes: list[Path], minimum: int) -> dict:
    measured = {}
    for unit in report["data"]:
        for item in unit["files"]:
            path = Path(item["filename"]).resolve()
            if any(path.is_relative_to(scope) for scope in scopes):
                if path in measured:
                    raise ValueError("QLT-009: duplicate coverage file")
                measured[path] = item["summary"]["branches"]
    missing = required - measured.keys()
    if missing:
        raise ValueError("QLT-009: missing translation units: " + ", ".join(str(p) for p in sorted(missing)))
    count = sum(item["count"] for item in measured.values())
    covered = sum(item["covered"] for item in measured.values())
    if not required or not count:
        raise ValueError("QLT-009: empty measurement")
    if any(not 0 <= item["covered"] <= item["count"] for item in measured.values()):
        raise ValueError("QLT-009: invalid branch counts")
    result = {"branches": count, "covered": covered, "percent": 100 * covered / count,
              "minimum": minimum,
              "files": {p.relative_to(root).as_posix(): v for p, v in sorted(measured.items())}}
    if 100 * covered < minimum * count:
        raise ValueError(f"QLT-009: branch coverage {result['percent']:.2f}% < {minimum}%")
    return result


def run(command: list[str], cwd: Path, **kwargs) -> subprocess.CompletedProcess:
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, encoding="utf-8",
                            errors="replace", **kwargs)
    if result.returncode:
        raise RuntimeError(f"{command[0]} failed ({result.returncode})\n{result.stdout}\n{result.stderr}")
    return result


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    policy = json.loads((root / "eng/coverage-policy.json").read_text(encoding="utf-8"))
    architecture = json.loads((root / "eng/architecture.json").read_text(encoding="utf-8"))
    scopes = [(root / architecture["modules"][name]["path"]).resolve() for name in policy["modules"]]
    test_scope = (root / architecture["modules"][policy["testModule"]]["path"]).resolve()
    all_paths = [(root / p).resolve() for p in inventory(root)]
    sources = sorted(p for p in all_paths if p.suffix == ".cpp" and any(p.is_relative_to(s) for s in scopes))
    tests = sorted(p for p in all_paths if p.suffix == ".cpp" and p.is_relative_to(test_scope))
    if not sources or not tests:
        raise ValueError("QLT-009: no canonical sources or tests")
    commands = json.loads((root / "build/compile_commands.json").read_text(encoding="utf-8"))
    compiled = {Path(c["file"]).resolve() for c in commands}
    if not set(sources + tests).issubset(compiled):
        raise ValueError("QLT-009: measurement source absent from canonical build")
    output = root / "out/coverage"
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "nib_coverage.exe"
    command = ["clang-cl", "/nologo", "/clang:-std=c++23", "/EHsc", "/MT", "/W4", "/WX", "/utf-8",
               "/clang:-fprofile-instr-generate", "/clang:-fcoverage-mapping", f"/Fe:{executable}"]
    command += [f"/I{scope}" for scope in scopes]
    command += [str(p) for p in sources + tests]
    run(command, output)
    results = {}
    for scenario, arguments in (("negative", ["--coverage-negative"]), ("complete", [])):
        raw = output / f"{scenario}.profraw"
        raw.unlink(missing_ok=True)
        profile = output / f"{scenario}.profdata"
        run([str(executable), *arguments], output, env={**os.environ, "LLVM_PROFILE_FILE": str(raw)})
        run(["llvm-profdata", "merge", "-sparse", str(raw), "-o", str(profile)], output)
        exported = run(["llvm-cov", "export", str(executable), f"-instr-profile={profile}"], output)
        (output / f"{scenario}.json").write_text(exported.stdout, encoding="utf-8")
        try:
            result = assess(json.loads(exported.stdout), root, set(sources), scopes, policy["minimumBranchPercent"])
        except ValueError as error:
            if scenario != "negative" or not str(error).startswith("QLT-009: branch coverage"):
                raise
            results[scenario] = str(error)
        else:
            if scenario == "negative":
                raise ValueError("QLT-007: incomplete test execution did not fail QLT-009")
            results[scenario] = result
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(results, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
