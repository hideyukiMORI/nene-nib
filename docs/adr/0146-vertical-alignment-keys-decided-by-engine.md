# ADR 0146 — 縦の位置合わせ `zt` `zz` `zb` `z<CR>` `z.` `z-` は engine が先頭行を決め、格子では画面行の単位で合わせる

- 状態: 受理（設計席 2026-10-18。独立レビュー R 1 / I 5 / Q 3 → 差し戻し 1・CI run 39096548878・hide の試用 OK・PR #646 で main `086472d`・D41）
- 日付: 2026-10-18
- Issue: #639・#654（決定 9）
- 規則: ARC-001 / ARC-004 / ARC-011 / CPP-002 / CPP-012、ADR 0019 / 0034 / 0042 / 0052 / 0129 / 0139 / 0140 / 0144 / 0145
- 調査: `D:/NeNeNib/outputs/20261018/survey-639.md`（今の経路・application の置き場・回数・oracle）・`probe-639.md`（Vim 9.1 の実測 87 件・`--not-a-term`・`scrolloff=0`・`nosmoothscroll`）・`probe-639-tall-line.md`（行が画面より高いときの `z<CR>` `z.` `z-`）

## 文脈

PR #637（ADR 0145 決定 5）で `z` の接頭辞の表 `z_bindings` ができ、縦の位置合わせの 6 鍵は 1 値 `align_screen_vertically` に写って**何もしない**（失敗にもしない）。鍵の違いはこの時点で消えている。Vim では:

- `zt` `zz` `zb`: キャレットの行を画面の上・中・下に置く。キャレットと欲しい列は動かない（`$ztj` の後も行末）。
- `z<CR>` `z.` `z-`: 同じ 3 つに加えてキャレットを行の最初の非空白へ動かす（欲しい列もそこ）。
- `{N}`: N 行目へ動いてから合わせる（N は 1〜行数に収める・`999zt` は最終行）。桁はバイトの桁のまま行の中に収め、欲しい列は変えない（`30zt` で want 8 のまま）。`0` は回数にならず行頭への移動。
- 端で動けなくても失敗しない（`zbx` の `x` は効く）。最終行の `zt` は下に `~` を残す（top 100）、`zz` は top 96。オペレータ待ちの `dzt` はオペレータを打ち消して打ち切る。VISUAL では選択が残り、`v30zt` は選択を 30 行目まで広げる。
- wrap on（smoothscroll off）でも単位は**論理行**で skipcol は 0 のまま。上下の行の画面行数は数える（52 行目の `zz` は 3 画面行の 50 行目を数えて top 50）。キャレットの行が画面より高いときは 3 鍵とも何も変わらない。
- nowrap の `zt` `zz` `zb` は leftcol を変えない。`z<CR>` はキャレットが行頭へ行くので追従で leftcol 0。

今の Nib（survey-639）: `aligned_screen`（`VimStep.cpp`）は待ちと回数を消すだけ。`vim_reads_viewport` の表に無いので viewport も受けていない。先頭行の置き場は `perform(VimNavigate)` の 1 か所で clamp も追従も呼ばず、Vim モードの extent は `last_line` なので最終行を画面の上に置ける（コード読み）。行の最初の非空白は `vim_first_non_blank`、画面移動の着地は `navigated`（最初の非空白へ）と `navigated_at`（キャレットのまま・ただし欲しい列をキャレットの桁に置き換える）。wrap on の text 形は `layout_request`（`VimLineRows`）で行の画面行数を 1 行ずつ問い（ADR 0144 決定 2）、格子は `row_request` と `VimScreenRows` の `backward` / `forward` で画面行を歩く。画面の高さは `VimViewport.visible_lines`（画面行の数）。

## 決定

