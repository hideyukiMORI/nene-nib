# ADR 0144 — 格子の画面移動と `gj` `gk` は画面行の単位で、engine が行の画面行を問いで引いて決める

- 状態: 提案（設計席 2026-10-17。施主決定 D54・案 A。merge は hide の試用の後・D41）
- 日付: 2026-10-17
- Issue: #595（格子と窓の画面移動）・#596（`gj` `gk`）
- 規則: ARC-001 / ARC-004 / ARC-011 / CPP-002 / CPP-012、ADR 0019 / 0034 / 0114 / 0128 / 0129 / 0139 / 0140
- 調査: `D:/NeNeNib/outputs/20261017/survey-595.md`（現状の経路）・`probe-596.md`（Vim 9.1 の `gj` `gk` の実測）

## 文脈

hide の試用（2026-10-16 夜）で、1 行 16 MiB を格子（ADR 0140）で開いて PgDn を押しても動かなかった。調査の結果:

- Vim の PgDn / PgUp / Ctrl-d / Ctrl-u / Ctrl-f / Ctrl-b は engine（`VimScreenGeometry.cpp`）が**論理行を単位**に、各行の画面行数を重みとして Vim 9.1 の巻きを再現する（ADR 0019・0114）。1 行しか無い文書では動く先が無く、`VimNavigate{selection, first_visible}` には画面行の欄が無いので、`perform` が `ScrollState.first_row` を 0 に戻す（`EditorController.cpp:2517`）。下へ送った後に押すと最上段へ戻り、キャレットは画面の外に残り得る。
- 通常モードの PgDn / PgUp は `move_visual`（`EditorView.cpp:74-109`）が画面行で歩くので格子でも動くが、送りは「キャレットが見える最小」（`ScrollFollow::minimal`）で、先頭からの最初の 1 回は 1 画面行しか送られない。通常モードの Ctrl-u / Ctrl-f / Ctrl-b は何にも割り当てられていない（Ctrl-d はブックマーク・ADR 0063）。
- `gj` `gk`（画面行で上下）は無い。折り返した行と格子では画面行で上下する鍵が無いと上下に行けない。

施主決定 D54（案 A・Vim に忠実）: `j` `k` は論理行のまま。画面の移動は格子と巨大な行の窓では画面行の単位で動き、キャレットも同じ数だけ動く。`gj` `gk` を足す（折り返した普通の行でも効く）。

Vim 9.1 の `gj` `gk` の実測（probe-596・通常端末モード・`wrap`・`columns=40`・smoothscroll off）: 画面行で ±1（回数つき）・欲しい列（curswant）は着いた位置の桁に置き直さず、移った画面行ぶんずらして持ち越す（行の途中の画面行では幅 − 1 に収め、最後の画面行では短くても保つ・170 字の行の 151 桁から `gj` → 170 桁・続く `j` は 191 桁）・`k` の後の `gj` は欲しい列を画面行の幅に収める・`$` の後は行末のまま（MAXCOL）・Tab の中では欲しい列を保って Tab の上に着く・端では失敗して後ろの鍵を打ち切る・オペレータは文字単位の exclusive（行をまたいでも行単位にならない）・`vgj` は inclusive・`Vgj` は行単位。幅つきの oracle `eng/vim-wrap-oracle.py`（ADR 0114）がこれを fixture にできる（入力を足さずに `tests/vim/wrap-fixtures.json` へ足すだけ）。

## 決定

