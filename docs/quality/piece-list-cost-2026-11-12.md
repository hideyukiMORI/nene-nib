# piece の列の費用（#1474 の下ごしらえ G3・2026-11-12）

> 下ごしらえの席 G3 の記録を写したもの（元は `D:/NeNeNib/outputs/20261112/1474/groundwork.md`）。決定は [ADR 0183](../adr/0183-piece-list-prefix-sums-and-binary-search.md)。段 1 の前後の probe は末尾の「段 1 の probe」。

- 起点: main `ebb41746`。作業木 `D:/NeNeNib/worktrees/1474-groundwork`（detached・commit なし）。
- 一時の harness: 作業木の `tests/performance/G3PieceHarness.cpp` と `CMakeLists.txt` の末尾の一時 target（`g3_piece_harness`・tidy を外した Release・作業木の `build-rel/`）。**commit しない。**
- ログ: 作業木の `out/1474-groundwork/run-*.log`。
- 機械は他の席で混んでいた（測定の終わりの CPU 負荷 100%）。**数字は比で読む。** 絶対値は基準値と比べない。

---

## 1. 今の作りの棚卸し

### 値の意味

- `TextBuffer` は不変の値。共有されるもの:
  - `original_`（`shared_ptr<const string>`）と `original_newlines_`
  - `pieces_`（`shared_ptr<const vector<Piece>>`・ADR 0180）
  - `columns_`（`shared_ptr<const VirtualColumnIndex>`・持つ文書だけ）
- 値ごとに持つもの: `add_`（`vector<shared_ptr<AddChunk>>`）。写すと chunk の数 C だけ参照数を上げる。
- `Piece` は 48 バイト（source・chunk・start・length・newline の窓の 2 端）。

### `replaced` 1 回が作り直すもの

記号: P は piece の数、C は add の chunk の数（add の総量 ÷ 64 KiB）、K は仮想桁の塊の数（N ÷ 64 KiB）、m は挿入するバイト数。

| 作り直すもの | 費用 | 中身 |
|---|---|---|
| piece の列 | **O(P)** | `collect` が前後の piece を新しい vector に push。端の 2 piece は `clipped` で、`count_before` が O(1) 程度 |
| 長さと改行の合計 | **O(P)×2** | 構築子の `total_length` と `total_newlines` が列をもう 2 周する |
| piece の束 | 確保 1 回 | `make_shared<const vector>` |
| add の列 | **O(C)** | `auto add = add_` が vector と shared_ptr を C 個写す（参照数の加算が C 回） |
| add の本体 | O(m) | 先端を持つ値だけがその場で伸ばす（ADR 0044）。索引も同じ所で伸びる |
| 仮想桁の索引（持つ文書だけ） | O(K) ＋ 塊 1〜2 個の読み直し | 前の塊を写す `assign`、`append_cut` の `text_range`、`append_carried` が後ろの K 塊を写す |
| 改行の索引 | 0 | source ごとに共有（ADR 0047 / 0121） |

### 読む口の費用

**読む口は全部、piece の 0 番目から線形に歩く。** piece の列には累積の長さも累積の改行の数も無い。

| 口 | 費用 | 道 |
|---|---|---|
| `text_range(b, e)` / `byte_at` | **O(P_before(e))** + 読む長さ | `visit_text_range` が先頭から `absolute` を足す。止まるのは `e` を越えたときだけ |
| `line_start(l)` | **O(P)** | `newline_offset` が piece を順に歩き、改行の数を足す |
| `line_end(l)` | **O(P) × 3** | `line_start` と `newline_offset` に、CRLF なら `byte_at` が加わる |
| `line_terminator_end` | O(P) | |
| `line_text` | O(P) × 4 | |
| `position_of` | O(P) × 3 程度 | `newlines_before`・`line_start`・`byte_at` と窓の歩き。索引を持つ文書では `cursor_at` → `line_start_within` が `line_start` を log 回呼ぶ = O(P log) |
| `offset_of` | O(P) × 2 + 行の歩き | |
| `longest_line_bytes` | O(P) | 呼ぶのは `DisplayQuestion` だけ |
| `segments` | O(P) | 保存・検索の歩き・文の探索・索引の構築 |
| `line_count` / `size_bytes` | O(1) | |

