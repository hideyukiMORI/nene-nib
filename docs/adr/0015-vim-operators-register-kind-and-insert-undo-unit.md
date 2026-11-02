# ADR 0015 — Vim のオペレータは自分の回数を持ち、レジスタは種類を持つ LF の本文、INSERT の編集は 1 つの `Edit` に吸収する

- 状態: 受理
- 日付: 2026-09-18
- Issue: #43
- 影響する規則: ARC-001 / ARC-004 / ARC-007 / ARC-009 / CPP-002 / CPP-003 / CPP-006 / CPP-011 / CPP-012 / QLT-009 / QLT-013

## 文脈

ADR 0012 で Vim のエンジンは core の純関数 `vim_step` になり、鍵と効果は閉じた和型、再現度は oracle の fixture の再生で守られている。範囲は移動と `d` だけで、ADR 0012 が「正直に」に残した限界が 3 つある:
回数の掛け算（`2d3w` が 23 語）、INSERT の中の undo の単位（`ia<BS>b<Esc>u` が Vim と一致しない。`EditHistory` の `coalesce` は削除を伴う編集をつなげない）、
無名レジスタが文字列だけで行単位か文字単位かを持たないこと（`p` の貼り方はレジスタの種類で決まる）。
T2 の「オペレータ `d c y p P` ＋回数＋移動」（SPECIFICATION 第 5 節）を、この 3 つを解いた上で足す。

## 決定

**保留中のオペレータは自分の回数を持つ値 `VimPendingOperator`、無名レジスタは種類つきの `VimRegister`（本文は LF だけ）、`p` `P` は効果 `VimPutString` の 1 つ、
INSERT にいる間の編集は `EditBoundary::absorb` で直前の `Edit` に吸収して 1 つの undo の単位にする。`c` は「削除の効果 ＋ 次の状態が INSERT」で、新しい効果を足さない。**

1. **`VimPendingOperator{VimOperator op; std::optional<VimCount> count}`**（core・公開 aggregate）。`VimState::pending` はこの型。`d` `c` `y` を押した時点の回数をオペレータが持ち、
   その後に積んだ回数（`VimState::count`）と**掛け算**して動作の回数にする（`2d3w` = 6 語、`2d3d` = 6 行、`3dd` = 3 行）。どちらも無ければ 1。`VimState::count` の意味（いま積んでいる回数）は変えない
2. **`VimOperator { remove, change, yank }`**。範囲の規則（inclusive / exclusive・行単位）は 3 つで同じ 1 本（`removed_by_motion` を「範囲を決める」と「効果にする」に分ける）。
   `change` の効果は `VimRemoveRange`（行単位 `cc` `cj` `ck` は行の内容だけを消して行を 1 本残す）で、次の `VimState` が INSERT。**モードは状態が持つので、`c` に合成の効果は要らない**。
   `cw` は Vim の特例（語の上では `ce`）を engine に書き、oracle の fixture が守る。`yank` は本文を変えず（`VimMoveTo`）、キャレットは範囲の先頭（`yb` `yk` で動く。oracle が決める）
3. **`VimRegister{std::string text; VimRegisterKind kind}`**（`VimRegisterKind { characters, lines }`）。`VimState::unnamed_register` はこの型。**本文は LF だけ**: Vim のレジスタは「行の列」で、改行の形（CRLF / LF）は文書の性質（ADR 0010）。
   `dd` `yy` は CRLF の文書でも LF で入れる（engine が `text_range` の結果から CR を落とす純関数を 1 つ持つ）。oracle は `getregtype('"')` も取り（`v` / `V`）、fixture の再生が種類も突き合わせる
4. **`VimPutString{Offset at; std::string utf8; Offset caret}`** を `VimEffect` に足す（8 → 9）。engine が「どこに」「何を（回数ぶん繰り返した LF の本文）」「貼ったあとキャレットをどこに」を決め、
   controller は `at` に `utf8` の LF を `newline_of(line_ending())` に直して入れて `caret` に置く。**改行の形を決めるのは `VimNewLine` と同じ controller の 1 か所**（ARC-009）。
   文字単位は文字の後ろ（`p`）/ 前（`P`）、行単位は下の行 / 上の行（最終行の下は行末に改行から）。キャレットの行き先は oracle が決める（文字単位 1 行・複数行・行単位で fixture）
