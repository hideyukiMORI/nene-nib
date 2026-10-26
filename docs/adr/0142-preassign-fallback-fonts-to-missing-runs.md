# ADR 0142 — 主字体に無い文字の連続区間には、OS の MapCharacters が選ぶ字体を layout の範囲に先に当てる

- 状態: 提案（設計席 2026-10-16 夜。採用の条件は決定 6 の画素と位置の一致。満たさなければ `hold` として記録する）
- 日付: 2026-10-16
- Issue: #586（probe `experiment/long-line-layout-probe`・報告 `D:/NeNeNib/worktrees/long-line-probe/out/reports/probe-long-line.md`）
- 規則: ARC-007 / CPP-017 / QLT-014、ADR 0071 / 0073 / 0114 / 0075

## 文脈

6 KB（2,077 code points）の日本語の行で 1 打鍵が 5.5〜6.3 ms かかり、そのうち `BodyTextLayouts::make_layout` の `GetMetrics` が 3.4〜4.5 ms を占める。probe の内訳: UTF-16 変換 18 µs・`CreateTextLayout` 9 µs・字体の保持の照合 220 µs（1,069 回）・OS の `MapCharacters` 180 µs（miss 12 回）で、残り 3.3〜4.0 ms は DirectWrite 自身が **主字体（Cascadia Code）に無い文字の代替の run を区切りごとに作る費用**である。DirectWrite は漢字・かな・句読点の文字種の境目ごとに代替を呼び、1 行で 1,081 回（平均 1.9 文字ずつ）になる。時間は文字数でなく代替の区切りの数に比例し（ASCII だけ 0.4 ms・漢字だけ 1.0 ms・ベンチの文書 4.1 ms）、主字体を日本語を持つ字体にすると 0.1 ms になる。

ADR 0071 は字体の選択を OS の `MapCharacters` だけに固定し、同じ要求の結果を保持することで 16 ms を 3.3 ms にした。保持を即答にしても 2 ms 残るのは、代替の run を作ること自体の費用だからである。ADR 0071 は「字体を明示して連結する実験は位置に差が出たので採用しない」とも記録している。今回の形は連結ではなく、1 つの layout の中で**OS が選ぶのと同じ字体を、DirectWrite が代替する前に範囲の書式として当てる**ものである。probe の単体の比較では 17 ms → 0.09 ms（走査込み）。

## 決定

