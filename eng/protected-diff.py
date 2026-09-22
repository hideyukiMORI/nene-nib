"""Report what a change did to the protected things between two refs (Issue #130).

ADR 0038 decision 5 and its table ("protected things"): a change must not delete or alter the
Vim fixtures, the machine references, the symbol allowlist, the conformance rules or the saved
settings schema without saying so, and the number of checks of an existing nib_tests scope must
not move behind the author's back. This script turns that "0 difference" claim into a record
that a PR can paste instead of a sentence.

  python eng/protected-diff.py --base <ref> [--head <ref>] [--allow <scope> ...]
                               [--base-tests <exe>] [--head-tests <exe>] [--build]

(1) Vim fixtures: tests/vim/fixtures.json is compared element by element (keyed by name, so a
    formatting change is not a difference) and the lines of tests/vim/VimFixtures.hpp are split
    into metadata (the sha256 line and the array size line), deleted, changed and added
    fixtures. Any deleted or changed fixture ends with 1 and lists its name and line number.
(2) The other protected files are reported as changed or unchanged only. A change that comes
    with an ADR is legitimate, so this never fails on them; the design seat reads the record.
(3) nib_tests: the scope list is read statically from each ref's tests/unit/NibTests.cpp (the
    exe has no way to list them), every scope is run once, and a scope whose number of checks
    moved ends with 1 unless it is named by --allow. A scope new in head is recorded only.
    Without an exe (no --base-tests / --head-tests and no --build) the counts are "未測" and
    nothing fails; build/nib_tests.exe is never used because it may be older than HEAD.
    If the scopes array cannot be read from base or head, the checks cannot be compared at all,
    so the record carries an "error" after (1) and (2) and the exit is 2: an empty comparison
    must not look like a success (the same rule as Issue #131).

--build makes a Debug nib_tests per ref in build/protected-<short sha>, the way
eng/build-release.ps1 does (eng/toolchain.ps1, a temporary build/worktree-<short sha> when the
ref is not the clean HEAD). An existing build/protected-<short sha>/nib_tests.exe is reused.
The default build/ and build-release/ are not touched. The record is written to
out/protected/<short head>.json. Exit: 0 clean, 1 protected difference, 2 unknown ref or
no scopes array.
Not a gate (QLT-010): it is run by hand and its output is pasted into the PR record.
"""

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "out/protected"
FIXTURES_JSON = "tests/vim/fixtures.json"
FIXTURES_HEADER = "tests/vim/VimFixtures.hpp"
UNIT_TESTS = "tests/unit/NibTests.cpp"
PROTECTED_FILES = (
    "eng/perf-reference.json",
    "eng/symbol-allowlist.json",
    "eng/conformance-rules.json",
    "src/adapters/win32/SettingsCodec.hpp",
    "src/adapters/win32/SettingsCodec.cpp",
)
UNMEASURED = "未測"
METADATA_LINES = (re.compile(r"^// fixtures\.json: sha256 [0-9a-f]{64} / \d+ fixtures$"),
                  re.compile(r"^constexpr std::array<VimFixture, \d+> vim_fixtures\{\{$"))
FIXTURE_LINE = re.compile(r'^\s*\{"((?:[^"\\]|\\.)*)",')
HUNK = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@")
SCOPES_BLOCK = re.compile(r"std::array<std::pair<std::string_view, void \(\*\)\(\)>, \d+> scopes\{\{(.*?)\}\};",
                          re.DOTALL)
SCOPE_ENTRY = re.compile(r'\{"(--[^"]+)",')
PASSED = re.compile(r"Nib unit tests passed: (\d+) checks")
FAILED = re.compile(r"Nib unit tests: (\d+) of (\d+) checks failed")


def git(*arguments: str, check: bool = True) -> subprocess.CompletedProcess:
    """git の出力は日本語を含むので、読む側の符号化をここ 1 か所で UTF-8 に固定する（Issue #106）。"""
    result = subprocess.run(["git", "-C", str(ROOT), *arguments], capture_output=True,
                            encoding="utf-8", errors="replace", check=False)
    if check and result.returncode != 0:
        raise RuntimeError(f"git {' '.join(arguments)}: {result.stderr.strip()}")
    return result


def resolve(ref: str) -> str | None:
    result = git("rev-parse", "--verify", "--quiet", f"{ref}^{{commit}}", check=False)
    return result.stdout.strip() if result.returncode == 0 and result.stdout.strip() else None


def show(commit: str, path: str) -> str | None:
    result = git("show", f"{commit}:{path}", check=False)
    return result.stdout if result.returncode == 0 else None


