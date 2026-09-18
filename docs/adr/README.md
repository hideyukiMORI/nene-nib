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
| [0016](0016-ci-speed-reference-per-host-fingerprint.md) | CI の速さの基準値は host の指紋ごとに持ち、基準値の無い host では記録だけにして黙らず、記録を artifact で残す | 受理 |
| [0017](0017-theme-model-and-builtin-colorschemes.md) | テーマは core の値型 `Theme`（UI・本文・出典）で、有名テーマの UI トークンは整数演算の `derive_ui` から導き、組み込み 9 テーマと名前の表は 1 か所 | 受理 |
