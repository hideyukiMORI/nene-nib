# ADR 0152 — Vim のマークは文書の値で、engine は借りて読み、変更を `VimStep` に添えて返す。編集で行がずれたら純関数で直す

- 状態: 提案（実装席 2026-10-21。merge は独立レビュー・CI・hide の試用の後・D41）
- 日付: 2026-10-21
- Issue: #767
- 規則: ARC-001 / ARC-004 / ARC-007 / CPP-002 / CPP-003 / CPP-011 / CPP-012、ADR 0012（移動の表）/ 0015 / 0031（オペレータと範囲の規則 1 本）/ 0019 / 0027 / 0032（`last_search` は窓に 1 つ）/ 0042 / 0052（`Edit.restore`）/ 0056（文書の束）/ 0059（`UnloadedDocument`）
- 測定: oracle（Vim 9.1）の fixture 110 件（`percent-` 33・`mark-` 33・`jump-mark-` 23・`change-mark-` 21）と、oracle に流せない失敗（E20 E19 は `-es` の Vim がエラーで終わる）を scope `--vim-marks` の手書きの契約で

## 文脈

Nib の Vim には `%` とマーク（`m{a-z}` `'{a-z}` `` `{a-z} `` `''` `` `` `` `'.` `` `. ``）が無かった。マークは Vim ではバッファごとの値で、行の挿入・削除で行が動き（`mark_adjust`）、行の中の編集では桁が動かない。飛ぶ鍵（`:help jump-motions`）は飛ぶ前の位置を `''` に置き、動かなかったら元へ戻す（`checkpcmark`）。

engine は純関数で、本文・選択・表示領域を借りて読み、効果を返す（ADR 0012 の決定 1・ADR 0019 の決定 1）。窓に 1 つの `VimState` は検索の記憶（ADR 0032）やレジスタを持つが、文書ごとの値は `DocumentState` が持つ（ADR 0056）。

## 決定