5. **`EditBoundary::absorb`**（3 つ目）。`EditHistory::pushed` は `absorb` で来た編集が直前の `Edit` に隣接していれば **1 つの `Edit` に畳む**（純関数 `absorbed(previous, edit) → std::optional<Edit>`）:
   `inserted` の直後の挿入は `inserted` に足す、`inserted` の末尾を消す削除は `inserted` を縮める、挿入開始より前を消す削除は `at` を前へ動かして `removed` の先頭に足す。隣接しなければ新しい単位。
   通常モードの `coalesce`（連続した文字入力だけ・ADR 0009 の決定 3）は変えない。**Vim モードで INSERT にいる間の `VimInsertString` / `VimNewLine` / `VimRemoveRange`（Backspace・`c` の削除）は `absorb`**、
   NORMAL の編集（`x` `d` `p`）は `separate`。境界は controller が「効果を写すときの Vim のモード」で決める（効果に境界を持たせない）。矢印・Home / End で動くと単位は切れる（`move_caret_to` が閉じる。Vim と同じ）
6. **鍵の追加は表の行**（CPP-012）: 移動 `e` `^`、`D` `C` `Y`（`d$` `c$` `yy`）、`<Home>` `<End>`（`VimSpecialKey` に `home` `end`。NORMAL は `0` `$`、INSERT は行の中で動く）。
   ui/win32 の表と oracle の `KEY_NAMES`、テストの `vim_keys_of` にそれぞれ 1 行
7. **undo の単位の検査は手書きの単体テスト**。oracle は `:normal!` 1 回がまるごと 1 単位なので、単位の境界を見分けられない（ADR 0012「正直に」）。fixture は本文・キャレット・レジスタ（種類つき）だけを突き合わせる
8. **やらないこと（3 本目以降）**: VISUAL（`v V Ctrl-v`）、画面の高さが要る移動（PgUp / PgDn・`Ctrl-d/u`・`H M L`。engine に見えている行数を渡す形を決めてから）、テキストオブジェクト、`.`、名前つきレジスタ、`> <`、`J r s S X`

### 決定 9〜11（2026-10-21 追記・#766）

9. **`J` `gJ` は動作で、オペレータではない**（Vim も `J` は operator でない）。`VimAction` の `join_lines` `join_lines_raw`（`g` の後ろの `J`）で、キャレットの行から回数の行（最小 2）をつなぐ。
   回数が本文の末尾を越えるとき、2 行なら失敗し、3 行以上なら最後の行まで（Vim の `nv_join`）。`J` は次の行の先頭の空白と Tab を落とし、つなぐ所に空白を 1 つ入れる。
   入れないのは、次の行（落とした後）が空か `)` で始まるとき・ここまでの本文が空のとき・前の行が Tab で終わるとき。前の行が空白で終わるときは足さない。
   **`joinspaces` は Vim 9.1 の既定どおりオン**で、前の行が `.` `?` `!` で終わる（空白の手前を見る）なら 1 つ足す（oracle で実測: `abc.` + `def` → `abc.  def`）。`gJ` は何も落とさず入れない。
   キャレットは最後につないだ所（入れた空白の始まり・行の外なら最後の文字・Vim の `do_join`）。レジスタは書かない。効果は `VimReplaceRange`（覆う行の内容の全体を 1 つの本文に置き換える）で、undo 1 単位・`.` で再生。
10. **大小文字は `~` と、オペレータ `g~` `gu` `gU`**（`VimOperator` に `toggle_case` `lower_case` `upper_case`）。範囲の規則は `d` `c` `y` と同じ 1 本（移動・テキストオブジェクト・回数の掛け算）で、
    「効果にする」が `VimReplaceRange` を返す（`r` と共用の 1 つの効果。controller は `replace` の 1 本で写す）。保留の後ろで行単位になる鍵は `g~~` `g~g~` `guu` `gugu` `gUU` `gUgU`（表 `operator_bindings` と
    `doubled_bindings`・`vim_operator_doubles`）。NORMAL の `U`（行の undo）は未実装で、表に無い鍵と同じに打ち消す（`gUU` と VISUAL の `U` のための値 `undo_line`）。
    `~` は `tildeop` off の 1 文字の反転で、回数ぶん右へ進み行末で止まり、空行は失敗。VISUAL（`v` `V`）の `~` `u` `U` は選択の反転・小文字化・大文字化（VISUAL の `u` は undo ではない）。
    キャレットは範囲の戻り先（行単位はキャレットと着地の小さい方・文字単位は範囲の先頭・VISUAL の行単位は最初の行の行頭・Vim の `oap->start`）。**変換は ASCII の英字だけ**で、非 ASCII は変えない（結合文字つきの `a` は `A`＋結合文字）。
