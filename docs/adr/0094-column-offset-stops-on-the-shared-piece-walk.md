# ADR 0094 — 桁からbyte位置への変換は共通の断片走査を必要な境界で止める

- 状態: 技術受理（2026-10-10・#353で統合、CI/main反映は受理記録に従う）
- 日付: 2026-10-10
- Issue: #348（A）
- 影響する規則: ARC-001 / ARC-003 / ARC-007 / ARC-008 / ARC-009 / CPP-005 / CPP-012 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / QLT-014

## 文脈

`TextBuffer::offset_of`は行全体を文字列に集めてから指定桁まで進む。長い行の先頭に近い位置にも全文のcopyが必要になる。本文の範囲走査には既存の`visit_text_range`があり、B1では`collect`も同じ経路へ集めた。B1の比較用exeと全試料を固定した後、Aを別commit・別計測として扱う。

## 決定

1. 公開APIは変えず、`line_start`/`line_end`を改行モデルの正本として使う。columnが0または1なら行頭を返す。行外の列は従来どおり行末へ丸める。
2. privateの`visit_text_range`だけを停止可能にする。visitorのbool返値はtrueで継続、falseで即終了。既存の`collect`/`text_range`/`position_of`/`byte_at`は従来処理後にtrueを返す。別walkerやvoid/boolの分岐を追加しない。
3. `offset_of`は行の範囲をこのwalkerで借用し、残りcodepoint数と進んだbyte数だけを呼出し内で持つ。最初のsliceは従来の行文字列と同じく先頭から`next_code_point`で進める。
4. 最初以外のsliceの先頭がcontinuationなら、既存`is_boundary`と`next_code_point`でその連続部分を越え、残りcodepoint数は減らさない。残りが0でもslice末尾では走査を続け、後続pieceに跨がるcontinuationを越えて次のleadまたは行末で止める。UTF8原始の複製や独自decoderを追加しない。
5. raw byte編集で第一sliceがcontinuationから始まる場合も従来意味を保つ。CRLF/LF/末尾単独CRの扱いは`line_end`に任せ、ここで別の改行判定をしない。範囲visitorの借用を保存しない。
6. 行全文copy、恒久cache、piece容量の事前reserve、生成時の集計融合は追加しない。行頭/行末を断片列から探す費用は残るので、変換全体をO(1)とは呼ばない。

## 検証と採否

- `--buffer-range`に限定し、変更したwalkerの既存4callerと位置変換の契約を確認する。多byte scalarを2〜4以上のpiecesへ跨がせた桁境界、列0/1/超過、途中byteから始まるslice、改行と旧snapshotを必要な範囲で追加する。期待値を新実装から作らない。
- 通常のDebug toolchain/tidy/ASan/UBSan、整形、対象sourceの規約検査を行う。B1の既存成功だけでは返値を変えたwalkerを検証できないため、この直接scopeは再実行する。
- ADR0082で既に固定したoffset4本の入力・区間・期待を変更せず、共通harnessのbefore/after Releaseを固定ABBAで比較する。B1のexe/rawは上書きせず、全失敗・欠測を保存する。
- main採用は計測結果、独立レビュー、影響する正式速度ゲート、実機の直接操作、CIで判断する。基準値・許容・fixture・抑制・waiverは変えない。

## 実験後の採用判断（#353）

実装2e719acのbuffer-range282checksと独立レビュー、統合b8fd7cbの直接5scope2141checks・通常Release・実機・対象正式4条件で受理阻害なし。固定ABBA4条件の対応比は行頭0.006667、中央0.933786、末尾0.964706、8193断片中央0.636695。行頭afterは128変換で1〜3µsと分解能に近く、精密な倍率は保証しない。中央/末尾は全試料が短縮したわけではない。行頭/行末探索とpiece列の費用は残る。行copyを除き既存walkerへ集める差分として採用する。全値・実行コマンド・失敗記録は[gate-proofs 5-df/5-dg](../quality/gate-proofs.md)。
