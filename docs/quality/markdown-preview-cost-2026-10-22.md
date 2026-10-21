# Markdown プレビューの差し替えの費用（16 MiB）とプレビューを開いた 1 打鍵（#818・枠）

外部講評（2026-10-22・#813 の 3 点目）への答えの記録。ADR 0155 の決定 7（錨から測る `PreviewBlockTops`）と
ADR 0138 の任意のベンチの形を使う。**数字は設計席が静かな機械で採る**（この文書は表の枠だけ・実装席は数字を書かない）。
基準値（`eng/perf-reference.json`）には載せず、ゲート（QLT-014）の判定にも使わない。

## 1. 道具

| 道具 | 何を測るか | 使い方 |
|---|---|---|
| probe `nib_preview_probe`（`tests/preview/`・CTest 外） | 決定的に作った Markdown の `parse_markdown` と、`PreviewTextLayouts` ＋ `PreviewBlockTops::measure` の時間（製品の `PreviewPane` を通さない） | `build/nib_preview_probe.exe <bytes> <head\|middle\|tail> <json>`（1 回の実行で 1 点・反復は設計席の台本）。JSON は schema・bytes・sha256・blocks・anchor・first・measured・parseMs・measureMs・layoutsCreated・viewport・commit・dirty・configuration。節目は `<json>.timing.json` |
| 任意のベンチ `key-to-frame-single-preview-open`（`eng/measure-speed.py`） | `preview-1000.md` を開いて Ctrl+Shift+M でプレビューを出した後の 1 打鍵 → 次の提示 | `python eng/measure-speed.py --bench key-to-frame-single-preview-open --record [--repetitions N]`（`--check` / `--adopt` / `--values` は断る） |

probe の入力は `tests/performance/MarkdownProbe.cpp` の `markdown_input` と同じ規則で `<bytes>` まで作る（16 MiB = 16,777,216 を渡すと同じバイト列）。
preview_tests は performance_tests に依存できないので、生成は `tests/preview/PreviewProbe.cpp` に写してある。
時刻は `Win32TimingAdapter` の節目だけで取る（ARC-007）。parse と measure をそれぞれ `probe_started` / `probe_finished` の 1 対で挟む。
そのため `eng/architecture.json` の `preview_tests` の依存に `adapters_win32` を足した（試験の target だけ・製品の依存は不変）。
viewport は錨・900 DIP 幅・640 DIP 高さ・DPI 120 で、錨は head = 0・middle = ブロックの数の半分・tail = 最後の 10 個前。

ベンチの文書 `preview-1000.md`: 1000 行・CRLF・BOM なし・5,944,334 bytes・SHA-256
`5878ba0b9dff061ce45b27cc92d8775277cc2fde4316cf981b11668232bbf3cf`。行は「印（`# ` / なし / `- ` を行ごとに回す）・
6 桁の番号と空白・`LONG_LINE_SENTENCE` の 86 回の連結」で、1 行 1,987 code points（巨大な行のしきい値 16,384 の下）。
番号を印の後に置くのは、行頭が番号だと見出しとリストにならないため。
製品はプレビューの節目を打たないので、試行は Ctrl+Shift+M の後に窓のスレッドが静かになるのを待ち、
プレビューの tick（0.5 秒）の後にもう一度静かになるのを待ってから暖機の 1 打鍵を送る。

## 2. 条件（設計席が埋める）

| 項目 | 値 |
|---|---|
| 日時 | 2026-10-21 12:00〜12:16 JST（記録の通し番号は 2026-10-22） |
| commit（full SHA） | probe: `8db12bd462ca788f4894ababb77c94ee1f619129`（PR #819 の HEAD・main `a2edcd0` と同じ内容）。ベンチの exe: main `356c1f95804e5022606d7115063c1f48d4a77b21`（#792 まで・プレビュー #800 / #801 入り） |
| 構成（probe / ベンチの exe） | probe: Release（`D:\NeNeNib\worktrees\818-preview-cost\build-release`）。ベンチ: Release（`D:\NeNeNib\evidence\20261022-trial\NeNeNib-main-356c1f9.exe`・SHA-256 `347bfd52…c9983`） |
| 機械（指紋・CPU・GPU・DPI） | `bc8a356f37c68491`・i9-10850K・RTX 3090・120 dpi |
| 負荷（測る前の読み・止めた席） | 席は全部止まっていた・CI は遠隔だけ・hide 不在（負荷は読まず、直前の #810 / #811 のゲートと同じ条件） |
| probe の入力（bytes・SHA-256・ブロック数） | 16,777,216 bytes・SHA-256 `33abde08e43df8452fe70a8c3853005b977e9b698b6f0b6b8ea7ae1c61cbd41c`・209,263 ブロック |
| viewport | 幅 900 DIP・高さ 640 DIP・DPI 120 |
| 反復の数 | probe: 錨ごとに 3 回（1 回の実行で 1 点）。ベンチ: 5 本 |

## 3. probe（16 MiB・錨ごと）

| 錨 | parse ms | measure ms | layouts |
|---|---|---|---|
| head（0） | 143.6（143.4 / 143.6 / 143.7） | 5.93（5.90 / 5.93 / 5.95） | 11（測った要素 9） |
| middle（ブロックの数の半分） | 142.8（142.6 / 142.8 / 149.3） | 5.82（5.79 / 5.82 / 5.93） | 14（測った要素 11） |
| tail（最後の 10 個前） | 143.2（142.1 / 143.2 / 147.7） | 5.82（5.69 / 5.82 / 5.84） | 12（測った要素 10） |

## 4. ベンチ（プレビューを開いた 1 打鍵）

| ベンチ | 反復 | 中央値 ms | 最小 ms | 最大 ms | 欠測 | 比べる値（`key-to-frame-single-long-line` の基準値） |
|---|---|---|---|---|---|---|
| `key-to-frame-single-preview-open` | 5 | 3.194 | 3.121 | 4.408 | 0 | 1.57（長い行・基準値）・同じ日の長い行の実測 1.94・空の文書の単打 0.65 |

## 5. 読み（設計席）

- **差し替えの費用は parse が支配し、measure は錨に依らず約 6 ms。** 16 MiB・209,263 ブロックでも measure は 5.8〜5.9 ms（作る layout は 11〜14・測る要素は枠に入る 9〜11 個）で、錨が先頭でも中央でも末尾でも同じ。ADR 0155 の決定「錨から枠の高さまでだけ測る」が効いている（直す前の形なら 209,263 個の layout を作っていた）。parse 143 ms は worker の裏で走り（打鍵の道には乗らない）、#765 の probe の 143.2 ms と一致。
- **プレビューを開いた 1 打鍵は 3.19 ms。** 同じ文書の形（1,000 行・1 行約 2,000 文字）の長い行のベンチ（プレビュー無し）が同日 1.94 ms なので、プレビューの枠の分は約 1.25 ms（毎 frame の `PreviewPane` の描画・tick ごとの無条件 `invalidate` は #813）。16.7 ms の予算の内で、基準値は持たない（記録だけ・ADR 0138）。
- 後続: #813（tick ごとの `invalidate` を application の「変わった」で抑える・リストの区切りの全体走査）。数字の採り直しはそのとき。
- 記録: `D:/NeNeNib/outputs/20261022/preview-cost/`（probe の JSON 9 本と timing・ベンチの `bench-preview-open-2026-10-21T03-16-11Z.json`）。
