# Markdown プレビューの差し替えの費用（16 MiB）とプレビューを開いた 1 打鍵（#818・枠）

外部講評（2026-10-22・#813 の 3 点目）への答えの記録。ADR 0155 の決定 7（錨から測る `PreviewBlockTops`）と
ADR 0138 の任意のベンチの形を使う。**数字は設計席が静かな機械で採る**（この文書は表の枠だけ・実装席は数字を書かない）。
基準値（`eng/perf-reference.json`）には載せず、ゲート（QLT-014）の判定にも使わない。

## 1. 道具

| 道具 | 何を測るか | 使い方 |
|---|---|---|
| probe `nib_preview_probe`（`tests/preview/`・CTest 外） | 決定的に作った Markdown の `parse_markdown` と、`PreviewTextLayouts` ＋ `PreviewBlockTops::measure` の時間（製品の `PreviewPane` を通さない） | （#818 の probe の節で決める） |
| 任意のベンチ `key-to-frame-single-preview-open`（`eng/measure-speed.py`） | `preview-1000.md` を開いて Ctrl+Shift+M でプレビューを出した後の 1 打鍵 → 次の提示 | `python eng/measure-speed.py --bench key-to-frame-single-preview-open --record [--repetitions N]`（`--check` / `--adopt` / `--values` は断る） |

ベンチの文書 `preview-1000.md`: 1000 行・CRLF・BOM なし・5,944,334 bytes・SHA-256
`5878ba0b9dff061ce45b27cc92d8775277cc2fde4316cf981b11668232bbf3cf`。行は「印（`# ` / なし / `- ` を行ごとに回す）・
6 桁の番号と空白・`LONG_LINE_SENTENCE` の 86 回の連結」で、1 行 1,987 code points（巨大な行のしきい値 16,384 の下）。
番号を印の後に置くのは、行頭が番号だと見出しとリストにならないため。
製品はプレビューの節目を打たないので、試行は Ctrl+Shift+M の後に窓のスレッドが静かになるのを待ち、
プレビューの tick（0.5 秒）の後にもう一度静かになるのを待ってから暖機の 1 打鍵を送る。

## 2. 条件（設計席が埋める）

| 項目 | 値 |
|---|---|
| 日時 | |
| commit（full SHA） | |
| 構成（probe / ベンチの exe） | |
| 機械（指紋・CPU・GPU・DPI） | |
| 負荷（測る前の読み・止めた席） | |
| probe の入力（bytes・SHA-256・ブロック数） | |
| viewport | 幅 900 DIP・高さ 640 DIP・DPI 120 |
| 反復の数 | |

## 3. probe（16 MiB・錨ごと）

| 錨 | parse ms | measure ms | layouts |
|---|---|---|---|
| head（0） | | | |
| middle（ブロックの数の半分） | | | |
| tail（最後の 10 個前） | | | |

## 4. ベンチ（プレビューを開いた 1 打鍵）

| ベンチ | 反復 | 中央値 ms | 最小 ms | 最大 ms | 欠測 | 比べる値（`key-to-frame-single-long-line` の基準値） |
|---|---|---|---|---|---|---|
| `key-to-frame-single-preview-open` | | | | | | |

## 5. 読み（設計席）

（数字を採った後に書く。）
