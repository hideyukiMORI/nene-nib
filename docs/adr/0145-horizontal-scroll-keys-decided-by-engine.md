# ADR 0145 — 窓と wrap off の横送り `zl` `zh` `zL` `zH` `zs` `ze` は engine が決め、application は幅と横位置を問いで答える

- 状態: 提案（設計席 2026-10-17。施主決定 D56。merge は hide の試用の後・D41）
- 日付: 2026-10-17
- Issue: #634
- 規則: ARC-001 / ARC-004 / ARC-011 / CPP-002 / CPP-012、ADR 0019 / 0034 / 0042 / 0053 / 0114 / 0129 / 0139 / 0140 / 0144
- 調査: `D:/NeNeNib/outputs/20261017/survey-634.md`（横位置の持ち主・窓の錨・engine が知らないもの）・`probe-634.md`（Vim 9.1 の実測）

## 文脈

巨大な行の窓（表示形式「いいえ」・ADR 0139）は 1 画面行なので、D54 の縦の 6 鍵では動けない。Vim 9.1 も 1 行だけの巨大な行を縦には送れず（`probe-d54-page.md`）、`nowrap` では横送りの鍵 `zl` `zh` `zL` `zH` `zs` `ze` で見る所を動かす。hide の決定（2026-10-17・D56）: この 6 鍵を Nib に足す。PgDn / PgUp を横に読み替えない（D55 は縦の 1 画面送りのまま）。

今の Nib（survey-634）:

- 横位置は `ScrollState::horizontal`（物理画素の float）の 1 つで、新しい値を書くのは追従 `VisualTextView::horizontal_for` の 1 か所（最小追従・余白は幅 − 3 px・通常モードと Vim で同じ）。巨大な行はキャレットの窓の始まり `origin_` からの相対で、`giant_horizontal` が窓をまたぐ読み替えをする（ADR 0139 決定 5）。
- engine（`VimViewport` / `VimLayoutView` / `VimRowRequest` / `VimRowSpan` / `VimNavigate`）は画面の幅（升の数）も今の横位置（leftcol）も知らない。`z` の接頭辞の表も無い。wrap off の text 形では `VimLayoutView.enabled = false` で `row_request` の往復が起きない。
- `perform(VimNavigate)` は horizontal を 0 に戻し追従を呼ばない（ADR 0144 決定 6）。wrap off でキャレットが右の方にあるとき Ctrl-f 等でキャレットが画面の外に残り得る（コード読み・未実測）。
- 通常モードに横送りの鍵・Shift+ホイール・`WM_MOUSEHWHEEL` は無い（共用先は無い）。

Vim 9.1 の実測（probe-634・`--not-a-term`・`nowrap`・`columns=40`・`sidescrolloff=0`）:

- `zl` / `zh`: leftcol を ±N（回数）。下限 0、上限は**キャレットの行の最後の文字の最初の升**（leftcol 0 始まりで `vcol(最後の文字) − 1`・最後の文字が左端に残る所まで）。画面の外に出たキャレットは端の文字へ押され（左は leftcol 以上で始まる最初の文字・右は `leftcol + columns − 1` までに収まる最後の文字）、押されたときだけ欲しい列（curswant）を押された位置の仮想桁に置き直す。押されなければキャレットも欲しい列も変わらない。
- `zL` / `zH`: 半画面（`columns / 2`）× 回数（`2zL` は 1 画面）。clamp と押しは `zl` `zh` と同じ。
- `zs`: leftcol = キャレットの文字の最初の升（`vcol − 1`）。`ze`: leftcol = キャレットの文字の最後の升 − `columns`（0 未満は 0）。キャレットは動かない。回数は無視。
- 端（leftcol 0 で `zh` `zH`・上限で `zl`）は**失敗にしない**（後ろの鍵は打ち切られない。対照の `0hx` は打ち切られる）。VISUAL の `vzl` は選択の端（キャレット）を同じく押す。
- 全角は升で数え（ADR 0034）、左端で半分欠けた全角は見えない扱いで、押し先はその次の文字。
- 追従は Vim 既定 `sidescroll=0` が「外れたら半画面送って中央」、Nib は最小追従（Vim の `sidescroll=1` に近い）。`$` のような大きな飛びは Vim がどの `sidescroll` でも中央へ寄せる。

