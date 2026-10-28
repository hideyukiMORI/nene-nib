# ADR 0165 — commentary・surround・autopairs は engine の操作で、囲みと記号の表は core の 1 つずつ

- 状態: 受理（施主決定 D67・FR-024・2026-10-28。実装は 3 席（commentary → surround → autopairs）・受理は D41 の形）
- 日付: 2026-10-28
- Issue: #1000（commentary）・#1001（surround）・#1004（autopairs）・計画 `docs/plans/2026-10-28-editing-features-roadmap.md` §1 の 2・3・6
- 規則: FR-024、ARC-001 / ARC-003 / ARC-004、CPP-002 / CPP-005 / CPP-011 / CPP-012、QLT-014、ADR 0012（oracle・本機能は oracle 不能）/ 0015（オペレータとレジスタ）/ 0030（`.`）/ 0031（テキストオブジェクトの範囲関数）/ 0033（VISUAL の `.`）/ 0052（undo のキャレット）/ 0154（言語の表）/ 0156（構文の区間）

## 文脈

vim-commentary（`gc`）・vim-surround（`ys` `ds` `cs`）・autopairs は Vim / Neovim で最も使われる編集の plugin で、どれも「範囲（移動かテキストオブジェクト）に対する 1 回の編集」として Nib の engine の形（オペレータ → 範囲関数 → `Edit` 1 つ・`.` の記録・undo 1 単位・ADR 0015 / 0030 / 0031）に載る。本物の Vim に無いので oracle の fixture は作れず（ADR 0012 の外）、契約と性質の試験で守る。D67 の速さの条件: engine の中で完結・範囲の行数に比例・使っていないときの打鍵の道に分岐を足さない。

## 決定

### commentary（#1000）

1. **鍵。** NORMAL の `gc{motion}`（オペレータ・`VimOperator::comment`）・`gcc`（行・回数は行数）・`gcgc` / `gcu`（周りのコメントの行の塊）・VISUAL（3 種）の `gc`（行単位に効く・矩形も行）。`g` の接頭辞の表（`VimPrefix::g`）に `c` を足す。`.` で繰り返す（記録は既存の鍵の列）。undo 1 単位・レジスタは書かない（ADR 0015 の決定 9〜11 と同じ）。
2. **記号の表は core の 1 つ。** `comment_tokens(Language) -> CommentTokens{std::string_view line; std::string_view block_open; std::string_view block_close;}`（`constexpr` の表・`Language` の宣言の順・`LanguageTable` の隣の 1 ファイル）。行コメントの無い言語（HTML / XML / CSS / Markdown / JSON（無し）/ plain）は行の先頭に `block_open` 行末に `block_close`。JSON と plain は「記号が無い」（`std::nullopt`・ビープ）。
3. **付けるか外すかは範囲の全行で決める（vim-commentary と同じ）。** 範囲の空行でない全行が記号で始まっていれば外し、そうでなければ付ける。付けるときは範囲の最小の字下げの位置に `記号 + 空白`（空行は触らない）。外すときは記号と直後の空白 1 つを消す。CRLF は保つ。
4. **範囲は 1 回の `Edit`。** 行ごとの挿入・削除を `VimInsertBlock` と同じく 1 つの束にして controller が 1 単位で写す（矩形の `~` と同じ形・#981）。キャレットは範囲の先頭の行の最初の非空白（vim-commentary の実測に合わせる・契約で固定）。

### surround（#1001）

