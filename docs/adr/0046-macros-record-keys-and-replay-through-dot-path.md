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
8. **後続（別 Issue）**: ステータスバーの「recording @a」の表示（renderer・application の表示値に 1 欄）。`"a` の接頭辞（`"ayy` `"ap`・名前つきのテキストレジスタ・`VimRegister` の 26 本表）と、そのときのマクロレジスタとの統合（Vim はレジスタが 1 つの表で `"ap` が鍵列を文字として貼る。統合の ADR で鍵列 ↔ 文字列の写しを 1 本にする）。`@:`・`q"`・数字レジスタ。

## 強制

- fixture（`register` 欄・再生だけ）: **active**（CTest・CNF-010 / 011 で生成物と json の一致）。
- 契約 `--vim-macro`: **active**（`eng/protected-diff.py --allow --vim-macro`）。
- `keys` に `q` を含む fixture の拒否: **active**（`eng/vim-oracle.py`・conformance の反例 1 つ）。
- 再生の経路が `.` と同じ 1 本（`perform(VimReplay)`）: **planned**（レビュー事項）。

## 結果

得られるもの: `qa…q` `@a` `@@` `3@a`、再帰マクロ、undo 1 単位、失敗で打ち切り。engine の 1 鍵 1 効果は不変（`@a` も `.` と同じく「鍵列を返す 1 効果」）。
失うもの・残る穴: 録画は oracle で観測できず契約だけ。失敗の印（`VimStep.failure`）は主な経路だけで、スクロールと VISUAL の未対応の鍵は再生を打ち切らない（後続）。oracle の `q` の拒否は検索の入力の外の `q` を一律に拒むので `fq` や挿入文字の `q` も書けない（拒みすぎる側・後続）。検索の失敗（E486）は Vim が `-es` でエラー終了するので fixture にできず契約で守る。Ex を含むマクロ・`"ap`・recording の表示（#180）・数字レジスタは後続。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| engine の中で `@a` の鍵列を回す | controller の undo の単位（`sealed()`）と `.` の再生経路が 2 本になる（ARC-001）。`.` と同じ `VimReplay` で足りる |
| マクロを `VimRegister`（本文の型）に文字列で入れる | 鍵列 → 文字列 → 鍵列の写しが今は無く、`"ap` も無い。統合は名前つきテキストレジスタの ADR で |
| `q` を含む鍵列を oracle で fixture にする | `:normal!` の中で録画は動かず、黙って間違った期待値になる（probe 3 節） |
| 再帰の上限を Vim と同じ 1000 に | 1 鍵ずつ `accept` に流す入れ子が 1000 段は深すぎる。100 で再帰マクロの慣用（終端は失敗）に足りる |
