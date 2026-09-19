# いまのタスク — NeNe Nib

> GitHub Issue が正。ここは要約であり、Markdown のチェックリストをタスク状態として扱わない。
> 更新は実測でだけ行う。検証は差分から選び、関連入力が不変の成功結果を再利用する（QLT-001 / QLT-012・[ADR 0021](../adr/0021-diff-scoped-verification-and-result-reuse.md)）。

2026-09-20: hide 指示の Issue #61 で全件・最終 HEAD ごとの再実行規定を置換した。過去の引き継ぎにある全件指示より新方針を優先する。設定機能 #60 も新しい方針で必要な検証だけを選ぶ。

## 現在の Issue

**最新の変更は Issue #60（C2・設定の保存と本文フォント）**。設定の所有・保存形式・失敗と競合・ptからの描画を [ADR 0020](../adr/0020-versioned-editor-settings-and-point-font-size.md) に定めた。Ctrl+拡大/縮小/既定復帰とCtrl+wheel、テーマ/フォントの復元、実機のクリック/IMEまで検証した。統合状態はGitHubを正とする。
Issue #58 / PR #59（Vim画面移動）と #61 / PR #62（差分検証・成功再利用）はmainへ統合済み。次は **C3**（Ex・テーマ選択・フォント設定コマンド）。

## 段階

| 段階 | 状態 |
| --- | --- |
| Phase 0 言語の実測 | ✅ 2026-09-15（114 記録・`docs/quality/phase0-results.json`） |
| Phase 1 文書とゲートの足場 | ✅ 2026-09-15（Issue #1・PR #2） |
| Phase 2 negative proof | ✅ 2026-09-15（26 反例・`docs/quality/gate-proofs.md`） |
| Phase 3 縦切り | 🔲 進行中。#3 窓（ADR 0007）✅ → #5 見た目（ADR 0008）✅ → #7 編集（ADR 0009）✅ → #11 ファイル（ADR 0010）✅ → #16 速さ（ADR 0011）✅ → #19 起動の内訳 ✅ → #22 Vim の最初の縦切り（ADR 0012）✅ → #24 窓を先に見せる（ADR 0013）✅ → #28 IME（ADR 0014）✅ → #31 タブの帯（D16）✅ → #30 計測器の揺れ ✅ → #36 欠測の言い方 ✅ → #44 生成物の SHA（CNF-010）✅ → #43 Vim の 2 本目（ADR 0015）✅ → #47 CI の速さの基準値（ADR 0016）✅ → #52 カラーテーマ C1（ADR 0017）✅ → #53 VISUAL（ADR 0018）✅ → **#58 画面移動（ADR 0019）✅ → #60 C2（ADR 0020） → C3** |
| Phase 4 公開 | 🔲 |

## 実装したもの（Issue #60）

C2: 設定の保存・復元、8〜40 ptの本文拡縮（Ctrl+`+` / `-` / `0` とCtrl+wheel）。本文・行番号だけを拡縮し、テーマ・フォント名・サイズを版付きで保存する。

枠なし窓（Snap と影は OS のまま）・Mica のタイトルバーにタブ 1 本と窓の操作・piece table の本文（複数行・スクロール・選択・Ctrl+C/X/V・Ctrl+Z/Y・クリックでキャレット）・
ステータスバーの「通常 | Vim」トグル・OS のライト／ダーク（茄子色 D11・橙 D12）・ファイルの開閉と保存（Ctrl+O / Ctrl+S / Ctrl+Shift+S・起動引数・UTF-8 / BOM / Shift_JIS・CRLF / LF・一時ファイルからの置換・未保存の印と確認）・速さのゲート（Release の exe で 4 本のベンチ・基準値の鍵 5 つ・実機の基準値・起動の内訳・窓を先に見せる起動（約 35 ms）・`WM_PAINT` で 1 フレームに 1 回の描画）・Vim（NORMAL / INSERT・`h j k l 0 $ w b e ^` と Home / End・回数（オペレータ側と移動側の掛け算）・`x`・`d c y` ＋移動・`dd cc yy`・`D C Y`・`p P`（文字単位と行単位・回数）・無名レジスタは種類つきで本文は LF・`i a I A`・Esc・`u` / Ctrl-r・INSERT 1 回が undo 1 単位・VISUAL `v V`（回数・`o`・`d x y c`・表示と Ctrl+C と操作が同じ範囲）。画面移動 `H M L`・Ctrl-d/u/f/b・PgUp/PgDn、window-local な半画面量、Vim の自動追従。本物の Vim 9.1 の oracle が生成した fixture 329 件を CTest が再生）・IME（IMM32・変換中の文字列は本文の外の `Composition` に持って renderer がキャレットの行に差し込んで描く・確定は 1 意図で通常モードは undo 1 単位・Vim INSERT は打鍵として engine へ・Vim NORMAL では IME を切り INSERT で戻す・候補窓はキャレットの直下）・タブの帯は不透明の `title_bar`（D16・アクティブなタブは本文色）・速さのゲートは刺激が届かなかった試行を欠測にし、計測不能を退行と別の終了コード 2 で言う。性能検証は必要な変更で選んで実行する。一致する指紋の基準値で判定し、通常のCIでは測り直さない（ADR 0016 / 0021）。生成物 `VimFixtures.hpp` と `fixtures.json` の一致は CNF-010 が守る。core にテーマの模型（`Theme` / `SyntaxPalette` / `derive_ui`・組み込み 9 テーマ・コントラスト比 4.5 以上を tests が要求。ADR 0017）があり、設定ファイルから指定テーマまたはOS追従を選べる。対話的な選択はC3。

## 動かないもの

64 MiB 超のファイル・文字コードと改行の手動切り替え・IME の再変換と TSF 固有の機能・
Vim の `Ctrl-v`（矩形）/ VISUAL の `p u ~ > < J r I A gv` と `X D C Y`（この縦切りでは何もしない）/ ドラッグで VISUAL /
テキストオブジェクト / `.` / 名前つきレジスタ / `J r s S` / 検索 / `:`・
複数タブ・Ctrl+P・テーマ/フォントの対話的コマンド（C3）・利用者テーマ（C4）・折り返し・横スクロール・ドラッグ選択。

## 次の 1 手

**C3** は Ex の `:` を先に接続し、`:colorscheme` / `:set fontsize` / `guifont` を C2 の同じ設定保存へ流す。その後 Ctrl+P の共通一覧へ進む。矩形選択とドラッグは別 Issue。
今回の判断・結果・未確認は [日報](../reports/2026-09-20.md) と [引き継ぎ](../handoffs/2026-09-20.md)。検証は差分に必要な範囲だけ選び、成功結果を再利用する。
