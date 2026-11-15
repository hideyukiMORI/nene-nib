# ADR 0018 — VISUAL は `VimMode` の 2 値で、engine は `Selection` を読み、選択を範囲に変える純関数 1 つでオペレータの経路に流す

- 状態: 受理
- 日付: 2026-09-19
- Issue: #53
- 影響する規則: ARC-001 / ARC-004 / ARC-007 / CPP-002 / CPP-003 / CPP-006 / CPP-011 / CPP-012 / QLT-009 / QLT-013

## 文脈

ADR 0012 は VISUAL を「`v` の範囲の規則（inclusive）と `Ctrl-v` の全角・タブが難所」として先送りし、ADR 0015（#43）で `vim_step` は「範囲を決める」（`motion_range` → `VimMotionRange{range, linewise}`）と「効果にする」（`removed` / `changed` / `yanked`）に分かれた。
選択は `EditorState` が `Selection{anchor, caret}` として所有し（ADR 0009 の決定 4・ARC-004）、通常モードの選択は `[anchor, caret)` の排他で、描画は `span_of` が `selection_range` から作る。
Vim の VISUAL は anchor と caret の**両端の文字を含む**（`v`）か**行まるごと**（`V`）で、キャレットは EOL の上にも置ける（`v$`）。制約: `VimState` は本文・キャレット・履歴を持たない（ADR 0012 の決定 1）。

## 決定

**VISUAL は `VimMode` に `visual` / `visual_line` の 2 値を足して表す。engine の入口は `Selection` を読み、効果 `VimSelect{Selection}` を 1 つ足す。選択 → 範囲は純関数 `vim_visual_range` の 1 本で、表示・Ctrl+C・`d x y c` が同じ範囲を使う。**

1. **`VimMode { normal, insert, visual, visual_line }`**。`mode_label` は `VISUAL` / `VISUAL LINE`、`CaretShape` はブロック、`follow_ime`（ADR 0014）は NORMAL と同じく IME を切る。閉じた enum に 2 値足すので、`switch` の写し先が足りない所はコンパイルが落ちる（CPP-002）
2. **`vim_step(const VimState&, const TextBuffer&, const Selection&, VimKey)`**。NORMAL / INSERT では anchor == caret で、engine は caret だけを読む。VISUAL では anchor を範囲の片端として読む。anchor を `VimState` に持たせない（選択の所有は `EditorState`・ARC-004）
3. **`VimSelect{Selection}`** を `VimEffect` に足す（9 → 10）。`v` `V` で入る（anchor = caret）・VISUAL の移動（anchor を保って caret を動かす）・`o`（入れ替え）が返す。controller は `EditorState` の選択をそのまま置き換える。VISUAL から出る（Esc・同じ鍵）は `VimMoveTo{caret}` で畳む（既存の `collapse`）
4. **`vim_visual_range(const TextBuffer&, const Selection&, VimMode) → VimMotionRange`**（core・純関数）: `visual` は小さいほうの位置から大きいほうの位置の**次の code point まで**（大きいほうが EOL の上なら改行を含む）、`visual_line` は小さいほうの行の行頭から大きいほうの行の終端まで（`linewise`）。NORMAL / INSERT で呼ぶと空の範囲（`switch` で網羅）
5. **表示と操作は同じ範囲**: controller の `span_of`（選択の描画）と Ctrl+C / Ctrl+X は VISUAL の間 `vim_visual_range` を使い、通常モードは `selection_range` のまま。`d x y c` は `vim_visual_range` の結果を #43 の `removed` / `changed` / `yanked` に流す（範囲 → 効果の経路は 1 本・ARC-001）。レジスタの種類は `visual` → `characters`・`visual_line` → `lines`。`c` は削除して INSERT（undo の単位は ADR 0015 の決定 5 のまま）
6. **キャレットの寄せ**: `settle_vim_caret` は VISUAL では寄せない（EOL の上に置ける）。VISUAL から NORMAL へ戻るときに寄せる（既存の経路）
7. **VISUAL で効く鍵**: 移動 `h j k l 0 $ w b e ^`・Home / End・矢印・回数・`o`・`d x y c`・Esc・`v` / `V`（同じ鍵で出る・違う鍵で種類を切り替える）。**効かない鍵は何もしない**（`u U ~ > < J p r I A gv` はこの縦切りでは無い。Vim の `u` は VISUAL では小文字化なので undo に流さない）。**`v` `V` の前の回数は Vim と同じく選ぶ量になる**（`3v` は 3 文字・`3V` は 3 行。`:help v` / `:help V`。2026-09-19 の改訂: 草稿は「回数を捨てる」だったが oracle と `:help` で誤りと分かった）
8. **やらないこと**: ~~`Ctrl-v`（矩形。`virtcol` が要る）~~ → **[ADR 0035](0035-vim-visual-block-as-column-ranges.md)（Issue #112）で `VimMode::visual_block` として入った。行ごとの範囲は `vim_block_range` が決め、`vim_visual_range` は文字単位と行単位のままである**・マウスのドラッグで VISUAL（FR-003。anchor が `EditorState` にあるので、ドラッグが選択を置いて `visual` に切り替える形で足せる）・画面の高さが要る移動（PgUp / PgDn・`Ctrl-d/u`・`H M L`。engine に見えている行数を渡す形と、oracle の `-es` で `lines` が効くかの実測が先）