## 決定

1. **横送りも engine が決める。application は「画面の幅」と「今の横位置」を問いで答え、engine が返した横位置を置くだけ。** ADR 0019 決定 1（画面移動の計算は engine）と ADR 0144 決定 1（2 本目の経路を作らない・ARC-001）を保つ。application に Vim の横送りの計算を持たせない。ADR 0019 決定 9 の「横スクロールは範囲外」はここで外す。
2. **`VimViewport` に `columns` と `left_column` を足す。** `columns` は本文の欄に入る升の数（`std::size_t`・幅 ÷ 1 升の幅・text 形は `giant_line_window` と同じ平均の字幅で割る・格子は `GridCells` の `columns`）。**横に送らない文書では `columns = 0`**（wrap on の普通の文書・格子・ADR 0140 決定 7）。`left_column` は今の横位置を升で数えた leftcol（0 始まり・キャレットの行の仮想桁）。controller は `viewport_request` の往復（ADR 0129）で一緒に答える。`columns == 0` のとき engine は 6 鍵を**何もしない**（失敗にもしない・Vim の `wrap` と同じ）。
3. **`VimRowSpan` に `width` と `column` を足す。** `width` はその画面行の升の数（`end_column − start_column`・巨大な行の窓は行の全体の仮想桁・`VirtualColumnIndex` から引き、行を写さない）、`column` は問いの `at` の仮想桁（`row` で問うたときは `start_column`）。engine はこれで「キャレットの升」「行の最後の文字の最初の升」を行を歩かずに得る（最後の文字は `vim_character_start(end)` の 1 文字だけ読み、`column_advance` で幅を引く）。`width` は ADR 0144 の限界「`gj` の前の欲しい列の切り詰めは engine に画面行の幅が要る」の穴も同じ値で塞ぐ（切り詰めの実装はこの Issue の範囲外・fixture `j2gjj` の復帰は後続）。
4. **wrap off の text 形では `VimLayoutView.enabled` は今日どおり false のまま（`gj` `gk` と縦の 6 鍵は `j` `k` と論理行の巻き・Vim 9.1 の nowrap と同じ）。`row_request` は `enabled` と独立で、engine は `columns > 0` のとき横送りのために立て、controller は常に答える。** 既存の fixture 1,853 件と wrap fixture 124 件は不変であること（改訂 2026-10-17・実装で `enabled = true` が nowrap の `dgj` を文字単位に変え、既存の契約 `verify_unlaid_display_moves` と Vim 9.1 に反すると分かったため）。
5. **`z` の接頭辞の表 `z_bindings` を `VimKeyTable` に足し（ADR 0042 決定 6）、6 鍵は `VimAction` の閉じた集合の新しい値（motion ではなく画面の操作・`default` を書かない・CPP-002）。** `zt` `zz` `zb` 等は足さない（表の形だけ共有・後続）。回数は `zl` `zh` が升の数、`zL` `zH` が半画面の数、`zs` `ze` は無視。オペレータ待ち（`dzl`）と VISUAL の扱いは Vim 9.1 の oracle の結果に従う（VISUAL はキャレット側の端を押す）。
6. **engine の計算は上の実測のとおり。** 新しい leftcol を `[0, 最後の文字の最初の升]` に収め、キャレットの文字の升が `[leftcol, leftcol + columns)` に収まらなければ端の文字へ押す（左は `offset_at_virtual_column(leftcol)` の文字がそこで始まらなければ次の文字・右は `offset_at_virtual_column(leftcol + columns − 1)` の文字がそこで終わらなければ前の文字・ADR 0053 の 1 文字）。押したときだけ `wanted_column` を押された位置の仮想桁に置く。端で動けなくても失敗にしない。行を写さない（`offset_at_virtual_column` と 1 文字の読みだけ）。
7. **`VimNavigate` に `first_column`（`std::optional<std::size_t>`）を足す。** 横送りの 6 鍵は `first_column` に新しい leftcol を入れ、`first_visible` と `first_row` は今の値のまま返す。縦の移動（6 鍵・`H M L` 等）は `std::nullopt`。
8. **`perform(VimNavigate)` は `first_column` があれば横位置を置き、無ければ今の横位置を保って横の追従を頼む。** 置くのは `VisualTextView` の 1 対の関数 `horizontal_of_column(line, column)` / `column_of_horizontal(line, horizontal)`（互いに逆・巨大な行は `window_x` と `origin_` で、普通の wrap off の行は同じ layout の `locate` / `hit`・ADR 0114 決定 3）。往復で値が変わらないことを契約にする（`zl` の繰り返しで横位置が揺れない）。`first_column` があるときは追従を呼ばない（engine がキャレットを画面に収めている・ADR 0144 決定 6 と同じ）。`std::nullopt` のときは **0 に戻さず今の値を保ち**、縦の移動の後に `horizontal_for` の最小追従を 1 回頼む（ADR 0144 決定 6 の「横の送りは今日どおり 0」をこれに置き換える・Ctrl-f でキャレットが画面の外に残る穴も塞がる）。追従の最小の規則は変えない（`sidescroll=0` の半画面送りに揃えない）。
9. **巨大な行の窓では、押されたキャレットが錨の刻み（2,048）をまたいでも横位置は新しい `origin_` で計り直す。** 決定 8 の対の関数が `origin_` を引数に持つので、`giant_horizontal` の隣の窓の読み替えに頼らない。キャレットは常に画面の中なので、窓の外（前後 4,096 の外）が空白になる横位置は作れない（契約で確かめる）。
10. **fixture は幅つきの oracle `eng/vim-wrap-oracle.py` に `nowrap` の形を足す。** viewport の入力に `columns` と `leftcol`、snapshot に `leftcol`（`winsaveview().leftcol`）。Vim 側は `set nowrap sidescroll=1 sidescrolloff=0`（Nib の最小追従に近い値）。追従の差（大きな飛びで Vim が中央へ寄せる）が混ざる鍵列は fixture にしない（生成で合わない件は証跡を残して除く・ADR 0144 の `j2gjj` と同じ扱い）。`"+` `"*` は使わない。
11. **通常モード（Vim でない）の横送りの鍵と Shift+ホイールは足さない。** Vim に対応物が無く、要るなら別の施主決定。