1. **マークは `core::VimMarks`（公開 aggregate・比較は非メンバー）。** 名前つきの 26 本 `named`、飛ぶ前の位置 `previous_context`（`''` `` `` ``）、最後の変更 `last_change`（`'.` `` `. ``）と、飛ぶ鍵が退けた前の値 `prior_context`（Vim の `w_prev_pcmark`・決定 5）。位置は `VimMarkPosition{line, byte}`（行と行頭からのバイトの桁・Vim の `pos_T`）で、Offset では持たない（行の中の編集で桁を動かさないため）。
2. **所有は文書。** `DocumentState.marks` と `EditorState` のアクティブな文書の欄（`display` と同じ所有）。切り替え・閉じる・束にするときに一緒に運び、開いた文書・新しいタブ・まだ読んでいない文書（ADR 0059）は空。窓に 1 つの `VimState` には入れない（Vim もバッファごと。`last_search` が `VimState` にあるのは検索が Vim でもグローバルだから）。engine は `VimEditorView.marks`（借用の `const VimMarks *`・既定は空の値を指し null にしない）で読み、変えるときは `VimStep.marks`（新しい値）に添える。controller の `step_vim` が**効果を写す前に**置く（飛ぶ鍵が置いた前の位置もオペレータの編集で直るように。Vim も `setpcmark` の後に削除の `mark_adjust` が走る）。
3. **鍵。** `m` `'` `` ` `` は次の 1 鍵を待つ接頭辞（`VimPrefix::mark` `mark_line` `mark_exact`）。`'{name}` `` `{name} `` は名前を解いて着地を `VimEditorView.mark_landing` に置き、移動の表の 1 本（`VimMotion::mark_line` 行単位・`mark_exact` exclusive の文字単位）へ写す。オペレータ・VISUAL・`.` は既存の移動の道を通る（`d'a` は行単位、``d`a`` は exclusive の規則を通り 1 行の中でも `"1` へ行く）。名前は `a`〜`z`・`'` `` ` ``（直前の位置）・`.`（最後の変更・読むだけ）。置いていないマークは失敗（E20）、本文の外の行も失敗（E19）。`m` は `a`〜`z` と `'` `` ` `` を置き、Vim が受けるがこの版が持たない名前（`A`〜`Z`・`0`〜`9`・`"` `[` `]` `<` `>`）は何もしない（失敗にしない）、ほかは失敗。`m<Esc>` `'<Esc>` は何もせず失敗にしない。オペレータの後ろの `m` は Vim の `nv_mark` と同じに打ち消す。`m` は `.` で再生されない（本文を変えない）。
4. **飛ぶ鍵は前の位置を置く。** 移動の表の `vim_motion_is_jump`（`gg` `G` `{N}G` `H` `M` `L` `%` `'x` `` `x ``）と見つかった検索（`/` `?` `n` `N` `*` `#`）。NORMAL・VISUAL・オペレータの後ろのどれでも置く。失敗した移動は置かない。
5. **鍵の終わりの確かめ（Vim の `checkpcmark`）。** `step_vim` の終わりに `vim_marks_checked`: 飛ぶ鍵が置いた前の位置が今のキャレットと同じか、編集で消えていたら退けた値へ戻し、`prior_context` を消す（`GG` は `''` を変えない・`y'a` を下へ取ったら変えない・`dG` で消えたら戻す。oracle で確かめた）。
6. **編集で直す。** `marks_adjusted(marks, 編集の前の本文, Edit)`（core の純関数・Vim の `mark_adjust`）を controller の `replace`・undo・redo の 3 か所で呼ぶ。編集の形（消した本文と入れた本文の共通の頭と尾を除いた後）から Vim が操作ごとに呼ぶ形を当てる: 行単位の削除（行の先頭から改行まで）・最後の行までの削除・本文の全体・行単位の変更（`cj`）は消えた行のマークを消し下を上げる。改行と字下げだけの削除（`J` `gJ`）はつないだ位置へ自然に写す。ほかの文字単位の行またぎの削除は Vim の `op_delete` と同じに最後の行のマークの桁へ最初の行の残りの長さを足す（引かない・oracle の実測）。行を入れた編集は、行の先頭へ改行で終わる本文を入れたら（`P` `O`）その行から下、ほかは下の行だけを押し下げる（`i<CR>` は分けた行のマークを桁ごと残す）。同じ行の中の編集は桁を動かさない（使う所で行の内容の終わりへ寄せる）。
7. **最後の変更。** `replace` が `vim_last_change_of` で置く: 編集の始まりの位置。ただし最終行の `dd` と行の下へ足した行（`o`・最終行の下の `p`）は Vim の `changed_lines` と同じに次の行の行頭（消したなら本文の外で `'.` は E19）、`J` `gJ` は消した次の行の行頭。undo と redo は編集の行の行頭（Vim の `u_undoredo`）。
8. **`%`（ADR 0012 の移動の表の 1 行）。** `VimMotion::matching_pair`。回数なしはキャレットから行の中で最初の `( ) [ ] { }`（'matchpairs' の既定を固定）の相手を入れ子を数えて探す（`vim_matching_pair`・Vim の `findmatchlimit`）。'cpoptions' に `%` が無い既定と同じに、行の `"` が偶数個なら走査が越えた `"` の内側の括弧を数えず（始まりが引用符の中なら外の相手は見つからない）、`'x'` `'\x'` を飛ばし、奇数個の `\` が前にある括弧は始まりと同じときだけ数える。inclusive の文字単位で 1 行の中でも `"1` へ行く。`{N}%`（1〜100）は本文の `(N × 行数 + 99) / 100` 行目の最初の非空白へ行単位、100 超と相手が無いときは失敗（オペレータを打ち消す）。

## 強制

- `VimMotion` `VimPrefix` `VimAction` の値が増えたら `switch` と表の `static_assert` が落ちる（CPP-002 / CPP-012）。
- oracle の fixture 110 件を CTest が再生する（CNF-010 が生成物と json の一致を守る）。oracle に流せない失敗・CRLF・タブの付いていき方・純関数の契約は scope `--vim-marks`。

