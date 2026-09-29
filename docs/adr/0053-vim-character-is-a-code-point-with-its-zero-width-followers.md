# ADR 0053 — Vim の 1 文字はコードポイント 1 つと直後に続く幅 0 のコードポイントの列で、判定は仮想桁の表を使う

- 状態: 受理（設計席 2026-09-29・Issue #216。hide 未確認）
- 日付: 2026-09-29
- Issue: #216
- 影響する規則: FR-003 / ARC-001 / ARC-012 / CPP-007 / CPP-011 / QLT-001 / QLT-012 / QLT-014 / CNF-010 / CNF-011
- 前提: [ADR 0034](0034-vim-virtual-column-one-table.md)（仮想桁と文字の幅の表・固定 Vim の実測値）・[ADR 0012](0012-vim-engine-first-slice-and-oracle-fixtures.md)（fixture は oracle）・[ADR 0040](0040-display-line-with-control-glyphs.md)（幅 0 と制御文字の描画）・[ADR 0052](0052-undo-caret-restores-the-first-edit-position.md)（キャレットの寄せは 1 か所）

## 文脈

Vim は結合文字（`e` + U+0301 など）を前の文字と合わせて 1 文字として扱う。Nib の Vim は `h` `l` `x` `X` `dh` が共有する `forward_characters` / `backward_characters`（`src/core/VimStep.cpp`）がコードポイント単位で進むので、結合文字の上にキャレットが立ち、`X` が結合文字だけを消す（Issue #206 の実装席が fixture で見つけた）。

本物の Vim 9.1 の実測（Sonnet の probe `out/probes/probe-combining-2026-09-29.md`・`encoding=utf-8`・U+0080〜U+10FFFF を Vim 自身で全域走査）:

- 「前の文字に含める」条件は 1 つ: **直後のコードポイントが composing であること**。集合は 1998 コードポイントで、Unicode の Mn と Me にほぼ一致する（Mc と Cf は含まない）。個数の上限は見えない（7〜8 個でも全部 1 文字。`maxcombine` の表示は 2）。
- 含める側: `e` + U+0301・`e` + U+0301 + U+0323・`か` + U+3099・`は` + U+309A・異体字セレクタ（`葛` + U+E0100・`☺` + U+FE0F）・タイ語の `ก` + U+0E49。
- 別の文字の側: ZWJ の連なり（ZWJ は幅 6 の `<200d>`・絵文字は各 1 文字）・肌の色の修飾・国旗（regional indicator 2 つ）・ハングルの字母の連なり・`ก` + U+0E33・ZWSP。
- 基底になる文字に条件は無い。Tab・空白・制御文字にも付き、行頭の孤立した結合文字はそれ自身が文字の先頭になる。
- `l` は結合文字の上に止まらず次の文字へ進む。`x` `X` `dl` `dh` は文字の全体を消す。`r` は結合文字ごと置き換える。`a` は文字の全体の後ろに入る。`$` と NORMAL のキャレットは基底のバイト桁に立つ。`j` `k` は結合文字の上に落ちない。VISUAL の選択と削除は文字の全体。`f` `t` の比較は文字の先頭のコードポイントだけ（`fe` は `e` + U+0301 に当たる）。

Nib の今の形: 文字の幅は `display_width(code_point)`（`src/core/DisplayWidth.hpp` と `DisplayWidthRange.hpp`・492 範囲・固定 Vim の `strdisplaywidth` の全域実測）。**`DisplayWidth::zero` は 1998 コードポイントで、Vim の composing 集合と完全に一致する**（probe で突き合わせ）。コードポイント単位で進む関数は `next_code_point` / `previous_code_point`（`src/core/Utf8.cpp`・呼び出しは core 57・application 1・ui 2）と、それを使う `forward_characters` / `backward_characters`（`VimStep.cpp` の中で 16 か所）。`previous_code_point` を直接呼ぶ所が `VimStep` `VimCaret` `VimTextObjectRange` `VimWordMotion` `VimPattern` にある。描画は DirectWrite のクラスタ整形に任せ、クリックはクラスタの境に寄る。通常モードの矢印・Backspace・Delete はコードポイント単位。

## 決定

**Vim モードの「1 文字」は、コードポイント 1 つと、その直後に続く幅 0（`DisplayWidth::zero`）のコードポイントの列である。判定は仮想桁の表の 1 つだけを使い、文字を歩く関数は core の 1 対にする。Vim の NORMAL と VISUAL のキャレットは文字の途中に立たない。**