## 影響

- core: `VimViewport`（2 欄）・`VimRowSpan`（2 欄）・`VimNavigate`（1 欄）・`VimAction`（6 値と `prefix_z`）・`VimKeyTable`（表 1 つ）・横送りの計算（新しい 1 ファイル・1 型・CPP-011）。
- application: `EditorView` の viewport の答え（`columns` `left_column`）と `enabled`・`VisualTextView::row_span` の `width` `column`・対の関数・`perform(VimNavigate)`。
- tests: wrap fixture（`nowrap` の節）・契約（`--application`: 往復の不変・窓の外に出ない・`columns == 0` で何もしない・`nullopt` で横位置を保って追従）。
- 文書: SPECIFICATION の D56・`docs/adr/README.md`・ADR 0144 決定 6 と限界の追記（`width` が入った）・`docs/todo/current.md`。

## 限界

- text 形の `columns` と `left_column` は平均の字幅の近似（proportional では升が画素と一致しない・ADR 0144 の限界と同じ）。等幅の字体では一致する。押しの判定は升で行い、置いた横位置に対して追従は呼ばないので、proportional で押された文字が数画素はみ出ることはあり得る。
- `gj` の前の欲しい列の切り詰め（ADR 0144 の限界）は `width` が入ったが、実装はこの Issue に含めない。
- 通常モードの横送りと横ホイールは無いまま（決定 11）。
- `enabled` と `columns` は別の問い（折り返すか・横に送るか）。格子と wrap on の普通の行は columns 0、wrap off の text 形は enabled false。両方 true になるのは wrap on の文書で巨大な行の窓（text 形）にキャレットがあるときだけで、窓は 1 画面行なので `gj` `gk` は `j` `k` と同じ所に着く（ADR 0144 決定 8）。
