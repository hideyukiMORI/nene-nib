# ADR 0086 — ASCIIの連続区間はUTF-8検証の正典の中でまとめて数える

- 状態: 受理（限定実装と比較、製品採用は実測後）
- 日付: 2026-10-09
- Issue: #330
- 影響する規則: ARC-001 / ARC-008 / CPP-014 / CPP-016 / CPP-018 / QLT-001 / QLT-010 / QLT-012 / QLT-014

## 文脈

#320はUTF-8の二重検証を一回にした。その一回の正典 `validate_utf8` は、ASCIIにも一文字ずつ一般の長さ・継続・過長符号化・最大値の検査を適用する。ASCIIが連続している間は、各byteが0x80未満であることだけで正しい一byteの符号値と確定する。hideの2026-10-09指示に従い、この閉じた処理を変更前後で比較する。

## 決定

1. public APIと所有境界は変えず、`Utf8.cpp` の `validate_utf8` だけを正典のまま保つ。先頭byteがASCIIの時だけ、privateな `ascii_end` で連続区間を数える。
2. 残りが `sizeof(std::uint64_t)` 以上の時だけ `std::memcpy` でその8byteを局所整数へ読む。`0x8080808080808080` のmaskとANDして0なら全byteがASCIIなので8進む。全byteで同じmaskなのでendiannessに依らない。アラインメントを要求せず、型を付け替えるcast、入力の書換、範囲外の先読みをしない。
3. 非ASCIIを含むwordでは止め、続く短いASCII列をscalarで読んで最初の非ASCIIまたは末尾で止まる。進んだbyte数だけcode point数を加える。非ASCIIからは従来の `scan` に戻り、継続byte・過長符号化・surrogate・U+10FFFF超・途中の末尾の拒否を一切変えない。ASCIIに続く不正byteを読み飛ばさない。
4. これはC++23の整数と `memcpy` による移植可能な実装で、SIMDの組み込み関数・専用命令の属性・targetの `/arch` ・CPU選択を追加しない。CPP-018が制限するintrinsicの区画・関数属性・fallback・合成ルートの規則は変更しない。`memcpy` は既存のcore許可シンボルであり、allowlistも変更しない。
5. 変更前の別実装やruntime切替を製品に残さない。比較は#320のみのcommitと、この候補のcommitに同じ#329 harnessを適用した別exeで行う。入力hash・toolchain・全試行・exe hashを保存し、warmupと結果確認を区間外に置く。

## 対象検証と採否

- Debugとclang-tidy、conformance/buildgraph、symbols、差分整形を行う。`--utf8` selectorは既存のUTF8・DisplayText・UTF16の境界契約を束ね、新たなASCII境界契約も既定実行と同じ関数から呼ぶ。本文/ファイルの直接callerは#320の `--file-text` で確認する。無関係なVim操作の全件再試験はしない。
- 0〜33byteのASCIIとNUL、subviewの開始位置0〜15、8byte境界の前後に来る有効な2/3/4byte列、不正byte・不正継続・過長・surrogate・最大値超・末尾途切れを確認する。期待値は既知の符号数と既知の不正列で与え、検証アルゴリズムの複製をtestに書かない。
- 計測対象はASCII 16,800,000byteと日本語6,219,000byteの `validate_utf8`、および16MiBのbuffer作成とcontrollerでの読込。試行順・回数は#329の固定ABBAに従い、途中で増減・選別しない。日本語で有意な悪化が出る、または利益が確認できない場合は不採用か追加設計として記録する。基準値・許容は緩めない。
- main統合はD41 / ADR0075の条件を別に満たす。短い区間の改善を起動全体の改善率として報告しない。waiverなし。