11. **字下げは `>` `<`**（`VimOperator` に `shift_right` `shift_left`）。範囲が覆う行（`>w` も行単位）の字下げを `shiftwidth` だけ動かす。`>>` `<<` は回数の行、VISUAL の `[count]>` は回数ぶん動かす（Vim の `op_shift` の `amount`）。
    Vim の既定 `shiftwidth=8` `tabstop=8` `noexpandtab` `noshiftround` を固定する（定数は `VirtualColumn.hpp` の `vim_shift_width` と `vim_tab_stop`・設定にはしない）。今の字下げを表示の桁（`column_advance`）に直して足し引きし（左は 0 で止まる）、
    `桁 / 8` 個の Tab と `桁 % 8` 個の空白で作り直す（Vim の `set_indent`）。空行は動かさない（空白だけの行は動かす）。キャレットは最初の行の最初の非空白。覆う行を 1 つの `VimReplaceRange` にするので undo 1 単位。
    VISUAL の `.` は回数ごと再生する（記録を始める鍵の前の回数を鍵の頭に残す・`Vj2>j.` は 2 段・実測）。

決定 9〜11 の共通: `J` と大小文字と字下げは**レジスタを書かない**（ADR 0050 の書き手 `registers_written` を通らない・`"a` を前に付けても無名もそのまま）。`.` の記録は既存の鍵の列で足りる。
矩形 VISUAL（`Ctrl-v`）の `~` `u` `U` `>` `<` は #770 で、いまは鍵を拒んで矩形のまま残る（Vim は行ごとに効く）。VISUAL の `J`・Unicode の大小の写し・`:set shiftwidth=` `expandtab`・`gq` `=` は後続。

### 決定 12（2026-10-28 追記・#966）

12. **`{N}$`（N ≥ 2）は最終行では失敗する**（Vim の `nv_dollar` の `cursor_down(N − 1)`・`g_` と同じ規則・ADR 0147 の決定 8）。最終行にいるときは動かず（`VimRepeatFailure::not_moved`・残りの鍵は #230 で打ち切る）、オペレータ（`d2$`）は打ち消して本文を変えず、VISUAL（`v2$`）も選択を動かさない。最終行でなければ本文の末尾の行に切り詰めて行末へ（`5$` `d5$` `v2$`）。着地は `landing_of` の `line_end_landing` の 1 本で、裸の移動と VISUAL が同じ値を読み、オペレータの範囲は前から同じ規則の `to_line_end`。欲しい列は失敗でも「行末」（`nv_dollar` は `cursor_down` の前に `curswant` を貼る）。fixture `count-dollar-`（11 件・Vim 9.1 の実測: 1 行の `2$` は (1, 1)・`$2$` は (1, 11)・2 行の `3$` は (2, 3)・3 行の `2$` は (2, 3)・`5j` の最終行の失敗も 1 件）。

### 追補（2026-10-28・#965）: NORMAL の `y` の後のキャレットと欲しい列

NORMAL の `y{motion}` の後のキャレットは Vim の `oap->start`（範囲の始まり）。**上へ動いた行単位の移動**（`k` `-` `gg` `G` `H` `M` `L` `'a` と回数つき）は移動の着地へ置き、桁は移動が決める
（`-` `gg` `H` などは最初の非空白・`k` は欲しい列を保つ・`$` の後の `k` は行末）。下へ動く行単位の移動（`j` `+` `<CR>` と回数）と `yy` `Y` はキャレットを動かさない。文字単位は範囲の先頭（`yb` は後ろへ動く）。
`3y-` のように回数が本文の先頭を越えても 1 行目の着地へ置く。y の後の欲しい列（curswant）は置いたキャレットの桁（Vim の `w_set_curswant`・`$yj` `j$yk` `y$` の後の `j` は `$` を引き継がない）で、これは前から合っていた（fixture で確かめた）。
決めるのは `operated` の 1 か所で、上へ動いた行単位の着地の桁を欲しい列に写して `yanked_caret` に渡す（#411 の `gg` `G` `H` `M` `L` `'a` の特例を全ての行単位の移動へ広げた）。
VISUAL の `y` は触らない。`d` `c` の後のキャレットは別の規則で変えない。移動 `_` は Nib に未実装なので `y_` は後続。fixture `yank-line-cursor-*` 12 件（Vim 9.1 の oracle で実測）。

