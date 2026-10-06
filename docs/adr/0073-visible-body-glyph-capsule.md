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

## 2026-10-05の検証結果

対象の表示/IME/契約/通常Release比較を完了した。実装の高速化は確認できたが、長行の前後半安定条件で性能の総合受理とmain統合は保留。[gate-proofs 5-cm](../quality/gate-proofs.md#5-cm--字体選択可視字形表示幅とsplit再評価issue-291adr-007100730074)に成功・失敗・再利用・未確認範囲を記録した。

## 2026-10-07の追記（Issue #300・ADR 0077）

`IDWriteTextLayout::Draw` は callback の HRESULT を無視して S_OK を返す（2026-10-07・`nib_window` の試験で実測。下線・取り消し線・inline object・effect つきの run のどれでも S_OK）。
そのため「未対応のコールバックでは収集全体を棄却」は `Draw` の戻り値では決まっていなかった。棄却は collector の印で決める。
未対応の callback と effect つきの run と収集の失敗は印を立て、`BodyGlyphCollector::take` は印が立っていたら部分の字形を渡さず nullopt を返し、
`Direct2DRenderer::layout_of` はその行を `DrawTextLayout` で描く。字形で描けるかを決めるのは `take` の 1 か所。
装飾も effect も無い行（今の本文のすべて）の呼び出しの列・順・結果は変わらない。

字形の経路と `DrawTextLayout` の画素が一致する前提は、描画先が 96 DPI（`core::reference_dpi`）・変換が恒等・行の原点が整数の 3 つ。
collector の `GetPixelsPerDip` は 1、`GetCurrentTransform` は恒等を返す。前提は `draw_body_text` のコメントに書き、`tests/ui` が 96 と collector の値を確かめる。
