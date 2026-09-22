# ADR 0033 — VISUAL の変更の `.` は範囲の大きさを記録し、選び直してから鍵を再生する

- 状態: 提案（実測で決定 1〜7 を確認したら受理へ更新する）
- 日付: 2026-09-23
- Issue: #91
- 影響する規則: ARC-001 / ARC-004 / ARC-007 / CPP-002 / CPP-004 / CPP-006 / CPP-011 / CPP-012 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / CNF-010

## 文脈

ADR 0030 の決定 5 は、VISUAL / 行単位 VISUAL で完了した変更を直前の変更として **残さず**、`.` は何もしないと定めた（古い NORMAL の変更を再生して意図しない編集になるより安全だから）。固定 Vim 9.1 は VISUAL の変更を「範囲の大きさ」で再生する（`:help visual-repeat`。行単位は同じ行数、文字単位の 1 行内は同じ文字数、複数行は同じ行数と最終行の同じ桁）。Issue #87 の実測（`out/issue87-oracle/probe.py` の `visual-*`）では `vlld.` が `ghij`、`xjvlld.` が `bcdefghij` / `qrst`、`Vdj.` が 2 行、`vllcZZ<Esc>j0.` が `ZZdefghij` / `ZZnopqrst` になる。

ADR 0030 は `.` を「鍵の列の再生」で閉じ、記録の形を命令の種類に依存させなかった。VISUAL の再生に必要なのは「どの範囲を選び直すか」だけで、選び直した後は既存の VISUAL の鍵がそのまま意味を定める。ADR 0030 の却下表にある「VISUAL の鍵をそのまま再生する」は、motion の到達点が変わるので採らない。

Vim ソースは読まない。help と実測を根拠にし、実測で食い違ったら決定を直す（fixture を都合よく変えない）。

## 決定

**VISUAL で完了した変更は「範囲の大きさ」と「VISUAL を終えた鍵の列」を記録し、`.` はキャレットから同じ大きさの範囲を選び直してから、その鍵を同じ `accept` 経路へ再生する。記録の形は ADR 0030 の `VimRepeatRecord` に大きさを 1 つ足すだけで、再生の経路は増やさない。**

1. **記録の形**: `VimRepeatRecord` に `std::optional<VimVisualExtent> visual` を足す。`VimVisualExtent` は閉じた和型 `std::variant<VimCharacterExtent, VimLineExtent>`。`VimCharacterExtent` は `lines`（範囲の行数・1 以上）と `columns`（1 行なら選んだ code point の数、複数行なら最終行の端の桁を code point で数えたもの）、`VimLineExtent` は `lines` だけ。空は「NORMAL の記録」を表す。本文・履歴・レジスタは持たない（ARC-004）。
2. **記録の開始**: `v` / `V` で VISUAL に入ったら `recording` を空から取り直す（ADR 0030 の決定 5「捨てる」を「取り直す」に置き換える）。VISUAL の中の移動・選択の伸縮・`o`・種類の切替（`v` ↔ `V`）は鍵として記録しない（大きさが表す）。Esc で NORMAL に戻れば捨てる。
3. **記録の確定**: VISUAL で受けた鍵が変更を起こしたとき（`d x c r p P` など。分けるのは鍵ではなく効果で、ADR 0030 の決定 3 と同じ 6 効果）、そのときの選択を `vim_visual_range` で範囲に直し、大きさを `VimVisualExtent` として記録に載せ、変更を起こした鍵から後ろを鍵の列に残す（`r` は次の文字まで、`c` は INSERT の鍵を Esc まで・ADR 0030 の決定 4 と同じ）。yank（`y`）は変更ではないので捨てて、直前の変更は変えない。回数は VISUAL の中でオペレータの前に打った回数があればそれ（無ければ空）。
4. **`.` の再生**: 直前の変更が `visual` を持つとき、`.` の step は `VimState.mode` を記録の種類の VISUAL に切り替え、効果 `VimReplay` に `reselect`（`std::optional<VimVisualExtent>`）を載せて返す。controller は `reselect` があれば core の純関数 `vim_visual_reselect(text, caret, extent) -> Selection` で選択を作り、`VimSelect` と同じ写しで `EditorState` に置いてから、鍵を順に `accept(VimKeyPress)` へ流す（ADR 0030 の決定 7 のとおり前後で履歴を閉じる）。NORMAL の記録では `reselect` が空で、いまと同じ。
5. **選び直しの規則**（`vim_visual_reselect`）: anchor はキャレット。文字単位 1 行内は anchor から `columns` 個の code point（行の内容の終わりを越えない・足りなければそこまで）。文字単位複数行は `lines - 1` 行下の行の `columns` 桁（その行がそれより短ければ内容の終わり・文書末で行が足りなければ最終行）。行単位は `lines` 行（足りなければ最終行まで）。Tab は 1 code point として数える（Vim の help は「同じ文字数」と言う。仮想桁で数えていたら実測で直す）。
6. **回数**: `N.` は ADR 0030 の決定 6 のとおり記録の回数を置き換え、再生では回数の桁をオペレータの鍵の前に展開する（大きさは変えない）。VISUAL の命令は普通は回数を持たないので、`vlld` `2.` / `Vd` `2.` / `vp` `2.` で Vim が回数をどう扱うか実測し、食い違えばこの決定を直す。
7. **undo と再生の非再帰**: `.` 1 回が undo 1 単位（ADR 0030 の決定 7 の 2 つの区切り）。再生の鍵に `.` は含まれないので再帰は深さ 1。再生が同じ `vim_step` を通るので、engine は決定 3 のとおり同じ大きさで記録し直し、`..` が続く。
8. **oracle**: `vlld.` は 1 回の `:normal!` で測れる（ビープしない）。文書末で範囲が足りない場合・空行・UTF-8・Tab・CRLF は境界だけ実測する。CRLF は #100 と同じ理由で fixture にできず unit で守る。
9. UI・IME・描画・保存形式は変更しない。

## 強制

- 記録の分岐漏れ・和型の写し漏れ: **active**（`std::visit` と `switch` の網羅性・CPP-002）
- 大きさの記録・選び直し・回数の置換・undo 単位: **active**（`--vim-dot` の対象 unit と oracle fixture・CNF-010）
- 再生が UI を経由しないこと: **active**（controller の unit が窓なしで通す）
- 期待値が本物の Vim の答えであること: **不能**（CI に Vim は無い）

## 結果

得られるもの: VISUAL の `d x c r p` が `.` で繰り返せ、ADR 0030 の決定 5 の穴が閉じる。記録の形は大きさ 1 つ増えるだけで、VISUAL の鍵の意味は既存の表がそのまま定める。
失うもの・残る穴: `Ctrl-v`（矩形）の大きさは後続（`VimVisualExtent` に 3 つめの値を足す）。`gv` は範囲外。VISUAL の `p` の再生はそのときの無名レジスタを使う（Vim も同じ・実測で確かめる）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| VISUAL の鍵（`v` と移動）をそのまま再生する | motion の到達点が変わり、Vim の「同じ大きさ」と食い違う（ADR 0030 で却下済み） |
| 選び直しを engine の中で行い `VimSelect` と `VimReplay` の 2 効果を返す | 1 鍵 1 効果を崩す。`reselect` を `VimReplay` に載せれば効果は 1 つのまま |
| 選択の正本を `VimState` に持たせて engine が選び直す | 選択の正本は `EditorState`（ADR 0018 の決定 3）。二重に持たない |
| 決定 5 のまま（VISUAL の `.` は何もしない） | 安全だが Vim と違い、`vllc` のあと `.` が効かないのは使う側が最初に気づく穴 |
