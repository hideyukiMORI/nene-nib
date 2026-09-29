# ADR 0050 — 数字レジスタ `"0`〜`"9` と小削除 `"-` は書き手 1 本の規則で埋め、`.` は番号を 1 つ進めて再生する

- 状態: 受理（設計席 2026-09-29・Issue #204。hide 未確認・引き継ぎ 09-23「7 回目の終了時点」の次の順の 1 番目）
- 日付: 2026-09-29
- Issue: #204
- 影響する規則: FR-003 / ARC-001 / ARC-004 / ARC-010 / CPP-002 / CPP-003 / CPP-011 / QLT-001 / QLT-012 / CNF-010 / CNF-011
- 前提: [ADR 0048](0048-named-registers-as-one-text-table.md)（名前つきレジスタと `"` の接頭辞・決定 12 が本 ADR の予告・決定 3 を本 ADR の決定 3 が改める）・[ADR 0046](0046-macros-record-keys-and-replay-through-dot-path.md)（マクロ）・[ADR 0030](0030-vim-dot-repeat-as-key-replay.md)（`.` は鍵の再生）・[ADR 0033](0033-vim-visual-dot-repeat-by-extent.md)（VISUAL の `.`）・[ADR 0035](0035-vim-visual-block-as-column-ranges.md)（矩形レジスタ）・[ADR 0032](0032-vim-search-as-input-line-and-one-key.md)（検索の移動）

## 文脈

`:help quote_number` `:help quote_-` `:help redo-register`。Vim は yank を `"0` に、削除と変更を `"1`（繰り下がって `"9` まで）か `"-` に置き、無名 `""` は最後に書いたレジスタを指す。`"1p` の後の `.` は `"2p` になる。

本物の Vim 9.1 の実測（Sonnet の probe 2 本・`out/probes/probe-numreg-2026-09-23.md` と `probe-numreg2-2026-09-29.md`・oracle と同じ設定・各問いは独立した起動）。Vim のソース（`op_delete` の「名指しの yank → `shift_delete_registers` → 小削除」と `start_redo` の番号送り）の読みと全部一致した:

- **yank**: `yy` `yw` と VISUAL / 矩形の `y` は `"0` と無名。`"ayy` は `"a` と無名で `"0` は不変。`"1yy` `"5yy` `"-yy` はそのレジスタを上書きするだけ（繰り下がらない・`"-` は行単位になる）。`y/pat<CR>` `y*` も `"0` だけ。
- **削除と変更（名前なし）**: 行単位か複数行にまたがる（`dd` `cc` `dj` `cj` `dG` `dgg`・複数行の `di(`・行をまたぐ `3dw`・行頭の `d<BS>` の `\n`・`vjd` `Vd` `Vc`・2 行の矩形）は `"1` へ入り `"1`〜`"8` が `"2`〜`"9` へ繰り下がる（`"9` は捨てる）。1 行の中（`x` `X` `3x` `dw` `cw` `D` `C` `d$` `d0` `dfg` `diw`・行末の `d<Space>`・`vld` `vlc`・1 行の矩形）は `"-` だけ。`"0` はどれも不変。
- **検索の移動の削除**: `d/pat<CR>` `d?pat<CR>` `dn` `dN` `d*` `d#` は 1 行の中でも `"1`（繰り下がりあり）と `"-` の両方。
- **名前を付けた削除**: 名指しの書き込みの後に上の `"1` の規則が続く 2 段。`"add` は `"a` と `"1`。`"ax` `"adw`（1 行の中）は `"a` だけで `"-` も `"1` も不変。`"ad/pat<CR>` は `"a` と `"1` で `"-` は不変。`"0dd` は `"0` と `"1`。`"-dd` は `"-` と `"1`。`"1dd` は `"1` へ書いてから繰り下げてもう一度 `"1` へ書く（`"1` と `"2` が今回の行・古い `"1` は消える）。`"3dd` は `"3` へ書いてから繰り下げる（今回の行が `"1` と `"4`）。`"0x` `"-x` はそのレジスタだけ。
- **`""` の明示**: `"0` を名指ししたのと同じ。`""yy` は `"0`。`""dd` は `"0` と `"1`。`""x` `""dw` は `"0` だけで `"-` は不変（`:help quote0` の「yank only」と食い違う。実測を正とする）。
- **追記**: `"ayy` の後の `"Add` は `"a` と無名が追記後の全体、`"1` は今回消した行だけ。`"Ax` は `"a` と無名だけ。
- **`"_`**: どのレジスタも変えない。
- **空の範囲**: 空行の `D` `x` は何も書かない。空行の `dw` `dd` は行を消して `"1` が `\n`（行単位）。
- **無名**: 直近に書いたレジスタと同じ値（名前なしの yank は `"0`・小削除は `"-`・削除は `"1`・名前つきは `"a`）。
- **種類**: `"0` `"1`〜`"9` `"-` はどれも文字・行・矩形を持てる（1 行の矩形の削除 `<C-v>ld` は `"-` に幅 2 の矩形）。`3"0p` `2"-P`・矩形の `"1p` `2"1p` は普通のレジスタと同じ貼り方。
- **`.`**: `"1p..` は `"1` `"2` `"3`。`"8p..` は `"8` `"9` `"9`。`"9p.` `"0p.` `"-p.` `"ap.` は同じレジスタ。`"1P..` も進む。`3"1p.` は `3"2p`（回数を引き継ぐ）。`"1pu.u.` は `u` を挟んでも `"2` `"3`。`"1dd.` の `.` は `"2dd`。
- **マクロ**: `@0`〜`@9` `@-` `@"` は本文を鍵として実行する。`q0`〜`q9` はそのレジスタへ録る。`q-` は録らない。`q"` は `"0` へ録る。
- **溢れ**: `dd` を 10 回で最初の行は捨てられる。その後の `yy` は `"1`〜`"9` を変えない。