1. **先頭行は engine が決め、`VimNavigate{selection, first_visible, first_row}` で返す。application は `perform(VimNavigate)` のまま変えない。** ADR 0019 決定 1（画面移動の計算は engine）・ADR 0144 決定 1（2 本目の経路を作らない・ARC-001）・ADR 0145 決定 1 と同じ形。application に「上・中・下」の計算を持たせない。
2. **`VimAction` の 1 値 `align_screen_vertically` を 6 値に分け、`z_bindings` の 6 行を付け替える。** 値は `align_top` `align_center` `align_bottom`（キャレットを動かさない）と `align_top_first_non_blank` `align_center_first_non_blank` `align_bottom_first_non_blank`（最初の非空白へ）。大分類 `screen_alignment` は 1 つのまま。値 → （位置・動かすか）は `constexpr` の表 1 つ（CPP-012）で、`switch` に `default` を書かない（CPP-002）。`vim_reads_viewport`（ADR 0129 決定 1 の表）に `screen_alignment` を足し、6 鍵は viewport を受ける。
3. **回数は行の選択。** `count` があれば行き先の行は `min(count, 行数)`（1 始まり）で、キャレットはその行の**同じバイトの桁**（行の最後の文字の先頭に収める・Vim の `check_cursor_col`）に置き、欲しい列は変えない。`count` が無ければ今の行。その後に位置合わせ。`0` が回数にならないのは今の `counts_as_digit` のまま。
4. **キャレットと欲しい列。** `align_top` 等 3 鍵は選択もキャレットも欲しい列も変えない（`navigated_at` は欲しい列を置き換えるので使わず、`row_navigated` の「動かなければ欲しい列を保つ」と同じ写し方。決定 3 で行が変わったときも欲しい列は保つ）。`_first_non_blank` の 3 鍵は `navigated`（`vim_first_non_blank`・欲しい列もそこ）。VISUAL では選択の錨を保ってキャレットだけ動く（`visual_acted` の道）。オペレータ待ちは今どおり `failed(cancelled, refused)`（motion でない）。INSERT では今どおり待ちを消すだけ。
5. **先頭行の計算は 3 つの形で、どれも Vim 9.1 の実測に合わせる。h は `visible_lines`（画面行の数）。**
   - **文字組みが無効（nowrap の text 形・`layout.enabled == false`）**: 1 論理行 = 1 画面行。`top`: 先頭 = 行。`center`: 先頭 = 行 − ⌊(h − 1) / 2⌋。`bottom`: 先頭 = 行 − (h − 1)。先頭は 1 未満を 1 に収め、**上限は収めない**（最終行を先頭に置けて下に空白が並ぶ・`perform` と `clamp_scroll` の `last_line` がそのまま保つ）。`first_row = 0`。
   - **wrap on の text 形（`enabled && unit == line`）**: 単位は論理行で `first_row = 0`。行の画面行数 `rows(l)` は `layout_request` で 1 行ずつ問う（ADR 0144 決定 2 の往復・最大 h 回）。`top`: 先頭 = 行。`bottom`: 先頭 = 行から上へ `rows(先頭..行) ≤ h` の間だけ足した行（Vim の `scroll_cursor_bot`）。`center`: Vim の `scroll_cursor_halfway` の交互の足し方 — `used = rows(行)`・`above = below = 0` から、`below ≤ above` なら下の行を 1 つ足し（`used += rows`・h を越えたら止める・`below += rows`。文書の末尾を越えたら `~` の 1 行として `below += 1`・`used += 1`）、次に `below > above` なら上の行を 1 つ足す（`used += rows`・h を越えたら止める・`above += rows`・先頭をその行に）。これを先頭が 1 になるか `used > h` で止まるまで繰り返す。nowrap の式（行 − ⌊(h − 1) / 2⌋・最終行で top 96）も wrap の実測（52 行目で h 10 → top 50・h 11 → top 49・h 4 → top 51）もこの 1 本から出る。**キャレットの行が画面より高い（`rows(行) > h`）ときは `zt` `zz` `zb` は何も変えない**（Vim の実測・先頭も `first_row` も今のまま・失敗にしない）。**ただしキャレットが画面の外に出る形は作らない**: 行が画面より高いときの先頭は「その行」で、`first_row` は今の値を `[キャレットの画面行 − (h − 1), キャレットの画面行]` に収めた値（キャレットの移動の後で計る）。同じ行での `zt` `zz` `zb` はキャレットが見えているので今の値のまま（Vim の不変と同じ）、最初の非空白へ動く 3 鍵はキャレットの画面行が 0 なので `first_row = 0`（Vim 9.1 の実測 `probe-639-tall-line.md`: `z<CR>` `z.` `z-` で skipcol 20 → 0）、回数で画面より高い別の行へ動いたときは今の `first_row` を同じ範囲に収める（実測 `4zt`: skipcol 20 のまま行 4 が先頭・キャレットは下の画面行に見える）。oracle の答えがこれと違う件が出たら fixture は oracle に従い、ADR に実測を追記する。
   - （3 つの形に共通）最初の非空白へ動く 3 鍵は、先にキャレットを動かしてから、動いた後のキャレットの行と画面行で先頭を決める（Vim の `nv_zet` の順。nowrap と wrap on では同じ行なので結果は同じ・格子では画面行が変わり得る）。
   - **格子（`unit == row`）**: 単位は**画面行**（施主決定 D54「格子では画面の移動は画面行の単位」の延長）。キャレットの画面行 r（`row_request` の `origin_of`）を、`top` は画面の 1 行目、`center` は ⌊(h − 1) / 2⌋ 行目（0 始まり）、`bottom` は h − 1 行目に置く。先頭の（行・画面行）は r から上へ `VimScreenRows` の `backward` で歩き、文書の先頭で止まる（上限は収めない）。巨大な 1 行の文書でも画面行で合うので、キャレットが画面の外に出ない。格子は文書全体（D53）なので普通の短い行では wrap on の text 形と同じ所に着く。
