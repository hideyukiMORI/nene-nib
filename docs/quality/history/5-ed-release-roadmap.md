# 5-ed — 正式リリースまでのロードマップ（Issue #408）

出典: docs/quality/gate-proofs.md の節 5-ed（2026-10-16 に D50 で移動・本文は移動時のまま）

hideの保存依頼に従い、[ロードマップ](../../plans/2026-10-11-release-roadmap.md)とtodo/current・日報・引き継ぎ、本節の5文書を更新する。範囲整理→大容量編集の成立性確認→大容量対応→日常編集→split→Markdown→併用確認と公開の順と理由、各段階の成果と条件、性能評価、縮小案、再開地点を記録する。

D7/D40・FR-007/FR-013・ADR0004/0010/0068/0075との境界を自己照合する。既存仕様・旧split判定hold・性能閾値を変更せず、実装や測定を開始しない。関連規則はQLT-001/QLT-010/QLT-012、GIT-001〜004。Waivers: none。製品挙動・API・schemaの変更なし。

変更文書の規則ID・相対リンク・記録先の欠落と空白を確認するため、既存conformance.document_checksへ変更5ファイルを渡す `python -B -X utf8 D:/NeNeNib/outputs/20261011-release-roadmap/verify-docs.py` と `git diff --cached --check` を選ぶ。依頼との自己照合では段階0〜6、2枠、ソース編集へのプレビュー追従、初版閲覧先行の縮小判断、数値条件未確定、実装未着手を確認する。文書のみなのでアプリのテスト・build・実機起動・性能測定は実行しない。

結果と5ファイルのSHAは同OUTのdocument-checks.json、実command/cwd/native exit/stdout/stderrはcommands/に保存する。自己レビューはself-review.md。初回の対象検証後、関連文書・検査器・規則が不変ならpush/レビュー/mergeで結果を再利用する。必須CIはGit規約・検証記録・空白の確認で、製品テストの証明にはしない。

恒久先はD:/NeNeNib/evidence/release-roadmap-20261011。main統合後に変更5文書・command原記録・Issue/PR内容・branchのGit bundleを収載してSHAを照合し、追加D:/NeNeNib/worktrees/408-release-roadmapとOUTだけを絶対path・未保存/未追跡/無視ファイル・稼働参照・リンクを確認して整理する。branch/commit保持。実完了は恒久先closeout.jsonに記録し、次の機能作業へ自動で進まない。