5. **鍵。** `ys{motion}{char}`（囲む）・`yss{char}`（行）・`yS` `ySS`（囲みを別の行に・後続）・`ds{char}`（外す）・`cs{old}{new}`（替える）・VISUAL の `S{char}`（文字単位と行単位・矩形は後続）。`y` の後の `s` は `VimInputWait` の新しい値（`y` は yank のオペレータなので `ys` は「yank の待ちの `s`」として表に 1 行）。`{char}` は 1 鍵（ASCII）で、`{old}` はテキストオブジェクトの `a{char}` の範囲、`{new}` は囲みの対。`.` で繰り返す（`cs` `ds` は記録に載る・`ys` は motion つきの記録）。undo 1 単位・レジスタは書かない。
6. **対の表は core の 1 つ。** `surround_pair(char32_t) -> std::optional<SurroundPair{std::string_view open; std::string_view close; bool spaced;}>`: `(` `{` `[` `<` は空白つき（`( x )`）・`)` `}` `]` `>` は無し・`b` `B` `r` `a` は `)` `}` `]` `>` の別名・`"` `'` `` ` `` は同じ・`t` はタグ（`<tag>` の入力行・後続の 1 文字の入力行の形を使う・ADR 0163 の決定 4）・**全角の `（` `）` `「` `」` `『` `』` `【` `】` `〈` `〉`** は対のまま（2 バイト対応・開きと閉じのどちらを打っても同じ対）。表に無い文字は「その文字を両側に」（vim-surround と同じ）。
7. **範囲はテキストオブジェクトの 1 本。** `ds` `cs` の `{old}` は `a{char}` の範囲関数（ADR 0031・`vi(` と同じ探索・VISUAL の起点の規則は #982）で、見つからなければビープ。`ys` の motion は既存の `motion_range`。囲みは範囲の両端に挿入する 2 つの編集を 1 つの束に。キャレットは vim-surround の実測（`ys` は範囲の先頭・`ds` `cs` は開きの位置）に合わせて契約で固定。

### autopairs（#1004）

8. **設定で切る・既定オフ。** `autopairs`（設定 v4 の `features` の欄・D68）。`:set autopairs` / `noautopairs` / `autopairs?`。オフのとき INSERT の打鍵の道に分岐を足さない（engine の INSERT の鍵の表を引く前に状態の 1 bit を見るだけ）。
9. **規則（nvim-autopairs の既定の部分集合）。** 開き `(` `[` `{` `"` `'` `` ` `` を打つと閉じを補いキャレットは間・閉じの上で同じ閉じを打つと通り抜け・空の対の間の Backspace は両方消す・空の対の間の Enter は 3 行（閉じを次の次の行へ・字下げは現在行と同じ）・`'` は直前が英数字なら補わない（`don't`）・直後が英数字なら補わない。対の表は決定 6 の表の ASCII の部分を読む（2 本にしない）。
10. **記録と undo。** 補った閉じは同じ打鍵の `Edit` の中（undo 1 単位は INSERT のまま）。`.` の記録は打った鍵のまま（再生時も autopairs の状態で同じに補う）。

### 共通

11. **契約。** `--vim-commentary`（28 言語の記号・付ける / 外す・最小の字下げ・空行・CRLF・`gcc` の回数・`gcgc`・VISUAL・`.`・undo・レジスタ不変）・`--vim-surround`（対の表・`ys` `ds` `cs` `S`・全角の対・見つからない・`.`・undo）・`--vim-autopairs`（9 の規則・オフで分岐なし・`.` の再生）。性質の試験: ランダムな本文と範囲で「付けて外すと元に戻る」（commentary）・「囲んで外すと元に戻る」（surround）・UTF-8 と改行の形の不変。fuzz の語には載せない（oracle 不能）。
12. **費用。** 範囲の行数に比例。表は `constexpr`。任意のベンチは要らない（打鍵の道に常駐しない・autopairs はオフで分岐なし・オンの A/B を PR 本文に）。
13. **限界。** `yS` `ySS`（別の行に囲む）・矩形の `S`・`cst`（タグの替え）・`gcap` の段落の記号の切り替え・autopairs の `fast wrap`・言語ごとの `'` の規則（Lisp）。

## 強制

- engine が純関数・OS に触れない: **active**（`eng/symbols.py`）。
- 記号の表と対の表が 1 つずつ（呼び出し元で記号を書かない）: レビュー事項 ＋ CNF-016（文のリテラルの検査・ADR 0123 の改訂）が記号のリテラルも拾う。
- 使っていないときの打鍵の道の不変と 8 本の速さ: **active**（QLT-014）。
- oracle の fixture: **不能**。

## 結果

得られるもの: `gcc` と `ysiw)` が Vim の plugin と同じ打ち方で効く。全角の括弧も囲める。
失うもの・残る穴: Vim の plugin との細かい差（キャレットの位置・空白の扱い）は契約で固定した値で、本物の plugin と突き合わせる機械は無い。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 言語ごとの記号を字句器から引く | 字句器は「読む」側で「書く」記号を持たない。表 1 つの方が網羅性で守れる |
| autopairs を既定オン | 好みが分かれる。D67 の「速さ最優先」で既定の打鍵の道を変えない |
| surround を `s` 系の鍵に | `s` は #768 の置換 |
