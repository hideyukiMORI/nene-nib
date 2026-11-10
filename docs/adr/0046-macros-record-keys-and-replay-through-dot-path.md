# ADR 0046 — マクロ `q` `@` は名前つきの鍵列を engine が録り、再生は `.` と同じ経路で 1 鍵ずつ流す

- 状態: 受理（設計席 2026-09-23・Issue #176。hide 未確認・引き継ぎの「次の順」の 2 番目。決定 2・3・4・6 は #176 の実装の実測で設計席が補正）
- 日付: 2026-09-23
- Issue: #176
- 影響する規則: FR-003 / ARC-001 / ARC-010 / ARC-012 / CPP-002 / CPP-005 / CPP-011 / QLT-001 / QLT-012 / CNF-010 / CNF-011
- 前提: [ADR 0012](0012-vim-engine-first-slice-and-oracle-fixtures.md)（engine は 1 鍵 1 効果・fixture は oracle）・[ADR 0030](0030-vim-dot-repeat-as-key-replay.md)（`.` は鍵の列を engine が記録し controller が同じ `accept` へ再生・undo は 1 単位）・[ADR 0033](0033-vim-visual-dot-repeat-by-extent.md)・[ADR 0035](0035-vim-visual-block-as-column-ranges.md)（幅つきレジスタ）

## 文脈

`:help q`: `q{0-9a-zA-Z"}` は打鍵をそのままレジスタへ記録し（大文字は追記）、`q` で止める。`@{register}` はその内容を `[count]` 回「マッピングとして」実行し、`@@` は直前の `@` を繰り返す。**録画は `:normal` とマッピングの中では動かない**ので、oracle（`-es` ＋ `:normal!`）では録画を観測できず、`qaxq@a` を流すと黙って間違った期待値になる（Sonnet の probe・scratchpad `probe-macro.md`）。再生は `let @a = "…"` で先に置けば oracle で正しく動く。

本物の Vim の実測（`feedkeys`・probe 2 節）: 失敗（ビープ）で残りは実行されない／`@a` 全体は 1 つの undo 単位、録画中に直接打った変更は 1 打鍵ずつ／`.` は `@a` 全体ではなく直前の 1 変更だけ／大文字は録画をまたいで追記／`"ap` は鍵列の文字列がそのまま貼れる。

Nib の今の形: `.` の記録 `VimRepeatRecord { count; keys: vector<VimKey>; visual }` と再生 `VimReplay { keys; reselect }`。controller の `perform(VimReplay)` は history を `sealed()` で閉じてから 1 鍵ずつ `accept` に流し、また閉じる（ADR 0030）。`q` `@` `"` はどこにも束縛されていない。無名レジスタ `VimRegister { text; kind; width }` は本文を持つ「結果」の型で、鍵列とは別。

## 決定

**マクロは engine が `q{a-z}` から `q` までの鍵を名前つきの鍵列として録り、`@{a-z}` `@@` `[count]@a` は `.` と同じ `VimReplay` の効果で controller が 1 鍵ずつ流す。oracle は録画を観測できないので、fixture は「レジスタを先に置いて再生だけ」を本物の Vim で守り、録画は契約テストで守る。**

