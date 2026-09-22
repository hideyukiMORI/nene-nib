"""Only the verification entry point and PR record wiring; never build/run the product (#61)."""

import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("git_conventions", ROOT / "eng/git-conventions.py")
CONVENTIONS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CONVENTIONS)
RECORD = ("確認する退行: 全件の誤起動と検証記録の欠落\n"
          "対象・依存: check.ps1、validate-git.ps1、git-conventions.py、CI\n"
          "検証結果: 対象を指定した unittest / 終了 0 / PR にログ\n"
          "再利用: 初回検証\n")


def run_tool(command, **environment):
    """子プロセスの入出力を端末符号化から切り離してここ 1 か所で UTF-8 に固定する（Issue #106）。

    text=True だけだと親は locale の符号化（この機械では cp932）で読むので、子が出す UTF-8 を
    読めずに reader thread が落ちて stdout / stderr が None になる。子には PYTHONUTF8=1 を渡して
    出力側も UTF-8 に寄せる（Issue #94 が validate-git.ps1 で入れたのと同じ固定）。
    pwsh のエラー表示だけはコンソールの code page（cp932 の省略記号など）で出るので、
    厳密に読まず errors="replace" にして規則 ID を読み落とさない（Issue #94）。
    """
    return subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace",
                          env=dict(os.environ, PYTHONUTF8="1", **environment))


class VerificationPolicy(unittest.TestCase):
    def test_record_and_reuse_are_accepted(self):
        self.assertEqual(CONVENTIONS.validate_pr(RECORD), [])
        reused = RECORD.replace("初回検証", "PR #61 の成功結果。実装と直接依存は不変、文書だけ変更")
        self.assertEqual(CONVENTIONS.validate_pr(reused.replace("\n", "\r\n")), [])

    def test_missing_or_blank_fields_are_rejected(self):
        for line in RECORD.splitlines():
            field = line.split(":", 1)[0]
            for replacement in ("", field + ":", field + ": \t"):
                with self.subTest(field=field, replacement=replacement):
                    errors = CONVENTIONS.validate_pr(RECORD.replace(line, replacement))
                    self.assertEqual(len(errors), 1)
                    self.assertIn(field, errors[0])

    def test_record_cli_returns_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            record = Path(directory) / "body.txt"
            record.write_text("確認する退行: only one field\n", encoding="utf-8")
            result = run_tool([sys.executable, str(ROOT / "eng/git-conventions.py"),
                               str(record), "--pr-body"])
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("GIT-004", result.stdout)

    def test_full_gate_requires_explicit_scope_and_reason(self):
        for arguments in ([], ["-Full"], ["-Full", "-Reason", " "], ["-Reason", "scope"]):
            with self.subTest(arguments=arguments):
                result = run_tool(["pwsh", "-NoProfile", "-File", str(ROOT / "eng/check.ps1"), *arguments])
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("QLT-001", result.stderr)

    def test_explicit_full_dispatches_before_stub_stops_it(self):
        # Copy the unchanged command, replacing only its toolchain in an isolated fixture.
        # The sentinel proves opt-in reaches the old path without executing any actual gate.
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory)
            shutil.copy2(ROOT / "eng/check.ps1", fixture / "check.ps1")
            (fixture / "toolchain.ps1").write_text("throw 'fixture-toolchain-stop'\n", encoding="utf-8")
            result = run_tool(["pwsh", "-NoProfile", "-File", str(fixture / "check.ps1"),
                               "-Full", "-Reason", "shared compiler configuration"])
            self.assertIn("Full verification selected: shared compiler configuration", result.stdout)
            self.assertIn("fixture-toolchain-stop", result.stderr)
            self.assertNotEqual(result.returncode, 0)

    def test_ci_and_hooks_do_not_invoke_product_checks(self):
        workflow = (ROOT / ".github/workflows/check.yml").read_text(encoding="utf-8")
        self.assertIn("./eng/validate-git.ps1", workflow)
        self.assertIn("git diff --check", workflow)
        for command in ("check.ps1", "ctest", "cmake --build", "measure-speed", "coverage.py"):
            self.assertNotIn(command, workflow)
        self.assertEqual((ROOT / ".githooks/pre-commit").read_text(encoding="utf-8").strip(),
                         "#!/bin/sh\nexec git diff --cached --check")
        self.assertFalse((ROOT / ".githooks/pre-push").exists())
        self.assertFalse((ROOT / ".githooks/pre-merge-commit").exists())

    def test_pr_event_uses_the_record_validator(self):
        title = "ci(verification): 差分に必要な検証を選ぶ (#61)"
        body = "Closes #61\n目的:\n使った正典経路:\n規則 ID:\nWaivers: none\n残るリスク:\n" + RECORD
        with tempfile.TemporaryDirectory() as directory:
            event = Path(directory) / "event.json"
            for text, success in ((body, True), (body.replace("再利用: 初回検証", "再利用:"), False)):
                event.write_text(json.dumps({"pull_request": {
                    "head": {"ref": "ci/61-diff-driven-verification"},
                    "base": {"sha": "origin/main"}, "draft": False, "title": title, "body": text,
                }}), encoding="utf-8")
                result = run_tool(["pwsh", "-NoProfile", "-File", str(ROOT / "eng/validate-git.ps1")],
                                  GITHUB_EVENT_PATH=str(event))
                with self.subTest(success=success):
                    self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
                    if not success:
                        self.assertIn("GIT-004", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
