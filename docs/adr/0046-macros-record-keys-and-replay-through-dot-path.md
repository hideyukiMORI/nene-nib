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

## 追補（2026-11-11・#1369）`q` `@` の後の Esc は取り消し

決定 2・3 の「名前でない鍵はビープ」から Esc を除く。`q<Esc>` `@<Esc>` は失敗にせず、回数と待ちを消すだけで後ろの鍵が効く（Vim 9.1 の `normal_cmd` は 2 鍵目が ESC なら `nv_record` `nv_at` を呼ばずに `clearop` で終わる・実測）。判定は `"` と同じ `escaped_or_refused` の 1 本。録画中の `q`（待たずに止める）は変えない。規則・fixture・契約は [ADR 0048](0048-named-registers-as-one-text-table.md) の追補 17〜19。

残る穴（判断待ち・#1341）: 失敗しない中身（`x` `0` `ix<Esc>`）の巨大な回数の `@` と、INSERT の回数の反復のうち打った本文だけでない列（`<Left>` や `<C-r>` を含む）は、Vim と同じく回数ぶん回るので 1 回の `apply` が終わらない（Vim は Ctrl-C で止められるが Nib に割り込みは無い）。周の数の上限で止めるか、周を意図をまたいで流して割り込みを受けるかは設計席の判断。→ `@` の分は次の追補（#1351）で上限に決めた。INSERT の回数の反復の分は残る。

## 追補（2026-11-11・#1351・施主決定 D75）いちばん外の再生が流す鍵の数に上限を置く

施主決定 D75（「エスケープじゃなくて上限でいいよ」・Issue #1351 の候補 A）: いちばん外の再生（`.` `@` を打った 1 鍵が返した `VimReplay`）が列から流す鍵の数に上限 `core::vim_replay_key_limit`（100,000）を置き、上限まで流した後に流す鍵が残っていれば、列・まだ積んでいない周・`map` の待ちを `dropped_replay` で捨てて「再生を 100,000 鍵で止めました（残りは捨てました）」を 1 行出す。失敗にはしない（鳴らさない・打った後ろの鍵は普通に流れる）。Esc / Ctrl-C の割り込み（候補 B）は作らない。数えるのは列の頭を取り出す 2 本のループ（`replayed_plain_queue` と `replayed_mapped_queue`）の 1 か所 `replay_step_allowed` で、入れ子の再生の鍵とまだ積んでいない周の鍵も、いちばん外の 1 つの数に入る（周の長さが違っても効くので、周の数でなく鍵の数で数える）。表に行があるときの歩は「列から取り出す 1 回か待ちを流す 1 回」で、待ちから列へ戻した鍵は取り出すたびに数える。

値の材料は Release の `nib_vim_replay`（窓を出さない・1 件 1 プロセス・main `25c20131` の再生器・道具 `D:\NeNeNib\scripts\20261111\1351\measure_replay.py`）。設計席が席を全部止めた静かな機械（負荷 2%）で 1 回流した値（`D:\NeNeNib\outputs\20261111\1351\release-base-quiet.tsv`・起動 0.068 s）:

| 中身 | 10^3 鍵 | 10^4 鍵 | 10^5 鍵 |
|---|---|---|---|
| `x`（`abc`） | 0.021 s | 0.150 s | 1.413 s（1 鍵 約 14 µs） |
| `j`（周 + 2 行・本文を作る費用を含む） | 0.020 s | 0.156 s | 1.536 s（約 15 µs） |
| `ix<Esc>`（`abc`・1 周 3 鍵・行が伸びる） | 0.035 s | 1.793 s | 走らせない（行の長さで 2 乗・読みで約 172 s） |

同じ日の朝に実装席が測った表（`release-base.tsv`・`x` 10^5 鍵 4.15 s・`ix<Esc>` 10^4 鍵 3.28 s・`j` 10^5 鍵 5.55 s ほか）は**無効**（測る前の負荷が 3 回とも 100%・60 秒待っても 100%・ほかの席のビルドで起動の費用が 0.3〜1.7 秒に揺れた）。記録として残し、値の根拠にしない。実装席はこの表から 10^4 を選んだが、設計席が 10^5 に決め直した。

