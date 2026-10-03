# ADR 0065 — 通常モードの結合文字と削除方向を区別する

- 状態: 受理
- 日付: 2026-10-03
- Issue: #282
- 影響する規則: FR-002 / D38 / ARC-001 / ARC-003 / ARC-004 / CPP-002 / CPP-011 / QLT-001 / QLT-012 / QLT-013

## 文脈

ADR 0053 は Vim の文字境界を決め、通常モードの矢印と削除は別件へ残した。2026-10-03、hide はアクセント・濁点・異体字セレクタから段階的に進めることを選択した。連結絵文字や言語固有の全書記素境界までは含めない。

[Microsoft の RichEdit hot keys](https://devblogs.microsoft.com/math-in-office/richedit-hot-keys/) を参照し、Windows の `msftedit.dll` 10.0.26100.8875 を非表示の自前 RICHEDIT50W control で調べた。記録は `out/probes/ordinary-characters-2026-10-03.json`。これは [Office RichEdit を使う Notepad](https://devblogs.microsoft.com/math-in-office/windows-11-notepad/) 本体の試験ではない。

## 決定

1. **単位を明示する**。`CaretMoveRequest` は motion・page_lines・`CaretUnit` を持つ。通常モードは ordinary、Vim INSERT と既存の Vim 用移動は code_point。全方向の経路は `moved_caret` の一つに保つ。
2. **通常モードの境界は一つ**。`ordinary_character_boundary` が前後移動・着地補正・前後削除の境界を返す。UTF-8 の読み解きは既存の `Utf8` だけ。読む窓は code point の最長 4 bytes に制限し、行全体を複製しない。改行は境界であり、CRLF は既存と同じく一つで越える。
3. **今回の文字範囲**。基底に続く幅 0 の文字は既存の `DisplayWidth` の固定表から判定する。アクセントと濁点のために表を複製しない。共通の VS（U+FE00..FE0F / U+E0100..E01EF）は独立した意味を持つ。Mongolian FVS と各言語固有のまとまり、ZWJ・肌色・国旗は完全互換の対象にしない。完全な UAX #29 実装とは呼ばない。
4. **左右と Delete**。左右は基底・後続の結合文字・最初の VS までを一つとして越える。VS の後の結合文字は別の単位。Delete は同じ範囲とさらに続く VS を消す。孤立した結合文字の列でも停止し、上下/ページ移動の着地が単位の途中なら先頭へ寄せる。行末はそのまま。
5. **Backspace**。直前のコードポイント一つを消す。ただし直前が VS なら連続する VS とその直前のコードポイント一つを消す。`e + accent + VS` なら accent と VS を消して e を残す。選択がある場合は既存の選択範囲をそのまま `replace` へ渡す。undo・redo は既存の履歴で一操作として復元する。
6. **共有境界**。Vim の文字境界・INSERT 移動・削除は既存の意味を維持する。通常モードからの `DeleteText` だけが通常の削除境界を使う。本文の型・保存形式・OS 入力・描画の契約は変えない。

## 強制と検証

自動検証は **active**（2026-10-03、[gate-proofs 5-cf](../quality/gate-proofs.md#5-cf--通常モードの結合文字境界issue-282adr-0065)）。新規 `--ordinary-characters` で UTF-8・結合文字列・VS・改行・選択・上下移動・undo と共有 Vim INSERT を確かめる。既存の移動契約と `--vim-characters` を直接の退行範囲として選ぶ。Release の実機確認は用意した実行ファイルと限定した手順を、hide の了承後に実施する。全件検証は行わない。

## 結果

通常モードの移動と削除が結合文字を扱い、Backspace でアクセントだけを取り除ける。複雑な文字のまとまりと正確な視覚桁の維持は後続の範囲である。
