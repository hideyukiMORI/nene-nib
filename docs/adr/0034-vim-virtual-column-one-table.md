# ADR 0034 — Vim の桁は core の表示幅の表 1 本で仮想桁に直し、使う経路を閉じる

- 状態: 受理（2026-09-22 に固定 Vim 9.1 で実測し、決定 1・2・3 を実測に合わせて直した。差は「補足」に残す）
- 日付: 2026-09-22
- Issue: #108
- 影響する規則: FR-003 / ARC-001 / ARC-003 / ARC-007 / CPP-001 / CPP-002 / CPP-004 / CPP-011 / CPP-012 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / CNF-010

## 文脈

engine の桁は `Column`（code point・1 始まり）で一貫している（ADR 0009 / 0012）。固定 Vim 9.1 は `j` `k` の欲しい列（`curswant`）・VISUAL の変更の `.` の再生（ADR 0033）・`Ctrl-v` の矩形を**仮想桁**（`virtcol`。Tab は次の 8 の倍数まで、全角は 2 桁）で数える。ADR 0012 の「文脈」はこの差を「矩形の縦切りで `virtcol` に直す」と予告し、ADR 0033 は Tab と幅の混在を fixture に採らず engine の答えを unit で固定した（Issue #91 の実測: `ab<Tab>cd` の `vll` の記録は Vim では次の行から 8 文字消す。`abc` の記録はひらがなの行で 2 文字）。

いまの fixture で Tab か全角を含む本文に `j` `k` `H M L` を打つものは 9 件だけで、どれも桁が一致する入力（`visual-wanted-unicode` 等）に限られている。桁の意味を変えるのは engine 全体に関わるので、`Ctrl-v` の機能とは別の Issue で先に行い、`Ctrl-v`（次の ADR）はこの仮想桁の上に載せる。

表示幅は描画の桁（DirectWrite が決める画素）とは別物で、Vim の意味論のためだけの純関数である。ロケール・OS には触れない（ARC-007）。Vim ソースは読まず、help（`:help virtcol()` / `:help 'tabstop'` / `:help 'ambiwidth'`）と実測を根拠にする。

## 決定

**表示幅の表と仮想桁の計算を core の純関数 1 本ずつに置き、Vim の桁の規則を使う経路（欲しい列・VISUAL の `.` の桁・矩形）はすべてそれを通す。それ以外の桁（本文の位置・テキストオブジェクト・`f` `t`・`r`・描画）は code point のままにする。**