6. **横位置は保つ。** `VimNavigate.first_column` は `std::nullopt`（ADR 0145 決定 7・8）。`perform` は横の追従を頼むので、`z<CR>` でキャレットが行頭へ行けば leftcol 0 へ追従する（Vim と同じ）。
7. **`z+` `z^` は #639 には入れず、後続の #654 で足した（決定 9）。** #639 の時点では表に無く、Vim の未知の鍵と同じく失敗して打ち切っていた。実測は済んでいた（`z+`: 窓の 1 つ下の行（`botline`）を先頭にして最初の非空白・回数つきは `{N}z<CR>` と同じ。`z^`: 窓の 1 つ上の行を下端にして最初の非空白・回数つきは N を下端に置いてから先頭だった行をさらに下端に）。`botline` は `window_of` の前向きの歩きで出せるので、同じ表に 2 行と決定 5 の関数で足した。
8. **検証。** nowrap は `eng/vim-oracle.py` の viewport つき fixture（`expected_first_visible` は既にある欄・`--regenerate --only z-align- --reuse-ref <表を変えた commit>` で部分再生成）: 6 鍵 × 字下げあり / なし・回数（`30zt` `999zz` `1zb` `0zt`）・端（2 行目の `zb`・99 / 100 行目の `zt` `zz`）・`$ztj`（欲しい列）・`ztx`（失敗しない）・`dzt`（打ち切り）・`vjzty` `v30zty` `Vjzby`（VISUAL）・`30zt` のマルチバイトの行（桁の収め方は oracle の答えが正本）。既存の `z-align-*-continues` 6 件は再生成で期待値が変わる（no-op でなくなる）。wrap on は `eng/vim-wrap-oracle.py`（`first` と `skipcol` は既にある欄・`--only` が無いので全件を測り直す・173 件 → 増える）: 3 画面行の行での 6 鍵・キャレットが 2 つ目の画面行のときの `zz`・上に 3 画面行の行があるときの `zz` `zb`（h 10 / 11 / 4）・行が画面より高いときの不変。格子は Vim に対応物が無いので契約（`--vim`・替え玉の `row_request`）: 巨大な 1 行の `zt` `zz` `zb` が キャレットの画面行を上・中・下に置く・短い行の文書では wrap on と同じ先頭・文書の先頭で止まる。application は契約 `--application` に「`VimNavigate` の `first_row` を置く・横位置は追従」の既存の検査を再利用（変えないので新しい契約は要らない）。速さのゲートは触らない（1 打鍵の経路は変えない）。

