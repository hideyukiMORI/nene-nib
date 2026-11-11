# ファイルエクスプローラーの見た目（採用案 B「色の点」・D81）

2026-11-10 夜 / hide「推しの B」/ Issue #1390 / ADR 0162 の追補（#1390）/ モック [`2026-11-10-explorer-look.html`](2026-11-10-explorer-look.html) の案 B の節。前の採用案は [`2026-10-28-explorer-look.md`](2026-10-28-explorer-look.md)（案 A・D66）。色は `core::Palette` と構文の色（`core::SyntaxPalette`）のトークンだけ（ADR 0008 の決定 8）。新しい色のトークンは足さない。テーマを切り替えると全部が追従する。

面の地をタブの帯と同じ色にして、帯と面が L 字の枠になり、本文が左上の角の丸い紙のように載る。ファイルは小さな色の点、フォルダは細い線の絵。

| 要素 | 値 |
|---|---|
| 面の地 | `title_bar`（タブの帯と同じ色。帯と面が L 字の枠になる） |
| 面と本文の境 | 面の右端に 1 DIP の `panel_border`（面に焦点があるとき `accent`・今と同じ）。本文の帯の**左上の角**を半径 8 DIP で丸める（角の外は `title_bar`）。面が消えているときは丸めない |
| 字 | UI の字体（タブの帯の題名と同じ text format の族と大きさ）。本文の字の大きさ（`fontsize`）に追従しない |
| 行の高さ | 22 DIP（固定）。先頭の行（ルート名）は 30 DIP |
| ルート名の行 | フォルダの名前・`text`・semibold・左の余白 12 DIP。ルートが無いときの案内の文は `muted`（文は今のまま） |
| 行の面 | 左右 6 DIP 内側・角の半径 5 DIP の丸い面 |
| 選んだ行 | `background`（本文の色）の丸い面。面に焦点があるときは `accent` の 1 DIP の枠を重ねる |
| マウスを載せた行 | `tab_hover` の丸い面（選んだ行には重ねない）。載せた行は application の状態（タブの帯と同じ・ui は意図を送るだけ） |
| アクティブな文書の行 | 名前を semibold（選んだ行と別の印。両方が同じ行なら両方） |
| 未保存の文書の行 | 行の右端に `accent` の 6 DIP の丸（タブの「●」と同じ意味。開いているタブのどれかが未保存のファイル） |
| 字下げ | 深さ × 12 DIP。深さごとに 1 DIP の縦の線（`panel_border`）を、開いたフォルダの子の行に通す |
| 三角 | 文字をやめ、幾何で描く山形（閉 `>`・開 `v`・線の太さ 1.25 DIP・`muted`・升 16 DIP） |
| フォルダの印 | 細い線のフォルダの絵（16 DIP の升・線 1.25 DIP・`muted`・開いているときは開いた形） |
| ファイルの印 | 16 DIP の升の中央に 6 DIP の角丸（半径 1.5 DIP）の四角。色は**文書の言語**（ADR 0154 の拡張子 → 言語の 1 本）から決める構文の色のトークン: C / C++ のソース = `function`・ヘッダ = `type`・Markdown = `string`・JSON / YAML / TOML = `keyword`・JS / TS = `type`・Python = `function`・HTML / XML / CSS = `operators`・ほか（plain）= `muted`。言語 → トークンの表は core の 1 か所 |
| 名前 | `text`。隠し属性は `muted`。長い名前は末尾を「…」（今のまま） |
| 「さらに…」の行 | 今のまま（`muted`） |

git の枝の名前（モックの右上の `main`）は入れない（飾りだった）。

## 実装で凍結した値（#1390）

表に無い寸法は、モックの案 B の CSS から写した。

- 行の丸い面の中の左右の余白 4 DIP（`.B .row` の `padding-right` と行の頭の 4 DIP）。山形の升は「丸い面の左 + 4 DIP + 深さ × 12 DIP」、印の升はその右、名前は印の升から 6 DIP 空ける（`.ico` の `margin-right`）。
- 未保存の丸の右端は丸い面の右端から 14 DIP（`padding-right` 4 DIP と `.dot` の `margin-right` 10 DIP）。名前は丸の 6 DIP 手前で切る。
- 縦の線は、その深さの山形の升の中央（升 16 DIP の左から 7 DIP）。
- 山形とフォルダの絵の点は、モックの SVG を 16 DIP の升へ写したもの（`ExplorerPainter.cpp` の表）。線の端と継ぎ目は丸。
- 本文の左上の角の弧は、面の右端の境界と同じ色・太さで引き、境界は弧の下（8 DIP）から始める。
- ファイルの印の色の役割は core の `explorer_mark_role` の 1 本（言語は `language_of_path`、C / C++ のヘッダは `header_of_path` の `h` `hh` `hpp` `hxx`）。表にない言語（PowerShell・shell・CMake・Rust ほか）は `muted`。

## 置き場

配置は core の `explorer_layout`・`explorer_line`・`explorer_line_at`・`explorer_row_parts`・`explorer_guide_x`（描画と当たりが同じ値を読む）、表示値は application の `ExplorerView`（`hovered`）と `ExplorerRowView`（`mark` `active` `unsaved`）、描画は ui の `ExplorerPainter`。