def classify_header_diff(diff: str) -> dict:
    """`git diff -U0` の VimFixtures.hpp を metadata・削除・変更・追加に分ける純関数。

    metadata は sha256 の行と配列の大きさの行の 2 種類だけ。同じ name の `-` と `+` が
    両方あれば変更（期待値の書き換え）、`-` だけなら削除、`+` だけなら追加。fixture の形を
    していない `-` 行も削除に数える（見逃しを 0 に寄せる）。行番号は削除なら base 側、
    変更と追加なら head 側。
    """
    removed: list[tuple[int, str]] = []
    added: list[tuple[int, str]] = []
    base_line = head_line = 0
    for line in diff.splitlines():
        hunk = HUNK.match(line)
        if hunk:
            base_line, head_line = int(hunk.group(1)), int(hunk.group(3))
            continue
        if line.startswith(("---", "+++")):
            continue
        if line.startswith("-"):
            removed.append((base_line, line[1:]))
            base_line += 1
        elif line.startswith("+"):
            added.append((head_line, line[1:]))
            head_line += 1
    metadata = [text for _, text in removed if any(p.match(text) for p in METADATA_LINES)]
    removed = [(number, text) for number, text in removed
               if not any(p.match(text) for p in METADATA_LINES)]
    added = [(number, text) for number, text in added
             if not any(p.match(text) for p in METADATA_LINES)]

    def name_of(text: str) -> str | None:
        match = FIXTURE_LINE.match(text)
        return match.group(1) if match else None

    added_names = {name_of(text): number for number, text in added if name_of(text) is not None}
    removed_names = {name_of(text) for _, text in removed}
    deleted, changed = [], []
    for number, text in removed:
        name = name_of(text)
        if name is not None and name in added_names:
            changed.append({"name": name, "line": added_names[name]})
        else:
            deleted.append({"name": name if name is not None else text.strip(), "line": number})
    new = [{"name": name, "line": number} for name, number in added_names.items()
           if name not in removed_names]
    return {"metadata": len(metadata), "deleted": deleted, "changed": changed, "added": new}


def compare_fixtures(base: list[dict], head: list[dict]) -> dict:
    """fixtures.json を name をキーに要素で比べる純関数（整形の差は差にならない）。"""
    before = {item["name"]: item for item in base}
    after = {item["name"]: item for item in head}
    deleted = [name for name in before if name not in after]
    changed = [name for name in before if name in after
               and (before[name].get("text"), before[name].get("keys"))
               != (after[name].get("text"), after[name].get("keys"))]
    added = [name for name in after if name not in before]
    return {"deleted": deleted, "changed": changed, "added": added}


def header_lines(text: str | None) -> dict[str, int]:
    lines: dict[str, int] = {}
    for number, line in enumerate((text or "").splitlines(), start=1):
        match = FIXTURE_LINE.match(line)
        if match:
            lines.setdefault(match.group(1), number)
    return lines


def parse_scopes(source: str | None) -> list[str]:
    """NibTests.cpp の `scopes` 配列から `--…` の名前だけを静的に読む純関数。"""
    block = SCOPES_BLOCK.search(source or "")
    return SCOPE_ENTRY.findall(block.group(1)) if block else []


def scopes_error(scopes: dict[str, list[str]]) -> str | None:
    """ref ごとの scope の一覧のどれかが空なら「測れない」の理由を返す純関数。空の比較を成功に見せない。"""
    for ref, names in scopes.items():
        if not names:
            return f"scopes not found in {ref}:{UNIT_TESTS}"
    return None


def fixtures_section(base: str, head: str) -> dict:
    diff = git("diff", "-U0", base, head, "--", FIXTURES_HEADER).stdout
    header = classify_header_diff(diff)
    base_json = json.loads(show(base, FIXTURES_JSON) or "[]")
    head_json = json.loads(show(head, FIXTURES_JSON) or "[]")
    elements = compare_fixtures(base_json, head_json)
    base_lines = header_lines(show(base, FIXTURES_HEADER))
    head_lines = header_lines(show(head, FIXTURES_HEADER))
    deleted = {entry["name"]: entry["line"] for entry in header["deleted"]}
    for name in elements["deleted"]:
        deleted.setdefault(name, base_lines.get(name))
    changed = {entry["name"]: entry["line"] for entry in header["changed"]}
    for name in elements["changed"]:
        changed.setdefault(name, head_lines.get(name))
    return {
        "counts": {"base": len(base_json), "head": len(head_json)},
        "metadata": header["metadata"],
        "deleted": [{"name": name, "line": line, "side": "base"} for name, line in deleted.items()],
        "changed": [{"name": name, "line": line, "side": "head"} for name, line in changed.items()],
        "added": len(set(elements["added"]) | {entry["name"] for entry in header["added"]}),
    }


