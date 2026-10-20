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
BODY = ("Closes #42\n確認する退行: 件名の転記\n対象・依存: 統合入口\n"
        "検証結果: 限定した道具試験\n再利用: 初回検証\n")
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


class MergeBoundary(unittest.TestCase):
    def invoke(self, record=None, execute=False, read_exit=0, merge_exit=0,
               merge_timeout=False, rules=None, merged=None, rules_exit=0, files=None, run=None,
               latest=None, ci_run=None, dispatch_exit=0, watch_exit=0, run_view=None):
        calls = []
        self.watch_timeouts = []
        run_responses = copy.deepcopy(run) if isinstance(run, list) else None
        watch_exits = list(watch_exit) if isinstance(watch_exit, list) else None
        view_responses = copy.deepcopy(run_view) if isinstance(run_view, list) else None

        def runner(arguments, timeout=None):
            calls.append(arguments)
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
            if arguments[1] == "view":
                if any(call[1] == "merge" for call in calls):
                    return subprocess.CompletedProcess(arguments, 0, json.dumps(MERGED if merged is None else merged), "")
                if latest is not None and sum(call[:2] == ["pr", "view"] for call in calls) > 1:
                    return subprocess.CompletedProcess(arguments, 0, json.dumps(latest), "")
                return subprocess.CompletedProcess(arguments, read_exit,
                                                   json.dumps(RECORD if record is None else record),
                                                   "read error" if read_exit else "")
            if arguments[1] == "merge":
                if merge_timeout:
                    raise subprocess.TimeoutExpired(arguments, 60)
                return subprocess.CompletedProcess(arguments, merge_exit, "merge output", "merge error" if merge_exit else "")
            self.fail(f"Unexpected command: {arguments}")

        output, error = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(error):
            result = MERGE.main(["900", "--expected-head", HEAD, *(["--execute"] if execute else []),
                                 *(["--ci-run", str(ci_run)] if ci_run else [])], runner)
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


if __name__ == "__main__":
    unittest.main()