## 結果

得られるもの: `%` とマークが Vim 9.1 の oracle と一致する。マークの意味は core の 1 か所（`VimMarkEdits`）にあり、engine は借りて読むだけ、controller は置く・直す・確かめるの 3 つを呼ぶだけ。

正直に（限界・後続）:

- **undo はマークを戻さない（契約で今の挙動を固定・#793）。** Vim は undo の単位が作られた時のマークを覚えていて、`u` で消えたマークも戻す（`madd u'a` は Vim では効く）。Nib は undo も同じ `marks_adjusted` で直すだけで、`dd` で消えたマークは戻らない。
- **編集の形から操作を当てる近似。** 空行の上の `o` と `i<CR>`（Vim はマークを残す）は `O` と同じ形の編集なので押し下げる。1 行だけの本文の `dd` は本文が空になるだけでマークを消さない。`3J` は文字単位の削除として桁を足す。本文の全体を文字単位で消す `v…d` は `dG` と同じ形なのでマークを消す（Vim は最初の行とマークを残す・oracle で確かめた）。今の値は契約 `verify_mark_approximations` が固定し、直すのは #793。
- **oracle の `:normal!` に固有の値は fixture にしない。** `-es` の Vim は開いた直後の `''` を最終行に持つ（Nib と非 Ex の Vim は 1 行目・追補 #962）ので、`''` は先に動いた飛ぶ鍵の後でだけ測る。`:normal!` は INSERT の続けた文字を 1 回で入れるので `'.` は最初の文字に付く（打った Vim と Nib は最後の文字）ため、`'.` の fixture は 1 文字の挿入だけ。
- 大文字のマーク `'A`〜`'Z`（ファイルをまたぐ）・`:marks` `:delmarks`・`]'` `['`・`'[` `']` `'<` `'>` `'^` `'"`・ジャンプリスト（`Ctrl-o` `Ctrl-i`）・マークの保存（viminfo）は後続。`%` の `#if` と `/* */` の対応・行末の `\` で続く引用符・'matchpairs' の設定も後続。

## 追補（2026-10-28・#962）: 開いた時点の前の位置は 1 行目

決定 2 の「開いた文書・新しいタブ・まだ読んでいない文書は空」のうち、`previous_context` だけを 1 行目の桁 0（`VimMarkPosition{LineNumber{1}, 0}`）に改める。本物の Vim 9.1（非 Ex・`--not-a-term`）はバッファを開いた時点で pcmark を (1, 1) に置き（`getpos("''")` は `[0, 1, 1, 0]`）、前の跳びが無い `''` は 1 行目の最初の非空白へ、`` `` `` は 1 行目の桁 0 へ跳んで失敗にしない。Nib は空の値を「跳ぶ先が無い」失敗にして残りの鍵を落としていた（#954 の fuzz で 7 件）。

- 置く所は `VimMarks` の既定値の 1 か所（`previous_context = VimMarkPosition{LineNumber{1}, 0}`）。開いた文書（`with_opened`）・新しいタブ・まだ読んでいない文書を読んだ束・起動の空の文書・`VimEditorView` が既定で借りる `no_vim_marks` は、どれもこの既定の値で始まる。名前つき・`last_change`・`prior_context` は空のまま。
- 跳び方（`''` は行単位で最初の非空白・跳んだ元が新しい前の位置・鍵の終わりの `vim_marks_checked`）は変えない。
- 文書を切り替えたときは文書の値を運ぶだけで初期化しない（Vim もバッファの印を保つ）。
- oracle の `-es` の Vim は最終行で開くので開いた直後の `''` は最終行へ跳ぶ。これは oracle の作りなので fixture にせず、契約 `--vim-marks` の `verify_marks_from_opening` が開いた文書と新しいタブで開いた文書の両方を固定する。
- 自分自身へ着く検索（当たりが自分だけの `*` `#` `/`・回数が折り返して元へ戻る `{N}*`）は鍵の終わりの `vim_marks_checked` で前の位置を跳ぶ前の値へ戻すので、前に跳びの無い fixture では oracle の最終行が `''` に出る（#1028 の `4*''` は Nib の不具合ではなかった）。fixture `star-count-mark-` は先に `m'` で別の行に前の位置を置いてから測る。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| マークを `VimState`（窓に 1 つ）に置く | Vim のマークはバッファごと。タブを切り替えると別の文書の行を指す。文書の値は `DocumentState` が持つ（ADR 0056） |
| マークを Offset で持ち、編集の差で動かす | Vim は行の中の編集で桁を動かさず、行の挿入・削除でだけ行を動かす。Offset では `x` の後の `` `a `` が別の文字を指す |
| controller が効果を写した後にマークを置く | 飛ぶ鍵の前の位置は編集の前の本文の位置で、オペレータの編集（`dG`）で直されないまま残る |
| `mark_adjust` を操作ごとに engine の効果へ添える | 効果の種類ごとに行の動きを書くことになり、undo と redo と普通の入力の道が別になる。`Edit` の形から当てる 1 本にした（近似は限界に書いた） |
| engine が行の挿入・削除の範囲を `VimStep` に添える（独立レビュー Q1） | 近似の例は正確になるが、undo と redo は engine を通らず Edit の形に頼るので、補正の道が 2 本になる。現案のまま近似を契約で固定し、#793 で決める |

## 関連

ADR 0012（移動の表・oracle）・ADR 0032（`last_search`）・ADR 0056（文書の束）・ADR 0059（まだ読んでいない文書）。

## 追補（2026-10-28・#983）: `.` の再生の `%`

`c%` `d%` を `.` で再生して再生の位置の行に括弧が無いとき、`%` は打ったときと同じく失敗する（探索の範囲は変えない・行をまたがない）。その失敗は `.` の失敗になり後ろの鍵を捨てる（ADR 0030 の追補 2026-10-28・fixture `dot-percent-` 10 件）。

## 追補（2026-10-29・#1039）: `'^` と `gi`

