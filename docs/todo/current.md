# いまのタスク — NeNe Nib

> GitHub Issue が正。ここは要約であり、Markdown のチェックリストをタスク状態として扱わない。
> 更新は実測でだけ行う（「動くもの」は `pwsh -NoProfile -File ./eng/check.ps1` が通ったもの）。

## 現在の Issue

次の焦点は **Vim の 2 本目**（オペレータ側の回数・INSERT の Backspace の undo 単位・Home / End / PgUp / PgDn・`c y p P`・VISUAL・生成物と `fixtures.json` の一致の検査）。小さい残件は [#36](https://github.com/hideyukiMORI/nene-nib/issues/36)（`keys_trial` の例外の口）。
Issue #1 / #3 / #5 / #7 / #9 / #11 / #13 / #14 / #16 / #19 / #22 / #24 / #28 / #30 / #31 は main へ統合済み（[日報 09-17](../reports/2026-09-17.md)）。main は `b694e3b`。

## 段階

| 段階 | 状態 |
| --- | --- |
| Phase 0 言語の実測 | ✅ 2026-09-15（114 記録・`docs/quality/phase0-results.json`） |
| Phase 1 文書とゲートの足場 | ✅ 2026-09-15（Issue #1・PR #2） |
| Phase 2 negative proof | ✅ 2026-09-15（26 反例・`docs/quality/gate-proofs.md`） |
| Phase 3 縦切り | 🔲 進行中。#3 窓（ADR 0007）✅ → #5 見た目（ADR 0008）✅ → #7 編集（ADR 0009）✅ → #11 ファイル（ADR 0010）✅ → #16 速さ（ADR 0011）✅ → #19 起動の内訳 ✅ → #22 Vim の最初の縦切り（ADR 0012）✅ → #24 窓を先に見せる（ADR 0013）✅ → #28 IME（ADR 0014）✅ → #31 タブの帯（D16）✅ → #30 計測器の揺れ ✅ → **Vim の 2 本目** |
| Phase 4 公開 | 🔲 |

## 動くもの（main `b694e3b`）

枠なし窓（Snap と影は OS のまま）・Mica のタイトルバーにタブ 1 本と窓の操作・piece table の本文（複数行・スクロール・選択・Ctrl+C/X/V・Ctrl+Z/Y・クリックでキャレット）・
ステータスバーの「通常 | Vim」トグル・OS のライト／ダーク（茄子色 D11・橙 D12）・ファイルの開閉と保存（Ctrl+O / Ctrl+S / Ctrl+Shift+S・起動引数・UTF-8 / BOM / Shift_JIS・CRLF / LF・一時ファイルからの置換・未保存の印と確認）・速さのゲート（Release の exe で 4 本のベンチ・基準値の鍵 5 つ・実機の基準値・起動の内訳・窓を先に見せる起動（約 35 ms）・`WM_PAINT` で 1 フレームに 1 回の描画）・Vim の最初の範囲（NORMAL / INSERT・`h j k l 0 $ w b`・回数・`x`・`d`＋移動・`dd`・`i a I A`・Esc・`u` / Ctrl-r。本物の Vim 9.1 の oracle が生成した fixture 87 件を CTest が再生）・IME（IMM32・変換中の文字列は本文の外の `Composition` に持って renderer がキャレットの行に差し込んで描く・確定は 1 意図で通常モードは undo 1 単位・Vim INSERT は打鍵として engine へ・Vim NORMAL では IME を切り INSERT で戻す・候補窓はキャレットの直下）・タブの帯は不透明の `title_bar`（D16・アクティブなタブは本文色）・速さのゲートは刺激が届かなかった試行を欠測にし、計測不能を退行と別の終了コード 2 で言う。

## 動かないもの

64 MiB 超のファイル・文字コードと改行の手動切り替え・IME の再変換と TSF 固有の機能・Vim の VISUAL / `c y p` / テキストオブジェクト / `.` / 検索 / `:`・複数タブ・Ctrl+P・設定の保存・カラーテーマ（C1〜C4）・フォントサイズ・折り返し・横スクロール・ドラッグ選択。

## 次の 1 手

hide が実機で #31 の帯を見て C の絵と同じか確かめる（結果を gate-proofs 5-i に 1 行）。Vim の 2 本目は fixture を足して `python eng/vim-oracle.py --regenerate`。#36 は eng だけで小さい。Vim の 2 本目（`c y p` / VISUAL / オペレータ側の回数 / Home・End）は fixture を足す形で。CI のベンチの値が数回たまったら CI の基準値の Issue。
カラーテーマ（D13）は `docs/plans/2026-09-15-colorschemes.md` の C1（core だけ）がいつでも着手できる。
