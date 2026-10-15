# 5-el — CIはマージ時だけ、文書のみなら不要（Issue #430・ADR 0118）

出典: docs/quality/gate-proofs.md の節 5-el（2026-10-16 に D50 で移動・本文は移動時のまま）

hideの2026-10-12指定。PRイベント起動を廃止し、`merge-pr.py --execute`から非文書変更だけ手動workflowを起動する。文書だけならCI呼び出し0。
`python -m unittest discover -s tests/conformance -p test_merge_pr.py`は16 tests成功（0.056秒）。文書免除・混在/rename・取得漏れ・CI失敗/異なるheadの拒否・plan無変更・同じrunでの再開を確認した。
`python -m unittest tests.conformance.test_verification_policy.VerificationPolicy.test_ci_and_hooks_do_not_invoke_product_checks tests.conformance.test_verification_policy.VerificationPolicy.test_merge_dispatch_uses_the_record_validator_and_expected_head`は2 tests成功（3.995秒）。手動起動配線とstub APIによる記録/期待headの正例・反例。TEMP/TMPはD側へ指定した。
文書は差分確認、`git diff --check`成功。製品テスト・ビルド・全件ゲートは実行しない。関連実装が不変ならこの結果を統合まで再利用する。
2026-10-12 12:36 JST、main ruleset `23337476`の`required_status_checks`だけを削除し、読戻し一致を確認。PR必須・squash・未解決レビュー拒否・削除/force push禁止を維持した。
設定前後は`D:/NeNeNib/evidence/merge-ci-430/ruleset-before.json`と`ruleset-after.json`。対象別CIは正典の統合道具で強制し、GitHub UI/直接CLI全体への強制とは主張しない。
本PRのマージ直前の手動CIを実接続確認として使い、実行・統合・整理の結果は同証跡先の`closeout.json`へ保存する。Waivers: none。

PR431の作成・Readyではrun 0。最初の手動run `38200540150`は成功したが、起動直後の情報照合が先走って統合道具が停止した。完了を待ってから対象情報を照合する位置へ修正し、同じ道具試験にqueued→完了の反例を追加。17 tests成功（0.043秒）。PowerShell入口の2 testsは不変なので再利用する。
