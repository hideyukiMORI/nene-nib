# ADR 0147 — 画面行の端と中の motion `g0` `g$` `gm` `gM` は engine が画面行の範囲と見えている升から決める

- 状態: 提案（設計席 2026-10-18。merge は独立レビュー・CI・hide の試用の後・D41）
- 日付: 2026-10-18
- Issue: #661
- 規則: ARC-001 / ARC-004 / CPP-002 / CPP-012、ADR 0019 / 0034 / 0042 / 0053 / 0144（決定 2・3・10）/ 0145（決定 2・3・4・6）/ 0146
- 調査: `D:/NeNeNib/outputs/20261018/survey-660-661.md`（motion の道・画面行の範囲の道具・oracle）・`probe-660-661.md`（Vim 9.1 の実測 126 件のうち `g0` 系 60 件余・`--not-a-term`・`scrolloff=0`・`nosmoothscroll`・nowrap は `sidescroll=1 sidescrolloff=0`）

## 文脈

Vim の `g0` `g$` `gm` `gM` は「画面行」の端と中へ動く motion。折り返しのある行では今の段の中、nowrap では見えている範囲の中、`gM` だけは行の本文の幅で決まる。Nib には無く（`g` の接頭辞の表に `j` `k` `t` `T` `e` `E` だけ）、todo の未起票だった。

engine が画面行を知る道具は揃っている: 今の画面行の範囲は `row_request` → `VimRowSpan{line, row, start, end, rows, start_column, width, column}`（ADR 0144 決定 2・`width` は段の中身の升の数）、窓の幅は `VimLayoutView.row_columns`（ADR 0144 決定 10・格子は `columns`・wrap on の text は平均の字幅の近似）、nowrap の見えている升は `VimViewport{columns, left_column}`（ADR 0145 決定 2・横に送る文書だけ `columns > 0`）、画面行の中を歩く関数は `landed(text, span, target)`（`VimScreenRows.cpp`・届かなければ段の最後の文字・最後の段なら `span.end` でモードの寄せは呼び手）。ADR 0145 の押しの道具 `left_edge_character` / `right_edge_character` / `character_at_cell` は無名名前空間にある。

Vim 9.1 の実測（要点・全表は probe）:

- wrap（幅 20・80 字の行・2 段目の 30 桁目）: `g0` c21・`g$` c40・`gm` c31（段の頭 + 20/2）・`gM` c41（行の幅 80 / 2）。**欲しい列は着地の桁**（`g$` の回数 1 も MAXCOL でない・`g$j` は次の行の 40 桁目）。`g0` `gm` は回数を無視。`{N}g$` は N − 1 画面行下へ行をまたいで進み、**欲しい列は MAXCOL**、本文の末尾を越えると失敗（`99g$x` の `x` は効かず `d99g$` は本文不変）。`gM` の回数 1〜100 は行の幅の %、100 超は無視。短い段では `gm` `g$` は最後の文字。
- nowrap（幅 20・leftcol 30・見えている升 31〜50）: `g0` c31・`g$` c50・`gm` c41・`gM` c41（行の中央・leftcol と無関係）。`2g$` は 1 行下の c50 で欲しい列は 49（MAXCOL でない）、`99g$` は最終行で失敗しない。左端で割れた全角は `g0` がその字に着いて leftcol が 1 戻る。
- オペレータと VISUAL: `dg$` は inclusive、`dg0` `dgm` `dgM` は exclusive。最後の段の `vg$y` `vgmy` は改行を含む（NUL に載る）。`dg$` は行末まで（改行を含まない）。
- 次の段へ送られた全角（1 段目が 19 升で 10 個目の全角が次の段へ）: Vim の virtcol は送った所の 1 升（`>`）を数えるので 2 段目の `g0` の欲しい列は 20、1 段目の `g$` の欲しい列も 20。Nib の `virtual_column` は数えない（ADR 0144 の限界の既存の穴）。

## 決定

1. **4 つは motion。`VimMotion` に `display_line_start`（`g0`）・`display_line_end`（`g$`）・`display_line_middle`（`gm`）・`line_middle`（`gM`）を足し、`g` の接頭辞の表に 4 行（ADR 0042 決定 6）。** `g$` は inclusive、ほかは exclusive。`default` / `else` を書かない（CPP-002）。画面の操作として motion の外で解く案は、オペレータと VISUAL の範囲を別に組むことになるので採らない。
2. **範囲は 2 つの源で、どちらを使うかは `columns` で決める（ADR 0145 決定 4 の区別と同じ）。**
   - **横に送る文書（`VimViewport.columns > 0`・nowrap の text 形と巨大な行の窓）**: 見えている升 `[left_column, left_column + columns)`。`g0` = その升以上で始まる最初の文字（左端で割れた全角はその字・ADR 0145 の `left_edge_character`）、`g$` = `left_column + columns − 1` までに収まる最後の文字（`right_edge_character`）、`gm` = 升 `left_column + columns / 2` を含む文字（`character_at_cell`・行の長さを越えれば最後の文字）。ADR 0145 の 3 つの道具を公開して共用し、2 本目を書かない。4 つは `vim_reads_viewport` の表に足して viewport を受ける（ADR 0129 決定 1）。
   - **折り返す文書（`columns == 0` で `layout.enabled`・wrap on の text 形と格子）**: 今の画面行の `VimRowSpan`（`row_request`・ADR 0144 決定 2）。`g0` = `landed(span, start_column)`、`gm` = `landed(span, start_column + row_columns / 2)`（短い段では最後の文字）、`g$`（回数 1）= `landed(span, nullopt)`（段の最後の文字・最後の段は `span.end` をモードで寄せる: NORMAL は最後の文字・VISUAL と INSERT は行の端）。`display_view` で `gj` `gk` と同じ所で先に解く。
   - **どちらでもない（幅 0・文字組みが無い）**: `g0` は `0`、`g$` は `$`（欲しい列は着地の桁）、`gm` は `g0` と同じ（幅が無いので半分も 0）。`gM` は源に依らず行の本文の幅（行頭からの仮想桁の全体・`VirtualColumnIndex` があれば O(塊)）の半分。
