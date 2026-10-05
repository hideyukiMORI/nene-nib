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