def protected_section(base: str, head: str) -> dict:
    return {path: ("changed" if git("diff", "--quiet", base, head, "--", path,
                                    check=False).returncode != 0 else "unchanged")
            for path in PROTECTED_FILES}


def run_quiet(command: list[str], log: Path) -> int:
    """build の出力は端末に流さずログへ落とす（ADR 0039 の手 3）。"""
    with log.open("a", encoding="utf-8") as stream:
        stream.write(f"$ {' '.join(command)}\n")
        stream.flush()
        return subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                              check=False).returncode


def build_tests(commit: str) -> tuple[Path | None, str]:
    """eng/build-release.ps1 と同じ流儀で Debug の nib_tests だけを作る。"""
    short = git("rev-parse", "--short=7", commit).stdout.strip()
    directory = ROOT / f"build/protected-{short}"
    executable = directory / "nib_tests.exe"
    if executable.exists():
        return executable, f"reused build/protected-{short}"
    OUTPUT.mkdir(parents=True, exist_ok=True)
    log = OUTPUT / f"build-{short}.log"
    head = git("rev-parse", "HEAD").stdout.strip()
    dirty = bool(git("status", "--porcelain").stdout.strip())
    worktree = None if commit == head and not dirty else ROOT / f"build/worktree-{short}"
    source = worktree or ROOT
    try:
        if worktree is not None:
            if worktree.exists():
                git("worktree", "remove", "--force", str(worktree))
            git("worktree", "add", "--detach", str(worktree), commit)
        script = (f". '{ROOT / 'eng/toolchain.ps1'}'; "
                  f"cmake -S '{source}' -B '{directory}' -G Ninja -DCMAKE_BUILD_TYPE=Debug; "
                  "if ($LASTEXITCODE -ne 0) { exit 1 }; "
                  f"cmake --build '{directory}' --target nib_tests; exit $LASTEXITCODE")
        code = run_quiet(["pwsh", "-NoProfile", "-Command", script], log)
    finally:
        if worktree is not None and worktree.exists():
            git("worktree", "remove", "--force", str(worktree), check=False)
            git("worktree", "prune", check=False)
    relative = log.relative_to(ROOT).as_posix()
    if code != 0 or not executable.exists():
        return None, f"build failed ({relative})"
    return executable, f"built build/protected-{short} ({relative})"


def count_checks(executable: Path | None, scope: str) -> dict:
    if executable is None:
        return {"checks": UNMEASURED}
    result = subprocess.run([str(executable), scope], capture_output=True, encoding="utf-8",
                            errors="replace", check=False)
    passed = PASSED.search(result.stdout)
    if result.returncode == 0 and passed:
        return {"checks": int(passed.group(1))}
    failed = FAILED.search(result.stderr)
    if failed:
        return {"checks": int(failed.group(2)), "failed": int(failed.group(1)), "state": "失敗"}
    return {"checks": UNMEASURED, "state": f"失敗 (exit {result.returncode})"}


def scopes_section(base_scopes: list[str], head_scopes: list[str], base_exe: Path | None,
                   head_exe: Path | None, allowed: set[str]) -> list[dict]:
    rows = []
    for scope in head_scopes + [name for name in base_scopes if name not in head_scopes]:
        before = count_checks(base_exe, scope) if scope in base_scopes else None
        after = count_checks(head_exe, scope) if scope in head_scopes else None
        row = {"scope": scope, "base": before, "head": after, "allowed": scope in allowed}
        if before is None:
            row["result"] = "新規"
        elif after is None:
            row["result"] = "消えた"
        elif UNMEASURED in (before["checks"], after["checks"]):
            row["result"] = UNMEASURED
        elif before["checks"] == after["checks"]:
            row["result"] = "同じ"
        else:
            row["result"] = "変化"
        row["fails"] = row["result"] in ("変化", "消えた") and not row["allowed"]
        rows.append(row)
    return rows