1. **表示幅**: `core::display_width(char32_t) -> DisplayWidth`（閉じた enum `zero / single / wide / unprintable`・1 ファイル 1 型）。範囲表は `constexpr std::array` 1 ファイル（`DisplayWidthRange.hpp`）で、**固定 Vim 9.1 の `strdisplaywidth()` を U+0000〜U+10FFFF の全 code point で測った値そのもの**を出典にする（491 行）。`wide`（2 桁）は East Asian Width の `W` / `F` に、Vim が `^X` と描く制御文字 `U+0000`〜`U+0008` と `U+000A`〜`U+001F` を足したもの。`A`（曖昧幅）は Vim の既定 `ambiwidth=single` に合わせて `single`。結合文字は `zero`。`U+007F`（DEL）は**実測で 1 桁**なので `single`。Vim が `<200b>` の形で描く書式用文字（`U+200B`〜`U+200F`・`U+FEFF` 等 8 範囲）は**実測で 6 桁**なので `unprintable`。ハングル・絵文字は範囲表に含める（ADR 0031 の語の種類表と同じく「未測」から「表にある」へ）。Tab は桁の位置で決まるので表に載せない。
2. **仮想桁**: `core::virtual_column(text, offset) -> VirtualColumn`（新しい型・1 始まり・`Column` とは別の型で混ぜられない・CPP-001）。行頭から `offset` の文字の**最初の桁**まで数える。Tab は `8 - ((vcol - 1) % 8)` 桁（`tabstop=8` 固定・設定にしない）。`virtual_column_end`（その文字の最後の桁）も同じ関数群に置く。**3 本目の `caret_virtual_column`（Vim がキャレットを描く桁）を足した**: Vim は Tab の上ではキャレットを**最後の**桁に描き、全角・制御文字・書式用文字では**最初の**桁に描く（`getvvcol` の cursor が Tab だけ `vcol + incr - 1` になるのと同じ・実測）。curswant が覚えるのも `.` の再生が起点にするのもこの桁である。逆引き `offset_at_virtual_column(text, line, vcol) -> Offset` は「その桁を含む文字」（Tab や全角の途中の桁はその文字の先頭）で、行がそれより短ければ**行の内容の終わり**（Vim が NUL を置く桁＝従来の `offset_of` と同じ止まり方）。そこから NORMAL で 1 文字戻すのも VISUAL で改行まで選ぶのも呼ぶ側の仕事のまま（ADR 0018 の決定 6）。
3. **欲しい列**: `VimWantedColumn.column` を `VirtualColumn` にする。`j` `k` `Ctrl-d/u/f/b` `PgUp/PgDn` `H M L`（既存の欲しい列を使う経路すべて）は仮想桁で着地する。欲しい列に入れるのは決定 2 の `caret_virtual_column`。`$` の `at_line_end` は変えない。着地は決定 2 の逆引き 1 本。INSERT の矢印は `VimWantedColumn` を通らず `CaretMove` の経路なので範囲外（Vim も INSERT では Tab の最初の桁にキャレットを描くので、合わせるなら別の Issue）。
4. **VISUAL の `.` の桁**: ADR 0033 の `VimCharacterExtent.column` を `VirtualColumn` にする。1 行の記録は「選択が覆う桁の数」＝ `virtual_column_end(末尾) - virtual_column(先頭) + 1`（`ab<Tab>cd` の `vll` は 8）、複数行の記録は最終行の `virtual_column_end`（絶対）。**記録の先頭は「最初の桁」・末尾は「最後の桁」で非対称**で、**選び直しの起点だけが `caret_virtual_column`**（Tab の上なら最後の桁）である。3 つとも実測で決めた（`out/issue108-oracle/probe4.py` / `probe5.py`）。選び直しは決定 2 の逆引き。ADR 0033 の「結果」の Tab / 幅の混在の穴が閉じる。
5. **変えないもの**: `TextPosition.column`・テキストオブジェクト・`f` `F` `t` `T`・`r`・語の移動・検索・描画・`Ctrl+C/X/V`。`|`（桁への移動）は未実装のままで、入れるときは仮想桁。
6. **矩形**: `Ctrl-v` は次の ADR（[ADR 0035](0035-vim-visual-block-as-column-ranges.md)・Issue #112 で受理）。矩形の左右の桁は本 ADR の仮想桁（`virtual_column` / `virtual_column_end`・端に掛かった文字も含む）を使う。本 ADR で `VimMode` や `VimRegisterKind` は増やさない。
7. **oracle**: Tab と全角と結合文字を含む本文で `j` `k` `H M L` と VISUAL の `.` を測り、fixture に採る。`ambiwidth` と `tabstop` は既定固定なので設定は書かない。

## 強制

- 表示幅の表と仮想桁が 1 か所であること: **active**（`--vim-virtual-column` の対象 unit・表の昇順と重なりは `static_assert`・ARC-001 はレビュー）
- 欲しい列・VISUAL の `.` の桁が Vim と一致すること: **active**（oracle fixture・CNF-010）
- ロケール・OS に触れないこと: **active**（`eng/symbols.py`）
- 期待値が本物の Vim の答えであること: **不能**（CI に Vim は無い）

## 結果

