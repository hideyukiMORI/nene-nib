# ADR 0073: 本文の字形結果を保持し画面外の描画だけを省く

- Status: accepted
- Date: 2026-10-05
- Issue: #291

## 決定

長い日本語行のフォールバック区切りと字形・文字位置はDirectWriteの結果をそのまま保持する。
本文レイアウト作成時にIDWriteTextRendererで字形を収集し、同じ字形・オフセット・測定方式で
DrawGlyphRunへ渡す。書式・文字列・幅の寿命はADR 0069の所有単位に一致させる。
フォント全体のglyphBox、全advanceの絶対値、最大advanceOffsetと2pxを含む保守的な水平上界で
完全に画面外だと分かるrunだけ捨てる。横向き字形や取得不能は捨てない。元の文字組みとHitTestは残す。
描画は整数原点、96dpi、恒等変換に固定した既存コンテキストを前提とする。
装飾・inline・effectなど未対応のコールバックでは収集全体を棄却し元のDrawTextLayoutで描く。
WVR-0002はSDK固定ABIだけに限定する。性能・画素一致の結果は実測後に収載する。
保持の run は文字の位置・cluster・advance も持ち、置き換えの描画は本文と同じ鍵の entry を引く（#531）。

## 2026-10-05の検証結果

対象の表示/IME/契約/通常Release比較を完了した。実装の高速化は確認できたが、長行の前後半安定条件で性能の総合受理とmain統合は保留。[gate-proofs 5-cm](../quality/history/5-cm-font-glyph-width-split-reeval.md)に成功・失敗・再利用・未確認範囲を記録した。

## 2026-10-07の追記（Issue #300・ADR 0077）

`IDWriteTextLayout::Draw` は callback の HRESULT を無視して S_OK を返す（2026-10-07・`nib_window` の試験で実測。下線・取り消し線・inline object・effect つきの run のどれでも S_OK）。
そのため「未対応のコールバックでは収集全体を棄却」は `Draw` の戻り値では決まっていなかった。棄却は collector の印で決める。
未対応の callback と effect つきの run と収集の失敗は印を立て、`BodyGlyphCollector::take` は印が立っていたら部分の字形を渡さず nullopt を返し、
`Direct2DRenderer::layout_of` はその行を `DrawTextLayout` で描く。字形で描けるかを決めるのは `take` の 1 か所。
装飾も effect も無い行（今の本文のすべて）の呼び出しの列・順・結果は変わらない。

字形の経路と `DrawTextLayout` の画素が一致する前提は、描画先が 96 DPI（`core::reference_dpi`）・変換が恒等・行の原点が整数の 3 つ。
collector の `GetPixelsPerDip` は 1、`GetCurrentTransform` は恒等を返す。前提は `draw_body_text` のコメントに書き、`tests/ui` が 96 と collector の値を確かめる。

## 2026-10-16の補足（Issue #586 の工程 2・ADR 0142）

横に画面と交わる run は字形の配列（字形・送り・offset・文字の位置・cluster map）をそのまま持ち、描くときに窓 `[first, last)` だけを渡す。
窓は字形ごとに送りの区間 [x, x + advance] をフォント全体の glyphBox の大きい方・run の最大 advanceOffset・2px だけ広げ、
画面の左端より左で終わる先頭の字形と右端より右で始まる末尾の字形を除いた区間（境界に掛かる字形は残す・run の単位の上界と同じ保守の形）。
窓の先頭の原点は、描画の原点（描く位置 + run の origin）から窓の前の送りを左から float で積んで決める。Direct2D が run の中で積むのと同じ順なので、画素は全体を描いたときと同じになる
（collector の座標で送りの和を先に足して origin をずらす形は、和の結合が違って遠い横送りで画素が変わったので採らない・#586 の画素の契約）。
描くのは `BodyGlyphRun.hpp` の `drawn_glyphs` と `drawn_origin` の 1 対だけで、製品の描画と試験の描画先が同じものを使う。
窓を狭めるのは横だけで、縦は run の単位のまま（#528）。双方向で右から左の run・横向きの run・1 字形の run は窓が全体のまま。
置き換えの描画（#538）が引く文字の位置と送りは run の全体のままで変わらない。