def describe(record: dict) -> list[str]:
    fixtures = record["fixtures"]
    lines = [f"Protected: {record['base']['short']}..{record['head']['short']}",
             f"Protected: fixtures {fixtures['counts']['base']} -> {fixtures['counts']['head']}"
             f" / metadata {fixtures['metadata']} / deleted {len(fixtures['deleted'])}"
             f" / changed {len(fixtures['changed'])} / added {fixtures['added']}"]
    lines += [f"Protected:   {kind} {entry['name']} (VimFixtures.hpp {entry['side']} L{entry['line']})"
              for kind in ("deleted", "changed") for entry in fixtures[kind]]
    changed = [path for path, state in record["protectedFiles"].items() if state == "changed"]
    lines.append(f"Protected: files changed {changed if changed else 'none'} (recorded only)")
    if "error" in record:
        lines.append(f"Protected: error {record['error']}")
        lines.append(f"Protected: exit {record['exit']} ({record['path']})")
        return lines
    for row in record["scopes"]:
        if row["result"] not in ("同じ", UNMEASURED) or any(
                side and "state" in side for side in (row["base"], row["head"])):
            before = row["base"]["checks"] if row["base"] else "-"
            after = row["head"]["checks"] if row["head"] else "-"
            mark = " FAIL" if row["fails"] else (" allowed" if row["allowed"] else "")
            lines.append(f"Protected:   {row['scope']} {row['result']} {before} -> {after}{mark}")
    same = sum(1 for row in record["scopes"] if row["result"] == "同じ")
    unmeasured = sum(1 for row in record["scopes"] if row["result"] == UNMEASURED)
    lines.append(f"Protected: scopes {len(record['scopes'])} / same {same}"
                 f" / {UNMEASURED} {unmeasured}")
    lines.append(f"Protected: base exe {record['tests']['base']}")
    lines.append(f"Protected: head exe {record['tests']['head']}")
    lines.append(f"Protected: exit {record['exit']} ({record['path']})")
    return lines


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    # `--allow --vim-search-highlight` の値は `--` で始まるので、argparse が読める形に寄せる。
    joined: list[str] = []
    index = 0
    while index < len(argv):
        if argv[index] == "--allow" and index + 1 < len(argv):
            joined.append(f"--allow={argv[index + 1]}")
            index += 2
            continue
        joined.append(argv[index])
        index += 1
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--base", required=True)
    parser.add_argument("--head", default="HEAD")
    parser.add_argument("--allow", action="append", default=[])
    parser.add_argument("--base-tests", type=Path)
    parser.add_argument("--head-tests", type=Path)
    parser.add_argument("--build", action="store_true")
    return parser.parse_args(joined)


def tests_for(commit: str, given: Path | None, build: bool) -> tuple[Path | None, str]:
    if given is not None:
        return (given, str(given)) if given.exists() else (None, f"{UNMEASURED}: {given} is missing")
    if build:
        return build_tests(commit)
    return None, f"{UNMEASURED}: no exe (pass --build or --base-tests / --head-tests)"


def main(argv: list[str]) -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    arguments = parse_arguments(argv)
    base, head = resolve(arguments.base), resolve(arguments.head)
    for name, commit in ((arguments.base, base), (arguments.head, head)):
        if commit is None:
            print(f"Protected: unknown ref {name}", file=sys.stderr)
            return 2
    allowed = {scope if scope.startswith("--") else f"--{scope}" for scope in arguments.allow}
    short = git("rev-parse", "--short=7", head).stdout.strip()
    record = {
        "base": {"ref": arguments.base, "commit": base,
                 "short": git("rev-parse", "--short=7", base).stdout.strip()},
        "head": {"ref": arguments.head, "commit": head, "short": short},
        "allow": sorted(allowed),
        "fixtures": fixtures_section(base, head),
        "protectedFiles": protected_section(base, head),
    }
    base_scopes = parse_scopes(show(base, UNIT_TESTS))
    head_scopes = parse_scopes(show(head, UNIT_TESTS))
    error = scopes_error({arguments.base: base_scopes, arguments.head: head_scopes})
    if error is not None:
        record["error"] = error
        record["exit"] = 2
    else:
        base_exe, base_note = tests_for(base, arguments.base_tests, arguments.build)
        head_exe, head_note = tests_for(head, arguments.head_tests, arguments.build)
        record["tests"] = {"base": base_note, "head": head_note}
        record["scopes"] = scopes_section(base_scopes, head_scopes, base_exe, head_exe, allowed)
        fixtures = record["fixtures"]
        failing = bool(fixtures["deleted"] or fixtures["changed"]) or any(
            row["fails"] for row in record["scopes"])
        record["exit"] = 1 if failing else 0
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / f"{short}.json"
    record["path"] = path.relative_to(ROOT).as_posix()
    path.write_text(json.dumps(record, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print("\n".join(describe(record)))
    return record["exit"]


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
