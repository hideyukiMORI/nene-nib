# ADR 0048 — 名前つきレジスタ `"a` はマクロと同じ 26 本の本文の表で、鍵列 ↔ 本文の写しは core の 1 対

- 状態: 受理（設計席 2026-09-23・Issue #193。hide 未確認・引き継ぎ 09-23 の「次の順」の 1 番目）
- 日付: 2026-09-23
- Issue: #193
- 影響する規則: FR-003 / ARC-001 / ARC-004 / ARC-010 / ARC-012 / CPP-002 / CPP-005 / CPP-007 / CPP-011 / QLT-001 / QLT-012 / CNF-010 / CNF-011
- 前提: [ADR 0046](0046-macros-record-keys-and-replay-through-dot-path.md)（マクロは鍵列・決定 8 が本 ADR の予告）・[ADR 0030](0030-vim-dot-repeat-as-key-replay.md)（`.` は鍵の再生）・[ADR 0032](0032-vim-search-as-input-line-and-one-key.md)（確定した検索は `VimSearchPattern` の 1 鍵）・[ADR 0035](0035-vim-visual-block-as-column-ranges.md)（幅つきレジスタ）・[ADR 0040](0040-display-line-with-control-glyphs.md)（制御文字と C1 の描画）

## 文脈

`:help registers`: レジスタは 1 つの表で、`"{a-z}` は本文を置き、`"{A-Z}` は追記し、`q{a-z}` の録画も同じ場所へ「打った鍵の文字」を置く。だから `"ap` はマクロの鍵列を文字として貼り、`"ayy` の後の `@a` は本文を鍵として実行する。無名レジスタ `""` は最後に書いたレジスタを指し、`"_` は捨てる。

本物の Vim 9.1 の実測（Sonnet の probe `probe-registers.md`・`-es` ＋ `:normal!` と `feedkeys(…, 'xt')`）:

- `"ayy`（`V` 型・`jj\n`）の後の `@a` は `j` `j` に続けて末尾の改行が `<NL>` として効き、もう 1 行下がる。
- `qaxq` の後の `"ap` は `x`（`v` 型）。`qaiab<Esc>q` は `iab` ＋ 0x1b、`qa/ab<CR>xq` は `/ab` ＋ 0x0d ＋ `x`、`qa<Left>q` は `<80>kl` の 3 バイトが literal に入る。
- `"ayy`（`V`・`abcdef\n`）へ `qAxq` で追記すると `abcdefx\n` で `V` を保つ（末尾の改行の手前へ繋ぐ）。
- `"a"byy` は後勝ち。`"a3yy` と `3"ayy` は同じ。`"a<Esc>` は接頭辞を捨てて何も変えない。`"aj` は移動だけでレジスタは不変。
- 名前つきへ書くと無名も同じになる（`"ayy` の後の `p` は `"ap` と同じ）。`"_dd` は無名を変えない。
- `"add` の後の `.` は同じ `"a` へ置き換え（最後の 1 行だけ残る）。`"Add` の後の `.` `.` は追記を繰り返して育つ。
- `:normal!` の中で `"ayy` `"ap` `"Ayy` は普通に動く（録画 `q` だけが動かない）。`let @a=` と `setreg(…, 'l'/'c')` で種類を置ける。

Nib の今の形: 無名レジスタ `VimRegister { text; kind; width }` は engine が `register_after`（`VimStep.cpp:1033`）で直接書き、`put_step` が読む。マクロは `VimMacroRegisters`（a〜z の `std::vector<VimKey>`）で、`stopped_macro` が置き `replayed_macro` が `VimReplay` にする。鍵 ↔ 本文の写しは core / application に無い（テストの `vim_keys_of` が文字列 → 鍵の片道だけ）。`"` は `action_for` に無く `refused`。`.` の記録 `normal_recording` は回数の桁だけを除く。controller の `perform(VimReplay)` は 1 鍵を `step_vim`（engine 直結）へ流し、入力行を通らない。NORMAL の `<CR>` は `cancelled`、`+` `-` `<NL>` は未束縛。

## 決定

**レジスタは a〜z の 26 本の `VimRegister`（本文）の表 1 つで、マクロも本文で持つ。`"` の接頭辞が書き先と読み元を選ぶ。鍵列 ↔ 本文の写しは core の 1 対の関数で、録画を止めるとき鍵列 → 本文、`@a` のとき本文 → 鍵列に写す。再生は窓の鍵と同じ口を通り、本文の `/ab\r` は入力行を経て確定する。**

