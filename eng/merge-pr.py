"""Merge once, running CI only at this boundary and only for non-document changes."""

import argparse
import importlib.util
import json
import math
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
# #1223: a merge call can fail with a GitHub 5xx or "Merge already in progress" while GitHub is still
# merging (#1259 merged on the third call). Wait on the same PR; never start a new CI run.
MERGE_RETRIES = 10
MERGE_RETRY_SECONDS = 30
TRANSIENT_MERGE_FAILURE = re.compile(r"(?i)\b5\d\d (?:bad gateway|service unavailable|gateway time-?out|"
                                     r"internal server error)\b|merge already in progress")
# #1345: after the merge, a read (compare, PR view) or a close can time out or get a 5xx while the merge
# has already landed (#1339's compare timed out after 60 s). Those calls wait on the same table as the merge
# call (MERGE_RETRIES x MERGE_RETRY_SECONDS); reads before the merge are not retried (nothing is done yet).
TRANSIENT_GITHUB_FAILURE = re.compile(r"(?i)\bHTTP 5\d\d\b|\b5\d\d (?:bad gateway|service unavailable|"
                                      r"gateway time-?out|internal server error)\b")
# GitHub's pulls/<n>/commits and compare list at most 250 commits (ADR 0021 追補 決定 6).
COMMIT_LIMIT = 250
# GitHub closes the referenced Issue for any of these keywords, in any case (#1223).
CLOSING_REFERENCE = re.compile(r"(?i)\b(?:close[sd]?|fix(?:e[sd])?|resolve[sd]?)\b:?[ \t]+"
                               r"((?:[\w.-]+/[\w.-]+)?#\d+|https?://\S+/(?:issues|pull)/\d+)")
PR_FIELDS = "number,title,body,headRefOid,headRefName,state,isDraft,baseRefName,baseRefOid,changedFiles,isCrossRepository"
# #648: documentation-only merges skip CI, so the PR body must carry the tree conformance result.
CONFORMANCE_RECORD = re.compile(r"(?m)^.*conformance\.py.*(?<![0-9])0 violation")
SPEC = importlib.util.spec_from_file_location("git_conventions", ROOT / "eng/git-conventions.py")
CONVENTIONS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CONVENTIONS)


class MergeRefused(ValueError):
    """The requested PR has not satisfied the normal merge boundary."""


class StillFailing(MergeRefused):
    """GitHub kept failing after the waits; after a merge, the resume can finish the rest (#1345)."""


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


def read_after_merge(arguments: list[str], runner) -> object:
    """Read once the merge may have landed; wait on a timeout or a 5xx (#1345, same table as #1223)."""
    retried = 0
    while True:
        try:
            result = runner(arguments)
        except subprocess.TimeoutExpired as error:
            failure = f"TimeoutExpired: {error}"
        else:
            if not result.returncode:
                return json.loads(result.stdout)
            failure = f"GitHub read failed ({result.returncode}): {result.stderr.strip()}"
            if TRANSIENT_GITHUB_FAILURE.search(f"{result.stderr}\n{result.stdout}") is None:
                raise MergeRefused(failure)
        if retried == MERGE_RETRIES:
            raise StillFailing(f"GitHub read still failing after {MERGE_RETRIES} retries: {failure}")
        retried += 1
        print(json.dumps({"readRetry": retried, "waitSeconds": MERGE_RETRY_SECONDS,
                          "read": " ".join(arguments[:4])}, ensure_ascii=False), flush=True)
        time.sleep(MERGE_RETRY_SECONDS)


def pr_command(action: str, number: int, *arguments: str) -> list[str]:
    return ["pr", action, str(number), "--repo", REPOSITORY, *arguments]


def merge_command(number: int, expected_head: str, record: dict, rules: list,
                  integration: bool = False) -> list[str]:
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
    if integration:
        if not rebase_allowed(rules):
            raise MergeRefused("The main ruleset does not allow rebase merges (allowed_merge_methods)")
        # ADR 0021 追補 決定 3: rebase merge は件名を使わない。
        return pr_command("merge", number, "--rebase", "--match-head-commit", expected_head)
    return pr_command("merge", number, "--squash", "--match-head-commit", expected_head,
                      "--subject", title)


