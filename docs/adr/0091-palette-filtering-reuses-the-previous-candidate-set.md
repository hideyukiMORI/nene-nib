# ADR 0091 — 一覧の照合は小文字の写しを作らず、前の候補集合を再利用する

- 状態: 受理（設計、採用は固定比較と対象検証後）
- 日付: 2026-10-09
- Issue: #339
- 規則: ARC-001/004/005、CPP-002/003/005/007/016、QLT-001/012/014、ADR0060/0061/0062/0078

## 文脈

原C6/C7の費用はmain eef5aadにも残る。CommandChoiceは候補の名前/場所/操作名をASCII小文字のstringへ写してから照合し、CommandPaletteは入力caretだけの移動でも全候補を照合する。queryの末尾に文字を足しても、前に不一致だった候補を含めて再走査する。候補の不変所有、ファイル結果をentry位置で持つ形、byte単位のscore、名前優先と同点の元順を保って、この重複を減らす。

## 決定

1. `match_score`のquery/candidate比較でASCII A〜Zだけを小文字相当へ写す。候補のowned lowercase文字列を作らない。既存UTF8のコードポイント境界、空白無視、byte長/gap/最初のgap×4、名前優先/場所罰点/元entry位置の順は維持する。場所のdetail+separator+label連結自体は今回残す。Exの候補は固定小文字とThemeNameの小文字規則に従うため、この共通kernel変更で現在のEx照合結果は変わらない。
2. 完全照合と部分対象照合のscore/sortは `CommandChoice.cpp` の同じ私有kernel一つ。既存 `listed_positions(entries, scope, query)` は全entry位置のrangeを渡す薄い口とし、同名の4引数口は借用spanのentry位置を渡す。入力spanは呼出し中だけ読み、保持しない。有効な位置を指定するAPIで、アクセスはentries.atにより検査する。順序は入力spanの並びに依らず元entry位置で同点を決める。CommandPaletteから渡す位置は同じentriesの私有結果なので有効/重複無しを保つ。
3. 入力の更新はCommandPaletteの私有 `refiltered` 一つが判断する。input本文が同じなら、新caretを持つCommandLine、従来どおりselected=0、同じentries/mode/resultで次の値を返す。空挿入や端の削除も同じ。選択だけの上下/クリックは従来のreselectedを維持する。
4. 本文が変わったとき、以前の結果がentry位置列で、palette_query_ofによるscopeが同じ、かつ新queryが旧queryで始まる場合だけ、その位置列を対象に照合する。名前一致から場所だけの一致へ変わる場合があるため、名前/場所/scoreは全て再計算し、新scoreと元entry位置で並べ直す。以前のscoreや並びは流用しない。
5. 上記以外は既存result_ofによる完全照合。削除/途中挿入/scope変更、commands/operationsの所有候補は完全照合へ戻す。openedは全候補、extendedは新しいentriesの全候補を同じfiltered経路で照合し、同一候補の選択を維持する。結果variantの処理は型別overloadとstd::visitで両型を列挙し、catch-allを設けない。entries/modeは同じpalette値の中で固定で、遅延mutable cacheやqueryの別正本を持たない。
6. 部分列照合ではquery末尾延長により一致集合は増えない。以前に名前/場所どちらにも一致しなかったentryが、長いqueryで新たに一致することはない。queryの空白を無視してもこの包含は保たれる。ただしscopeはraw入力のprefixから推測せず、正典のpalette_query_ofの結果を使う。
7. ADR0062の内部観測契約のうち、left/home等の本文不変編集でresultを作り直す期待だけを置き換える。同じresultを共有し、caret/選択reset/候補内容・順序は以前と同じことを確認する。既存のBackspaceによる本文変更、入力追加/filledによる本文変更の再生成は維持。外部の動き、fixtureや照合期待を緩める変更ではない。

## 対象検証と採否

Debug製品/対象単体build、`--command-palette`、`--operations`、`--background-work`。ASCII大小・Unicode境界・byte score・名前から場所への移行・元順・全scope・query空白・途中編集・候補到着・IME確定/取消・旧snapshotを確認する。新しい部分対象結果を、同じ入力を新しくopenedした完全結果と全行で照合する。内部共有期待の変更は決定7の二条件だけとし、旧期待の数/内容を勝手に削らない。

#337の同一harnessで名前/場所5000件、末尾追加5000→50、caret移動の4区間を固定ABBA比較する。全sample/入力・exe hashを保持し、改善しなければ不採用にする。最終候補は正式palette5000benchと対象画面で確認し、独立レビュー/CIを経て統合する。基準値/許容/抑制/allowlist/保存schemaは不変、waiver none。