1. **当てる場所は `BodyTextLayouts::make_layout` の 1 か所。** `CreateTextLayout` の直後・最初の問い合わせ（`GetMetrics` / 字形の収集）の前に、本文の layout（通常行・IME の差し込み行・格子の画面行・巨大な行の窓）に当てる。ステータス・行番号・コマンド入力の layout には当てない（短い・ADR 0070 / 0079 の保持の外）。
2. **区間は DirectWrite の文字種の区切り（`IDWriteTextAnalyzer::AnalyzeScript`）で、呼ぶのは主字体に無い文字を含む区切りだけ。** 「主字体に無い文字」は主字体の face の `HasCharacter` で code point ごとに決め、結果は本文の書式が所有する表（BMP は bitset・それ以外は小さな map）に保持し、書式を作り直すときに捨てる（`FontFallbackCache` と同じ寿命・ADR 0071）。幅 0 の code point（結合文字・VS・`DisplayWidth::zero`）は直前の文字に従う（ADR 0053 の 1 文字の境目を割らない）。主字体で足りる区切りは字体選択を呼ばない。区切りを受けるのは sink の型 1 つ（`ScriptRunSink`・`FallbackRunSource` と対）。
3. **区切りごとに OS の `MapCharacters` を呼び、`mappedLength` の分だけ当てる。同じ字体の隣は 1 つの範囲にまとめる。** 呼ぶのは ADR 0071 の `FontFallbackCache` 経由（要求の文字列は区切りの文字列で、今の DirectWrite と同じ長さなので保持が効く・locale と書式の weight / style / stretch は layout と同じ）。返った `IDWriteFont` の family 名を `SetFontFamilyName(range)` で当て、weight / style / stretch が書式と違えば同じ範囲に当て、`scale` が 1 でなければ `SetFontSize(size × scale, range)`。`mappedLength` が区切りより短ければ残りで繰り返す。渡す長さは保持の鍵の上限 256 単位（`fallback_key_text_limit`・ADR 0071）まで。区切りの内側でも残りの長い列を毎回走査しない（切り目はサロゲートの対と幅 0 の code point を割らない）。返った字体が直前の範囲と同じ（family・weight / style / stretch・scale）で隣り合えば範囲を伸ばし、`SetFontFamilyName` の呼び出しを減らす。主字体と同じ字体が返れば当てない。font が null（どの字体にも無い）なら何も当てない（DirectWrite が今までどおり欠字の字形を出す）。字体を選ぶ規則は増やさない（ADR 0071）。区間全体を 1 回で渡す最初の形は、OS が文字種の区切りの長さしか返さず残りの長い文字列を毎回走査して長い行の 1 打鍵が 5 ms → 137 ms になったので採らない（#586 の実測）。
4. **区間の数に上限を置く。** 1 行で当てる範囲の数が `fallback_range_limit`（最初は 4,096）を超えたら、超えた分は当てずに DirectWrite の代替に任せる（巨大な行の窓は 16,384 文字なので上限に届き得る・費用が線形に戻るだけで意味は同じ）。上限は定数 1 つ。
5. **`FontFallbackCache` はそのまま残す。** 当てなかった範囲（上限超・null の字体・判定の表に無い astral の文字など）は今までどおり DirectWrite が保持つきの代替を呼ぶ。経路は 2 本にならない（選ぶのは OS・保持は 1 つ・当てる場所は 1 つ）。
6. **採用の条件は画素と位置の一致。** (a) `tests/ui` の契約: 日本語・混在・絵文字（ZWJ の連なり・国旗・VS16）・結合文字・ハングル・Tab 入り・双方向の見本で、当てた layout と当てない layout の **全 UTF-16 位置の `HitTestTextPosition` と字形の列（face・字形番号・送り・offset・絶対位置）が一致**し、行数と幅も一致する。字形の列は run を順に一列に並べたもので、run の区切りは DirectWrite が文字種で切るのに対し当てた側は同じ字体がつながるので比べない。offset の null は 0 の配列と同じとみなし、位置（`HitTestTextPosition` と絶対 x・y）は float の丸め |a − b| ≤ 2 × FLT_EPSILON × max(|a|, |b|) を許す。(b) `eng/compare-frames.py` で、長い行のベンチの文書・`verify-window --capture --keys` の日本語と IME の節・堅牢性の入力（`flags-100k` / `zwj-100k` / 混在）の PNG が前後で差 0。どちらかが崩れたら採用せず、差の出た見本と理由を ADR に `hold` として書く（ADR 0075 の形）。
7. **速さは正式ゲートで示す。** `key-to-frame-single-long-line` が基準（約 6 ms）から 1 ms 台へ下がり、ほかの 7 本と巨大 1 行の 2 本に退行が無いこと（5 試行・静かな機械・QLT-014）。基準値の更新は merge 後（D52 / D41 の形）。

当てた字体の run は長くなるので、横の切り落としは字形の単位で行う（字形の配列は全体のまま持ち、描く窓だけを狭める・ADR 0073 の 2026-10-16 の補足）。

## 却下した選択肢

- 主字体を日本語を持つ字体（MS Gothic・BIZ UDGothic）にする: 施主の字体の選択（D13 / D14 の `set guifont=`）を変える話で、速さのために既定を変えない。
- `IDWriteFontFallbackBuilder` で CJK → 1 字体の規則を足す: 14 ms で効かない（probe の表）。字体の選択規則を増やすので ADR 0071 にも反する。
- 字体ごとに layout を分けて連結する: ADR 0071 が位置の差で却下した形。
- 保持の照合を hash に・隣り合う同じ face の run を収集でつなぐ・毎回の 12 miss を潰す: 合わせて 1 ms 余り。決定 1〜5 の後に残る費用の内訳を見てから別に判断する。
- 行を分割して layout を小さくする（split）: ADR 0075 の保留のまま。

## 限界

