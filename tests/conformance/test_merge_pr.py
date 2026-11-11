"""Only the merge tool boundary; no network, repository mutation or product execution."""

import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("merge_pr", ROOT / "eng/merge-pr.py")
MERGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MERGE)
HEAD = "a" * 40
TITLE = "fix(git): 件名「'$()' と `」を保持する (#42)"
BODY = ("Closes #42\n目的: 件名を保つ\n使った正典経路: eng/merge-pr.py\n規則 ID: GIT-004\n"
        "振る舞い・スキーマの変更: なし\n検証（実行したコマンドと結果）: 道具の試験\n"
        "確認する退行: 件名の転記\n対象・依存: 統合入口\n"
        "検証結果: 限定した道具試験\n再利用: 初回検証\nWaivers: none\n残るリスク: なし\n")
RECORDED_BODY = BODY + "検証（実行したコマンドと結果）: python eng/conformance.py → 0 violation(s)\n"
RECORD = {"number": 900, "headRefOid": HEAD, "state": "OPEN", "isDraft": False,
          "baseRefName": "main", "baseRefOid": "c" * 40, "title": TITLE, "body": BODY,
          "changedFiles": 1, "headRefName": "fix/42-subject", "isCrossRepository": False}
FILES = [[{"filename": "eng/merge-pr.py", "status": "modified"}]]
REPOSITORY = "github.com/hideyukiMORI/nene-nib"
RULES = [{"type": "pull_request"}, {"type": "non_fast_forward"}]
RUN = {"event": "workflow_dispatch", "path": ".github/workflows/check.yml", "head_sha": "c" * 40,
       "head_branch": "main", "display_title": f"Merge PR #900 at {HEAD}",
       "repository": {"full_name": "hideyukiMORI/nene-nib"}, "status": "completed", "conclusion": "success"}
MERGED = {"number": 900, "headRefOid": HEAD, "state": "MERGED",
          "mergeCommit": {"oid": "b" * 40}, "url": "https://github.com/hideyukiMORI/nene-nib/pull/900"}
# ADR 0021 追補（#1220）: まとめ枝の commit は元の PR の題・`Closes #N`・`PR #K（head <sha>）` の行を持つ。
INTEGRATION_RULES = [{"type": "pull_request", "parameters": {"allowed_merge_methods": ["squash", "rebase"]}},
                     {"type": "non_fast_forward"}]
SOURCE_HEADS = {"1157": "1" * 40, "1160": "2" * 40}


def stacked(subject, issue, source=None):
    lines = [subject, "", f"Closes #{issue}"]
    if source is not None:
        lines.append(f"PR #{source}（head {SOURCE_HEADS[source]}）を squash と同じ中身でまとめ枝 #42 に積む。")
    lines += ["", f"* {subject}"]
    return {"sha": "e" * 40, "commit": {"message": "\n".join(lines) + "\n"}}


COMMITS = [[stacked("feat(ui): 印刷の口を足す (#1050)", 1050, "1157"),
            stacked("fix(ui): 印刷の余白を直す (#1158)", 1158, "1160")],
           [stacked("feat(eng): merge-pr.py にまとめ枝の口を足す (#1220)", 1220)]]
# The post-merge count starts at the merge base read just before merge, not the PR baseRefOid ("c" * 40).
MERGE_BASE = "d" * 40
COMPARE = {"behind_by": 0, "ahead_by": 3, "status": "ahead", "merge_base_commit": {"sha": MERGE_BASE}}
MAIN_SHAS = ["7" * 40, "8" * 40, "9" * 40]
LANDED = {"total_commits": 3, "commits": [
    {"sha": sha, "commit": {"message": entry["commit"]["message"]}}
    for sha, entry in zip(MAIN_SHAS, [entry for page in COMMITS for entry in page])]}
SOURCES = {number: {"headRefOid": head, "state": "OPEN"} for number, head in SOURCE_HEADS.items()}
# #1223: what `gh pr merge` printed for PR #1259 (D:/NeNeNib/outputs/20261104/merge-1259*.out).
BAD_GATEWAY = (1, 'non-200 OK status code: 502 Bad Gateway body: "<html>\\r\\n<head><title>502 Bad Gateway'
                  '</title></head>"\n')
IN_PROGRESS = (1, "GraphQL: Merge already in progress (mergePullRequest)\n")
STILL_OPEN = {"number": 900, "headRefOid": HEAD, "state": "OPEN", "mergeCommit": None,
              "url": "https://github.com/hideyukiMORI/nene-nib/pull/900"}
# #1345: what the compare of #1339 raised after the merge (D:/NeNeNib/outputs/20261110/merge-1339.out).
COMPARE_TIMEOUT = subprocess.TimeoutExpired(["gh", "api", "--hostname", "github.com",
                                             f"repos/hideyukiMORI/nene-nib/compare/{MERGE_BASE}...{'b' * 40}"], 60)
ON_MAIN = {"behind_by": 0, "ahead_by": 0, "status": "identical"}
# For a rebase merge, mergeCommit is the commit main was moved to: the last stacked commit.
RESUME_LANDED = {"total_commits": 3, "commits": [
    {"sha": sha, "commit": {"message": entry["commit"]["message"]}}
    for sha, entry in zip(MAIN_SHAS[:2] + ["b" * 40], [entry for page in COMMITS for entry in page])]}
# #1501: what the read during the wait for CI printed for PR #1500 (D:/NeNeNib/outputs/20261112/merge-1500.log).
CUT_BODY = (1, "unexpected end of JSON input\n")
# Transient read failures before the merge: a cut body, a 5xx, a timeout and exit 0 with a body that is not JSON.
TRANSIENT_READS = {"cut body": CUT_BODY,
                   "5xx": (1, "HTTP 502: Bad Gateway (https://api.github.com/repos/hideyukiMORI/nene-nib)\n"),
                   "timeout": subprocess.TimeoutExpired(["gh"], 60),
                   "not JSON": (0, '[[{"filename": "eng/merge-pr.py", "sta')}
NOT_FOUND = (1, "HTTP 404: Not Found (https://api.github.com/repos/hideyukiMORI/nene-nib/rules/branches/main)\n")
UNSOURCED = {"subject":"feat(eng): merge-pr.py にまとめ枝の口を足す (#1220)", "reason": "no PR #K（head <sha>） line"}


