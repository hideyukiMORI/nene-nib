# ADR 0092 — CP932変換は上限内の出力を一回で書く

- 状態: 受理・#341で製品採用
- 日付: 2026-10-09
- Issue: #340
- 影響する規則: ARC-003 / ARC-007 / ARC-010 / CPP-003 / CPP-005 / CPP-007 / CPP-016 / QLT-001 / QLT-012 / QLT-014

## 文脈と根拠

CP932の復号はOSで必要長を調べ、wideをゼロ埋めしてから再びOSで書いていた。Aの成功owned値moveとBのUTF8出力予約とは別の費用として比較する。

Microsoftの[Windowsコードページ一覧](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-ucoderef/28fefe92-d66c-4b03-90a9-97b473223d43)は932をANSI/OEMの区画に置く。[MS-UCODEREFの変換疑似コード](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-ucoderef/5d543f48-e18b-4828-91d4-69b1488748cf)では、有効な一byteまたは二byteの文字をそれぞれ一UTF16単位へ写し、surrogate pairは扱わない。そこから、CP932でMB_COMPOSITEを使わない現在の変換に限り、入力byte数が出力UTF16単位数の上限となると推論する。全codepageへ一般化しない。

[MultiByteToWideCharのAPI契約](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar)では正の入力長をbyteで、出力容量をUTF16単位で指定し、成功は書いた単位数、失敗は0を返す。MB_ERR_INVALID_CHARSを保ち、MB_COMPOSITEによる出力拡張は使わない。

## 決定

1. `widen_cp932`だけを変更する。空は空成功。非空入力のsizeがint最大を越える場合はcast前に既存undecodableを返す。
2. private ownedのwstringを`resize_and_overwrite(text.size(), callback)`で確保し、OSを一回呼ぶ。callbackはnoexcept。入力bytesと出力容量は同じ検査済の正intを明示し、callbackが渡す容量をOSに渡さない。
3. 書いた数が正で入力bytes以下の時だけ、その初期化済prefixの長さを返す。それ以外は長さ0とし、callback外でundecodableへ写す。未初期化の末尾を読み出さず、失敗を空成功にしない。
4. 公開port/enum、CP932の表、入力viewの寿命、64MiB読込上限、逆変換は不変。別経路・fallback・再試行は置かない。外部の防御コピー境界も変えない（ADR0010/0080）。Waivers: none。

## 検証と採否

実adapterの共通code-pages契約を対象Debugで実行し、空、日本語、ASCII、半角かな、埋込みNUL、不正pairと末尾lead、長短・失敗を挟む結果の独立を確認する。int上限は数GiB確保や偽viewを作らず、比較式がcastを支配することを静的レビューする。conformance/formatを確認し、core/applicationに不変なBのsymbols結果を再利用する。

A、A+B、A+B+Cを別commit・同じharnessで比較し、全結果を残す。DBCS入力のwide容量上限は実使用の最大二倍となり、sizeを実writtenへ縮めてもcapacityの縮小は強制しない。性能がこのメモリ費用に見合わなければCを不採用にできる。
逆方向from_utf8の既存int narrowingと二回目OS戻り値未確認はこの決定の外に残す。入力上限の拒否もCの採否に含み、C不採用時に直ったと報告しない。

2026-10-10採否: B→Cの固定CP932区間は58699.5→47816.5us、対応after/before比中央値0.827020。DBCSのwide要求容量が実使用の最大二倍となる代償を受け、Cを採用した。RSSやpeak memoryを測った倍率ではない。code-pages22 checks、統合の実CP932保存と正式8本の0退行を確認した。A/Bとの別比較の改善率を足さない。数値と全証拠は[gate-proofs 5-db/5-dc](../quality/gate-proofs.md)。
