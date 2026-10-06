# ADR 0078 — 操作は core の 1 つの表に持ち、鍵と F1 の一覧が同じ 1 本の道で実行する

- 状態: 受理
- 日付: 2026-10-06
- Issue: #303
- 影響する規則: FR-006 / FR-018 / D5 / D10 / D12 / D29 / D43 / ARC-001 / ARC-004 / ARC-012 / CPP-002 / CPP-012 / ADR 0056（決定 10）/ ADR 0060 / ADR 0063 / ADR 0064

## 文脈

施主は 2026-10-06 に、F1 で開くヘルプを「操作の一覧」に決めた（D43・[採用案](../design/2026-10-06-guide.md)）。Ctrl+P と同じ面に操作の名前・短い説明・鍵が並び、打って絞れて、Enter でその場で実行する。

いまのコードには、そのための正本が無い（`out/reports/probe-commands.md`）。

- 鍵 → 操作は、core の 2 つの表（`TabKeyTable`・`BookmarkKey`）と、ui の `press_control_key` の `switch` と `FontShortcut` に分かれている。「Ctrl+Shift+S」のような表示名はどこにも無い。
- 面の候補が運べるのは Ex の文字列かファイルのパスだけで、操作そのものを運ぶ型が無い。
- OS のダイアログ（開く・名前を付けて保存）と「保存しますか」は ui の関数に閉じていて、application から頼む道は `close_request` の「1 意図だけ立つ frame の欄」の形だけ。

一覧のための表を別に足すと、鍵の割り当てと一覧の表示が 2 本の正本になり、片方だけ直して食い違う（ARC-001）。

## 決定

1. **操作は core の閉じた enum `EditorOperation`。** 最初の値は、ファイルを開く・保存・名前を付けて保存・新しいタブ・タブを閉じる・前に使ったタブへ・その逆向き・ファイルとタブの一覧・操作の一覧・ブックマークに付ける / 外す・元に戻す・やり直す・文字を大きく・小さく・戻す・通常 / Vim の切り替え。操作を足すのはここに 1 値で、落ちた `switch` を直す（CPP-002）。
2. **鍵は core の値 `KeyChord`（Ctrl・Shift・`OperationKey`）。** `OperationKey` は使う鍵だけの閉じた enum（文字の鍵・F1・F4・Tab・`+`・`-`・`0`）。仮想キー → `KeyChord` の写しは ui の 1 つの関数で、主キーとテンキーの `+` `-` `0` は同じ値にする。
3. **割り当ての表は 1 つ。** core の `operation_bindings` は「操作・効くモード（通常 / Vim / 両方）・鍵（無くてもよい）」の行の `constexpr` の列。読む口は 3 つ: 鍵とモード → 操作（`operation_for`）、操作とモード → 見せる鍵（その操作の、そのモードで効く最初の行）、操作がそのモードで使えるか（行が 1 つでもあるか）。鍵の無い行は「一覧からだけ実行できる」操作で、通常 / Vim の切り替えがそれ（鍵は D10 で未定）。今の ui が Shift を見ていなかった鍵は、Shift つきの行を表に明示して振る舞いを保つ。
4. **いまある 2 つの表は、この表を読む口になる。** `tab_command_for(TabKey, EditMode)` と `toggles_bookmark(BookmarkKey, EditMode)` は署名を変えずに、`operation_bindings` を引いて答える。「Ctrl+W は通常モードでだけ閉じる」（ADR 0056 決定 10）・「ブックマークは通常 Ctrl+D / Vim Ctrl+Shift+D」（ADR 0063）の正本は表の行に移る。既存の試験と契約はそのまま通ること。
5. **操作の名前・読み・短い説明は core の 1 つの表 `operation_texts`。** 読みはひらがなで、絞り込みにだけ使う（「ほぞん」→ 保存）。鍵の表示名（`Ctrl+Shift+S`）は `KeyChord` から作る関数 1 つで、文字列を表に書かない。
6. **一覧は面の出どころの 1 つ。** `PaletteScope` に値を 1 つ足し、記号の表 `palette_marks` に `?` の行を足す（案内の文字列も同じ表から出る）。候補は core の関数が「操作の表のうち、いまのモードで使える操作」から作り、絞り込みは名前・読み・鍵の表示名に `match_score` を当てる（説明は照合しない）。入力が空なら表の順、入力があれば当たりの良い順（同点は表の順）で並べる。`std::stable_sort` は使わない（ARC-003・`listed_positions` と同じ書き方）。`CommandChoice` に「操作」と「鍵の表示名」の欄を足し、`CommandChoiceKind` に値を 1 つ足す。
7. **実行の道は 1 本: ui の `run_operation(EditorOperation)`。** 鍵で押したときも一覧で Enter を押したときも、この関数が今までと同じ意図（`NewTab` `HistoryAction` `AdjustFontSize` `ToggleBookmark` `OpenCommandPalette` `SelectEditMode` …）を送り、OS のダイアログと「保存しますか」は今の ui の関数（`open_document` `save_document` `save_document_as` `close_tab`）を呼ぶ。controller に操作ごとの実行の分岐を書かない。
8. **一覧から ui へ渡すのは、1 意図だけ立つ frame の欄。** 一覧で操作を選ぶと、controller は面を閉じて、状態に「頼まれた操作」を 1 つ立てる（`close_request` と同じ形・`begin_intent` が次の意図の頭で消す）。ui は `deliver` の終わりにそれを読んで `run_operation` を呼ぶ。`run_operation` の中の `send` はもう頼みが空なので、再入は 1 段で止まる。
9. **F1 は意図 `OpenOperationList`。** controller は `:ls` と同じ口（`run_palette_request`）で、入力が `?` の面を開く。通常モードでも Vim のどのモードでも、入力行や面が開いていても効く（開いていれば `?` の面に置き換える）。IME の変換中は送らない。この意図は `OpenCommandPalette` `OpenTabList` と同じ側（Ctrl+Tab の歩きを確定し、1 行の知らせを消す）。
10. **「前に使ったタブへ」は、Ctrl を押しているかで確定が変わる。** `run_operation` は `WalkRecentTab` を送り、Ctrl が押されていなければ続けて `SettleRecentTab` を送る。鍵（Ctrl+Tab）では今までどおり Ctrl を離したときに確定し、一覧からは 1 歩で確定する。
11. **一覧に載る操作は、いまのモードで使えるものだけ。** 元に戻す・やり直す（Ctrl+Z / Ctrl+Y）は通常モードの行だけを持つので、Vim の一覧には出ない（Vim では `u` と Ctrl-r）。タブを閉じるの鍵は、通常モードでは `Ctrl+W`、Vim では `Ctrl+F4` と見える。
12. **行の右端は鍵。** 面の行の右端の欄（112 DIP）に、鍵の表示名を等幅 12 DIP・角丸 5 の枠（`panel_border`）と面（本文の地）で描く。鍵の無い操作は何も描かない。枠の幅は文字の幅を測って決め、右寄せの書式で測らない（#258 の二重の右寄せを繰り返さない）。この描画は保持しない（ADR 0069 / 0070 の保持に入れない）。脚の「Enter 決定」は今の文字のまま変えない。
13. **この Issue で表に移す鍵は、一覧に載せる操作の分だけ。** Ctrl+O・Ctrl+S・Ctrl+Shift+S・Ctrl+P・Ctrl+Z・Ctrl+Y・文字の大きさの 3 つ・F1 は、ui が `KeyChord` → `operation_for` → `run_operation` で受ける（`press_control_key` の該当の `case` と `FontShortcut` の写しは消す）。タブとブックマークの鍵は決定 4 の口のまま（ui の経路は変えない）。コピー・切り取り・貼り付け・すべて選択・Vim の鍵は今のまま。