1. **画面行の単位の移動は engine が決める。application は「行の中の画面行」を問いで答えるだけ。** ADR 0019 の決定 1（画面移動の計算は engine・ui / application は持たない）と ADR 0114 の決定 5（画面行への Vim 操作は同じ位置解決を使う）を保つ。application に Vim の画面移動の計算を持たせない（2 本目の経路を作らない・ARC-001）。
2. **問いは 1 種類、`VimStep.row_request`。** `layout_request`（行の画面行数・`VimLineRows`）と同じ往復で、engine が立てたら controller が答えて同じ鍵を流し直す。問い `VimRowRequest{LineNumber line, std::variant<TextOffset at, std::size_t row>}` は「この位置はその行の何番目の画面行か」か「この行の r 番目の画面行はどこからどこまでか」のどちらかで、答え `VimRowSpan{line, row, start, end, rows}`（画面行の番号・その画面行の本文の範囲 `[start, end)`・行の画面行の総数）を `VimLayoutView.spans` に積む（`lines` と同じ借用の span）。答えるのは `VisualTextView` の 1 本で、格子は `GridCells`（`grid_placed_cell`・ADR 0140 決定 4）、text 形は `TextLayoutPort` の同じ layout（ADR 0114 決定 3）。巨大な行の窓（ADR 0139）は 1 画面行なので `start = 行頭・end = 行末・rows = 1`。行の全体を写さない（格子は索引の O(塊)、text は layout の `locate` / `hit`）。
3. **欲しい列（curswant）は画面行の中の仮想桁で、engine が画面行の範囲の中だけを歩いて位置にする。** 行き先の画面行の `[start, end)` を `text_range` で読み、ADR 0034 の表（`column_advance`）で仮想桁を足して、欲しい列に達する Vim の 1 文字（ADR 0053）に着く。画面行の最後の文字を越えない（NORMAL は最後の文字・VISUAL と INSERT の端の扱いは論理行と同じ）。`$` の MAXCOL は画面行の末尾。歩く長さは画面行 1 つ分（格子は `columns` 升・text は折り返した 1 段）で、行の長さに依らない。画面行が 1 つしか無い行（`rows == 1`・巨大な行の窓を含む）への着地は `j` `k` と同じ論理行の道（`offset_at_virtual_column`・索引を持つ本文は索引から O(塊)）で解き、`[start, end)` を `text_range` で読むのは画面行が 2 つ以上の行だけ（レビュー #631 の I-1・差し戻し 2）。
   - 追補（2026-10-17・レビュー #630 の I-2・Q-3）: **欲しい列は `wanted_column`（curswant）から決め、キャレットの実際の桁からは決めない。** `VimRowSpan` に `start_column`（その画面行の始まりの仮想桁・0 始まり・答え手が索引から O(塊) で引く）を足す。画面行の中の桁 `c = clamp(wanted_column − start_column(今のキャレットの画面行), 0, 画面行の幅 − 1)` で、行き先の画面行の `start_column + c` の仮想桁を占める文字に着く。`wanted_column` が無ければ今のキャレットの桁（`j` `k` と同じ `caret_virtual_column`）、行末（`$` の MAXCOL 相当）なら画面行の末尾。着いた後の欲しい列は着いた位置の桁には置き直さず、移った画面行ぶんずらして持ち越す（行き先の始まりの桁 + (欲しい列 − 今の画面行の始まりの桁)）。行の途中の画面行では、その画面行の最後の桁（幅 − 1）に収める。論理行の最後の画面行は短くても保つ（`kept_column`・Vim 9.1 の実測: 170 字の行の 151 桁から `gj` で 170 桁に着き、続く `j` は 191 桁・fixture `wrap-j-after-gj-short-last-row`）。`$` の後は行末の印のまま。Vim 9.1 の `gj` と同じ規則（probe-596・`nv_screengo` の curswant の扱い）。engine は画面行の升の数を持たないので、上の端（幅 − 1）は「画面行の最後の文字を越えない」着地が兼ねる。
   - 追補（Q-3）: この追補は `gj` `gk` には当たらない（`gj` `gk` は NORMAL と VISUAL の motion で INSERT に無い）。INSERT は、その論理行の最後の画面行では行末（最後の文字の後）に着いてよく、それ以外の画面行では最後の文字まで（次の画面行の先頭へ落ちない）。今の形は論理行の INSERT と同じ寄せ（`vim_resting_caret`）で、最後の画面行でも最後の文字に着く。