得られるもの: `j` `k` と VISUAL の `.` が Tab と全角の混ざる本文で固定 Vim と一致する（`virtcol-` の fixture 47 件）。`Ctrl-v` の矩形がこの上に載る。表示幅の表は core の純関数なので engine は純粋なまま。
失うもの・残る穴: 範囲表は**固定 Vim 9.1 の答え**に固定されているので、Vim を上げると Unicode の版ごとずれうる（差は fixture で見つける）。`tabstop` `ambiwidth` `listchars` の設定は範囲外。INSERT の矢印は仮想桁を通らない。1 打鍵あたり行頭からの走査が増える（行の長さに比例。速さのゲートが見張る。#108 の実測では 1 打鍵 0.936 ms / 16 MiB 270.950 ms で基準値の許容内）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `Column` の意味を仮想桁に変える | 本文の位置・テキストオブジェクト・描画の桁まで変わり、Vim が code point で数える所（`f` `t` `r`）も壊れる。型を分けて経路を閉じるほうが安全 |
| 描画側（DirectWrite）の桁を engine に渡す | engine が UI に依存する（ARC-003 / ARC-004）。Vim の仮想桁は表示と独立した規則 |
| `Ctrl-v` と同じ Issue で行う | 桁の意味の変更は既存の `j` `k` に影響し、fixture の追記と実測が矩形と混ざる。先に閉じる |
| `wcwidth` 相当を OS の API で取る | ロケール依存で core に置けない（ARC-007）。Vim の既定（`ambiwidth=single`）とも一致しない |

## 補足（2026-09-22・実測で直したこと）

実装の前に固定 Vim 9.1 を 5 本の probe で測った（`out/issue108-oracle/probe*.py`。`out/` は追跡外）。決定 4〜7 は提案のまま通り、決定 1・2・3 を次の 4 点で直した。Vim のソースは読まず、`strdisplaywidth()` / `virtcol('.')` / `winsaveview().curswant` と本文の `j` `k` `.` の答えだけを根拠にしている。

1. **`U+007F`（DEL）は 2 桁ではなく 1 桁**だった。提案は「制御文字は `^X` と 2 桁」から `U+007F` も 2 と書いていたが、`encoding=utf-8` の Vim 9.1 では DEL は 1 桁で、`a<DEL>b` の `b` は 3 桁目にある（`probe3` の `del-j-after`）。`U+0000`〜`U+0008` と `U+000A`〜`U+001F` だけが 2 桁。
2. **幅は 3 種類では足りず、6 桁の class があった**。Vim は `U+200B`〜`U+200F`・`U+202A`〜`U+202E`・`U+2060`〜`U+206F`・`U+070F`・`U+180E`・`U+FEFF`・`U+FFF9`〜`U+FFFB`・`U+FFFE`〜`U+FFFF` を `<200b>` の形で描き、これは 6 桁を占める（`probe3` の `zwsp-j-after`: `a<ZWSP>b` の `b` は 8 桁目）。`DisplayWidth` に `unprintable` を足した。
3. **キャレットの桁は「最初の桁」だけでは足りない**。Tab の上に居るときだけ Vim はキャレットを Tab の**最後の**桁に描き、`j` はそこから落ちる（`ab<Tab>cd` で `2lj` は次の行の 8 桁目へ）。全角・制御文字・書式用文字は最初の桁のまま（`probe1` の `tab-j-from-tab` / `emoji-j` / `control-j`）。`caret_virtual_column` を 3 本目の関数として足し、欲しい列と `.` の再生の起点だけがそれを通る。
4. **逆引きの端は「行の内容の終わりの文字」ではなく「行の内容の終わり」**だった。提案の言葉どおりに最後の文字で止めると、短い行へ畳まれた VISUAL の `.` が改行を巻き込まなくなり、Issue #91 で実測済みの振る舞い（`--vim-dot` の unit）が変わる。`Column` で数えていたときの `TextBuffer::offset_of` と同じく、行より右の桁は Vim が NUL を置く桁で止める。

表の出典も変えた。提案は「Unicode の EastAsianWidth の版をコメントに書く」だったが、Vim 9.1 に版を問い合わせる手立てがなく、Unicode の表を写すと Vim との差が黙って入る。全 code point を `strdisplaywidth('a' . nr2char(cp)) - 1` で測り、その値そのものを表にした（単体の `strdisplaywidth(nr2char(cp))` は孤立した結合文字に 1 を返すので、基底文字の後ろで測っている）。`U+0000` だけは `nr2char(0)` が空文字列になって測れないので、同じ `^@` の形で描かれる制御文字に合わせて 2 桁にしてある。