- 区切りの数だけ保持の照合が残る（ベンチの行で 1 行 1,081 回・約 0.2 ms）。
- 当てた字体は「OS が今日選ぶ字体」で、OS の字体の表が変われば変わる（DirectWrite の代替と同じ依存）。
- `HasCharacter` の表は書式ごとに作る。最初の行で BMP の bitset（8 KiB）を埋める費用は code point の出現ごとの遅延評価にする（全 BMP を一度に問い合わせない）。
- 折り返さない行の遠い位置（約 8,000 px 先）では、run の区切りが違うので位置の累積の丸めが float の 1 ulp（約 0.001 px）違うことがある（#586 の実測）。
- 折り返さない行では隣の run がつながるので、ADR 0073 の「完全に画面外の run を除く」が効きにくくなる（ベンチの行で 2 run / 1,007 字形・画面外の約 960 字形を描画に渡す）。工程 2 で run の中の画面外の字形を描く窓から外す（ADR 0073 の 2026-10-16 の補足）。
- 絵文字の色の字形（COLR）や縦書きは今日の製品に無く、契約の見本にも入れない。

## 追補（2026-10-27・#875）: 決定 3 の片の切り目を内容で決める

決定 3 の「渡す長さは 256 単位まで」の切り目を、区切りの先頭からの位置ではなく片の内容で決める。決定 1・2・4・5 と ADR 0071 の字体を選ぶ規則・`hold` / `flush` / `apply` は変えない。

- **理由**: 位置で 256 単位ごとに切ると、行の途中に 1 文字入れたときに後ろの片がすべて 1 文字ずれ、`FontFallbackCache` の鍵（片の文字列）が変わって保持が外れる。長い日本語の行（`key-to-frame-single-long-line`・1 行 2077 code point・約 9 片）の 1 打鍵 1.93 ms のうち約 1.0〜1.5 ms が片ごとの OS の `MapCharacters` だった（2026-10-27 の診断 exe の節目 `text_layout_created → fallback_mapped`・Issue #875）。
- **規則（2 段）**: 切り目を決めるのは `src/ui/win32/FallbackPiece` の純関数 `fallback_piece_length(wide, start, remaining)` の 1 本。
  1. `start` から code point を歩き、code point `c` の後ろで切る。条件は (a) 歩いた長さが最小 `fallback_piece_minimum` = 16 単位以上、(b) `(c × 0x9E3779B1) >> 26`（上位 6 ビット）が 0（平均 64 単位に 1 つ）、(c) 次の単位がサロゲートの対の後半か幅 0 の code point でない（ADR 0053 の 1 文字の境目を割らない）。
  2. 上限 `fallback_key_text_limit` = 256 単位まで 1 段目の当たりが無ければ、16 以上 256 未満の切ってよい所（(a) と (c) を満たす所）のうち hash が最小の所（同点は後ろ）で切る。切ってよい所が 1 つも無いときだけ今までの位置の規則（256 から境目を割らない所まで戻す）。
  - `remaining` が 16 以下か、256 以内で 1 段目の当たりが無ければ残りの全部を 1 片にする。`mappedLength` が片より短いときは `start + done` から同じ規則で次の片を切る。
  - 2 段目を置く理由: ベンチの行は 23 種の文字の文（「日本語の長い行と分割表示の文字組みを測定する。」）の繰り返しで、1 段目の当たりが 1 つも無い（23 種で当たりが無い確率は (63/64)^23 ≈ 70%）。1 段目だけでは 9 片すべてが位置の 256 で切られ、1 文字の挿入で 9 片すべてが変わった。2 段目では新しくなる片は最悪 1（`D:\NeNeNib\scripts\875\sim.py` の試算・挿入位置 37 単位おき）。
- 切り目は内容だけで決まるので、同じ文字列は位置に依らず同じ片になり、挿入・削除で変わるのは触れた片とその隣まで（`nib_window_tests` の `verify_fallback_pieces`: 位置に依らない・長さの範囲・サロゲート / U+0301 / U+FE0F の手前で切らない・2077 単位の行に 1 文字入れて変わる片は 2 つ以内）。
- 片が小さくなる（平均 64）ぶん、`FontFallbackCache` の保持の件数を 128 → 512 にする（ADR 0071 の決定の件数の上書き・線形探索のまま・鍵の比較は長さの違いで先に落ちる）。
- 定数（0x9E3779B1・26・16・256）は仮置きで、速さの確かめ（設計席の `key-to-frame-single-long-line` と 8 本の `--check`）の後に見直す。
