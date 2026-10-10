"""Use the validated PR title verbatim as the squash subject (GIT-003 / GIT-004)."""

import argparse
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
REPOSITORY = "github.com/hideyukiMORI/nene-nib"
SPEC = importlib.util.spec_from_file_location("git_conventions", ROOT / "eng/git-conventions.py")
CONVENTIONS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CONVENTIONS)


class MergeRefused(ValueError):
    """The requested PR has not satisfied the normal merge boundary."""


def run_gh(arguments: list[str]) -> subprocess.CompletedProcess:
    return subprocess.run(["gh", *arguments], cwd=ROOT, shell=False, capture_output=True,
                          text=True, encoding="utf-8", timeout=60, check=False)


def read_json(arguments: list[str], runner) -> object:
    result = runner(arguments)
    if result.returncode:
        raise MergeRefused(f"GitHub read failed ({result.returncode}): {result.stderr.strip()}")
    return json.loads(result.stdout)


def pr_command(action: str, number: int, *arguments: str) -> list[str]:
    return ["pr", action, str(number), "--repo", REPOSITORY, *arguments]


def merge_command(number: int, expected_head: str, record: dict, checks: list,
                  rules: list) -> list[str]:
    if not isinstance(record, dict):
        raise MergeRefused("PR response is not an object")
    expected = {"number": number, "headRefOid": expected_head, "state": "OPEN",
                "isDraft": False, "baseRefName": "main"}
    if any(type(record.get(key)) is not type(value) or record.get(key) != value
           for key, value in expected.items()):
        raise MergeRefused("PR number, head, OPEN/ready state or main base differs")
    title, body = record.get("title"), record.get("body")
    if not isinstance(title, str) or not isinstance(body, str):
        raise MergeRefused("PR title/body is missing")
    errors = CONVENTIONS.validate_merge_title(title, body) + CONVENTIONS.validate_pr(body)
    if errors:
        raise MergeRefused("; ".join(errors))
    if not isinstance(checks, list) or not checks or any(
            not isinstance(check, dict) or check.get("state") != "SUCCESS" for check in checks):
        raise MergeRefused("Required checks are missing or not all successful")
    if not isinstance(rules, list) or not rules or any(
            not isinstance(rule, dict) or not isinstance(rule.get("type"), str) for rule in rules):
        raise MergeRefused("Branch rules are missing or malformed")
    if any(rule["type"] == "merge_queue" for rule in rules):
        raise MergeRefused("Merge queues are not supported by this synchronous merge tool")
    return pr_command("merge", number, "--squash", "--match-head-commit", expected_head,
                      "--subject", title)


def positive_number(value: str) -> int:
    if not re.fullmatch(r"[1-9][0-9]*", value):
        raise argparse.ArgumentTypeError("PR number must be a positive integer")
    return int(value)


def commit_hash(value: str) -> str:
    if not re.fullmatch(r"[0-9a-f]{40}", value):
        raise argparse.ArgumentTypeError("expected head must be the full lowercase commit SHA")
    return value


def main(argv=None, runner=run_gh) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("number", type=positive_number)
    parser.add_argument("--expected-head", type=commit_hash, required=True)
    parser.add_argument("--execute", action="store_true", help="execute once; otherwise only print the plan")
    options = parser.parse_args(argv)
    executing = False
    try:
        record = read_json(pr_command("view", options.number, "--json",
                                      "number,title,body,headRefOid,state,isDraft,baseRefName"), runner)
        checks = read_json(pr_command("checks", options.number, "--required", "--json",
                                      "name,state,link"), runner)
        host, repository = REPOSITORY.split("/", 1)
        rules = read_json(["api", "--hostname", host, f"repos/{repository}/rules/branches/main"], runner)
        command = merge_command(options.number, options.expected_head, record, checks, rules)
        print(json.dumps({"execute": options.execute, "repository": REPOSITORY, "number": options.number,
                          "head": options.expected_head, "subject": record["title"],
                          "requiredChecks": checks, "branchRules": rules, "command": ["gh", *command]},
                         ensure_ascii=False), flush=True)
        if not options.execute:
            return 0
        executing = True
        result = runner(command)
        print(json.dumps({"mergeExit": result.returncode, "stdout": result.stdout,
                          "stderr": result.stderr}, ensure_ascii=False), flush=True)
        if result.returncode:
            return result.returncode
        merged = read_json(pr_command("view", options.number, "--json",
                                      "number,headRefOid,state,mergeCommit,url"), runner)
        print(json.dumps({"observedAfterMerge": merged}, ensure_ascii=False), flush=True)
        if not isinstance(merged, dict) or merged.get("number") != options.number or \
                merged.get("headRefOid") != options.expected_head or merged.get("state") != "MERGED":
            raise MergeRefused("The expected PR has not been observed as MERGED")
        commit = merged.get("mergeCommit")
        if not isinstance(commit, dict) or not re.fullmatch(r"[0-9a-f]{40}", str(commit.get("oid", ""))):
            raise MergeRefused("The merge commit is not recorded")
        return 0
    except (OSError, UnicodeError, ValueError, subprocess.TimeoutExpired) as error:
        suffix = "; merge outcome unknown: inspect the PR before any further action" if executing else ""
        print(f"Merge refused: {error}{suffix}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