## 工程

| 工程 | 範囲 | 受け入れ |
| --- | --- | --- |
| 1 | core: `EditorOperation` `OperationKey` `KeyChord`・`operation_bindings`・`operation_texts`・鍵の表示名・決定 4 の読み替え・`?` の記号・一覧の候補の関数 | 既存の `--tabs` `--bookmarks` `--command-palette` の scope が通る。新しい scope で表の全行（モードごとの鍵・使える操作・表示名・読みの絞り込み）を確かめる |
| 2 | application: `CommandChoice` の欄と種類・面の `?`・意図 `OpenOperationList`・頼まれた操作の欄 | 契約: 一覧で選ぶと面が閉じて頼みが 1 意図だけ立つ・次の意図で消える・`OpenOperationList` が歩きを確定し知らせを消す |
| 3 | ui: `KeyChord` の写し・F1・`run_operation`・決定 13 の鍵の移し替え・`deliver` の終わりで頼みを読む・行の右端の鍵の描画 | Debug / Release のビルド。実機の画と鍵の確認は設計席 |
| 4 | 検証の記録・PR の本文 | — |

## 判定の限界

- 「一覧からの実行が鍵と同じ」は、ui の関数が 1 つであることで保つ。ui の試験は無いので、実機で操作ごとに確かめる（QLT-013）。
- 表に移すのは決定 13 の鍵だけ。ui に残る鍵（コピーなど）を一覧に足すときは、その鍵も表に移す。

## 却下

| 選択肢 | 却下の理由 |
| --- | --- |
| 一覧のための表を別に持ち、鍵の割り当ては今のまま | 鍵と表示が 2 本の正本になる。片方だけ直して食い違う（ARC-001） |
| controller が操作ごとに意図を実行し、ダイアログの要る操作だけ ui に頼む | 実行の道が 2 本になる。鍵の道（ui）と一覧の道（controller）で同じ操作の中身を 2 回書く |
| 一覧の候補を Ex の文字列で運ぶ（`:w` など） | Ex の `:w` は無題で E32 を返し、ダイアログを出さない（ADR 0066 決定 4）。鍵の Ctrl+S と結果が違う |
| ui の全部の鍵をこの Issue で表に移す | Vim の鍵・矢印・コピーの経路まで動かすと、確かめる範囲が一覧の範囲を越える。一覧に載せる分から移す |
| 読みを持たず、確定した漢字だけで絞る | 「保存」を探すのに変換が要る。操作は十数個で、読みの欄は安い |
| 早見表の 1 枚・ヘルプの文書をタブで開く | 施主が採らなかった（D43） |
