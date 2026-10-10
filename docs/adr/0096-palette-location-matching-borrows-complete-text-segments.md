# ADR 0096 — 一覧の場所照合は完結した文字列の区間を借りて読む

- 状態: 技術受理（2026-10-10・固定Count案を代償込みで採用）
- 日付: 2026-10-10
- Issue: #358
- 規則: ARC-001 / ARC-003 / ARC-004 / ARC-005 / ARC-008 / ARC-012 / CPP-003 / CPP-005 / CPP-008 / CPP-012 / CPP-016 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / QLT-014

## 文脈

ADR0091で小文字の写しと同じ候補の再評価を減らした後も、`CommandChoice.cpp::listed_score`は名前に当たらなかった候補ごとに`located_name`でdetail、区切り`\`、labelを所有stringへ連結する。照合に必要なのは同じ文字列順の読み取りとbyte座標であり、追加の所有文字列は外部へ返らない。公開入口の候補防御copyは別の不変性境界なので保つ。

## 決定

1. `CommandChoice.cpp`の私有`match_score`を、`std::span<const std::string_view>`で借りた完結文字列の順序列へ適用する一つのkernelにする。単一の名前・操作名・読み・鍵・Ex候補はstack上の1要素arrayを渡す薄いstring_view overloadから同じkernelへ入る。場所はdetail/区切り/labelの3要素arrayを同期呼出し中だけ借りる。view/arrayを返却値・候補・cacheへ保存しない。public APIや新しいsegmented-text型は追加しない。
2. UTF8 scalarを探す正本は既存の`code_point_from(string_view, Offset, wanted)`の走査とする。区間列用の私有overloadは前の区間のbyte長を加算し、globalなfromを各区間のlocalなfromへ写して、この同じscalar走査へ渡す。既に通過した区間ではfromがsize以上となり既存loopが何も読まない。見つかれば既存`next_code_point`からそのscalarの終端を求め、globalな`OffsetRange{begin,end}`を返す。独自byte分類・decoder・大小変換は追加しない。
3. 各区間はそれ自身で完結したUTF8文字列である。detail/labelは検証済みDisplayText、区切りはASCII一文字、単一区間の他callerは従来の入力である。scalarを区間境界で分割する一般APIは提供しない。Unicode scalarの途中へ区間を切り出す呼出しは作らない。
4. scoreの初期値は区間byte長の合計。queryを既存UTF8原始で読み、ASCII空白だけを無視し、ASCII A〜Zだけを比較時に小文字相当へ写す。見つかったglobal beginと直前global endのgapを使い、直前endが0のときだけ4倍する。次の探索fromはfound.end。同じ連結stringのbyte座標なので、区間を跨ぐgap・最初のgap・多byteの重みを変えない。空白だけのqueryも従来どおり総byte長を返す。
5. `listed_score`は名前を先に照合し、成功ならそのscoreを返す。不一致かつdetail無しなら不一致。detail有りのときだけ3区間を同じkernelへ渡し、成功時は従来のlocation_only_penaltyを加算する。`located_name`を削除する。名前優先、場所罰点、scored_positions、scope、同点時の元entry順、query空の全列順を変更しない。
6. `CommandPalette::opened/extended`の公開防御copy、不変候補所有、前の候補集合を再利用する条件と全件fallbackを変更しない。mutable cache・保持量追加・新しい公開口・renderer/IME変更・schema変更はしない。
7. 既存assertを維持し、公共listed_positions/CommandPalette入口から、場所末尾→区切り→Unicode label、境界を越すbyte gapとscore順、名前優先と元順、空白だけ/空query、detail無し/不一致を確認する。private scorerを試験のため公開せず、候補自身で期待を生成しない。既存の完全絞込と部分対象の比較も再利用する。
8. 固定済みharness53条件から`palette-listed-name-5000`、`palette-listed-location-5000`、既存の末尾絞込5000→50を使う。harness/入力/期待/区間を追加変更しない。通常Releaseの同一harnessで試料数/ABBA/timeoutを事前固定し、全試料と初回失敗・欠測・悪化を保持して採否を決める。名前単一区間のwrapper費用も含むので、場所だけの利益から全般的な改善を仮定しない。

## 対象検証と採否

通常Debug/tidy/ASan/UBSan、直接`--command-palette`/`--operations`、変更sourceの規約/整形と完成core symbols。共通scorerのファイル一覧・操作名/読み/鍵・Ex候補のcallerを選ぶ。Ex候補はcommand-palette内の既存verify_palette_choicesが直接確認する。ex-settingsのCommandLine.completionsはExResult::command_completionsのprefix照合であり、今回のscorerを通らないので実行範囲へ加えない。背景I/O/候補所有/rendererは変えないため全回帰は既定にしない。既存契約に不足する境界だけ追加し、旧assert/selectorを削除しない。

直接GUIでは同じ一覧/検索入力の候補順・表示・通常/場所の跨境界を確認し、正式速度は変更した照合と直接callerが含まれるpalette5000を選ぶ。公開操作一覧/Exは直接scopeと必要な画面刺激で確認する。独立レビュー/通常Release/CIを含め、利益がなければ不採用とする。関連入力不変の成功結果は文書/push/review/merge理由で繰り返さない。基準/許容/fixture/抑制/allowlistは変更しない。

## 限界

区間数は現在の私有callerの1または3に閉じる。queryの各scalarで区間列を辿る固定費用は残り、別の位置索引やcacheを導入しない。所有候補の作成/到着費用、一覧描画、IME、RSS/capacity、全操作の速度は未測。今回の手法を任意に分割したbyte列の汎用decoderとして扱わない。

## 初回の結果と固定区間数の追試設計（11:41 JST）

初回08116c3は直接1011checks/独立レビュー/17画面本文・status0画素差/正式palette中央値1.984ms・0退行・0計測不能。固定3比較（20iterations×3ABBA/120対応組）は、名前332→363.5us・対応比中央値1.089419127、場所1348→963us・0.710424595、末尾絞込38→40us・1.054054054。全試料/遅い組/欠測なしをD原記録へ保持する。正式gate成功だけで名前の悪化を消さず、初回をそのまま採用しない。

次の独立した実装候補では、決定1/2のspanにコンパイル時の区間数を保持する。`code_point_from`区間overloadと`match_score`の一つの本体を`template <std::size_t Count>`にし、引数は`std::span<const std::string_view, Count>`。既存stack arrayから`std::span{parts}`/`std::span{located}`を渡す。現在のcallerはCount1または3であり、この既知の個数を消して実行時の区間数として扱う必要がない。

scalar走査、prefix/local変換、begin/end、score式、所有、全ての関数本体の実行文は維持する。Count専用分岐・template特殊化・別scorer・decoder・cache・flags・harness・閾値を追加しない。1/3のコンパイラ生成実体は一つのtemplateソースから生成し、意味の実装を複製しない。速度効果は未確認の仮説で、初回試料を更新/除去したり同じ実装を繰り返し測ったりしない。

新候補の変更sourceを通常Debug/tidy/ASanUBSanで増分buildし、同じ2scope1011checksを一度実行する。試験内容は意味を変えないため追加しないが、製品sourceが変わるので前の実行結果だけでは代用しない。通常Release/同buildprobe、同じ前後17画面/短い正しさsmoke3、独立差分レビューの後、旧基準fdae17d対新候補で同じ3条件・20iterations×3ABBA/timeout180を一度測る。初回候補の時間値と比較表を残し、新旧を直接対応させていない実験間の改善率は主張しない。正式速度は同じpalette5000の1条件/5sample/既存基準。新候補で利益が無い場合も記録し、閾値や固定入力を変えて合格を取りにいかない。

## 採用結果

最終333086cの固定Count案を採用した。旧版対最終の対応比中央値は名前0.928876560、場所0.657614200、末尾絞込1.054054054。場所は120組すべて短縮し、絞込の37→39µs/約5.4%増加は112組に残る。初回の名前約9%悪化も保持し、全条件改善としない。意味/所有/画像は同一、直接1011checks、独立レビュー、通常Release、正式palette1.852ms/0退行/0計測不能を確認。具体的なコマンド・全範囲・再利用・初回失敗・収載と限界は[gate-proofs 5-di](../quality/gate-proofs.md)を正本とする。
