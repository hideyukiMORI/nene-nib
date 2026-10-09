# ADR 0091 — 一覧の照合は小文字の写しを作らず、前の候補集合を再利用する

- 状態: 受理・#341で製品採用
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
8. **候補の不変性は公開入口の防御コピーで成立させる。** `opened`と`extended`の生の候補列は`const std::vector<CommandChoice>&`で借り、呼出し中に一度だけコピーして私有所有する。rvalueで渡されても生の配列/要素文字列をmoveで受理しない。開くときはshared const vectorへコピーし、追加到着は私有grownへ旧列と新列をコピーする。私有のgrown/resultは外へ可変参照を出さず従来どおり内部moveできる。生列をby-valueで受けた後さらにコピーする二重境界は作らない。外部APIのsignature変更はこの二口だけで、呼び出しの引数式と返却値の契約は保つ。

独立レビューで、旧openedのvector move前の要素pointerやextendedの長いcommand/keyのdata pointerが、移管後の候補を変更できると判明した。以前にもsnapshot不変条件を破る穴で、今回のsubsetはさらに「前の不一致は後も対象外」という判断へ影響する。CPP-003/ADR0080に従って入口を保護し、規則や入力集合の約束を緩めない。元rawと保持済みaliasを変更してもopened/extendedと後続の絞込結果が変わらない反例を追加する。候補作成/到着でコピー一回の費用は必要条件として残し、毎入力のコピーには戻さない。

## 対象検証と採否

Debug製品/対象単体build、`--command-palette`、`--operations`、`--background-work`。ASCII大小・Unicode境界・byte score・名前から場所への移行・元順・全scope・query空白・途中編集・候補到着・IME確定/取消・旧snapshotを確認する。新しい部分対象結果を、同じ入力を新しくopenedした完全結果と全行で照合する。内部共有期待の変更は決定7の二条件だけとし、旧期待の数/内容を勝手に削らない。

#337の同一harnessで名前/場所5000件、末尾追加5000→50、caret移動の4区間を固定ABBA比較する。全sample/入力・exe hashを保持し、改善しなければ不採用にする。最終候補は正式palette5000benchと対象画面で確認し、独立レビュー/CIを経て統合する。基準値/許容/抑制/allowlist/保存schemaは不変、waiver none。

2026-10-10採否: 名前・場所・末尾絞込の固定3区間で短縮を観測し採用した。caretはafterの20試料中10が0usで比較器が拒否したため倍率未算出。本文不変時に同じ結果を共有する契約として採用し、速度量は主張しない。公開入口の防御copyは固定区間外なので、候補作成/到着全体の改善は未確認。対象480/516/89 checks、統合25場面の0画素差、正式8本の0退行を確認。数値と再利用の根拠は[gate-proofs 5-db/5-dc](../quality/gate-proofs.md)。