## 強制

- fixture の再生（ADR 0005 / 0012）— **active**（`nib_unit`。VISUAL の fixture を 61 件足して 195 → 256 件。fixture は NORMAL で終わる規約のまま）
- CPP-002: `VimMode` の `switch` に `default` を書かない・`std::visit` の写し先を欠かさない — **active**
- 表示の範囲と操作の範囲が同じ — 単体テスト（`vim_visual_range` の 4 形と、VISUAL の `EditorFrame` の選択スパンが `vim_visual_range` と一致すること）
- ARC-007 / CPP-013: core から時刻・OS のシンボルが出ない — **active**

## 結果

得られるもの: `v` `V` で選んで `d x y c`。ドラッグで VISUAL（FR-003）は選択を置いてモードを切り替えるだけで足せる。~~`Ctrl-v` は `vim_visual_range` に矩形の形を 1 つ足す位置が決まった~~ → 矩形は 1 つの範囲では表せないので、ADR 0035 は `vim_block_range` を別の 1 本として足した（`vim_visual_range` は変えていない）。
失うもの: engine の入口の型が変わる（`Offset` → `Selection`）ので、#43 までの呼び出し（controller とテストの再生）は全部書き換える。`p` `~` `>` `<` `J` は VISUAL で効かない（何もしない）。
正直に: `v$` の改行の扱い・`vo` のあとのキャレット・行単位で `j` `k` の欲しい列・空行の `v` は oracle の結果に合わせる（実装の前に規則を書き切らない）。VISUAL の終わりの位置（`'<` `'>`）は fixture では見ない（`d` `y` の結果で範囲を確かめる）。

### oracle が決めたこと（Issue #53 の実測・2026-09-19）

fixture 61 件を Vim 9.1 に測らせて、規則は実装の前ではなく答えのほうから決まった。

1. **VISUAL のキャレットは行の内容の終わり（Vim が NUL を置く桁）に載る。** `v$d` は改行まで消して次の行と繋がり（`"abc\ndef"` → `"def"`・レジスタは `"abc\n"` の文字単位）、`vl` も行末に届く（`"abc\ndef"` に `llvld` で `"abdef"`）。Vim の `coladvance` の `one_more` が `VIsual_active` で立つのと同じで、決定 6 のとおり寄せるのは NORMAL へ戻るときだけ。空行の `v` は改行 1 つを選ぶ
2. **`op_delete` の「奇妙な Vi の振る舞い」は VISUAL には掛からない**（Vim の条件が `!oap->is_VIsual`）。`"  abc\n   "` の `vjd` は行単位にならず `"  abc\n "` を文字単位で消す。`d` ＋移動の経路（`whole_lines_for_delete`）とは別の入口を用意した
3. **決定 7 の「回数は VISUAL に入る前のものを捨てる」は Vim と違う。** `:help v` は「前の選択が無ければ `[count]` 文字を選ぶ。カーソルを右へ N 個ぶん動かすのと同じで、`'selection'` が `"exclusive"` でなければ 1 つ少ない」、`:help V` は同じく「`[count]` 行を選ぶ」と書いており、実測も `3v` が 3 文字・`3V` が 3 行だった。**実装は Vim に合わせて、入った直後に `count - 1` だけ `v` は右へ・`V` は下へ動かす**（ADR 0005 の「本物の Vim が正」）。決定 7 の括弧は同日に改訂した（上）
4. `V` の中の `$` `0` はキャレットだけ動いて範囲は行単位のまま・`Vy` のあとのキャレットは最初の行の同じ桁（最初の非空白ではない）・`vy` のあとは範囲の先頭、はどれも草稿どおりだった

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `VimState` に anchor を持つ | 選択の正本が 2 つになる（ARC-004）。`EditorState` の `Selection` がそのまま anchor を持てる |
| `VimMode::visual` 1 値 ＋ `VimState::visual_kind` | 2 つの状態が矛盾しうる（`visual_kind` が `normal` で残る）。enum の 2 値なら `switch` の網羅で機械が守る |
| VISUAL の移動を `VimMoveTo` にして controller が「次のモードが VISUAL なら anchor を保つ」 | anchoring がモードから暗黙に決まり、`o`（入れ替え）には別の効果が要る。`VimSelect` 1 つで移動・入れ替え・入るを同じ形にする |
| 選択を排他 `[anchor, caret+1)` に置き換えて `selection_range` をそのまま使う | 描くキャレットが 1 つ右にずれる。位置は本当の位置のまま持ち、範囲だけを関数で決める |
| `x` を VISUAL では `d` の別名として表に書かず、`d` に写す | 表の行として `x` → `remove_character` を VISUAL では `d` と同じ動作に引くほうが、表 1 つで NORMAL と VISUAL の違いが読める |

## 希望列の寿命の補正（Issue #77・2026-09-20）