- **`'^` は文書の値。** `VimMarks.last_insert` に最後に INSERT を抜けた位置を持つ（Vim の `b_last_insert`）。位置は Esc が 1 文字左へ戻る前のキャレットで、行末の 1 つ右（`A<Esc>` の後は行の内容の終わり）もあり得る。回数の繰り返し（`3ix<Esc>`）は繰り返しを入れた後ろ（`l3ia<CR>b<Esc>` は 4 行目・実測）。
- **置くのは効果を写した後。** 回数の繰り返しは Esc の効果が本文を入れるので、`'^` は写した後の本文の位置になり、写す前に置くと行を足す繰り返しで `marks_adjusted` がずれを掛けられない（入れた行の上の行に残る）。engine は `VimStep.insert_left` に「写した後のキャレットから同じ行を右へ何バイトか」（Esc が戻した分・戻らなければ 0）を添え、controller が効果を写した直後に文書のマークへ置く（`with_insert_left` の 1 か所）。差で持つのは、CRLF の文書で入れた本文の改行が長くなっても同じ行の中の差は変わらないから。却下した選択肢の「controller が効果を写した後にマークを置く」は飛ぶ鍵の前の位置の話で、`'^` は写した後の位置そのものなので当たらない。
- **controller の割り込みで INSERT を抜けたときは `'^` を置かない。** タブの切り替え（`vim_switched_document` が NORMAL へ戻す）など engine の Esc を通らない道は `insert_left` を返さないので、`'^` は前の値のまま残る（Vim の `b_last_insert` は Esc 以外でも置かれる場合があるが、この版は Esc の 1 本に限る）。
- **編集の行のずれ。** `marks_adjusted` が `'.` と同じ規則（Vim の `one_adjust`）で動かす。消えた行の `'^` は消え、そのときの `gi` は `i` と同じくキャレットの所で入る（`ix<Esc>ddjgiQ<Esc>`・実測）。
- **`gi`。** `'^` の行（本文の外なら最後の行）の桁で `i` を始める。桁は行の内容の終わりまで寄せる（`lliQ<Esc>0xxgiR<Esc>` は行末に入る・Vim の `nv_g_cmd`）。`'^` が無ければキャレットの所の `i`。回数は `i` と同じ繰り返し。VISUAL の中とオペレータの後ろ（`dgiw`）は失敗（実測）。
- **`.` の記録は `i`。** Vim の `nv_g_cmd` は命令を `i` に変えてから挿入を始めるので、`gi` の後の `.` は `'^` へ行かずキャレットの所で同じ本文を入れる（`lix<Esc>jgiY<Esc>j.` は 2 行目のキャレットの所・実測。Issue の見込みの「鍵 `gi` と打った本文」と違い、実測を正本にした）。記録の更新の 1 か所（`normal_recorded`）が、`g` の次の鍵で INSERT へ入った記録の末尾の `g` を `i` に置き換える。マクロの録画は打った鍵のまま。
- fixture は `gv-gi-gi-` の 13 件（`gi` の側）。