1. **表（core）**: `VimNamedRegisters`（`src/core/VimNamedRegisters.hpp`・1 型・`std::array<VimRegister, 26>`）が `VimMacroRegisters` を置き換える。`vim_macro_index` は `vim_register_index` に改名（規則は同じ: `a-z` と `A-Z` が同じ位置）。`VimState.macros` → `registers`。`VimMacroRecording`（録画中の鍵列）と `last_macro`（`@@` 用）はそのまま。空のレジスタは `VimRegister{"", uninitialized}`。無名レジスタ `unnamed_register` は今の場所のまま（Vim の「最後に書いたレジスタを指す」は値の写しで表す・決定 3）。
2. **接頭辞 `"`**: `VimPrefix::quote` を足し、NORMAL / VISUAL の `"` は次の 1 鍵を待つ（`q` `@` と同じ待ち）。`a`〜`z` は名前つき（追記なし）、`A`〜`Z` は追記、`"` は無名、`_` はブラックホール。それ以外の鍵は `refused`（接頭辞と回数を捨てる・ビープ相当・Vim の `"!`）。結果は `VimState.register`（`std::optional<VimRegisterSelection>`）。`VimRegisterSelection { VimRegisterTarget target; char name; bool append; }`（`src/core/VimRegisterSelection.hpp`・1 型）、`VimRegisterTarget { named, unnamed, black_hole }`（`src/core/VimRegisterTarget.hpp`・閉じた enum）。回数は `"` の前後どちらに置いても同じ（`"` は積んだ回数を消さず、後ろの桁は普通に積む）。`"a"b` は後勝ち。`<Esc>` は選択と回数を捨てる（`cancelled`）。命令が完了して `vim_resting_from` を通ると選択は消える（`"aj` は移動だけ・`"ayy` の次の `yy` は無名へ）。
3. **書き**: 削除・変更・yank（`x` `dd` `d{motion}` `c{motion}` `cc` `C` `D` `yy` `y{motion}` `Y`・VISUAL と矩形の `d` `x` `y`）は今の `register_after` の結果を 1 本の `registers_written(state, value)` に通す。`black_hole` → 何も変えない（無名も不変）。`unnamed`（`""` か接頭辞なし）→ 無名へ（今と同じ）。`named` → `registers[i]` へ置き（`append` なら決定 4 で繋ぐ）、**無名へも同じ値（追記後の全体）を写す**。空の文字単位の範囲は今までどおり書かない。
4. **追記の規則**: yank 系の `"A`: 種類は「どちらかが `lines` なら `lines`、さもなくば `characters`」。本文は old が `characters` で new が `lines` なら old ＋ `"\n"` ＋ new、old が `lines` で new が `characters` なら old ＋ new ＋ `"\n"`、同じ種類なら old ＋ new。old が `uninitialized`（空）なら new そのもの。`block` が絡む追記（`"A` で矩形を取る・矩形レジスタへ追記）は `refused`（Vim と違う・後続）。録画の追記（`qA…q`）は「本文の末尾の改行の手前へ繋ぐ」（`lines` なら末尾の `"\n"` の前・`characters` なら末尾）・種類は保つ（実測の `abcdefx\n`）。
5. **読み**: `p` `P`（矩形を含む）は選択が `named` なら `registers[i]`、`unnamed` か無しなら無名、`black_hole` なら空（`VimNoEffect`・Vim の `"_p` は何もしない）。`"Ap` は `"ap` と同じ。`uninitialized` は今の「空なら何もしない」と同じ。
6. **マクロ**: 録画を止める `q` は録った鍵列を `vim_register_text(keys)` で本文にし `characters` として置く（追記は決定 4 の録画の規則）。`@{a-z}` `@@` は `registers[i].text` を `vim_keys_of_text(text)` で鍵列にして `count` 回繋いだ `VimReplay`（`lines` の末尾の `"\n"` はそのまま鍵になる・決定 9）。`@"` は無名を実行する（足す）。空は `refused`（今と同じ）。application の `StoreVimMacro` は `StoreVimRegister { char name; VimRegister value; }` に変える（後続の `:let @a=` も同じ口）。fixture の harness は今の「別の editor で `q{name}` を打って録る」経路の結果の鍵列を `vim_register_text` で本文にして置く（録画の経路を 1 本のまま守る・ADR 0046）。
7. **鍵列 ↔ 本文の写し（core・`src/core/VimRegisterText.hpp` / `.cpp`・1 対）**: `vim_register_text(std::span<const VimKey>) → std::string` と `vim_keys_of_text(std::string_view) → std::vector<VimKey>`。
   - `VimCharacter` ↔ その UTF-8。
   - `VimSpecialKey` ↔ Vim が typeahead に置くバイト（閉じた `switch`・鍵が増えたらコンパイルが落ちる・CPP-002）: `escape` `\x1b`・`enter` `\r`・`control_r` `\x12`・`control_d` `\x04`・`control_u` `\x15`・`control_f` `\x06`・`control_b` `\x02`・`control_v` `\x16`・`backspace` `<80>kb`・`arrow_left` `<80>kl`・`arrow_right` `<80>kr`・`arrow_up` `<80>ku`・`arrow_down` `<80>kd`・`home` `<80>kh`・`end` `<80>@7`・`page_up` `<80>kP`・`page_down` `<80>kN`。ここで `<80>` は **U+0080（UTF-8 で `C2 80`）**。Vim の `K_SPECIAL` は生の 1 バイト 0x80 だが、本文は正しい UTF-8 でなければならず（CPP-007・`TextBuffer` の不変条件）、描画は ADR 0040 の `<80>` で Vim と同じ見た目になる。
   - `VimSearchPattern { direction, pattern }` → `/` か `?` ＋ pattern ＋ `\r`。逆向きは無い（本文の `/` は文字のまま鍵になり、決定 8 で入力行を通って確定する）。
   - 本文 → 鍵列は上の逆に加えて `\x08` も `backspace`（Vim の Ctrl-H）。U+0080 の後ろの 2 文字が表に無ければ U+0080 も文字。`\n` は `VimCharacter{U'\n'}`（決定 9）。それ以外の制御文字は文字（engine が知らない鍵は `refused` で再生が止まる）。
   - 往復: `VimSearchPattern` を含まない鍵列で `vim_keys_of_text(vim_register_text(keys)) == keys`（契約）。