値を 100,000 にした理由（設計席）: 10,000 鍵では正当なマクロ（30 鍵のマクロを 500 回 = 15,000 鍵）を途中で切り、切った害（途中まで処理した本文）が数秒の待ちより大きい。100,000 鍵は `x` や `j` の形で約 1.5 秒で返る。`ix<Esc>` の形が 2 乗になるのは上限と別の根（長い行で 1 鍵の費用が行の長さに比例する疑い・#1451）で、上限では扱わない。上限が効くのは 100,000 鍵を超える再生だけで、Vim が終わる回数（fixture `huge-count-macro-` の 6 件はどれも鍵の失敗で上限より前に止まる）の結果は変えない。

Vim と違う最小の形は契約 `--vim-macro` の `verify_macro_key_limit`（本文 `ab`・a = `0`・c = `3@ax` で `@c` の `x` は 7 鍵目。上限 7 なら流れて `b`、上限 6 なら `x` を捨てて `ab` と知らせ。Vim は `b`）。Debug で 100,000 鍵を流すと 1 件約 80 秒かかる（負荷 70% の機械で 3 件 240 秒）ので、契約は試験の口 `EditorController::limit_replay_keys` で小さい上限を置き（製品は呼ばない・既定は `core::vim_replay_key_limit`）、製品の値は `static_assert`（知らせの文の数と上限・契約の 100,000）で守る。fixture にしないのは Vim と違う結果だから。

限界（PR #1407 のレビュー）: 表に行があるときの 1 歩は右辺 1 つぶんを流すので、実際に engine へ流れる鍵の数は上限の右辺の長さ倍まで伸びる。知らせの「100,000 鍵」はその場合に厳密でない（文は変えない・止まることは同じ）。上限で止めた所が INSERT の途中なら、鍵の失敗や E132 と同じく INSERT のまま残り、後で打った文字は本文に入る（今の扱いのまま）。打った 1 鍵の `map` の右辺に `@a` を並べると、それぞれがいちばん外の再生になって数が 0 に戻る（後続）。

## 追補（2026-11-12・#1517・ADR 0182）INSERT の `Ctrl-o` と録画・再生

打った `Ctrl-o` は録画に積む（vim_macro_typed の 1 本のまま）。`<C-o>q` で止めた録画は末尾の `Ctrl-o` を外す（Vim の get_recorded）。`<C-o>@a` は Vim の do_execreg と同じに始め直す印を消し、再生の全部の周の後に `i` を 1 つ流す（`VimReplay.insert_after`・失敗で列を捨てれば `i` も捨てる）。マクロの中の `Ctrl-o` の後は命令 1 つで INSERT を始め直し（マクロの鍵は typeahead）、`.` の再生の鍵が列に残る間は始め直さない（Vim の stuff_empty()）。再生の中の undo は今までどおり全体で 1 単位。詳細は ADR 0182 の決定 5・7。

## 追補（2026-11-12・#1528）`:` の行も打った鍵として録る

決定 2 の「Ex（`:`）は application の入力行なので本 ADR では録らない」を改める。Vim 9.1 は `qa:set so=2<CR>jq` の a を `:set so=2^Mj` にし、`@a` は `:set so=2` も再生する（`-es -u NONE` の `feedkeys(…, 'xt')` で実測・道具 `D:/NeNeNib/scripts/20261111/c10/p1.vim` `p5.vim`）。Nib は次の形で揃える。

