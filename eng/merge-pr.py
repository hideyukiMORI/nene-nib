"""Merge once, running CI only at this boundary and only for non-document changes."""

import argparse
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent.parent
REPOSITORY = "github.com/hideyukiMORI/nene-nib"
HOST, REPO_NAME = REPOSITORY.split("/", 1)
API_ROOT = f"repos/{REPO_NAME}"
# ADR 0136: CI の build-and-test は Release の全 target と ctest で 7〜11 分かかる（check.yml の上限は 45 分）。
# 600 秒だと毎回切れて --ci-run での再開が要った（#522 / #524 / #526・Issue #533）。
WATCH_SECONDS = 2700
# #667: `gh run watch` can exit non-zero while the same run is still going; wait on that run again.
WATCH_RETRIES = 3
PR_FIELDS = "number,title,body,headRefOid,headRefName,state,isDraft,baseRefName,baseRefOid,changedFiles,isCrossRepository"
# #648: documentation-only merges skip CI, so the PR body must carry the tree conformance result.
CONFORMANCE_RECORD = re.compile(r"(?m)^.*conformance\.py.*(?<![0-9])0 violation")
SPEC = importlib.util.spec_from_file_location("git_conventions", ROOT / "eng/git-conventions.py")
CONVENTIONS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CONVENTIONS)


class MergeRefused(ValueError):
    """The requested PR has not satisfied the normal merge boundary."""


def run_gh(arguments: list[str], timeout: float | None = None) -> subprocess.CompletedProcess:
    if timeout is None:
        timeout = WATCH_SECONDS if arguments[:2] == ["run", "watch"] else 60
    return subprocess.run(["gh", *arguments], cwd=ROOT, shell=False, capture_output=True,
                          text=True, encoding="utf-8", timeout=timeout, check=False)


def read_json(arguments: list[str], runner) -> object:
    result = runner(arguments)
    if result.returncode:
        raise MergeRefused(f"GitHub read failed ({result.returncode}): {result.stderr.strip()}")
    return json.loads(result.stdout)


def pr_command(action: str, number: int, *arguments: str) -> list[str]:
    return ["pr", action, str(number), "--repo", REPOSITORY, *arguments]


def merge_command(number: int, expected_head: str, record: dict, rules: list) -> list[str]:
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
    if not isinstance(rules, list) or not rules or any(
            not isinstance(rule, dict) or not isinstance(rule.get("type"), str) for rule in rules):
        raise MergeRefused("Branch rules are missing or malformed")
    if any(rule["type"] == "merge_queue" for rule in rules):
        raise MergeRefused("Merge queues are not supported by this synchronous merge tool")
    return pr_command("merge", number, "--squash", "--match-head-commit", expected_head,
                      "--subject", title)


def api_command(path: str, *arguments: str) -> list[str]:
    return ["api", "--hostname", HOST, f"{API_ROOT}/{path}", *arguments]


def documentation_only(pages: object, expected_count: int) -> bool:
    """Use the complete changed-file list, including the old path of a rename."""
    if not isinstance(pages, list) or any(not isinstance(page, list) for page in pages):
        raise MergeRefused("Changed-file pages are malformed")
    files = [entry for page in pages for entry in page]
    if type(expected_count) is not int or expected_count <= 0 or len(files) != expected_count:
        raise MergeRefused("Changed-file list is empty or incomplete")
    paths = []
    for entry in files:
        if not isinstance(entry, dict) or not isinstance(entry.get("filename"), str):
            raise MergeRefused("Changed-file entry is malformed")
        paths.append(entry["filename"])
        if entry.get("status") == "renamed":
            if not isinstance(entry.get("previous_filename"), str):
                raise MergeRefused("Renamed file is missing its previous path")
            paths.append(entry["previous_filename"])
    if any(not path or path.startswith("/") or "\\" in path or
           any(part in ("", ".", "..") for part in path.split("/")) for path in paths):
        raise MergeRefused("Changed-file path is malformed")
    return all(path.startswith("docs/") or ("/" not in path and path.endswith(".md")) or
               path in ("LICENSE", "THIRD_PARTY_NOTICES.txt") for path in paths)


def conformance_recorded(body: object) -> bool:
    return isinstance(body, str) and CONFORMANCE_RECORD.search(body) is not None


