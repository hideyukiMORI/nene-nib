# ADR 0044 — piece table の add バッファは固定長の chunk の列で持ち、piece は chunk を跨がず、先端を持つ値だけがその場で伸ばす

- 状態: 受理（設計席 2026-09-23・Issue #174。hide 未確認・引き継ぎの「次の順」の 1 番目）
- 日付: 2026-09-23
- Issue: #174
- 影響する規則: ARC-003 / ARC-007 / CPP-001 / CPP-011 / QLT-001 / QLT-012 / QLT-014
- 前提: [ADR 0009](0009-editing-slice-piece-table-and-editing-states.md)（piece table・original と add の 2 本）・[ADR 0011](0011-speed-measurement-timing-port-and-paint-coalescing.md)（速さのゲート）・[ADR 0016](0016-ci-speed-reference-per-host-fingerprint.md)（機械ごとの基準値）・[ADR 0036](0036-line-ending-owned-by-text-buffer.md)

## 文脈

`TextBuffer` は値型で、`original`（読んだ本文）と `add`（入力）を `shared_ptr<const std::string>` で共有し、編集は piece の列だけを作り直す（ADR 0009）。ところが `TextBuffer::replaced` は非空の挿入のたびに `*add + std::string(text)` で **add の全バイトを新しい文字列へ複製**してから足す（`src/core/TextBuffer.cpp:159-163`）。1 打鍵・貼り付け・IME の確定・undo / redo（`erase` → `insert` で add に再追記）がすべてこの 1 本を通るので、連続 K 打鍵の複製量は O(K²)、add は縮まず単調に増える（Sonnet の probe・scratchpad `probe-addbuffer.md`）。

速さのゲートはこれを見ていない。`key-to-frame-single`（0.9 ms）と `key-to-frame-burst-200` は **空の文書**への打鍵で add がほぼ空、`open-large-file-16mib` は開いて最初の frame まで。引き継ぎ 09-23 と CLAUDE.md 6 節の「16 MiB の 1 打鍵は 0.9 ms」は測っていない値で、本 ADR で言い方を正す（実測の値は「空の文書で 1 打鍵 0.9 ms」）。

1 GB は `maximum_file_bytes = 64 MiB` で開けず、`original` の遅延構築は ADR 0004 の別課題。本 ADR は add だけを扱う。

## 決定

**add は固定長 chunk（64 KiB）の列。1 つの piece は 1 つの chunk の中だけを指し、`view_of` は今までどおり 1 本の `string_view` を返す。末尾の chunk への追記は「その chunk の先端（書き込み済みの長さ）を自分の値が持っているときだけその場で伸ばし、持っていなければ新しい chunk を始める」。複製は起きない。**

1. **型**: `src/core/AddChunk.hpp` に `struct AddChunk { std::string bytes; }`（1 ファイル 1 型・`bytes` は作るときに `reserve(chunk_bytes)` する）。`TextBuffer` は `std::vector<std::shared_ptr<AddChunk>> add_` と `std::size_t add_fill_`（末尾 chunk の、自分の値が知っている書き込み済みの長さ）を持つ。`Piece` は `source == add` のとき `chunk`（`std::size_t`・chunk の番号）を持ち、`start` はその chunk の中のバイト位置のまま（`Offset` の意味は「そのバッファの中の位置」で変わらない。original は `chunk` を使わない）。
2. **追記の規則**（`replaced` の非空の分岐 1 か所）: `text` が `chunk_bytes` 以下で末尾 chunk に `bytes.size() == add_fill_` かつ残り容量が足りるなら、`bytes.append(text)` でその場に足し、`add_fill_` を進める。そうでなければ `std::max(chunk_bytes, text.size())` で新しい chunk を作って `text` を入れ、列の末尾に足す（`text` が chunk より大きいときはその 1 本だけの chunk）。**`bytes.size() != add_fill_` は「別の値が先に伸ばした」しるし**で、そのときは決して上書きせず新しい chunk を始める（値型の意味論を守る。古い値は自分の `add_fill_` までしか読まないので、後から伸びた分は見えない）。`reserve` した容量を超えて伸ばさないので、`string_view` は chunk が生きている間ずっと有効。
3. **piece の結合**: `append_insertion` の「直前の piece の続きなら伸ばす」（`continues`）は、同じ chunk の中で続くときだけ。chunk が変わったら新しい piece（piece 数は 64 KiB ごとに 1 つ増えるだけ）。
4. **読み**: `view_of` は `add_[piece.chunk]->bytes` の `[start, start + length)`。`text` / `text_range` / `line_text` / `line_end` は piece ごとの `string_view` を今までどおり繋ぐ（呼び出し側は変えない）。
5. **不変**: 本文の値・行索引・fixture 1339 件・`nib_tests` の checks 数・保存 schema は変わらない。`erase` は add に触れない（今と同じ）。add の回収（消した文字の領域を返す）は行わない（undo が差分を持つだけなので、消えた領域は別の値から参照されうる。回収は別 ADR）。
6. **速さの基準**（後続の Issue）: `eng/measure-speed.py` に `key-to-frame-burst-200-16mib`（16 MiB を開いてから 200 打鍵・`bench_large_file` の後に `bench_keys` の burst と同じ測り方）を足し、`eng/perf-reference.json` に実機と CI の指紋の基準値を記録する（ADR 0016 の手順・保護対象の変更なので別 PR）。既存 4 本の基準値は変えない。
7. **ARC-003 / 007**: 時刻・スレッド・OS には触れない。chunk の大きさは `constexpr`。

## 強制

- fixture 1339 件・`nib_tests` の総数・scope ごとの checks 数の不変: **active**（`python eng/protected-diff.py --base origin/main --build`・`--allow` 無し）。
- 追記がその場で行われ複製が起きないこと: **planned**（core は計時できない・ARC-007。決定 6 のベンチが入るまではレビュー事項。契約として「同じ値から 2 回分岐して挿入しても互いの本文が壊れない」（決定 2 の `add_fill_` の規則）を `--text-buffer` の unit に足す: `a = base.insert(0,"x")`・`b = base.insert(0,"y")` で `a.text()` と `b.text()` がそれぞれ `x…` `y…`）。
- symbols（core が libc の allocator 以外を出さない）: **active**（`eng/symbols.py`・今と同じ）。

## 結果

得られるもの: 連続入力の複製が O(K²) から O(K) に落ち、undo / redo も複製しない。値型の意味論・`view_of`・fixture は不変。
失うもの・残る穴: add は消した領域を回収しない（今と同じ）。`Piece` が 1 欄増える（8 バイト）。chunk 境界で piece が 1 つ増える。1 GB と original の遅延構築は ADR 0004 のまま。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 挿入 1 回ごとに chunk を 1 本作る（先端の追記をしない） | 1 打鍵ごとに piece と割り当てが 1 つ増え、piece の走査が O(K) に戻る |
| 不変の連結リスト（追記チェーン） | 位置から chunk を引く索引が要り、結局 chunk の列になる |
| `std::rope` 相当の木 | piece table の上にもう 1 層。ADR 0009 の「編集は piece の列だけ」を崩す |
| add を `std::string` のまま `shared_ptr<std::string>` で共有し、その場で `append` | 古い値が同じ文字列を指したまま、別の値の追記が容量超えで再確保すると `string_view` が壊れる。先端の規則と `reserve` が要るので chunk にする |