1. engine の `macro_recorded` は `:`（効果 `VimOpenCommandLine`・NORMAL の `N:` と VISUAL の `:` も同じ効果・#1526）を積む。範囲 `'<,'>` `.,.+N` は engine が置くので録らない（Vim も録るのは `:` だけ）。`/` `?`（`VimOpenSearch`）は今までどおり積まず、確定した検索は `VimSearchPattern` の 1 鍵（本文は `/pat\r`）で積む。
2. 入力行の中の鍵は controller が録画の書き手 `vim_macro_typed` の 1 本で積む（`record_command_keys`）。積むのは開いている入力行が Ex の行（`CommandLine`）で、録画中・再生の外・鍵の出どころが `typed`（`map` の右辺でない）のときだけ。窓の意図と fixture の harness の鍵の口は同じ入口を通るので、積む所は入口の 4 つ: `CommandText`（1 コードポイント 1 鍵・IME の確定と Ctrl+V の貼り付けも同じ口なので文字として積む）・`EditCommand`（下の表）・確定 `confirmed_command`（`<CR>`）・取消 `accept(CancelCommand)`（`<Esc>`）。文書の切り替え・空の行の Backspace・F1 の一覧（`OpenOperationList`）と保存の形の一覧（`OpenDocumentFormatList`）が開いている行を閉じる取消は `cancel_command_input` を直接呼び、`<Esc>` を積まない（打った Esc ではない・レビュー V-1552 の L2）。こうして確定も Esc も無く捨てた `:` の行は、`:` から後の録画を戻す（#1569 の L1・ADR 0022 の #1569 の追補 3・契約 `verify_macro_list_closes_ex_line_unrecorded`）。
3. 編集の写し（Vim の実測）: ← → Home End BS Del は `<Left>` `<Right>` `<Home>` `<End>` `<BS>` `<Del>`、履歴の上下は `<Up>` `<Down>`、補完の Tab は文字の Tab（Vim の再生では 'wildchar' が効かず Tab が入る・`:se<Tab>so=6` は `:set so=6` と同じに効く）。Shift+Tab（Vim は `<S-Tab>`）は Nib の鍵の表に無いので積まない。面（Ctrl+P）の中の鍵は Ex の行ではないので積まない。
4. 再生と fixture の鍵の `<Up>` `<Down>` は、入力行で窓と同じ履歴の前・次に写す（Vim の再生の `<Up>` も履歴を引く・実測: 打った `:set so=4<CR>:set so=1<CR>` の後の `:<Up><Up><CR>` の再生で so=4）。今までは捨てていた。

守り: VISUAL の反例（レビュー V-1547）`qavj:set so=2<CR>xq` の a は `vj:set so=2^Mx` で、`@a` は E481 で打ち切られて `x` を流さない（実測 `p6.vim`・契約 `verify_macro_records_visual_ex_line`）。fixture `macro-ex-`（7 件・register 欄の `@a` の再生・録画の harness `store_vim_fixture_macro` が入力行の鍵を録れるようになったので `:set so=2<CR>j` を fixture にできる）と契約 `--vim-macro` の `verify_macro_records_ex_line` / `verify_macro_replays_ex_line`。

5. 履歴に足す（レビュー V-1552 の M1 で「限界」を直した）: Vim の getcmdline は `:` の行を、行の鍵が 1 つでも打たれたとき（`some_key_typed`・`KeyTyped` の鍵）だけ履歴に足し、検索の行は打っていなくても足す（`histype == HIST_SEARCH`・ex_getln.c 9.1）。Nib は足す口 `remember` の 1 か所で同じにする: 入力行に届いた鍵が打った鍵なら印を立て（`note_command_key`・入口は `CommandText` `EditCommand` `confirmed_command` の 3 つ・再生の中・`map` の右辺・`press_vim_keys`（:normal!）の鍵は打っていない）、印は `close_command_input` が倒す。`remember(CommandLine)` は印が立っているときだけ足す。実測（`feedkeys(…, 'xt')`・`D:/NeNeNib/scripts/20261112/c10b/h.vim`）: `nnoremap Z :3<CR>` の打った `Z` は足さない、`nnoremap X :2` に打った `<CR>` だけでも足す、`@r`（`:5`）に打った `<CR>` は足す、`:normal! :1<CR>` は足さない、`@s`（`/l4<CR>`）・map の右辺の `/l2<CR>`・`:normal! /l5<CR>` は検索の履歴に足す。
6. 再生と :normal! の `<Up>` `<Down>` が履歴を動けなければ、残りの鍵を打ち切る（Vim の `c_<Up>` は動けないと `beep_flush` で先読みを捨てる・ex_getln.c）。controller の `recalled_key` が `recalls_command` で動くかを先に見て `not_moved` を返す。窓の上下（`EditCommand`）は打った鍵で、先読みが無いので何もしない（今のまま）。

守り（V-1552）: 反例 r12 `qa:3<CR>:<Up><CR>xq:5<CR>1G@a`（Vim は本文 `l1 l2 3 l4 5`・5 行目）と h1 の B・map の右辺・:normal!・検索の行の契約 `verify_macro_replay_keeps_ex_history`、fixture `macro-ex-history-`（2 件・oracle の :normal! は `:` の履歴が空なので、再生の `:3<CR>` を足すと `:<Up>` が 3 を引き、足さなければ `<Up>` が先読みを捨てる）。

