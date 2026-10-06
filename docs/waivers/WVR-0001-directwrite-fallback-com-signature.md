# WVR-0001: DirectWrite字体選択のCOM署名

- Status: removed
- Rule: CPP-012
- Issue: #291
- Owner: NeNeNibサナ
- Created: 2026-10-05
- Expires: 2026-11-04
- Scope: src/ui/win32/FontFallbackCache.cpp#FontFallbackCache::MapCharacters

## 正確な範囲

readability-function-sizeの引数数だけ。IDWriteFontFallback::MapCharactersを実装する
COM境界の11引数はWindows SDKのABIで固定される。
境界は要求値の構築と委譲だけの4行で、実処理mapは4引数以内の通常検査を受ける。

## 理由

正典Releaseの初回ビルド54403dfで、11 parameters (threshold 4)が唯一のエラーだった。
引数を減らすとoverrideにならず、DirectWriteがこの実装を呼べない。
ゲート設定と他の宣言は変更しない。ADR 0071の字体選択カプセルだけに閉じる。

## リスクと封じ込め

属性の取り違えは字体や文字位置を変える。要求一致条件の検査と、OS標準経路に対する
字形・位置・HitTest・多言語画素比較を行う。境界には条件分岐やキャッシュ処理を置かない。

## 解除条件

この実験を不採用として宣言を削除するか、OS固定ABIの境界だけを扱う恒久規則を別ADRで
受理し、他の関数への検査が維持されることを実証したとき。期限までに解決しなければゲートが落ちる。

## 却下した代替案

引数を構造体へ変更するとCOM契約に一致しない。内部の実処理は構造体で渡す。
ゲート全体の閾値変更・ファイル除外・広い抑制は使わない。

## 閉じた記録

2026-10-06・Issue #299 で removed にした。SDK が固定した COM の署名は期限つきの例外ではなく変わらない事実なので、恒久の規則 CPP-019 と機械の検査 CNF-012（[ADR 0076](../adr/0076-sdk-fixed-com-signatures-are-a-permanent-boundary-rule.md)）に置き換えた。抑制の直前行は `// Waiver: WVR-NNNN` から `// SDK-ABI: <Interface>::<Method>` へ 1 対 1 で置き換え、コードの行は変えていない。この台帳は経緯として残す。