def rebase_allowed(rules: list) -> bool:
    for rule in rules:
        parameters = rule.get("parameters") if rule["type"] == "pull_request" else None
        methods = parameters.get("allowed_merge_methods") if isinstance(parameters, dict) else None
        if isinstance(methods, list) and "rebase" in methods:
            return True
    return False


def flatten_pages(pages: object, what: str) -> list:
    if not isinstance(pages, list) or any(not isinstance(page, list) for page in pages):
        raise MergeRefused(f"{what} pages are malformed")
    return [entry for page in pages for entry in page]


def commit_message(entry: object) -> str:
    commit = entry.get("commit") if isinstance(entry, dict) else None
    message = commit.get("message") if isinstance(commit, dict) else None
    if not isinstance(message, str):
        raise MergeRefused("Commit entry is malformed")
    return message


def subject_of(message: str) -> str:
    lines = message.splitlines()
    return lines[0] if lines else ""


def integration_commits(pages: object) -> list[dict]:
    """ADR 0021 追補 決定 1 / 3: every stacked commit is GIT-003 and closes the Issue in its subject.

    GitHub lists at most 250 commits of a PR (COMMIT_LIMIT), so an integration branch holds at most 250;
    a longer branch is refused by the ahead_by count in check_contains_main (ADR 0021 追補 決定 6).
    """
    entries = flatten_pages(pages, "Commit")
    if not entries:
        raise MergeRefused("Integration PR has no commits")
    commits = []
    for entry in entries:
        message = commit_message(entry)
        subject = subject_of(message)
        errors = CONVENTIONS.validate(message)
        if errors:
            raise MergeRefused(f"Commit {subject!r}: " + "; ".join(errors))
        issue = re.search(r"\(#([1-9]\d*)\)$", subject).group(1)
        closes = re.findall(r"(?m)^Closes #([1-9]\d*)\b", message)
        if not closes or any(number != issue for number in closes):
            raise MergeRefused(f"Commit {subject!r} must close #{issue} on a line starting with Closes #{issue}")
        # #1223: rebase merge keeps the message, so any closing keyword would close that Issue too.
        others = [reference for reference in CLOSING_REFERENCE.findall(message) if reference != f"#{issue}"]
        if others:
            raise MergeRefused(f"Commit {subject!r} has closing keywords for {', '.join(others)}; "
                               f"only #{issue} may be closed")
        source = re.search(r"(?m)^PR #([1-9]\d*)（head ([0-9a-f]{40})）", message)
        commits.append({"subject": subject,
                        "source": None if source is None else {"number": int(source.group(1)),
                                                                "head": source.group(2)}})
    return commits


def check_contains_main(comparison: object, count: int) -> str:
    """Return the merge base, which is main's tip when the branch is not behind (#1220)."""
    base = comparison.get("merge_base_commit") if isinstance(comparison, dict) else None
    if not isinstance(comparison, dict) or type(comparison.get("behind_by")) is not int or \
            type(comparison.get("ahead_by")) is not int or not isinstance(base, dict) or \
            not re.fullmatch(r"[0-9a-f]{40}", str(base.get("sha", ""))):
        raise MergeRefused("Branch comparison with main is malformed")
    if comparison["behind_by"] != 0:
        raise MergeRefused(f"Integration branch is {comparison['behind_by']} commit(s) behind main: "
                           "rebase it onto main first")
    if comparison["ahead_by"] != count:
        limit = (f" (GitHub lists at most {COMMIT_LIMIT} commits; an integration branch holds at most "
                 f"{COMMIT_LIMIT})") if comparison["ahead_by"] > COMMIT_LIMIT else ""
        raise MergeRefused(f"Integration branch is {comparison['ahead_by']} commit(s) ahead of main "
                           f"but the PR has {count} commit(s){limit}")
    return base["sha"]


