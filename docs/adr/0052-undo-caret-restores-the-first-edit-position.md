# ADR 0052 — `u` と Ctrl-r の後のキャレットは undo の単位が覚えた戻り先で、戻り先は最初の編集の瞬間のキャレット

- 状態: 受理（設計席 2026-09-29・Issue #208。hide 未確認）
- 日付: 2026-09-29
- Issue: #208
- 影響する規則: FR-003 / ARC-001 / ARC-004 / ARC-005 / CPP-002 / CPP-003 / CPP-011 / QLT-001 / QLT-012 / CNF-010 / CNF-011
- 前提: [ADR 0009](0009-editing-slice-piece-table-and-editing-states.md)（`Edit` と `EditHistory`・決定 3）・[ADR 0012](0012-vim-engine-first-slice-and-oracle-fixtures.md)（1 鍵 1 効果・`u` は INSERT 1 回が 1 単位）・[ADR 0015](0015-vim-operators-register-kind-and-insert-undo-unit.md)（`VimMotionRange`）・[ADR 0018](0018-vim-visual-selection-as-range.md)（VISUAL）・[ADR 0030](0030-vim-dot-repeat-as-key-replay.md) / [ADR 0046](0046-macros-record-keys-and-replay-through-dot-path.md)（再生は 1 つの undo 単位に畳む）・[ADR 0050](0050-numbered-and-small-delete-registers.md)（`VimMotionRange` に印を運ばせる先例）

## 文脈

Nib の `u` は取り消した編集の先頭（`edit.at`）にキャレットを置く（Issue #22 の実測「変わったところの先頭」）。`x` `dd` `dw` ではキャレットと変更の先頭が同じなので合っていたが、`p` `o` `O` `A` や後ろ向き・行単位のオペレータでは Vim と違う（Issue #208・`yyp` の後の `u` は Vim が貼る前の行、Nib は 1 行下）。

本物の Vim 9.1 の実測（Sonnet の probe `out/probes/probe-putcaret-2026-09-29.md` の B 節と `out/probes/probe-undocaret-2026-09-29.md`・330 ケース・変更ごとに undo を区切って測定）と、`undo.c` の `u_undoredo`（`uh_cursor`）の設計席の読み:

- undo の単位は「**最初の変更を保存する瞬間のカーソル**」R を 1 つ覚える。`u` の後のキャレットは R を戻した後の本文で寄せた位置、`<C-r>` の後のキャレットは同じ R（行と桁）をやり直した後の本文で寄せた位置。`u` の後の `j` の桁は戻したキャレットの仮想桁。
- R はオペレータごとに違うが、1 本で言える: **オペレータは範囲の先頭（キャレットと移動の着地の小さい方）へカーソルを動かしてから保存する**。
  - 文字単位（`x 3x dw d$ df diw cw`・後ろ向きの `db d0 dF d^ cb`・行をまたぐ `d/ d? 3dw d<BS>`）: 範囲の先頭。
  - 行単位: 先頭行の上で、キャレットと着地のうち先頭行にある方の桁。`dj` `3dd` は元の桁、`dk` は `k` の着地の桁（欲しい列の仮想桁・Tab の行から (3,1) → (2,8)）、1 行の `dd` `cc` は着地が最初の非空白なので min(元の桁, 最初の非空白の桁)（桁を 1〜26 で掃引して確定）。
  - 行単位の `c` が 2 行以上（`cj`）: 上の位置の **1 行下**（桁は同じ・その行の長さで寄せる）。`op_change` が残りの行を消す間だけカーソルを 1 行下げて最初の保存をするため（機構は読み・規則は実測）。
  - VISUAL: 文字単位は選択の先頭（行末の 1 つ先＝改行の位置に立てる・`vkd` を短い行へ）。行単位は先頭行の上で、動く端が先頭行ならその桁、固定の端が先頭行なら 1 桁目（`Vjd` は 1 桁目・`Vkd` は上の行の着地の桁）。矩形は左上。
- INSERT に入る命令は最初の入力の瞬間のカーソル: `i` は元の位置、`a` は 1 つ右、`I` は最初の非空白、`A` は行末の 1 つ先（`u` は行の長さへ寄せて最後の文字・`<C-r>` は行が長いのでそのまま）、`o` `O` は元の位置。
- 貼り付け（文字単位・複数行・行単位・矩形・回数つき・`p` `P`）と `r` `3rx` は元の位置。
- `.` と `3.` は普通の変更と同じ（`.` を打つ直前から決まる R）。`@a` `2@a` は 1 回の `u` で全部戻り、戻り先はマクロの最初の変更の R。
- 寄せ: 桁が行の長さを越えたら最後の文字。R の行が本文に無い（最終行の `dd` のやり直し）ときは最後の行の最初の非空白。
- 失敗した変更（`9rx`）は履歴に載らない。
- INSERT の中の矢印は打鍵では undo を区切るが、`:normal!` の中では区切らない。

