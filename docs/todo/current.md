# いまのタスク — NeNe Nib

2026-10-14の継続。Issueが正本。前回の区切りは[日報](../reports/2026-10-13.md)と[引き継ぎ](../handoffs/2026-10-13.md)、検証は[gate-proofs](../quality/gate-proofs.md)と各品質記録を参照する。

## 運用

- hideの本セッション指定によりAstraが設計・採否、SOLが調査・実装・検証を担当して#452を継続する。既定はサナ単体で、この分担を他案件や次回へ無条件に継承しない。
- 対象と直接の境界だけを検証し、関連入力不変の成功結果は再利用する（QLT-001/012）。文書のみならCI/製品試験不要。#467のログ取得だけの追加CTestは手順違反として記録済み。
- 追加worktree/出力はD:/NeNeNib配下。完了後に恒久収載・監査整理し、branch/commitを保持する。hideが実機は空いていると伝えた本日、#452の3案目を固定条件で測定。4案目の性能/製品openは未実施。

## いまのIssueと次の順

| 対象 | 状態・再開点 |
| --- | --- |
| #452 / PR453 分割UTF-8 | draft、性能未受理。3案目の対応比はASCII1.105407/日本語1.033577で全値を保持。4案目は検査と復号を一回にし対象14,246 checks/静的検査が成功、性能未測定。固定比較2条件→採否判断後に製品open。2作業木/原ログは残す |
| #450 / #468 本文の接続 | [ADR0127](../adr/0127-large-file-integration-reserves-prepares-and-commits.md)と[全caller移行表](../design/2026-10-13-large-file-integration-map.md)を設計。次は資源台帳/退避回収の小実験と正典UTF-8/索引の結合。本文取得と保存版は同じ製品候補で統合。製品移行/2GiB未受理 |
| #467 / PR470 取消実験 | ADR0126と[138契約の結果](../quality/overlapped-cancel-2026-10-13.md)をmain統合。実験コードは専用branch。実disk取消性・100msは未受理。恒久先はD:/NeNeNib/evidence/overlapped-cancel-467 |
| #469 ASan診断 | #467で非致命的interception診断。静的にはUCRT frexp+4と強く整合するが最終hook/全検査範囲未確認。既存EXE/PDB/ログから切り分ける。無関係な既存不具合と断定しない |
| #463 / PR464、#465 / PR466 | 宣言長までの製品readは139契約で統合。同期退避の寿命は228契約の隔離実験で受理。恒久収載/整理済み。製品portへ試作をそのまま取り込まない |
| #446 / PR447、#460 | 改行索引は統合済み。入力/長行/openは基準内、連打は変更前も基準超過で#460へ分離。2GiB索引payloadの計算値288MiBは実容量受理ではない |
| #454 / #456 設定と日英 | [画面案](../design/2026-10-12-settings-dialog-boards.md)とADR0123は提案。見た目の採用・settings.v4・全対訳・状態/IMEの実装は未了 |

## いまの数字・動かないもの・段階

- 公開版は[v0.1.0-beta.3](https://github.com/hideyukiMORI/nene-nib/releases/tag/v0.1.0-beta.3)。配布はD:/NeNeNib/releases/v0.1.0-beta.3/。本文余白/断片保存/改行索引/読込完了はmainのみで公開版へ未反映。
- 組み込み125テーマ、Vim oracle 1853件。settings.v3/session.v2、64MiB上限、同期FilePortを維持。wrapは既定オン、set wrap/nowrapで切替。保存schema変更なし。
- 2GiB/512MiB/100ms、split、Markdown、構文解析、設定モーダル/日英切替、手動横スクロール/スクロールバー、Vim gj/gk/g0/g$・ドラッグは未対応/未受理。
- 段階1Aの保存/索引/読込の土台から、本文の所有と失敗を扱う一括移行へ進む。大容量→日常編集→split→Markdown→公開の順。splitのD45/ADR0120新条件は未測定で旧holdを維持。
- 既存#411/#365/#373/#424/#460は今回修正していない。#419の古いjunction出力は承認拒否の記録を継承して保持。Waivers none。