## 追補（2026-10-29・#1037）: ジャンプの列と変更の列

`Ctrl-o` `Ctrl-i`（`<Tab>`）のジャンプの列と、`g;` `g,` の変更の列を足す。どちらも決定 2 と同じ文書の値で、`VimMarks` の `jumps` と `changes`（型 `VimJumpList`・位置の固定長の配列と件数と今の番号）に持ち、engine は `VimEditorView` で借りて読み `VimStep.marks` に添えて返す。列の意味は `VimJumpLists.cpp` の純関数の 1 か所（`vim_jump_appended` `vim_jumps_cleaned` `vim_jump_moved` `vim_change_moved` `vim_change_recorded`）。値は Vim 9.1 の実測（`getjumplist()` `getchangelist()`）で決めた。

- 積む所は飛ぶ鍵の前の位置を置く `vim_marks_jumped`（Vim の setpcmark）の 1 か所で、`''` を置く鍵がそのまま列にも積む。`m'` `` m` `` も積む（setmark_pos は setpcmark を呼ぶ）。積むときは同じ行を消さず、上限 100 を越えたら最も古い位置を捨てる。
- 同じ行の位置は歩く前に後ろの 1 つだけ残して消す（cleanup_jumplist・`jumpoptions` は空）。`Ctrl-o` の後に別の所へ跳んでも先の位置は切らない（stack ではない。Issue の見込みは「切れる」だったが、実測は切らない）。
- `Ctrl-o` は末尾の後ろにいれば今の位置を飛ぶ鍵と同じに積んで（`''` も置く）から戻る。列の端を越える回数は鳴らす失敗で動かない。着地は `` `a `` と同じ（NORMAL の寄せ・欲しい列は着地の桁）。オペレータの後ろと VISUAL では失敗する（checkclearopq）。
- 変更の列は controller の `replace` が `'.` と同じ位置で置く（`vim_change_recorded`）。新しい undo の単位を始めた編集（履歴の位置が進んだ。`.` と `@` の再生の中は最初の編集だけ＝Vim では再生の全体が 1 単位）で印 `change_started` を立て、印があって最後の位置と違う行か 79 桁より離れていれば足して印を下ろす。どちらでも最後の位置を今の変更にし、今の番号を末尾の後ろにする（Vim の changed_common と b_new_change）。undo と Ctrl-r は印を立てない。
- `g;` `g,` の回数は列の端で止まり、すでに端にいるときだけ失敗する（E662 E663・空の列は E664・本文の外の行は E19）。
- 編集の行のずれは `marks_adjusted` が名前つきと同じ形で直し、消えた行の位置は消さずに消えた最初の行へ置く（one_adjust_nodel。文字単位の削除の中の行は最初の行へつなぐ）。
- 鍵: NORMAL の表に `<Tab>`（文字 0x09＝`Ctrl-i`）、`g` の表に `;` `,`、特別な鍵 `control_o`。fixture の記法 `<C-o>` と `<Tab>` を `KEY_TABLE` に足した（`<C-i>` は同じ鍵 0x09 なので別の名前にしない。表の名前は鍵と 1 対 1 に往復する・#1081 の契約。Vim の表示名も `<Tab>`）。