**oracle の制約**: `eng/vim-oracle.py` は鍵を 1 本の `:normal!` に流すので、変更は全部 1 つの undo の塊になる。変更が 1 つだけの鍵列（移動 → 変更 → `u` → `<C-r>`）は fixture にできる。変更が 2 つ以上ある鍵列の `u` は Nib と本文がずれるので fixture にできない。

Nib の今の形（main `2592a57`）: `core::Edit { at, removed, inserted }`。`EditorController::replace` が `Edit` を作って `EditHistory::pushed` に渡し、`absorbed`（INSERT の続き）と `covering_edit`（再生を 1 つに畳む）が `Edit` を作り直す。`undo_edit` / `redo_edit` は戻した本文の末尾にキャレットを置き（通常モードの Ctrl+Z / Ctrl+Y）、`perform(VimUndo)` / `perform(VimRedo)` がその後で `edit.at` へ上書きする。`replace` が呼ばれる時点のキャレットは「その鍵を受ける前のキャレット」で、効果を写す `perform` がキャレットを動かすのは `replace` の後。`Edit{` の構築は src に 5 か所・テストに 38 か所。

## 決定

**`Edit` は「その編集を始める瞬間のキャレット」`restore` を覚える。`replace` がそのときのキャレットを書き、undo の単位を畳む所は最初の編集の値を残す。Vim の `u` と Ctrl-r は `restore` の行と桁へ行き、その時点の本文で寄せる。オペレータと VISUAL の戻り先は engine が決めて 1 打鍵の結果に添え、controller は効果を写す前にキャレットをそこへ置く（Vim のオペレータが範囲の先頭へ動いてから保存するのと同じ形）。**

1. **`Edit.restore`（core）**: `Edit { Offset at; std::string removed; std::string inserted; Offset restore; }`。`restore` は編集の前の本文の上の位置。既定値は付けない（構築する所はすべて書く・書き忘れはコンパイルが落とす）。`operator==` は 4 欄を比べる。テストの 38 か所は値を明示する。
2. **書くのは `replace` の 1 か所**: `restore` は `replace` が呼ばれた時点のキャレット（`state_.selection().caret`）。通常モードの編集も同じ値を持つ。
3. **畳むときは最初の値を残す**: `absorbed`（INSERT の続き）は前の `Edit` の `restore`。`merge_replayed_edits` は再生が積んだ最初の `Edit` の `restore` を `covering_edit` の結果に写す（`.` と `@a` の戻り先は最初の変更の R）。`EditHistory` の結合（coalesce）も前の値。
4. **Vim の `u`（`perform(VimUndo)`）**: 本文を戻した後、キャレットを `restore` へ置く。寄せは NORMAL のキャレットの既存の寄せ（`settle_vim_caret`・行末の 1 つ先は最後の文字）に任せる。`edit.at` への上書きはやめる。
5. **Vim の Ctrl-r（`perform(VimRedo)`）**: やり直す前の本文で `restore` の行とバイトの桁を読み、やり直した後の本文の同じ行と桁へ置く。桁が行の長さを越えたら既存の寄せ、文字の途中ならその文字の先頭、行が本文に無ければ最後の行の最初の非空白。行と桁 → 位置の写しは core の純関数 1 本（`src/core/` の既存の行と桁の関数の隣）。
6. **通常モードの Ctrl+Z / Ctrl+Y は変えない**: 戻した本文の末尾に置く今の形のまま（`restore` は読まない）。Windows の編集の慣習で、Vim の規則に寄せるかは別の判断。
7. **engine が決める戻り先（core）**: `VimStep` に `std::optional<Offset> restore = std::nullopt` を足す（`notice` `clipboard` と同じ「1 打鍵の結果に添える値」）。空は「いまのキャレットのまま」。controller は `step_vim` で、`step.restore` があれば効果を写す前にキャレット（選択は畳む）をそこへ置く。これで決定 2 の 1 本の規則のまま、オペレータの戻り先が `Edit` に入る。
8. **範囲が戻り先を運ぶ**: `VimMotionRange` に `std::optional<Offset> restore = std::nullopt` を足す（空は範囲の先頭 `range.begin`）。範囲を作る所がキャレットと着地を知っているので、行単位の範囲を作る所だけが書く。
   - 行単位の移動（`dj` `dk` `dG` `dgg` など）: 先頭行の上で、キャレットと着地のうち先頭行にある方の桁。
   - `dd` `cc` `yy` の仲間（回数つきを含む）: 着地は範囲の最後の行の最初の非空白。1 行ならキャレットと着地の小さい方、2 行以上ならキャレット。
   - 文字単位とテキストオブジェクト: 空のまま（範囲の先頭）。
   - `removed_exactly` `changed`（と `x` `r` の道）が `range.restore.value_or(range.range.begin)` を `VimStep.restore` に写す。yank は本文を変えないので写さない。