9. **（追補 2026-10-18・#654）`z+` `z^` を足す。** 決定 7 の後続。
   1. `z_bindings` に `+` と `^` の 2 行、`VimAction` に `align_below_window_top_first_non_blank`（`z+`）と `align_above_window_bottom_first_non_blank`（`z^`）の 2 値。大分類は `screen_alignment` のまま。値 → （位置・最初の非空白へ動かすか・行の選び方 `VimAlignedLine`）の表に 2 行（CPP-012・`default` なし）。既存の 6 鍵の行の選び方は `caret`（決定 3）。
   2. 行の選び方（Vim 9.1 の `nv_zet` と実測 `probe-639.md` の `z+z^` の節）: `z+` は回数が無ければ窓の 1 つ下の行（botline・画面に全部入っている最後の行の次。文書の末尾を越えるなら最終行）を先頭に置き（決定 5 の `top`）、最初の非空白へ。回数つきは `{N}z<CR>` と同じ。`z^` は回数が無ければ窓の 1 つ上の行（`first_visible − 1`・1 行目が先頭なら 1 行目・格子で `first_row > 0` なら 1 つ上の画面行は同じ行にあるので同じ行）を下端に置き（決定 5 の `bottom`）、最初の非空白へ。回数つきは、行 N を下端に置いたときの先頭 T を決定 5 の `bottom` の関数で求め、T の最初の非空白へ動いて T を下端に置く（実測 `30z^`・h 10 → top 12・21 行目）。
   3. botline は今の前向きの歩きで求め、2 本目の歩きを書かない。nowrap は `first_visible + visible_lines`（最終行を越えれば最終行）。wrap on の text 形は `window_of` の「入らなくなった最初の行」（`below`）で、行の画面行数は `layout_request` で 1 行ずつ問う。格子は `VimScreenRows` の `forward` で画面の先頭から画面の高さぶん歩いた画面行の行（文書の最後の画面行で止まる）。
   4. キャレット・欲しい列・VISUAL・オペレータ待ち・端で失敗しないことは決定 3・4 と同じ（最初の非空白へ動くので `navigated`）。横位置は決定 6（`nullopt`）。
   5. 検証: nowrap は viewport つき fixture（`z-plus-` `z-caret-` の 12 件・`--regenerate --only z-plus- --only z-caret-`）、wrap on は `eng/vim-wrap-oracle.py` の 4 件（3 画面行の行が窓の下端にかかるときと先頭のときの `z+`・上にあるときの `z^` と `60z^`・全件を測り直す）、格子は契約 `--vim`（巨大な 1 行で `z+` `z^` が同じ行の先頭の画面行を上・下に置く・短い行の文書で wrap on と同じ先頭）。
      件数は main（#653 の後）への積み直しの後で Vim の fixture 1913 → 1914（`999z+` の範囲の外を含む 13 件）・wrap 205 → 209。oracle の答えと決定 2 の差は無し。
   6. （追補 2026-11-04・#1227）回数つきの `z^` の 1 段目は余白（`scrolloff`）を含む。Vim 9.1 の `nv_zet` の `case '^'` は回数があると `scroll_cursor_bot(0, TRUE)` を呼んでから `w_cursor.lnum = w_topline` とする。`scroll_cursor_bot` は `set_topbot` の枝で行 N を下端に置いた後、`extra >= so`（下の余白を数え終えた）か `boff.lnum + 1 > ml_line_count`（本文の終わり）まで下の行を足して `scrollup` するので、T は「N を下端に置き、N の下に余白の分（本文の終わりで切れる）の画面行を足した窓」の先頭になる。2 では余白を数えず、余白の分だけ上の行に着いていた（生成器 5 の 10 件・#1224）。直しは `counted_above_line` が決定 5 の `bottom` の先頭に追従の 1 本 `vim_topline_updated`（Vim の `update_topline` → `check_botline` → `scroll_cursor_bot`・ADR 0019 の追補）を当てて T を求める（余白の数え方を 2 本書かない）。2 段目（T を下端に）は従来どおり追従が余白を当てる。回数なしの `z^` と `z+` は変わらない。fixture `viewport-z-caret-count-` の 5 件（Issue の表・本文の終わりで余白が 1 になる `41z^` を含む）。

## 却下した選択肢

- **engine は意図 `AlignScreen{top|center|bottom}` に写すだけで application が先頭行を決める**: 文字組みの往復が要らず小さいが、ADR 0019 決定 1 と ADR 0144 決定 1 に反して画面移動の計算が 2 か所になる。
- **wrap on でも画面行の単位で合わせる（smoothscroll の形）**: Vim 9.1 の既定（`nosmoothscroll`）と違い、oracle の fixture で守れない。格子だけ画面行なのは D54 が格子を画面行の単位と決めたから。
- **`z+` `z^` を同じ PR に入れる**: `botline` の前向きの歩きと回数つきの 2 段の計算が増え、最初の PR が大きくなる。実測は残したので後続で足せる。
- **`{N}zt` で `G` と同じく最初の非空白へ動かす**: Vim はバイトの桁を保つ（実測 `30zt` で col 9 のまま）。

## 影響

- core: `VimAction`（1 値 → 6 値・`vim_action_count`）・`VimKeyTable`（`z_bindings` の 6 行・位置の表）・`VimScreenGeometry`（`vim_reads_viewport`・nowrap と wrap on の先頭行の計算。`page_above` が近いが「キャレットの行自身を数える」形に合うかは実装席が確かめる）・`VimScreenRows`（格子の画面行の歩き・`backward` の再利用）・`VimStep`（`aligned_screen` を「行を選ぶ → 先頭を決める → 写す」に）。application は不変。
- tests: `tests/vim/fixtures.json`（viewport つき・`z-align-` 系の追加と 6 件の期待値の更新・`VimFixtures.hpp` は生成物・CNF-010）・`tests/vim/wrap-fixtures.json`（全件再生成）・`tests/unit/VimEngineTests.cpp` の格子の契約。
- 文書: `docs/adr/README.md`・ADR 0145 決定 5 と限界（「`zt` 等は何もしない」を外す）・ADR 0129 決定 1 の表に 1 種・`docs/todo/current.md`（Vim の移動の行）・SPECIFICATION の Vim の節に 6 鍵。

## 限界

