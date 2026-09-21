# ADR 0031 — テキストオブジェクトは 1 本の範囲関数で d c y と VISUAL に同じ範囲を渡す

- 状態: 提案（実測で決定 1〜8 を確認したら受理へ更新する）
- 日付: 2026-09-22
- Issue: #93
- 影響する規則: ARC-001 / ARC-004 / ARC-007 / ARC-009 / CPP-002 / CPP-004 / CPP-006 / CPP-011 / CPP-012 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / CNF-010

## 文脈

FR-003 の T2「テキストオブジェクト `iw aw i" a" i( a( i{ it`」を実装する。engine はオペレータの範囲を `VimMotionRange`（範囲＋文字/行の種類）1 本で決め（ADR 0015 の決定 2）、VISUAL の選択も同じ型で描画・Ctrl+C・`d x y c` に渡す（ADR 0018 の決定 4）。次キー待ちは排他的な `VimInputWait`（ADR 0027 / 0029）。`.` は鍵の列を再生する（ADR 0030）ので、テキストオブジェクトは鍵として記録されれば追加の記録なしに再生対象になる。

`i` / `a` は NORMAL では挿入命令で、オペレータ保留中と VISUAL でだけ「内側 / 周り」の接頭辞になる。固定 Vim 9.1 の help（`:help text-objects`）と実測を根拠にし、Vim ソースは読まない。実測で食い違ったら決定を直し、fixture を都合よく変えない。

## 決定

**テキストオブジェクトの範囲は純関数 1 本が決め、オペレータの後ろでも VISUAL でも同じ範囲を使う。`i` / `a` は保留中と VISUAL だけの排他的な次キー待ちとして持つ。**

1. `VimInputWait` に `VimTextObjectScope`（`inner` / `around`）を足す。オペレータ保留中と VISUAL / 行単位 VISUAL の `i` / `a` でこの待ちに入り、次の鍵を閉じた `VimTextObject`（`word` / `big_word` / `double_quote` / `single_quote` / `backtick` / `paren` / `brace` / `bracket` / `angle`）に表で写す。`b` は `paren`、`B` は `brace` の別名として同じ表の行。表に無い鍵と Esc は取消で、既存の待ちと同じくモード・選択・希望列・検索記憶を保つ。`it` / `at`・`is` / `as`・`ip` / `ap` はこの縦切りに入れない。
2. 範囲は新しい純関数 `vim_text_object_range(text, caret, scope, object, count)` が `std::optional<VimMotionRange>` で返す（`src/core/VimTextObjectRange.cpp`・1 ファイル 1 型・CPP-011）。語の種類は既存の `VimCharacterRange` の表（ADR 0012 の決定 5）を使い、通常モードの `moved_caret` には触れない。見つからなければ nullopt で取消。
3. オペレータの後ろでは既存の `performed` に渡す。`adjusted_for_exclusive` は通さない（テキストオブジェクトは exclusive / inclusive の motion ではない）。`whole_lines_for_delete` の Vi 互換規則は d にだけ既存どおり通す（実測で確認）。`cw` の特例（`word_end_for_change`）は `ciw` に適用しない。`y` はキャレットを範囲の先頭へ戻す（既存の `yanked_caret`）。
4. VISUAL では選択を範囲に置き換える（anchor = 範囲の先頭、caret = 末尾の文字）。行単位になる範囲（決定 7）は VISUAL を行単位に切り替える。選択が 1 文字より大きいときの拡張（`viwiw` は語と空白を交互に 1 つずつ、`vi(i(` は 1 段外側）は実測で規則を確かめ、`count` の連続扱いに畳む。実測で規則が閉じなければ「選択が 1 文字のときだけ」に絞り、残りを Issue に書いて後続にする。
5. 回数は積（`2di(` = `d2i(`）。語は語と空白の塊を交互に数え、引用符は 2 で引用符を含む（空白は含まない）、括弧は段の数。
6. 引用符は行内だけを見る。`\` の直後の引用符は数えない（`quoteescape` の既定）。キャレットが引用符の上なら行頭から数えて対になる側を決め、行内の最初の引用より前なら最初の対を選ぶ。`a"` は後ろの空白を含み、無ければ前の空白を含む。`i"` は引用符の内側だけ。対が無ければ取消。
7. 括弧は入れ子を飛ばして後ろへ未対応の開きを探し、前へ対になる閉じを探す。複数行をまたぐ。`i(` で開きの直後が改行で閉じの前が空白だけなら、中の行だけの行単位の範囲にする（Vim の `di(` が中の行を丸ごと消す振る舞い。実測で確認）。`a(` は括弧を含む文字単位。キャレットが開きの上なら自分を開きとし、閉じの上なら自分を閉じとする。
8. 記録・再生（ADR 0030）・UI・IME・描画・保存形式は変えない。VISUAL でのテキストオブジェクトも鍵として記録されるが、VISUAL の変更の `.` は #91 の範囲。

## 強制

- 網羅性: **active**（`VimTextObject` の switch・`VimInputWait` の visit・`VimTextObjectScope` の switch。選択肢が増えたらコンパイルが落ちる）
- 範囲の正しさ・取消・VISUAL の置換・再生: **active**（oracle fixture と `--vim-text-objects` の対象 unit・CNF-010 で生成物の一致）
- 期待値が本物の Vim の答えであること: **不能**（CI に Vim は無い。ADR 0012 のとおり）

## 結果

得られるもの: `diw` `ciw` `di"` `ci(` など T2 で最も使う編集が d/c/y と VISUAL と `.` の全部で使える。範囲の規則が 1 か所なので、`> < gu gU` を足すときも範囲は再利用できる。
失うもの・残る穴: タグ・文・段落は後続。VISUAL の拡張規則は実測で閉じた範囲だけ。Vim の `iw` は `iskeyword` に従うが、いまの語の種類表は ADR 0012 の範囲（ハングル・絵文字は未測）。`quoteescape` と `matchpairs` の設定は既定固定。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| テキストオブジェクトを `VimMotion` の列挙に足す | motion はキャレットから 1 方向へ伸びる範囲で exclusive / inclusive の補正を通る。テキストオブジェクトはキャレットを含む両側の範囲で補正が違う。表を分けたほうが分岐が閉じる |
| `i` / `a` を `VimPrefix` に足す | `VimPrefix` は NORMAL の接頭辞（g / r）。`i` / `a` は NORMAL では挿入なので、保留中 / VISUAL だけで意味を持つ別の待ちにする |
| VISUAL の拡張規則を最初から全部実装する | `viwiw` / `vi(i(` の拡張は実測で閉じてから。閉じなければ 1 文字のときだけに絞る |
| 範囲関数をオペレータ用と VISUAL 用に分ける | 同じ意味に 2 つの経路ができ、片方だけ直す事故が起きる（ARC-001） |