Nib の今の形: `VimRegisterTarget { named, unnamed, black_hole }`。書き手は `registers_written(state, value)`（`VimStep.cpp`）の 1 本で、呼び出し元（`removed_exactly` `changed` `yanked` `block_operated`）は yank か削除かを渡さない。読みは `register_read`（`p` `P`）と `replayed_register`（`@`）の 2 本で、どちらも `vim_register_index` で a〜z の表を引く。`"` の次の数字は `register_selection_of` が名前にせず `refused`。`.` の記録 `normal_recording` は数字を `counts_as_digit` で落とすので、`"` の次の数字も記録に残らない。`VimState` は鍵を 1 つ食べるたびに `vim_resting_from` がレジスタを値で写す。

## 決定

**数字レジスタは 10 本の表、小削除は 1 本で、`VimState` が無名と名前つきの隣に持つ。書き手は `registers_written` の 1 本のままで、「名指しの書き込み → `"1` の規則 → `"-` の規則 → 無名」の順に埋める。`"1` へ行くかはレジスタの値そのものから決まり、範囲が運ぶのは「検索の移動は必ず `"1`」の印 1 つだけ。`.` は記録の先頭の `"{1〜8}` を 1 つ進めた鍵列を再生する。**

1. **置き場（core）**: `VimNumberedRegisters`（`src/core/VimNumberedRegisters.hpp`・1 型・`std::array<VimRegister, 10>`・位置 0 が `"0`）と `vim_numbered_index(char32_t) → std::optional<std::size_t>`（`0`〜`9` だけが位置を持つ）。`VimState` に `numbered`（`VimNumberedRegisters`）と `small_delete`（`VimRegister`）を足し、`vim_resting_from` が `registers` と同じく持ち越す。空は `VimRegister{"", uninitialized}`。`VimNamedRegisters` は a〜z の 26 本のまま変えない。`small_delete` の種類は文字単位に限らない（実測）。
2. **選択**: `VimRegisterTarget` に `numbered` と `small_delete` を足す。`register_selection_of` は `0`〜`9` を `numbered`（`name` はその数字）、`-` を `small_delete`（`name` は `-`）にする。`append` はどちらも常に false。`"` の待ちの次の数字は回数の桁ではなく名前で、`"12p` は `"1` を 2 回、`2"1p` も同じ。`VimRegisterSelection` の形は変えない。
3. **書きの規則（`registers_written` の 1 本・ADR 0048 の決定 3 を改める）**: 引数に操作（既存の `VimOperator`・`yank` か `remove` / `change`）と範囲の印（決定 4）を足す。既定引数は使わず、呼び出し元がすべて渡す。値が無い（空の文字単位の範囲）ときと `"_` のときはどのレジスタも変えない。それ以外は次の順。
   1. **名指しの書き込み**: 選択が `named` なら表へ（`append` なら ADR 0048 の決定 4 で繋ぐ・矩形が絡む追記は今までどおり `refused`）。`numbered` ならその位置へ、`small_delete` なら `small_delete` へ置き換える。`unnamed`（`""`）は `"0` へ置き換える。選択が無い yank も `"0` へ置き換える。選択が無い削除・変更はここでは何も書かない。
   2. **`"1` の規則**（削除と変更だけ）: 今回の値が「行単位か、本文に改行を含む」か、範囲の印が `always` なら、`"1`〜`"8` を `"2`〜`"9` へ繰り下げ（`"9` は捨てる）、`"1` へ今回の値（追記前の、今回消した分だけ）を置く。1 の名指しが数字でも順は同じ（`"1dd` は `"1` と `"2` が今回の値になる）。
   3. **`"-` の規則**（削除と変更だけ）: 選択が無く、今回の値が「行単位でなく、本文に改行を含まない」なら `small_delete` へ今回の値を置く（検索の移動の 1 行の中の削除は 2 と 3 の両方に入る）。
   4. **無名**: `append` なら追記後の全体、ほかは今回の値を写す（値の写し・ADR 0048 と同じ）。
