# ADR 0033 — VISUAL の変更の `.` は範囲の大きさを記録し、選び直してから鍵を再生する

- 状態: 受理（2026-09-22 に固定 Vim 9.1 で実測し、決定 5・6 を実測に合わせて直した）
- 日付: 2026-09-22
- Issue: #91
- 影響する規則: ARC-001 / ARC-004 / ARC-007 / CPP-002 / CPP-004 / CPP-006 / CPP-011 / CPP-012 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / CNF-010

## 文脈

ADR 0030 の決定 5 は、VISUAL / 行単位 VISUAL で完了した変更を直前の変更として **残さず**、`.` は何もしないと定めた（古い NORMAL の変更を再生して意図しない編集になるより安全だから）。固定 Vim 9.1 は VISUAL の変更を「範囲の大きさ」で再生する（`:help visual-repeat`。行単位は同じ行数、文字単位の 1 行内は同じ文字数、複数行は同じ行数と最終行の同じ桁）。Issue #87 の実測（`out/issue87-oracle/probe.py` の `visual-*`）では `vlld.` が `ghij`、`xjvlld.` が `bcdefghij` / `qrst`、`Vdj.` が 2 行、`vllcZZ<Esc>j0.` が `ZZdefghij` / `ZZnopqrst` になる。

ADR 0030 は `.` を「鍵の列の再生」で閉じ、記録の形を命令の種類に依存させなかった。VISUAL の再生に必要なのは「どの範囲を選び直すか」だけで、選び直した後は既存の VISUAL の鍵がそのまま意味を定める。ADR 0030 の却下表にある「VISUAL の鍵をそのまま再生する」は、motion の到達点が変わるので採らない。

Vim ソースは読まない。help と実測を根拠にし、実測で食い違ったら決定を直す（fixture を都合よく変えない）。

### 実測（2026-09-22・Issue #91・固定 Vim 9.1）

実装の前に 138 ケースを測った（`out/issue91-oracle/probe.py` 71 件・`probe2.py` 31 件・`probe3.py` 23 件・`probe4.py` 13 件）。すべて命令の切れ目で `:normal!` を区切っている（gate-proofs 5-v の教訓）。決定 1〜4・7〜9 は提案のまま通り、**決定 5 と 6 を実測に合わせて直した**。

- **決定 6 は違った。`N.` は VISUAL の記録では回数を使わない。** `vlld` `2.` は 3 文字、`Vd` `2.` は 1 行、`vjld` `3.` は 2 行、`vllcZ<Esc>` `2.` は `Z` を 1 つ、`vllrZ` `2.` は 3 文字で、どれも回数が効かない。続く `.` も影響を受けない（`vlld` `2.` `.` は 3 文字ずつ）。回数の置換（ADR 0030 の決定 6）は NORMAL の記録だけの規則である。
- **決定 5 の Tab は違った。Vim は仮想桁で数える。** `ab<Tab>cd` で `vll`（3 code point・仮想桁 1〜8）を記録すると、`.` は次の行から **8 文字**消す。最終行の桁も仮想桁で、`x<Tab>yz` の Tab を端にした記録は 8 桁として再生される。全角も 1 文字 2 桁で、`abc` の記録はひらがなの行で 2 文字、`あいう` の記録は ASCII の行で 6 文字になる。**この engine は `Column`（code point・1 始まり）を `wanted_column` から `H M L` まで一貫して使っており、仮想桁は `Ctrl-v` と同じ前提（`virtcol`）が要る。** 本 ADR は桁を code point 単位のままにし、Tab と幅の混ざった本文では固定 Vim と食い違うことを既知の差として残す（該当 fixture は採らず、`--vim-dot` の対象 unit が **この engine の答え**を守る）。幅の揃った本文（ASCII だけ・全角だけ）では両者が一致するので、その fixture は採った。
- **決定 5 の桁の意味を実測で 2 つ確定した。**（a）1 行の記録は「桁の個数」でキャレットからの相対、（b）複数行の記録は「最終行の**絶対**桁」でキャレットの桁に依らない。`vjld`（最終行 2 桁）を 5 桁目から再生すると最終行は 2 桁で終わる。絶対桁がキャレットより左に来ると選び直しは後ろ向きになり、Vim も同じ範囲を取る。
- **決定 5 に `$` の記録を足した。** `v$` で選んだ範囲は桁に凍らせず「行末まで」のまま覚える（Vim の curswant が MAXCOL に残る）。3 文字の行で `v$d` した記録は 10 文字の行を丸ごと消す。`j` をまたいでも行末のままで、`h` で戻せば普通の桁になる。この engine では `VimWantedColumn` の `at_line_end` がそのまま同じ意味を持つので、記録は `VimColumnWish` を 1 つ持つ形にした。
- **決定 4 に「記録は畳まれても縮まない」を足した。** 桁や行が足りずに畳まれた再生のあとでも、次の `.` は元の大きさで繰り返す（`vlld` を 2 文字の行で再生してから別の行で `.` すると 3 文字）。`vim_step` は再生中も記録し直すので、`.` の step が「大きさだけを持つ記録」を種として `recording` に置き、再生した鍵はその大きさをそのまま確定する。
- 決定 3 の「VISUAL の中でオペレータの前に打った回数」は Vim では効かない（`vll2d` は `vlld` と同じ）。記録に回数は載せない。
- `vlcZ<Home>Y<Esc>` の `.` は挿入だけを繰り返す（ADR 0030 の決定 4 の「移動のあとは `i` から取り直す」が VISUAL の大きさも落とす）。実測どおりで、engine も同じになった。
- VISUAL の `p` は Vim では「そのときの無名レジスタで置き換え」だが、この engine は VISUAL の `p` を持たない（Issue #91 の範囲外）。probe に記録だけ残した。
- undo の単位は oracle では測れない。`:normal!` を script から呼ぶと命令のあいだで undo が同期されず、`vlld` `.` `u` が両方を戻してしまう。`.` 1 回が undo 1 単位であることは `--vim-dot` の対象 unit が確かめる。

