# いまのタスク — NeNe Nib

> GitHub Issue が正。ここは要約であり、Markdown のチェックリストをタスク状態として扱わない。
> 更新は実測でだけ行う（「動くもの」は `pwsh -NoProfile -File ./eng/check.ps1` が通ったもの）。

## 現在の Issue

次の焦点 Issue は [#19](https://github.com/hideyukiMORI/nene-nib/issues/19)（起動 190 ms の内訳を節目で測る。小さい）。その次は IME（FR-012）か Vim エンジン（ADR 0005）。
Issue #1 / #3 / #5 / #7 / #9 / #11 / #13 / #14 / #16 は main へ統合済み（[日報 09-16](../reports/2026-09-16.md)）。main は `e592adc`。

## 段階

| 段階 | 状態 |
| --- | --- |
| Phase 0 言語の実測 | ✅ 2026-09-15（114 記録・`docs/quality/phase0-results.json`） |
| Phase 1 文書とゲートの足場 | ✅ 2026-09-15（Issue #1・PR #2） |
| Phase 2 negative proof | ✅ 2026-09-15（26 反例・`docs/quality/gate-proofs.md`） |
| Phase 3 縦切り | 🔲 進行中。#3 窓（ADR 0007）✅ → #5 見た目（ADR 0008）✅ → #7 編集（ADR 0009）✅ → #11 ファイル（ADR 0010）✅ → #16 速さ（ADR 0011）✅ → **#19 起動の内訳** → IME か Vim |
| Phase 4 公開 | 🔲 |

## 動くもの（main `e592adc`）

枠なし窓（Snap と影は OS のまま）・Mica のタイトルバーにタブ 1 本と窓の操作・piece table の本文（複数行・スクロール・選択・Ctrl+C/X/V・Ctrl+Z/Y・クリックでキャレット）・
ステータスバーの「通常 | Vim」トグル・OS のライト／ダーク（茄子色 D11・橙 D12）・ファイルの開閉と保存（Ctrl+O / Ctrl+S / Ctrl+Shift+S・起動引数・UTF-8 / BOM / Shift_JIS・CRLF / LF・一時ファイルからの置換・未保存の印と確認）・速さのゲート（Release の exe で 3 本のベンチ・実機の基準値・`WM_PAINT` で 1 フレームに 1 回の描画）。

## 動かないもの

64 MiB 超のファイル・文字コードと改行の手動切り替え・IME・Vim エンジン・複数タブ・Ctrl+P・設定の保存・カラーテーマ（C1〜C4）・フォントサイズ・折り返し・横スクロール・ドラッグ選択。

## 次の 1 手

Issue #19 の依頼書を実装リナに渡し、起動の内訳を測って記録する。続いて IME（FR-012）か Vim エンジンの最初の縦切り（ADR 0005）。CI のベンチの値が数回たまったら CI の基準値の Issue。
カラーテーマ（D13）は `docs/plans/2026-09-15-colorschemes.md` の C1（core だけ）がいつでも着手できる。
