"""Measure the canonical core sources with LLVM; prove QLT-009 using real profiles.

The instrumented build compiles the translation units of the canonical unit test
target (nib_tests) and of the modules it links, each with the flags the canonical
Debug build gives that translation unit in build/compile_commands.json (defines,
include directories, language standard, warnings), plus clang-cl's
-fprofile-instr-generate -fcoverage-mapping. Sanitizers, debug information and the
canonical output paths are left out. It is a measurement build for verification,
not a second product build (ADR 0003), and it holds no flags of its own (ARC-001 /
Issue #1921: a definition CMake gives one translation unit reaches it here too).
The unit test executable is run twice: once with --coverage-negative (which skips
most tests and must fail the threshold) and once completely (which must pass).

eng/check.ps1 -Full runs it after configuring build/. The command assembly is
proved without a build by tests/conformance/test_coverage.py (eng/test-conformance.py
and the CI check job), so a flag the measurement build drops is seen there too.
Python standard library only; ported from NeNe Loupe eng/coverage.py.
"""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import subprocess

from conformance import inventory

# The executable that owns --coverage-negative (tests/unit/NibTests.cpp). Its translation units
# are the ones whose canonical object lies under CMakeFiles/<target>.dir/ (Ninja's layout).
TEST_TARGET = "nib_tests"
INSTRUMENTATION = ["/clang:-fprofile-instr-generate", "/clang:-fcoverage-mapping"]
# Canonical flags the measurement build does not keep: the compile-only switch, debug information,
# the sanitizers (their runtime does not link with the profile runtime) and the canonical outputs.
DROPPED_FLAGS = {"-c", "/c", "-Zi", "/Zi"}
DROPPED_PREFIXES = ("-Fo", "/Fo", "-Fd", "/Fd", "-fsanitize", "-fno-sanitize", "/fsanitize")


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


def split_command(line: str) -> list[str]:
    """Split a Windows command line the way the C runtime does (CMake writes clang-cl's this way)."""
    arguments, current, started, quoted, index = [], [], False, False, 0
    while index < len(line):
        character = line[index]
        if character == "\\":
            end = index
            while end < len(line) and line[end] == "\\":
                end += 1
            if end < len(line) and line[end] == '"':
                current.append("\\" * ((end - index) // 2))
                if (end - index) % 2:
                    current.append('"')
                    end += 1
            else:
                current.append("\\" * (end - index))
            index, started = end, True
        elif character == '"':
            quoted, started, index = not quoted, True, index + 1
        elif character in " \t" and not quoted:
            if started:
                arguments.append("".join(current))
            current, started, index = [], False, index + 1
        else:
            current.append(character)
            started, index = True, index + 1
    if started:
        arguments.append("".join(current))
    return arguments


def entry_arguments(entry: dict) -> list[str]:
    return list(entry["arguments"]) if "arguments" in entry else split_command(entry["command"])


def measured_command(arguments: list[str], obj: Path) -> list[str]:
    """The canonical compile of one translation unit, instrumented and written to obj."""
    if "--" not in arguments:
        raise ValueError("QLT-009: canonical compile command has no '--' before its source: " + " ".join(arguments))
    split = arguments.index("--")
    sources = arguments[split + 1:]
    if len(sources) != 1:
        raise ValueError("QLT-009: canonical compile command must name one source: " + " ".join(arguments))
    kept = [a for a in arguments[1:split] if a not in DROPPED_FLAGS and not a.startswith(DROPPED_PREFIXES)]
    return [arguments[0], *kept, *INSTRUMENTATION, "/c", f"/Fo{obj}", "--", sources[0]]


def closure(names: list[str], modules: dict) -> set[str]:
    found: set[str] = set()
    pending = list(names)
    while pending:
        name = pending.pop()
        if name not in found:
            found.add(name)
            pending += modules[name]["dependencies"]
    return found


def translation_units(commands: list[dict], root: Path, target: str, libraries: list[Path]) -> dict[Path, list[str]]:
    """Each translation unit of target and of the libraries it links, with its canonical arguments."""
    units: dict[Path, list[str]] = {}
    for entry in commands:
        path = Path(entry["file"]).resolve()
        output = entry.get("output", "").replace("\\", "/")
        if output.startswith(f"CMakeFiles/{target}.dir/") or any(path.is_relative_to(p) for p in libraries):
            if path in units:
                raise ValueError(f"QLT-009: {path.relative_to(root)} has two canonical compile commands")
            units[path] = entry_arguments(entry)
    return units


def run(command: list[str], cwd: Path, **kwargs) -> subprocess.CompletedProcess:
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, encoding="utf-8",
                            errors="replace", **kwargs)
    if result.returncode:
        raise RuntimeError(f"{command[0]} failed ({result.returncode})\n{result.stdout}\n{result.stderr}")
    return result


def build(units: dict[Path, list[str]], root: Path, output: Path, executable: Path, jobs: int) -> None:
    objects = {path: output / "obj" / (path.relative_to(root).as_posix().replace("/", "__") + ".obj")
               for path in units}
    (output / "obj").mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        failures = [error for error in pool.map(
            lambda p: compile_one(measured_command(units[p], objects[p]), output), sorted(units)) if error]
    if failures:
        raise RuntimeError(f"QLT-009: {len(failures)} translation unit(s) failed to compile\n" + "\n".join(failures[:5]))
    response = output / "link.rsp"
    response.write_text("\n".join(f'"{o}"' for o in sorted(objects.values())) + "\n", encoding="utf-8")
    compiler = next(iter(units.values()))[0]
    run([compiler, "/nologo", *INSTRUMENTATION[:1], f"/Fe:{executable}", f"@{response}"], output)


def compile_one(command: list[str], cwd: Path) -> str | None:
    try:
        run(command, cwd)
    except RuntimeError as error:
        return str(error)
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1, help="parallel compiles")
    options = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    policy = json.loads((root / "eng/coverage-policy.json").read_text(encoding="utf-8"))
    modules = json.loads((root / "eng/architecture.json").read_text(encoding="utf-8"))["modules"]
    scopes = [(root / modules[name]["path"]).resolve() for name in policy["modules"]]
    linked = closure(modules[policy["testModule"]]["dependencies"], modules)
    if not set(policy["modules"]) <= linked:
        raise ValueError("QLT-009: the test module does not link every measured module")
    libraries = [(root / modules[name]["path"]).resolve() for name in sorted(linked)]
    all_paths = [(root / p).resolve() for p in inventory(root)]
    sources = sorted(p for p in all_paths if p.suffix == ".cpp" and any(p.is_relative_to(s) for s in scopes))
    commands = json.loads((root / "build/compile_commands.json").read_text(encoding="utf-8"))
    units = translation_units(commands, root, TEST_TARGET, libraries)
    test_scope = (root / modules[policy["testModule"]]["path"]).resolve()
    if not sources or not any(p.is_relative_to(test_scope) for p in units):
        raise ValueError("QLT-009: no canonical sources or tests")
    if not set(sources).issubset(units):
        raise ValueError("QLT-009: measurement source absent from canonical build")
    output = root / "out/coverage"
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "nib_coverage.exe"
    build(units, root, output, executable, max(1, options.jobs))
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