class MergeBoundary(unittest.TestCase):
    def invoke(self, record=None, execute=False, read_exit=0, merge_exit=0,
               merge_timeout=False, rules=None, merged=None, rules_exit=0, files=None, run=None,
               latest=None, ci_run=None, dispatch_exit=0, watch_exit=0, run_view=None,
               integration=False, commits=None, compare=None, landed=None, sources=None, close_exit=0,
               landed_failures=(), resume=False, on_main=None, merge_base=MERGE_BASE,
               read_failures=None):
        calls = []
        landed_failures = list(landed_failures)
        # #1501: {marker: [failure, ...]}; the first calls whose API path or "pr view" / "run view" contains
        # the marker get the failures in order: (code, stderr), (0, stdout) for an unreadable body, or raised.
        read_failures = {marker: list(failures) for marker, failures in (read_failures or {}).items()}
        close_exits = list(close_exit) if isinstance(close_exit, list) else None
        compare_responses = copy.deepcopy(compare) if isinstance(compare, list) else None
        self.watch_timeouts = []
        run_responses = copy.deepcopy(run) if isinstance(run, list) else None
        watch_exits = list(watch_exit) if isinstance(watch_exit, list) else None
        view_responses = copy.deepcopy(run_view) if isinstance(run_view, list) else None
        merge_results = list(merge_exit) if isinstance(merge_exit, list) else None
        merged_responses = copy.deepcopy(merged) if isinstance(merged, list) else None
        self.sleeps = []

        def runner(arguments, timeout=None):
            calls.append(arguments)
            target = arguments[3] if arguments[0] == "api" else " ".join(arguments[:2])
            for marker, failures in read_failures.items():
                if marker in target and failures:
                    failure = failures.pop(0)
                    if isinstance(failure, Exception):
                        raise failure
                    code, text = failure
                    return subprocess.CompletedProcess(arguments, code, "" if code else text, text if code else "")
            if arguments[0] == "api":
                path = arguments[3]
                if path.endswith("rules/branches/main"):
                    value, code = RULES if rules is None else rules, rules_exit
                elif "/files?" in path:
                    value, code = FILES if files is None else files, 0
                elif path.endswith("/dispatches"):
                    value, code = {"workflow_run_id": 101}, dispatch_exit
                elif "/actions/runs/" in path:
                    value, code = (run_responses.pop(0) if run_responses is not None else RUN if run is None else run), 0
                elif path.endswith("/pulls/900/commits?per_page=100"):
                    self.assertEqual(arguments[4:], ["--paginate", "--slurp"])
                    value, code = COMMITS if commits is None else commits, 0
                elif path.endswith(f"/compare/main...{HEAD}"):
                    value = (compare_responses.pop(0) if compare_responses is not None else
                             COMPARE if compare is None else compare)
                    code = 0
                elif path.endswith(f"/compare/{MERGE_BASE}...{'b' * 40}"):
                    if landed_failures:
                        failure = landed_failures.pop(0)
                        if isinstance(failure, Exception):
                            raise failure
                        return subprocess.CompletedProcess(arguments, failure[0], "", failure[1])
                    value, code = LANDED if landed is None else landed, 0
                elif path.endswith(f"/compare/{'b' * 40}...main"):
                    self.assertTrue(resume)
                    value, code = ON_MAIN if on_main is None else on_main, 0
                else:
                    self.fail(f"Unexpected API: {arguments}")
                return subprocess.CompletedProcess(arguments, code, json.dumps(value), "rules error" if code else "")
            if arguments[:2] == ["run", "watch"]:
                self.watch_timeouts.append(timeout)
                code = watch_exits.pop(0) if watch_exits is not None else watch_exit
                return subprocess.CompletedProcess(arguments, code, "", "CI failed" if code else "")
            if arguments[:2] == ["run", "view"]:
                self.assertEqual(arguments[2:], ["101", "--repo", REPOSITORY, "--json", "status,conclusion"])
                value = (view_responses.pop(0) if view_responses is not None else
                         {"status": "completed", "conclusion": "failure"})
                return subprocess.CompletedProcess(arguments, 0, json.dumps(value), "")
            if arguments[1] in ("view", "close") and arguments[2] != "900":
                self.assertTrue(integration and (resume or any(call[1] == "merge" for call in calls)))
                if arguments[1] == "close":
                    code = close_exits.pop(0) if close_exits is not None else close_exit
                    if isinstance(code, Exception):
                        raise code
                    if isinstance(code, tuple):
                        return subprocess.CompletedProcess(arguments, code[0], "", code[1])
                    return subprocess.CompletedProcess(arguments, code, "", "close error" if code else "")
                self.assertEqual(arguments[5:], ["--json", "headRefOid,state"])
                value = (SOURCES if sources is None else sources)[arguments[2]]
                if isinstance(value, list):
                    value = value.pop(0) if len(value) > 1 else value[0]
                if isinstance(value, Exception):
                    raise value
                return subprocess.CompletedProcess(arguments, 0, json.dumps(value), "")
            if arguments[1] == "view":
                if resume or any(call[1] == "merge" for call in calls):
                    self.assertEqual(arguments[5:], ["--json", "number,headRefOid,state,mergeCommit,url"])
                    value = (merged_responses.pop(0) if merged_responses is not None else
                             MERGED if merged is None else merged)
                    if isinstance(value, Exception):
                        raise value
                    return subprocess.CompletedProcess(arguments, 0, json.dumps(value), "")
                if latest is not None and sum(call[:2] == ["pr", "view"] for call in calls) > 1:
                    return subprocess.CompletedProcess(arguments, 0, json.dumps(latest), "")
                return subprocess.CompletedProcess(arguments, read_exit,
                                                   json.dumps(RECORD if record is None else record),
                                                   "read error" if read_exit else "")
            if arguments[1] == "merge":
                self.assertFalse(resume)
                if merge_timeout:
                    raise subprocess.TimeoutExpired(arguments, 60)
                if merge_results is not None:
                    code, stderr = merge_results.pop(0)
                    return subprocess.CompletedProcess(arguments, code, "", stderr)
                return subprocess.CompletedProcess(arguments, merge_exit, "merge output", "merge error" if merge_exit else "")
            self.fail(f"Unexpected command: {arguments}")

        output, error = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(error), \
                patch.object(MERGE.time, "sleep", side_effect=self.sleeps.append):
            result = MERGE.main(["900", "--expected-head", HEAD, *(["--execute"] if execute else []),
                                 *(["--ci-run", str(ci_run)] if ci_run else []),
                                 *(["--integration"] if integration else []),
                                 *(["--resume-after-merge", "--merge-base", merge_base] if resume else [])], runner)
        return result, calls, output.getvalue(), error.getvalue()

    def test_plan_never_mutates_and_keeps_issue_distinct_from_pr(self):
        result, calls, output, error = self.invoke()
        self.assertEqual(result, 0, error)
        self.assertEqual([call[1] for call in calls if call[0] == "pr"], ["view"])
        self.assertFalse(any("POST" in call for call in calls))
        plan = json.loads(output)
        self.assertFalse(plan["execute"])
        self.assertEqual(plan["number"], 900)
        self.assertEqual(plan["repository"], REPOSITORY)
        self.assertEqual(plan["subject"], TITLE)
        self.assertEqual(plan["command"], ["gh", "pr", "merge", "900", "--repo", REPOSITORY, "--squash",
                                           "--match-head-commit", HEAD, "--subject", TITLE])

    def test_execute_uses_verbatim_title_once_and_retains_branches(self):
        result, calls, output, error = self.invoke(execute=True)
        self.assertEqual(result, 0, error)
        self.assertEqual([call[1] for call in calls if call[0] == "pr"], ["view", "view", "merge", "view"])
        self.assertEqual(calls[-2], ["pr", "merge", "900", "--repo", REPOSITORY, "--squash",
                                     "--match-head-commit", HEAD, "--subject", TITLE])
        for call in calls:
            if call[0] == "pr":
                self.assertEqual(call[3:5], ["--repo", REPOSITORY])
            elif call[0] == "api":
                self.assertEqual(call[1:3], ["--hostname", "github.com"])
                self.assertTrue(call[3].startswith("repos/hideyukiMORI/nene-nib/"))
        self.assertEqual(sum("POST" in call for call in calls), 1)
        self.assertEqual(json.loads(output.splitlines()[-2])["mergeExit"], 0)
        self.assertEqual(json.loads(output.splitlines()[-1])["observedAfterMerge"], MERGED)

    def test_invalid_pr_state_title_issue_or_record_never_mutates(self):
        changes = [("number", 42), ("number", True), ("headRefOid", "b" * 40),
                   ("state", "MERGED"), ("isDraft", True), ("baseRefName", "release"),
                   ("title", "bad title"), ("title", TITLE + "\nextra"),
                   ("title", TITLE.replace("#42", "#900")), ("body", BODY.replace("#42", "#43")),
                   ("body", BODY + "Closes #43\n"), ("body", BODY.replace("再利用: 初回検証", "再利用:")),
                   ("body", None), ("title", None)]
        changes.extend(("title", TITLE + separator + "追加 (#42)")
                       for separator in ("\u2028", "\u2029", "\x85", "\x0b", "\x0c", "\x1c", "\x1d", "\x1e"))
        for key, value in changes:
            with self.subTest(key=key, value=value):
                record = copy.deepcopy(RECORD)
                record[key] = value
                result, calls, output, error = self.invoke(record=record, execute=True)
                self.assertEqual(result, 2)
                self.assertNotIn("merge", [call[1] for call in calls])
                self.assertEqual(output, "")
                self.assertIn("Merge refused:", error)

    def test_unicode_line_separator_cannot_change_the_validated_issue(self):
        title = "fix(git): 確認 (#42)\u2028追加 (#99)"
        errors = MERGE.CONVENTIONS.validate_merge_title(title, "Closes #99")
        self.assertTrue(errors)
        self.assertTrue(any("single line" in error for error in errors))

    def test_dispatch_uses_main_definition_and_head_input(self):
        result, calls, output, error = self.invoke(execute=True)
        self.assertEqual(result, 0, error)
        dispatches = [call for call in calls if "POST" in call]
        self.assertEqual(len(dispatches), 1)
        self.assertIn("ref=main", dispatches[0])
        self.assertFalse(any(argument == f"ref={RECORD['headRefName']}" for argument in dispatches[0]))
        self.assertIn(f"inputs[expected_head]={HEAD}", dispatches[0])

    def test_ci_failure_skipped_or_wrong_target_never_merges(self):
        for key, value in (("conclusion", "failure"), ("conclusion", "skipped"),
                           ("event", "push"), ("path", "other.yml"), ("display_title", "Merge PR #42"),
                           ("repository", None), ("head_branch", "fix/42-subject")):
            with self.subTest(key=key, value=value):
                run = dict(RUN, **{key: value})
                result, calls, output, error = self.invoke(run=run, execute=True)
                self.assertEqual(result, 2)
                self.assertNotIn("merge", [call[1] for call in calls])
                self.assertIn("CI", error)

    def test_documents_skip_ci_including_at_merge(self):
        files = [[{"filename": path} for path in ("README.md", "docs/images/theme.svg", "docs/design/gallery.html")]]
        record = dict(RECORD, changedFiles=3, body=RECORDED_BODY)
        result, calls, output, error = self.invoke(record=record, files=files, execute=True)
        self.assertEqual(result, 0, error)
        self.assertEqual(json.loads(output.splitlines()[0])["ci"], "skip (documentation only)")
        self.assertTrue(json.loads(output.splitlines()[0])["conformanceRecorded"])
        self.assertFalse(any("actions/" in " ".join(call) or call[0] == "run" for call in calls))
        self.assertEqual([call[1] for call in calls].count("merge"), 1)

    def test_documents_with_conformance_record_plan_and_merge(self):
        files = [[{"filename": "docs/quality/x.md"}]]
        record = dict(RECORD, body=RECORDED_BODY)
        result, calls, output, error = self.invoke(record=record, files=files)
        self.assertEqual(result, 0, error)
        self.assertTrue(json.loads(output)["conformanceRecorded"])
        result, calls, output, error = self.invoke(record=record, files=files, execute=True)
        self.assertEqual(result, 0, error)
        self.assertEqual([call[1] for call in calls].count("merge"), 1)

    def test_documents_without_conformance_record_are_refused_before_merge(self):
        files = [[{"filename": "docs/quality/x.md"}]]
        for body in (BODY, BODY + "検証（実行したコマンドと結果）: python eng/conformance.py → 10 violation(s)\n",
                     BODY + "検証（実行したコマンドと結果）: python eng/conformance.py\n0 violation(s)\n"):
            with self.subTest(body=body):
                record = dict(RECORD, body=body)
                result, calls, output, error = self.invoke(record=record, files=files)
                self.assertEqual(result, 0, error)
                self.assertFalse(json.loads(output)["conformanceRecorded"])
                result, calls, output, error = self.invoke(record=record, files=files, execute=True)
                self.assertEqual(result, 2)
                self.assertNotIn("merge", [call[1] for call in calls])
                self.assertIn("requires a recorded eng/conformance.py result with 0 violation(s)", error)

    def test_non_documents_without_conformance_record_still_run_ci_and_merge(self):
        result, calls, output, error = self.invoke(execute=True)
        self.assertEqual(result, 0, error)
        self.assertFalse(json.loads(output.splitlines()[0])["conformanceRecorded"])
        self.assertEqual(sum("POST" in call for call in calls), 1)
        self.assertEqual([call[1] for call in calls].count("merge"), 1)

    def test_full_file_list_and_previous_rename_path_control_scope(self):
        self.assertTrue(MERGE.documentation_only([[{"filename": "README.md"}], [{"filename": "docs/x.png"}]], 2))
        for path in ("src/Main.cpp", "eng/check.ps1", ".github/workflows/check.yml", "tests/x.md", "unknown.txt"):
            self.assertFalse(MERGE.documentation_only([[{"filename": "README.md"}, {"filename": path}]], 2))
            self.assertFalse(MERGE.documentation_only([[{"filename": "docs/old.md", "status": "renamed",
                                                        "previous_filename": path}]], 1))
        for pages, count in (([], 0), ([[]], 1), ([[{"filename": "README.md"}]], 2),
                             ([[{"filename": "docs/x", "status": "renamed"}]], 1),
                             ([[{"filename": "docs/../src/x.cpp"}]], 1)):
            with self.subTest(pages=pages), self.assertRaises(MERGE.MergeRefused):
                MERGE.documentation_only(pages, count)

    def test_docs_refuse_leftover_global_status_requirement(self):
        result, calls, output, error = self.invoke(files=[[{"filename": "README.md"}]],
                                                  rules=RULES + [{"type": "required_status_checks"}], execute=True)
        self.assertEqual(result, 2)
        self.assertNotIn("merge", [call[1] for call in calls])
        self.assertFalse(any("POST" in call for call in calls))

    def test_resume_same_ci_run_does_not_dispatch_again(self):
        result, calls, output, error = self.invoke(ci_run=101, execute=True)
        self.assertEqual(result, 0, error)
        self.assertFalse(any("POST" in call for call in calls))

    def test_queued_run_metadata_is_validated_after_completion(self):
        result, calls, output, error = self.invoke(run=[{"status": "queued"}, RUN], execute=True)
        self.assertEqual(result, 0, error)
        self.assertEqual(sum(call[:2] == ["run", "watch"] for call in calls), 1)
        self.assertEqual(sum("POST" in call for call in calls), 1)

    def test_ci_dispatch_failure_or_watch_failure_does_not_retry_or_merge(self):
        for options in ({"dispatch_exit": 1}, {"run": dict(RUN, status="in_progress"), "watch_exit": 1}):
            result, calls, output, error = self.invoke(execute=True, **options)
            self.assertEqual(result, 2)
            self.assertEqual(sum("POST" in call for call in calls), 1)
            self.assertNotIn("merge", [call[1] for call in calls])

    def test_interrupted_watch_waits_on_the_same_run_again(self):
        # #667 (a): watch exits non-zero while the run is still in progress; the second watch succeeds.
        result, calls, output, error = self.invoke(run=[{"status": "in_progress"}, RUN], execute=True,
                                                  watch_exit=[1, 0], run_view=[{"status": "in_progress"}])
        self.assertEqual(result, 0, error)
        self.assertEqual(sum(call[:2] == ["run", "watch"] for call in calls), 2)
        self.assertEqual(sum(call[:2] == ["run", "view"] for call in calls), 1)
        self.assertEqual(sum("POST" in call for call in calls), 1)
        self.assertIn({"ciWatchRetried": 1}, [json.loads(line) for line in output.splitlines()])
        self.assertEqual([call[1] for call in calls].count("merge"), 1)
        self.assertEqual(self.watch_timeouts[0], MERGE.WATCH_SECONDS)
        self.assertIsInstance(self.watch_timeouts[1], int)
        self.assertLessEqual(self.watch_timeouts[1], MERGE.WATCH_SECONDS)

    def test_remaining_watch_time_is_whole_seconds_within_the_limit(self):
        # #716: deadline - now on this clock is 2700.000000000058 in floating point; the watch gets 2700.
        timeouts = []
        responses = {"watch": [1, 0]}

        def runner(arguments, timeout=None):
            if arguments[:2] == ["run", "watch"]:
                timeouts.append(timeout)
                return subprocess.CompletedProcess(arguments, responses["watch"].pop(0), "", "")
            return subprocess.CompletedProcess(arguments, 0, json.dumps({"status": "in_progress"}), "")

        with (patch.object(MERGE.time, "monotonic", return_value=522054.3407681393),
              contextlib.redirect_stdout(io.StringIO())):
            MERGE.watch_run(101, runner)
        self.assertEqual(timeouts, [MERGE.WATCH_SECONDS, MERGE.WATCH_SECONDS])
        self.assertIsInstance(timeouts[1], int)

    def test_interrupted_watch_of_a_failed_run_is_refused_without_retry(self):
        # #667 (b): the run itself completed with a failure.
        for conclusion in ("failure", "cancelled", "timed_out"):
            with self.subTest(conclusion=conclusion):
                result, calls, output, error = self.invoke(
                    run={"status": "in_progress"}, execute=True, watch_exit=1,
                    run_view=[{"status": "completed", "conclusion": conclusion}])
                self.assertEqual(result, 2)
                self.assertEqual(sum(call[:2] == ["run", "watch"] for call in calls), 1)
                self.assertNotIn("merge", [call[1] for call in calls])
                self.assertNotIn("ciWatchRetried", output)
                self.assertIn("CI did not succeed", error)

    def test_watch_retries_stop_after_three(self):
        # #667 (c): four interrupted watches in a row while the run stays in progress.
        result, calls, output, error = self.invoke(run={"status": "in_progress"}, execute=True,
                                                  watch_exit=[1, 1, 1, 1],
                                                  run_view=[{"status": "in_progress"}] * 4)
        self.assertEqual(result, 2)
        self.assertEqual(sum(call[:2] == ["run", "watch"] for call in calls), 4)
        self.assertEqual(sum("POST" in call for call in calls), 1)
        self.assertNotIn("merge", [call[1] for call in calls])
        self.assertIn("status=in_progress", error)

    def test_watch_cut_just_before_success_continues(self):
        # #667 (d): watch exits non-zero but the run already completed successfully.
        result, calls, output, error = self.invoke(run=[{"status": "in_progress"}, RUN], execute=True,
                                                  watch_exit=1, run_view=[{"status": "completed",
                                                                           "conclusion": "success"}])
        self.assertEqual(result, 0, error)
        self.assertEqual(sum(call[:2] == ["run", "watch"] for call in calls), 1)
        self.assertNotIn("ciWatchRetried", output)
        self.assertEqual([call[1] for call in calls].count("merge"), 1)

    def test_pr_changes_during_ci_stop_merge(self):
        for key, value in (("headRefOid", "b" * 40), ("title", TITLE + "x"), ("body", BODY + "x"),
                           ("baseRefOid", "d" * 40)):
            result, calls, output, error = self.invoke(latest=dict(RECORD, **{key: value}), execute=True)
            self.assertEqual(result, 2)
            self.assertNotIn("merge", [call[1] for call in calls])

    def test_queue_or_unreadable_rules_never_mutate(self):
        for rules in ([], {}, [None], [{"type": None}], [*RULES, {"type": "merge_queue"}]):
            with self.subTest(rules=rules):
                result, calls, output, error = self.invoke(rules=rules, execute=True)
                self.assertEqual(result, 2)
                self.assertNotIn("merge", [call[1] for call in calls])
                self.assertEqual(output, "")
        result, calls, output, error = self.invoke(rules_exit=1, execute=True)
        self.assertEqual(result, 2)
        self.assertNotIn("merge", [call[1] for call in calls])
        self.assertIn("rules error", error)

    def test_zero_merge_exit_requires_observed_completion(self):
        for key, value in (("state", "OPEN"), ("headRefOid", "c" * 40), ("number", 42),
                           ("mergeCommit", None), ("mergeCommit", {"oid": "invalid"})):
            with self.subTest(key=key, value=value):
                merged = copy.deepcopy(MERGED)
                merged[key] = value
                result, calls, output, error = self.invoke(merged=merged, execute=True)
                self.assertEqual(result, 2)
                self.assertEqual([call[1] for call in calls].count("merge"), 1)
                self.assertEqual(json.loads(output.splitlines()[-1])["observedAfterMerge"], merged)
                self.assertIn("outcome unknown", error)

    def test_read_failure_stops_and_merge_failures_are_not_retried(self):
        result, calls, output, error = self.invoke(execute=True, read_exit=1)
        self.assertEqual(result, 2)
        self.assertEqual([call[1] for call in calls], ["view"])
        self.assertIn("read error", error)
        result, calls, output, error = self.invoke(execute=True, merge_exit=7)
        self.assertEqual(result, 7)
        self.assertEqual([call[1] for call in calls].count("merge"), 1)
        self.assertEqual(json.loads(output.splitlines()[-1])["stderr"], "merge error")
        result, calls, output, error = self.invoke(execute=True, merge_timeout=True)
        self.assertEqual(result, 2)
        self.assertEqual([call[1] for call in calls].count("merge"), 1)
        self.assertIn("outcome unknown", error)

    def test_transient_merge_failure_waits_and_calls_the_same_merge_again(self):
        # #1223: #1259 got 502, then "Merge already in progress", then merged on the third call.
        result, calls, output, error = self.invoke(execute=True, merge_exit=[BAD_GATEWAY, IN_PROGRESS, (0, "")],
                                                  merged=[STILL_OPEN, STILL_OPEN, MERGED])
        self.assertEqual(result, 0, error)
        merges = [call for call in calls if call[:2] == ["pr", "merge"]]
        self.assertEqual(merges, [["pr", "merge", "900", "--repo", REPOSITORY, "--squash",
                                   "--match-head-commit", HEAD, "--subject", TITLE]] * 3)
        self.assertEqual(sum("POST" in call for call in calls), 1)
        self.assertFalse(any(call[0] == "run" for call in calls))
        self.assertEqual(self.sleeps, [MERGE.MERGE_RETRY_SECONDS] * 2)
        self.assertEqual(MERGE.MERGE_RETRY_SECONDS, 30)
        lines = [json.loads(line) for line in output.splitlines()]
        self.assertEqual([line["mergeRetry"] for line in lines if "mergeRetry" in line], [1, 2])
        self.assertEqual(lines[-1]["observedAfterMerge"], MERGED)

    def test_failed_merge_call_observed_as_merged_goes_on_to_the_post_merge_check(self):
        result, calls, output, error = self.invoke(execute=True, merge_exit=[BAD_GATEWAY], merged=[MERGED, MERGED])
        self.assertEqual(result, 0, error)
        self.assertEqual([call[1] for call in calls].count("merge"), 1)
        self.assertEqual(self.sleeps, [])
        self.assertEqual(json.loads(output.splitlines()[-1])["observedAfterMerge"], MERGED)
        # The post-merge check still applies: a wrong merge commit stops with "outcome unknown".
        result, calls, output, error = self.invoke(execute=True, merge_exit=[BAD_GATEWAY],
                                                  merged=[MERGED, dict(MERGED, mergeCommit=None)])
        self.assertEqual(result, 2)
        self.assertIn("outcome unknown", error)

    def test_failed_merge_call_with_another_head_or_state_stops(self):
        for observed in (dict(STILL_OPEN, headRefOid="c" * 40), dict(STILL_OPEN, state="CLOSED"),
                         dict(STILL_OPEN, number=42), None):
            with self.subTest(observed=observed):
                result, calls, output, error = self.invoke(execute=True, merge_exit=[IN_PROGRESS],
                                                          merged=[observed])
                self.assertEqual(result, 2)
                self.assertEqual([call[1] for call in calls].count("merge"), 1)
                self.assertEqual(self.sleeps, [])
                self.assertIn("after a failed merge call", error)
                self.assertIn("merge outcome unknown: inspect the PR", error)

    def test_merge_retries_stop_after_ten(self):
        result, calls, output, error = self.invoke(execute=True, merge_exit=[IN_PROGRESS] * 11,
                                                  merged=[STILL_OPEN] * 11)
        self.assertEqual(result, 2)
        self.assertEqual([call[1] for call in calls].count("merge"), 1 + MERGE.MERGE_RETRIES)
        self.assertEqual(MERGE.MERGE_RETRIES, 10)
        self.assertEqual(self.sleeps, [30] * 10)
        self.assertEqual(sum("POST" in call for call in calls), 1)
        self.assertIn("still failing after 10 retries", error)
        self.assertIn("merge outcome unknown", error)

    def test_other_merge_failures_are_returned_without_reading_or_waiting(self):
        for stderr in ("GraphQL: Pull Request is not mergeable (mergePullRequest)",
                       "Head branch was modified. Review and try the merge again.", "HTTP 422"):
            with self.subTest(stderr=stderr):
                result, calls, output, error = self.invoke(execute=True, merge_exit=[(1, stderr)])
                self.assertEqual(result, 1)
                self.assertEqual([call[1] for call in calls if call[0] == "pr"], ["view", "view", "merge"])
                self.assertEqual(self.sleeps, [])

    def test_squash_post_merge_read_is_not_waited_on(self):
        # #1345 changes only the integration path: the squash path stops at once as before.
        result, calls, output, error = self.invoke(execute=True, merged=[subprocess.TimeoutExpired(["gh"], 60)])
        self.assertEqual(result, 2)
        self.assertEqual(self.sleeps, [])
        self.assertEqual([call[1] for call in calls].count("merge"), 1)
        self.assertNotIn("unfinished", output)
        self.assertIn("merge outcome unknown: inspect the PR", error)

    def assert_read_waited_once(self, result, calls, output, error, reason):
        self.assertEqual(result, 0, error)
        self.assertEqual([call[1] for call in calls].count("merge"), 1)
        self.assertEqual(sum("POST" in call for call in calls), 1)
        self.assertEqual(self.sleeps, [MERGE.MERGE_RETRY_SECONDS])
        retries = [line for line in map(json.loads, output.splitlines()) if "readRetry" in line]
        self.assertEqual([line["readRetry"] for line in retries], [1])
        self.assertEqual(retries[0]["waitSeconds"], MERGE.MERGE_RETRY_SECONDS)
        self.assertIn(reason, retries[0]["reason"])

    def test_transient_read_before_the_merge_is_waited_on_and_merges(self):
        # #1501: a read before the merge (PR, rules, file list, CI run) waits on the same table as #1223 / #1345.
        reasons = {"cut body": "unexpected end of JSON input", "5xx": "HTTP 502", "timeout": "TimeoutExpired",
                   "not JSON": "not JSON"}
        for marker in ("pr view", "rules/branches/main", "/files?", "/actions/runs/"):
            for kind, failure in TRANSIENT_READS.items():
                with self.subTest(marker=marker, kind=kind):
                    result, calls, output, error = self.invoke(execute=True, read_failures={marker: [failure]})
                    self.assert_read_waited_once(result, calls, output, error, reasons[kind])

    def test_transient_run_view_during_the_watch_is_waited_on(self):
        result, calls, output, error = self.invoke(run=[{"status": "in_progress"}, RUN], execute=True,
                                                  watch_exit=[1, 0], run_view=[{"status": "in_progress"}],
                                                  read_failures={"run view": [CUT_BODY]})
        self.assert_read_waited_once(result, calls, output, error, "unexpected end of JSON input")
        # #667 is unchanged: the same run is watched again, no new run is dispatched.
        self.assertEqual(sum(call[:2] == ["run", "watch"] for call in calls), 2)
        self.assertIn({"ciWatchRetried": 1}, [json.loads(line) for line in output.splitlines()])

    def test_read_before_the_merge_failing_on_every_wait_is_still_failing(self):
        result, calls, output, error = self.invoke(execute=True,
                                                  read_failures={"rules/branches/main": [CUT_BODY] * 11})
        self.assertEqual(result, 2)
        self.assertEqual(sum(call[-1].endswith("rules/branches/main") for call in calls), 1 + MERGE.MERGE_RETRIES)
        self.assertEqual(self.sleeps, [MERGE.MERGE_RETRY_SECONDS] * MERGE.MERGE_RETRIES)
        self.assertNotIn("merge", [call[1] for call in calls])
        self.assertIn("still failing after 10 retries", error)
        self.assertNotIn("outcome unknown", error)

    def test_read_before_the_merge_that_is_not_transient_stops_at_once(self):
        for failure in (NOT_FOUND, (1, "HTTP 403: Resource not accessible by integration\n")):
            with self.subTest(failure=failure):
                result, calls, output, error = self.invoke(execute=True,
                                                          read_failures={"rules/branches/main": [failure]})
                self.assertEqual(result, 2)
                self.assertEqual(self.sleeps, [])
                self.assertEqual(sum(call[-1].endswith("rules/branches/main") for call in calls), 1)
                self.assertNotIn("merge", [call[1] for call in calls])
                self.assertIn(failure[1].strip(), error)

    def test_ci_dispatch_is_never_called_again(self):
        # The POST that starts CI is not a read: a cut or unreadable answer stops without a second run.
        for failure in (CUT_BODY, (0, '{"workflow_run_id": 10'), subprocess.TimeoutExpired(["gh"], 60)):
            with self.subTest(failure=failure):
                result, calls, output, error = self.invoke(execute=True, read_failures={"/dispatches": [failure]})
                self.assertEqual(result, 2)
                self.assertEqual(sum("POST" in call for call in calls), 1)
                self.assertEqual(self.sleeps, [])
                self.assertNotIn("merge", [call[1] for call in calls])

    def test_process_boundary_does_not_use_shell_or_rewrite_subject(self):
        command = ["pr", "merge", "900", "--repo", REPOSITORY, "--squash", "--match-head-commit", HEAD, "--subject", TITLE]
        with patch.object(MERGE.subprocess, "run") as run:
            MERGE.run_gh(command)
        run.assert_called_once_with(["gh", *command], cwd=ROOT, shell=False, capture_output=True,
                                    text=True, encoding="utf-8", timeout=60, check=False)

    def test_cli_rejects_ambiguous_target_without_network(self):
        for arguments in (["0", "--expected-head", HEAD], ["900", "--expected-head", "abc"],
                          ["900", "--expected-head", HEAD, "--subject", TITLE]):
            with self.subTest(arguments=arguments), patch.object(MERGE, "run_gh") as runner:
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                    MERGE.main(arguments, runner)
                self.assertEqual(error.exception.code, 2)
                runner.assert_not_called()


