# ADR 0149 — UTF-16（LE / BE・BOM つき）のファイルは BOM で見分けて読み、同じ形で書き戻す

- 状態: 提案（設計席 2026-10-20・hide 不在の自走の日。merge は独立レビュー・CI の後、試用は hide）
- 日付: 2026-10-20
- Issue: #497（外部講評 P4）
- 規則: FR-005 / D6、ARC-001 / ARC-003 / ARC-010 / CPP-002 / CPP-005 / CPP-014、ADR 0010（ファイル・保存は一時ファイル → 置き換え）/ 0036（改行の形）/ 0137（自動退避は保存と同じ `encoded_parts`）

## 文脈

`detect_encoding`（`src/core/TextEncoding.cpp`）は UTF-8 BOM → UTF-8 → Shift_JIS の順で見分け、`TextEncoding` は 3 値。UTF-16 の BOM（`FF FE` / `FE FF`）を持つファイルは UTF-8 として不正でも Shift_JIS としては「読めてしまう」ことがあり、文字化けして開く（外部講評 P4・#497）。Windows のメモ帳の既定は UTF-8 になったが、PowerShell の `Out-File` の既定（5.1）や古いツールは UTF-16 LE BOM を書く。UTF-8 ↔ UTF-16 の変換は core の `Utf16.hpp` の 1 対（#13）で既にある。

## 決定

1. **`TextEncoding` に `utf16_le` と `utf16_be` を足す（BOM つきだけ）。** `detect_encoding` は先頭の `FF FE` を `utf16_le`、`FE FF` を `utf16_be` と見分け、UTF-8 BOM・UTF-8・Shift_JIS の判定より**先**に置く。BOM の無い UTF-16 は見分けない（NUL の並びの推測は誤検出の元・限界に書く）。`switch` が落ちる所（ステータスバーの表示名・保存の符号化・ `with_opened` の判定）を全部直す（CPP-002）。
2. **読みは core の 1 本。** `DetectedText::from_bytes` が BOM を外し、奇数バイトなら `EncodingFailure`（閉じた enum に `odd_utf16_length` を 1 値）、BE は 2 バイトずつ入れ替えてから `to_utf8`（`Utf16.hpp`）で UTF-8 に。対になっていないサロゲートは既存の `invalid_utf16` → `FileFailure::undecodable`（開けなかったダイアログ・今の UTF-8 の不正と同じ道）。改行の形（LF / CRLF）は UTF-8 にした後に既存の `TextBuffer` の判定で決める（ADR 0036）。
3. **書きは `encoded_parts` の 1 本で同じ形に戻す。** 保存と自動退避（ADR 0137）は `encoded_parts(text, encoding)` が BOM（LE は `FF FE`・BE は `FE FF`）＋ `to_utf16` の結果（BE は入れ替え）を返す。本文が UTF-16 で表せない列（対になっていないサロゲートは UTF-8 の正しい本文には無いので起きない）は考えない。`Ctrl+Shift+S`（別名）でも符号化は文書の値のまま（今の UTF-8 BOM / Shift_JIS と同じ規則）。
4. **ステータスバーの表示は `UTF-16 LE` / `UTF-16 BE`**（`UTF-8` `UTF-8 BOM` `Shift_JIS` の隣・`encoding_label` の表に 2 行）。README の「扱う符号化」に足す。
5. **符号化の切り替えの命令は足さない**（`:set fileencoding=` は後続・今も無い）。
6. **検証**: core の `detect_encoding`（`FF FE` / `FE FF` / 奇数バイト / BOM の後に不正なサロゲート）・`from_bytes` → UTF-8 の本文（BMP・サロゲートペア・CRLF）・`encoded_parts` の往復（読んだバイト列と書くバイト列が一致・BOM と改行の形を含む）・`ExFileTests` / `--application`（開く → 編集 → 保存 → 読み直し）・`ScriptedFiles` の替え玉で UTF-16 のファイルを開く契約。実機は `verify-window.py` の節は足さず、hide の試用（PowerShell 5.1 の `Out-File` で作ったファイル）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| BOM の無い UTF-16 も NUL の並びで推測 | ASCII の多いバイナリや Shift_JIS と取り違える。誤検出は文字化けより悪い |
| 読んだら UTF-8 で保存する | 読んだ形のまま保つのが FR-005 / 今の UTF-8 BOM・Shift_JIS の規則と同じ |
| adapters で変換する | 符号化の判定と変換は core の 1 本（ARC-001・#13 の UTF-16 の 1 本化） |

## 限界

- BOM の無い UTF-16・UTF-32・UTF-7 は読まない（Shift_JIS か UTF-8 として判定される・今までどおり）。
- 符号化の切り替えの命令は無い。