- `z+` `z^` は足した（決定 9）。`z{height}<CR>`（高さの変更）は無い（窓の大きさなので入れない）。
- wrap on の text 形で画面の高さぶんの `layout_request` の往復が起きる（最大 h 回・ADR 0144 と同じ上限）。1 打鍵の経路ではないので速さのゲートの外。
- 格子の画面行の単位は Vim と違う（Vim には格子が無い）。巨大な行で `z.` を打つとキャレットは行の最初の非空白（行頭）へ飛ぶ。これは Vim の `z<CR>` と同じ動きで、遠いのは行が巨大なため。
- 格子の回数つき `z^` は、1 段目の先頭の行の画面行 0（行頭）を下端に置く（`z<CR>` の格子と同じく行の単位で選ぶ）。窓の先頭が巨大な 1 行の途中（`first_row > 0`）で窓の 1 つ下の画面行が同じ行のとき、格子の `z+` は同じ行の画面行 0 を先頭に置くので画面が上へ戻る（Vim の画面より高い行の `z+` と同じ）。
- 格子の巨大な行の回数つき `z+` `z^` は契約で未検査（契約は短い行の `5z+` `25z^` だけ）。
- `scrolloff` は無い（Nib は常に 0）。

## 追補（2026-11-04・#1235）: 回数つきの位置合わせは跳び

Vim の `nv_zet` は `z<CR>` `zt` `z.` `zz` `z-` `zb` `z+` `z^` に回数 `count0` があり今の行の番号と違えば、回数の行へ動く前に `setpcmark()` する。Nib も `aligned_screen` が同じ条件（`counted_alignment_jumps`）で前の位置を `''` `` `` `` とジャンプの列へ置く。跳びの一覧と鍵の終わりの戻し方は ADR 0152 の追補（#1235）。

## 追補（2026-11-04・#1231）: 回数つきの `zt` `zz` `zb` の欲しい列は動く前のキャレットで決まる

生成器 5 の fuzz（#1224）が見つけた違い（`so=2` 高さ 11・先頭 14・17 行目の `gI<Tab>かな<Esc>18zz3k` は Vim 8 桁 / Nib 5 桁）。

1. **Vim の規則。** `exec_normal`（`:normal!`）は鍵ごとに `update_topline_cursor` → `update_curswant` を、対話の `main_loop` も鍵ごとに `update_curswant` を呼ぶので、`w_set_curswant` が立っている（Esc の `oneleft` の後など）欲しい列は**次の鍵を始める前**に今のキャレットの桁で決まる。`nv_zet` の回数の行への移動は `w_cursor.lnum` を変えて `check_cursor_col` で桁を収めるだけで、`w_set_curswant` を立てない。だから `{N}zt` `{N}zz` `{N}zb` の後の `j` `k` は**動く前の行の桁**を使う（`z<CR>` `z.` `z-` は `beginline` で欲しい列も最初の非空白・決定 4 のまま）。
2. **Nib。** 欲しい列がまだ決まっていない（`VimState.wanted_column` が無い）ときに `aligned_screen` がそれを空のまま運び、次の `k` が回数の行のキャレットの桁から決めていた。`aligned_screen` は `wanted_column_of(本文, 状態, 動く前のキャレット)` を置く（決まっていればそのまま・決定 4 の「欲しい列を保つ」の写し方は変えない）。
3. **検証。** fixture `viewport-small-` 7 件（直す前に落ちるのは Esc の後の 2 件・`l` の後の 5 件は Nib も欲しい列を決めているので前から一致する守り・Issue の件を `so=2` / `so=0` で 2 件・`4l17zzk` `4l17ztk` `4l17zbk`（`so=2`）の短い行で収めた桁・同じ行の `$16zz2j`・回数つきの `z.` は最初の非空白）。
4. **同じ Issue の残り 2 件。** (a) `<C-d>vggx2<C-u>f;` は #1228（カーソルが 1 行目の Ctrl-u は失敗して残りの鍵を捨てる）で既に直っていて、fixture `viewport-edge-fails-half-up-after-a-visual-delete-to-the-top` が守る（Issue の見込みの `startofline` ではなかった）。(b) `z-M`（`so=3` 高さ 11・先頭 5・8 行目で Vim 4 行目 / Nib 6 行目）は本件では直さない。Vim の `scroll_cursor_bot(0, TRUE)` は先頭が 1 行目に着くと `set_empty_rows(used)` で「1〜8 行目の和」から空き段 3 を置き、`VALID_BOTLINE` を立てたまま返る（先頭が変わったときは「`update_screen()` が botline を決め直す」とだけ書いてある）。`:normal!` の中では描き直しが無いので、次の `M` の `validate_botline` は古い `w_empty_rows` = 3 を読み、半分が `(11 − 3 + 1) / 2 = 4` になる。`z-` と `M` の間に `redraw!` を挟むと Vim も 6 行目（Nib と同じ・`D:/NeNeNib/scripts/20261104/1231/probe.vim` で実測）。打った鍵の Nib は毎回描く対話の Vim と同じ答えなので、今の Nib のままにする（設計席の判断・候補 B）。oracle の `:normal!` は描き直しが無いので `scroll_cursor_bot(0, TRUE)` が置いた古い `w_empty_rows` を次の `M` / `L` が読む。`redraw!` を挟んだ対話の Vim は Nib と同じ。fuzz では既知として除く（probe は `D:/NeNeNib/scripts/20261104/1231/probe.vim`）。