4. **`gj` `gk` は motion。** `VimKeyTable` に 2 行（`g` の接頭辞の表・ADR 0042 決定 6）、motion の閉じた集合に `display_down` / `display_up` を足す（`default` を書かない・CPP-002）。回数は画面行の数。行の端を越えたら隣の論理行の最後 / 最初の画面行へ続く（`VimLineRows` で隣の行の画面行数を引く）。本文の先頭 / 末尾の画面行で動けなければ失敗（後ろの鍵を打ち切る・`:normal!` と同じ）。欲しい列は決定 3 の追補のとおり、移った画面行ぶんずらして持ち越す（行の途中の画面行では幅 − 1 に収め、最後の画面行では短くても保つ）。オペレータ `d c y` は文字単位 exclusive、VISUAL `v` は inclusive、`V` は行単位。`.` と録画は鍵の列なので変えない。layout が無効（幅 0・`nowrap` の text）なら `j` `k` と同じ（1 行 1 画面行）。
5. **格子では 6 鍵（PgDn / PgUp / Ctrl-f / Ctrl-b / Ctrl-d / Ctrl-u）が画面行の単位。** `VimLayoutView` に閉じた enum `VimScrollUnit { line, row }` を足し、controller は `display() == grid` のとき `row` を渡す（text 形は wrap on でも `line`・Vim 9.1 と同じ巻きのまま）。`row` のとき engine は先頭（`first_visible` の行の `first_row` 番目の画面行）とキャレット（その行の中の画面行）を**画面行の列**として数え、1 画面 = `visible_lines` 行・半画面 = `scroll_lines`（ADR 0019 決定 6 の値・既定は半分）だけ送る。先頭とキャレットを同じ数だけ動かし、欲しい列は保つ（決定 3 の歩き）。端では動ける分だけ動き（画面の先頭が本文の最初の画面行、または最後の画面行が画面の先頭になるまで）、動けなければ失敗にしない（Vim の Ctrl-f と同じく何も変えない）。回数は Ctrl-f / Ctrl-b が画面の数、Ctrl-d / Ctrl-u が `scroll_lines` の置き換え（既存どおり）。NORMAL は選択を畳み VISUAL は anchor を保つ（`VimNavigate` の既存の形）。
   - 追補（2026-10-17・レビュー #630 の Q-1・Q-2・I-3）: **端の動きは鍵ごとに決める。** Ctrl-d / Ctrl-u は Vim の `:help CTRL-D` のとおり、画面が送れない端でもキャレットを `scroll_lines` の画面行だけ動かす（最後 / 最初の画面行で止める）。Ctrl-d の画面は最後の画面行が画面に見えたら止まり（論理行の道 `half_down` の `at_bottom` と同じ）、Ctrl-u の画面は最初の画面行で止まる（`vim_row_half_navigation`）。Ctrl-f / Ctrl-b / PgDn / PgUp は画面が動いた数だけキャレットも動く（D54「同じ数だけ」・端で画面が動けなければ何も変えない・`vim_row_navigation`）。通常モードの PgDn / PgUp はキャレットを常に `visible_lines` の画面行だけ動かし（最後 / 最初の画面行で止める）、画面は動ける分だけ送る。通常モードの画面の送りは ADR 0019 決定 4 の `ScrollExtent::filled_viewport`（末尾の画面を埋める）で止め、最後の 1 行だけが見える所まで送らない（`move_visual` の後の追従が同じ端で収める今の形のまま・契約 `verify_ordinary_page_end`）。Vim は `last_line` のまま。オペレータの後の `d<C-d>` は何もしない（スクロールは motion でない・Vim と同じ）。
6. **`VimNavigate` に `first_row` を足し、`perform` はそれを置く。** `VimNavigate{selection, first_visible, first_row}`。`line` の単位の移動は `first_row = 0`（今日どおり・smoothscroll off の Vim）。`perform(VimNavigate)` は `ScrollState{first_visible, visible_lines, first_row}` を置き、横の送りは今日どおり 0（格子は横に送らない）。追従は呼ばない（engine がキャレットを画面に収めている）。
7. **通常モードの PgDn / PgUp は画面を 1 画面ぶん送り、キャレットも同じ数の画面行だけ動く（すべての文書）。** `move_visual` の page は `VisualTextView::shifted(±visible)` でキャレットを歩いた後、先頭も同じ数の画面行だけ送る（最小追従ではなく、格子は `first_row`、text 形は `first_visible` と `first_row`）。端では本文の最初 / 最後の画面行で止める。普通の文書の通常モードも同じになる（最初の 1 回が 1 画面行しか送られない今日の形をやめる）。通常モードの Ctrl-u / Ctrl-f / Ctrl-b は割り当てないまま（Ctrl-d はブックマーク）。
8. **巨大な行の窓（text 形・いいえ）は変えない。** 巨大な行は 1 画面行（ADR 0139 決定 2）なので、論理行の巻きと画面行の巻きは同じ所へ着く。1 行だけの文書は縦に動く先が無い。横に 1 画面ぶん送る鍵は D54 に無いので足さない（要るなら施主決定）。
9. **検証。** `gj` `gk` は `eng/vim-wrap-oracle.py` が生成する fixture（`tests/vim/wrap-fixtures.json`・ASCII・全角・Tab・`k` の後・`$` の後・端の失敗・`dgj` `vgj` `Vgj`・短い最後の画面行への `vgj`・行より遥かに大きい欲しい列・skipcol の出る高さ）を `ScriptedTextLayouts` で再生する（CNF-010 の SHA）。skipcol の出る高さは 1 本の `:normal!` の中で画面が送られる形（`7gj`・`7gj5gk`）だけを fixture にし、鍵ごとの再描画を要する形（`7gj3gk`）は限界に書いて外す。件数は既存 78 ＋ 新規 46 ＝ 124（既存 78 件は不変）。格子は手書きの契約（`--application`）: 16,384 超の 1 行を格子で開き、PgDn 1 回で `first_row` が `visible_lines` だけ進みキャレットも同じ画面行・升、Ctrl-d は半分、端で止まる、`gj` `gk` が升を保つ、VISUAL で anchor を保つ、`$` の後の `gk` `gj`、論理行をまたぐ複数行の格子、巨大な行の窓（text 形）で `gj` `gk` が `j` `k` と同じに着く、文字組みが無効（幅 0・`nowrap`）なら `j` `k` と同じ。通常モードの PgDn の契約は普通の文書と格子の両方。既存 fixture 1,853 件と wrap の 78 件は変えない（`VimScrollUnit::line` の道は今日のまま）。

