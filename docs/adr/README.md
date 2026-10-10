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
| [0050](0050-numbered-and-small-delete-registers.md) | 数字レジスタ `"0`〜`"9` と小削除 `"-` は書き手 1 本の規則で埋め、`.` は番号を 1 つ進めて再生する | 受理 |
| [0051](0051-clipboard-registers-through-controller.md) | クリップボードのレジスタ `"+` `"*` は engine が写しを読み書きし、OS との往復は controller が `ClipboardPort` で行う | 受理 |
| [0052](0052-undo-caret-restores-the-first-edit-position.md) | `u` と Ctrl-r の後のキャレットは undo の単位が覚えた戻り先で、戻り先は最初の編集の瞬間のキャレット | 受理 |
| [0053](0053-vim-character-is-a-code-point-with-its-zero-width-followers.md) | Vim の 1 文字はコードポイント 1 つと直後に続く幅 0 のコードポイントの列で、判定は仮想桁の表を使う | 受理 |
| [0054](0054-fixture-key-notation-has-one-table.md) | fixture の鍵の記法の表は oracle の 1 つで、C++ の表はそこから作る生成物 | 受理 |
| [0055](0055-pasted-text-takes-the-document-line-ending.md) | 貼り付けた本文の改行は文書の改行の形に揃える | 受理 |
| [0056](0056-tabs-park-inactive-documents-behind-the-active-one.md) | 複数タブは、アクティブな文書を今の形のまま持ち、ほかの文書は不変の束として脇に置く | 受理 |
| [0057](0057-tab-destination-is-one-pure-function.md) | タブの行き先は 1 つの純関数が決め、Vim の `gt` `gT` と Ex と一覧が共用する | 受理 |
| [0058](0058-ctrl-tab-walks-tabs-in-recent-order.md) | Ctrl+Tab は最近使った順に歩き、Ctrl を離したときに確定する | 受理 |
| [0059](0059-session-remembers-tabs-and-loads-them-on-view.md) | 前回のタブは別のファイルに覚え、起動のときは見ていたタブだけを読む | 受理 |
| [0060](0060-ctrl-p-lists-files-and-marks-select-the-source.md) | Ctrl+P の一覧は 1 つの候補の列で、行頭の記号が出どころを絞る | 受理 |
| [0061](0061-the-palette-takes-ime-input-and-opens-with-it-off.md) | Ctrl+P の面は日本語入力を受け、IME はオフで開いて使う人に任せる | 受理 |
| [0062](0062-the-worker-reads-the-folder-and-the-palette-keeps-its-result.md) | 同じフォルダは裏のワーカーが読み、面は絞り込みの結果を持って見えている行だけを載せる | 受理 |
| [0063](0063-bookmarks-are-explicit-and-share-the-file-palette.md) | ブックマークは明示操作で保存し、同じファイルの候補を一覧で共有する | 受理 |
| [0064](0064-ex-file-commands-open-the-shared-palette.md) | Ex のファイル命令は共通の一覧を開く | 受理 |
| [0065](0065-ordinary-combining-character-boundaries.md) | 通常モードの結合文字と削除方向を区別する | 受理 |
| [0066](0066-ex-save-and-quit-use-document-operations.md) | Ex の保存と終了は既存の文書操作を通る | 受理 |
| [0067](0067-ex-filenames-share-atomic-save-policy.md) | Ex のファイル名は共通保存へ渡し、新規作成を原子的に守る | 受理 |
| [0068](0068-split-starts-with-a-bounded-render-probe.md) | split は文書タブを維持する案と限定した描画試作から判断する | 受理（実験方法・製品採用は未決） |
| [0069](0069-reuse-visible-body-text-layouts.md) | 表示中の本文の文字組みを描画器が再利用する | 受理 |
| [0070](0070-status-text-layouts-stay-in-a-fixed-renderer-cache.md) | ステータスの文字組みは描画器の固定枠に保持する | 受理 |
| [0071](0071-cache-complete-system-font-fallback-requests.md) | 完結した字体選択要求の固定上限保持 | 受理・性能総合hold |
| [0072（実験記録）](../quality/gate-proofs.md#5-cm--字体選択可視字形表示幅とsplit再評価issue-291adr-007100730074) | 行bitmapの保持（原ADRは隔離実験枝と証拠bundle） | 最終候補に不採用 |
| [0073](0073-visible-body-glyph-capsule.md) | 本文の可視字形の保持 | 受理・性能総合hold |
| [0074](0074-index-display-width-from-the-canonical-ranges.md) | 正本の幅表からBMP索引を生成 | 受理・性能総合hold |
| [0075](0075-merge-acceptance-uses-the-speed-gate-and-effect-size.md) | 性能改善の統合は正式な速さのゲートと効果の大きさで受理し、事前実験の保留は記録として残す | 受理 |
| [0076](0076-sdk-fixed-com-signatures-are-a-permanent-boundary-rule.md) | SDK が固定した COM の署名は恒久の境界の規則にし、waiver を閉じる | 受理 |
| [0077](0077-render-cache-contracts-are-tested-without-a-window.md) | 描画の保持の契約は窓を作らない試験で守り、字体選択の保持の取り付けは最善の努力にする | 受理 |
| [0078](0078-operations-are-one-table-for-keys-and-the-f1-list.md) | 操作は core の 1 つの表に持ち、鍵と F1 の一覧が同じ 1 本の道で実行する | 受理 |
| [0079](0079-operation-guides-and-settings-migration.md) | 操作の案内は操作表から描き、設定は旧版を残して移行する | 受理 |
| [0080](0080-detected-file-text-owns-the-validated-bytes.md) | 文字コードを判定した本文が検証済みのバイト列を所有し、本文へ移す | 受理 |
| [0081](0081-consume-temporary-editor-state-through-one-update-path.md) | 一時的なエディタ状態は同じ更新経路へ所有権を渡し、不要な置換を省く | 受理 |
| [0082](0082-scoped-probes-compare-identical-work-without-a-window.md) | 同じ固定処理を窓なしで前後比較する | 受理 |
| [0083](0083-reuse-gutter-layouts-and-swap-chain-target.md) | 行番号の文字組みと描画先を寿命の内で再利用する | 受理 |
| [0084](0084-intent-delivery-does-not-build-visible-lines.md) | 意図の反映値は行を作らず、描画と当たり判定でだけ frame を作る | 受理 |
| [0085](0085-completed-history-edits-are-private-immutable-values.md) | 確定済み履歴の編集は私有の不変値として共有する | 受理・#334で採用 |
| [0086](0086-ascii-runs-stay-inside-the-utf8-validator.md) | ASCIIの連続区間はUTF-8検証の正典の中でまとめて数える | 受理・#334で採用 |
| [0087](0087-release-thinlto-keeps-symbol-boundaries.md) | ReleaseのThinLTOとシンボル境界の実験 | 製品不採用・実験記録 |
| [0088](0088-vim-registers-are-private-immutable-snapshots.md) | Vimレジスタ本文は私有の不変snapshotで共有する | 受理・#334で採用 |
| [0089](0089-recorded-vim-keys-are-private-immutable-chunks.md) | 記録したVim鍵列は私有の不変chunkとして共有する | 受理・#334で採用 |
| [0090](0090-block-caret-and-recording-reuse-retained-text.md) | ブロックカーソルと録画表示も保持済みの文字を使う | 受理・#334で採用 |
| [0091](0091-palette-filtering-reuses-the-previous-candidate-set.md) | 一覧の照合は小文字の写しを作らず前の候補集合を再利用する | 受理・#341で採用 |
| [0092](0092-cp932-conversion-writes-the-bounded-output-once.md) | CP932変換は上限内の出力を一回で書く | 受理・#341で採用 |
| [0093](0093-pattern-evaluation-keeps-prioritized-states-per-position.md) | 検索の照合は文字位置ごとに優先順を保った状態を一度ずつ評価する | 技術受理・#353 |
| [0094](0094-column-offset-stops-on-the-shared-piece-walk.md) | 桁からbyte位置への変換は共通の断片走査を必要な境界で止める | 技術受理・#353 |
| [0095](0095-visible-line-spans-advance-through-the-owned-line.md) | 行の選択と検索の桁は所有済み本文の端点を順に数える | 技術受理・#355 |
| [0096](0096-palette-location-matching-borrows-complete-text-segments.md) | 一覧の場所照合は完結した文字列の区間を借りて読む | 技術受理・#358 |
| [0097](0097-renderer-positions-advance-through-the-display-line.md) | 描画の桁変換は表示行の端点を順に数える | 技術受理・#360 |
| [0098](0098-tinted-text-reuses-the-body-glyph-path.md) | 色を塗り直す文字も本文の字形経路を使う | 技術受理・#362 |
| [0099](0099-tint-clips-outside-the-current-target-are-skipped.md) | 現在の描画面の完全外側にある着色clipを省く | 実験不採用・#364 |
| [0100](0100-visible-lines-reserve-only-the-existing-row-range.md) | 表示行一覧は既存の生成範囲ぶんを一度予約する | 提案・#367 |