9. **行単位の `c` が 2 行以上**: `changed` は決定 8 の位置を 1 行下へ写す（桁は同じバイトの桁・その行の長さで寄せる）。1 行の `cc` は写さない。
10. **VISUAL**: 文字単位は選択の先頭（固定の端と動く端の小さい方）。行単位は先頭行の上で、動く端が先頭行ならその位置、固定の端が先頭行なら行頭。矩形は左上（先頭行の上の、矩形の左の桁の位置）。`d x c r` が `VimStep.restore` に写す。
11. **engine が書かないもの**: INSERT に入る命令と INSERT の入力・貼り付け `p` `P`・NORMAL の `r`。どれも `replace` の時点のキャレットが Vim の R と同じになる（`A` は先に行末へ動いている・`o` `O` `p` は元の位置のまま）。
12. **fixture と契約**: fixture の名前は `undo-caret-*`。変更が 1 つだけの鍵列（移動 → 変更 → `u`、同じ形の → `u<C-r>`、→ `uj`）に限る。probe の U1〜U6・U8・U9・U13 の各系統から、1 行の `dd` `cc` の桁の 3 か所（インデントの中・最初の非空白より右・行末）と Tab の行を含めて 60 件程度。変更が 2 つ以上ある形（`.` の後の `u`・`@a` の後の `u`・続けて戻す `u u u` と `<C-r>` × 3・最終行の `dd` のやり直し）は契約で、期待値は probe の U10〜U13。INSERT の中の矢印の区切りは本 ADR の範囲の外（oracle と打鍵で Vim の結果が違う）。oracle の報告が本文の行の長さを越える桁を返す形（矩形の一部）は fixture にせず、PR に鍵と実測を書く。
13. **#204 の fixture**: `register-dot-undo-advances`（`"1Pu.u.`）に加えて、`p` の形（`"1pu.u.`）を足す（Issue #208 の受け入れ条件）。

## 強制

- fixture（決定 12）: **active**（CTest・CNF-010 / 011）。
- 契約（変更が 2 つ以上の形）: **active**（既存の scope の翻訳単位に足す・`eng/protected-diff.py --allow`）。
- `Edit` の欄の書き忘れ: **active**（既定値なしの集成初期化・`-Wmissing-field-initializers` を含む厳格集合）。
- `restore` を書くのが `replace` の 1 か所・畳む所が最初の値を残すこと・Vim の `u` / Ctrl-r が `edit.at` を読まないこと: **planned**（レビュー事項・`grep -n "restore" src/core/EditHistory.cpp src/application/EditorController.cpp`）。

## 結果

得られるもの: `p` `P` `o` `O` `A` `I`・後ろ向きと行単位のオペレータ・VISUAL・`.`・マクロの後の `u` と Ctrl-r のキャレットが Vim と同じ位置になる。規則は「単位が覚えた戻り先へ行って寄せる」の 1 本で、`u` と Ctrl-r が同じ値を読む。
失うもの・残る穴: `Edit` が 1 欄（8 バイト）増える。キャレットの決め方は通常モード（戻した本文の末尾）と Vim（`restore`）の 2 つのまま残る（決定 6）。`cj` の 1 行下は Vim の実装の癖の写しで、`:help` に根拠が無い（fixture が守る）。変更が 2 つ以上ある鍵列は oracle で観測できず契約だけ。INSERT の中の矢印が undo を区切るかは扱わない。`VimMotionRange` の欄が 4 つになる（`numbered` と `restore`）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| controller が `edit.at` とキャレットから戻り先を推し量る（小さい方など） | 行単位の `P` と `O`（編集の先頭は行頭・Vim の戻り先は元の桁）、1 行の `dd`（最初の非空白との小さい方）、`dk` の着地の桁は、編集の位置からは出ない。オペレータの意味を知っているのは engine（ARC-004） |
| 効果の型（`VimRemoveRange` など）ごとに戻り先の欄を足す | 本文を変える効果は 9 つあり、9 か所に同じ欄が並ぶ。1 鍵 1 効果なので 1 打鍵の結果に 1 つ添えれば足りる |
| 戻り先を controller が履歴とは別の列で持つ | 履歴の位置と 2 つ同期させることになり、`absorbed` と再生の畳みのたびにずれうる（ARC-001） |
| `Edit.restore` に既定値を付ける（テストの 38 か所を触らない） | 書き忘れた編集が黙って本文の先頭へ戻る。欄を必須にすればコンパイルが教える |
| `Edit.restore` を `std::optional` にして、空なら今までどおり `edit.at` | 製品の編集はすべて `replace` を通るので空になる理由が無い。空の枝は試験のためだけの経路になる |
| 通常モードの Ctrl+Z も `restore` へ寄せる | Issue #208 の「やらないこと」。通常モードの使い手の期待（戻した本文の末尾）を変える別の判断 |
| `cj` の 1 行下を再現しない | fixture は本物の Vim が期待値を書く。再現しないなら `cj` の `u` を fixture から外すことになり、穴が機械から見えなくなる |
| oracle に undo の塊を区切る記法を足してから始める | 変更が 1 つだけの鍵列で規則の全部の枝を fixture にできる。記法は別の Issue の候補 |