### 呼び出しの数

- 読む口（`line_start` `line_end` `text_range` `position_of` `offset_of` `line_text`）の呼び出し元は、core に 38 ファイル、application に 20 ファイルある。
  - 多い所: `VisualTextView.cpp` 21・`EditorControllerView.cpp` 13・`EditorControllerPreview.cpp` 12・`EditorController.cpp` 12・`GridCells.cpp` 10・`EditorControllerVim.cpp` 10・`MatchParen.cpp` 9。
- 編集の入口は 4 か所:
  - `EditorControllerEditing.cpp:281`（replace）
  - `:682` と `:713`（undo / redo は削除した本文を**新しい add の piece として入れ直す**）
  - `VimStepPut.cpp:395 / :440`（矩形の put が行ごとに `replaced` を呼ぶ = 1 鍵で O(行数 × P)）
- 実測から逆算した歩きの回数（C2・P = 16k・1 周 ≈ 2.2 ns/piece ≈ 35 µs）:
  - **frame 1 枚 ≈ 290 回**（見えている 40 行。1 行あたり約 7 回）
  - **編集しない 1 鍵（`l` / `h`）≈ 24 回**

---

## 2. 実測（Release・一時の harness・中央値・µs）

### S1: 同じ所（本文の先頭）へ 1 文字ずつ挿入（`ix<Esc>` の形）

本文は 2,000 行 × 80 B。挿入 1 回で piece が 1 つ増える。

| P | `replaced` 先頭 | 中ほど | 末尾 | `text_range` 64 B（先頭 / 中 / 末） | `line_start` | `line_end` | `position_of` | frame 相当（40 行 × 4 口） |
|---|---|---|---|---|---|---|---|---|
| 1,000 | 30.0 | 32.9 | 32.6 | 0.9 / 1.9 / 1.7 | 1.1 | 2.1 | 3.8 | 290 |
| 4,000 | 57.4 | 65.2 | 56.2 | 0.8 / 7.2 / 7.1 | 4.7 | 9.5 | 9.4 | 1,235 |
| 16,000 | 504 | 513 | 493 | 1.0 / 31 / 30 | 22 | 43 | 43 | 5,621 |
| 64,000 | **4,315** | 4,593 | 4,260 | 0.8 / 150 / 136 | 99 | 196 | 177 | **27,340** |
| 256,000 | 測っていない | | | | | | | |

- 256,000 は、積む費用が Σ O(P) で 1 時間を超えるので測っていない。読みは線形の外挿で約 4 倍（frame 相当 ≈ 110 ms）。`replaced` は 4 倍以上（≥ 17 ms）。
- `replaced` は 16k → 64k で約 9 倍になる。列（64k × 48 B = 3 MB）がキャッシュに収まらなくなるためと読める。
- 先頭の `text_range` だけは P に依らない（すぐ止まる）。中ほどと末尾は P に比例する。

### S2: 行頭へ散らばる挿入（マクロで全行に `I#<Esc>j` の形）

本文は 200,000 行 × 40 B = 8 MB。挿入 1 回で piece が 2 つ増える。

| P | `replaced` 先頭 / 中 / 末 | `text_range` 64 B（先頭 / 中 / 末） | `line_start` | `line_end` | `position_of` | frame 相当 |
|---|---|---|---|---|---|---|
| 1,001 | 79 / 65 / 49 | 0.2 / 1.0 / 1.8 | 0.5 | 0.9 | 0.9 | 142 |
| 4,001 | 55 / 58 / 54 | 0.2 / 3.5 / 7.6 | 2.2 | 4.3 | 7.5 | 600 |
| 16,001 | 210 / 229 / 218 | 0.2 / 15 / 31 | 11 | 23 | 37 | 2,924 |
| 64,001 | **4,726 / 4,945 / 4,735** | 0.4 / 76 / 157 | 48 | 94 | 152 | **11,717** |