## 追補（2026-11-10・#1294）: 回数つきの位置合わせの後の行単位の `y` は動く前の位置と欲しい列の早い方に着く

生成器 5 の fuzz（seed 15・`fuzz-15-0158`）が見つけた違い（`so=0` 高さ 10・先頭 18・空の 23 行目で `2a<Tab><C-r>"<Esc>27zt"q4yy` は 27 行目で Vim 2 桁 / Nib 16 桁）。

1. **Vim の規則（z 系）。** `nv_zet`（normal.c）は回数 `count0` が今の行と違えば `w_cursor.lnum = count0` と `check_cursor_col()`（misc2.c の `check_cursor_col_win`）で、**バイトの桁を保ち**、行の長さを超えれば最後の文字の先頭、空の行なら 0 に収める。桁が全角の途中に落ちたときは鍵の終わりの `normal_cmd` の `mb_adjust_cursor()` が文字の先頭へ寄せる。`w_set_curswant` は立てないので欲しい列は動く前のもの（#1231 の追補）。ここまでは Nib も決定 3 のとおり同じだった（fixture で一致を確かめた）。
2. **Vim の規則（後ろの行単位の `y`）。** 違いはその後の `yy` `{N}yy` `Y` `y_` `yj` にあった。`nv_lineop`（normal.c）は `cursor_down(count1 - 1)`（edit.c）で回数の最後の行へ下り、`n` が 0 でも `coladvance(w_curswant)` で**欲しい列へ動く**。`do_pending_operator`（ops.c）は動く前の位置 `oap->start` と今のカーソルを `LT_POS`（行, 桁）で比べて早い方へ置く。だから下へ動く `{N}yy` `yj` は動く前のバイトの桁のまま、1 行の `yy` `Y`（`yy` を積む）`y_` は「動く前の位置」と「同じ行の欲しい列」の早い方。その後に `w_set_curswant = TRUE` を立てるので、次の `j` `k` の欲しい列は置いた位置から決まる。
3. **Nib。** `yanked_caret`（`VimStepOperators.cpp`）は NORMAL の行単位で範囲の最初の行に**欲しい列から引き直した**キャレットを置いていた。ふだんは欲しい列とキャレットの桁が同じで違いが出ず、回数つきの `zt` `zz` `zb` の後（欲しい列は動く前の行の桁・キャレットは回数の行の同じバイトの桁）だけ違った。NORMAL の行単位は欲しい列から引き直さず、呼び手が渡した位置そのものを置く。「動く前の位置と着地の早い方」（Vim の `oap->start`）は `operator_start` の 1 本が決め、行単位の `y` の呼び手はすべてそこを通る: `yy` `Y` `y_` は `line_yank_from`（回数が 1 なら同じ行の欲しい列が着地・回数つきは下の行なので動く前）、移動の `y` は `operated`（上へ・同じ行の手前の `yk` `ygg` `$yH` `y'a` など・ADR 0015 の追補 #411 / #965 / #1065 は結果が前と同じ）、検索の `y` は `search_operated`（後ろ向きで行単位になった `y?pat<CR>` `yN` `yn` `y#` は検索の当たりに着く・前向きの `y/pat<CR>` は動く前）。範囲の外の行から欲しい列で引き直す古い道は無くした（後ろ向きの検索がそこへ落ちて、Vim は当たり `oap->start` に着くのに Nib は欲しい列の桁に置いていた・PR #1315 のレビュー）。着地は範囲と同じ移動の答えで、オペレータの後ろの `H` `M` `L` は余白（scrolloff）へ寄せない（Vim の `nv_scroll` は `OP_NOP` のときだけ `cursor_correct`・`screen_range` と同じ行。古い道が寄せた行の着地を範囲の最初の行へ引き直して隠していた）。
4. **検証。** fixture `viewport-z-count-col-` 25 件（Tab の行から ASCII の行へ `zt` `zb` とその後の `j`・空の行で `2a<Tab><Esc>` の後・ASCII から全角の行へ文字の途中と先頭とその後の `j`・全角から Tab の行へ・短い行と空の行と全角の行で切れる形・回数が行数を超える・今の行と同じ回数・VISUAL・その後の `"q4yy` `4yy` `yj` `yk` `2yy` `yyj`・Tab の行での 1 行の `yy` `Y` `y_` が欲しい列へ寄る形）。main の再生器（beta.8 と同じ製品のコード）で落ちるのは `yy` 系の桁の 5 件。最初の直し（範囲の最初の行ならキャレットそのもの）だけでは fuzz の seed 19（`fuzz-19-0285`・`55ztY*` の `Y` が欲しい列へ寄らず `*` の語が変わる）が新しく落ちたので、2 の 1 行の規則を足した。fixture `yank-line-caret-` 12 件（`set scrolloff=3` の `yH` 4:1・後ろ向きの検索 `8G0y?indented<CR>` 6:5・`15l3jy?alpha<CR>` 1:1 とその後の `j` 2:1・Tab の行の `4G0y?foo<CR>` 3:2・`yN` `yn` 7:5・`8G0y#` 3:2・前向きで行単位の `3Gy/^\s*indented<CR>` 3:2・マークの `y'a` 7:5・`ygg` 2:1・`ykj` 4:1）。直す前に落ちるのは後ろ向きの検索の 7 件。