def watch_run(run_id: int, runner) -> None:
    """Wait on one run; an interrupted watch waits on the same run again, never a new one (#667)."""
    deadline = time.monotonic() + WATCH_SECONDS
    retried = 0
    while True:
        result = runner(["run", "watch", str(run_id), "--repo", REPOSITORY, "--exit-status", "--interval", "5"],
                        timeout=WATCH_SECONDS if retried == 0 else max(deadline - time.monotonic(), 0))
        if not result.returncode:
            break
        failure = f"CI did not succeed: {result.stderr.strip()} {result.stdout.strip()}"
        state = read_json(["run", "view", str(run_id), "--repo", REPOSITORY, "--json", "status,conclusion"], runner)
        if not isinstance(state, dict):
            raise MergeRefused(failure)
        if state.get("status") == "completed" and state.get("conclusion") == "success":
            break
        if state.get("status") not in ("in_progress", "queued") or retried == WATCH_RETRIES or                 deadline - time.monotonic() <= 0:
            raise MergeRefused(f"{failure} (status={state.get('status')}, conclusion={state.get('conclusion')})")
        retried += 1
    if retried:
        print(json.dumps({"ciWatchRetried": retried}), flush=True)


def run_ci(number: int, head: str, record: dict, runner, run_id: int | None) -> dict:
    if record.get("isCrossRepository") is not False or not record.get("headRefName"):
        raise MergeRefused("Merge CI requires a branch in the canonical repository")
    if run_id is None:
        response = read_json(api_command("actions/workflows/check.yml/dispatches", "--method", "POST",
                             "-H", "X-GitHub-Api-Version: 2026-03-10", "-f", "ref=main",
                             "-f", f"inputs[pr_number]={number}", "-f", f"inputs[expected_head]={head}",
                             "-F", "return_run_details=true"), runner)
        if not isinstance(response, dict) or type(response.get("workflow_run_id")) is not int:
            raise MergeRefused("CI dispatch outcome unknown: inspect Actions before dispatching again")
        run_id = response["workflow_run_id"]
    print(json.dumps({"ciRun": run_id, "resume": f"--ci-run {run_id}"}), flush=True)
    run = read_json(api_command(f"actions/runs/{run_id}"), runner)
    # ADR 0143: the definition is main's check.yml; the PR head is proven by the input-based run name.
    expected = {"event": "workflow_dispatch", "path": ".github/workflows/check.yml",
                "head_branch": "main", "display_title": f"Merge PR #{number} at {head}"}
    if not isinstance(run, dict):
        raise MergeRefused("CI run response is malformed")
    # Dispatch can return before GitHub resolves the input-based run name.
    # Observe completion before validating the final run identity.
    if run.get("status") != "completed":
        watch_run(run_id, runner)
        run = read_json(api_command(f"actions/runs/{run_id}"), runner)
    if not isinstance(run, dict) or run.get("status") != "completed" or run.get("conclusion") != "success" or \
            any(run.get(key) != value for key, value in expected.items()) or \
            not isinstance(run.get("repository"), dict) or run["repository"].get("full_name") != REPO_NAME:
        raise MergeRefused("CI has not completed successfully for the expected head")
    return {"id": run_id, "url": run.get("html_url"), "conclusion": "success"}


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
    parser.add_argument("--ci-run", type=positive_number, help="resume a previously dispatched merge CI run")
    options = parser.parse_args(argv)
    executing = False
    try:
        record = read_json(pr_command("view", options.number, "--json", PR_FIELDS), runner)
        rules = read_json(api_command("rules/branches/main"), runner)
        command = merge_command(options.number, options.expected_head, record, rules)
        pages = read_json(api_command(f"pulls/{options.number}/files?per_page=100", "--paginate", "--slurp"), runner)
        docs_only = documentation_only(pages, record.get("changedFiles"))
        if docs_only and any(rule["type"] == "required_status_checks" for rule in rules):
            raise MergeRefused("Documentation-only merge is still blocked by required status checks in the ruleset")
        recorded = conformance_recorded(record.get("body"))
        print(json.dumps({"execute": options.execute, "repository": REPOSITORY, "number": options.number,
                          "head": options.expected_head, "subject": record["title"],
                          "ci": "skip (documentation only)" if docs_only else "run before merge",
                          "conformanceRecorded": recorded,
                          "branchRules": rules, "command": ["gh", *command]},
                         ensure_ascii=False), flush=True)
        if not options.execute:
            return 0
        if docs_only and not recorded:
            raise MergeRefused("Documentation-only merge requires a recorded eng/conformance.py result with 0 violation(s)")
        if not docs_only:
            print(json.dumps({"verifiedCI": run_ci(options.number, options.expected_head, record,
                                                  runner, options.ci_run)}), flush=True)
        latest = read_json(pr_command("view", options.number, "--json", PR_FIELDS), runner)
        if latest != record:
            raise MergeRefused("PR changed during merge preparation; inspect it before continuing")
        latest_rules = read_json(api_command("rules/branches/main"), runner)
        if latest_rules != rules:
            raise MergeRefused("Branch rules changed during merge preparation")
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
