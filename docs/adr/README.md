# アーキテクチャ決定記録（ADR）

トレードオフのある判断を、**理由ごと**残す場所。正本ドキュメントが「いま何が規範か」を書くのに対し、
ADR は「なぜそう決めたか」「何を却下したか」を書く。

## 書き方

- 連番 4 桁 ＋ kebab の題名: `NNNN-short-kebab-title.md`
- `0000-template.md` を複製して書く
- 状態は `提案` / `受理` / `却下` / `置換（→ NNNN）`
- **却下した選択肢を必ず書く。** 再提案は、その理由への反論から始めること
- 受理された ADR を後から書き換えない。変えるときは新しい ADR で置き換える
- 文脈は可能な限り実データ（コード・履歴・計測・コンパイラの出力）で裏を取る

## 一覧

| ADR | 題名 | 状態 |
| --- | --- | --- |
| [0001](0001-strictness-is-mechanically-enforced.md) | 厳格さは機械で強制する | 受理 |
| [0002](0002-plain-win32-with-direct2d.md) | UI ライブラリを使わず、素の Win32 ＋ Direct2D / DirectWrite で描く | 受理 |
| [0003](0003-cpp23-clang-cl-foundation-and-measured-limits.md) | C++23 / clang-cl の検査基盤と実測できた限界を固定する | 受理 |
| [0004](0004-ui-thread-plus-one-worker.md) | 並行性は「UI スレッド＋固定ワーカー 1 系統」で、やり取りはメッセージだけ | 受理 |
| [0005](0005-own-vim-engine-verified-against-real-vim.md) | Vim は自前実装で、本物の Vim との差分テストで再現度を担保する | 受理 |
| [0006](0006-speed-gate-simd-and-table-driven-dispatch.md) | 速さはゲートで見張り、SIMD は区画に閉じ、Vim の分岐は表で書く | 受理 |
| [0007](0007-first-slice-frameless-window-direct2d-line.md) | 最初の縦切り: 枠なし窓に Direct2D で 1 行描き、OS のライト／ダークに従う | 受理 |
| [0008](0008-adopted-look-tabs-titlebar-statusbar-mica.md) | 採用した見た目: タブのタイトルバー・モードトグルのステータスバー・Mica・茄子色と橙 | 受理 |
| [0009](0009-editing-slice-piece-table-and-editing-states.md) | 編集の縦切り: piece table の本文・UTF-8 の内部表現・undo の単位・編集中の状態の見た目 | 受理 |
| [0010](0010-file-slice-fileport-encoding-detection-atomic-save.md) | ファイルの縦切り: `FilePort` と `CodePagePort`・文字コードと改行の判別・一時ファイルからの置換・未保存の印 | 受理 |
| [0011](0011-speed-measurement-timing-port-and-paint-coalescing.md) | 速さの縦切り: `TimingPort` の節目・ベンチ 3 本と機械ごとの基準値・`WM_PAINT` で 1 フレームにまとめる描画 | 受理 |
| [0012](0012-vim-engine-first-slice-and-oracle-fixtures.md) | Vim エンジンの最初の縦切り: core の純関数 `vim_step`・閉じた和型の鍵と効果・oracle が生成した fixture の再生 | 受理 |
| [0013](0013-startup-shows-the-window-before-the-device.md) | 起動は窓を先に見せてから D3D の device を作り、「窓が見えるまで」を第 2 の起動の値として基準値に載せる | 受理 |
| [0014](0014-ime-imm32-composition-outside-the-buffer.md) | 日本語入力は IMM32 を ui/win32 が直接受け、変換中の文字列は本文の外に持って描き、Vim の NORMAL では IME を切る | 受理 |
| [0015](0015-vim-operators-register-kind-and-insert-undo-unit.md) | Vim のオペレータは自分の回数を持ち、レジスタは種類を持つ LF の本文、INSERT の編集は 1 つの `Edit` に吸収する | 受理 |
| [0016](0016-ci-speed-reference-per-host-fingerprint.md) | CI の速さの基準値は host の指紋ごとに持ち、基準値の無い host では記録だけにして黙らず、記録を artifact で残す | 受理 |
| [0017](0017-theme-model-and-builtin-colorschemes.md) | テーマは core の値型 `Theme`（UI・本文・出典）で、有名テーマの UI トークンは整数演算の `derive_ui` から導き、組み込み 9 テーマと名前の表は 1 か所 | 受理 |
| [0018](0018-vim-visual-selection-as-range.md) | VISUAL は `VimMode` の 2 値で、engine は `Selection` を読み、選択を範囲に変える純関数 1 つでオペレータの経路に流す | 受理 |
| [0019](0019-vim-viewport-input-and-navigation-effect.md) | 表示領域は engine への入力で、画面移動は選択と先頭行を一緒に返す | 受理 |
| [0020](0020-versioned-editor-settings-and-point-font-size.md) | 設定は版付きで保存し、本文フォントは pt から描画とクリックへ同じ配置を導く | 受理 |
| [0021](0021-diff-scoped-verification-and-result-reuse.md) | 検証は差分から選び、関連入力が不変の成功結果を再利用する | 受理 |
| [0022](0022-ex-command-line-and-settings-evaluation.md) | Ex入力は本文から分離し設定の評価と保存を共用する | 受理 |
| [0023](0023-command-palette-and-shared-input-session.md) | Ctrl+PとExは入力session・候補・設定評価を共有する | 受理 |
| [0024](0024-user-theme-file-and-owned-values.md) | 利用者テーマは版付きファイルから所有する値へ検証して読む | 受理 |
| [0025](0025-user-theme-catalog-and-selection.md) | 利用者テーマの不変カタログを設定・Ex・Ctrl+Pで共有する | 受理 |
| [0026](0026-vim-character-search-and-scoped-oracle.md) | 行内文字検索の待ちと記憶を分け、oracleの未変更結果を再利用する | 受理 |
| [0027](0027-vim-line-jumps-and-exclusive-input-wait.md) | 指定行移動は既存motionへ接続し、次キー待ちは排他的に持つ | 受理 |
| [0028](0028-vim-open-lines-and-insert-repeat.md) | 行を開く操作と回数付きINSERTは既存の挿入・履歴へ流す | 受理 |
| [0029](0029-vim-character-replace-as-range-edit.md) | rは排他的な次文字待ちと範囲置換の効果で表す | 受理 |
| [0030](0030-vim-dot-repeat-as-key-replay.md) | `.` は直前の変更を鍵の列として記録し同じ経路へ再生する | 受理 |
| [0031](0031-vim-text-objects-as-one-range-function.md) | テキストオブジェクトは 1 本の範囲関数で d c y と VISUAL に同じ範囲を渡す | 受理 |
| [0032](0032-vim-search-as-input-line-and-one-key.md) | 検索は Ex と同じ入力行から入り、確定は engine の 1 つの鍵として届く | 受理 |
| [0033](0033-vim-visual-dot-repeat-by-extent.md) | VISUAL の変更の `.` は範囲の大きさを記録し、選び直してから鍵を再生する | 受理 |
| [0034](0034-vim-virtual-column-one-table.md) | Vim の桁は core の表示幅の表 1 本で仮想桁に直し、使う経路を閉じる | 受理 |
| [0035](0035-vim-visual-block-as-column-ranges.md) | 矩形 VISUAL は仮想桁の矩形を 1 本の範囲関数で決め、行ごとの範囲の列として編集する | 受理 |
| [0036](0036-line-ending-owned-by-text-buffer.md) | 改行の形は本文が 1 つ持ち、`\r` が改行の一部かどうかはその形だけで決まる | 受理 |
| [0037](0037-search-highlight-as-visible-line-spans.md) | 検索の当たりは見えている行だけを照合器で数え、行ごとの列として描く（`hlsearch` は既定オン） | 受理 |
| [0038](0038-model-per-seat-and-scripted-preparation.md) | 背景席は仕事の種類でモデルを明示し、繰り返す下ごしらえはスクリプトにする | 受理 |
| [0039](0039-implementation-seat-per-step-and-small-tool-output.md) | 実装席は工程ごとに新しい席にし、道具の出力を小さく保つ | 受理 |
| [0040](0040-display-line-with-control-glyphs.md) | 描画用の行は core の純関数が作り、制御文字は `^X`、書式用文字は `<xxxx>` に置き換えて桁の対応表を持つ | 受理 |
| [0041](0041-vim-incsearch-preview-outside-the-engine.md) | `incsearch` は入力中の当たりを engine の外の preview として持ち、確定は今までどおり 1 つの鍵で届く | 受理 |
| [0042](0042-unit-tests-split-by-scope.md) | 単体テストは scope ごとの翻訳単位に分け、`VimStep.cpp` は純関数の切り出しだけで縮める | 受理 |
| [0043](0043-incsearch-hop-moves-the-search-start.md) | incsearch の Ctrl-G / Ctrl-T は preview の検索の起点を当たりへ動かし、確定の鍵はその起点を運ぶ | 受理 |
| [0044](0044-add-buffer-in-chunks.md) | piece table の add バッファは固定長の chunk の列で持ち、piece は chunk を跨がず、先端を持つ値だけがその場で伸ばす | 受理 |
| [0045](0045-tab-stops-follow-vim-columns.md) | 本文の Tab は空白 8 個ぶんの tab stop で描き、Vim の仮想桁と同じ位置に止まる | 受理 |
| [0046](0046-macros-record-keys-and-replay-through-dot-path.md) | マクロ `q` `@` は名前つきの鍵列を engine が録り、再生は `.` と同じ経路で 1 鍵ずつ流す | 受理 |
| [0047](0047-shared-newline-index-per-source.md) | 改行の索引はバッファ（original と各 add chunk）ごとに 1 本を共有し、piece はその窓だけを持つ | 受理 |
| [0048](0048-named-registers-as-one-text-table.md) | 名前つきレジスタ `"a` はマクロと同じ 26 本の本文の表で、鍵列 ↔ 本文の写しは core の 1 対 | 受理 |
| [0049](0049-space-backspace-wrap-motions.md) | `<Space>` `<BS>` は行をまたぐ `l` `h` で、オペレータ待ちと VISUAL では行末の位置に一度止まる | 受理 |