def close_once(source: dict, comment: str, runner) -> str | None:
    """Close one stacked PR; on a timeout or a 5xx read it again and close again only while OPEN (#1345).

    Returns None when the PR is closed (by this call or by an earlier call that timed out), else the reason.
    """
    retried = 0
    while True:
        try:
            result = runner(pr_command("close", source["number"], "--comment", comment))
        except subprocess.TimeoutExpired as error:
            failure = f"TimeoutExpired: {error}"
        else:
            if not result.returncode:
                return None
            failure = f"close failed: {result.stderr.strip()}"
            if TRANSIENT_GITHUB_FAILURE.search(f"{result.stderr}\n{result.stdout}") is None:
                return failure
        state = read_after_merge(pr_command("view", source["number"], "--json", "headRefOid,state"), runner)
        if not isinstance(state, dict) or state.get("headRefOid") != source["head"]:
            return f"{failure}; head differs after the failed close"
        if state.get("state") == "CLOSED":
            return None
        if state.get("state") != "OPEN" or retried == MERGE_RETRIES:
            return f"{failure} (state={state.get('state')}, retries={retried})"
        retried += 1
        print(json.dumps({"closeRetry": retried, "number": source["number"],
                          "waitSeconds": MERGE_RETRY_SECONDS}), flush=True)
        time.sleep(MERGE_RETRY_SECONDS)


def close_sources(number: int, commits: list[dict], landed: list, runner,
                  resume: bool = False) -> tuple[list, list]:
    """Close each stacked PR whose head is unchanged; report the rest without stopping.

    The rebased commits land in the stacked order, so the main commit is matched by position, not by
    subject (two PRs can share a title, #1223). A commit without its source line is reported too.
    Each notClosed entry is {"number", "reason"[, "observed"]} for a stacked PR, or
    {"subject", "reason"} for a commit without its `PR #K（head <sha>）` line.
    With resume (#1345), a PR already CLOSED at the stacked head counts as closed ("alreadyClosed").
    """
    closed, not_closed = [], []
    for position, commit in enumerate(commits):
        source = commit["source"]
        if source is None:
            not_closed.append({"subject": commit["subject"], "reason": "no PR #K（head <sha>） line"})
            continue
        entry = landed[position] if position < len(landed) else None
        message = entry.get("commit", {}).get("message") if isinstance(entry, dict) and \
            isinstance(entry.get("commit"), dict) else None
        sha = entry.get("sha") if isinstance(entry, dict) else None
        if not isinstance(message, str) or not isinstance(sha, str) or \
                not re.fullmatch(r"[0-9a-f]{40}", sha) or subject_of(message) != commit["subject"]:
            not_closed.append({"number": source["number"],
                               "reason": f"main commit {position + 1} is missing or has another subject"})
            continue
        try:
            state = read_after_merge(pr_command("view", source["number"], "--json", "headRefOid,state"), runner)
            if resume and isinstance(state, dict) and state.get("state") == "CLOSED" and \
                    state.get("headRefOid") == source["head"]:
                closed.append({"number": source["number"], "mainCommit": sha, "alreadyClosed": True})
                continue
            if not isinstance(state, dict) or state.get("state") != "OPEN" or \
                    state.get("headRefOid") != source["head"]:
                not_closed.append({"number": source["number"], "reason": "not OPEN or head differs",
                                   "observed": state})
                continue
            comment = f"まとめ枝 #{number} の rebase merge で main に入った（commit {sha}・ADR 0021 追補・#1220）"
            failure = close_once(source, comment, runner)
        except (OSError, ValueError, subprocess.TimeoutExpired) as error:
            # #1223: one failed call must not hide which PRs were closed and which were not.
            not_closed.append({"number": source["number"], "reason": f"{type(error).__name__}: {error}"})
            continue
        if failure is not None:
            not_closed.append({"number": source["number"], "reason": failure})
            continue
        closed.append({"number": source["number"], "mainCommit": sha})
    return closed, not_closed


