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
RECORD = ("目的: 差分に必要な検証を選ぶ\n"
          "使った正典経路: eng/check.ps1 と eng/validate-git.ps1\n"
          "規則 ID: QLT-001 / QLT-012 / GIT-004\n"
          "振る舞い・スキーマの変更: なし\n"
          "検証（実行したコマンドと結果）: python eng/test-conformance.py → OK\n"
          "確認する退行: 全件の誤起動と検証記録の欠落\n"
          "対象・依存: check.ps1、validate-git.ps1、git-conventions.py、CI\n"
          "検証結果: 対象を指定した unittest / 終了 0 / PR にログ\n"
          "再利用: 初回検証\n"
          "Waivers: none\n"
          "残るリスク: なし\n")
BODY = "Closes #61\n" + RECORD


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
        self.assertEqual(CONVENTIONS.validate_pr(BODY), [])
        reused = BODY.replace("初回検証", "PR #61 の成功結果。実装と直接依存は不変、文書だけ変更")
        self.assertEqual(CONVENTIONS.validate_pr(reused.replace("\n", "\r\n")), [])

    def test_the_record_lists_every_field_of_the_one_table(self):
        # #710: CI と local は git-conventions.py の PR_FIELDS の 1 つを読む。validate-git.ps1 に欄を書かない。
        self.assertEqual([line.split(":", 1)[0] for line in RECORD.splitlines()], list(CONVENTIONS.PR_FIELDS))
        validator = (ROOT / "eng/validate-git.ps1").read_text(encoding="utf-8")
        for field in CONVENTIONS.PR_FIELDS:
            self.assertNotIn(f"'{field}:'", validator)

    def test_missing_or_blank_fields_are_rejected_by_name(self):
        for line in RECORD.splitlines():
            field = line.split(":", 1)[0]
            for replacement in ("", field + ":", field + ": \t", "  " + line, "- " + line):
                with self.subTest(field=field, replacement=replacement):
                    errors = CONVENTIONS.validate_pr(BODY.replace(line, replacement))
                    self.assertEqual(len(errors), 1)
                    self.assertIn("GIT-004", errors[0])
                    self.assertIn(field, errors[0])

    def test_heading_form_is_rejected_by_name(self):
        # #709: 見出しの形（## 目的）は CI で落ちた。local の --pr-body も同じに落とす。
        headed = "Closes #61\n" + "".join(f"## {field}\n{line.split(': ', 1)[1]}\n"
                                         for field, line in zip(CONVENTIONS.PR_FIELDS, RECORD.splitlines()))
        errors = CONVENTIONS.validate_pr(headed)
        self.assertEqual(len(errors), len(CONVENTIONS.PR_FIELDS))
        for field, error in zip(CONVENTIONS.PR_FIELDS, errors):
            self.assertIn(field, error)

    def test_closing_issue_must_start_a_line(self):
        for body in (RECORD, RECORD + "これは Closes #61 を含む\n"):
            with self.subTest(body=body):
                self.assertEqual(CONVENTIONS.validate_pr(body),
                                 ["GIT-001: PR body must close an Issue on a line starting with Closes #N"])

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
        self.assertIn("workflow_dispatch:", workflow)
        self.assertIn("-PullRequest $env:MERGE_PR -ExpectedHead $env:MERGE_HEAD", workflow)
        for trigger in ("pull_request:", "push:", "ready_for_review", "synchronize", "edited"):
            self.assertNotIn(trigger, workflow)
        self.assertIn('git diff --check "$base...HEAD"', (ROOT / "eng/validate-git.ps1").read_text(encoding="utf-8"))
        # ADR 0136（D46）: check job は規約と記録だけ。build-and-test job は PR の最終 HEAD を Release で
        # 全 target ビルドして ctest を回すが、全件ゲート・速さ・カバレッジ・Debug は呼ばない。
        check_job, build_job = workflow.split("\n  build-and-test:", 1)
        for command in ("check.ps1", "ctest", "cmake --build", "measure-speed", "coverage.py"):
            self.assertNotIn(command, check_job)
        # #568: check job は規約の検査より先に conformance の自己テストを回す（契約の遅れを merge 前に止める）。
        self.assertIn("python eng/test-conformance.py", check_job)
        self.assertLess(check_job.index("python eng/test-conformance.py"),
                        check_job.index("./eng/validate-git.ps1"))
        for command in ("cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release",
                        "cmake --build build-release", "ctest --test-dir build-release",
                        "build-release/.cmake/api/v1/query/codemodel-v2",
                        "python eng/symbols.py --build-dir build-release --require core application"):
            self.assertIn(command, build_job)
        # #1024: リンカのシンボルの検査（ARC-003）は Release のビルドの後に回す（ADR 0136 の決定 7）。
        self.assertLess(build_job.index("cmake --build build-release"),
                        build_job.index("python eng/symbols.py --build-dir build-release"))
        for command in ("check.ps1", "measure-speed", "coverage.py", "-DCMAKE_BUILD_TYPE=Debug"):
            self.assertNotIn(command, build_job)
        self.assertEqual((ROOT / ".githooks/pre-commit").read_text(encoding="utf-8").strip(),
                         "#!/bin/sh\nexec git diff --cached --check")
        self.assertFalse((ROOT / ".githooks/pre-push").exists())
        self.assertFalse((ROOT / ".githooks/pre-merge-commit").exists())

    def test_merge_dispatch_uses_the_record_validator_and_expected_head(self):
        title = "ci(verification): 差分に必要な検証を選ぶ (#61)"
        body = BODY
        with tempfile.TemporaryDirectory() as directory:
            event = Path(directory) / "event.json"
            head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
            command = ("function global:gh { Get-Content -LiteralPath $env:NIB_TEST_PR_RESPONSE -Raw; $global:LASTEXITCODE=0 }; "
                       "& $env:NIB_TEST_VALIDATOR -PullRequest 61 -ExpectedHead $env:NIB_TEST_HEAD")
            for text, pr_head, success in ((body, head, True), (body.replace("再利用: 初回検証", "再利用:"), head, False),
                                           (body.replace("目的:", "## 目的\n"), head, False),
                                           (body, "0" * 40, False)):
                event.write_text(json.dumps({"number": 61, "state": "open",
                    "head": {"ref": "ci/61-diff-driven-verification", "sha": pr_head,
                             "repo": {"full_name": "hideyukiMORI/nene-nib"}},
                    "base": {"sha": "origin/main", "ref": "main"}, "draft": False, "title": title, "body": text,
                }), encoding="utf-8")
                result = run_tool(["pwsh", "-NoProfile", "-Command", command],
                                  NIB_TEST_PR_RESPONSE=str(event), NIB_TEST_VALIDATOR=str(ROOT / "eng/validate-git.ps1"),
                                  NIB_TEST_HEAD=head)
                with self.subTest(success=success):
                    self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
                    if not success:
                        self.assertIn("GIT-004", result.stdout + result.stderr)


