# ADR 0077 — 描画の保持の契約は窓を作らない試験で守り、字体選択の保持の取り付けは最善の努力にする

- 状態: 受理
- 日付: 2026-10-07
- Issue: #300
- 影響する規則: ARC-002 / ARC-007 / CPP-011 / CPP-019 / QLT-010 / QLT-013 / CNF-012

## 文脈

#291 は字体選択の保持（[ADR 0071](0071-cache-complete-system-font-fallback-requests.md)）と字形の保持（[ADR 0073](0073-visible-body-glyph-capsule.md)）を入れた。
その契約（鍵の一致条件・上限・OS の失敗を覚えないこと・未対応の装飾の棄却・画面外の境界・画素の一致）は、
独立の診断（`/W4` だけの 1 回限りのソース）で確かめただけで、CTest に入っていなかった（独立レビュー L2・L4・O2）。
また本文の書式を作るとき、保持の取り付けのどこかが失敗すると `RenderFailure` で窓が終わっていた（L3）。保持は速さのためだけにあり、
取り付けられなくても OS の既定の字体選択で同じ表示が出る。

## 決定

1. **ui/win32 の描画の保持の契約は、新しい試験の exe 1 つで守る。** 置き場 `tests/ui/`・target `nib_window_tests`・CTest `nib_window`。
   `eng/architecture.json` に module `window_tests`（依存 core / application / ui_win32。application は ui_win32 から推移的に入る）を足し、
   `eng/targets.cmake` は ui / adapters と同じ Win32 の定義を当てる。
2. **持ってよい OS の資源は DirectWrite の factory と、WIC のソフトウェアの描画先まで。** OS ライブラリは ole32・d2d1・dwrite・windowscodecs。
   窓・device・swap chain・GPU・時計・乱数・スレッド・ファイルを使わない。インストール済みのフォントに結果が依る検査は「環境依存」（QLT-013）で、
   無いときは落とさずに `not measured (environment): ...` と 1 行言って数に入れない（終わりの行に数を出す）。
3. **`FontFallbackKey` と `FontFallbackCache` は替え玉で試す。** 本文の source の替え玉 `ScriptedTextSource` と OS の役の替え玉 `ScriptedFontFallback`
   （`MapCharacters` は CPP-019 の印つき・[ADR 0076](0076-sdk-fixed-com-signatures-are-a-permanent-boundary-rule.md)）を 1 ファイル 1 型で置く。
   替え玉の OS は `scale` に何回目の呼び出しかを入れるので、保持から返った答えかどうかを値で見分ける。
4. **`BodyGlyphCollector` は替え玉で呼べる所を直接呼び、実 DirectWrite が要る所を環境依存として試す。** 画面外の境界は本物の face の寸法から同じ式で上界を作って
   左端ちょうど・1 ulp 外・右端 ±1・offset・横向きを見る。画素は 3 family × 3 大きさ × 4 幅 × 5 行 = 180 組で、保持した字形の描画が `DrawTextLayout` と
   1 バイトも違わないことと、保持した字体選択が OS の既定の字体選択と同じ画素になることを見る。
5. **取り付けは最善の努力にする。** `Direct2DRenderer::attach_font_fallback`（private・結果を返さない）が `GetSystemFontFallback` → `As<IDWriteTextFormat2>` →
   `SetFontFallback(Make<FontFallbackCache>)` を今と同じ順で呼び、どこで失敗しても OS の既定の書式のまま続ける。`IDWriteFactory2` の替え玉は作らない。
6. **字形の経路の前提を 1 か所に書く。** 96 DPI・恒等の変換・整数の原点の 3 つを `draw_body_text` のコメントと ADR 0073 に書き、
   試験は `core::reference_dpi == 96` と collector の `GetPixelsPerDip == 1`・恒等の変換を確かめる。collector の答えを同じ定数から出す形にはしていない。
7. **`WM_FONTCHANGE` は扱わない**（ADR 0071 の限界）。

## 強制

- `nib_window`（CTest）: **active**。この機械では 1521 checks・環境依存の測れなかった数 0。
  反例: `FontFallbackKey.cpp` の locale の比較を外すと言語の 2 件が落ち、`BodyGlyphCollector::take` の棄却の印を見る行を外すと 8 件が落ちる（2026-10-07）。
- 設計 5 の失敗の経路は試験が無い（替え玉の factory を作らない）。レビュー事項。

## 結果

- 試験を入れたことで、ADR 0073 の「未対応の callback では収集全体を棄却する」が実 DirectWrite で成り立っていなかったことが分かった。
  `IDWriteTextLayout::Draw` は callback の HRESULT を無視して S_OK を返す。collector の棄却の印で決める形に直した（ADR 0073 の追記）。
- 装飾も effect も無い本文の行（今の本文のすべて）では、呼び出しの列と順と結果は変わらない（画素 180 組が一致し続ける）。

## 限界

- 画素の一致は WIC のソフトウェアの描画先・グレースケールの文字の縁で見ている。製品の GPU の描画先と ClearType の画素は、設計席の実機の確認が受け持つ。
- フォントが無い機械では画素と画面外の境界は数に入らない（黙って通さず 1 行言う）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 契約を adapters の試験や単体テストに入れる | 単体テストは OS 資源を持たない（QLT-013）。adapters の試験は ui/win32 に依存できない（ARC-002） |
| `IDWriteFactory2` や `IDWriteFontFace1` の替え玉を作る | インターフェースが大きく、替え玉が試験の大半になる。face は実物を環境依存で使う |
| 取り付けの失敗で窓を終わらせ続ける | 保持は速さのためだけで、無くても同じ表示が出る |