## 追補（2026-11-12・#1483）: 先頭が 1 行目に詰まった `zb` `z-` の後の古い下端は `:normal!` の中だけの違い

生成器 6 の fuzz（seed 66・`fuzz-66-0128`）が見つけた違い（30 行・高さ 12・先頭 2・9 行目・`so=2` の `zb<C-b>` は 1 本の `:normal!` の Vim 7 行目 / Nib 9 行目）。#1231 の追補 4 (b) の `z-M` と、ADR 0019 の #1362 の追補 5 の `z-2<C-f>` と同じ根で、Nib は変えない（設計席の判断・案 A）。

1. **Vim の規則（source の読み・9.1.0004）。** `zb` と `z-` は `move.c` の `scroll_cursor_bot(0, TRUE)`。`set_topbot` の道は先に `w_botline = cln + 1` と置き、先頭を上へ戻して窓に入る間だけ行を足し、`set_empty_rows(used)` で空き段を置いて `VALID_BOTLINE | VALID_BOTLINE_AP` を立てる。先頭が 1 行目に詰まって窓が埋まらない（`used < w_height`）と、`w_botline` は本当の下端（窓の高さ + 1）より上のまま残る。終わりで `w_botline` を戻すのは先頭が変わらなかったときだけで、変わったときは「`update_screen()` will set botline」とだけ書いてある。`:normal!`（`exec_normal`）は鍵の間で描き直さないので、次の鍵がこの古い下端を読む。対話の Vim は鍵ごとに描き直すので読まない。
2. **読む鍵は 5 つ。** FAIL した `<C-b>`（`onepage` が `beep_flush` の後も呼ぶ `cursor_correct`）・`<C-y>`（`scroll_redraw` の後の `cursor_correct`・`so` > 0 のとき）・`<C-f>`（`onepage` の FORWARD の重なりの起点 `loff.lnum = w_botline`）・`L`（`w_botline - 1`）・`M`（`w_botline` と `w_empty_rows` の半分）。`cursor_correct` は下の欲しい行を古い下端から数えるので、`zb<C-b>` は「キャレットの行 − `so`」へ上がる（`so=0` は欲しい行が 0 で動かない）。`H` `<C-u>` `<C-d>` `<C-e>` は読まない（実測で 3 つの形が一致）。
3. **消える鍵と残る鍵（1 本の `:normal!`・`so=2`）。** `j` `k` `l` の後は消える（`zbk<C-b>` 8・`zbl<C-b>` 9 で鍵ごとに描く形と同じ）。`0` と 2 回目の `zb` の後は残る（`zb0<C-b>` 7・`zbzb<C-b>` 7。2 回目の `zb` は先頭が 1 のままなので古い値を戻す）。`<C-y>` の後に続く鍵は、1 回目の `<C-y>` がもう 7 へ動かしているので残るかを決められない（確かめていない）。
4. **鏡の形は起きない。** 窓の下が本文の末尾に詰まる側の `zt` `z<CR>`（`scroll_cursor_top`）は先頭が変われば `VALID_BOTLINE` を落とし、`zz` `z.`（`scroll_cursor_halfway`）も落とす。先頭 19〜20・行 25〜30・`so` 0 / 2 / 3 の `zt` `z<CR>` `zz` `G` の後の `<C-f>` `<C-e>` `H` の 108 件は 3 つの形がすべて一致した。
5. **実測（218 件・`docs/quality/vim-stale-bottom-2026-11-12.md` に全件）。** 1 本の `:normal!`（oracle と fuzz の形）・鍵ごとに `redraw!` を挟む形（対話の Vim）・今の Nib（`nib_vim_replay`）を比べた。1 本の `:normal!` と Nib が違うのは 43 件で、すべてこの族（先頭が変わって 1 行目に詰まった `zb` `z-` の直後に上の 5 つの鍵）。対話の Vim と Nib は 216 件で一致し、残る 2 件（`zb<C-b>L`）は分けた形だけの差（1 本の `:normal!` では FAIL の `beep_flush` が後ろの `L` を捨て、Nib も捨てる・#1228）。代表: `zb<C-b>`（先頭 2・9 行目・`so=2`）は 1 本 7 / 対話と Nib 9、`zbL`（`so=0`）は 9 / 12、`zb<C-f>`（`so=2`）は先頭 8・10 行目 / 先頭 11・13 行目。
6. **fuzz。** 生成器 6 の `DEFAULT_KNOWN` に #1483 の行を足した（`eng/vim-fuzz.py` の `stale_bottom_after_zb`）。既知に数えるのは、縮んだ件の語がちょうど `zb` / `z-` と読む 5 つの鍵の 1 つの 2 語で、違う欄が `line` `column` `first_visible` だけで、Vim 9.1 で古い下端が残ると測った形だけ（PR #1504 のレビューと直しの席の実測・`-u NONE`・1 本の `:normal!` と鍵ごとに描く形の 400 件で、規則が既知に入れて 2 つの形が一致した件は 0）。
   - 鍵の前の先頭が 1 でない（1 なら `zb` が下端を戻す）。
   - `zb` / `z-` に回数が無いか、回数が鍵の前の行と同じ。回数が今の行と違う `5zb` `8zb` `5z-` は、回数の行へ動く道を通ると 1 本の `:normal!` でも古い下端が残らない（30 行・高さ 12・先頭 10・15 行目・`so=2` の `5zb<C-b>` は 3 つの形とも 5 / 1。レビューの 11 件が同じ。source のどこで直るかは確かめていない）。
   - 行が窓の高さより上で、行に余白（`scrolloff` を窓の `(高さ − 1) / 2` で頭打ちにした値）を足して窓の高さ以下（先頭が 1 行目に詰まって窓が埋まらない形。高さ 12・`so=2` の 11 行目は先頭 2 で止まり 3 つの形が一致、10 行目は残る。`so=20` は 5 として 7 行目まで）。
   - `<C-b>` `<C-y>` は `so` > 0 のときだけ（`so=0` の `cursor_correct` は動かない）。`M` は行が窓の最後の行より 2 行以上上のときだけ（1 行上は高さが偶数だと同じ行に丸まる・高さ 12 の 11 行目・高さ 10 の 9 行目。奇数の高さでは違いが出るが、確かめきれない側として既知に入れない）。
   `j` `k` `l` など 3 語目を挟んだ形は既知にしない（古い下端が消えるので、違えば別の原因）。試験は `tests/conformance/test_vim_fuzz_known.py`。#1483 は閉じるが直さない違いなので、閉じた後も行を残す。
