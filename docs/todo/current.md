# いまのタスク — NeNe Nib

> GitHub Issue が正。ここは要約であり、Markdown のチェックリストをタスク状態として扱わない。
> 更新は実測でだけ行う（「動くもの」は `pwsh -NoProfile -File ./eng/check.ps1` が通ったもの）。

## 現在の Issue

**最新の変更は Issue #58（Vim の 4 本目・画面移動）**。統合・最終チェックの状態は GitHub の Issue / PR を正とする。hide の 2026-09-19 の続行指示に従い、`H M L`・`Ctrl-d/u/f/b`・PgUp / PgDn を先に進める。表示領域は借用する入力、移動は選択と先頭行を一緒に返す効果とした（[ADR 0019](../adr/0019-vim-viewport-input-and-navigation-effect.md)）。Vim 9.1 の `-es` では画面位置を測れず、表示領域を指定する fixture だけ通常端末モードで測る。矩形選択（`Ctrl-v`・`virtcol`）とドラッグで VISUAL は別 Issue とし、その後の候補は **C2**（設定の保存・D14 フォントサイズ）と **C3**（`:colorscheme`・Ex の `:`）。
Issue #1 / #3 / #5 / #7 / #9 / #11 / #13 / #14 / #16 / #19 / #22 / #24 / #28 / #30 / #31 / #36 / #43 / #44 / #47 / #52 / #53 / #56 は main へ統合済み（[日報 09-19](../reports/2026-09-19.md)）。作業開始時の main は `8326aa6`。

## 段階

| 段階 | 状態 |
| --- | --- |
| Phase 0 言語の実測 | ✅ 2026-09-15（114 記録・`docs/quality/phase0-results.json`） |
| Phase 1 文書とゲートの足場 | ✅ 2026-09-15（Issue #1・PR #2） |
| Phase 2 negative proof | ✅ 2026-09-15（26 反例・`docs/quality/gate-proofs.md`） |
| Phase 3 縦切り | 🔲 進行中。#3 窓（ADR 0007）✅ → #5 見た目（ADR 0008）✅ → #7 編集（ADR 0009）✅ → #11 ファイル（ADR 0010）✅ → #16 速さ（ADR 0011）✅ → #19 起動の内訳 ✅ → #22 Vim の最初の縦切り（ADR 0012）✅ → #24 窓を先に見せる（ADR 0013）✅ → #28 IME（ADR 0014）✅ → #31 タブの帯（D16）✅ → #30 計測器の揺れ ✅ → #36 欠測の言い方 ✅ → #44 生成物の SHA（CNF-010）✅ → #43 Vim の 2 本目（ADR 0015）✅ → #47 CI の速さの基準値（ADR 0016）✅ → #52 カラーテーマ C1（ADR 0017）✅ → #53 VISUAL（ADR 0018）✅ → **#58 画面移動（ADR 0019） → C2 / C3** |
| Phase 4 公開 | 🔲 |

## 実装したもの（Issue #58）

枠なし窓（Snap と影は OS のまま）・Mica のタイトルバーにタブ 1 本と窓の操作・piece table の本文（複数行・スクロール・選択・Ctrl+C/X/V・Ctrl+Z/Y・クリックでキャレット）・
ステータスバーの「通常 | Vim」トグル・OS のライト／ダーク（茄子色 D11・橙 D12）・ファイルの開閉と保存（Ctrl+O / Ctrl+S / Ctrl+Shift+S・起動引数・UTF-8 / BOM / Shift_JIS・CRLF / LF・一時ファイルからの置換・未保存の印と確認）・速さのゲート（Release の exe で 4 本のベンチ・基準値の鍵 5 つ・実機の基準値・起動の内訳・窓を先に見せる起動（約 35 ms）・`WM_PAINT` で 1 フレームに 1 回の描画）・Vim（NORMAL / INSERT・`h j k l 0 $ w b e ^` と Home / End・回数（オペレータ側と移動側の掛け算）・`x`・`d c y` ＋移動・`dd cc yy`・`D C Y`・`p P`（文字単位と行単位・回数）・無名レジスタは種類つきで本文は LF・`i a I A`・Esc・`u` / Ctrl-r・INSERT 1 回が undo 1 単位・VISUAL `v V`（回数・`o`・`d x y c`・表示と Ctrl+C と操作が同じ範囲）。画面移動 `H M L`・Ctrl-d/u/f/b・PgUp/PgDn、window-local な半画面量、Vim の自動追従。本物の Vim 9.1 の oracle が生成した fixture 329 件を CTest が再生）・IME（IMM32・変換中の文字列は本文の外の `Composition` に持って renderer がキャレットの行に差し込んで描く・確定は 1 意図で通常モードは undo 1 単位・Vim INSERT は打鍵として engine へ・Vim NORMAL では IME を切り INSERT で戻す・候補窓はキャレットの直下）・タブの帯は不透明の `title_bar`（D16・アクティブなタブは本文色）・速さのゲートは刺激が届かなかった試行を欠測にし、計測不能を退行と別の終了コード 2 で言う。CI の速さゲートは EPYC 7763 の指紋の run で基準値と比べて落ち、他の host は記録だけと 1 行言って通り、記録は artifact に 90 日残る（ADR 0016）。生成物 `VimFixtures.hpp` と `fixtures.json` の一致は CNF-010 が守る。core にテーマの模型（`Theme` / `SyntaxPalette` / `derive_ui`・組み込み 9 テーマ・コントラスト比 4.5 以上を tests が要求。ADR 0017）があるが、UI はまだ OS の明暗で 2 テーマを選ぶだけ。

## 動かないもの

64 MiB 超のファイル・文字コードと改行の手動切り替え・IME の再変換と TSF 固有の機能・
Vim の `Ctrl-v`（矩形）/ VISUAL の `p u ~ > < J r I A gv` と `X D C Y`（この縦切りでは何もしない）/ ドラッグで VISUAL /
テキストオブジェクト / `.` / 名前つきレジスタ / `J r s S` / 検索 / `:`・
複数タブ・Ctrl+P・設定の保存・カラーテーマ（C1〜C4）・フォントサイズ・折り返し・横スクロール・ドラッグ選択。

## 次の 1 手

**C2**（設定の保存・D14 フォントサイズ）は設定全体の形を ADR にする。**C3** は Ex の `:` が先で、導出した 7 テーマの見た目を比較できる画面案にして hide が見る。矩形選択とドラッグは画面移動とは別の Issue にする。
今回の判断・検証の注意点は [夜の日報](../reports/2026-09-19-night.md) と [引き継ぎ](../handoffs/2026-09-19-night.md)。フルゲートは他のビルド・測定を止めて 1 本だけ回す。
CI の他の指紋の基準値は artifact `speed-records` がたまってから項を足す。