固定Vimで開始・種類切替・回数・終了を実測した。`v/V`は元の希望列を保つ。`2v`以上で
文字選択が実際に横へ広がる場合だけ、希望列を到達列へ更新する。`1v`は元の希望列を保ち、回数付きVは
元の希望列で到達行の位置を求める。`v↔V`の切替と同じキーによる終了も元の希望列を保つ。
空行の横移動や最終行からの縦移動が成立しないときは、現在位置と希望列の両方を保つ。
Escによる終了は現在列へ戻す既存の動作を維持する。この違いをモード切替の既存関数で扱い、
`VimState::wanted_column`以外の記憶や新しい効果を追加しない。

実測は `out/issue77-oracle/`、既存のg取消比較は `out/line-jump-oracle/continuation-probe-result.json`。
入力・型・所有の設計変更ではなく、決定3/7のVim既定動作との一致を回復する修正である。

## 行単位yankの戻り位置の補正（Issue #81・2026-09-21）

上記#53の「最初の行の同じ桁」は実測範囲が狭く、一般化が誤っていた。固定Vimの追加実測では、
行単位VISUALのyankは下向き・単一行なら範囲先頭の列1へ戻り、上向きの複数行選択なら現在の列に留まる。
同じ行の中で左へ動いた場合も列1である。通常モードに戻る際の行末の寄せは既存の共通経路を使う。
字下げ・タブ・空行・$・回数・oによる端点交換・v/V切替を測り、startoflineの有無で
単一行/下向き/上向きの戻り位置が変わらないことも分割normalで確認した。

正典は既存のyanked_caret。行単位VISUALのときだけ、現在行が範囲の最終行なら先頭位置、
それより前なら現在位置を返す。現在位置は選択の片端なので、単一行と方向を追加状態なしで判定できる。
NORMALの行単位yankと文字単位yankは既存動作を維持する。レジスタ本文・種類・履歴・保存schemaは不変。
証拠は `out/issue81-oracle/`。#77で分離した1件は入力/結果不変で再利用し、他22件だけを新規測定する。

### 後続の文字置換（Issue #84・2026-09-21）

決定7で後続としたrは [ADR 0029](0029-vim-character-replace-as-range-edit.md) で接続した。
文字/行単位VISUALの範囲は引き続きvim_visual_rangeが正で、文字だけを置換して改行を残す。
通常のUnicode文字とTabが対象。VISUAL r<Enter>のliteral CRはIssue #85で扱う。

### 端の行の j k（Issue #730・2026-10-20）

VISUALの `j` `k` は、Vimの cursor_down / cursor_up と同じく最終行・最初の行にいるときだけ失敗し（`landing_of` の `vertical_landing`）、動いたかどうかの位置では決めない。欲しい列が `$` の「行末」だと、最終行の `j` も決定6の行の内容の終わりへ動いてしまい、`$vjd` が失敗せずに後ろの `d` を流していたため。

## 関連

ADR 0009（選択と undo）・ADR 0012（engine の形）・ADR 0014（IME と Vim のモード）・ADR 0015（範囲 → 効果・レジスタの種類）・採用案 `docs/design/2026-09-15-editing-look.md`（D15・選択の描き方）。

## 追補（2026-10-29・#1039）: `gv` と前の選択

- **前の選択は文書の値。** `VimLastVisual{mode, anchor, caret, wanted}`（種類・始めの端・キャレットの端・欲しい列）を `VimMarks.last_visual` に持つ（Vim の `b_visual` と同じく文書ごとに 1 つ・ADR 0152 の形）。端は行と桁（`VimMarkPosition`）で、矩形の `$` と文字単位の `$` は欲しい列の「行末」で残る（`l<C-v>j$<Esc>Ggvd` と `lv$<Esc>0gvjd` を実測）。
- **置くのは 1 か所。** `vim_step` の `visual_remembered` が、鍵の前が VISUAL で後が VISUAL でない（Esc・オペレータ・`c` で INSERT へ・`p` など）すべての鍵で、鍵の前の選択と種類と欲しい列を `VimStep.marks` に添える。抜け方ごとの関数には書かない。
- **VISUAL の中の `gv` も前の選択を書く。** 入れ替え（`visual_exchanged`）は VISUAL を抜けないので `visual_remembered` を通らず、今の選択を前の選択として `VimStep.marks` に添える。前の選択を書くのはこの 2 か所だけ。
- **`.` の VISUAL の再生は置かない。** Vim の `redo_VIsual_busy` と同じく、`.` が選び直した選択は前の選択を置き換えない（`vlxj0.gggvy` は 1 行目の 2 文字を選ぶ・実測）。`.` の 1 か所が `VimState.redoing_visual` を立て、`visual_remembered` が再生の鍵（`VimKeySource::replayed`）が VISUAL にいるあいだだけ持ち越す。マクロ（`@`）の中の VISUAL は打った鍵と同じく置く。
- **`gv`。** NORMAL では前の選択を種類・両端・欲しい列ごと選び直す。VISUAL では今の選択と前の選択を入れ替え（`vl<Esc>jjvjgvd` は前の選択を消し、`gv` を 2 度で元へ戻る・`V` の中の `gv` は前の種類になる）、前の選択が無ければ失敗して今の選択のまま残る。前の選択が無い・始めの端の行が本文の外（最終行の `dd` の後）なら失敗（Vim の `nv_gv`）。キャレットの端の行は最後の行へ寄せ、桁はどちらも行の内容の終わりまで寄せる。オペレータの後ろの `gv`（`dgv`）は失敗。回数は使わない。
- **編集の行のずれ。** `marks_adjusted` が両端を Vim の `one_adjust_nodel` と同じに動かす。消えた行の端は消さずに消えた範囲の最初の行へ寄せ（`vl<Esc>ddgvy` は新しい 1 行目を選ぶ）、文字単位の削除でつながった行の端は最初の行へ桁を足してつなぐ（`lvjld0gvy` は `jkl` を選ぶ）。近似の限界は ADR 0152 の決定 6 と同じ。
- **やらないこと。** `'<` `'>` のマークの読み（`` `< `` `'>` など）・`gv` の後の `:` の範囲・クリックで VISUAL を抜けたとき（controller の割り込み）の前の選択は後続。
- fixture は `gv-gi-` の 19 件（`gv` の側）。