## 追補（2026-11-12・#1525）Ex のエラーは再生を打ち切る

決定 3 の「失敗で打ち切り」に Ex の行を足す。Vim 9.1（`-u NONE -es`・道具 `D:/NeNeNib/scripts/20261111/c10/gen4.py` → `p4.vim` / `out4.txt`）で、本文 abc/def/ghi の 1 行目から `let @a = ":{行}\rj"` の `@a` と `exe "normal! :{行}\rj"` を測ると、Ex のエラーはどれも残りの `j` を流さない（行 1）。成功は流す（行 2）。

| 行 | Vim のエラー | Vim の再生 |
|---|---|---|
| `set so!` `set so!?` `set so&?` `set wrap!?` | E488 | 打ち切る |
| `set invso` `set noso` `set wrap=1` `set noso&` `set ff=nosuch` `set display=grid` | E474 | 打ち切る |
| `set foo` `set invnowrap` `set fontsize=12` `set so=2 foo` | E518 | 打ち切る |
| `foo` | E492 | 打ち切る |
| `3noh` | E481 | 打ち切る |
| `set so=abc` | E521 | 打ち切る |
| `colorscheme nosuchtheme` | E185 | 打ち切る |
| `tabnext 5`（タブ 1 つ） | E475 | 打ち切る |
| `noh` `set so?` `set so&` `set so&vim` `set wrap!` `set invwrap` `set nowrap!` `set ic!` `set so=-1` `echo 1` `set ft=nosuchlang` `set fenc=nosuch` | 成功 | 流す |

打ち切りの道は ADR 0022 の追補 #1526 の決定 6（`evaluate_command` が評価の失敗と設定の保存の失敗で `VimRepeatFailure::refused` を返し、`submit(CommandLine)` がそのまま返す・#1547）の 1 本で、本追補は第 2 の道を作らない。返り値は再生（`@a` `.`）と fixture の harness（`press_vim_keys`）の今の打ち切りの道へ流れる。本追補が足すのは上の表（Vim の実測）と、`:set` の `&` `!` `inv`（ADR 0022 の追補 #1525）で、表の行のうち命令の要求の後の失敗（`tabnext 5` の E475）は決定 6 のとおり今は打ち切らない（残る違い）。

oracle の `-es` は Ex のエラーで終了 1 を返すので、エラーの行は fixture にできない（`run_vim` が拒む・測定のコードを変えると全件の再生成が要る）。エラーの行は上の表の値を契約 `--vim-macro` の `verify_ex_failure_stops_replay`（`:normal!` と `@a` の両方）に置き、成功の行は fixture `ex-failure-`（17 件）で守る。

限界（Vim と違う形）: Vim が受けて Nib が受けない行（`:echo 1`・`:set ft=nosuchlang` `:set fenc=nosuch`（Nib の値は閉じている）・Nib に無い Vim の option）は Nib では拒否なので打ち切る（Vim は流す）。Nib だけの書き方（`:set fontsize=12` `:set guide` `:set display=grid`）は Vim ではエラーで打ち切るが Nib は流す。

## 追補（2026-11-14・#1672）再生の中の `:w` の保存時点を畳んだ履歴へ写す

決定 3 の「再生の全体を 1 つの undo の単位に畳む」は保存時点を動かしていなかった。再生の中の `:w`（#1528 で `:` の行を録るようになって `qa:w<CR>xq` が普通の形になった）は畳む前の本文の位置（`content_position`）で保存時点を覚え、畳むと位置は「再生の前 + 1」に戻るので、数がたまたま合うとファイルに無い本文を保存済みと判定していた（`qa:w<CR>xq` `u` `2@a` で本文 `c`・ファイル `bc`・● 無し・閉じても聞かない・退避を書かない。表は Issue #1672）。