4. **範囲の印**: `VimNumberedRule { by_extent, always }`（`src/core/VimNumberedRule.hpp`・閉じた enum）を `VimMotionRange` の 3 つ目の欄 `numbered` に持つ（既定のメンバ初期化子は `by_extent`・今の構築の式は変えない）。`always` にするのはオペレータの後ろの検索の移動（`/ ? n N * #`）の範囲を作る所だけ。`% ( ) { }` と `` ` `` が入るときは同じ印を立てる（`:help quote_number` の一覧）。VISUAL と矩形の削除は `by_extent`。「行単位か改行を含むか」は呼び出し元が数えず、`registers_written` がレジスタの値（`kind` と `text`）から 1 か所で決める（複数行の矩形の本文は行の区切りの改行を含む）。
5. **読み（1 本）**: `register_read` を「選択 → レジスタ」の 1 本の関数にし、`p` `P`（矩形を含む）と `@` の読み元（今の `replayed_register`）が同じ関数を通る。`numbered` は表の 1 本、`small_delete` は `small_delete`。ほかは ADR 0048 の決定 5 のまま。
6. **マクロ**: `@{0-9}` `@-` は決定 5 の読みで本文を鍵にして実行する（`@@` の `last_macro` は数字と `-` も覚える）。`q{0-9}` はそのレジスタへ録る（追記なし・`characters`・無名は変えない）。`q-` は今の「名前にならない鍵」と同じ `refused`。`q"` は後続。録画を止めるときの書き込みは決定 3 を通らない（繰り下がらない・ADR 0048 の決定 6 のまま）。
7. **`vim_register_stored`**: `0`〜`9` と `-` を置き換えで受ける（`:let @0=` に当たる口・契約の前置きが使う）。ほかの名前は今までどおり状態を変えない。
8. **`.` の番号送り**: core の 1 関数が「記録の鍵列の先頭に並ぶ `"{名前}` の組のうち最後の組」を見て、名前が `1`〜`8` なら 1 つ進めた鍵列を返す（`9` `0` `-` と名前つきはそのまま・INSERT で打った本文の中の `"1` は先頭の組ではないので触らない）。`repeated_change`（NORMAL）と `replayed_visual`（VISUAL）が再生の鍵を作るときにこの関数を通す。再生した鍵は今までどおり記録し直すので、次の `.` はさらに進み、`u` を挟んでも進む。回数は ADR 0030 のまま引き継ぐ。put に限らずどの命令でも同じ（`"1dd.` は `"2dd`）。
9. **`.` の記録**: `normal_recording` は「`"` の待ちの次の鍵」を数字でも記録に残す（今は `counts_as_digit` が落とす）。VISUAL の記録（`visual_recorded`）は今の形で残る。
10. **fixture と契約**: 名前は `register-numbered-*` `register-small-*` `register-dot-*`（`register-*` と同じ `--vim-macro` の scope が再生する・表が 1 つ・ADR 0048 の決定 11）。oracle の測定コードと `register` 欄は変えない。数字と `-` の中身は `"1p` `"0p` `"-p` で貼り戻して本文で見る。45 件程度: 文脈の各項目から少なくとも 1 件ずつ（yank と `"0`・`"ayy` は `"0` 不変・`"1yy` `"-yy`／`dd` × 3 の繰り下がり・10 回の溢れ・1 行の中の 8 種と `"-`・複数行の 8 種と `"1`／検索の 6 種の両方／名前を付けた削除 9 種／`""` の 4 種／`"Add` と `"1`・`"Ax`／`"_`／空行の `D` `x` `dw` `dd`／無名の 4 種／VISUAL と矩形の 10 種／貼り方の 5 種／`.` の 9 種／`@0` `@1` `@-` `@2`）。録画（`q0`〜`q9`・`q-` の拒否）と `vim_register_stored` の数字と `-` は契約 `--vim-macro`（期待値は probe 2 の Q10 の実測）。`u` を含む fixture は oracle が undo の塊を区切れる形で書けるものだけ fixture にし、書けなければ契約にして PR に理由を書く。
11. **後続（別 Issue）**: クリップボード `"+` `"*`（ADR 0051・`VimRegisterTarget::clipboard` と controller の `ClipboardPort` の橋渡し）・読み取り専用の `". ": "/ "%`・`"=`・`q"`・`:reg`・INSERT の Ctrl-R・レジスタの本文の共有（「結果」の速さ）。

