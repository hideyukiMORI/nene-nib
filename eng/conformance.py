"""Repository conformance checks for NeNe Nib; Python standard library only (Issue #1)."""

from __future__ import annotations

import argparse
import datetime
import json
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path

RULE_ID = r"(?:ARC|CPP|QLT|CNF|GIT)-\d{3}"


@dataclass(frozen=True)
class Finding:
    rule: str
    path: str
    detail: str

    def __str__(self) -> str:
        return f"{self.rule}: {self.path}: {self.detail}"


def inventory(root: Path) -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        cwd=root, check=True, capture_output=True,
    )
    return sorted({Path(p) for p in result.stdout.decode("utf-8").split("\0") if p and (root / p).is_file()})


def cpp_code(text: str) -> str:
    # Preserve newlines while masking comments, quoted strings, and raw strings.
    pattern = r'R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\(.*?\)(?P=delimiter)"|//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    return re.sub(pattern, lambda m: re.sub(r"[^\n]", " ", m.group()), text, flags=re.S)


def primary_types(code: str) -> list[str]:
    """Names of class/struct/enum definitions that sit directly in a namespace (not nested)."""
    declarations = []
    scopes = []
    token = r"\bnamespace(?:\s+[A-Za-z_]\w*(?:::\w+)*)?\s*\{|\b(?:enum\s+class|enum\s+struct|class|struct|enum)\s+(\w+)\s*(?:final\s*)?(?::[^;{}]+)?\{|[{}]"
    for match in re.finditer(token, code):
        text = match[0]
        if text.startswith("namespace"):
            scopes.append("namespace")
        elif match[1]:
            if all(scope == "namespace" for scope in scopes):
                declarations.append(match[1])
            scopes.append("type")
        elif text == "{":
            scopes.append("other")
        elif scopes:
            scopes.pop()
    return declarations


def generic_name(name: str, suffixes: list[str]) -> bool:
    """CNF-001 folds snake_case and CamelCase to the same generic suffix."""
    folded = name.lower().replace("_", "")
    return any(folded.endswith(suffix) for suffix in suffixes)


def includes_of(text: str) -> list[str]:
    return re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', text, re.M)


def concurrency_and_simd(path: str, text: str, rules: dict) -> list[Finding]:
    """CNF-009: threads live only in the worker adapter; SIMD lives only in the SIMD module.

    tests/ is deliberately out of scope: that is where intentional violations are written.
    """
    if not path.startswith("src/"):
        return []
    findings = []
    for include in includes_of(text):
        name = Path(include).name
        if name in rules["concurrencyHeaders"] and not path.startswith(rules["concurrencyAllowedPath"]):
            findings.append(Finding("CPP-013", path, f"concurrency header {include} outside {rules['concurrencyAllowedPath']}"))
        if name in rules["simdHeaders"] and not path.startswith(rules["simdModulePath"]):
            findings.append(Finding("CPP-018", path, f"SIMD header {include} outside {rules['simdModulePath']}"))
    return findings


def source_checks(path: str, text: str, rules: dict, waivers: dict) -> list[Finding]:
    findings = []
    code = cpp_code(text.replace("\\\n", ""))
    production = path.startswith("src/")
    if production:
        for part in Path(path).parts:
            if part.lower() in rules["moduleNames"]:
                findings.append(Finding("CNF-001", path, f"forbidden module name {part}"))
        declarations = re.findall(r"\b(?:enum\s+class|enum\s+struct|class|struct|enum|using)\s+([A-Za-z_]\w*)", code)
        for name in declarations:
            if generic_name(name, rules["typeSuffixes"]):
                findings.append(Finding("CNF-001", path, f"forbidden type name {name}"))
        definitions = primary_types(code)
        if len(definitions) > 1:
            findings.append(Finding("CNF-002", path, "multiple type definitions; review primary/nested declarations"))
        if definitions and definitions[0] != Path(path).stem:
            findings.append(Finding("CNF-002", path, f"filename does not match {definitions[0]}"))
    if not path.startswith("src/adapters/win32/"):
        identifiers = set(re.findall(r"\b[A-Za-z_]\w*\b", code))
        banned = identifiers.intersection(rules["nondeterministicNames"])
        if banned:
            findings.append(Finding("ARC-007", path, "forbidden input API: " + ", ".join(sorted(banned))))
    if path.startswith(("src/core/", "src/application/")):
        for include in includes_of(text):
            if Path(include).name in rules["platformHeaders"]:
                findings.append(Finding("ARC-003", path, f"platform header {include}"))
    findings.extend(concurrency_and_simd(path, text, rules))
    raw_lines = text.splitlines()
    code_lines = cpp_code(text).splitlines()
    for number, line in enumerate(raw_lines, 1):
        code_line = code_lines[number - 1] if number <= len(code_lines) else ""
        comment = line[line.index("//"):] if "//" in line else ""
        if re.search(rules["suppressionPragmaPattern"], code_line) or re.search(rules["broadSuppressionPattern"], comment):
            findings.append(Finding("CNF-003", path, f"line {number}: broad suppression is forbidden"))
        if rules["lineSuppressionToken"] in comment:
            previous = raw_lines[number - 2] if number > 1 else ""
            match = re.fullmatch(r"\s*// Waiver: (WVR-\d{4})\s*", previous)
            waiver = waivers.get(match[1]) if match else None
            if not waiver or not re.search(rules["lineSuppressionPattern"], line) or waiver["scope"].split("#")[0] != path:
                findings.append(Finding("CNF-003", path, f"line {number}: missing valid, scoped waiver"))
    return findings