def merge_once(command: list[str], number: int, head: str, runner, before_retry=None,
               reader=read_json) -> int:
    """Call the merge; on a transient GitHub failure wait on the same PR (#1223, ADR 0021 追補 決定 8).

    Retried only while the PR is still OPEN at the same head: a MERGED PR goes on to the post-merge
    check, any other state stops. No new CI run is started. Other failures are returned unretried,
    except after a retry, when the earlier merge may have finished: then a MERGED PR goes on too.
    before_retry runs just before each repeated call (the integration branch re-reads compare).
    reader reads the PR after a failed call (the integration branch waits on its timeouts, #1345).
    """
    retried = 0
    while True:
        result = runner(command)
        print(json.dumps({"mergeExit": result.returncode, "stdout": result.stdout,
                          "stderr": result.stderr}, ensure_ascii=False), flush=True)
        if not result.returncode:
            return 0
        transient = TRANSIENT_MERGE_FAILURE.search(f"{result.stderr}\n{result.stdout}") is not None
        if not transient and not retried:
            return result.returncode
        observed = reader(pr_command("view", number, "--json", "number,headRefOid,state,mergeCommit,url"),
                          runner)
        print(json.dumps({"observedAfterMergeFailure": observed}, ensure_ascii=False), flush=True)
        if not transient:
            # V-1263 D-1: "already merged" / "not mergeable" after a wait can mean the earlier call merged.
            merged = isinstance(observed, dict) and observed.get("number") == number and \
                observed.get("headRefOid") == head and observed.get("state") == "MERGED"
            return 0 if merged else result.returncode
        if not isinstance(observed, dict) or observed.get("number") != number or \
                observed.get("headRefOid") != head:
            raise MergeRefused("PR number or head differs after a failed merge call")
        if observed.get("state") == "MERGED":
            return 0
        if observed.get("state") != "OPEN":
            raise MergeRefused(f"PR is {observed.get('state')} after a failed merge call")
        if retried == MERGE_RETRIES:
            raise StillFailing(f"merge call still failing after {MERGE_RETRIES} retries")
        retried += 1
        print(json.dumps({"mergeRetry": retried, "waitSeconds": MERGE_RETRY_SECONDS}), flush=True)
        time.sleep(MERGE_RETRY_SECONDS)
        if before_retry is not None:
            before_retry()


def api_command(path: str, *arguments: str) -> list[str]:
    return ["api", "--hostname", HOST, f"{API_ROOT}/{path}", *arguments]


def resume_command(number: int, head: str, merge_base: str) -> list[str]:
    return ["python", "eng/merge-pr.py", str(number), "--expected-head", head, "--integration",
            "--resume-after-merge", "--merge-base", merge_base, "--execute"]


def observed_merge(merged: object, number: int, head: str) -> str:
    """Return the merge commit of the expected PR once it is observed as MERGED."""
    if not isinstance(merged, dict) or merged.get("number") != number or \
            merged.get("headRefOid") != head or merged.get("state") != "MERGED":
        raise MergeRefused("The expected PR has not been observed as MERGED")
    commit = merged.get("mergeCommit")
    if not isinstance(commit, dict) or not re.fullmatch(r"[0-9a-f]{40}", str(commit.get("oid", ""))):
        raise MergeRefused("The merge commit is not recorded")
    return commit["oid"]


def check_landed_exactly(landed: object, commits: list[dict], merge_commit: str) -> None:
    """#1345: before the resume closes anything, main from the merge base to the merge commit must be
    exactly the stacked commits, in order, each with its subject and its `PR #K（head <sha>）` line."""
    entries = landed.get("commits") if isinstance(landed, dict) else None
    if not isinstance(entries, list) or landed.get("total_commits") != len(commits) or \
            len(entries) != len(commits):
        raise MergeRefused(f"main from the merge base does not hold exactly the {len(commits)} stacked commit(s)")
    for position, (entry, commit) in enumerate(zip(entries, commits)):
        message = commit_message(entry)
        source = re.search(r"(?m)^PR #([1-9]\d*)（head ([0-9a-f]{40})）", message)
        found = None if source is None else {"number": int(source.group(1)), "head": source.group(2)}
        if subject_of(message) != commit["subject"] or found != commit["source"]:
            raise MergeRefused(f"main commit {position + 1} differs from the stacked commit {commit['subject']!r}")
    if not isinstance(entries[-1], dict) or entries[-1].get("sha") != merge_commit:
        raise MergeRefused("The last stacked commit on main is not the PR's merge commit")


