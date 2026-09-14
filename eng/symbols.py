"""Linker-level determinism, concurrency and dependency check (ARC-003 / ARC-007 / CPP-013).

A static library compiled from a canonical module may only leave the undefined
symbols listed in eng/symbol-allowlist.json after resolving against itself and
against the libraries of the modules it is allowed to depend on
(eng/architecture.json). Everything else is either a non-deterministic input
(ARC-007), a concurrency primitive outside the worker adapter (CPP-013), or an
undeclared dependency (ARC-003).
Python standard library only; the symbol table comes from llvm-nm.

Phase 0 TH2 measured the limit of this check: inline atomics leave no symbol, so
shared mutable state cannot be seen by the linker. CNF-009 in eng/conformance.py
covers that hole lexically.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path


def symbol_table(nm_output: str) -> tuple[set[str], set[str]]:
    """(defined external symbols, undefined symbols) of one object or archive."""
    defined, undefined = set(), set()
    for line in nm_output.splitlines():
        match = re.fullmatch(r"\s*(?:[0-9a-fA-F]+\s+)?([A-Za-z?-])\s+(\S+)", line)
        if not match:
            continue
        kind, symbol = match[1], match[2]
        if kind == "U":
            undefined.add(symbol)
        elif kind.isupper():
            defined.add(symbol)
    return defined, undefined


def undefined_symbols(nm_output: str) -> set[str]:
    return symbol_table(nm_output)[1]


def dependency_closure(module: str, modules: dict) -> set[str]:
    closure, pending = set(), list(modules.get(module, {}).get("dependencies", []))
    while pending:
        current = pending.pop()
        if current in closure or current not in modules:
            continue
        closure.add(current)
        pending.extend(modules[current]["dependencies"])
    return closure


def unresolved(module: str, tables: dict[str, tuple[set[str], set[str]]], modules: dict) -> set[str]:
    """Undefined symbols of a module that neither itself nor its declared dependencies define."""
    defined, undefined = tables[module]
    provided = set(defined)
    for dependency in dependency_closure(module, modules):
        if dependency in tables:
            provided |= tables[dependency][0]
    return undefined - provided


def classify(symbols: set[str], module: str, allowlist: dict) -> list[str]:
    allowed = [re.compile(p) for p in allowlist["modules"].get(module, [])]
    nondeterministic = [re.compile(p) for p in allowlist["nondeterministic"]]
    concurrency = [re.compile(p) for p in allowlist["concurrency"]]
    findings = []
    for symbol in sorted(symbols):
        bare = symbol[1:] if symbol.startswith("_") and not symbol.startswith("__") else symbol
        def matches(patterns: list[re.Pattern], name: str = symbol, stripped: str = bare) -> bool:
            return any(p.fullmatch(name) or p.fullmatch(stripped) for p in patterns)
        if matches(nondeterministic):
            findings.append(f"ARC-007: {module}: non-deterministic input symbol {symbol}")
        elif matches(concurrency):
            findings.append(f"CPP-013: {module}: concurrency symbol outside the worker adapter {symbol}")
        elif not matches(allowed):
            findings.append(f"ARC-003: {module}: undeclared external symbol {symbol}")
    return findings


def run_nm(path: Path) -> str:
    result = subprocess.run(["llvm-nm", str(path)], capture_output=True, text=True,
                            encoding="utf-8", errors="replace")
    if result.returncode:
        raise RuntimeError(f"llvm-nm failed for {path}\n{result.stdout}\n{result.stderr}")
    return result.stdout


def module_artifacts(root: Path, build_dir: Path, modules: dict) -> list[tuple[str, Path]]:
    reply = build_dir / ".cmake/api/v1/reply"
    indexes = sorted(reply.glob("index-*.json"))
    if not indexes:
        raise RuntimeError("ARC-002: CMake File API reply is missing")
    index = json.loads(indexes[-1].read_text(encoding="utf-8"))
    model = json.loads((reply / index["reply"]["codemodel-v2"]["jsonFile"]).read_text(encoding="utf-8"))
    artifacts = []
    for config in model["configurations"]:
        for entry in config["targets"]:
            target = json.loads((reply / entry["jsonFile"]).read_text(encoding="utf-8"))
            if target.get("type") != "STATIC_LIBRARY":
                continue
            sources = [s["path"].replace("\\", "/") for s in target.get("sources", [])]
            owners = {m for m, s in modules.items() if any(p.startswith(s["path"] + "/") for p in sources)}
            if len(owners) != 1:
                continue
            for artifact in target.get("artifacts", []):
                artifacts.append((next(iter(owners)), build_dir / artifact["path"]))
    return artifacts


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--object", type=Path, help="check one object or library instead of the build")
    parser.add_argument("--module", help="module name for --object")
    parser.add_argument("--require", nargs="*", default=[], help="modules that must be present in the build")
    args = parser.parse_args()
    root = args.root.resolve()
    allowlist = json.loads((root / "eng/symbol-allowlist.json").read_text(encoding="utf-8"))
    modules = json.loads((root / "eng/architecture.json").read_text(encoding="utf-8"))["modules"]
    if args.object:
        if not args.module:
            raise SystemExit("--object requires --module")
        targets = [(args.module, args.object)]
    else:
        if not args.build_dir:
            raise SystemExit("--build-dir or --object is required")
        targets = [(m, p) for m, p in module_artifacts(root, args.build_dir.resolve(), modules) if m in allowlist["modules"]]
    tables: dict[str, tuple[set[str], set[str]]] = {}
    for module, path in targets:
        defined, undefined = symbol_table(run_nm(path))
        previous = tables.get(module, (set(), set()))
        tables[module] = (previous[0] | defined, previous[1] | undefined)
    findings = []
    for module in tables:
        findings.extend(classify(unresolved(module, tables, modules), module, allowlist))
    for module in args.require:
        if module not in tables:
            findings.append(f"ARC-003: required module {module} has no static library in the build")
    for finding in findings:
        print(finding)
    print(f"Symbols: {len(targets)} librar{'y' if len(targets) == 1 else 'ies'} checked, {len(findings)} violation(s)")
    return int(bool(findings))


if __name__ == "__main__":
    raise SystemExit(main())