**広げ（2026-10-29・#1065）: 同じ行に着く行単位の移動**。Vim の `do_pending_operator` は範囲の始まりを `LT_POS`（行と桁）で比べるので、行単位の移動でも移動の前と着地が同じ行なら桁の早い方がキャレットになる。
`operated` の比べ方を「着地の行がキャレットの行より上」から「着地の位置がキャレットの位置より前」（本文の位置の順＝(行, 桁) の順・`landed` の戻り先と同じ比べ方）に替えた。
`foofoo? a` の `$ygg` は (1, 1)・`  abc` の `$ygg` は (1, 3)・`abc⏎xyz` の `j$yG` は (2, 1)・`ma$y'a` は最初の非空白・`$yggj` の欲しい列は置いた桁（(2, 3)）。行が違うとき（`j$ygg` `$yG` `$yj` `j$yk` `$yy` `j$y1G`）と後ろに着くとき（`0yG`）は前のまま。移動 `_` は y の下でカーソルを動かさない（Vim の `nv_lineop` は `OP_YANK` で `beginline` を呼ばない・決定 13 の `j$y_`）ので比べない。文字単位の `` y`a `` は前から範囲の先頭。fixture `yank-same-line-*` 15 件（Vim 9.1 の oracle で実測・fuzz #954 seed 21 の fuzz-21-0006 の原因）。

### 決定 13（2026-10-28 追記・#976）

13. **移動 `_` は回数 − 1 行下の行の最初の非空白へ行く行単位の移動**（`VimMotion::first_non_blank_below`・鍵は `VimKeyTable` の NORMAL の表の 1 行）。`1_` は今の行。範囲は `+` `j` と同じ `lines_below` に回数 − 1 を渡す 1 本で、`d_` は `dd`・`2d_` は `2dd`・`y_` は `yy` と同じ範囲とレジスタ（行単位・`"1` は `registers_written` が決める）。
    着地は `g_` と同じ `counted_line_landing` の 1 本（回数 − 1 行下・本文の末尾で止まり、最終行で回数が 2 以上なら Vim の `nv_lineop` の `cursor_down` が失敗してオペレータを打ち消す・`j$5_x` は動かず `x` も打ち切る）。最終行の `1_` は失敗しない。
    `y_` の後のキャレットは動かない（`j$y_` は `$` の位置のまま・`l2y_` も同じ）。`c_` は行の本文を消して空の行で INSERT（`autoindent` 無しの既定）、`>_` は `>>` と同じ。fixture は `underscore-` の 16 件（Vim 9.1 の実測）。

### 決定 10 の追記（2026-10-28・#781）: 非 ASCII は Unicode の簡単な大小の写像

- 決定 10 の「変換は ASCII の英字だけ」を置き換える。写像は **Unicode の簡単な（1 対 1 の）写像の表 1 つ**で、生成物 `src/core/CaseMappingTable.hpp`（`case_to_upper` 194 行・`case_to_lower` 181 行の「first・last・step・delta」の範囲）を、
  生成器 `eng/case-mapping.py --write` が固定の Python（3.12・`unicodedata` は Unicode 15.0.0・版は見出しに書く）の `str.upper` / `str.lower` の結果が 1 code point のものだけから作る。Vim のソースの表は写さない。
  一致は CNF-010（`eng/conformance.py` の `case_mapping_checks` が全文を比べる・`VimKeyNames.hpp` と同じ形）。引くのは `VimCase.hpp` の純関数 `vim_to_upper` `vim_to_lower` `vim_toggled`（二分探索・実行時の初期化なし・ADR 0074 の形）。
- **Vim の 1 文字（ADR 0053）の基底だけを写し、続く結合文字はそのまま**（`e`＋U+0301 の `~` は `E`＋U+0301・`α`＋U+0345 の `gUU` は `Α`＋U+0345・固定 Vim 9.1 の実測）。
- **大文字の写像を持つ字が「小文字」、小文字の写像を持つ字が「大文字」で、小文字の判定が先**（Vim の `swapchar`）。だからタイトルケースの `ǅ` は小文字で、`~` と `gU` は `Ǆ` にし、`gu` と VISUAL の `u` は変えない（実測 `ǄǅǆΣ` の `v$u` → `ǆǅǆσ`）。
- fixture は `unicode-case-` の 15 件（全角・`é É`・ギリシャ・キリル・結合文字 2・混在の `g~G`・`Vj~`・`v$u`・タイトルケース・`.`・トルコ語 2・BMP の外）。