## 決定

**VISUAL で完了した変更は「範囲の大きさ」と「VISUAL を終えた鍵の列」を記録し、`.` はキャレットから同じ大きさの範囲を選び直してから、その鍵を同じ `accept` 経路へ再生する。記録の形は ADR 0030 の `VimRepeatRecord` に大きさを 1 つ足すだけで、再生の経路は増やさない。**

1. **記録の形**: `VimRepeatRecord` に `std::optional<VimVisualExtent> visual` を足す。`VimVisualExtent` は閉じた和型 `std::variant<VimCharacterExtent, VimLineExtent>`。`VimCharacterExtent` は `lines`（範囲の行数・1 以上）と `wish`（`VimColumnWish`。`at_line_end` は `$` で選んだ「行末まで」）と `column`（`at_column` のときだけ見る。1 行なら桁の個数、複数行なら最終行の絶対桁）、`VimLineExtent` は `lines` だけ。空は「NORMAL の記録」を表す。本文・履歴・レジスタは持たない（ARC-004）。
2. **記録の開始**: `v` / `V` で VISUAL に入ったら `recording` を空から取り直す（ADR 0030 の決定 5「捨てる」を「取り直す」に置き換える）。VISUAL の中の移動・選択の伸縮・`o`・種類の切替（`v` ↔ `V`）は鍵として記録しない（大きさが表す）。Esc で NORMAL に戻れば捨てる。
3. **記録の確定**: VISUAL で受けた鍵が変更を起こしたとき（`d x c r`。分けるのは鍵ではなく効果で、ADR 0030 の決定 3 と同じ 6 効果）、そのときの選択から大きさを `VimVisualExtent` として記録に載せ、変更を起こした鍵から後ろを鍵の列に残す（`r` は次の文字まで、`c` は INSERT の鍵を Esc まで・ADR 0030 の決定 4 と同じ）。yank（`y`）は変更ではないので捨てて、直前の変更は変えない。**回数は載せない**（決定 6・実測）。VISUAL の `p` `P` は engine がまだ持たないので対象外（Issue #91 の範囲外）。
4. **`.` の再生**: 直前の変更が `visual` を持つとき、`.` の step は `VimState.mode` を記録の種類の VISUAL に切り替え、効果 `VimReplay` に `reselect`（`std::optional<VimVisualExtent>`）を載せて返す。controller は `reselect` があれば core の純関数 `vim_visual_reselect(text, caret, extent) -> Selection` で選択を作り、`VimSelect` と同じ写しで `EditorState` に置いてから、鍵を順に `accept(VimKeyPress)` へ流す（ADR 0030 の決定 7 のとおり前後で履歴を閉じる）。NORMAL の記録では `reselect` が空で、いまと同じ。**大きさは記録を始めた 1 回だけ測る。** `.` の step は同じ大きさを持つ空の記録を `recording` に種として置き、再生した鍵はそれを確定するので、桁や行が足りずに畳まれても記録は縮まない（実測）。
5. **選び直しの規則**（`vim_visual_reselect`）: anchor はキャレット。`$` で選んだ記録（`VimColumnWish::at_line_end`）は行の内容の終わり（Vim が NUL を置く桁）まで＝ `vim_visual_range` が改行を含める。桁の記録は、文字単位 1 行内ならキャレットの桁から `column` 個ぶん、文字単位複数行なら `lines - 1` 行下の行の **絶対** `column` 桁。行が足りなければ最終行まで、桁が足りなければ行の内容の終わりまで畳む（`TextBuffer::offset_of` がそこで止まる）。絶対桁がキャレットより左なら選び直しは後ろ向きになる。行単位は `lines` 行（足りなければ最終行まで）。桁は `Column`（code point・1 始まり）で数える。**固定 Vim は仮想桁で数えるので、Tab と幅の混ざった本文では答えが違う**（上の実測。`virtcol` は `Ctrl-v` と同じ後続）。
6. **回数**: `N.` は VISUAL の記録では **使わない**（実測。大きさが範囲を決める）。`.` に付いた回数も記録の回数も再生に影響せず、続く `.` にも引き継がれない。ADR 0030 の決定 6 の回数の置換は NORMAL の記録だけの規則である。VISUAL の記録は回数を持たない（VISUAL の中でオペレータの前に打った回数も Vim では効かない）。
7. **undo と再生の非再帰**: `.` 1 回が undo 1 単位（ADR 0030 の決定 7 の 2 つの区切り）。再生の鍵に `.` は含まれないので再帰は深さ 1。再生が同じ `vim_step` を通るので、engine は決定 4 の種を確定して同じ大きさを持つ記録に戻し、`..` が続く。
8. **oracle**: `vlld.` は 1 回の `:normal!` で測れる（ビープしない）。候補は `out/issue91-oracle/add-fixtures.py` が「命令の切れ目で区切った形」と「1 回の `:normal!` の形」の両方で測り、食い違う候補を機械的に拒否する（#93 / #100 と同じ要領）。fixture にできないもの（CRLF・undo 単位・ビープの後ろに鍵が続く列・Tab と幅の混ざった本文）は `--vim-dot` の対象 unit が守る。
9. UI・IME・描画・保存形式は変更しない。