## 強制

- fixture（決定 10）: **active**（CTest・CNF-010 / 011）。
- 契約 `--vim-macro`（録画と `vim_register_stored`）: **active**（`eng/protected-diff.py --allow --vim-macro`）。
- 閉じた enum の写し漏れ（`VimRegisterTarget` の 2 値・`VimNumberedRule`）: **active**（`switch` の網羅性・CPP-002。`registers_written` `register_read` `register_selection_of` が落ちて直す場所を教える）。
- 書き手が `registers_written` の 1 本・読みが `register_read` の 1 本であること: **planned**（レビュー事項・`grep -n "\.numbered\|small_delete" src/core/VimStep.cpp` がこの 2 本と `vim_resting_from` と `vim_register_stored` と録画の停止だけ）。

## 結果

得られるもの: `"0p`（消した後でも yank を貼る）・`"1`〜`"9` の繰り下がりと `"1pu.u.`・`"-`・`@0`・`q0`。`""` の明示と名前を付けた削除が Vim と同じレジスタを書く。`p` `P` と `@` の読みが 1 本になる。
失うもの・残る穴: `VimState` が鍵を 1 つ食べるたびに写すレジスタが 11 本増える。巨大な削除（16 MiB の `dG` など）を重ねると、その本文が最大 9 本ぶん残って 1 打鍵ごとに写される（無名と名前つきにも今ある性質で、本 ADR で最大 9 倍になる）。merge の前に設計席が Release で速さの 6 本を測り、巨大な削除の後の打鍵は別 Issue でベンチを足して本文の共有を検討する。空行の `dw` の行単位化など、範囲の規則が Vim と違う所は fixture が見つける（見つけたら本 Issue では直さず別 Issue）。`q"`・クリップボード・読み取り専用のレジスタは無い。録画は oracle で観測できず契約だけ（ADR 0046 と同じ）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| a〜z の表を 37 本に広げて数字と `-` を位置の算術で引く | 「名前つき」の型が数字を持つ嘘になり、追記できる・繰り下がる・小削除だけ、の違いが位置の比較に隠れる。閉じた enum の `switch` なら足し忘れをコンパイルが落とす（CPP-002） |
| 「`"1` へ行くか」を呼び出し元が数えて渡す | レジスタの値から決まることを 5 か所の呼び出し元に写すと食い違う（ARC-001）。呼び出し元しか知らないのは検索の移動かどうかだけ |
| `""` を今までどおり「接頭辞なし」と同じに扱う | 実測と違う（`""x` は `"-` に書かず `"0` に書く・`""dd` は `"0` にも書く） |
| `.` の番号送りを put の特例にして「次の番号」を状態に持つ | Vim は記録の書き換えで、`"1dd.` も進む。記録を書き換えれば `u` を挟む場合も回数の引き継ぎも今の再生の経路のまま出る |
| 無名を「どのレジスタを指すか」で持つ | 値の写しで観測は同じ（probe 2 の Q11）。指す先を持つと `vim_resting_from` と追記の全体の扱いが 2 通りになる |
| クリップボード `"+` `"*` を同じ ADR に入れる | core だけで閉じる本 ADR と違い、application の `ClipboardPort` との往復の設計が要る。1 task = 1 PR に収まらない |