def waiver_checks(root: Path, paths: list[Path], today: datetime.date) -> tuple[list[Finding], dict]:
    findings, valid = [], {}
    index = root / "docs/waivers/README.md"
    index_text = index.read_text(encoding="utf-8") if index.exists() else ""
    seen = set()
    for path in paths:
        name = path.as_posix()
        if not name.startswith("docs/waivers/") or path.name in {"README.md", "0000-template.md"}:
            continue
        match = re.fullmatch(r"(WVR-\d{4})-[a-z0-9]+(?:-[a-z0-9]+)*\.md", path.name)
        if not match:
            findings.append(Finding("CNF-004", name, "invalid waiver filename"))
            continue
        identifier = match[1]
        initial_errors = len(findings)
        text = (root / path).read_text(encoding="utf-8")
        fields = dict(re.findall(r"^- (\w+):\s*(.+)$", text, re.M))
        required = {"Status", "Rule", "Issue", "Owner", "Created", "Expires", "Scope"}
        if not required.issubset(fields) or identifier in seen:
            findings.append(Finding("CNF-004", name, "missing fields or duplicate waiver ID"))
            continue
        seen.add(identifier)
        if fields["Status"] not in {"proposed", "active", "expired", "removed"}:
            findings.append(Finding("CNF-004", name, "unknown status"))
        if not re.fullmatch(r"#[1-9]\d*", fields["Issue"]) or not re.fullmatch(r"`?" + RULE_ID + r"`?", fields["Rule"]):
            findings.append(Finding("CNF-004", name, "invalid Issue or Rule"))
        scope = fields["Scope"].strip("`")
        scope_path = scope.split("#")[0]
        if "#" not in scope or any(c in scope for c in "*?\\") or ".." in Path(scope_path).parts or Path(scope_path).is_absolute() or not (root / scope_path).is_file():
            findings.append(Finding("CNF-004", name, "scope must name an existing repository file and declaration"))
        try:
            created = datetime.date.fromisoformat(fields["Created"])
            expires = datetime.date.fromisoformat(fields["Expires"])
            if expires < created or created > today:
                raise ValueError("expiry precedes creation")
        except ValueError:
            findings.append(Finding("CNF-004", name, "invalid date interval"))
            continue
        if fields["Status"] == "active":
            if expires < today:
                findings.append(Finding("CNF-004", name, "expired active waiver"))
            elif identifier not in index_text or path.name not in index_text:
                findings.append(Finding("CNF-004", name, "active waiver missing from index"))
            elif len(findings) == initial_errors:
                valid[identifier] = {"scope": scope, "rule": fields["Rule"].strip("`")}
    for identifier in set(re.findall(r"\bWVR-\d{4}\b", index_text)):
        if identifier not in valid:
            findings.append(Finding("CNF-004", "docs/waivers/README.md", f"index contains non-active waiver {identifier}"))
    return findings, valid