## 強制

- 記録の分岐漏れ・和型の写し漏れ: **active**（`std::visit` と `switch` の網羅性・CPP-002）
- 大きさの記録・選び直し・回数を使わないこと・undo 単位: **active**（`--vim-dot` の対象 unit と oracle fixture 78 件・CNF-010）
- 桁を code point で数えること（Vim の仮想桁との差）: **active**（`--vim-dot` の対象 unit が engine の答えを固定する。fixture には採らない）
- 再生が UI を経由しないこと: **active**（controller の unit が窓なしで通す）
- 期待値が本物の Vim の答えであること: **不能**（CI に Vim は無い）

## 結果

得られるもの: VISUAL の `d x c r` が `.` で繰り返せ、ADR 0030 の決定 5 の穴が閉じる。記録の形は大きさ 1 つ増えるだけで、VISUAL の鍵の意味は既存の表がそのまま定める。
失うもの・残る穴: **桁は code point 単位なので、Tab と幅の混ざった本文では固定 Vim（仮想桁）と答えが違う**。幅の揃った本文では一致する。直すには `virtcol`（表示幅の表と tabstop）が要り、`Ctrl-v`（矩形）と同じ前提なので一緒に扱う。`Ctrl-v` の大きさは後続（`VimVisualExtent` に 3 つめの値を足す）。`gv` は範囲外。VISUAL の `p` `P` はまだ engine に無いので、足すときに同じ記録へ乗る（Vim はそのときの無名レジスタを使う・実測）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| VISUAL の鍵（`v` と移動）をそのまま再生する | motion の到達点が変わり、Vim の「同じ大きさ」と食い違う（ADR 0030 で却下済み） |
| 選び直しを engine の中で行い `VimSelect` と `VimReplay` の 2 効果を返す | 1 鍵 1 効果を崩す。`reselect` を `VimReplay` に載せれば効果は 1 つのまま |
| 選択の正本を `VimState` に持たせて engine が選び直す | 選択の正本は `EditorState`（ADR 0018 の決定 3）。二重に持たない |
| 決定 5 のまま（VISUAL の `.` は何もしない） | 安全だが Vim と違い、`vllc` のあと `.` が効かないのは使う側が最初に気づく穴 |
| 桁を仮想桁で数えて Vim に完全に合わせる | 表示幅の表と tabstop を core に足すことになり、`wanted_column` から `H M L` まで engine 全体の桁の意味を変える。`Ctrl-v` と同じ前提なので別の Issue で一度に決める |
| 再生のあいだ記録を書き換えない旗を `VimState` に持つ | 「いま再生中か」を状態に持つと純関数の意味が増える。`.` の step が大きさだけの記録を種として置けば、既存の記録の経路がそのまま同じ大きさを確定する |
