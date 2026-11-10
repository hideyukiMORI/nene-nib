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

## 追補 2026-10-30（#1136）: INSERT の矢印も Vim の 1 文字で歩く

Vim モードの INSERT の `<Left>` `<Right>` は、決定 3 の 1 対で 1 文字ずつ歩く（`<Left>` は `vim_character_start`・`<Right>` は `vim_character_end`・Vim の `oneleft` と `mb_ptr2len`）。結合文字・濁点の結合・異体字セレクタの手前には止まらない。ZWJ の連なりは決定 1 のとおり別々の文字。行の端の扱いは ADR 0049 の追補。読むのはキャレットの周りの窓だけで、行の全体は写さない。fixture は `insert-arrow-` の 14 件。通常モードの矢印は決定 8 のまま変えない。

## 追補 2026-11-10（#1325）: Vim モードの Delete キーは engine の `<Del>`

Vim モードの窓は Delete キーを `VimKeyPress{VimSpecialKey::delete_key}` として engine へ流す（表は `src/ui/win32/VimSpecialKeys.hpp` の `vim_specials` の 1 行）。通常モードの `DeleteText`（ADR 0065 の決定 6）は Vim の意味を持たず、Vim モードでは送らない。fixture の記法は `eng/vim-oracle.py` の `KEY_TABLE` に `<Del>` の 1 行（ADR 0054・生成物 `VimKeyNames.hpp`）。規則は Vim 9.1.0004 の source と `-es -u NONE` の実測:

1. **NORMAL は `x`。** `nv_cmds` の `K_DEL` は `nv_abbrev` で、`cmdchar` を `'x'` に替えて `nv_optrans` へ渡す（`"dl"` を積む）。打った回数は 3 のとおり `<Del>` に届かない（`3<Del>` は回数を消すだけで本文を変えない）。回数が `x` に届くのは `.` の流し直し（`2l<Del>3.` は 3 文字）とマクロの回数（`2@a`）。オペレータの後（`d<Del>`）は `nv_optrans` の `checkclearopq` で失敗し、オペレータを打ち消す。
2. **`"a<Del>` は名前を捨てる。** `K_DEL` の行は `NV_KEEPREG` を持たないので、`normal_cmd` が命令の後に `clearop` で名前を消し、積まれた `"dl"` は無名へ書く（`x` の行は `NV_KEEPREG` を持つので `"ax` は `"a` へ書く）。engine は `normal_deleted` で `selected_register` を外して `x` の道を通す。
3. **回数の途中は最後の桁を消す。** `normal_cmd_get_count` は `count0 != 0` の間の `K_DEL` を `count0 /= 10` に読む（`12<Del>x` は 1 文字・`3<Del>x` は回数なし・`2<Del>0` の `0` は行頭への移動・オペレータの後の回数 `2d3<Del>w` も同じ）。判定は `drops_count_digit` の 1 本で、`.` の記録も回数の桁と同じく残さない。次の 1 鍵を待つ状態（`r` `f` `"` `g` など）では名前にも対象にも読まず、Vim の `clearopbeep` で失敗して後ろを打ち切る（`r<Del>x` `f<Del>x` `g<Del>x` は本文を変えない・実測。NORMAL はオペレータと回数も捨て、VISUAL は待ちだけを消す。INSERT の `Ctrl-r` の待ちは除く）。
4. **VISUAL は `x`**（`nv_abbrev` → `v_visop` が `d` のオペレータにする・名前は保つ）。回数の途中は 3 と同じ。
5. **INSERT は `ins_del`。** キャレットの下の Vim の 1 文字（`del_char` → `del_chars` の `utfc_ptr2len`・結合文字を含む・決定 1 の 1 文字）を消し、キャレットは動かない。行の内容の終わりでは次の行とつなぐ（`backspace` の `eol`・`do_join` の `insert_space` は FALSE なので `gJ` と同じく空白を足さず次の行の字下げも残す）。最後の行の終わりでは何も消さない（`vim_beep`）が、`AppendCharToRedobuff(K_DEL)` は必ず通るので、回数の鍵の列と `.` の記録に必ず載せる（Backspace の「止まった鍵は足さない」を通さない）。回数つきの INSERT（`3i<Del>x<Esc>`）の残りの周も `<Del>` を流し直して前方を消すので、流し直しの置き換えの終わりは Esc の位置ではなく、キャレットの後ろの手を付けていない尾の長さから決める（`replayed_insert`・`xxxdefgh`）。
6. **入力行（`:` `/` `?`）の Delete は `c_<Del>`**（`cmdline_erase_chars`）: キャレットの下を消し、行末では手前の 1 文字を消す（実測 `:let g:a=1234<Del>` は 123）。空の行では Backspace と同じく閉じる（source。`-es` は ex mode なので測れない）。controller の `vim_line_edit` が `erase` を行末で `backspace` に替え、通常モードの「検索: 」「行: 」と面の Delete は変えない。再生の中の `<Del>` も同じ `EditCommand{erase}` に写す。

fixture は `del-key-` の 46 件、controller の契約は `nib_tests --vim-del`、窓の表は `nib_window_tests --vim-keys`。