def resume_after_merge(options, runner) -> int:
    """#1345: only the post-merge check and the closing, after a run that stopped once the merge landed.

    Never merges, never starts CI, and closes nothing unless main holds the stacked commits exactly.
    """
    merged = read_after_merge(pr_command("view", options.number, "--json",
                                         "number,headRefOid,state,mergeCommit,url"), runner)
    print(json.dumps({"observedAfterMerge": merged}, ensure_ascii=False), flush=True)
    merge_commit = observed_merge(merged, options.number, options.expected_head)
    commits = integration_commits(read_after_merge(api_command(
        f"pulls/{options.number}/commits?per_page=100", "--paginate", "--slurp"), runner))
    on_main = read_after_merge(api_command(f"compare/{merge_commit}...main"), runner)
    if not isinstance(on_main, dict) or type(on_main.get("behind_by")) is not int or on_main["behind_by"] != 0:
        raise MergeRefused("The merge commit is not on main")
    landed = read_after_merge(api_command(f"compare/{options.merge_base}...{merge_commit}"), runner)
    check_landed_exactly(landed, commits, merge_commit)
    sources = [commit["source"]["number"] for commit in commits if commit["source"] is not None]
    print(json.dumps({"resume": True, "mergeCommit": merge_commit, "verified": True, "sources": sources},
                     ensure_ascii=False), flush=True)
    if not options.execute:
        return 0
    closed, not_closed = close_sources(options.number, commits, landed["commits"], runner, resume=True)
    return report_closing(options, commits, closed, not_closed, options.merge_base)