1. **型（core）**: `VimMacroRegisters`（`src/core/VimMacroRegisters.hpp`・1 型）は a〜z の 26 本の `std::vector<VimKey>` の表。`VimMacroRecording { char name; std::vector<VimKey> keys; bool append; }`（`src/core/VimMacroRecording.hpp`・1 型）は録画中の状態。`VimState` に `macros`（`VimMacroRegisters`）・`macro_recording`（`std::optional<VimMacroRecording>`）・`last_macro`（`std::optional<char>`・`@@` 用）を足す。`.` の `recording` / `last_change` とは独立（Vim と同じ・probe の `qallxq@a.`）。
2. **録画**: NORMAL / VISUAL の `q` は次の 1 鍵を待つ（`r` `f` と同じ待ちの経路）。`a`〜`z` で新しい録画（`append = false`）、`A`〜`Z` で追記（`append = true`）、それ以外は何もしない（ビープ相当の閉じた失敗・入力を捨てる）。録画中は**打った鍵**（移動・INSERT の文字・`<Esc>`・検索の確定の鍵 `VimSearchPattern`・`.`・`@a` は展開せず `@a` のまま）を `macro_recording.keys` に積み（再生で流れた鍵は積まない。鍵が打たれたものか再生かは `VimEditorView.source`（`VimKeySource`・閉じた enum）で engine に伝える）、止める `q`（録画中の NORMAL / VISUAL の `q`）だけは積まずに録画を閉じて `macros[name]` に置く（追記なら末尾に繋ぐ）。録画は実行の経路を変えない（undo は今までどおり 1 打鍵ずつ）。再生の中（`VimReplay` で流れている鍵）では `q` は無効（何もしない・Vim の「レジスタ実行中は無効」）。Ex（`:`）は application の入力行なので本 ADR では録らない（`:s` 等をマクロに入れるのは後続）。
3. **再生**: `@` は次の 1 鍵を待つ。`a`〜`z` は `macros[name]` の鍵列を `count` 回繋いだ `VimReplay { keys }` を効果として返し `last_macro = name`。`@@` は `last_macro` で同じ（無ければ何もしない・E748 相当の閉じた失敗）。空のレジスタは何もしない。controller の `perform(VimReplay)` は 1 鍵ずつ `accept` に流し、終わりに再生の前後の本文の差分を覆う 1 つの `Edit` に置き換えて履歴に積む（`apply_block` と同じ考え方。履歴の 1 単位は連続した `Edit` 1 つなので、`sealed()` で挟むだけでは複数の編集がまとまらない・#176 の実測）。`.` の再生も同じ関数を通る。`[count]@a` 全体で undo 1 単位。**失敗で打ち切り**: 流している途中の鍵が閉じた失敗（`VimRepeatFailure` と同じ種類・移動できない・見つからない）を返したら、残りの鍵を捨てる（`.` の再生と同じ規則。`.` に無ければ両方に足す）。
4. **再帰の上限**: 再生の中で `@a` が `VimReplay` を返したら、controller は再帰せずに残りの鍵の列の**頭へ差し込む**（Vim の先読みと同じ順。再帰で書くと深さ 100 で ASan の Debug がスタックを溢れた・#176 の実測）。入れ子の深さは `constexpr` 上限 100（Vim の `maxmapdepth` 1000 は再帰マクロの終端を失敗で作る慣用に足りる値として 100）。超えたら残りを捨てる（閉じた失敗・報せは E132 相当の Nib の文言 1 行。Vim 9.1 は `let @a="0@a"` の `@a` が止まらない）。
5. **`.` との関係**: 再生された鍵は普通に `accept` を通るので、`last_change` は再生の中の最後の変更になり、`@a` の後の `.` はその 1 変更だけを繰り返す（Vim と同じ）。録画中の `.` は鍵 `.` として録られ、再生時に `.` として動く。
6. **fixture（oracle）**: `tests/vim/fixtures.json` に任意の欄 `"register"`（`{"name": "a", "keys": "llx"}`・1 件に 1 本・鍵の記法は `keys` と同じ）を足す。`eng/vim-oracle.py` は `probe_script` の `:normal!` の前に `let @a = "<vim_keys(keys)>"` を書く。`tests/vim/VimFixture.hpp` に `std::optional<VimMacroFixture>`（`VimMacroFixture { char name; std::string_view keys; }`・1 型・`tests/vim/VimMacroFixture.hpp`）。テストの再生は fixture の `register` を application の意図 `StoreVimMacro`（controller に状態を置く口として足す。後続の `:let @a=` も同じ口）で `VimState::macros` に置いてから `keys` を流す。fixture は `@a` `2@a` `@@` `@a` の失敗の打ち切り・INSERT を含む再生・複数行・`?` 検索を含む再生など 15 件程度。**`q` を含む鍵列は fixture に書かない**（oracle が観測できない。`eng/vim-oracle.py` は `keys` に `q` を含む fixture を拒む: 終了 1・CNF-011 と同じ「黙って通さない」）。
7. **契約（unit・scope `--vim-macro`）**: 録画→再生の一気通貫（probe 2 節の表をそのまま期待値に: `qaxq@a`・`qa0xjq2@a`・`qaiab<Esc>q@a`・`qallq` → `qAxq`・`qaxq@a@@`・`qallxq@a`・`qallxq@a.`・`qaxxq@au`・`qaxxqu`）・再生中の `q` 無効・再帰の上限・空のレジスタ・`@@` が無いとき。
8. **後続（別 Issue）**: ステータスバーの「recording @a」の表示（renderer・application の表示値に 1 欄）。`"a` の接頭辞（`"ayy` `"ap`・名前つきのテキストレジスタ・`VimRegister` の 26 本表）と、そのときのマクロレジスタとの統合（Vim はレジスタが 1 つの表で `"ap` が鍵列を文字として貼る。統合の ADR で鍵列 ↔ 文字列の写しを 1 本にする → [ADR 0048](0048-named-registers-as-one-text-table.md)）。`@:`・`q"`・数字レジスタ。