### controller の道（`apply(VimKeyPress)`・frame を除く）

**C1: 本文 `aaa` の行頭で `ix<Esc>` を r 周**（#1474 の本文の形・行も伸びる）

| r | 1 周 | frame 1 枚 |
|---|---|---|
| 250 | 119 | 209 |
| 1,000 | 146 | 375 |
| 4,000 | 260 | 1,117 |
| 16,000 | 521 | 3,970 |

- **#1474 の本文の 84 / 140 / 260 µs（r = 250 / 1,000 / 4,000）と合う**（4,000 で一致。250 の差は機械の混み）。
- ただしこの形は行の長さ（#1451）も同時に伸びるので、P の分だけを切り出せない。

**C2: 20,000 行 × 80 B の本文で `I#<Esc>j` を r 周**（行は短いまま・P ≈ 2r）

| r（P） | 1 周（4 鍵） | frame 1 枚 | **編集しない `l` `h` の 1 鍵** |
|---|---|---|---|
| 500（約 1k） | 321 | 913 | 81 |
| 2,000（約 4k） | 755 | 2,408 | 216 |
| 8,000（約 16k） | 2,862 | **10,103** | **844** |

- **P に線形。** 16k の piece の文書では、編集しない 1 鍵が 0.84 ms、frame が 10 ms になる。どちらも 1 打鍵の基準値（空の文書 0.64 ms）を超える。
- P = 16k の `replaced` 1 回（S1 で約 0.5 ms）より、読みのほうが大きい。**#1474 の本文は `replaced` の O(P) を穴としているが、実測では読む口の O(P) × 呼び出しの数のほうが支配的。**

### piece の増え方（S3・S4・S5）

| 形 | 編集の数 | 増えた piece |
|---|---|---|
| 打つだけ（キャレットが進む） | 100,000 | 4（`append_insertion` が畳む） |
| 5 文字打って BS 1 回を繰り返す | 120,000 | **20,003**（BS の後の打鍵は add の先端と続かないので、1 周に 1 つ） |
| 直前の文字の手前へ打つ（`ix<Esc>` 型） | 40,000 | 40,002（1 回に 1 つ） |
| 1 KiB の貼り付けを 1 万回・進むキャレット | 10,000 | 159（64 KiB の chunk ごとに 1 つ） |
| 1 KiB の貼り付けを 1 万回・各行の頭 | 10,000 | **20,000**（1 回に 2 つ・合計 5.5 秒） |
| すべて置換（10 万か所） | 1 回の `replaced` | 2（増えない） |

**すべて置換は、piece は増えないが、piece の多い本文では極端に遅い。**

| 置換の前の P | `substituted_all` | `replaced` |
|---|---|---|
| 1 | 221 ms | 1.7 ms |
| 8,001 | **6,787 ms**（約 30 倍） | 3.7 ms |

`each_line_match` が行ごとに `line_start` と `line_text` を呼ぶので、O(L × P) になる。

- undo / redo も `replaced` で本文を入れ直すので、piece が増える側に入る。

### add の chunk の数（S7）

64 KiB を末尾に入れて chunk を増やした後、先頭へ 1 文字を挿入した 1 回の時間。

| chunk の数 | 1 回 |
|---|---|
| 約 16 | 3.6 µs |
| 約 256 | 10.2 µs |
| 約 2,048 | 87 µs |

P も一緒に増えている（48 / 319 / 2,142）ので混ざっている。それでも O(C) の写しがあるのは事実。1 GiB を貼った文書では C ≈ 16k になる。

### 仮想桁の索引を持つ本文（S8・P = 32）