1. **定義**: 文字の先頭はどのコードポイントでもよい（行頭の孤立した結合文字はそれ自身が文字の先頭）。先頭の後ろに続く `display_width(cp) == DisplayWidth::zero` のコードポイントは、個数の上限なく同じ文字に含める。ZWJ の連なり・肌の色・国旗・ハングルの字母は Vim と同じく別々の文字で、書記素クラスタにはしない。異体字セレクタは幅 0 なので含まれる。
2. **表は 1 つ**: 判定は `display_width` の表（ADR 0034）。結合文字のための表や関数の 2 つ目を作らない（ARC-001）。表を作り直すとき（固定 Vim の版が変わるとき）は文字の歩き方も一緒に変わる。
3. **歩く関数は core の 1 対**（`src/core/VimCharacterBoundary.hpp` / `.cpp`）: `vim_character_end(std::string_view utf8, Offset at) → Offset`（`at` から始まる文字の終わり）と `vim_character_start(std::string_view utf8, Offset at) → Offset`（`at` の手前で終わる文字の先頭。`at` が文字の途中ならその文字の先頭）。`next_code_point` / `previous_code_point` は変えない（UTF-16 との変換・表示・照合器が使う）。
4. **使う所**: `forward_characters` / `backward_characters` がこの 1 対で進む（`h` `l` `x` `X` `dl` `dh` `<Space>` `<BS>` と回数）。`r` は文字の全体を置き換える。`a` は文字の全体の後ろへ入る。VISUAL の選択の端は文字の全体を含む。語の移動 `w` `e` `b`・テキストオブジェクト・`f` `t`（比較は文字の先頭のコードポイントだけ）は、fixture が落ちた所を同じ 1 対に置き換える。`previous_code_point` / `next_code_point` の直呼びのうち「文字を 1 つ歩く」意味の所だけを置き換え、バイト列の走査（検索の照合器・UTF-16 との変換・表示）は変えない。
5. **キャレットの不変条件**: Vim の NORMAL と VISUAL のキャレットは文字の先頭にだけ立つ。`$` と行末の寄せは行の最後の文字の先頭（基底）。`j` `k` `H` `M` `L` の欲しい列からの着地・検索の着地・`u` と Ctrl-r の戻り先が文字の途中になったら、その文字の先頭へ寄せる。寄せは NORMAL のキャレットの既存の寄せの 1 か所に足す（寄せの規則を 2 つにしない・ADR 0052）。
6. **INSERT**: `i` `a` `I` `A` の挿入位置は文字の境。INSERT の `<BS>` は Vim の既定（`nodelcombine`）に合わせる。probe で測っていないので、fixture を先に生成して Vim の結果を見てから直す。直す所が INSERT の 1 か所で済まないなら、その形を報告して本 Issue から外す。
7. **fixture**: 名前は `combining-*`。本文は UTF-8 で直接書く（既存の `virtcol-combining-zero-*` と同じ）。probe の A1〜A11 の各系統から 40 件程度: `h` `l` の 1 歩と回数・`x` `X` `3x`・`dl` `dh` `yl` `d2l`・`rz`・`aZ<Esc>` `iZ<Esc>`・`$` `0`・`j` `k`・`vld` `vly`・`w` `e` `b`（分解形の `café au lait`）・`fe` `te`・`diw`・`.`・`u`。文字は `e` + U+0301・結合文字 2 つ・`か` + U+3099（全角の基底）・異体字セレクタ・行頭の孤立した結合文字（歩き方だけ）と、別の文字の側の逆例（ZWJ の連なり・国旗・肌の色・ハングルの字母）。#206 から外した `$hX` を足す。
8. **範囲の外（別 Issue）**: 通常モードの矢印・Backspace・Delete の単位（Windows の慣習とクリックの寄り方に合わせて別に決める）・行頭の孤立した結合文字の幅を Vim と同じ 1 にすること（仮想桁の表と描画の両方に効く・今は 0）・`f` `t` に結合文字そのものを打つ形・検索の照合器（`/e` が `e` + U+0301 に当たらないのは Vim と同じかを別に確かめる）・`~` `gu` `gU`。

## 強制

- fixture（決定 7）: **active**（CTest・CNF-010 / 011）。
- 判定の表が 1 つであること: **planned**（レビュー事項・`grep -rn "DisplayWidth::zero" src/core` が `VimCharacterBoundary.cpp` と仮想桁と表示だけ）。
- 歩く関数が 1 対であること・`forward_characters` / `backward_characters` がそれを使うこと: **planned**（レビュー事項）。
- 速さ: **active**（QLT-014・merge の前に設計席が Release で 6 本を測る。1 歩ごとに表を 1 回引く）。

## 結果

得られるもの: 結合文字・濁点の結合・異体字セレクタつきの本文で、Vim の移動と編集が Vim と同じ 1 文字で動く。NORMAL のキャレットが結合文字の途中に立たない。判定の表は仮想桁と共有で、2 つ目の表が無い。
失うもの・残る穴: ZWJ の連なり・国旗・肌の色は Vim と同じく別々の文字なので、見た目の 1 文字の途中にキャレットが立つ（Vim と同じ・描画は DirectWrite のクラスタ）。通常モードのキー移動はコードポイント単位のままで、クリック（クラスタの境）や Vim モードと揃っていない。行頭の孤立した結合文字の幅は Vim の 1 と違い 0 のまま。1 歩ごとに幅の表の二分探索が 1 回入る。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 書記素クラスタ（UAX #29）で歩く | Vim と違う（Vim は ZWJ の連なり・国旗・肌の色を別々の文字にする）。fixture は本物の Vim が期待値を書くので合わない。表も別に要る |
| `next_code_point` / `previous_code_point` の意味を変える | UTF-16 との変換・表示・照合器が同じ関数でバイト列を走査している。意味を変えると表示側が壊れる |
| 結合文字の集合を Unicode のカテゴリ（Mn / Me）から別の表で持つ | 仮想桁の表の幅 0 と 1998 コードポイントで完全に一致している。表を 2 つ持つと版の更新で食い違う（ARC-001） |
| 通常モードの矢印・Backspace・Delete も同じ Issue で変える | 通常モードの使い手の期待は Windows の慣習（クラスタで歩く・Backspace は結合文字だけ消す、など）で、Vim の規則とは別の判断 |
| 行頭の孤立した結合文字の幅を同時に 1 にする | 仮想桁の表と描画（`display_line`）の両方を変える。歩き方は本 ADR の定義のままで Vim と一致するので、幅は別に決める |
