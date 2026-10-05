# ADR 0071 — 完結した字体選択要求だけを描画器内で再利用する

- 状態: 受理（製品速度と表示の検証は実施中）
- 日付: 2026-10-05
- Issue: #291
- 影響する規則: ARC-001 / ARC-007 / ARC-011 / CPP-011 / CPP-012 / CPP-016 / CPP-017 / QLT-001 / QLT-012

## 根拠と決定

長い日本語行の診断で、1行の文字組みに約16ms、19行の描画呼び出しに約10msを要した。
DirectWriteは同じ短い文字列の字体選択を繰り返す。字体を明示して連結する実験は速いが、
文字の位置に差が生じたため採用しない。システム選択の同じ結果だけを返す試作は、
字形・字形位置・全UTF-16位置のHitTestTextPositionが一致し、約3.3msだった。
試作数値は製品の速度受理を意味しない。

本文書式のIDWriteFontFallbackをFontFallbackCacheが包む。唯一の選択処理は
GetSystemFontFallbackで得たOSのMapCharactersであり、新しい字体選択規則は持たない。
未登録と対象外は同じOS処理へ渡す。レイアウト全文・区切り・Tab・字形・描画は変更しない。

再利用できるのは位置0から256 UTF-16単位以下の完結したanalysis sourceだけである。
要求全体とGetTextAtPositionの長さが等しく、前後に文字がなく、localeとnumber substitution
の範囲が全文を覆う必要がある。拡張sourceは対象外とする。キーは全文・locale・基本字体名・
font collectionとnumber substitutionの所有参照・読み方向・weight/style/stretch。
名前は128単位まで。保持したCOM参照によりポインタ再使用を防ぐ。
S_OKの結果だけを保存し、mappedLength・font（欠字時のnullを含む）・scaleをそのまま返す。

最大128件の固定上限を設け、満杯なら古い枠から置換する。書式を作り直せば新しいcacheを使う。
本文書式がcacheを所有し、layoutが書式由来のfallbackを保持する。別の状態正本やスレッドを作らない。

## 検証

対象はcacheの一致条件・上限・対象外・OS失敗、および実字体を使う多言語・結合・Tab・
双方向・IME・クリック・フォントとサイズ変更。通常Releaseで空文書・短行・長行を比較する。
split採用条件は変えない。現在残る描画費用は別の実験として分離する。

## 参照

- [IDWriteFontFallback::MapCharacters](https://learn.microsoft.com/en-us/windows/win32/api/dwrite_2/nf-dwrite_2-idwritefontfallback-mapcharacters)
- [IDWriteTextAnalysisSource](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwritetextanalysissource)