| 本文 | 索引 | `replaced`（中ほど） | `position_of` |
|---|---|---|---|
| 16 MiB | なし | 2.8 | 0.3 |
| 16 MiB | あり | 154 | 1.0 |
| 256 MiB | なし | 2.2 | 0.4 |
| 256 MiB | あり | 102 | 1.1 |

- 索引を持つと、触れた塊（約 64 KiB）の読み直しが 100〜150 µs かかる。K（256 MiB で 4k）の写しはその下に隠れる。P には依らない。

---

## 3. 候補の設計 3 つ

### A: 永続の平衡木（B 木の piece tree・節は `shared_ptr<const Node>`）

- **構造**: 葉に piece を 16〜64 個持つ。内側の節は子の要約（バイト数・改行の数・必要なら `LineLengths`）の配列と子への参照を持つ。高さは log_B P（P = 1M・B = 32 で 4 段）。
- **写し**: 根を 1 つ共有するだけで O(1)。
- **`replaced`**: 経路の写しで O(B log_B P)。葉の分割と併合で平衡を保つ。
- **読む口**: offset でも改行の序数でも O(log P)。`text_range` は木の歩き（iterator）。
- **索引との関係**:
  - 改行の索引: 今のまま source ごと。piece の窓も今のまま。節は改行の数の和だけを持つ。
  - 仮想桁: 節に入れられる。Tab の止まりは「最初の Tab までの幅・Tab の有無・最後の Tab の後の幅」の 3 つ組で合成できる。ただし、切った piece の要約を本文を読まずに出すには、source ごとの仮想桁の前置和が要る（今の索引は本文ごと）。格子の升は幅 `columns` に依るので、木に入れられない（今の `GridCells` のまま）。
  - **初版は索引を別のまま保つ。**
- **実装の大きさ**: 新しい型 3〜4（節・葉・カーソル・列の型）。`TextBuffer.cpp` の全部（`visit_text_range` `newline_offset` `newlines_before` `collect` `longest_line_bytes` `segments`）を書き換える。公開の口は変えない。木の不変条件（節の要約の一致・空の葉なし・平衡）を確かめる口が要る。
- **危険**:
  - 平衡（分割・併合・根の昇降）の誤りが、本文の破れとして出る。
  - 1 ファイル 1 型（CPP-011）と関数長の門で、型と関数の数が膨らむ。
  - 再帰の深さ・確保の回数（経路ごとに節 log 個の `make_shared`）。
  - `segments()` と `text_range` の iterator が木の借用を持つ。

### B: 塊の列（2 段の B 木・ADR 0180 の履歴と同じ形）

- **構造**: piece を最大 2B 個（B = 256 前後）の塊 `PieceBlock`（`vector<Piece>` と塊の中の累積の長さ・改行の数）に分ける。塊の列 `vector<shared_ptr<const PieceBlock>>` と、塊ごとの累積（バイト数・改行の数の 2 本の `size_t` 列）を、1 つの `shared_ptr<const>` の束で持つ。
- **写し**: O(1)。
- **`replaced`**: O(P/B + B)。
  - 塊の列を写す（参照数の加算が P/B 回）と、累積の 2 列を写す（`size_t` が 2P/B 個）。
  - 当たった 1〜2 塊だけを作り直す（≤ 2B piece）。
  - 塊が 2B を超えたら半分に割り、B/2 を下回ったら隣と合わせる。
  - P = 256k・B = 256 なら、1,000 参照と 16 KB の写しと 256 piece。見積りは数 µs〜20 µs。
- **読む口**: O(log(P/B) + log B)。塊の累積を二分探索し、塊の中の累積をまた二分探索する。`visit_text_range` は当たった piece から歩き始める。
- **索引との関係**:
  - 改行の索引: 今のまま。塊は改行の数の和を持つ。
  - 仮想桁の索引: 別のまま。同じ塊の列の形は、仮想桁の索引の `replaced` の O(K) の写しにも当てられる（後続）。
  - add の列: 同じ考え方で O(C) の写しを消せる。chunk が増えるときだけ写す。