1. 畳む所（`merge_replayed_edits`・タブを替えるときの `close_replayed_unit` も同じ 1 本）が、畳んだ履歴と同時に文書の保存時点を `after_units_folded`（`Document.hpp`・純関数）で写す。畳む前の今の位置と同じ保存時点は畳んだ後の位置へ（ファイルは今の本文）、再生の前の位置以下はそのまま（再生の前の履歴の本文）、ほか（畳んだ単位の途中）は消す（どの位置とも一致しない＝必ず未保存）。undo の単位は割らない（Vim 9.1 の `-es` の実測: `let @a=":w\rx"` の `2@a` の後の `u` は 1 回で全部戻り、`&modified` は `2@a` の後 1・`u` の後 1・`<C-r>` の後 1。`xx:w\r` の `@a` は 0・`u` で 1・`<C-r>` で 0。道具 `D:/NeNeNib/scripts/20261113/replay/m1.vim`）。
2. 再生の中で再生の前の単位を `u` で戻した後の保存は、畳んだ履歴の同じ位置の単位が別の本文を指し得る（`u$x:w<CR>xx` を `0x` の後で `@a` → `u` で本文 `bc`・ファイル `ab`）。controller が再生の中の undo（`undo_edit`）と、その後の保存を 2 つの印で覚え、印が立っていれば再生の前の位置以下の保存時点も消す（安全側）。印は畳みの起点を置く所（`perform(VimReplay)` と `reopen_replayed_unit`）と再生の終わりが倒す。
3. 安全側に倒した結果、保存済みなのに ● が付く形が 2 つ残る: (a) 再生の中で `u` を流した後に保存して、その保存時点へ畳んだ単位の `u` で戻った形（印は再生の前の単位の `u` だけでなく、再生の中で積んだ単位の `u`（`x u :w xxx`）でも立つ）・(b) 再生の前の redo の列の先の保存時点を、本文の差し引きが 0 の再生が畳んだ形（最初の編集の `after_edit_at` が先に消している）。害は閉じるときの余分な問いと余分な退避で、本文は失わない。未保存なのに ● が付かない形は、契約 `--fold-save-point` の形の中では無い。
4. map の右辺（`nnoremap Q x:w<CR>x`）は畳まないので今のまま正しい。`x:wq<CR>x` `x:x<CR>x` は窓の要求で再生を打ち切る（#1569）。`.` は変更だけを記録するので `:w` は入らない。保存時点を改訂番号で持つ形（減らない通しの値）は影響が広いので本追補では取らない（後続の候補）。
5. 同じ種類の穴が再生でない道にもあった（独立レビュー V-1675 の M1・前からある）: 矩形の INSERT の Esc の畳み（`fold_block_into_insert`）は一番上の INSERT の単位を外して、写した行と合わせた単位に置き換える。1 行目を打った直後の Ctrl+S（`<C-o>` を通らず矩形の待ちが残る）の保存時点は外した単位の位置を指し、置き換えた単位と同じ数になる（本文 `aa` `bb` の 2 行で `<C-v>jIX` Ctrl+S `<Esc>` → 本文は 2 行とも `X` つき・ファイルは 1 行目だけ・● 無し）。置き換えも同じ純関数 `after_units_folded` を通す（kept は外した単位の下の位置・replayed は写した後の位置・folded は置き換えた後の位置）。`I` `A` `$A` `c` `C`・3 行・`u` `<C-r>` `.`・保存の失敗・マクロの中の矩形を契約に置いた。保存の後に打ってから Esc した形・保存の後に `<BS>` で消してから Esc した形・1 行だけの矩形・`<C-o>:w`（矩形の待ちを捨てる）・保存の失敗は前から正しい。
6. 単位をまとめる・置き換える口（レビューの表）: `EditHistory::pushed` の coalesce・absorb と空の単位への absorb は、保存が seal するので保存した単位へ畳まない。`undone` `redone` `past_saved_only_units` は位置だけを動かす。`after_edit_at` は redo の列を切るときに先の保存時点を消す。タブの切り替えは seal だけ。畳む口は再生と矩形の 2 つで、どちらも `after_units_folded` を通す。後から単位を畳む・置き換える口を足す変更（例: `:s` の `c` の問いの畳み）は、同じ純関数を通す。
7. Vim 9.1 と違う所（直さない・害なし）: (1) 差し引き 0 の `xP:w<CR>` の再生の後の `u` は、Vim は未保存（`&modified` 1）、Nib は保存済みのまま（畳む単位が無い）。(2) 手で打つ INSERT の `<C-o>:w` は、Vim では undo の 1 単位のまま、Nib は保存の seal で 2 単位に分かれる（保存の判定は正しい）。