8. **再生は窓の鍵と同じ口**: `perform(VimReplay)` の 1 鍵は `step_vim` 直結ではなく、窓の `VimKeyPress` が通る配送と同じ 1 本（入力行が開いていれば `CommandText` / `SubmitCommand` / `CancelCommand` と同じ写し・閉じていれば engine へ）。これで本文の `/ab\r` は `/` が `VimOpenSearch` で入力行を開き、`a` `b` が入力行へ入り、`\r` が確定して `VimSearchPattern` の鍵として engine に届く。`.` の記録に入っている `VimSearchPattern` は今までどおり engine へ。失敗の打ち切りは engine の失敗と検索の確定の失敗（E486）。**再生の中の入力行の `<Esc>` は取消ではなく確定**（`:help c_<Esc>`「マクロの中では入力した命令を実行する」・工程 2 の実測: `/ab<Esc>x` の `@a` は `one ab` → `one b`。窓で打った `<Esc>` は今までどおり取消）。入力行の中の矢印・Home / End・`<BS>` は窓と同じ写し（`EditCommand`）で、上下（履歴）は捨てる。写しの表は controller の 1 か所（`command_key`）で、テストの harness も同じ口を使う。入力行を開いたまま再生が終わったら開いたまま（Vim も同じ）。incsearch の preview は再生の中でも動くが描画は `WM_PAINT` の 1 フレームに 1 回のまま。`.` と `@a` の再生の経路は引き続き 1 本。
9. **`<NL>` `<CR>` `+` `-`（NORMAL / VISUAL）**: `<NL>`（`VimCharacter{U'\n'}`・Vim の Ctrl-J）は `j` と同じ。`<CR>` と `+` は次の行の最初の非空白、`-` は前の行の最初の非空白（`:help +`・回数つき・動けなければ `j` `k` と同じ失敗）。VISUAL でも同じ移動。fixture の記法に `<NL>` を足す（`eng/vim-oracle.py` の `KEY_NAMES` と `tests/unit/VimTestSupport.hpp` の `vim_key_names` の両方・ARC-012 の既知の 2 か所）。
10. **`.` の記録**: `"` とその名前は回数の桁と違って記録に残す（`normal_recording` の除外に載せない）。`"add` の後の `.` は `"a` へ置き換え、`"Add` の後の `.` は追記を繰り返す。`"ap` の後の `.` も同じ名前から貼る。
11. **fixture と契約**: `register` 欄は今の形（`name`・`keys`）のまま。名前つきの本文は `"ayy` 等の鍵列で作れば `-es` で観測できるので新しい欄は足さない。期待値は本文・キャレット・無名レジスタ（`register_text` / `register_kind`）。fixture は 30 件程度: `"ayy"ap`・`"add"ap`・`"a3yy` と `3"ayy`・`"a"byy`・`"a<Esc>yy`・`"aj`・`"_dd` の後の `p`・`"Ayy` / `"Ayw` の 4 組・`"ayy@a`・`"ayy@"`・`""yy`・VISUAL の `"ay` `"ad`・矩形の `"ay"ap`・`"add.`・`"Add..`・`"ap.`・`<CR>` `+` `-` `<NL>`。録画がらみ（`qaxq"ap`・`"ayyqAxq`・`qaxq"Ayy`・特殊鍵の本文 `<Esc>` `<CR>` `<Left>`・往復・`block` の追記の `refused`・`@"`・空）は契約 `--vim-macro`（期待値は `feedkeys(…, 'xt')` で実測）。
12. **後続（別 Issue）**: 数字レジスタ `0-9`・`+ *`（クリップボード）・`- . : / %`・`q"`・`@:`・`:let @a=`・`:reg`・矩形レジスタへの追記・`showcmd` 相当の `"a` の表示・Ex を含むマクロ。