## 追補（2026-11-10・#1361）: 矩形のオペレータの後の前の選択

- **前の選択は操作の前の選択のまま。** Vim は `do_pending_operator`（ops.c）の `VIsual_active` の枝で、オペレータを流す前に `b_visual` の `vi_start`（`VIsual`）・`vi_end`（カーソル）・`vi_mode`・`vi_curswant` を置き、`op_shift` `op_change` `op_insert` `op_delete` `op_replace` `op_tilde` の矩形の道はどれも `b_visual` も `b_op_start` も書き直さない（`gv` が読むのは `nv_gv_cmd`（normal.c）の `b_visual` だけ・`b_op_start` へ寄せるのは VISUAL の `p` の `nv_put_opt` だけ）。Nib も `visual_remembered` の 1 か所が鍵の前の選択を置く形のままで、`c` `C` `I` `A` `>` `<` `r` `d` `y` `~` の後で両端を書き直さない。
- **矩形の編集はマークを動かさない。** 矩形のオペレータは行の中だけを書き換えて `mark_adjust` を呼ばない（行のずれが無い・桁も動かさない・`mark_col_adjust` を呼ぶのは `do_join` だけ）。controller の `apply_block` は行ごとの置き換えを上から下までを覆う 1 つの Edit に畳んで履歴へ積む（ADR 0035 の決定 3）が、その畳んだ Edit を `marks_adjusted` に通すと改行を含む置き換えとして行が消えた形に読まれ、前の選択の端と名前つきのマークが 1 行目へ寄っていた（`gg<C-v>jj>gv<Esc>` が 1 行目・`Gma<C-v>ggl>'a` が 1 行目）。`replace` に行ごとの置き換えを渡し、マークは core の `marks_block_adjusted`（行ごとの置き換えを下の行から `marks_adjusted` に通す・ADR 0152 の決定 6 の追補）で直す。改行を含まない行の置き換えはマークを動かさず、本文の終わりに行を足す矩形の貼付だけが行を足す。
- **矩形の `C` の欲しい列は行末。** Vim の `v_visop`（normal.c）は矩形の `C` と `D` で `curwin->w_curswant = MAXCOL` にしてから `c` を流すので、`do_pending_operator` が置く `vi_curswant` は MAXCOL（`ggl<C-v>jjCXY<Esc>gvd` は行末まで消す・実測）。`visual_remembered` は矩形の `C`（引数を待つ鍵の文字を除く）の前の選択の欲しい列を行末の印にする。矩形の `D` も同じ（#1417 で入った・下の追補）。
- **`$` の矩形の後の `>`。** `gg<C-v>$G>gvd` は行末まで選び直さず左の 1 桁（Tab）だけを消す（実測）。`G` が欲しい列を桁へ戻すので、`$G<Esc>gvd`（既存の振る舞いと一致）と同じく鍵の前の欲しい列のまま置けば Vim と一致する。
- **測った（変えない・後続）。** 行単位と文字単位の行を保つ書き換え（`Vj>` `Vj~` `vjl~` `vjlr`）の後の `gv` と `'a`、矩形の `>` と `d` の後の `u` の後の `gv` も、Vim はマークを動かさないが Nib は 1 行目へ寄せる（同じ根: 改行を含む 1 つの置き換えを `marks_adjusted` が行の消えた形に読む）。直し方は設計の判断（後続の Issue）。文字単位の `vjcX<Esc>gvd` は一致。
- fixture は `gv-after-block-` の 22 件（修正前の再生器で 21 件が落ちる・`char-change` は文字単位の一致の確認）。

## 追補（2026-11-11・#1417）: VISUAL の `J` `gJ` `X` `D` `Y`

決定 7 の「効かない鍵」のうち、`J` `gJ` `X` `D` `Y` を Vim 9.1 の実測（`-es -u NONE`・下ごしらえの probe 96 件と fixture 93 件）どおりに入れる。`p` `P` は #1418、文字単位と行単位の `C`（`vjC` は行単位の変更・実測）は後続。