## 強制

- fixture（`register` 欄・再生だけ）: **active**（CTest・CNF-010 / 011 で生成物と json の一致）。
- 契約 `--vim-macro`: **active**（`eng/protected-diff.py --allow --vim-macro`）。
- `keys` に `q` を含む fixture の拒否: **active**（`eng/vim-oracle.py` の `records_a_macro` が鍵を 1 つずつ歩く状態機械で、`q` が NORMAL / VISUAL のコマンドの位置に届くときだけ拒む・迷う位置は拒む側・挿入文字・文字引数・レジスタ名・検索の入力の `q` は通す・conformance の反例 `qaxq@a` `"aqa` `viwqa` `xqb` `dq` `gq` と通る例 `fqx` `rq` `iq<Esc>` `cwq<Esc>x` `@q` `"qyy` `diq` `vrq` `vcq<Esc>`・#190）。
- 再生の経路が `.` と同じ 1 本（`perform(VimReplay)`）: **planned**（レビュー事項）。

## 結果

得られるもの: `qa…q` `@a` `@@` `3@a`、再帰マクロ、undo 1 単位、失敗で打ち切り。engine の 1 鍵 1 効果は不変（`@a` も `.` と同じく「鍵列を返す 1 効果」）。
失うもの・残る穴: 録画は oracle で観測できず契約だけ。失敗の印（`VimStep.failure`）は主な経路だけで、スクロールと VISUAL の未対応の鍵は再生を打ち切らない（後続）。oracle の `q` の拒否は NORMAL / VISUAL のコマンドの位置の `q` だけに狭めた（#190・`fq` や挿入文字の `q` は書ける）。字句の判定なので、失敗する `c` の動作（`cfz` で z が無い）の後の `q` は INSERT の文字と見て通す（本文に依る失敗は見ない）。検索の失敗（E486）は Vim が `-es` でエラー終了するので fixture にできず契約で守る。Ex を含むマクロ・`"ap`・recording の表示（#180）・数字レジスタは後続。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| engine の中で `@a` の鍵列を回す | controller の undo の単位（`sealed()`）と `.` の再生経路が 2 本になる（ARC-001）。`.` と同じ `VimReplay` で足りる |
| マクロを `VimRegister`（本文の型）に文字列で入れる | 鍵列 → 文字列 → 鍵列の写しが今は無く、`"ap` も無い。統合は名前つきテキストレジスタの ADR で |
| `q` を含む鍵列を oracle で fixture にする | `:normal!` の中で録画は動かず、黙って間違った期待値になる（probe 3 節） |
| 再帰の上限を Vim と同じ 1000 に | 1 鍵ずつ `accept` に流す入れ子が 1000 段は深すぎる。100 で再帰マクロの慣用（終端は失敗）に足りる |

## 追補（2026-11-10・#1312）

決定 2 の「鍵が打たれたものか再生かは `VimEditorView.source`（`VimKeySource`）で engine に伝える」は、値が `typed` `mapped` `replayed` の 3 つになった（`mapped` は `map` の表に当たった右辺の鍵・`q` が効き、`.` の記録には載り、マクロには積まない）。録画に積むのは `typed` だけで、表に当たった単位は controller が打った左辺を同じ書き手 `vim_macro_typed` で積む（Vim の「録るのは打った鍵だけ」と同じ）。決定 3 の「`@` は `.` と同じ `VimReplay` の効果で controller が 1 鍵ずつ流す」は、`VimReplay.kind` が `macro` の鍵を流す時点の `map` の表で引く（`.` は引かない）。詳しくは [ADR 0176](0176-vim-map-is-a-settings-row-applied-by-one-key-table-at-the-window-entry.md) の追補 32〜35。決定 4 の上限の実測をもう 1 つ: Vim 9.1 は `nmap x @a` と `a=x` の `@a` も E223 を出さずに止まらず回る（60 秒で打ち切り・PR #1328 のレビューの P9）。Nib は表の右辺を通っても深さ 101 で E132 を出して止まる。

## 追補（2026-11-10・#1341）巨大な回数の `@` は 1 周ずつ積み、回数で本文を増やす編集は文書の上限で断る