限界（実測・Vim 9.1 の `toupper()` `tolower()` を U+0080〜U+10FFFF で全数比べた結果、違うのは次の 28 字だけ）:

- Python の `str.upper` / `str.lower` は SpecialCasing の 1 対多を返すので、そこに載る字の簡単な写像は表に入らない。Vim は UnicodeData の簡単な写像を使うので、
  `İ`（U+0130）の小文字化（Vim は `i`）と、ypogegrammeni つきのギリシャ小文字 27 字（U+1F80〜U+1FA7 の 24 字・U+1FB3・U+1FC3・U+1FF3）の大文字化（Vim は U+1F88 などのタイトルケース）が Vim と違い、Nib は変えない。fixture は Vim と合う `İ` の `gUU` だけを置いた。
- **`ß` の `gU` は Vim では `SS`**（1 対多・実測 `straße` → `STRASSE`）。1 対多はこの追記の範囲の外で、Nib は `ß` を変えない。fixture は Vim でも変わらない `g~~` だけを置いた。
- 矩形 VISUAL の `~` `u` `U` は #959（PR #981）で、同じ `cased_text` を通る。

### 決定 10 の追補（2026-10-29・#1064）: 空の範囲の `g~` `gu` `gU` は実行する

- Vim の既定の `cpoptions` に `E` は無く（`:help cpo-E`）、`op_tilde` は `oap->empty` を見ないので、**範囲が空でも `g~` `gu` `gU` は成功**する。`d` `y` `c` の空の範囲は前から Vim と合っていて変えない。`cpoptions` は設定にしない。
- 書き換える本文は `case_span` の 1 本が決める（`case_changed` が範囲を受ける 1 か所・呼び出し元では数えない）。文字単位の空の範囲（1 桁目の `h` `0` `1|`・最初の非空白での `^`）は、Vim が exclusive の終わりを 1 文字戻してから数える個数に合わせて、
  **本文の先頭なら着地の 1 文字**（`x1` の `gUh` → `X1`）、**2 行目以降の 1 桁目なら行の残り全体**（前の行の末尾へ戻るので行をまたいだ扱い・`a⏎x1y` の `jgUh` → `a⏎X1Y`）、**それ以外の桁は 0 文字**（`␣␣ab` の `llg~^` は変えない）。
  Issue の見込み（いつもカーソルの下の 1 文字）は 1 行目だけ正しく、2 行目以降は Vim 9.1 の oracle の実測を正本にした。
- 書き換える本文が空（空の行・0 文字）でも効果は `VimReplaceRange` で返すので **`.` の記録に残る**（`⏎ab` の `g~^jl.` → `⏎Ab`）。空を空に置き換える編集は controller の `replace` が履歴に積まず、キャレットだけ置く。
- キャレットは範囲の先頭（Vim の `oap->start`）。`:help exclusive-linewise` で行単位になった範囲が空の行だけでも同じで（`⏎x1` の `jgUb` は (1, 1)）、行単位の空の範囲も打ち消さない。
- fixture は `empty-case-` の 36 件（Vim 9.1 の実測・`d` `y` の一致している形と VISUAL の空の行の `.` も入れて不変を固定）。

### 決定 10 の追補（2026-10-29・#1109）: 行単位の VISUAL の `~` `u` `U` の後のキャレット