3. **回数。** `g0` `gm` は回数を無視する。`gM` は 1〜100 を行の幅の %（`幅 × N / 100` の升を含む文字・`100gM` は最後の文字）、100 超は無視（半分）。`{N}g$` は N > 1 のとき、折り返す文書では `vim_row_landing`（ADR 0144 決定 3 の歩き）で N − 1 画面行下へ欲しい列 MAXCOL で歩き（行をまたぐ）、動き切れなければ失敗（オペレータは打ち消し・後ろの鍵は打ち切り・Vim の `nv_screengo`）、横に送る文書では N − 1 行下の `g$`（失敗しない・欲しい列は着地の桁・Vim の実測）。
4. **欲しい列。** `g0` `gm` `gM` と回数 1 の `g$` は着地の仮想桁（`navigated_state` の置き換えでよい）。`{N}g$`（N > 1・折り返す文書）だけ MAXCOL。`vim_row_landing` に欲しい列を渡す口を 1 つ足す（`gj` `gk` の持ち越し `wanted_in` は使わない）。
5. **オペレータと VISUAL。** `dg$` は inclusive で行末まで（改行を含まない）、`dg0` `dgm` `dgM` は exclusive（決定 1 の表）。最後の段での VISUAL の `g$` `gm` は行の端（NUL）に載り、yank は改行を含む（`$` の VISUAL と同じ側）。`.` は motion なので無関係。
6. **横位置は engine が置かない。** `first_column` は `std::nullopt`（ADR 0145 決定 7）で、左端で割れた全角に `g0` で着いたときは application の最小追従が leftcol を 1 戻す（Vim の実測と同じ結果）。
7. **検証。** `eng/vim-wrap-oracle.py` の fixture: wrap（80 字の行の 2 段目で 4 鍵・後の `j`・`2g$` `9g$`（行をまたぐ）・`99g$x`（失敗）・`d99g$`（不変）・`3g0` `3gm`（無視）・`25gM` `50gM` `100gM` `150gM`・最後の短い段と短い行・Tab の行・`dg$` `dg0` `dgm` `dgM` `d2g$`・`vg$y` `vg0y` `vgmy`・最後の段の `vg$y`）と nowrap（leftcol 30 で 4 鍵・`g$j`・`2g$`・`99g$x`（失敗しない）・leftcol 0・左端で割れた全角の `g0` `g$`・`dg$` `dg0`）。送られた全角の段に当たる件は Nib と合わない見込みなので、oracle の答えを証跡に残して json から外し、限界に記録する。格子は契約（`--vim`・替え玉の `row_request`・`columns` を W にして `gm` が段の頭 + W/2・巨大な 1 行の `g0` `g$` が段の端・`{N}g$` が画面行をまたぐ）。既存 fixture（Vim 1,914 件 + #660 の分・wrap 209 件）は不変。

## 却下した選択肢

- **画面の操作（motion の外）として解く**: オペレータと VISUAL の範囲を別に組むことになり、`dg$` `vg0` の経路が 2 本になる。
- **nowrap でも `row_request` の段（行の全体）を使う**: nowrap の `g0` は見えている左端であって行頭ではない（実測 c31）。
- **`g$` の回数 1 で MAXCOL を置く**: Vim は着地の桁（`g$j` が次の行の 40 桁目）。

## 影響

- core: `VimMotion`（4 値）・`VimKeyTable`（`g` の表 4 行・inclusive の表）・`VimScreenGeometry` / `VimScreenRows`（`display_view` での解決・`vim_row_landing` の欲しい列の口）・`VimHorizontalScroll`（3 つの道具の公開）・`vim_reads_viewport`。application は不変。
- tests: `tests/vim/wrap-fixtures.json`（wrap と nowrap）・`tests/unit/VimEngineTests.cpp`（格子の契約）。
- 文書: `docs/adr/README.md`・`docs/todo/current.md`（Vim の移動の行）・SPECIFICATION の Vim の節に 4 鍵。

## 限界

- 送られた全角の段では Vim の virtcol が 1 多く、`g0` の欲しい列と 1 段目の `g$` の欲しい列が 1 ずれ得る（ADR 0144 の限界と同じ穴・#656 と同じ根）。
- text 形の `row_columns` と `columns` は平均の字幅の近似（ADR 0144 / 0145 の限界と同じ）。`gm` は proportional な字体では Vim と升がずれ得る。
- `g^`（画面行の最初の非空白）・`g_`・INSERT の `<C-o>g$`・矩形 VISUAL の `g$`（`$` の「行末まで」の特例）は入れない（後続）。