7. **限界。** 対話で打つマクロ `@a` は鍵ごとに描き直す形と同じで Nib と一致する。古い下端が残るのは `:normal!` の中と `lazyredraw` の下だけで、Nib にはどちらも無い（PR #1504 のレビューの実測（`D:/NeNeNib/outputs/20261112/review-1504/review.md` の表の 30 形）: 1 本の `:normal!`・`feedkeys` で打つ形・対話のマクロ・`:normal! @a`・`lazyredraw` のマクロの 5 つの形と Nib を比べ、Nib を測った 29 形は打つ形と対話のマクロの 2 つとすべて一致し、`:normal! @a` と `lazyredraw` のマクロは 1 本の `:normal!` と同じ値。回数が今の行と違う `Nzb` `Nz-` の 11 件は 5 つの形と Nib がすべて一致した）。マクロの fixture（register 欄）も oracle が `:normal!` で測るので、この族は fixture に入れられない。対話の Vim の形（鍵ごとに描き直す）で Nib が一致することを守る fixture は足していない。今の oracle（`eng/vim-oracle.py`）は鍵の列を 1 本の `:normal!` で測るので、この形を fixture の期待値にできない（ADR 0019 の #1300 の追補「束ねた形とだけ違う形は fixture に入れない」）。上の 216 件の一致は実測の記録だけで、退行は fuzz の既知の規則の外に出る違いとしてしか見えない。折り返す行は測っていない（viewport の oracle は `set nowrap` に固定、`eng/vim-wrap-oracle.py` は `so=0` に固定で、この族は `so` > 0 か `L` `M` `<C-f>` で出る）。source の `move.c` は GitHub の vim/vim の tag `v9.1.0004` から取った（それまでの読み合わせの置き場に無かった）。