class IntegrationMerge(unittest.TestCase):
    """ADR 0021 追補 決定 3 / 6: the integration branch is rebase-merged once after one CI run."""

    invoke = MergeBoundary.invoke

    def integrate(self, **options):
        return self.invoke(integration=True, rules=options.pop("rules", INTEGRATION_RULES), **options)

    def assert_refused(self, result, calls, error, reason):
        self.assertEqual(result, 2)
        self.assertNotIn("merge", [call[1] for call in calls])
        self.assertIn("Merge refused:", error)
        self.assertIn(reason, error)

    def test_plan_prints_rebase_without_subject_and_commit_count(self):
        result, calls, output, error = self.integrate()
        self.assertEqual(result, 0, error)
        self.assertEqual([call[1] for call in calls if call[0] == "pr"], ["view"])
        self.assertFalse(any("POST" in call for call in calls))
        plan = json.loads(output)
        self.assertTrue(plan["integration"])
        self.assertEqual(plan["commits"], 3)
        self.assertEqual(plan["command"], ["gh", "pr", "merge", "900", "--repo", REPOSITORY, "--rebase",
                                           "--match-head-commit", HEAD])

    def test_execute_runs_ci_once_merges_once_and_closes_the_stacked_prs(self):
        result, calls, output, error = self.integrate(execute=True)
        self.assertEqual(result, 0, error)
        self.assertEqual(sum("POST" in call for call in calls), 1)
        merges = [call for call in calls if call[:2] == ["pr", "merge"]]
        self.assertEqual(merges, [["pr", "merge", "900", "--repo", REPOSITORY, "--rebase",
                                   "--match-head-commit", HEAD]])
        self.assertEqual(sum(f"compare/main...{HEAD}" in " ".join(call) for call in calls), 2)
        closes = [call for call in calls if call[:2] == ["pr", "close"]]
        self.assertEqual([call[2] for call in closes], ["1157", "1160"])
        for call, sha in zip(closes, MAIN_SHAS):
            self.assertEqual(call[3:6], ["--repo", REPOSITORY, "--comment"])
            self.assertEqual(call[6], f"まとめ枝 #900 の rebase merge で main に入った（commit {sha}・ADR 0021 追補・#1220）")
        self.assertGreater(calls.index(closes[0]), calls.index(merges[0]))
        final = json.loads(output.splitlines()[-1])
        self.assertEqual(final, {"integration": True, "commits": 3,
                                 "closed": [{"number": 1157, "mainCommit": MAIN_SHAS[0]},
                                            {"number": 1160, "mainCommit": MAIN_SHAS[1]}]})
        # #1223: a commit without its `PR #K（head <sha>）` line is reported, not skipped silently.
        self.assertEqual(json.loads(output.splitlines()[-2]), {"notClosed": [UNSOURCED]})

    def test_changed_or_closed_source_prs_are_reported_not_closed(self):
        sources = {"1157": {"headRefOid": "f" * 40, "state": "OPEN"},
                   "1160": {"headRefOid": SOURCE_HEADS["1160"], "state": "CLOSED"}}
        result, calls, output, error = self.integrate(execute=True, sources=sources)
        self.assertEqual(result, 0, error)
        self.assertFalse(any(call[:2] == ["pr", "close"] for call in calls))
        lines = [json.loads(line) for line in output.splitlines()]
        self.assertEqual([entry.get("number") for entry in lines[-2]["notClosed"]], [1157, 1160, None])
        self.assertEqual(lines[-1]["closed"], [])

    def test_ruleset_without_rebase_is_refused(self):
        for rules in (RULES, [{"type": "pull_request", "parameters": {"allowed_merge_methods": ["squash"]}}],
                      [{"type": "pull_request", "parameters": None}]):
            with self.subTest(rules=rules):
                result, calls, output, error = self.integrate(rules=rules, execute=True)
                self.assert_refused(result, calls, error, "ruleset")
                self.assertEqual(output, "")

    def test_commit_that_is_not_git_003_or_does_not_close_its_issue_is_refused(self):
        bad = {"subject": stacked("Merge branch 'main' into chore/42", 42),
               "english": stacked("fix(ui): fix printing (#1158)", 1158),
               "no closes": {"sha": "e" * 40, "commit": {"message": "fix(ui): 余白を直す (#1158)\n\n本文\n"}},
               "other number": stacked("fix(ui): 余白を直す (#1158)", 1159),
               "extra closes": {"sha": "e" * 40, "commit": {
                   "message": "fix(ui): 余白を直す (#1158)\n\nCloses #1158\nCloses #1159\n"}},
               "malformed": {"sha": "e" * 40}}
        for name, entry in bad.items():
            with self.subTest(name=name):
                commits = [[COMMITS[0][0], entry], COMMITS[1]]
                result, calls, output, error = self.integrate(commits=commits, execute=True)
                self.assert_refused(result, calls, error, "ommit")
                self.assertFalse(any("POST" in call for call in calls))
        result, calls, output, error = self.integrate(commits=[[]], execute=True)
        self.assert_refused(result, calls, error, "no commits")

    def test_branch_behind_main_or_with_a_different_count_is_refused(self):
        for compare, reason in ((dict(COMPARE, behind_by=1), "behind main"),
                                (dict(COMPARE, ahead_by=4), "ahead of main"),
                                (dict(COMPARE, behind_by="0"), "malformed"),
                                ({"behind_by": 0, "ahead_by": 3}, "malformed"),
                                (dict(COMPARE, merge_base_commit={"sha": "abc"}), "malformed")):
            with self.subTest(compare=compare):
                result, calls, output, error = self.integrate(compare=compare, execute=True)
                self.assert_refused(result, calls, error, reason)
                self.assertFalse(any("POST" in call for call in calls))

    def test_main_moving_during_ci_is_refused_before_merge(self):
        result, calls, output, error = self.integrate(compare=[COMPARE, dict(COMPARE, behind_by=1)],
                                                      execute=True)
        self.assert_refused(result, calls, error, "behind main")
        self.assertEqual(sum("POST" in call for call in calls), 1)
        self.assertNotIn("outcome unknown", error)

    def test_transient_integration_read_before_the_merge_is_waited_on(self):
        # #1501: the commit list and both compares before the merge wait like the reads after it (#1345).
        for marker, failures in (("/pulls/900/commits?", [CUT_BODY]), (f"/compare/main...{HEAD}", [CUT_BODY]),
                                 (f"/compare/main...{HEAD}", [(0, "")])):
            with self.subTest(marker=marker, failures=failures):
                result, calls, output, error = self.integrate(execute=True, read_failures={marker: failures})
                MergeBoundary.assert_read_waited_once(self, result, calls, output, error,
                                                      "unexpected end of JSON input" if failures[0][0] else "not JSON")
                self.assertEqual(json.loads(output.splitlines()[-1])["closed"],
                                 [{"number": 1157, "mainCommit": MAIN_SHAS[0]},
                                  {"number": 1160, "mainCommit": MAIN_SHAS[1]}])

    def test_ci_failure_never_merges(self):
        result, calls, output, error = self.integrate(run=dict(RUN, conclusion="failure"), execute=True)
        self.assert_refused(result, calls, error, "CI")

    def test_landed_commit_count_mismatch_asks_to_inspect_main(self):
        for landed in (dict(LANDED, total_commits=4), dict(LANDED, total_commits=2), None, {"total_commits": 3}):
            with self.subTest(landed=landed):
                result, calls, output, error = self.integrate(landed=landed if landed is not None else [],
                                                              execute=True)
                self.assertEqual(result, 2)
                self.assertEqual([call[1] for call in calls].count("merge"), 1)
                self.assertFalse(any(call[:2] == ["pr", "close"] for call in calls))
                self.assertIn("merge outcome unknown: inspect main", error)

    def test_transient_merge_failure_is_waited_on_without_new_ci(self):
        result, calls, output, error = self.integrate(execute=True, merge_exit=[BAD_GATEWAY, (0, "")],
                                                      merged=[STILL_OPEN, MERGED])
        self.assertEqual(result, 0, error)
        self.assertEqual([call for call in calls if call[:2] == ["pr", "merge"]],
                         [["pr", "merge", "900", "--repo", REPOSITORY, "--rebase", "--match-head-commit", HEAD]] * 2)
        self.assertEqual(sum("POST" in call for call in calls), 1)
        self.assertEqual(self.sleeps, [30])
        self.assertEqual([call[2] for call in calls if call[:2] == ["pr", "close"]], ["1157", "1160"])

    def test_other_failure_after_a_wait_reads_the_pr_and_continues_when_merged(self):
        # V-1263 D-1: the earlier merge finished while waiting; the next call fails as "already merged".
        result, calls, output, error = self.integrate(
            execute=True, merge_exit=[IN_PROGRESS, (1, "GraphQL: Pull request is already merged (mergePullRequest)")],
            merged=[STILL_OPEN, MERGED, MERGED])
        self.assertEqual(result, 0, error)
        self.assertEqual([call[1] for call in calls].count("merge"), 2)
        self.assertEqual(self.sleeps, [30])
        self.assertEqual([call[2] for call in calls if call[:2] == ["pr", "close"]], ["1157", "1160"])
        # Still OPEN: the failure is returned as before.
        result, calls, output, error = self.integrate(
            execute=True, merge_exit=[IN_PROGRESS, (1, "GraphQL: Pull Request is not mergeable")],
            merged=[STILL_OPEN, STILL_OPEN])
        self.assertEqual(result, 1)
        self.assertFalse(any(call[:2] == ["pr", "close"] for call in calls))

    def test_main_moving_while_waiting_stops_before_the_next_call(self):
        # V-1263 F-3: compare is read again before each repeated call.
        result, calls, output, error = self.integrate(
            execute=True, merge_exit=[IN_PROGRESS], merged=[STILL_OPEN],
            compare=[COMPARE, COMPARE, dict(COMPARE, behind_by=1)])
        self.assertEqual(result, 2)
        self.assertEqual([call[1] for call in calls].count("merge"), 1)
        self.assertEqual(sum(f"compare/main...{HEAD}" in " ".join(call) for call in calls), 3)
        self.assertIn("behind main", error)
        self.assertIn("merge outcome unknown", error)

    def test_closing_keywords_for_another_issue_are_refused(self):
        # #1223: GitHub closes on close/closes/closed/fix/fixes/fixed/resolve/resolves/resolved in any case.
        for line in ("Fixes #1159", "closes #1159", "RESOLVED: #1159", "fix #1159", "Closed #1159",
                     "resolves hideyukiMORI/nene-nib#1159", "fixed https://github.com/hideyukiMORI/nene-nib/issues/1159",
                     "* fix(ui): 余白 (#1158) fixes #1159"):
            with self.subTest(line=line):
                entry = stacked("fix(ui): 印刷の余白を直す (#1158)", 1158, "1160")
                entry["commit"]["message"] += line + "\n"
                result, calls, output, error = self.integrate(commits=[[COMMITS[0][0], entry], COMMITS[1]],
                                                              execute=True)
                self.assert_refused(result, calls, error, "closing keywords")
                self.assertFalse(any("POST" in call for call in calls))
        for line in ("Fixes #1158", "resolved #1158", "prefix #1159", "unresolved #1159", "fix(ui): x (#1159)"):
            with self.subTest(line=line):
                entry = stacked("fix(ui): 印刷の余白を直す (#1158)", 1158, "1160")
                entry["commit"]["message"] += line + "\n"
                commits = [[COMMITS[0][0], entry], COMMITS[1]]
                self.assertEqual(len(MERGE.integration_commits(commits)), 3)

    def test_more_commits_than_github_lists_is_refused_with_the_limit(self):
        with self.assertRaisesRegex(MERGE.MergeRefused, "at most 250"):
            MERGE.check_contains_main(dict(COMPARE, ahead_by=251), 250)
        with self.assertRaises(MERGE.MergeRefused) as refused:
            MERGE.check_contains_main(dict(COMPARE, ahead_by=4), 3)
        self.assertNotIn("at most", str(refused.exception))
        self.assertEqual(MERGE.check_contains_main(dict(COMPARE, ahead_by=250), 250), MERGE_BASE)

    def test_main_commits_are_matched_by_stacked_order_not_subject(self):
        # #1223: two stacked PRs with the same title each get their own main commit.
        same = "fix(ui): 印刷の余白を直す (#1158)"
        commits = [[stacked(same, 1158, "1157"), stacked(same, 1158, "1160")], COMMITS[1]]
        landed = {"total_commits": 3, "commits": [
            {"sha": sha, "commit": {"message": entry["commit"]["message"]}}
            for sha, entry in zip(MAIN_SHAS, [entry for page in commits for entry in page])]}
        result, calls, output, error = self.integrate(commits=commits, landed=landed, execute=True)
        self.assertEqual(result, 0, error)
        self.assertEqual(json.loads(output.splitlines()[-1])["closed"],
                         [{"number": 1157, "mainCommit": MAIN_SHAS[0]}, {"number": 1160, "mainCommit": MAIN_SHAS[1]}])

    def test_main_commit_with_another_subject_or_missing_is_reported_not_closed(self):
        landed = copy.deepcopy(LANDED)
        landed["commits"][1]["commit"]["message"] = "fix(ui): 別の題 (#1158)\n"
        for value, reason in ((landed, "main commit 2"), (dict(LANDED, commits=LANDED["commits"][:1]), "main commit 2")):
            with self.subTest(reason=reason):
                result, calls, output, error = self.integrate(landed=value, execute=True)
                self.assertEqual(result, 0, error)
                self.assertEqual([call[2] for call in calls if call[:2] == ["pr", "close"]], ["1157"])
                not_closed = json.loads(output.splitlines()[-2])["notClosed"]
                self.assertEqual(not_closed[0]["number"], 1160)
                self.assertIn(reason, not_closed[0]["reason"])
                self.assertEqual(not_closed[1:], [UNSOURCED])

    def test_failed_close_is_reported_and_the_rest_continue(self):
        for close_exit, reason, tries in ((1, "close failed: close error", 1),
                                          (subprocess.TimeoutExpired(["gh"], 60), "TimeoutExpired", 11),
                                          ((1, "HTTP 502: Bad Gateway"), "HTTP 502", 11),
                                          (OSError("gh is missing"), "OSError: gh is missing", 1)):
            with self.subTest(reason=reason):
                result, calls, output, error = self.integrate(execute=True, close_exit=close_exit)
                self.assertEqual(result, 0, error)
                # #1345: a timeout or a 5xx is waited on while the PR stays OPEN at its head (10 retries).
                self.assertEqual([call[2] for call in calls if call[:2] == ["pr", "close"]],
                                 ["1157"] * tries + ["1160"] * tries)
                lines = [json.loads(line) for line in output.splitlines()]
                self.assertEqual([entry.get("number") for entry in lines[-2]["notClosed"]], [1157, 1160, None])
                self.assertIn(reason, lines[-2]["notClosed"][0]["reason"])
                self.assertEqual(lines[-1]["closed"], [])

    def test_failed_source_read_is_reported_and_the_next_pr_is_closed(self):
        for failure in (OSError("gh is missing"), subprocess.TimeoutExpired(["gh"], 60)):
            with self.subTest(failure=type(failure).__name__):
                sources = dict(SOURCES, **{"1157": failure})
                result, calls, output, error = self.integrate(execute=True, sources=sources)
                self.assertEqual(result, 0, error)
                lines = [json.loads(line) for line in output.splitlines()]
                self.assertEqual(lines[-2]["notClosed"][0]["number"], 1157)
                self.assertIn(type(failure).__name__, lines[-2]["notClosed"][0]["reason"])
                self.assertEqual(lines[-1]["closed"], [{"number": 1160, "mainCommit": MAIN_SHAS[1]}])


    # #1345: the post-merge reads and the closing wait on a timeout or a 5xx; a stop says how far it got.
    def test_post_merge_compare_timing_out_once_is_waited_on_and_closes(self):
        for failure in (COMPARE_TIMEOUT, (1, "gh: Bad Gateway (HTTP 502)")):
            with self.subTest(failure=failure):
                result, calls, output, error = self.integrate(execute=True, landed_failures=[failure])
                self.assertEqual(result, 0, error)
                self.assertEqual([call[1] for call in calls].count("merge"), 1)
                self.assertEqual(sum("POST" in call for call in calls), 1)
                self.assertEqual(self.sleeps, [MERGE.MERGE_RETRY_SECONDS])
                lines = [json.loads(line) for line in output.splitlines()]
                self.assertEqual([line["readRetry"] for line in lines if "readRetry" in line], [1])
                self.assertEqual([call[2] for call in calls if call[:2] == ["pr", "close"]], ["1157", "1160"])
                self.assertNotIn("resume", lines[-1])

    def test_post_merge_compare_timing_out_on_every_wait_prints_progress_and_the_resume(self):
        result, calls, output, error = self.integrate(execute=True, landed_failures=[COMPARE_TIMEOUT] * 11)
        self.assertEqual(result, 2)
        self.assertEqual([call[1] for call in calls].count("merge"), 1)
        self.assertFalse(any(call[:2] == ["pr", "close"] for call in calls))
        self.assertEqual(self.sleeps, [30] * 10)
        self.assertIn("still failing after 10 retries", error)
        self.assertIn("merge outcome unknown: inspect main", error)
        stopped = json.loads(output.splitlines()[-1])
        self.assertEqual(stopped["unfinished"], {"number": 900, "mergeBase": MERGE_BASE, "merged": True,
                                                 "mergeCommit": "b" * 40, "mainChecked": False, "closed": [],
                                                 "notClosed": "all stacked PRs"})
        self.assertEqual(stopped["resume"], ["python", "eng/merge-pr.py", "900", "--expected-head", HEAD,
                                             "--integration", "--resume-after-merge", "--merge-base", MERGE_BASE,
                                             "--execute"])

    def test_post_merge_read_failing_for_another_reason_is_not_waited_on_and_has_no_resume(self):
        result, calls, output, error = self.integrate(execute=True, landed_failures=[(1, "HTTP 404: Not Found")])
        self.assertEqual(result, 2)
        self.assertEqual(self.sleeps, [])
        stopped = json.loads(output.splitlines()[-1])
        self.assertTrue(stopped["unfinished"]["merged"])
        self.assertNotIn("resume", stopped)

    def test_observed_after_merge_timing_out_once_is_waited_on(self):
        result, calls, output, error = self.integrate(
            execute=True, merged=[subprocess.TimeoutExpired(["gh"], 60), MERGED])
        self.assertEqual(result, 0, error)
        self.assertEqual(self.sleeps, [30])
        self.assertEqual([call[2] for call in calls if call[:2] == ["pr", "close"]], ["1157", "1160"])

    def test_close_timing_out_then_observed_closed_is_not_closed_again(self):
        closed = {"headRefOid": SOURCE_HEADS["1157"], "state": "CLOSED"}
        sources = dict(SOURCES, **{"1157": [SOURCES["1157"], closed]})
        result, calls, output, error = self.integrate(
            execute=True, sources=sources, close_exit=[subprocess.TimeoutExpired(["gh"], 60), 0])
        self.assertEqual(result, 0, error)
        self.assertEqual([call[2] for call in calls if call[:2] == ["pr", "close"]], ["1157", "1160"])
        self.assertEqual(self.sleeps, [])
        self.assertEqual(json.loads(output.splitlines()[-1])["closed"],
                         [{"number": 1157, "mainCommit": MAIN_SHAS[0]}, {"number": 1160, "mainCommit": MAIN_SHAS[1]}])

    def test_resume_closes_only_the_stacked_prs_still_open_and_never_merges(self):
        sources = dict(SOURCES, **{"1157": {"headRefOid": SOURCE_HEADS["1157"], "state": "CLOSED"}})
        result, calls, output, error = self.integrate(execute=True, resume=True, landed=RESUME_LANDED,
                                                      sources=sources)
        self.assertEqual(result, 0, error)
        self.assertFalse(any(call[:2] == ["pr", "merge"] or call[0] == "run" or "POST" in call for call in calls))
        self.assertEqual([call[2] for call in calls if call[:2] == ["pr", "close"]], ["1160"])
        final = json.loads(output.splitlines()[-1])
        self.assertEqual(final["closed"], [{"number": 1157, "mainCommit": MAIN_SHAS[0], "alreadyClosed": True},
                                           {"number": 1160, "mainCommit": MAIN_SHAS[1]}])
        self.assertNotIn("resume", final)
        # Without --execute the resume only checks main and lists the stacked PRs.
        result, calls, output, error = self.integrate(resume=True, landed=RESUME_LANDED)
        self.assertEqual(result, 0, error)
        self.assertFalse(any(call[:2] in (["pr", "merge"], ["pr", "close"]) for call in calls))
        self.assertEqual(json.loads(output.splitlines()[-1])["sources"], [1157, 1160])

    def test_resume_closes_nothing_unless_main_holds_the_stacked_commits(self):
        other_subject = copy.deepcopy(RESUME_LANDED)
        other_subject["commits"][1]["commit"]["message"] = "fix(ui): 別の題 (#1158)\n"
        other_head = copy.deepcopy(RESUME_LANDED)
        other_head["commits"][0]["commit"]["message"] = \
            other_head["commits"][0]["commit"]["message"].replace(SOURCE_HEADS["1157"], "f" * 40)
        no_source = copy.deepcopy(RESUME_LANDED)
        no_source["commits"][1]["commit"]["message"] = "fix(ui): 印刷の余白を直す (#1158)\n\nCloses #1158\n"
        cases = {"subject": {"landed": other_subject}, "source head": {"landed": other_head},
                 "source line": {"landed": no_source},
                 "count": {"landed": dict(RESUME_LANDED, total_commits=4)},
                 "fewer": {"landed": dict(RESUME_LANDED, total_commits=2, commits=RESUME_LANDED["commits"][:2])},
                 "last sha": {"landed": LANDED},
                 "not on main": {"landed": RESUME_LANDED, "on_main": dict(ON_MAIN, behind_by=1)},
                 "not merged": {"landed": RESUME_LANDED, "merged": STILL_OPEN},
                 "other head": {"landed": RESUME_LANDED, "merged": dict(MERGED, headRefOid="c" * 40)}}
        for name, options in cases.items():
            with self.subTest(name=name):
                result, calls, output, error = self.integrate(execute=True, resume=True, **options)
                self.assertEqual(result, 2)
                self.assertFalse(any(call[:2] in (["pr", "merge"], ["pr", "close"]) for call in calls))
                self.assertIn("Resume refused:", error)
                self.assertIn("nothing was closed", error)

    def test_resume_options_are_checked_without_network(self):
        for arguments in (["--resume-after-merge", "--merge-base", MERGE_BASE],
                          ["--integration", "--resume-after-merge"],
                          ["--integration", "--resume-after-merge", "--merge-base", MERGE_BASE, "--ci-run", "5"],
                          ["--integration", "--merge-base", MERGE_BASE],
                          ["--integration", "--resume-after-merge", "--merge-base", "abc"]):
            with self.subTest(arguments=arguments), patch.object(MERGE, "run_gh") as runner:
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                    MERGE.main(["900", "--expected-head", HEAD, *arguments], runner)
                self.assertEqual(error.exception.code, 2)
                runner.assert_not_called()

if __name__ == "__main__":
    unittest.main()
