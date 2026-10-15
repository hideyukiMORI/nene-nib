# いまのタスク — NeNe Nib

2026-10-15 の区切り。Issue が正本。経緯は[日報](../reports/2026-10-15.md)、次回の入口は[引き継ぎ](../handoffs/2026-10-15.md)、検証は [gate-proofs](../quality/gate-proofs.md) と各品質記録を参照する。

## 運用

- 2026-10-15 は hide の指示で一日中、速度と安定性の向上を試した（設計席 Fable・実装と調査は Opus の席）。成果はすべて PR で、main は変えていない。性能改善の main 統合は施主決定 D41（独立レビュー・正式ゲート・CI・hide の試用）で、merge は hide の試用の後。
- 対象と直接の境界だけを検証し、関連入力不変の成功結果は再利用する（QLT-001/012）。基準値・許容・ゲートは一切変えない（縮んだ分は余裕のまま）。文書のみなら CI/製品試験不要。
- 追加 worktree/出力は D:/NeNeNib 配下。merge 後に恒久収載・監査整理し、branch/commit を保持する。席の依頼書には「作業を捨てる git の命令は使わず Edit で直して報告」を書く（権限の確認で約 11 時間止まった）。

## いまの Issue と次の順

| 対象 | 状態・再開点 |
| --- | --- |
| #474 / PR477 追従の遅延解決（ADR 0128） | 連打 11.3→2.1 ms・16 MiB 14.7→2.2 ms。レビュー・ゲート 8 本・CI 緑。Ready。hide の試用の後に merge。#460 は同じ原因で解消 |
| #476 / PR478 Vim の viewport は要求で（ADR 0129） | Vim INSERT 連打 12.8→4.0 ms。レビュー・契約追加・ゲート 8 本退行 0。base は #477。merge 後に base を main へ変えて rebase・CI |
| #485 / PR487 最初の frame は WARP で子窓に（ADR 0132） | 起動 178→40.5 ms・16 MiB open 174→52 ms・単打同等。レビュー・ゲート・差し替えの標本 3 回。draft。base は #482。merge 後に base を main へ |
| #481 / PR482 device の先行作成（ADR 0131） | 起動 192→178 ms・16 MiB 202→174 ms。レビュー反映。draft。#477 の merge 後に rebase して CI |
| #479 / PR480 DLL の遅延読み込み（ADR 0130） | origin −2 ms。draft。#477 の merge 後に rebase して 8 本を測り直す |
| #452 / PR453 分割UTF-8 4案目 | 局所比較 ASCII 1.003 / 日本語 1.034・正式 open 200.8 ms 退行 0。契約・レビュー・CI 緑。Ready。速さの改善を主張しない refactor。統合は hide の判断 |
| #450 / #468 本文の接続 | [ADR0127](../adr/0127-large-file-integration-reserves-prepares-and-commits.md)と[全caller移行表](../design/2026-10-13-large-file-integration-map.md)を設計。製品移行/2GiB未受理 |
| #469 ASan診断 | #467で非致命的interception診断。既存EXE/PDB/ログから切り分ける。無関係な既存不具合と断定しない |
| #454 / #456 設定と日英 | [画面案](../design/2026-10-12-settings-dialog-boards.md)とADR0123は提案。未採用 |

merge の順: #477 → #478 → #482 → #487 → #480 → #453（どれも独立に戻せる）。merge 後に worktree と outputs を収載・監査整理する。

## いまの数字・動かないもの・段階

- 公開版は[v0.1.0-beta.3](https://github.com/hideyukiMORI/nene-nib/releases/tag/v0.1.0-beta.3)。本文余白/断片保存/改行索引/読込完了は main のみで公開版へ未反映。
- main 89c582d の実機の速さ: 起動 192 ms・窓 31 ms・単打 0.64 ms・16 MiB open 202 ms・連打 11.3 ms（基準超過・#474 で 2.1）・長行単打 6.25 ms（6 KB 行の layout 4.6 ms が床・ADR 0114 が分割再整形を却下）。
- 組み込み125テーマ、Vim oracle 1853件。settings.v3/session.v2、64MiB上限、同期FilePortを維持。保存schema変更なし。
- 2GiB/512MiB/100ms、split、Markdown、構文解析、設定モーダル/日英切替、手動横スクロール/スクロールバー、Vim gj/gk/g0/g$・ドラッグは未対応/未受理。
- 残る速さの候補: 16 MiB open の document_opened 12 ms（UTF-8 検証と索引）、frame の描画 0.4〜0.5 ms の内訳、ShowWindow の 9 ms は OS 側（WM_ACTIVATE の IME 文脈）で製品では縮まない。
- #486: `eng/verify-window.py` の look の節が #304 の案内と合わず落ちる（既存失敗・未修正）。
- 既存#411/#365/#373/#424は今回修正していない。Waivers none。