def report_closing(options, commits: list[dict], closed: list, not_closed: list, merge_base: str) -> int:
    if not_closed:
        print(json.dumps({"notClosed": not_closed}, ensure_ascii=False), flush=True)
    final = {"integration": True, "commits": len(commits), "closed": closed}
    if any("number" in entry for entry in not_closed):
        # A stacked PR left open by a failed call can be closed by the resume, which checks main again.
        final["resume"] = resume_command(options.number, options.expected_head, merge_base)
    print(json.dumps(final, ensure_ascii=False), flush=True)
    return 0


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
    # #716: the remaining time is whole seconds (floor), so float drift never exceeds WATCH_SECONDS.
    deadline = time.monotonic() + WATCH_SECONDS
    retried = 0
    while True:
        result = runner(["run", "watch", str(run_id), "--repo", REPOSITORY, "--exit-status", "--interval", "5"],
                        timeout=WATCH_SECONDS if retried == 0 else max(math.floor(deadline - time.monotonic()), 0))
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
    parser.add_argument("--integration", action="store_true",
                        help="rebase-merge an integration PR of stacked commits (ADR 0021 addendum, #1220)")
    parser.add_argument("--resume-after-merge", action="store_true",
                        help="after a stop once the integration PR merged: only check main and close the "
                             "stacked PRs; never merges (#1345)")
    parser.add_argument("--merge-base", type=commit_hash,
                        help="with --resume-after-merge: the mergeBase printed by the stopped run")
    options = parser.parse_args(argv)
    if options.resume_after_merge and (not options.integration or options.merge_base is None or options.ci_run):
        parser.error("--resume-after-merge needs --integration and --merge-base, and takes no --ci-run")
    if options.merge_base is not None and not options.resume_after_merge:
        parser.error("--merge-base is only for --resume-after-merge")
    if options.resume_after_merge:
        try:
            return resume_after_merge(options, runner)
        except (OSError, UnicodeError, ValueError, subprocess.TimeoutExpired) as error:
            print(f"Resume refused: {error}; nothing was closed", file=sys.stderr)
            return 2
    executing = False
    inspect = "the PR"
    # #1345: what is done so far, printed when the integration run stops after the merge call.
    progress = {"number": options.number, "mergeBase": None, "merged": "unknown", "mergeCommit": None,
                "mainChecked": False}
    try:
        record = read_json(pr_command("view", options.number, "--json", PR_FIELDS), runner)
        rules = read_json(api_command("rules/branches/main"), runner)
        command = merge_command(options.number, options.expected_head, record, rules, options.integration)
        commits = []
        if options.integration:
            commits = integration_commits(read_json(api_command(
                f"pulls/{options.number}/commits?per_page=100", "--paginate", "--slurp"), runner))
            check_contains_main(read_json(api_command(f"compare/main...{options.expected_head}"), runner),
                                len(commits))
        pages = read_json(api_command(f"pulls/{options.number}/files?per_page=100", "--paginate", "--slurp"), runner)
        docs_only = documentation_only(pages, record.get("changedFiles"))
        if docs_only and any(rule["type"] == "required_status_checks" for rule in rules):
            raise MergeRefused("Documentation-only merge is still blocked by required status checks in the ruleset")
        recorded = conformance_recorded(record.get("body"))
        plan = {"execute": options.execute, "repository": REPOSITORY, "number": options.number,
                "head": options.expected_head, "subject": record["title"],
                "ci": "skip (documentation only)" if docs_only else "run before merge",
                "conformanceRecorded": recorded,
                "branchRules": rules, "command": ["gh", *command]}
        if options.integration:
            plan.update({"integration": True, "commits": len(commits)})
        print(json.dumps(plan, ensure_ascii=False), flush=True)
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
        if options.integration:
            # main can move while CI runs; the rebased tree must still be the tree CI passed.
            merge_base = check_contains_main(read_json(api_command(f"compare/main...{options.expected_head}"),
                                                       runner), len(commits))
            progress["mergeBase"] = merge_base
        executing = True
        # The squash path reads as before; the integration path waits on post-merge reads (#1345).
        reader = read_after_merge if options.integration else read_json
        # V-1263 F-3: main must not move while waiting; keep the window to seconds as before the wait.
        before_retry = (lambda: check_contains_main(reader(api_command(f"compare/main...{options.expected_head}"),
                                                           runner), len(commits))) \
            if options.integration else None
        code = merge_once(command, options.number, options.expected_head, runner, before_retry, reader)
        if code:
            return code
        merged = reader(pr_command("view", options.number, "--json",
                                   "number,headRefOid,state,mergeCommit,url"), runner)
        print(json.dumps({"observedAfterMerge": merged}, ensure_ascii=False), flush=True)
        merge_commit = observed_merge(merged, options.number, options.expected_head)
        if not options.integration:
            return 0
        progress.update({"merged": True, "mergeCommit": merge_commit})
        inspect = "main"
        landed = reader(api_command(f"compare/{merge_base}...{merge_commit}"), runner)
        if not isinstance(landed, dict) or landed.get("total_commits") != len(commits) or \
                not isinstance(landed.get("commits"), list):
            total = landed.get("total_commits") if isinstance(landed, dict) else None
            raise MergeRefused(f"main gained {total} commit(s) but the PR has {len(commits)}")
        progress["mainChecked"] = True
        closed, not_closed = close_sources(options.number, commits, landed["commits"], runner)
        return report_closing(options, commits, closed, not_closed, merge_base)
    except (OSError, UnicodeError, ValueError, subprocess.TimeoutExpired) as error:
        if executing and options.integration:
            # #1345: say how far the run got; nothing was closed, since closing comes after every read.
            stopped = {"unfinished": dict(progress, closed=[], notClosed="all stacked PRs")}
            if isinstance(error, StillFailing):
                stopped["resume"] = resume_command(options.number, options.expected_head, progress["mergeBase"])
            print(json.dumps(stopped, ensure_ascii=False), flush=True)
        suffix = f"; merge outcome unknown: inspect {inspect} before any further action" if executing else ""
        print(f"Merge refused: {error}{suffix}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    # #1200: PR titles can carry characters cp932 cannot encode (#1169's "ß" stopped the merge queue).
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    raise SystemExit(main())
