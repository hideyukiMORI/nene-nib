# WVR-0002: DirectWrite描画コールバックのCOM署名

- Status: removed
- Rule: CPP-012
- Issue: #291
- Owner: NeNeNibサナ
- Created: 2026-10-05
- Expires: 2026-11-04
- Scope: src/ui/win32/BodyGlyphCollector.cpp#BodyGlyphCollector::DrawGlyphRun, BodyGlyphCollector::DrawUnderline, BodyGlyphCollector::DrawStrikethrough, BodyGlyphCollector::DrawInlineObject

## 範囲と理由

readability-function-sizeの引数数のみ。IDWriteTextRendererのSDK固定署名（5〜7引数）は変更できない。
各境界は5行以内の委譲かE_NOTIMPL返却に限定する。通常関数collect/outsideは4引数以内。

## リスクと検証

DirectWriteからの借用配列はその呼び出し内で所有配列へ複製する。未対応の装飾は収集全体を
失敗させ、元のDrawTextLayoutだけを実行する。多言語・フォント・選択・検索の画素比較で検証する。

## 解除条件

実験の不採用・境界の削除、または固定ABI規則の別ADR受理。ゲート閾値・ファイル除外は変更しない。

## 閉じた記録

2026-10-06・Issue #299 で removed にした。SDK が固定した COM の署名は期限つきの例外ではなく変わらない事実なので、恒久の規則 CPP-019 と機械の検査 CNF-012（[ADR 0076](../adr/0076-sdk-fixed-com-signatures-are-a-permanent-boundary-rule.md)）に置き換えた。抑制の直前行は `// Waiver: WVR-NNNN` から `// SDK-ABI: <Interface>::<Method>` へ 1 対 1 で置き換え、コードの行は変えていない。この台帳は経緯として残す。
