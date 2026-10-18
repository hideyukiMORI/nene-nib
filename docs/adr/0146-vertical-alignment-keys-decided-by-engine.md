# ADR 0146 — 縦の位置合わせ `zt` `zz` `zb` `z<CR>` `z.` `z-` は engine が先頭行を決め、格子では画面行の単位で合わせる

- 状態: 提案（設計席 2026-10-18。merge は独立レビュー・CI・hide の試用の後・D41）
- 日付: 2026-10-18
- Issue: #639
- 規則: ARC-001 / ARC-004 / ARC-011 / CPP-002 / CPP-012、ADR 0019 / 0034 / 0042 / 0052 / 0129 / 0139 / 0140 / 0144 / 0145
- 調査: `D:/NeNeNib/outputs/20261018/survey-639.md`（今の経路・application の置き場・回数・oracle）・`probe-639.md`（Vim 9.1 の実測 87 件・`--not-a-term`・`scrolloff=0`・`nosmoothscroll`）

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
   - **wrap on の text 形（`enabled && unit == line`）**: 単位は論理行で `first_row = 0`。行の画面行数 `rows(l)` は `layout_request` で 1 行ずつ問う（ADR 0144 決定 2 の往復・最大 h 回）。`top`: 先頭 = 行。`bottom`: 先頭 = 行から上へ `rows(先頭..行) ≤ h` の間だけ足した行（Vim の `scroll_cursor_bot`）。`center`: Vim の `scroll_cursor_halfway` の交互の足し方 — `used = rows(行)`・`above = below = 0` から、`below ≤ above` なら下の行を 1 つ足し（`used += rows`・h を越えたら止める・`below += rows`。文書の末尾を越えたら `~` の 1 行として `below += 1`・`used += 1`）、次に `below > above` なら上の行を 1 つ足す（`used += rows`・h を越えたら止める・`above += rows`・先頭をその行に）。これを先頭が 1 になるか `used > h` で止まるまで繰り返す。nowrap の式（行 − ⌊(h − 1) / 2⌋・最終行で top 96）も wrap の実測（52 行目で h 10 → top 50・h 11 → top 49・h 4 → top 51）もこの 1 本から出る。**キャレットの行が画面より高い（`rows(行) > h`）ときは 3 鍵とも何も変えない**（Vim の実測・先頭も `first_row` も今のまま・失敗にしない）。
   - **格子（`unit == row`）**: 単位は**画面行**（施主決定 D54「格子では画面の移動は画面行の単位」の延長）。キャレットの画面行 r（`row_request` の `origin_of`）を、`top` は画面の 1 行目、`center` は ⌊(h − 1) / 2⌋ 行目（0 始まり）、`bottom` は h − 1 行目に置く。先頭の（行・画面行）は r から上へ `VimScreenRows` の `backward` で歩き、文書の先頭で止まる（上限は収めない）。巨大な 1 行の文書でも画面行で合うので、キャレットが画面の外に出ない。格子は文書全体（D53）なので普通の短い行では wrap on の text 形と同じ所に着く。
6. **横位置は保つ。** `VimNavigate.first_column` は `std::nullopt`（ADR 0145 決定 7・8）。`perform` は横の追従を頼むので、`z<CR>` でキャレットが行頭へ行けば leftcol 0 へ追従する（Vim と同じ）。
7. **`z+` `z^` はこの Issue に入れない（後続）。** 表に無いままで、Vim の未知の鍵と同じく失敗して打ち切る。実測は済んでいる（`z+`: 窓の 1 つ下の行（`botline`）を先頭にして最初の非空白・回数つきは `{N}z<CR>` と同じ。`z^`: 窓の 1 つ上の行を下端にして最初の非空白・回数つきは N を下端に置いてから先頭だった行をさらに下端に）。`botline` は `window_of` の前向きの歩きで出せるので、足すときは同じ表に 2 行と決定 5 の関数で済む。
8. **検証。** nowrap は `eng/vim-oracle.py` の viewport つき fixture（`expected_first_visible` は既にある欄・`--regenerate --only z-align- --reuse-ref <表を変えた commit>` で部分再生成）: 6 鍵 × 字下げあり / なし・回数（`30zt` `999zz` `1zb` `0zt`）・端（2 行目の `zb`・99 / 100 行目の `zt` `zz`）・`$ztj`（欲しい列）・`ztx`（失敗しない）・`dzt`（打ち切り）・`vjzty` `v30zty` `Vjzby`（VISUAL）・`30zt` のマルチバイトの行（桁の収め方は oracle の答えが正本）。既存の `z-align-*-continues` 6 件は再生成で期待値が変わる（no-op でなくなる）。wrap on は `eng/vim-wrap-oracle.py`（`first` と `skipcol` は既にある欄・`--only` が無いので全件を測り直す・173 件 → 増える）: 3 画面行の行での 6 鍵・キャレットが 2 つ目の画面行のときの `zz`・上に 3 画面行の行があるときの `zz` `zb`（h 10 / 11 / 4）・行が画面より高いときの不変。格子は Vim に対応物が無いので契約（`--vim`・替え玉の `row_request`）: 巨大な 1 行の `zt` `zz` `zb` が キャレットの画面行を上・中・下に置く・短い行の文書では wrap on と同じ先頭・文書の先頭で止まる。application は契約 `--application` に「`VimNavigate` の `first_row` を置く・横位置は追従」の既存の検査を再利用（変えないので新しい契約は要らない）。速さのゲートは触らない（1 打鍵の経路は変えない）。

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

- `z+` `z^` と `z{height}<CR>`（高さの変更）は無い（決定 7・高さの変更は窓の大きさなので入れない）。
- wrap on の text 形で画面の高さぶんの `layout_request` の往復が起きる（最大 h 回・ADR 0144 と同じ上限）。1 打鍵の経路ではないので速さのゲートの外。
- 格子の画面行の単位は Vim と違う（Vim には格子が無い）。巨大な行で `z.` を打つとキャレットは行の最初の非空白（行頭）へ飛ぶ。これは Vim の `z<CR>` と同じ動きで、遠いのは行が巨大なため。
- `scrolloff` は無い（Nib は常に 0）。
