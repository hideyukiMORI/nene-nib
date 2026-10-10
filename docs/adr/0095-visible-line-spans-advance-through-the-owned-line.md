# ADR 0095 — 行の選択と検索の桁は所有済み本文の端点を順に数える

- 状態: 技術受理・#355（2026-10-10）
- 日付: 2026-10-10
- Issue: #355（固定比較harness #356）
- 規則: ARC-001 / ARC-003 / ARC-004 / ARC-005 / ARC-008 / ARC-011 / ARC-012 / CPP-003 / CPP-005 / CPP-008 / CPP-012 / CPP-014 / CPP-016 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / QLT-014

## 文脈

`EditorController::line_view`は本文を`LineView::text`へ所有した後も、一致ごとに`span_of`から`TextBuffer::position_of`を二度呼ぶ。多数の一致の各端点で同じ行頭prefixを数え直す。選択と一致は同じ原文codepointの桁でなければならない（ADR0018/0037）。coreの文書全体の位置API、表示置換の桁対応、rendererのUTF16位置はそれぞれ別の境界である。

## 決定

1. `application/LineSpanEvaluation.hpp/cpp`に行一回の桁投影を持つprivate補助型一つを置く。constructorと入口/状態はprivate、`EditorController`だけをfriendにする。新しいpublic位置API、テスト専用の公開口、共有cache、EditorStateのフィールドを増やさない。
2. 行の所有stringを`LineView`へmoveした**後**、`view.text`を借用する。行頭の文書絶対byteと既存`line_terminator_end`を渡す。内容末尾は行頭+借用本文のsize。`line_text`が既存`line_end`で切り出した範囲を使い、CRLF/LF/末尾単独CRを再判定しない。借用はこの`line_view`の終了までで、戻す値に保存しない。
3. `span(range)`が旧`span_of`の範囲clip/absent/改行を含む+1桁を一つに所有する。begin=max(range.begin,line_start)、end=min(range.end,terminator_end)、begin>=endならabsent。beginを内容末尾へ丸めた桁と、endが内容末尾を越す場合は内容末尾の桁+1、そうでなければendの桁を返す。
4. private `column_at`は前回の行内byte cursorと1始まりColumnだけを持つ。端点がcursor以降ならdelta substringを既存`core::code_point_count`で数え、前へ戻るならcursor=0/column=1へ戻してから同じ経路で数える。独自UTF8 decoderやbyte分類を追加しない。文書全体の位置変換も同じ計数原始を使い、行内投影は行番号探索をやり直さない。
5. 選択を最初に投影し、その後に既存の非重複`vim_line_matches`列を同じ補助型で投影する。選択が行の後方まで進む場合、最初の一致でresetする。その後の端点は単調なので各prefixを繰り返し数えない。0長の一致はこれまでどおりabsentで落とし、current_matchのキャレット包含条件は変えない。
6. 旧`span_of`を除き、通常選択/VISUAL/矩形選択と検索は新しい一つの桁投影を使う。`display_line`/LineViewの公開値/検索の構文と照合/rendererは変更しない。原文のTab・補助平面は各1 codepointで、制御文字の表示幅への変換は既存DisplayLineが担当する。
7. 追加の配列確保はなく、借用viewと固定個数の位置だけを保持する。選択の後戻りを含めても行本文の計数は高々選択1往路と検索1往路。ただしframe全体のparse/一致列挙/表示生成/文書行境界探索の費用は残る。全体をO(1)と呼ばない。実容量/RSSは別に測らなければ未測定とする。

## 検証と採否

- 既存の通常選択/PlaceCaret/取消/基本frame/mode選択のassertを薄い`--frame-selection`へ束ね、既定applicationからも一度だけ実行する。新規契約も既定で実行し、既存期待を弱めない。
- 主な退行は選択後に最初の検索一致が前へ戻る場合、行をまたぐ選択の+1、空範囲/空行、CRLF/LF/末尾CR、全角/補助平面/Tab/制御文字の桁。公共controller入口から確認し、private helperを試験のため公開しない。入力前提は既存の検証済み本文で、未検証byteの新API契約を作らない。
- 直接scopeはframe-selection、vim-search-highlight、vim-search-incremental、vim-visual-block。選択の投影、強調とcurrent/preview、矩形行ごとのclipを確認する。照合器/UTF8/rendererは変更せず、無関係な全application/全fixtureは既定で回さない。
- #356で先に固定する5条件はdense ASCII、dense混合、sparse末尾一致、通常選択だけ、dense+VISUAL。各区間は公開`frame()`1回で準備/照合/破棄は外。前後とも同じmain採用済みmatcherとharnessを使う。新候補から期待を生成しない。
- 通常toolchain/tidy/ASan/UBSan、対象規約/完成library symbols、独立レビュー、直接GUI、影響する正式速度、CIで採否を判断する。正式条件も範囲の理由と入力を事前に記録する。失敗/欠測/遅い条件を保存し、成功まで再試行しない。関連入力不変の成功は工程が変わっても再利用する。

## 対象外と限界

rendererのUTF16換算、glyph/fallback/tint、解析済みpatternのcache、後方行の索引、永続状態、共有する追加の位置表は別の課題。選択と一致が互い違いに任意順で渡る一般APIの最適計算量は提供せず、現在の呼出し順に閉じる。実測と採否は次節に記録する。

## 2026-10-10 技術受理

通常Debug直接1385checks、前後GUI19場面0画素差、独立製品レビュー、通常Release、正式編集4条件0退行/0計測不能を確認。固定frame5条件各120対応組は全観測し、dense ASCII/mixed/VISUALの対応比中央値は約0.0083/0.0090/0.0084、疎末尾と通常選択は約0.960/0.971。dense3条件は全組短縮、疎/通常には遅い試料もあり全記録を保持した。所有・意味・計数原始を一つに保ち、行内の重複prefix計数を除く効果を根拠に採用する。RSS/renderer/入力応答全体の改善量は未測。初回比較器の複雑度診断、GUI刺激同期失敗も保存。詳細/再利用/収載/CIの正本は[gate-proofs 5-dh](../quality/gate-proofs.md)。