- **実装の大きさ**: 新しい型 2（塊・束。束は列の型）。`TextBuffer.cpp` の歩きの 5 関数と `collect` / 構築子を書き換える。公開の口は変えない。
  - 塊の分割と併合は 1 段だけなので、A の平衡より単純。
  - 不変条件（塊の要約と累積の一致・空の塊なし）を確かめる口は、試験だけが読む。
- **危険**:
  - P/B の写しは残る。P が百万を超える文書では 1 回 ~0.1 ms 程度（P = 1M・B = 256 で 4k の参照）。上限は B を上げれば下がる。B を増やしたくなったら 3 段目＝A へ育てる。
  - 塊の境の piece（`append_insertion` が直前の塊の末尾に続くか）を正しく扱う。

### C: 隣り合う piece の併合と、編集の位置の近くの覚え（今の列のまま定数を下げる）

- **構造**: 列は今のまま。
- **併合**: `replaced` のときに、隣り合う短い add の piece（例えば合わせて 64 B 以下）の本文を、add の先端に写し直して 1 つにする。
  - `ix<Esc>` 型と「BS の後に打つ」型の増え方を、およそ 1/64 に抑えられる。
  - original を割る散らばった編集（S2・矩形・各行への貼り付け）は畳めない。piece は本当に別。
- **位置の覚え**: 値ごとに「最後に読んだ piece の番号と、その始まりの offset と改行の数」を覚える。不変の値に可変の覚えを持つことになるので、`mutable` か、呼び出し側が持つカーソルになる。これで近くの読みを O(1) にする。
- **索引との関係**: 変わらない。
- **実装の大きさ**: 小さい（`TextBuffer.cpp` の 2〜3 関数）。
- **危険**:
  - **`replaced` の O(P) は残る。**
  - 遠い読み（frame の先頭・末尾・`substituted_all` の全行）は速くならない。
  - 併合は add の領域を余分に使う。add は回収しないので、本文が増える。
  - `mutable` の覚えは「値は不変」の意味と ARC-005 に逆らう。

### 比較

| | A 木 | B 塊の列 | C 併合と覚え |
|---|---|---|---|
| 写し | O(1) | O(1) | O(1) |
| `replaced` | O(B log P) | O(P/B + B) | **O(P)** |
| 読む口 | O(log P) | O(log P) | 近い所 O(1)・遠い所 O(P) |
| frame（40 行 × 7 口） | log | log | P のまま（近ければ速い） |
| すべて置換（O(L × 読み)） | L log P | L log P | L × P |
| 散らばった編集（S2）で増える P | 防がない（費用は log） | 防がない（費用は小） | 防がない |
| 実装の大きさ | 大 | 中 | 小 |
| 危険 | 平衡の誤り | 塊の境 | 根の穴が残る |
| 他の索引への流用 | 仮想桁の要約まで寄せられる | 仮想桁の索引と add の列に同じ形 | なし |

### 既存の実装の要点（一次情報）