- **`J` `gJ`（Vim の `nv_join` は VISUAL では `nv_operator` へ流す・`OP_JOIN`）。** 選んだ行の最初から最後までをつなぐ。種類（文字単位・行単位・矩形）を問わず、1 行だけなら次の行までの 2 行。回数は使わない（`V2J` `V3J` は 2 行・`3VJ` は `V` の回数で 3 行を選ぶ）。2 行目が本文の外なら鳴らして VISUAL を抜け、選択の始めに残る（`GVJ`）。つなぐ本体は NORMAL の `J` `gJ` と同じ `lines_joined`（空白の規則・joinspaces・`)`・キャレットはそのまま）で、NORMAL の `joined` は回数から、VISUAL の `visual_joined` は選択の行から最初と最後の行を決めるだけ。レジスタは書かない（`"aJ` の名前も使わない）。
- **`J` の u の戻り先は VISUAL の `d` と同じ `visual_restore`。** 行単位は始めの端が上の行なら行頭、下の行ならキャレット（`lVjJu` は 1,1・`jlVkJu` は 1,2）、文字単位は小さいほうの位置（`lvjgJu` は 1,2）。
- **`X` `D` `Y`（Vim の `v_visop`）。** 文字単位と行単位は種類を行単位に読み替えて VISUAL の `d` `y` へ流す（`visual_operated` に種類だけ `visual_line` にした状態を渡す・範囲・レジスタ・`"1` の規則・キャレット・u の戻り先は `Vd` `Vy` と同じ道）。前の選択は鍵の前の種類のまま（`lvjYgv<Esc>` は文字単位の 2,2・Vim の `VIsual_mode_orig`）。矩形の `X` `Y` は矩形の `d` `y`（`block_operated`）、矩形の `D` は `C` と同じく行末まで取り直して消す（`line_end_block_of`・`block_removed_to_line_end`）。矩形の `D` の後の前の選択は `C` と同じく欲しい列が行末（`visual_left_state`）。回数は使わない（`v2X` `v3Y` は選んだ行だけ）。
- **`.`（ADR 0033）。** 記録は既存の `visual_recorded` のまま（範囲の大きさは鍵の前の種類で測り、再生は選び直してから同じ鍵を流す）。`vjX.` は 2 行ずつ、`l<C-v>jDj.` は記録の矩形の左上から行末まで、`Y` は本文を変えないので記録しない（`l<C-v>jYjj.` は何もしない）。
- **測った（変えない・後続）。** 3 行以上をつなぐ `J` の中の行のマーク（`jlmaggVjjJ`a` は Vim 1,5・Nib はマークが消えて `` `a `` が効かず 1,6 に残る）は NORMAL の `3J` と同じ根（PR #1408 のレビューの後続 1・#793）。
- fixture は `visual-line-command-` の 93 件（修正前の再生器で 88 件が落ちる。通る 5 件は `GVJ` と `J` `gJ` の後の `u`＝本文が戻るだけの形）。

## 追補（2026-11-11・#1436）: VISUAL の `C` `R` `s` `I` `A`

決定 7 の「効かない鍵」のうち、文字単位と行単位の `C` `I` `A`、全部の種類の `R`、文字単位と矩形の `s` を Vim 9.1 の実測（`-es -u NONE`・下ごしらえの probe 約 200 件と fixture 137 件）と source（normal.c の `v_visop` `nv_subst` `nv_Replace` `nv_edit`・ops.c の `do_pending_operator` `op_insert` `get_op_vcol`）どおりに入れる。どれも種類だけを読み替えて既存の `c` と INSERT の入口（`entered_insert`・NORMAL の `i a I A` と同じ）へ流し、第 2 の経路は作らない。