def document_checks(root: Path, paths: list[Path], rules: dict) -> list[Finding]:
    findings, definitions, statuses = [], {}, {}
    for filename in rules["normativeFiles"]:
        path = root / filename
        if not path.is_file():
            findings.append(Finding("CNF-006", filename, "missing normative document"))
            continue
        text = path.read_text(encoding="utf-8")
        for match in re.finditer(r"^### (" + RULE_ID + r")\b(.*?)(?=^### |\Z)", text, re.M | re.S):
            identifier = match[1]
            if identifier in definitions:
                findings.append(Finding("CNF-006", filename, f"duplicate definition {identifier}"))
            definitions[identifier] = filename
            status = re.search(r"機械強制:\s*\*\*(active|planned|不能|不採用)\*\*", match[2])
            if not status:
                findings.append(Finding("CNF-006", filename, f"missing enforcement state {identifier}"))
            else:
                statuses[identifier] = status[1]
    gate_path = root / "docs/QUALITY_GATES.md"
    gate = gate_path.read_text(encoding="utf-8") if gate_path.exists() else ""
    matrix_part = gate.split("## 3. 強制マトリクス", 1)[-1].split("## 4.", 1)[0]
    rows = re.findall(r"^\| (" + RULE_ID + r") \| (active|planned|不能|不採用) \|", matrix_part, re.M)
    matrix = dict(rows)
    if len(matrix) != len(rows):
        findings.append(Finding("CNF-006", "docs/QUALITY_GATES.md", "duplicate matrix row"))
    for identifier in definitions.keys() | matrix.keys():
        if identifier not in definitions or identifier not in matrix or statuses.get(identifier) != matrix.get(identifier):
            findings.append(Finding("CNF-006", "docs/QUALITY_GATES.md", f"definition/matrix mismatch {identifier}"))
    proof_file = root / "docs/quality/gate-proofs.md"
    proofs = proof_file.read_text(encoding="utf-8") if proof_file.exists() else ""
    for identifier, status in matrix.items():
        if status == "active" and not re.search(r"^\| " + identifier + r" \|", proofs, re.M):
            findings.append(Finding("CNF-006", "docs/quality/gate-proofs.md", f"missing active proof row {identifier}"))
    for path in paths:
        if path.suffix != ".md":
            continue
        text = (root / path).read_text(encoding="utf-8")
        name = path.as_posix()
        if re.search(r"\{\{.*?\}\}", text):
            findings.append(Finding("CNF-006", name, "unresolved template placeholder"))
        for identifier in set(re.findall(r"\b" + RULE_ID + r"\b", text)):
            if identifier not in definitions:
                findings.append(Finding("CNF-006", name, f"undefined rule {identifier}"))
        for link in re.findall(r"\]\(([^)]+)\)", text):
            if re.match(r"[a-z]+:|#", link, re.I):
                continue
            target = link.split("#")[0]
            if target and not (root / path.parent / target).exists():
                findings.append(Finding("CNF-006", name, f"broken relative link {target}"))
    return findings


def configuration_checks(root: Path, paths: list[Path], rules: dict) -> list[Finding]:
    findings = []
    binding_file = root / "eng/config-bindings.json"
    bindings = json.loads(binding_file.read_text(encoding="utf-8"))
    for filename, consumers in bindings.items():
        if not (root / filename).is_file():
            findings.append(Finding("CNF-007", filename, "declared configuration is missing"))
        for consumer in consumers:
            consumer_path = root / consumer
            if not consumer_path.is_file() or Path(filename).name not in consumer_path.read_text(encoding="utf-8"):
                findings.append(Finding("CNF-007", filename, f"binding missing in {consumer}"))
    for path in paths:
        name = path.as_posix()
        if any(word in path.name.lower() for word in rules["forbiddenFileWords"]):
            findings.append(Finding("CNF-005", name, "forbidden configuration filename"))
        if path.name in {".clang-tidy", ".clang-format"} and name not in bindings:
            findings.append(Finding("CNF-007", name, "unregistered tool configuration"))
        if path.suffix in rules["configurationExtensions"] or path.name in {"CMakeLists.txt", ".clang-tidy"}:
            text = (root / path).read_text(encoding="utf-8")
            for pattern in rules["disabledGatePatterns"]:
                if re.search(pattern, text):
                    findings.append(Finding("CNF-005", name, "gate disabling option"))
        if path.suffix in {".cpp", ".hpp", ".h", ".ps1", ".py", ".cmake"} and not name.startswith("tests/conformance/"):
            for number, line in enumerate((root / path).read_text(encoding="utf-8").splitlines(), 1):
                if re.search(rules["taskMarkerPattern"], line) and not re.search(r"#[1-9]\d*", line):
                    findings.append(Finding("CNF-008", name, f"line {number}: task marker needs an Issue number"))
    return findings