- キャレットは Vim の `do_pending_operator` が置く `oap->start` で、`op_tilde` は動かさない。行単位の VISUAL では **固定の端（`V` を打った位置）が先頭行なら先頭行の行頭、動く端（キャレット）だけが先頭行ならキャレットの位置のまま**（`jVgg~` は `gg` の着いた最初の非空白・`jwVk~` は `k` の着いた桁・`wV~` `wVj~` `wwVb~` `wVw~` は行頭）。同じ行の中で左右に動いても行頭（Vim は `oap->start` の桁を 0 にしてから位置で比べる）。
- `.` の再生（Vim の `redo_VIsual_busy`）は桁を 0 にしないので、**選び直した選択の錨（`.` を打った位置）**に残る（Vim は行単位の再生でキャレットの桁を動かさない）（`wV~jl.` は 2 行目の 2 桁目・`jjwVkUl.` も同じ）。印は `VimState.redoing_visual`（ADR 0018 の追補 #1039）。
- 規則は `visual_restore` の 1 本（ADR 0052 の決定 10 の u の戻り先と同じ値）で、大小文字のキャレットと u の戻り先の両方に使う。オペレータの種類で分けない。前は大小文字だけキャレットを範囲の先頭（行頭）にしていた（fuzz seed 37 の `fuzz-37-0206`・`Vgg~c$`）。`.` の再生の行頭へ寄せない規則は `visual_restore` の中にあるので `d` `c` `r` `>` `<` と大小文字の再生の u の戻り先にも効く（Vim の同じ分岐）。`u` を `.` の後に打つ形は oracle が 1 本の `:normal!` に流すので測れず（ADR 0052）、Vim の redo 分岐からの値を契約（`VimDotTests.cpp` の `verify_undo_caret_after_visual_line_dot`）で固定する。
- 文字単位（`wvj~` は選択の先頭）と矩形は変えない。fixture は `visual-line-case-` の 19 件（Vim 9.1 の実測・`g~~` `gUk` の比較と `u` の後を含む）。

### 決定 10 の追補（2026-10-30・#990）: 残りの 28 字と `ß` の `gU`

- #781 の追記の「限界」を閉じる。生成器 `eng/case-mapping.py` は、Python の `str.upper` / `str.lower` が SpecialCasing の 1 対多を返す字のうち、**UnicodeData.txt（Unicode 15.0.0）の簡単な写像を持つ 28 字を上書きの表 `SIMPLE_OVERRIDES` に持つ**（大文字化は U+1F80〜U+1F87・U+1F90〜U+1F97・U+1FA0〜U+1FA7 の 24 字と U+1FB3・U+1FC3・U+1FF3 をタイトルケースの U+1F88 などへ、小文字化は `İ`（U+0130）を `i` へ）。
  上書きは「Python の写像が 2 code point 以上の字」だけに許し（生成器が検査する）、生成物の見出しに字数を書く。表の形・生成の仕組み・CNF-010 は変えない。生成した表は固定 Vim 9.1 の `toupper()` `tolower()` と U+0080〜U+10FFFF の全数で一致する（2026-10-30 に再測定・違い 0）。
- **`ß` の 1 対多は表の外の 1 か所**で、`cased_text` が `gU` と VISUAL の `U`（`VimOperator::upper_case`）のときだけ `ß` を `SS` にする（Vim の `swapchar` の OP_UPPER の特別扱い）。`~` `g~` `gu` と VISUAL の `~` `u` は `ß` を変えない。`ß` も `SS` も 2 バイトなので、範囲の後ろのバイトの位置は変わらない。矩形の `U` も同じ `cased_text` を通る。
- 照合（`ignorecase` / `smartcase`）は同じ表を引くので、`smartcase` は `İ` を大文字として数え、`[ᾈ]` の大小を畳む照合は `ᾀ` にも当たる。どちらも固定 Vim 9.1 で実測した（`:set ic scs`・本文 `xB a` で `search('İ\|b', 'cW')` は 0（`İ` が大文字なので大小を区別し `b` が `B` に当たらない）・対照の `ı\|b` と `x\|b` は 1・`:set ic` の `/[ᾈ]` は `ᾀ` に当たる）。照合の残りは #1147。
- fixture は `case-rest-` の 14 件（`İ` の `guu` `~` VISUAL の `u`・ギリシャの `gUU` `g~~` `3~`・`ß` の `gUU` `gUl` `.` `~` `g~~` `guu` VISUAL の `U` と `V` の `U`・Vim 9.1 の実測）。#781 の `unicode-case-turkish-dotted-I-upper` はそのまま。

### 決定 14（2026-10-29 追記・#1038）