- **鍵 → 動作。** `s` と `R` は鍵の表に `VimAction::substitute` と `VimAction::replace_mode` の 2 値を足す（群は `edit_line`）。NORMAL の `s`（`cl`）と REPLACE モードの `R` は未実装のままで、表に無い鍵と同じに打ち消す（`U` の先例・ADR 0015 の決定 10）。
- **読み替えの 1 本 `operated_mode`。** 大文字の `X` `D` `Y` `C` `I` `A` は矩形でなければ行単位、`R` は矩形も行単位（`nv_Replace` は種類を問わず `'V'`）、`s` は種類のまま（`nv_subst`・矩形の `s` は矩形の `c`）。前の選択（`gv`）は鍵の前の種類のまま（`VIsual_mode_orig`・`l<C-v>jRX<Esc>gv` は矩形）。回数は使わない（`v2C` `v3R` `v2s`）。
- **`C` `R` `s`。** `C` `R` は行単位の VISUAL の `c`（`visual_linewise_changed`・範囲・レジスタ・`"1` の規則・u の戻り先は `Vc` と同じ道）、`s` は `visual_changed`。
- **`I` `A` の位置（`do_pending_operator` と `op_insert`）。** 範囲の両端は「選択の始めの端（`VIsual`）の桁を 0 にしてからカーソルと並べる」（`oap->start = VIsual; if 'V' start.col = 0`）。`I` は始まり（`jlvkIX` はカーソルの 2 桁目・`lvjIX` は 1 行目の頭）、`A` は終わりを行の内容の中へ寄せ（`check_cursor_col`）、行が空でなく `oap->start_vcol != oap->end_vcol` のときだけ終わりの文字の後ろ（`inc_cursor`）。ほかの行へは写さない（矩形の `I` `A` は従来どおり・ADR 0035 の追補 #1042）。回数は INSERT の回数（`lv3AX` は 3 回・NORMAL の `3A` と同じ）。u の戻り先は打ち始める所。
- **`start_vcol` `end_vcol` は前のオペレータの値を残す。** Vim はこの 2 つを `$` でないときに終わりの最後の桁、1 行だけなら始まりの最初の桁も書き直し、ほかは前の VISUAL のオペレータの値のまま残す（`lvldvjAX` は 2 行目の 1 文字目の後ろ・`lvjdvjAX` は後ろ・`vj$AX` は手前・実測）。矩形は `get_op_vcol` が左右の桁にする。矩形の `C` `D` は `v_visop` が curswant を MAXCOL にしてから流すので、終わりは一番長い行の行末の桁（`l<C-v>jCX<Esc>0vj$AY` は `gXY`・実測・前の選択の欲しい列と同じ `block_line_end_state` の 1 本を読む）。Nib は `VimState.operator_columns`（`VimOperatorColumns`）に持ち、書くのは VISUAL をオペレータで抜けた歩の 1 か所（`visual_operation_noted`・`visual_acted` の範囲の変更・`u` `U`・`Ctrl-a` `Ctrl-x`・行に効く命令の群と `r{char}`）。VISUAL の `p` `P` も `nv_put_opt` が選択を消すために `do_pending_operator` を通すので書き直す（`operates()` は put を外さない・Nib の VISUAL の `p` `P` は #1418 で入った・`lyllvlp0vj$AY` は Vim `ghijklY`・対照の `llvlo<Esc>0vj$AY` は `ghijkYl`。fixture は #1418 の `visual-put-columns-`）。
- **`.`（ADR 0033）。** 大文字の鍵は Vim の `resel_VIsual_mode` が読み替えた種類なので、記録の大きさも読み替えた種類で測る（`measured_state`・`vlAX<Esc>j0.` は 1 行を選び直す）。#1417 の `X` `D` `Y` の大きさもこの形になる（結果は同じ）。再生の中の行単位は `oap->start` がカーソルで、終わりは同じバイトの桁のまま行数ぶん下（coladvance しない）。ただし記録が `$` で選んだ大きさ（`rv_vcol == MAXCOL`）なら coladvance(MAXCOL) で行の最後の文字（`v$AX<Esc>j0.` は `ghijkXl`・`VimLineExtent` が `$` の印を持ち、再生の欲しい列を行末にする）。`start_vcol` `end_vcol` は書き直さない（`lVjAX<Esc>j0.` は 4 行目の 1 文字目の後ろ・実測）。
- **`S` は入れない（設計席の判断・候補 B）。** Vim の VISUAL の `S` は行単位の `c`（`vjSX` `VjSX` `l<C-v>jSX` は `X/ef`・実測）だが、ADR 0165 の決定 5（施主決定 D67）が VISUAL の `S{char}` を surround に充てるので、surround（#1001）まで何もしない。
- **測った（変えない・後続）。** `'^` の桁（`lvjIX<Esc>`^` は Vim 1,2・Nib 1,1）は NORMAL の `lIX<Esc>`^` も同じ違いで本件の外（#1448）。空の文書の `VcX<Esc>`（`vCX` `vRX` も）で Nib は無名へ `\n` 行単位を書く（Vim は書かない）のも NORMAL の `Vc` と同じ根（ML_EMPTY・#980 にコメントで足した）。
- **まだ何もしない VISUAL の鍵（後続）。**

| 鍵 | Vim 9.1 | Nib | 後続 |
|---|---|---|---|
| `S` | 行単位の `c`（`nv_subst`） | 何もしない（NORMAL の `S` は #768 で `cc`・ADR 0015 の決定 15） | surround に充てる（ADR 0165 決定 5）ので入れない・#1001 |
| `p` `P`（矩形） | 矩形を消して貼る（`nv_put`） | #1461 の追補で入った（文字単位と行単位は #1418） | #1461 |
| `=` | 字下げのオペレータ | 何もしない | 未起票 |
| `gq` `gw` | 整形のオペレータ | 何もしない | 未起票 |
| `g?` | rot13 のオペレータ | 何もしない | 未起票 |
| `:'<,'>` | 選択の行の範囲で Ex | 何もしない（一般 Ex は後続） | 未起票 |

- fixture は `visual-change-key-` の 137 件（修正前の再生器で 135 件が落ちる。通る 2 件は `l<C-v>jsX<Esc>u` と `vjA<Esc>`＝本文が戻る・変わらない形）。

## 追補（2026-11-11・#1418）: 文字単位と行単位の VISUAL の `p` `P`

決定 7 の「効かない鍵」のうち、文字単位と行単位の VISUAL の `p` `P` を Vim 9.1 の実測（`-es -u NONE`・下ごしらえの probe 160 件と fixture `visual-put-`）と `normal.c` の `nv_put_opt`・`register.c` の `do_put` の読みどおりに入れる。矩形の VISUAL の `p` `P` は後続で、#1461 の追補で入った。