def architecture_checks(root: Path, paths: list[Path], build_dir: Path | None) -> list[Finding]:
    findings = []
    graph = json.loads((root / "eng/architecture.json").read_text(encoding="utf-8"))
    modules = graph["modules"]
    if graph["runtimeDependencies"]:
        findings.append(Finding("QLT-011", "eng/architecture.json", "runtime dependencies require an ADR, pinned lock, and gate implementation"))
    for module, settings in modules.items():
        if module in settings["dependencies"] or any(d not in modules for d in settings["dependencies"]):
            findings.append(Finding("ARC-002", "eng/architecture.json", f"invalid dependency list for {module}"))
    def visit(node: str, stack: tuple[str, ...]) -> None:
        if node in stack:
            findings.append(Finding("ARC-002", "eng/architecture.json", "cyclic module graph"))
            return
        for child in modules[node]["dependencies"]:
            if child in modules:
                visit(child, stack + (node,))
    for module in modules:
        visit(module, ())
    def owner(source: str) -> str | None:
        return next((m for m, s in modules.items() if source.startswith(s["path"] + "/")), None)
    source_paths = {p.as_posix() for p in paths if p.suffix in {".cpp", ".hpp", ".h", ".cc", ".cxx", ".hxx"}}
    for path in source_paths:
        if path.startswith("src/") and owner(path) is None:
            findings.append(Finding("ARC-002", path, "source has no approved module"))
        if not path.startswith("src/"):
            continue
        source_owner = owner(path)
        for include in re.findall(r'^\s*#\s*include\s*"([^"]+)"', (root / path).read_text(encoding="utf-8"), re.M):
            resolved = (root / Path(path).parent / include).resolve()
            candidates = [resolved] + [(root / s["path"] / include).resolve() for s in modules.values()]
            for candidate in candidates:
                if not candidate.is_relative_to(root.resolve()):
                    continue
                destination = owner(candidate.relative_to(root.resolve()).as_posix())
                if candidate.is_file() and source_owner and destination and destination != source_owner and destination not in modules[source_owner]["dependencies"]:
                    findings.append(Finding("ARC-002", path, f"forbidden include {include}"))
    if build_dir is None:
        return findings
    reply = build_dir / ".cmake/api/v1/reply"
    indexes = sorted(reply.glob("index-*.json"))
    if not indexes:
        return [Finding("ARC-002", str(reply), "CMake File API reply is missing")]
    index = json.loads(indexes[-1].read_text(encoding="utf-8"))
    model = json.loads((reply / index["reply"]["codemodel-v2"]["jsonFile"]).read_text(encoding="utf-8"))
    compiled = set()
    for config in model["configurations"]:
        targets = {}
        for entry in config["targets"]:
            target = json.loads((reply / entry["jsonFile"]).read_text(encoding="utf-8"))
            sources = [s["path"].replace("\\", "/") for s in target.get("sources", []) if not s.get("isGenerated")]
            owners = {owner(s) for s in sources}
            if len(owners) != 1 or None in owners:
                findings.append(Finding("ARC-002", entry["name"], "target has unowned or mixed-module sources"))
            targets[entry["id"]] = (target, next(iter(owners)) if len(owners) == 1 else None)
            compiled.update(s for s in sources if Path(s).suffix in {".cpp", ".cc", ".cxx"})
        for target, module in targets.values():
            if module is None:
                continue
            for dependency in target.get("dependencies", []):
                destination = targets.get(dependency["id"], ({}, None))[1]
                if destination not in modules[module]["dependencies"]:
                    findings.append(Finding("ARC-002", target["name"], f"forbidden target dependency {destination}"))
    for source in source_paths:
        if source.startswith(("src/", "tests/build/", "tests/unit/")) and Path(source).suffix in {".cpp", ".cc", ".cxx"} and source not in compiled:
            findings.append(Finding("ARC-002", source, "translation unit is absent from the build"))
    return findings


def check(root: Path, today: datetime.date, build_dir: Path | None = None) -> list[Finding]:
    paths = inventory(root)
    rules = json.loads((root / "eng/conformance-rules.json").read_text(encoding="utf-8"))
    findings, waivers = waiver_checks(root, paths, today)
    findings.extend(document_checks(root, paths, rules))
    findings.extend(configuration_checks(root, paths, rules))
    findings.extend(architecture_checks(root, paths, build_dir))
    for path in paths:
        if path.suffix in rules["cppExtensions"]:
            findings.extend(source_checks(path.as_posix(), (root / path).read_text(encoding="utf-8"), rules, waivers))
    return findings


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--today", type=datetime.date.fromisoformat)
    args = parser.parse_args()
    today = args.today or datetime.datetime.now(datetime.timezone.utc).date()
    findings = check(args.root.resolve(), today, args.build_dir)
    for finding in findings:
        print(finding)
    print(f"Conformance: {len(findings)} violation(s)")
    return int(bool(findings))


if __name__ == "__main__":
    raise SystemExit(main())