class ReleaseBuildArguments(unittest.TestCase):
    """eng/build-release.ps1 の引数の検査だけを回す（Issue #129）。

    製品は build も起動もしない（このファイルの約束・#61）。正例は「検査を通り抜けて
    eng/toolchain.ps1 に届いたこと」を sentinel で見る fixture で、cmake には一度も届かない。
    fixture は本物の git リポジトリにする（作業ツリーが clean かどうかを git に聞く検査なので）。
    """

    def prepare(self, directory):
        repository = Path(directory)
        engineering = repository / "eng"
        engineering.mkdir()
        shutil.copy2(ROOT / "eng/build-release.ps1", engineering / "build-release.ps1")
        (engineering / "toolchain.ps1").write_text("throw 'fixture-toolchain-stop'\n", encoding="utf-8")
        for command in (["init", "-b", "main"], ["add", "-A"],
                        ["-c", "user.email=fixture@example.invalid", "-c", "user.name=fixture",
                         "commit", "-m", "chore: fixture (#129)"]):
            result = run_tool(["git", "-C", str(repository), *command])
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return repository

    def script(self, repository, *arguments):
        return run_tool(["pwsh", "-NoProfile", "-File",
                         str(repository / "eng/build-release.ps1"), *arguments])

    def test_a_named_ref_on_a_clean_tree_reaches_the_toolchain(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.script(self.prepare(directory), "-Ref", "HEAD")
            self.assertIn("fixture-toolchain-stop", result.stderr)
            self.assertNotIn("QLT-013", result.stdout + result.stderr)
            self.assertNotEqual(result.returncode, 0)

    def test_missing_ref_unknown_ref_and_dirty_tree_stop_before_the_toolchain(self):
        with tempfile.TemporaryDirectory() as directory:
            repository = self.prepare(directory)
            cases = [((), "name the ref"), (("-Ref", "no-such-ref"), "unknown ref")]
            for arguments, wording in cases:
                with self.subTest(arguments=arguments):
                    result = self.script(repository, *arguments)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("QLT-013", result.stderr)
                    self.assertIn(wording, result.stderr)
                    self.assertNotIn("fixture-toolchain-stop", result.stderr)
            (repository / "untracked.txt").write_text("dirty\n", encoding="utf-8")
            result = self.script(repository, "-Ref", "HEAD")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("QLT-013", result.stderr)
            self.assertIn("not clean", result.stderr)
            self.assertNotIn("fixture-toolchain-stop", result.stderr)

    def test_the_release_script_does_not_start_the_product(self):
        """起動は設計席が行う（ADR 0038）。注釈は読まず、実行される行だけを見る。"""
        text = (ROOT / "eng/build-release.ps1").read_text(encoding="utf-8")
        code = [line for line in text.splitlines() if not line.lstrip().startswith("#")]
        for starter in ("Start-Process", "Invoke-Item", "& $executable", "&$executable"):
            self.assertFalse(any(starter in line for line in code), starter)
        # 出力先はゲートと measure-speed.py の持ち物（既定の build/ と build-release/）ではない。
        self.assertTrue(any("build/release-$short" in line for line in code))
        self.assertFalse(any("build-release/" in line for line in code))


if __name__ == "__main__":
    unittest.main()
