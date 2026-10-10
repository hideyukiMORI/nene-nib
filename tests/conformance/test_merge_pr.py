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
RECORD = {"number": 900, "headRefOid": HEAD, "state": "OPEN", "isDraft": False,
          "baseRefName": "main", "title": TITLE, "body": BODY}
CHECKS = [{"name": "check", "state": "SUCCESS", "link": "https://example.invalid/check"}]
REPOSITORY = "github.com/hideyukiMORI/nene-nib"
RULES = [{"type": "pull_request"}, {"type": "required_status_checks"}]
MERGED = {"number": 900, "headRefOid": HEAD, "state": "MERGED",
          "mergeCommit": {"oid": "b" * 40}, "url": "https://github.com/hideyukiMORI/nene-nib/pull/900"}


class MergeBoundary(unittest.TestCase):
    def invoke(self, record=None, checks=None, execute=False, read_exit=0, merge_exit=0,
               merge_timeout=False, rules=None, merged=None, rules_exit=0):
        calls = []

        def runner(arguments):
            calls.append(arguments)
            if arguments[0] == "api":
                return subprocess.CompletedProcess(arguments, rules_exit,
                                                   json.dumps(RULES if rules is None else rules),
                                                   "rules error" if rules_exit else "")
            if arguments[1] == "view":
                if any(call[1] == "merge" for call in calls):
                    return subprocess.CompletedProcess(arguments, 0, json.dumps(MERGED if merged is None else merged), "")
                return subprocess.CompletedProcess(arguments, read_exit,
                                                   json.dumps(RECORD if record is None else record),
                                                   "read error" if read_exit else "")
            if arguments[1] == "checks":
                return subprocess.CompletedProcess(arguments, 0, json.dumps(CHECKS if checks is None else checks), "")
            if arguments[1] == "merge":
                if merge_timeout:
                    raise subprocess.TimeoutExpired(arguments, 60)
                return subprocess.CompletedProcess(arguments, merge_exit, "merge output", "merge error" if merge_exit else "")
            self.fail(f"Unexpected command: {arguments}")

        output, error = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(error):
            result = MERGE.main(["900", "--expected-head", HEAD, *(["--execute"] if execute else [])], runner)
        return result, calls, output.getvalue(), error.getvalue()

    def test_plan_never_mutates_and_keeps_issue_distinct_from_pr(self):
        result, calls, output, error = self.invoke()
        self.assertEqual(result, 0, error)
        self.assertEqual([call[1] for call in calls if call[0] == "pr"], ["view", "checks"])
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
        self.assertEqual([call[1] for call in calls if call[0] == "pr"], ["view", "checks", "merge", "view"])
        self.assertEqual(calls[-2], ["pr", "merge", "900", "--repo", REPOSITORY, "--squash",
                                     "--match-head-commit", HEAD, "--subject", TITLE])
        for call in calls:
            if call[0] == "pr":
                self.assertEqual(call[3:5], ["--repo", REPOSITORY])
            else:
                self.assertEqual(call, ["api", "--hostname", "github.com", "repos/hideyukiMORI/nene-nib/rules/branches/main"])
        self.assertEqual(json.loads(output.splitlines()[1])["mergeExit"], 0)
        self.assertEqual(json.loads(output.splitlines()[2])["observedAfterMerge"], MERGED)

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

    def test_required_checks_must_be_present_and_all_successful(self):
        for checks in ([], {}, [None], [{"state": "PENDING"}], [{"state": "FAILURE"}],
                       [{"state": "SKIPPED"}], [*CHECKS, {"state": "FAILURE"}]):
            with self.subTest(checks=checks):
                result, calls, output, error = self.invoke(checks=checks, execute=True)
                self.assertEqual(result, 2)
                self.assertNotIn("merge", [call[1] for call in calls])
                self.assertEqual(output, "")
                self.assertIn("Required checks", error)

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
                self.assertEqual(json.loads(output.splitlines()[2])["observedAfterMerge"], merged)
                self.assertIn("outcome unknown", error)

    def test_read_failure_stops_and_merge_failures_are_not_retried(self):
        result, calls, output, error = self.invoke(execute=True, read_exit=1)
        self.assertEqual(result, 2)
        self.assertEqual([call[1] for call in calls], ["view"])
        self.assertIn("read error", error)
        result, calls, output, error = self.invoke(execute=True, merge_exit=7)
        self.assertEqual(result, 7)
        self.assertEqual([call[1] for call in calls].count("merge"), 1)
        self.assertEqual(json.loads(output.splitlines()[1])["stderr"], "merge error")
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