14. **`Ctrl-a` `Ctrl-x` は数の増減で、1 回の `Edit`・undo 1 単位・`.` に載り・レジスタを書かない。`nrformats` は `bin,hex` に固定する**（Vim の defaults.vim の値・8 進・英字・unsigned は読まない・`:set nrformats=` は後続）。
    - 数を探す・増減した本文を作るのは core の純関数 `VimNumber.hpp` の 3 本（`vim_find_number` `vim_find_selected_number` `vim_incremented_text`）で、Vim 9.1 の `do_addsub` と `vim_str2nr` をそのまま写す: キャレットが 2 進・16 進の桁か `0x` `0b` の上ならその数、そうでなければキャレットの上か右の最初の 10 進の数。直前の `-` は 10 進の負号で、16 進と 2 進の前の `-` は残して正の数として増減する。
      値は 64 bit の符号なしで、10 進は回り込むと負号が反転し（`18446744073709551615` の `Ctrl-a` は `-18446744073709551615`）、読むときに溢れた数は最大値に貼り付いて増減しない。`0` で始まる数は桁数（符号と接頭辞を除く）を保って 0 で埋め（`007` → `008`・`0x00ff` → `0x0100`・`00` の `Ctrl-x` は `-01`）、16 進の桁の大小は数の最後の英字に合わせ（`0xaF` → `0xB0`）、`0x` `0X` の大小は元のまま。回数は 999999999 に貼り付く（Vim の normal.c）。
    - **行を写さない**: engine（`VimStepNumbers.cpp`）はキャレットの前後 4096 バイトの窓を `text_range` で読み、純関数が窓の端（行の端ではない端）まで読みに行ったら `VimNumberMiss::window_short` を返すので窓を倍にして読み直す。16 MiB の 1 行でも数がキャレットの近くにあれば 8 KiB しか写さない。
    - NORMAL: 置き換えは `VimReplaceRange` の 1 つで、キャレットは新しい数の最後の桁、`u` の戻り先は押したときのキャレット（`Edit.restore` の既定）。数が無ければ失敗（`not_found`・残りの鍵は打ち切る）、空行は失敗にせず何もしない、オペレータの後ろ（`d<C-a>`）は失敗にせず打ち消す（Vim の `clearop`）、NORMAL の `g Ctrl-a` は失敗（Vim のビープ）。
    - VISUAL: 各行の選択の中の最初の数を増減し、行ごとの置き換えを `VimReplaceBlock` にまとめる（undo 1 単位）。数を探す範囲は文字単位なら選択と行の重なり・行単位なら行の全体・矩形なら丸ごと入っている本文（Vim の `block_prep` の textcol / textlen）で、行単位のほかは数を範囲の中で切る（`12345` の `llvl<C-a>` は `12355`）。`-` は選択の中にあるときだけ負号。`g Ctrl-a` `g Ctrl-x` は数を変えた行ごとに足す量を回数ずつ増やす（数の無い行は数えない）。キャレットと `u` の戻り先は選択の始まり（行単位は最初の行の行頭・矩形は左上）。数の無い選択も失敗にしない。
    - 鍵は `VimSpecialKey::control_a` `control_x`（`g` の後ろは表の 0x01 と 0x18・本文 ↔ 鍵の写しも同じ値・`r<C-a>` は他の Ctrl 鍵と同じく何もしない）。窓は Vim の NORMAL と VISUAL でだけ Ctrl+A / Ctrl+X を写す（D71 の Vim 優先・INSERT は通常モードの表へ落ちる）。INSERT の `Ctrl-a`（直前の挿入）と `Ctrl-x`（補完）は未対応で何もしない。
    - fixture は `increment-` の 72 件（Vim 9.1 の oracle で実測・どれも `set nrformats=bin,hex` を fixture の設定に書く。oracle の `-u NONE` の Vim の既定は `bin,octal,hex` で、`007` の `Ctrl-a` が 8 進の `010` になる）。窓の外を読まないこと・値の往復・範囲の形は `--vim-number-property`（固定 seed の性質の試験）。

## 強制

- fixture の再生（ADR 0005 / 0012）— **active**（`nib_unit` が `VimFixtures.hpp` の全項目を再生し、レジスタの種類も突き合わせる）
- 生成物と `fixtures.json` の一致（ADR 0012 の planned）— Issue #44（SHA-256 の規約検査）で別に active にする
- CPP-002: `VimOperator` / `VimRegisterKind` / `EditBoundary` の `switch` に `default` を書かない・`std::visit` の写し先を欠かさない — **active**（コンパイルと clang-tidy）
- ARC-007 / CPP-013: core から時刻・OS のシンボルが出ない — **active**（`eng/symbols.py`）
- `J` `gJ`・`~` `g~` `gu` `gU`・`>` `<`（決定 9〜11）— **active**（fixture `J-` `gJ-` `tilde-` `gtilde-` `gu-` `gU-` `shift-right-` `shift-left-` を `nib_unit` が再生・CRLF の文書と undo の単位と `.` の記録は `--vim-replace` の手書きの契約）
- `Ctrl-a` `Ctrl-x` と VISUAL の連番（決定 14）— **active**（fixture `increment-` 72 件を `nib_unit` が再生・純関数の性質は `--vim-number-property`）
- undo の単位（決定 5）— 単体テスト（CTest）。`ia<BS>b<Esc>u`・`cwfoo<Esc>u` は 1 単位、`ia<Left>b<Esc>u` は 2 単位