## 強制

- fixture（決定 11）: **active**（CTest・CNF-010 / 011）。
- 契約 `--vim-macro`（録画がらみ・往復・特殊鍵の本文）: **active**（`eng/protected-diff.py --allow --vim-macro`）。
- 閉じた和型と enum の写し漏れ（`VimSpecialKey` ↔ 本文・`VimRegisterTarget`・`VimPrefix`）: **active**（`switch` / `std::visit` の網羅性・CPP-002）。
- 本文の不変条件（レジスタの本文は正しい UTF-8）: **active**（`TextBuffer` の `from_utf8` / `replaced` が守る・CPP-007）。
- 鍵列 ↔ 本文の写しが core の 1 対だけであること・再生の口が窓の鍵と同じ 1 本であること: **planned**（レビュー事項・`grep -rn "vim_register_text\|vim_keys_of_text"` が core と harness と契約だけ）。

## 結果

得られるもの: `"ayy` `"ap` `"add` `"Ayy` `""` `"_`・`"ap` がマクロを文字として貼る・`"ayy@a` が本文を鍵として実行する・`.` がレジスタ名と追記を繰り返す・`<CR>` `+` `-`。マクロと本文の表が 1 つになり、`VimMacroRegisters` と `StoreVimMacro` は消える。engine の 1 鍵 1 効果は不変。
失うもの・残る穴: 矩形への追記は `refused`（Vim は繋ぐ）。特殊鍵の本文は Vim の `K_SPECIAL`（生の 0x80）ではなく U+0080 なので、貼ってから保存したバイト列は Vim と違う（描画は同じ）。数字・クリップボード・読み取り専用のレジスタは無い。録画がらみは oracle で観測できず契約だけ（ADR 0046 と同じ）。録画中に窓で打った入力行の `<Esc>` は取消なので engine に届かず、Vim が録る `/ab<Esc>`（再生で確定）は Nib では録れない（後続）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| マクロは鍵列の表のまま、`"ap` と `@a` のときだけ写す（表 2 つ） | 同じレジスタ名の実体が 2 つになり（ARC-001）、`qA` で本文のレジスタへ追記するとき結局写しが要る。Vim は表 1 つ |
| 本文 → 鍵列で `/…\r` を文脈なしに `VimSearchPattern` に組み立てる | INSERT の中の `/cd\r` を検索と誤読して黙って違う再生になる。入力行を通せば engine の文法を core の外へ写さずに済む（決定 8） |
| 特殊鍵を Vim と同じ生の 0x80 で本文に置く | `TextBuffer` の不変条件（正しい UTF-8）を破る。U+0080 なら描画が `<80>` で Vim と同じ |
| 特殊鍵を私用領域（U+E000〜）に置く | 描画が Vim と違う（`<80>kl` に見えない）。得るものが無い |
| 特殊鍵を含む録画を `refused` にする | INSERT の中の矢印・Home / End（ADR 0028）が普通の打鍵なので、それを含むマクロが録れなくなる |
| `<NL>` を足さず `"ayy@a` の末尾の改行を無視する | Vim と着地が 1 行ずれる（実測）。`<NL>` `<CR>` `+` `-` は表に 4 行足すだけ |
