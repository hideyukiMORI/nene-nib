# いまのタスク — NeNe Nib

> GitHub Issue が正。ここは要約であり、Markdown のチェックリストをタスク状態として扱わない。
> 更新は実測でだけ行う。検証は差分から選び、関連入力が不変の成功結果を再利用する（QLT-001 / QLT-012・[ADR 0021](../adr/0021-diff-scoped-verification-and-result-reuse.md)）。

2026-09-20: hide 指示の Issue #61 で全件・最終 HEAD ごとの再実行規定を置換した。過去の引き継ぎにある全件指示より新方針を優先する。設定機能 #60 も新しい方針で必要な検証だけを選ぶ。

## 現在の Issue

**直近の修正は [Issue #77](https://github.com/hideyukiMORI/nene-nib/issues/77)（VISUAL移行時の希望列）**。開始/種類切替/回数の希望列を補正し、対象テストとnativeを確認。採用18fixture・計489件。行単位VISUAL yankの既存問題はIssue #81へ分離。検証と再利用はgate-proofs 5-s、統合状態はGitHubが正。

**前回の実装・限定検証完了は [Issue #79](https://github.com/hideyukiMORI/nene-nib/issues/79)（Vimのo/O）**。hideの#76実機確認と続行指示により、main `8f48f49`から `feat/79-vim-open-lines` で実装した。ADR 0028、追加oracle40件・計471件、初回unit551中549成功と修正/追加対象430・46成功、native3件、build/tidy/symbols/conformance成功。統合状態はGitHubを正とする。

成果の統合単位は [PR #80](https://github.com/hideyukiMORI/nene-nib/pull/80)。独立レビューは未解決指摘なし。関連実装が不変のため、push/review/mergeではgate-proofs 5-rの成功結果を再利用する。

直前の完了はIssue #76 / PR #78（gg/Gと指定行移動）。main `8f48f49`へ統合し、hideが実機確認済み。既存VISUAL移行時の希望列解除はIssue #77へ分離している。

直前の完了はIssue #72（Vimの行内文字検索と反復）。2026-09-20夜のhideの続行指示で再開し、`feat/72-vim-character-search`で既存移動・範囲へ接続した。[ADR 0026](../adr/0026-vim-character-search-and-scoped-oracle.md)、追加oracle60件（旧329件は維持）、単体621 checks、native7シナリオ、build/tidy/symbols/conformanceが成功。PR #75でmainへ統合済み。

直前の完了はIssue #70（C4b・利用者テーマの選択/保存/復元）。[ADR 0025](../adr/0025-user-theme-catalog-and-selection.md)、対象unit88、Ex153、palette130、設定codec23、catalog301 checksとnative120 DPIが成功し、[PR #71](https://github.com/hideyukiMORI/nene-nib/pull/71)で製品commit `9a493fa`へ統合。停止記録はPR #74、再開時のmainは `b6d19ff`。
Issue #68 / PR #69（C4a）、#66 / PR #67（C3b）、#64 / PR #65（C3a）、#60 / PR #63（C2）、#58 / PR #59（Vim画面移動）、#61 / PR #62（差分検証・成功再利用）はmainへ統合済み。

## 段階

| 段階 | 状態 |
| --- | --- |
| Phase 0 言語の実測 | ✅ 2026-09-15（114 記録・`docs/quality/phase0-results.json`） |
| Phase 1 文書とゲートの足場 | ✅ 2026-09-15（Issue #1・PR #2） |
| Phase 2 negative proof | ✅ 2026-09-15（26 反例・`docs/quality/gate-proofs.md`） |
| Phase 3 縦切り | 🔲 進行中。#3 窓（ADR 0007）✅ → #5 見た目（ADR 0008）✅ → #7 編集（ADR 0009）✅ → #11 ファイル（ADR 0010）✅ → #16 速さ（ADR 0011）✅ → #19 起動の内訳 ✅ → #22 Vim の最初の縦切り（ADR 0012）✅ → #24 窓を先に見せる（ADR 0013）✅ → #28 IME（ADR 0014）✅ → #31 タブの帯（D16）✅ → #30 計測器の揺れ ✅ → #36 欠測の言い方 ✅ → #44 生成物の SHA（CNF-010）✅ → #43 Vim の 2 本目（ADR 0015）✅ → #47 CI の速さの基準値（ADR 0016）✅ → #52 カラーテーマ C1（ADR 0017）✅ → #53 VISUAL（ADR 0018）✅ → **#58 画面移動 ✅ → #60 C2 ✅ → #64 C3a ✅ → #66 C3b ✅ → #68 C4a ✅ → #70 C4b ✅ → #72 行内文字検索 ✅（ADR 0026）→ #76 指定行移動 ✅（ADR 0027）→ #79 開行と反復 ✅（ADR 0028）** |
| Phase 4 公開 | 🔲 |

## 実装したもの（Issue #77の補正まで）

VISUALの希望列: v/Vの開始・種類切替・同じキーでの終了は保持し、回数付きの横選択は実際に移動した到達列へ更新する。

Vimの開行: NORMALの `o/O`、回数付き入力のEsc時反復、BS/Enter/UTF-8、CRLF/LF、既存INSERT/IME/undo/redo。移動・元本文への削除・外部編集で古い反復を解除する。VISUALではo/Oとも端点交換。

Vimの指定行移動: `gg` / `G`、絶対行番号、明示回数と未指定の区別、d/c/yの両端を含む行単位範囲、VISUAL端点。g待ちと文字検索待ちは排他的。取消と無効後続はキーを消費し、モード・選択・欲しい列・検索記憶を維持する。

Vimの行内文字検索: `f/F/t/T`と`;`/`,`、回数、d/c/y、VISUAL端点、UTF-8、行境界、未発見・取消・検索記憶、undo。未設定registerと空yank済みを区別する。oracleは対象prefixの指定と既存期待行の検証付き再利用に対応した。NORMAL/VISUALでIMEを閉じる仕様は継続する。

C4b: `%LOCALAPPDATA%/NeNeNib/themes`の起動時カタログ（最大128件）。Ex/Tab/Ctrl+Pから選択、保存と復元、名前付き診断、壊れた保存設定の保護。ファイル変更は再起動で反映。

C4a: ThemeDocumentによる動的な名前/出典の所有、`.v1.theme`の厳密な読込と色/コントラスト検証。既存設定とkey=value解析を共通化。選択はC4bで接続済み。

C3b: Ctrl+Pの共通設定一覧。テーマ名の部分列照合、上下/Tab/ホイール選択、クリック/Enter実行、フォント値の補完入力。本文・選択・Vim状態を保ち、入力中はIMEを閉じ終了後に復元する。

C3a: NORMALの `:` から設定用Ex入力。Tab/Shift+Tab補完、単行貼付、取消、テーマ/pt/フォントの即時反映と保存。結果は左ステータスへ表示。

C2: 設定の保存・復元、8〜40 ptの本文拡縮（Ctrl+`+` / `-` / `0` とCtrl+wheel）。本文・行番号だけを拡縮し、テーマ・フォント名・サイズを版付きで保存する。

枠なし窓（Snap と影は OS のまま）・Mica のタイトルバーにタブ 1 本と窓の操作・piece table の本文（複数行・スクロール・選択・Ctrl+C/X/V・Ctrl+Z/Y・クリックでキャレット）・
ステータスバーの「通常 | Vim」トグル・OS のライト／ダーク（茄子色 D11・橙 D12）・ファイルの開閉と保存（Ctrl+O / Ctrl+S / Ctrl+Shift+S・起動引数・UTF-8 / BOM / Shift_JIS・CRLF / LF・一時ファイルからの置換・未保存の印と確認）・速さのゲート（Release の exe で 4 本のベンチ・基準値の鍵 5 つ・実機の基準値・起動の内訳・窓を先に見せる起動（約 35 ms）・`WM_PAINT` で 1 フレームに 1 回の描画）・Vim（NORMAL / INSERT・`h j k l 0 $ w b e ^ gg G` と Home / End・回数（オペレータ側と移動側の掛け算）・`x`・`d c y` ＋移動・`dd cc yy`・`D C Y`・`p P`（文字単位と行単位・回数）・無名レジスタは種類つきで本文は LF・`i a I A`・`o O`・Esc・`u` / Ctrl-r・INSERT 1 回が undo 1 単位・VISUAL `v V`（回数・`o`・`d x y c`・表示と Ctrl+C と操作が同じ範囲）。画面移動 `H M L`・Ctrl-d/u/f/b・PgUp/PgDn、window-local な半画面量、Vim の自動追従。本物の Vim 9.1 の oracle が生成した fixture 489 件を CTest が再生）・IME（IMM32・変換中の文字列は本文の外の `Composition` に持って renderer がキャレットの行に差し込んで描く・確定は 1 意図で通常モードは undo 1 単位・Vim INSERT は打鍵として engine へ・Vim NORMAL では IME を切り INSERT で戻す・候補窓はキャレットの直下）・タブの帯は不透明の `title_bar`（D16・アクティブなタブは本文色）・速さのゲートは刺激が届かなかった試行を欠測にし、計測不能を退行と別の終了コード 2 で言う。性能検証は必要な変更で選んで実行する。一致する指紋の基準値で判定し、通常のCIでは測り直さない（ADR 0016 / 0021）。生成物 `VimFixtures.hpp` と `fixtures.json` の一致は CNF-010 が守る。core にテーマの模型（`Theme` / `SyntaxPalette` / `derive_ui`・組み込み 9 テーマ・コントラスト比 4.5 以上を tests が要求。ADR 0017）があり、設定ファイルから指定テーマまたはOS追従を選べる。NORMALのExからも切替可能。Ctrl+Pの設定一覧はC3bで接続済み。

## 動かないもの

64 MiB 超のファイル・文字コードと改行の手動切り替え・IME の再変換と TSF 固有の機能・
Vim の `Ctrl-v`（矩形）/ VISUAL の `p u ~ > < J r I A gv` と `X D C Y`（この縦切りでは何もしない）/ ドラッグで VISUAL /
autoindent / 一般のi/a回数 / テキストオブジェクト / `.` / 名前つきレジスタ / `J r s S` / 全文検索 / 一般Ex（`:w` / `:q`、範囲、パイプ、履歴）・
複数タブ・Ctrl+Pのファイル/フォルダ/ブックマーク/履歴統合・折り返し・横スクロール・ドラッグ選択。

## 次の 1 手

[Issue #77](https://github.com/hideyukiMORI/nene-nib/issues/77)の統合状態を確認し、次候補は分離済みの [Issue #81](https://github.com/hideyukiMORI/nene-nib/issues/81)（行単位VISUAL yankの戻り位置）。dot・全文検索・テキストオブジェクト等も別の焦点Issueとして順に進める。
今回の結果と未確認は [日報](../reports/2026-09-20.md) / [引き継ぎ](../handoffs/2026-09-20.md)。変更に関係する検証だけを行い、成功結果を再利用する。
