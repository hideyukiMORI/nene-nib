# ADR 0047 — 改行の索引はバッファ（original と各 add chunk）ごとに 1 本を共有し、piece はその窓だけを持つ

- 状態: 受理（設計席 2026-09-23・Issue #184。hide 未確認）
- 日付: 2026-09-23
- Issue: #184
- 影響する規則: ARC-003 / ARC-007 / CPP-001 / CPP-011 / QLT-001 / QLT-012 / QLT-014
- 前提: [ADR 0009](0009-editing-slice-piece-table-and-editing-states.md)（piece table）・[ADR 0044](0044-add-buffer-in-chunks.md)（add は chunk の列・piece は chunk を跨がない）・[ADR 0036](0036-line-ending-owned-by-text-buffer.md)（`'\n'` だけを数えれば CRLF も狂わない）・#179 のベンチ `key-to-frame-burst-200-16mib`

## 文脈

#179 の 6 本目のベンチ（16 MiB・200,000 行を開いてから 200 打鍵）は 447 ms（1 鍵 2.2 ms・空の文書の 0.9 ms の 2.4 倍）で、#174（add の chunk 化）の前後で変わらない。Sonnet の probe（scratchpad `probe-keycost.md`）と設計席の読み: `Piece::newlines` は piece の先頭からの相対位置を持つ値型の `std::vector<Offset>`（開いた直後の original の piece は 200,000 個・約 1.6 MiB）で、`TextBuffer::clipped`（`src/core/TextBuffer.cpp:166-175`）は piece をキャレットで割るたびに **切った側の本文を `newlines_in` で走査し直して索引を作り直す**。`replaced` は prefix と suffix で `collect` を 2 回、controller の `replace` は `erase().insert()` で `replaced` を 2 回呼ぶので、1 打鍵で 16 MiB を最大 2 回なめ、索引を最大 4 回作り直す（memchr 相当で 16 MiB ≈ 1 ms・実測と合う）。行数・行の先頭・行番号の問い（`line_count` / `newline_offset` / `newlines_before`）は O(P) か二分探索で、本文全体には触れていない。

## 決定

**改行の索引（`'\n'` のバイト位置の昇順の列）はバッファごとに 1 本: original は `from_utf8` で 1 回作って共有し、add の各 chunk は追記のたびに末尾へ伸ばす（追記専用・前の要素は動かない）。piece は自分の範囲 `[start, start + length)` に入る索引の窓 `[newline_begin, newline_end)` だけを持つ。piece を切る・伸ばす・数えるはすべて二分探索か定数時間で、本文を走査し直さない。**

1. **索引の置き場**: original の索引は `TextBuffer` が `std::shared_ptr<const std::vector<Offset>>`（`original_newlines_`）で共有する（`from_utf8` の `newlines_in(text)` 1 回だけ）。add の索引は `AddChunk` が `std::vector<Offset> newlines`（chunk の中のバイト位置・昇順・追記専用）として持ち、`bytes` を伸ばすとき（ADR 0044 決定 2 の「先端を持つ値だけがその場で伸ばす」と同じ条件）に伸びた分の `'\n'` を末尾へ足す。古い値の piece は自分の `newline_end` までしか読まないので、後から伸びた分は見えない（ADR 0044 と同じ規則）。
2. **piece の欄**: `Piece { source; chunk; start; length; newline_begin; newline_end; }`。`newlines` の `vector` は持たない。`newline_end - newline_begin` がその piece の改行数。索引の本体は `TextBuffer` が `source` と `chunk` から引く（`index_of(piece)` 1 本）。
3. **切る**: `clipped(piece, from, length)` は `index_of(piece)` を `[start + from, start + from + length)` で `lower_bound` 2 回して窓を求める（O(log L)）。本文は読まない。
4. **伸ばす**: `append_insertion` が末尾の add piece を同じ chunk の中で伸ばすときは、chunk の索引に足した分だけ `newline_end` を進める（`newlines_in(text)` は足した `text` だけを走査する・O(c)）。
5. **数える**: `line_count` は piece の改行数の和（今と同じ O(P)・作るときに 1 回）、`newline_offset(index)` と `newlines_before(at)` は piece を順に見て窓の中を `lower_bound`（今と同じ O(P + log L)）。`newlines_in` の結果の意味（`'\n'` の位置・CRLF は `'\n'` だけ・ADR 0036）は変えない。
6. **不変**: 本文の値・行番号・fixture 1359 件・checks 数・保存 schema・`view_of` は変わらない。#174 の `add_fill_` の規則もそのまま（索引は同じ「先端」の条件で伸びる）。
7. **速さ**: `key-to-frame-burst-200-16mib` が下がることを実機で確かめ（期待は空の文書の burst に近い 1 桁 ms 台〜数十 ms）、下がったら設計席が `--adopt --bench key-to-frame-burst-200-16mib` で基準値を締める（ADR 0016 の手順・保護対象の変更の根拠は本 ADR）。他の 5 本は不変。

## 強制

- fixture・checks 数の不変: **active**（`python eng/protected-diff.py --base origin/main --build`・`--allow` 無し）。
- 本文を走査し直さないこと: **active**（`key-to-frame-burst-200-16mib` の基準値を締めた後は QLT-014 のゲートが守る）。それまでは契約: 16 MiB 相当の本文（200,000 行）を `from_utf8` してから 200 回 `insert` しても `line_count` と行の先頭が正しい（正しさだけ・時間は core で測れない・ARC-007）。
- symbols: **active**（今と同じ）。

## 結果

得られるもの: 大きな本文での 1 打鍵が本文の大きさに依らなくなる。索引の複製が消える（開いた直後の 1.6 MiB × 最大 4 回 / 打鍵）。
失うもの・残る穴: `AddChunk` が可変の索引を持つ（`bytes` と同じ「先端」の規則で守る）。`Piece` の欄が 2 つ増え `vector` が 1 つ減る。1 GB と original の遅延構築は ADR 0004 のまま。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `Piece::newlines` を `shared_ptr<const vector>` にして切るときだけ複製 | 切るのは毎打鍵（キャレットの piece）で、複製が O(L) のまま。窓なら O(log L) |
| 索引を持たず必要なときに走査する | `line_count` と行番号の問いが本文全体の走査になり、描画のたびに 16 MiB をなめる |
| 行ごとの木（rope / B-tree の行索引） | piece table の上にもう 1 層。ADR 0009 の「編集は piece の列だけ」を崩す。窓つき索引で足りる |