- **消してから貼る。** 貼る本文は消す前に読む（Vim は消す前のレジスタを `reg1` に取り、消した後に戻して貼る・`"1p` `"-p` も消す前の本文）。消すのは VISUAL の `d` と同じ道（`removed_exactly`）で、`p` は名前なしの `d`（無名・`"1`・`"-` を `registers_written` の規則どおりに書く）、`P` は `"_d`（Vim の `keep_registers`・レジスタを変えない）。`"a` の名前は貼る側だけに効く。読みは `register_read` の 1 本。
- **行単位の VISUAL（`PUT_LINE`）。** 本文の種類を問わず行として貼り（文字単位と矩形は末尾に改行を足す）、選んだ行を貼った行で置き換える。最後の行まで選んだときは上の行の下へ貼り、全部の行なら本文の終わりの空の行を残さない（`nv_put_opt` の empty の後始末）。キャレットは貼った最初の行の最初の非空白。`"_` は空の文字単位（`get_spec_reg` の `""`）なので空の行が 1 行（回数ぶん）入る。
- **文字単位の VISUAL。** 行単位の本文は消した所で行を割ってその間に貼る（`PUT_LINE_SPLIT`・キャレットは貼った最初の行の最初の非空白）。文字単位と矩形の本文は消した後の本文の消した所へ NORMAL の `P` と同じ `put_step` で貼る（消して行末に残るときの Vim の `p` も同じ所へ入る）。効果は消す範囲と貼る本文を 1 つの置き換え（矩形は最初の行の編集が消す範囲を含む行ごとの編集）に畳むので undo 1 単位で、`u` の戻り先は VISUAL の `d` と同じ `visual_restore`。空の本文（`"_` と空の文字単位）は消すだけ。
- **貼れないレジスタ（E353）。** Vim は先に消してから鳴らす（`lvp` は `b` を消して `"` に入れる・実測）。Nib も消した効果に `refused` の失敗を添える（oracle は E353 で測れないので fixture は無い）。
- **`gv`。** Vim は貼った後に `b_visual` の両端だけを貼った本文の両端（`b_op_start`・`b_op_end`）へ置き直し、種類と欲しい列は鍵の前のまま。両端は効果を写した後の位置なので、engine は `VimStep.visual_after` に返し、controller が効果を写した後に前の選択へ置く（`insert_left` と同じ形）。
- **`.`（ADR 0033）。** `nv_put_opt` が流す VISUAL の `d` の `prep_redo` が put の記録を上書きするので、`.` は同じ大きさを選び直して `d`（`P` は `"_d`）を流すだけ（`vp` の後の `w.` は貼らない・実測）。回数と `"a` は記録に載らない。
- **`:set clipboard=unnamed`。** 文字単位と行単位の VISUAL の `p` `P` もクリップボードを無名へ読む（`vim_reads_unnamed_clipboard`）。
- **巨大な回数。** `v999999999p` は貼る本文が文書の上限を超えるので、消す前に断って選択も本文も変えない（#1341 の方針・`failed(cancelled(state))`）。Vim は先に消してから貼れずに終わる。
- **測った（変えない・後続）。** 矩形の VISUAL の `p` `P`（`abc⏎def` の `yy<C-v>jp` は Vim では矩形を消して下へ行を貼り `bc⏎ef⏎abc`・Nib は何もしない・#1461）。`'.`（`` `. `` `g;`）は、消して貼る全体を 1 つの置き換えに畳むので消した所になる。Vim は行を貼ったら貼った最初の行の行頭で、行単位の本文を貼る 2 つの形で違う: 文字単位の VISUAL に行単位の本文（`ab⏎cd⏎ef⏎gh` の ``yyjlvpgg`.`` は Vim 3,1・Nib 2,1。``yjjvpgg`.`` ``yyjlv2pgg`.`` `yyjlvpggg;` も同じ）と、最後の行まで選んだ `V`（``yyGVpgg`.`` は Vim 4,1・Nib 3,2）。文字単位の本文を貼る形と最後の行でない `V` は一致する。直し方は操作の種類を渡す #793 の形（レビュー V-1460）。
- fixture は `visual-put-` の 149 件（修正前の再生器で最初の 143 件がすべて落ちる。#1436 と合わせた木で足した `visual-put-columns-` の 6 件は、VISUAL の `p` `P` が `visual_operation_noted` を通って `start_vcol` `end_vcol` を書き直すことを測る・`lyllvlp0vj$AY` は `ghijklY`・対照の `llvlo<Esc>0vj$AY` は `ghijkYl`）。

## 追補（2026-11-11・#1461）: 矩形の VISUAL の `p` `P`

#1418 の追補で後続にした矩形の VISUAL（`<C-v>`）の `p` `P` を、Vim 9.1 の実測（`-es -u NONE`・下ごしらえの probe 121 件と fixture `block-visual-put-` 113 件）と `normal.c` の `nv_put_opt`・`register.c` の `do_put`（v9.1.0004）の読みどおりに入れる。

