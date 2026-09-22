# ADR 0034 — Vim の桁は core の表示幅の表 1 本で仮想桁に直し、使う経路を閉じる

- 状態: 提案（実測で決定 1〜7 を確認したら受理へ更新する）
- 日付: 2026-09-22
- Issue: #108
- 影響する規則: FR-003 / ARC-001 / ARC-003 / ARC-007 / CPP-001 / CPP-002 / CPP-004 / CPP-011 / CPP-012 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / CNF-010

## 文脈

engine の桁は `Column`（code point・1 始まり）で一貫している（ADR 0009 / 0012）。固定 Vim 9.1 は `j` `k` の欲しい列（`curswant`）・VISUAL の変更の `.` の再生（ADR 0033）・`Ctrl-v` の矩形を**仮想桁**（`virtcol`。Tab は次の 8 の倍数まで、全角は 2 桁）で数える。ADR 0012 の「文脈」はこの差を「矩形の縦切りで `virtcol` に直す」と予告し、ADR 0033 は Tab と幅の混在を fixture に採らず engine の答えを unit で固定した（Issue #91 の実測: `ab<Tab>cd` の `vll` の記録は Vim では次の行から 8 文字消す。`abc` の記録はひらがなの行で 2 文字）。

いまの fixture で Tab か全角を含む本文に `j` `k` `H M L` を打つものは 9 件だけで、どれも桁が一致する入力（`visual-wanted-unicode` 等）に限られている。桁の意味を変えるのは engine 全体に関わるので、`Ctrl-v` の機能とは別の Issue で先に行い、`Ctrl-v`（次の ADR）はこの仮想桁の上に載せる。

表示幅は描画の桁（DirectWrite が決める画素）とは別物で、Vim の意味論のためだけの純関数である。ロケール・OS には触れない（ARC-007）。Vim ソースは読まず、help（`:help virtcol()` / `:help 'tabstop'` / `:help 'ambiwidth'`）と実測を根拠にする。

## 決定

**表示幅の表と仮想桁の計算を core の純関数 1 本ずつに置き、Vim の桁の規則を使う経路（欲しい列・VISUAL の `.` の桁・矩形）はすべてそれを通す。それ以外の桁（本文の位置・テキストオブジェクト・`f` `t`・`r`・描画）は code point のままにする。**

1. **表示幅**: `core::display_width(char32_t) -> DisplayWidth`（閉じた enum `zero / single / wide`・1 ファイル 1 型）。`wide` は Unicode の East Asian Width が `W` か `F` の範囲（`constexpr` の範囲表・1 ファイル・出典と Unicode の版をコメントに書く）。`A`（曖昧幅）は Vim の既定 `ambiwidth=single` に合わせて `single`。結合文字（`Mn` / `Me` / `Mc` の主な範囲）は `zero`。制御文字（`U+0000`〜`U+001F` と `U+007F`）は Vim が `^X` と 2 桁で描くので `wide` と同じ 2（Tab は下の規則で別扱い）。ハングル・絵文字は範囲表に含める（ADR 0031 の語の種類表と同じく「未測」から「表にある」へ）。
2. **仮想桁**: `core::virtual_column(text, offset) -> VirtualColumn`（新しい型・1 始まり・`Column` とは別の型で混ぜられない・CPP-001）。行頭から `offset` の文字の**最初の桁**まで数える。Tab は `8 - ((vcol - 1) % 8)` 桁（`tabstop=8` 固定・設定にしない）。`virtual_column_end`（その文字の最後の桁）も同じ関数群に置く。逆引き `offset_at_virtual_column(text, line, vcol) -> Offset` は「その桁を含む文字」（Tab や全角の途中の桁はその文字の先頭）で、行がそれより短ければ行の内容の終わりの文字。
3. **欲しい列**: `VimWantedColumn.column` を `VirtualColumn` にする。`j` `k` `Ctrl-d/u/f/b` `PgUp/PgDn` `H M L`（既存の欲しい列を使う経路すべて）は仮想桁で着地する。`$` の `at_line_end` は変えない。着地は決定 2 の逆引き 1 本。
4. **VISUAL の `.` の桁**: ADR 0033 の `VimCharacterExtent.column` を `VirtualColumn` にする。1 行の記録は「選択が覆う桁の数」（`ab<Tab>cd` の `vll` は 8）、複数行の記録は最終行の絶対仮想桁。選び直しは決定 2 の逆引き。ADR 0033 の「結果」の Tab / 幅の混在の穴が閉じる。
5. **変えないもの**: `TextPosition.column`・テキストオブジェクト・`f` `F` `t` `T`・`r`・語の移動・検索・描画・`Ctrl+C/X/V`。`|`（桁への移動）は未実装のままで、入れるときは仮想桁。
6. **矩形**: `Ctrl-v` は次の ADR。矩形の左右の桁は本 ADR の仮想桁を使う。本 ADR で `VimMode` や `VimRegisterKind` は増やさない。
7. **oracle**: Tab と全角と結合文字を含む本文で `j` `k` `H M L` と VISUAL の `.` を測り、fixture に採る。`ambiwidth` と `tabstop` は既定固定なので設定は書かない。

## 強制

- 表示幅の表と仮想桁が 1 か所であること: **active**（`display_width` / `virtual_column` の対象 unit・ARC-001 はレビュー）
- 欲しい列・VISUAL の `.` の桁が Vim と一致すること: **active**（oracle fixture・CNF-010）
- ロケール・OS に触れないこと: **active**（`eng/symbols.py`）
- 期待値が本物の Vim の答えであること: **不能**（CI に Vim は無い）

## 結果

得られるもの: `j` `k` と VISUAL の `.` が Tab と全角の混ざる本文で固定 Vim と一致する。`Ctrl-v` の矩形がこの上に載る。表示幅の表は core の純関数なので engine は純粋なまま。
失うもの・残る穴: 範囲表は Unicode の版に固定され、Vim の版とずれうる（差は fixture で見つける）。結合文字の幅 0 は主な範囲だけ。`tabstop` `ambiwidth` の設定は範囲外。1 打鍵あたり行頭からの走査が増える（行の長さに比例。速さのゲートが見張る）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `Column` の意味を仮想桁に変える | 本文の位置・テキストオブジェクト・描画の桁まで変わり、Vim が code point で数える所（`f` `t` `r`）も壊れる。型を分けて経路を閉じるほうが安全 |
| 描画側（DirectWrite）の桁を engine に渡す | engine が UI に依存する（ARC-003 / ARC-004）。Vim の仮想桁は表示と独立した規則 |
| `Ctrl-v` と同じ Issue で行う | 桁の意味の変更は既存の `j` `k` に影響し、fixture の追記と実測が矩形と混ざる。先に閉じる |
| `wcwidth` 相当を OS の API で取る | ロケール依存で core に置けない（ARC-007）。Vim の既定（`ambiwidth=single`）とも一致しない |