## 却下した選択肢

- application が格子のときだけ画面移動を解く（調査の案 a）: Vim の画面移動の計算が engine と application の 2 か所になり（ADR 0019 決定 1 に反する）、`gj` `gk` のオペレータの範囲は engine の中で要るので結局 engine が位置を解く必要がある。
- engine に格子の升の計算（`columns`・`grid_placed_cell`）を持たせて問いを無くす: 折り返した text 形の画面行は DirectWrite が決めるので問いは要る。格子だけ engine で解くと 2 本になる。
- 行の画面行の始まりをすべて答える（`row starts` の配列）: 16 MiB の 1 行で 10 万件を 1 打鍵ごとに作る。問いは 1 画面行の範囲だけ。
- 通常モードの page を格子だけ 1 画面送りにする: `if (grid)` の分岐が 1 つ増え、普通の文書の「最初の 1 回だけ 1 行」が残る。全文書で 1 画面送りにする方が 1 本で、GUI のエディタの普通の動き。
- 窓で横に 1 画面ぶん送る: D54 に無い。施主決定の後。

## 検証の記録

実装の PR に、fixture の件数・契約の名前・`verify-window.py` の該当する節（PgDn の通常モード）を書く。速さ: 格子の 1 打鍵のベンチ（`key-to-frame-single-giant-line-16mib`）に悪化なし（問いは O(塊)）。計測は機械が静かな日に 1 回（今日は不可）。

## 限界

- 複数行の格子で大きな回数（`99<C-f>` など）を打つと、問いの往復は行ごとに 1 回で行数に比例する（回数には比例しない・折り返しの巻きと同じ形）。上限は設けない（追補 2026-10-17・レビュー #630 の I-1・Q-4）。
- `H` `M` `L` と `zt` `zz` `zb` の格子での意味は変えない（論理行のまま・後続）。
- `g0` `g$` `gm` は別 Issue（todo の一覧）。
- text 形の画面行の仮想桁は等幅の近似（proportional な字体では着く位置が Vim とずれ得る）。格子は等幅なので一致する。
- 行末の 1 升に入らず次の画面行へ送った全角の空きを、替え玉 `ScriptedTextLayouts` は升に数えるが `column_advance`（ADR 0034 の表）は数えない。着地は画面行の始まりからの相対なので fixture は一致するが、送った全角の後ろの絶対の欲しい列は送った全角 1 つにつき 1 桁ずれ得る（レビュー #631 の Q-3・未測定）。
- oracle は 1 本の `:normal!` の間に再描画しないので、鍵ごとの skipcol を要する列（`7gj3gk`）は fixture にできない。Nib は鍵ごとに追従する（追補 2026-10-17・#596）。
- Nib の画面の追従は `first_row`（ADR 0114・Vim の smoothscroll に近い）なので、長い行から下の行へ出るとき smoothscroll off の Vim の `topline` と違う。`j` でも同じ差で、fixture から外す（追補 2026-10-17・#596）。
- `gj` `gk` の最初に Vim は欲しい列を「今の行を画面行の幅で埋めた長さ − 1」に切り詰めるが、engine は画面行が 1 つの行の幅を知らない（`VimRowSpan` に幅が無く、text 形は proportional）。200 字 / short / 50 字 / 200 字の 4 行で 1 行目の 150 桁から `j2gjj` を打つと Vim は 4 行目の 80 桁、Nib は 190 桁に着く（証跡 `D:/NeNeNib/evidence/596-wrap-oracle-7`）。直すには幅を engine に渡す設計が要るので後続とし、この fixture は json から外してある（2026-10-17・差し戻し 2）。
- 画面行が 1 つの行の着地が行を写さないこと（決定 2）は振る舞いの契約（`j` `k` と同じ・1 行なら失敗）だけで守り、読んだバイト数は数えていない（`text_range` に読んだ量を返す口が無い）。数える口を足すかは後続。
