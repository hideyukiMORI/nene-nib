# 5-ci — 名前付きEx仕様の追補とsplit前の区切り（Issue #288）

出典: docs/quality/gate-proofs.md の節 5-ci（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-03。FR-003 / FR-008 / D39 / QLT-001 / QLT-012 / GIT-001〜004。SPECIFICATION.md第5節に残っていた#284時点の説明を#286 / ADR0067の実装へ揃えた。文書だけの変更で、splitの製品挙動は加えない。main統合後、注釈付き`checkpoint/pre-split-20261003`を置き、別Issueで分割表示の範囲・速度の採用条件・限定試作を扱う。

確認する退行は仕様と実装の矛盾、文書の規則ID/リンク/強制状態と空白だけ。`conformance.document_checks(root, inventory(root), rules)`は違反0（`out/288-doc-conformance.log`）。`git diff --check`は終了0。`git diff --exit-code 591b99f -- src tests eng CMakeLists.txt`は差分0で、#286の製品・試験・関連依存を変更していない。

再利用: [5-ch](5-ch-ex-named-save.md)の991 checks、Release生成、実機8場面と正常終了3回を再利用する。Releaseは`build/release-591b99f/NeNeNib.exe`、SHA-256 `312B55BE0A62F8593CEB95AF1EDC87EB13C3BB7C7E39A739948A43DF939C66E6`。実行元コミットは591b99fa7bbd2b6a4149fe45b81c81cafeb3e03a、製品の統合コミットは3d055dad43d3b87a38df9d8b3fa566298c29344a。性能の既存記録は5-cc / 5-cdで、タグだけを理由に測り直さない。splitの採用前後の比較条件は次Issueで定める。

自己レビュー: D39・ADR0067と保存系の解析/共通保存を読み合わせ、コピーと命名、失敗、E32/E37、残る未対応範囲を明記した。ゲート・schema・製品挙動の変更なし。Waivers: none。タグは開発の区切りで、配布Releaseではない。ローカルとoriginのタグ照合は統合後に行い、タグの注釈と後続の準備記録へ実際の参照先を残す。
