# いまのタスク — NeNe Nib

> GitHub Issue が正。ここは要約であり、Markdown のチェックリストをタスク状態として扱わない。
> 更新は実測でだけ行う（「動くもの」は `pwsh -NoProfile -File ./eng/check.ps1` が通ったもの）。

## 現在の Issue

[Issue #11](https://github.com/hideyukiMORI/nene-nib/issues/11) — ファイルを開いて保存し、UTF-8 / Shift_JIS と改行の形を保ち、未保存の印を出す（ファイルの縦切り）。ADR 0010。ブランチ `feat/11-file-open-save`。
Issue #1 / #3 / #5 / #7 / #9 は main へ統合済み（[日報](../reports/2026-09-15.md)）。

## 段階

| 段階 | 状態 |
| --- | --- |
| Phase 0 言語の実測 | ✅ 2026-09-15（114 記録・`docs/quality/phase0-results.json`） |
| Phase 1 文書とゲートの足場 | ✅ 2026-09-15（Issue #1・PR #2） |
| Phase 2 negative proof | ✅ 2026-09-15（26 反例・`docs/quality/gate-proofs.md`） |
| Phase 3 縦切り | 🔲 進行中。#3 窓（ADR 0007）✅ → #5 見た目（ADR 0008）✅ → #7 編集（ADR 0009）✅ → **#11 ファイル（ADR 0010）** → 速さの基準値（QLT-014）→ IME か Vim |
| Phase 4 公開 | 🔲 |

## 動くもの（main `82aa30e`）

枠なし窓（Snap と影は OS のまま）・Mica のタイトルバーにタブ 1 本と窓の操作・piece table の本文（複数行・スクロール・選択・Ctrl+C/X/V・Ctrl+Z/Y・クリックでキャレット）・
ステータスバーの「通常 | Vim」トグル・OS のライト／ダーク（茄子色 D11・橙 D12）。

## 動かないもの

ファイルの開閉と保存（#11 で実装中。**未保存の本文は終了で消える**）・IME・Vim エンジン・複数タブ・Ctrl+P・設定の保存・カラーテーマ（C1〜C4）・フォントサイズ・折り返し・横スクロール・ドラッグ選択。

## 次の 1 手

Issue #11 をフルゲート・adapter テスト・実機（起動引数で開く）・CI で通して squash merge する。
その次は速さの基準値（QLT-014）を最初に測る縦切り、続いて IME（FR-012）か Vim エンジンの最初の縦切り（ADR 0005）。
カラーテーマ（D13）は `docs/plans/2026-09-15-colorschemes.md` の C1（core だけ）がいつでも着手できる。
