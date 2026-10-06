# ADR 0076 — SDK が固定した COM の署名は恒久の境界の規則にし、waiver を閉じる

- 状態: 受理
- 日付: 2026-10-06
- Issue: #299
- 影響する規則: CPP-012 / CPP-015 / CPP-017 / CPP-019 / QLT-010 / CNF-003 / CNF-004 / CNF-012

## 文脈

#291 の `FontFallbackCache::MapCharacters`（[ADR 0071](0071-cache-complete-system-font-fallback-requests.md)）と `BodyGlyphCollector` の `IDWriteTextRenderer` の 4 メソッド（[ADR 0073](0073-visible-body-glyph-capsule.md)）は、
Windows SDK が決めた COM の署名を実装する。引数の数は `MapCharacters` が 11（`dwrite_2.h`）、`DrawGlyphRun` と `DrawInlineObject` が 7、`DrawUnderline` と `DrawStrikethrough` が 5（`dwrite.h`）で、
減らすと override にならず DirectWrite が呼べない。CPP-012 の引数の上限 4 を `NOLINTNEXTLINE(readability-function-size)` で外し、WVR-0001 / WVR-0002 が期限 2026-11-04 で支えていた。

これは期限つきの例外ではなく変わらない事実なので、期限が来るたびに延ばすか、無関係な PR を止めるしかない（独立レビュー L1）。
また clang-tidy は引数の数だけを止められないので、その抑制は行数・入れ子もまとめて止めており、「引数の数だけ」を守っていたのはレビューだった（同 O1）。

## 決定

1. **規則 CPP-019 を足す。** SDK が固定した COM の署名は境界で受けて、すぐ中へ渡す。その関数は CPP-012 の引数の数の上限の対象から外れる。
   本体は 6 行以内で、要求値を組んで 4 引数以内の関数へ渡すか、未対応の引数を境界で断る（`E_NOTIMPL` などを返す）だけにする。ループと保持（キャッシュの操作）は書かない。
2. **書き方は 1 つ。** 定義のすぐ前の 2 行を `// SDK-ABI: <Interface>::<Method>` と `// NOLINTNEXTLINE(readability-function-size)` の順で書く。印の文法は空白 1 つで末尾に何も書かない形だけ。
3. **表は 1 つ。** `eng/sdk-abi-signatures.json` に interface・method・SDK の引数の数・宣言のあるヘッダを 1 行ずつ持つ。いまは上の 5 行だけ。表に行を足すのは PR のレビュー事項。
4. **機械の検査 CNF-012 を足す。** 印の文法・置き場（`src/ui/win32/` か `tests/ui/`）・表・次の行の抑制がちょうどその 1 check であること・次に始まる定義の関数名・本体 6 行を見る。
   印が正しいときだけ、その `NOLINTNEXTLINE` は CNF-003 の waiver の要求から外れる。**印の無い NOLINT と、ほかの check を混ぜた NOLINT は今までどおり CNF-003 が落とす。**
5. **WVR-0001 / WVR-0002 を閉じる。** 台帳は `Status: removed` と閉じた記録を書いて残し、索引から外す。ソースは `// Waiver: WVR-000N` の行を `// SDK-ABI: ...` の行へ 1 対 1 で置き換え、コードの行は変えない。

### 6 行にした理由と経緯

はじめ設計席は「本体 4 行・分岐なし」とした。実装席が `BodyGlyphCollector::DrawGlyphRun` に当て、未対応の effect を境界で断る早期 return（ADR 0073 の「effect では収集全体を棄却」）を含む 5 行で止まった。
設計席は事後に数字を合わせるのではなく、規則の目的（抑制が効くのを引数の数だけに閉じる）から数え直した。この書式（`{` `}` は単独行）で入れ子 4 を書くには 13 行以上要るので、6 行では関数の長さ 60 行と入れ子 3 の上限を原理的に越えられない。
認知的複雑度（`readability-function-cognitive-complexity`）は抑制の対象ではなく、今までどおり掛かる。境界での引数の検査は境界の仕事なので、分岐の禁止は外した。

## 強制

- CNF-012: **active**（`eng/conformance.py` の `sdk_abi_checks`・`tests/conformance` の `SdkAbiChecks`。反例は [gate-proofs 5-cr](../quality/gate-proofs.md#5-cr--sdk-が固定した-com-の署名の印issue-299adr-0076)）。
- CPP-019: **planned**。表の「SDK の引数の数」と実際の署名の照合は字句ではできない。ループと保持を書かないことも機械では見ない。どちらもレビュー事項。
- clang-tidy の閾値と check の一覧（`.clang-tidy`）は変えていない（QLT-010）。

## 結果

- waiver の一覧は `none` に戻り、期限でゲートが落ちる心配が無くなる。
- 抑制の対象の関数が増えるときは、表に行が要るので PR の差分に必ず出る。表に無い印・形の崩れた印・印の無い NOLINT はゲートが落とす。
- 残る穴: 表の行が正しい SDK の署名かは人が見る。印のある関数の本体 6 行の中にループや保持を書いても、関数の長さと入れ子の上限には当たらないので機械は気づかない（認知的複雑度には当たり得る）。
- `tests/ui/`（#300 で作る）の替え玉も同じ書き方で書ける。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| waiver の期限を延ばし続ける | 変わらない事実を期限つきの例外として扱うことになり、期限ごとに無関係な PR が止まる |
| `.clang-tidy` の `ParameterThreshold` を上げる・ファイルを除外する | ゲートの弱体化で、ほかのすべての関数の上限も緩む（QLT-010） |
| 引数を構造体にまとめる | COM の vtable と一致せず override にならない |
| 本体 4 行・分岐なし（最初の設計） | 境界で未対応の引数を断る早期 return を書けない。目的（抑制を引数の数に閉じる）には 6 行で足りる |
| 印をゆるい正規表現で受ける | 書き方が 2 つ以上になる（ARC-001）。1 つの形だけを受ける |