- **VS Code の piece tree**（[VS Code の blog 2018-03](https://code.visualstudio.com/blogs/2018/03/23/text-buffer-reimplementation)）
  - 赤黒木。節は「左の部分木の長さと改行の数」を持つ。
  - バッファは読み専用と追記専用。改行の位置はバッファごとに 1 回だけ作り、節は窓だけを持つ。Nib の ADR 0047 と同じ考え方で、記事は約 3 倍速くなったとしている。
  - 編集も行の検索も O(log N)。編集で節が増えると読みが落ちる点を弱点に挙げ、正規化も検討していた。
  - **木は可変で、永続ではない**（記事に snapshot の話は無い）。Nib の不変の値には、そのまま使えない。
- **Zed の SumTree / Rope**（[Zed の blog](https://zed.dev/blog/zed-decoded-rope-sumtree)）
  - B+ 木。葉は最大 128 文字の chunk を内に持つ。節の要約は和（len・len_utf16・行の Point・最長の行）。
  - `Arc` の節で永続・copy-on-write。snapshot は参照数を上げるだけ。
  - 次元ごとの seek が O(log N)。**A の形の実例。**
- **Ropey**（[設計の文書](https://github.com/cessen/ropey/blob/master/design/design.md)）
  - B 木。節は `Arc` で、`make_mut` で共有されているときだけ写す。
  - 節はバイト数・文字数・改行の数を持つ。CRLF を葉の境で割らない規則がある。
- **xi**（[rope science 1](https://xi-editor.io/docs/rope_science_01.html)）
  - 節の要約はモノイドで、編集は経路だけ要約を作り直して O(log n)。
  - 最長の行の要約は「最初の改行まで・最長・最後の改行の後」の 3 つ組。Nib の `LineLengths` の `joined` と同じ形。
- **Kakoune**（[buffer.hh](https://github.com/mawww/kakoune/blob/master/src/buffer.hh)）
  - 行の共有文字列の vector（`Vector<StringDataPtr>`）。履歴は変更の列。
  - 行の列は O(L) の写しで、Nib の今の形に近い。

### Nib の制約への当てはまり

| 制約 | A | B | C |
|---|---|---|---|
| 不変の値 | ◎ Zed・Ropey が実例 | ◎ ADR 0180 の履歴が実例 | △ 覚えが `mutable` |
| `std::atomic` 不可（CNF-009） | `shared_ptr` だけで足りる（ADR 0180 の決定 4 と同じ） | 同じ | 同じ |
| スレッド 1 本 | 問題なし | 問題なし | 問題なし |
| 16 MiB〜1 GiB | 問題なし | 問題なし | 問題なし |
| 巨大な 1 行 | P には関わらない | 同じ | 同じ |

- 文書の大きさ: original は 1 piece で始まるので、P は編集の回数だけで決まる。
- 巨大な 1 行: P には関わらない。行の長さの分は #1451 と仮想桁の索引の領分。

---

## 4. おすすめと段

### おすすめ: B（塊の列）

理由:

1. 実測では、支配的なのは**読む口**の O(P) × 呼び出しの数（frame ≈ 290 回・1 鍵 ≈ 24 回）。B は `replaced` と読みの両方を、ほぼ log にする。
2. 形が ADR 0180 の履歴（塊の `shared_ptr<const>` の列）と ADR 0044 と同じで、規約と試験の考え方をそのまま使える。
3. 平衡は 1 段の分割と併合だけ。A より危険が小さい。
4. 公開の口を変えない。束の型を `TextBuffer` の内側に閉じれば、後で 3 段目（= A）へ育てても `TextBuffer.cpp` の中だけで済む。

C の併合は、B と直交する小さな追加として後に回す（増える P の定数を下げる）。

### 段（各段で振る舞いは不変・性質の試験が守る）

**段 0（試験と probe だけ・製品は不変）**
- probe を足す（ゲートには載せない・比で読む）:
  - `piece-heavy-{1000,4000,16000}-*`: S1 / S2 の形の `replaced` と、読む口（中ほどの `text_range`・`line_start`・`line_end`・`position_of`）
  - C2 の形: 散らばった N 回の編集の後の、frame 1 枚と `l` `h` の 1 鍵
  - `substituted_all` を P = 1 と P = 8k で
- `--text-buffer-property` に、P が数千の case を足す（固定 seed）:
  - 塊の大きさの 2 倍を超える P
  - 先頭への挿入の連続
  - 散らばった挿入
  - 削除で塊が空になる列
  - 古い値の分岐（aliasing）を 97 歩ごとに残して、最後に照合する
- 今の網は `small_cases = 12 × 20 歩` で、P は数十までしか届かない。**塊の境を踏む網が無い。**

**段 1（読む口を二分探索に・`replaced` は O(P) のまま）**
- piece の束に、piece ごとの累積のバイト数と改行の数を持たせる。今は塊 1 つの B と見なせる。
- `visit_text_range` `newline_offset` `newlines_before` `byte_at` を、当たった piece から始める。
- 構築子の `total_*` の 2 周は、累積の最後の値で済む。
- 期待: C2 の frame（P = 16k で 10 ms）と `l` `h`（0.84 ms）が、P に依らなくなる。すべて置換が O(L log P) になる。
- 小さく入り、効果が最も大きい段。

**段 2（塊の列）**
- 束を「塊の列 ＋ 塊ごとの累積」にする（B = 256 前後・定数は 1 か所）。
- `replaced` は当たった塊だけを作り直す。分割と併合の規則を 1 本にする（ADR 0180 の `kept_with` と同じ考え方）。
- 期待: S1 / S2 の `replaced`（P = 64k で 4.3〜4.9 ms）が、P = 1k の値（30〜80 µs）の近くで平らになる。

**段 3（同じ形を広げる・必要なら）**
- add の列（O(C) の写し）を共有の束にする。chunk が増えるときだけ写す。
- 仮想桁の索引の O(K) の写しにも、同じ塊の列を当てる。
- 矩形の put の行ごとの `replaced`（VimStepPut）を 1 回の多箇所の編集にするかは、別に判断する。

**段 4（任意）**
- C の併合（隣り合う短い add の piece を写し直して 1 つにする）。
- 上限と、add の余分に使う量は、設計席が決める。

### 測るもの

**最初の条件**: 1 打鍵の基準値を悪くしないこと。
- 空の文書: 0.64 ms
- 16 MiB で 200 打鍵: 2.34 ms

- 各段は、正式な速さのゲート（QLT-014）の 8 本を 0 regression で通す。
- 特に段 1 と段 2 は、P が小さい文書での定数（二分探索と塊の間接参照の分）が増える。次の 3 本を静かな機械で確かめる:
  - `key-to-frame-burst-200`
  - 16 MiB の 200 打鍵
  - 長い行の 1 打鍵
- P の分は段 0 の probe で読む。N を 4 倍にしたときの比が 1 に近いこと。
- 試験の網:
  - 既存: `--text-buffer-property` `--editing-property` `--edit-history-property`・`NewlineIndexTests`・`VirtualColumnIndexTests`・fixture 全件
  - 段 0 で足す網: 大きな P・塊の境・aliasing

---

## 5. 設計席が決める点

1. **#1474 の範囲を「読む口」まで広げるか。** 本文の数字は `replaced` の O(P) だが、実測で大きいのは読みの O(P) × 回数。段 1 を先に入れるか。
2. 塊の大きさ B と、分割・併合のしきい値。定数を 1 か所に置くか。
3. 仮想桁と `LineLengths` を、束の要約に寄せるか（A の方向）、別のまま保つか（初版は別のままを推す）。
4. add の列の O(C) と、仮想桁の索引の O(K) を同じ Issue で扱うか、別の Issue にするか。
5. C の併合で add を余分に使ってよいか（add は回収しない・ADR 0044 の決定 5）。
6. すべて置換の O(L × P) は段 1 で解ける。#1474 の受け入れに `substituted_all` の probe を入れるか。

## 6. 危険

- 段 1 と段 2 で、P の小さい文書の 1 打鍵に定数が足される。累積の配列を作る費用と、二分探索の分。基準値が狭いので、静かな機械で確かめる。
- `append_insertion` が直前の piece を伸ばす道（ADR 0044 の決定 3）が、塊の境と累積の更新に絡む。古い値の分岐（先端の規則）を性質の試験で踏む必要がある。
- move した後の値は、束が null になる（ADR 0180 の L3 と同じ）。
- 混んだ機械での測定なので、P = 1k の絶対値は揺れている（S2 の 77 µs と S1 の 14〜30 µs）。結論は P を 4 倍にしたときの比（線形）だけを使う。

---

## 段 1 の probe（#1474・ADR 0183・2026-11-12）

- 道具: `nib_perf_probes`（Release・窓を出さない）。直す前は段 0 の commit `af3ec8f0`（probe と試験だけを足した木）、直した後は段 1 の木。2 つを workload ごとに交互に（周ごとに順を入れ替えて）3 周回し、各周の中央値の 3 周の中央値を比べる。走らせた道具は `D:/NeNeNib/scripts/20261112/1474/run_probes.py`、ログは作業木の `out/1474-impl/probes-v2.log`。
- **機械は混んでいた**（周の始めの CPU 負荷 90〜100%）。絶対値は基準値と比べず、比で読む。
- 区間の中身は `tests/performance/PieceListWorkload.hpp` の注釈のとおり（read は 40 行 × 6 口 × 50 周・replace は列を伸ばさない `replaced` 200 回・replace-all は 10 万か所 1 回・frame は `l` `h` 100 組で鍵ごとに frame）。

| probe | 直す前（µs） | 直した後（µs） | 後 / 前 |
|---|---|---|---|
| `piece-read-after-1-pieces` | 1,062 | 1,072 | 1.01 |
| `piece-read-after-1000-pieces` | 32,840 | 1,474 | 0.045 |
| `piece-read-after-4000-pieces` | 85,598 | 1,778 | 0.021 |
| `piece-read-after-16000-pieces` | 484,880 | 2,290 | 0.005 |
| `piece-replace-after-1-pieces` | 763 | 750 | 0.98 |
| `piece-replace-after-1000-pieces` | 3,279 | 5,001 | **1.53** |
| `piece-replace-after-4000-pieces` | 14,143 | 7,810 | 0.55 |
| `piece-replace-after-16000-pieces` | 62,836 | 29,303 | 0.47 |
| `replace-all-100000-after-1-pieces` | 131,504 | 131,910 | 1.00 |
| `replace-all-100000-after-8000-pieces` | 6,549,306 | 162,012 | 0.025 |
| `frame-after-1-pieces` | 43,160 | 46,350 | 1.07（周ごとは 0.95 / 1.07 / 1.01） |
| `frame-after-16000-pieces` | 2,298,305 | 64,590 | 0.028 |

- 読む口は P = 1k / 4k / 16k で 22 / 48 / 212 倍速くなり、P にほぼ依らなくなった（1.5 → 2.3 ms）。frame（P = 16k）は 1 鍵 ≈ 11.5 ms → 0.32 ms、すべて置換（P = 8k）は 6.5 s → 0.16 s。
- P = 1 の読み・`replaced`・すべて置換・frame は比 0.98〜1.07 で、混んだ機械の揺れの中（静かな機械の 8 本のゲートは設計席が測る）。
- **P = 1k の `replaced` だけが 1.5 倍に遅くなった**（3 周とも: 3.3 / 3.7 / 3.2 ms → 5.3 / 5.0 / 4.8 ms・200 回ぶん）。P = 4k / 16k では速くなる（列を作る周が累積を埋め、合計の 2 周と再確保の写しが無くなった）。独立レビュー V-1562 が測り直した: 続く遅れではなく、probe の process の最初の数回だけ遅い暖まりの費用（heap の大きな塊が落ち着くまで）。5 周交互に回すと中央値は 1.75 倍だが最小値は 0.96 倍で、新しい版の 9 回の値は 10.8 ms から 1.6 ms へ下がっていく（前の版は最初から平ら）。1 鍵あたりは落ち着いた後で約 0 µs・暖まり中の最悪で +45 µs、読む口は約 −60 µs / 鍵なので正味でも速い。probe は暖まりの回を捨てるか最小値も記録する形に直す（後続）。`replaced` の O(P) そのものは段 2 の範囲。
- 最初の実装（構築子が別の周で累積を埋め、`std::ranges::upper_bound` で引く形）は P = 16k の `replaced` が 2.4 倍に遅くなった（`out/1474-impl/probes-v1.log`）。列の末尾に置く口 `placed` が列を作る周の中で埋め、`next.reserve` で再確保を無くし、二分探索を手で書いた今の形に直した。