- **消してから貼る。** 貼る本文は消す前に読み（`"_` は空の文字単位）、矩形は VISUAL の `d` と同じ道（`block_operated`）で消す。`p` は名前なしの `d`（無名と `"1` `"-` を `registers_written` の規則どおりに書く）、`P` は `"_d`（レジスタを変えない）。読みは `register_read` の 1 本。貼れないレジスタ（E353）は文字単位と同じく消してから鳴らす。
- **貼る所と向き。** Vim は消した後のキャレットが `b_op_start`（矩形の 1 行目の左の文字の頭）より左なら前向き（FORWARD）に貼る。消した所が行末に残ると `check_cursor` がキャレットを最後の文字へ寄せるのがその場合で、左の文字が端で切れた Tab や全角なら寄せたキャレットがその頭に戻るので後ろ向きのまま（`x<Tab>b⏎abcdefghij⏎ab` の `ylfb<C-v>jjp` は `xx ⏎axbj⏎axb`・実測）。Nib は `block_put_place` の 1 本。
- **本文の種類ごとの形。**
  - 行単位の本文: `p` は選択の終わりの角（`b_visual.vi_end`＝キャレット）の行の下へ（`PUT_LINE_FORWARD`・下から上へ選んだら上の行の下・`yyjj<C-v>kp` は `abc⏎ef⏎abc⏎hi`）、`P` は消した後のキャレットの行の上へ（前向きなら下へ・`yyjl<C-v>jlP` は `abc⏎d⏎abc⏎g`）。
  - 改行を含まない文字単位の本文: 矩形の各行へ入れる（`do_put` の `VIsual_active` の繰り返し・`end_lnum` は矩形の最後の行）。1 行目は消した所へ、下の行は 1 行目の桁で始まる文字の前（行の終わりがちょうどその桁なら後ろ）へ入れ、その桁で始まる文字が無い行（短い行・Tab や全角の途中＝`getvpos` の FAIL）は飛ばす（`abcd⏎a⏎abcd` の `yll<C-v>jjlp` は 2 行目の終わりへ足して `aa`）。キャレットと前の選択の終わりは 1 行目に入れた本文の最後の文字。
  - 改行を含む文字単位と矩形の本文: NORMAL の `p` `P` と同じ `put_step` で消した所へ貼る（矩形が本文の終わりを越えれば行を足す）。
- **1 つの編集に畳む。** 消した後の本文（`block_removed_text`）の上で貼る編集を作り、矩形を消す行ごとの置き換えに重ねて消す前の本文の上の行ごとの置き換えの列に畳む（`block_put_merged`・消した所に触れる貼付はその行の置き換えと 1 つに合わせ、触れない貼付は手前の置き換えの伸び縮みのぶんずらす）。効果は `VimInsertBlock`（`separate`）で undo 1 単位、`u` の戻り先は矩形の `d` と同じ左上（`Edit.restore`）。マークは `apply_block` が行ごとの置き換えで `marks_block_adjusted` に通す（行を足す貼付だけが下のマークを動かす・`'a` の fixture で一致）。
- **`gv`・`.`・`:set clipboard=unnamed`。** 前の選択は貼った本文の両端（`b_op_start`・`b_op_end`）で種類は矩形のまま（`VimStep.visual_after`・#1418 と同じ形）。何も貼らないとき（`"_` と空の文字単位）は寄せたキャレットの 1 文字。`.` は同じ大きさの矩形の `d`（`P` は `"_d`）を流すだけ（`visual_put_recorded`）。`vim_reads_unnamed_clipboard` は矩形の VISUAL でも真。
- **測った（変えない・後続 #1475）。** `'.`（`` `. `` `g;`）は畳んだ 1 つの編集の頭（矩形の 1 行目・下から上へ選んだ形を含む）になる。Vim は最後に変えた行で、各行へ入れる文字単位の本文（`abc⏎def⏎ghi` の ``yl<C-v>jjlpG`.`` は Vim 3,1・Nib 1,1）と、行単位の本文を矩形の下へ貼る `p`（``yy<C-v>jpgg`.`` は Vim 3,1・Nib 1,1・下から上へ選んだ ``yyjj<C-v>kpgg`.`` は Vim 3,1・Nib 2,1）で違う。矩形の本文・`P` の行・改行を含む文字単位の本文は一致する。#1418 の `'.` と同じ根で、直し方は #793 の形。`'[` `']` はこの版がマークを持たない（ADR 0152 の限界）ので測らない。`"+` `"*` は実機のクリップボードを書き換えるので fixture にも probe にも入れていない（ADR 0051）。
- **巨大な回数。** 各行へ入れる文字単位の本文は、入れる行を先に数え、本文 × 回数 × 行の数が文書の上限に入るかを本文を作る前に `fits_repeated` で見る（#1341 と同じ・溢れない割り算）。入らなければ消す前に断り、本文もレジスタも変えない（`abc⏎def⏎ghi` の `yl<C-v>j999999999p`・契約 `verify_vim_block_put_huge_count`・レビュー V-1478）。ほかの形は `put_step` の `repeated` と `block_put_fits` が同じく作る前に断る。Vim は 2 GB を作りにいくので oracle では測らない。
- fixture は `block-visual-put-` の 113 件（修正前の再生器で 111 件が落ちる。通る 2 件は `lll` の 3 つ目が行末で失敗して後ろの鍵が流れない `<C-v>jylll<C-v>jjp`（Vim も本文を変えない）と、`u` で本文が戻る `jjyykk<C-v>jlPu`）。