Vim 9.1 の実測（`-es -u NONE`・`feedkeys(…, 'tx')`・1 件 20 秒で打ち切り・`D:\NeNeNib\outputs\20261110\1341\cases1.txt`）: `nv_at` は回数ぶん `do_execreg` を呼び、毎回レジスタを typebuf の頭へ積んでから流す。積む費用が回数の 2 乗に近く、`qajq100000@a` 83 ms・`qaxq100000@a` 0.78 s・`qaxq1000000@a` 9 s で、`999999999@a` は中身が `j`（最終行で失敗する）でも流す段に来ないまま 20 秒で終わらない。`x` は空の行でも失敗しないので、`x` だけのマクロは回数ぶん回る（`qaix<Esc>q10000@a` 0.76 s・`100000@a` は終わらない）。

決定 3 の「`count` 回繋いだ `VimReplay { keys }`」を次に改める。engine は 1 周ぶんの鍵と周の数 `VimReplay.rounds`（`.` は 1）を返し、controller は 1 周ぶんだけを列に置く。周の最後の鍵には `ReplayLevel.round_end` の印を立て、それを列から取り出したとき、鍵を流す前に次の周を列の頭へ積む（流した鍵が差し込む入れ子の再生はさらにその前に入るので、順は回数ぶん繋いだ列と同じ）。まだ積んでいない周は `ReplayRounds` の積み重ね（頂がいま流している最も深い周）で持ち、鍵の失敗と深さの上限（決定 4）で列と一緒に捨てる。確保は 1 周ぶんで回数に比例せず、失敗する中身は失敗の周で止まる（`999999999@a` の `j` は 3 周を流して返り、積んだ周は 4・最後の周の最後の鍵を流す前に次の周を積むため）。Vim が終わる回数では結果は Vim と同じ（fixture `huge-count-macro-` の 6 件）。積んだ周の数は `EditorController::replay_rounds()` が数え、契約 `--vim-macro` は時計を読まずに周の数で断言する。

回数で本文を増やす編集（`p` `P` の文字・行・矩形、INSERT の回数の反復、`o` の回数、矩形の `I` `A` `c` `C` が Esc で残りの行へ写す本文）は、編集の後の文書が開ける上限 `maximum_document_bytes`（1 GiB・ADR 0159 の決定 7）を超えるなら、確保の前に `VimRepeatFailure::too_large` で断る（本文は変えず、再生の残りも捨てる。INSERT は 1 周目の本文を残す）。効果の本文は LF で数え controller が判定の後で文書の改行へ写すので、判定は CRLF の文書では改行を 2 バイトに数える（`written_bytes`・矩形の行の改行も同じ）。矩形の判定は 1 行ぶんの上界に行の数を掛ける: 矩形の `p` は 1 行に回数ぶんの本文と幅までの空白に加えて多くとも max(桁, tabstop) と改行の幅（行末より右の桁の埋め・足す行・桁の途中で割る Tab）、矩形の `I` `A` `c` `C` は写す本文（回数ぶん）に多くとも max(桁, tabstop)（`block_copy_fits`）。写すのが上限を超えたら 1 周目の本文だけを写し直し、それも超えれば 1 行目だけを残す（PR #1348 の再レビュー M1 / M2）。VISUAL の回数つきの `>` と矩形の `>` も同じ上限で断る（1 行に足すのは多くとも回数 + 9 バイト = shiftwidth × 回数 / tabstop + tabstop + 1・`shift_fits`）。上限は `VimEditorView.document_limit`（既定は `maximum_document_bytes`・製品は置き換えない）で、契約 `--vim-macro` は数百バイトの上限を置いて「LF なら通り CRLF なら断る」境を確かめる（1 GiB は確保しない）。断った鍵は `EditorController::stepped_vim` が `E1240: Resulting text too long`（Vim 9.1 の同じ種類の文）を 1 行出し、鳴らさない（ADR 0153 の決定 2）。再生の中で断ったときも、断った鍵の 1 回だけ出る。Vim は確保できるかぎり実行する（`yl999999999p` は 2.7 秒で 1 GB の行、`<C-v>jy999999999p` は 2 GB の 2 行、`yy999999999p` と `999999999ix<Esc>` は 20 秒で終わらない）。上限の内の回数は Vim と同じ結果のまま（fixture `huge-count-` の put と INSERT の 6 件）。

残る穴（判断待ち・#1341）: 失敗しない中身（`x` `0` `ix<Esc>`）の巨大な回数の `@` と、INSERT の回数の反復のうち打った本文だけでない列（`<Left>` や `<C-r>` を含む）は、Vim と同じく回数ぶん回るので 1 回の `apply` が終わらない（Vim は Ctrl-C で止められるが Nib に割り込みは無い）。周の数の上限で止めるか、周を意図をまたいで流して割り込みを受けるかは設計席の判断。