## 結果

得られるもの: `d c y p P` ＋回数＋移動が Vim と同じに動き、INSERT 1 回が undo 1 単位になる。VISUAL は「範囲を決める」の 1 本に選択を渡す形で足せる。
失うもの: `Edit` は 1 つの連続した置換なので、INSERT の中で矢印で動いてから打った文字は別の単位になる（Vim も同じ）。レジスタを LF に正規化するので、CR だけの改行（`fileformat=mac`）の文書は最初から扱っていない。
正直に: `cw` の特例・`p` のキャレット・`yk` の行き先は oracle の結果に合わせる（実装の前に規則を書き切らない）。`e` の語の切れ目は `w` と同じ表（ハングル・絵文字は未測のまま）。

2026-09-18 に oracle で実測して分かったこと（Issue #43 の実装は下に合わせてある。詳細と SHA は `docs/quality/gate-proofs.md` 5-g）:

- **`ia<Left>b<Esc>u` は oracle では `hello` になる**（この実装は決定 5 のとおり 2 単位で `ahello`）。これは `:normal!` の 1 回がまるごと 1 つの undo 単位になるという
  **oracle の限界**であって、対話の Vim の振る舞いではない。対話の Vim は `:help ins-special-special` のとおり矢印・Home / End で単位を切る
  （"The changes (inserted or deleted characters) before and after these keys can be undone separately"）＝決定 5 のまま。
  fixture には置けない（置けば oracle の限界のほうに落ちる）ので、単体テスト `verify_vim_insert_motion_breaks_the_unit` が「切れること」を守る
- 行単位のオペレータと `j` `k` の回数は、**最終行（最初の行）にいるときだけ**失敗し、そうでなければ本文の端で止まる（Vim の `cursor_down` / `cursor_up`）。`5dd` は 3 行の本文を全部消す
- 範囲の規則は 3 つで同じ 1 本（決定 2）だが、Vim には**オペレータで違う 2 つの後処理**がある: exclusive な移動の言い換え（`:help exclusive`。`d` `c` `y` 共通）と、
  複数行にまたがる文字単位の削除だけが行単位になる `op_delete` の規則（`c` と `y` には無い）。`dw` が空行を丸ごと消すのも `2D` が行単位になるのもこれで、レジスタの種類は `V` になる
- `$` は回数を取る（`2$` は 1 行下の行末）。`D` `C` はその `$` に回数を渡す＝決定 6 の「`D` は `d$`」は回数つきでも成り立つ

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `VimEffect` を列（`std::vector<VimEffect>`）にして `c` を「削除 ＋ INSERT へ」の 2 つで返す | モードは `VimState` が既に持つ。列にすると controller が「途中で失敗したら」を考える経路が増える。1 鍵 1 効果のまま足りる |
| レジスタの本文を文書の改行のまま持つ | engine は `line_ending()` を知らない。最終行の `dd` が足す改行の形を engine が決められず、改行の正典が 2 つになる（ARC-009） |
| Backspace の undo 単位を `coalesce` の規則を緩めて解く | 通常モードの Backspace は separate（ADR 0009 の決定 3）。規則を混ぜると通常モードの単位が変わる。Vim の単位は「INSERT 1 回」で別の概念 |
| INSERT の単位を controller が「INSERT に入ったときの履歴の長さ」で覚えて Esc で畳む | 履歴の外に第 2 の状態を持つ。`EditHistory` の中で `Edit` 1 つに畳めば、undo / redo / 保存の印がそのまま効く |
| 回数の掛け算を `VimState::count` に積算する | `2d` のあとの `3` は新しい回数で、`23` ではない。オペレータが自分の回数を持つのが Vim の構造（`opcount` と `count`） |
| fixture の `keys` を `:normal!` の列にして undo の単位を oracle で見る | `:normal!` 1 回の中の境界は見えないままで、偶然の一致（両方 1 単位）と本当の一致を見分けられない |

## 関連

ADR 0005（自前 Vim・oracle）・ADR 0009（`Edit`・undo の区切り）・ADR 0010（改行の形は文書の性質）・ADR 0012（エンジンの形・oracle の限界）。
