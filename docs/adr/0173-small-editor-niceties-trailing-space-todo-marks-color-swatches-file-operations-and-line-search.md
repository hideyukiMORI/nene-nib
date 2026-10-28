# ADR 0173 — 末尾の空白・TODO の強調・色の見本・ファイル操作・行のファジー検索は、既存の道に 1 行ずつ足す

- 状態: 受理（施主決定 D70（2 段目の 5 本・2026-10-28「簡単で速度を犠牲にしないものは取り入れる」）。実装は 5 席（各 0.5〜1 席日）・受理は D41 の形）
- 日付: 2026-10-28
- Issue: #1011（末尾の空白）・#1012（TODO）・#1013（色の見本）・#1014（ファイル操作）・#1015（行のファジー検索）・計画 `docs/plans/2026-10-28-editing-features-roadmap.md` §3.5
- 規則: FR-024 / FR-025 / FR-026、ARC-001 / ARC-003 / ARC-004 / ARC-007、CPP-002 / CPP-005 / CPP-011、QLT-014、ADR 0008 / 0040（制御文字と桁の表）/ 0060（面の出どころ・`palette_marks`）/ 0062（面は見えている行だけ）/ 0135（一時ファイルと保存）/ 0156（構文の区間と役割）/ 0162（エクスプローラー）/ 0170（`features` の欄）

## 決定

1. **末尾の空白（#1011）。** 見えている行の行末の空白（空白と Tab）を `muted` の 30% の面で描く（`LineView` に `std::optional<Column> trailing_from` を 1 欄・`display_line` の桁の表から O(行の末尾の空白の長さ)）。`trailing_space`（既定オン・`features`）。`strip_whitespace`（既定オフ）は保存の直前に行末の空白を消す 1 回の `Edit`（undo 1 単位・保存の本文は `encoded_parts` の 1 本のまま）。
2. **TODO の強調（#1012）。** 字句器がコメントの区間を出した後、`SyntaxLines` が区間の中の `TODO` `FIXME` `XXX` `HACK`（語の境・大文字だけ・直後が `:` か空白か終わり）を `warning` の区間に、`NOTE` を `function` の区間に切り出す（字句器は触らない・1 か所・コメントの区間の中だけ探す）。`todo_marks`（既定オン）。plain の文書（区間が無い）は出ない（限界）。
3. **色の見本（#1013）。** 見えている行の `number` / `string` / `constant` の区間と、plain の文書は行の全部から `#RRGGBB` `#RGB` `#RRGGBBAA` `rgb(…)` `rgba(…)` `hsl(…)` を純関数 `color_swatches_in(line_text, spans) -> std::vector<ColorSwatch{Column after; RgbColor color;}>` で拾い、renderer が直後に行高の 60% の四角（`FillRectangle` 1 つ・枠は `panel_border` 1 DIP）を描く。文字の位置はずらさない（直後の升の左に 2 DIP 寄せて重ねる・升を占めない）。`color_swatches`（既定オン）。
4. **ファイル操作（#1014・エクスプローラーの後）。** エクスプローラーの行で `r`（名前の変更・面の先頭の入力行に今の名前・Enter で確定）・`d`（ごみ箱へ・1 行の問い「y / n」）・`a`（新規ファイル・同じフォルダに名前を入力）・`A`（新規フォルダ）。Ex `:Rename {name}`（アクティブな文書・開いているタブの名前も替える）・`:Delete`（アクティブな文書をごみ箱へ・タブを閉じる・1 行の問い）。adapter は `IFileOperation`（`FOF_ALLOWUNDO` でごみ箱・`FOF_NO_UI`・失敗は値）の 1 本 `FileOperationPort{rename, trash, create_file, create_folder}`。開いているタブのファイルを消すときは先にタブを閉じる（未保存なら保存の問い）。
5. **行のファジー検索（#1015）。** Ctrl+P の出どころ `document_lines`（記号 `>`・`palette_marks` に 1 行・`PaletteScope` と `PaletteOrigin` に 1 値）。候補はアクティブな文書の行（`<行番号>  <本文（256 バイトで切る）>`・空行は出さない・16 MiB 超と巨大な行の文書は出どころを出さない・限界）。候補の列は面を開くときに 1 回作る（行の数に比例・16 MiB 以下なら 200,000 行で数十 ms の見込み・見えている行だけ描く・ADR 0062）。選ぶとその行の最初の非空白へ（`PlaceCaret`・`''` を置く）。
6. **契約。** それぞれ scope 1 つ（`--trailing-space` `--todo-marks` `--color-swatches` `--file-operations`（替え玉）`--palette-lines`）と窓の画素（1〜3）。adapters のファイル操作は一時フォルダで本物の OS（ごみ箱は `FOF_ALLOWUNDO` の結果だけ見て中身は確かめない）。
7. **費用。** 1〜3 は見えている行だけ・O(見えている行の長さ)。4 は操作のときだけ。5 は面を開くときだけ。既定オンの 1〜3 は空の文書の 1 打鍵の A/B を PR 本文に（+1% の線）。
8. **限界。** 末尾の空白の `listchars` 相当の文字（`·`）は出さない（面だけ）。TODO の語の設定。色の見本の名前付きの色（`red`）。ファイル操作の複数選択・移動・コピー。行の検索の正規表現。

## 強制

- 見えている行だけ・打鍵の道の不変: **active**（契約・QLT-014）。
- ファイル操作が adapter の 1 本・ごみ箱: **active**（契約・`eng/symbols.py`）。