限界: oracle は鍵を 1 本の `:normal!` に流すので undo の単位が 1 つしか無く、変更の列は fixture では 1 件になる。また打った INSERT の文字を先読みでまとめて入れるので、`'.` と変更の列の位置が打鍵ごとの Vim と違う。複数の単位の列・E662〜E664・E19 は契約 `--vim-marks` の `verify_change_list` が、単位ごとに `:normal!` を分けた Vim の測定値で固定する。INSERT の `<BS>` が挿入の始まりより前を消すと Nib の履歴は新しい単位になるが Vim は同じ単位のままで、行をまたいだときだけ変更の列が違う。fuzz（`eng/vim-fuzz.py`）は `g;` `g,` を語にしない（1 本の `:normal!` は 1 単位）。`-es` の Vim は開いた最終行をジャンプの列に持つので、fixture は `Ggg` で始めて両方の列を揃える。`<Tab>` は窓から文字 0x09 で届く。窓の Ctrl+O は Vim モードではジャンプの戻り（`control_o`・INSERT は engine が何もしない）で、開くは Vim では Ctrl+Shift+O・F1 の一覧・Ctrl+P（`operation_bindings` の Ctrl+O の行は通常モードだけ・D71）。窓の Ctrl+I は `WM_CHAR` が Ctrl を押した鍵を捨てるので届かない（後続）。session への保存・`:jumps` `:changes`・タブをまたぐ跳びは後続。

## 追補（2026-10-29・#1116）: 開いた位置の 1 項と oracle の揃え方

- **文書のジャンプの列は開いた位置の 1 項で始まる。** 普通に起動した Vim（`--not-a-term` の実測）は開いた直後の `:jumps` に開いた位置 `1 0` を 1 項持ち、`jj<C-o>` で開いた位置へ戻れる。Nib も文書を開いたときに、開いた位置（1 行目の桁 0）を列に 1 項持って始める。作るのは core の `vim_marks_opened()` の 1 本で、中は `m'` と同じ `vim_mark_set`（別の積み方は書かない）。呼ぶのは本文を読んで文書を作る 3 か所: `OpenDocument` と起動引数の `EditorState::with_opened`、前回のタブを読む `reach_tab` の `reached_bundle`、自動退避から戻す `restored_document`。タブの切り替えでは積まない（列は文書の値・決定 2）。前回のタブの復元で戻すキャレットは開いた後に置くので、列の 1 項は 1 行目のまま。
- **回数つきの `Ctrl-o` の規則は変えない。** Vim 9.1 の `movemark` は `idx + count < 0` なら積む前に失敗し、末尾の後ろにいれば今の位置を積んで 1 つ前から数える。Nib の `vim_jump_moved` は #1037 からこの形で、fuzz seed 45（`?nd<CR>2<C-o>`）の違いは列の起点の 1 項の有無だった（fixture `jumplist-count-` 17 件）。
- **oracle の揃え方。** `-es` の Vim は開いた直後のカーソルが最終行なので列に `(最終行, 0)` を持ち、`call cursor()` は列に積まない。`eng/vim-oracle.py` の probe は、カーソル（viewport の始めの位置を含む）を置いた後に `clearjumps` と `normal! m'` を流し、普通の起動と同じ「置いた位置の 1 項」の列にしてから鍵を流す。測定コードが変わったので全件（3061 件）を再生成し、値が変わったのは #1116 の `jumplist-count-` の 3 件だけだった（既存の 3044 件は不変）。#1037 の限界の「fixture は `Ggg` で始めて両方の列を揃える」は不要になったが、既存の fixture の鍵は変えない。
- 限界: viewport の fixture は oracle が viewport の位置を列に積むが、Nib の再生は開いた（1 行目の）後に `PlaceCaret` で置くので積まない。viewport の fixture にジャンプの鍵を入れない。新しい空の文書（ファイル無しの起動・Ctrl+T）は開く道を通らないので空の列のまま（後続）。
