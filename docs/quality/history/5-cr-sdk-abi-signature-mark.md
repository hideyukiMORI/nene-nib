# 5-cr — SDK が固定した COM の署名の印（Issue #299・ADR 0076）

出典: docs/quality/gate-proofs.md の節 5-cr（2026-10-16 に D50 で移動・本文は移動時のまま）

確認する退行: CNF-003 の waiver の要求に例外を作るので、(1) 印の無い NOLINT とほかの check を混ぜた NOLINT が今までどおり落ちること、(2) 印の形・表・置き場・本体の行数が外れたら CNF-012 で落ちること、(3) 既存の 5 か所が clang-tidy で今までどおり黙ること、(4) 製品のコードの行が変わらないこと。

- `python eng/test-conformance.py` → `Ran 253 tests`・`OK`。`SdkAbiChecks` の 13 件: 正例 4（1 行の `E_NOTIMPL`・本体 6 行ちょうど・`if` の早期 return を含む境界・`tests/ui/`）、反例 9（表に無い method・関数名が印と違う・本体 7 行・空行とコメントを数えて 7 行・NOLINT に `bugprone-x` を混ぜる（CNF-003 も）・置き場が `src/application/`・印の文法 4 通り・本体の無い宣言・印の無い NOLINT は CNF-003）。
- 実リポジトリの反例: `BodyGlyphCollector.cpp` の印を `DrawGlyphRuns` に変えると `CNF-012 ... is not in eng/sdk-abi-signatures.json` と `CNF-003 ... missing valid, scoped waiver`・終了 1。戻すと `Conformance: 0 violation(s)`・終了 0。
- WVR-0001 / WVR-0002 は `Status: removed`・索引は「なし」。閉じた後の `python eng/conformance.py` は 0 violation（CNF-004 の指摘 0）。
- `. ./eng/toolchain.ps1` の後 Debug で `cmake --build build --target nenenib_window` → 終了 0。clang-tidy（`CXX_CLANG_TIDY`）は `FontFallbackCache.cpp` と `BodyGlyphCollector.cpp` で何も出さない。
- `git diff -U0 src` は 10 行（5 か所の `// Waiver: WVR-000N` → `// SDK-ABI: ...` の削除と追加だけ）。`clang-format --dry-run --Werror` と `git diff --check` は終了 0。
- 表の「SDK の引数の数」は Windows SDK 10.0.26100.0 の `dwrite_2.h`（`MapCharacters` 11）と `dwrite.h`（`DrawGlyphRun` 7・`DrawUnderline` 5・`DrawStrikethrough` 5・`DrawInlineObject` 7）を読んで書いた。機械では照合しない。
- やっていない: Release・GUI・速さ・全件の CTest（製品のコードの行が不変で、実行ファイルに入る差分が無い）。

適用: #299 / ADR 0076 / CPP-012 / CPP-015 / CPP-019 / CNF-003 / CNF-004 / CNF-012 / QLT-010。`.clang-tidy` の閾値と check の一覧は変更なし。Waivers: none。
