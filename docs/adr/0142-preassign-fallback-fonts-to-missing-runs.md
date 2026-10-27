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

## 追補（2026-10-27・#884）: 区切りの片の文字列の覚え

区切りの数が多い文（漢字とかなの交互）では、決定 3 の区切りごとの `MapCharacters` が 1 行に 1,000 回を超え（診断 #875 の大きな文書で 1 回の当てに 1,080 片・`FontFallbackCache` の当たり 1,080 / 外れ 1）、当たりでも 1 回約 0.85 µs（鍵の文字列と locale の写し・保持の線形の照合）が積もって `analyzed → assigned` が約 0.93 ms になる。`PrimaryFontCoverage` に区切りの片の文字列の覚え `FallbackSegmentMemo` を 1 つ持たせ、片ごとにまず覚えを引き、当たればその字体・`scale`・`mappedLength` で決定 3 と同じ `hold` へ進み、外れたときだけ今までどおり `FontFallbackCache` 経由の `MapCharacters` を呼んで、`S_OK` で進んだ（`mappedLength` が 0 でない）答えだけを覚える（失敗は覚えない）。鍵は片の文字列だけで、書式の family・weight / style / stretch・locale・向きは覚えを持つ `PrimaryFontCoverage` の中で一定なので鍵に入れない。字体が null（どの字体にも無い）の答えも覚える。探索は FNV-1a（64 ビット）の開番地法（番地は上限の 2 倍）で、引くときは文字列を確保せず単位ごとに比べ、入れるときだけ写す。上限は 4,096 件（`fallback_segment_memo_limit`）で、超えたら全部捨てて覚え直す（単純で決定的）。寿命は `PrimaryFontCoverage` と同じ（書式の作り直し・device lost で `BodyTextLayouts` が作り直す）。字体を選ぶのは今までどおり OS の `MapCharacters` だけで、区切りの作り方・`hold` / `flush` / `apply`・ADR 0071 の `FontFallbackCache`（DirectWrite が layout の中で呼ぶ道）は変えない。契約は `nib_window_tests --fallback-memo`（同じ片の 2 回目は字体選択を呼ばない・違う片は呼ぶ・null の答えも覚える・失敗は覚えない・4,096 件を超えると捨てて呼び直す）。限界の 1 行目の「区切りの数だけ保持の照合が残る」は、2 回目以降の同じ片では覚えの 1 回の hash に置き換わる。

## 追補（2026-10-27・#889）: 覚えの字体の正規化と `joins` の比べ方

OS の `MapCharacters` は同じ family でも呼ぶたびに別の `IDWriteFont` を返すので、覚えに入る区切りの字体はすべて別のポインタになり、直前に名前を引いた字体の再利用（`named_`）が効かず、`joins` と主字体の判定のたびに `english_family`（COM 4 往復と `std::wstring` の確保）が走っていた（1 行に約 1,080 回）。`FallbackSegmentMemo` に正規の字体の表（行ごとに english family 名・weight・style・stretch・`ComPtr<IDWriteFont>` の平行な列）を持たせ、`MapCharacters` の答えの字体は覚えに入れる前に必ず正規の行にする: 表にある物はその行、新しい物は family 名を 1 回だけ読んで同じ組の行を探し、無ければ足す。覚えの entry と `PrimaryFontCoverage` の当てる前の範囲は字体を行の番号で持ち、行の番号が等しいことは組が等しいことと同じなので、`joins` は行の番号・隣り合い・`scale` で、主字体の判定と `apply` は行の family 名と weight / style / stretch で比べ・当て、どちらも COM を呼ばない（`family_of` と `named_` は無くした）。null の字体と family 名の読めない字体は行にせず null と同じに扱う（今までも名前が空なら `apply` は何も当てなかった・DirectWrite の代替のまま）。表は覚えと同じ寿命で、覚えを 4,096 件で捨てるときも残す（組の数は collection の字体の数を超えない）。字体を選ぶ規則・区切りの作り方・`hold` / `flush` の範囲のまとめ方・`apply` が当てる値は変えない。契約は `nib_window_tests --fallback-memo` に足した 2 つ（同じ組の別の物は同じ行と同じ `ComPtr`・family 名は新しい物のときだけ読む・違う weight は別の行／委ね先の答えを片ごとに別の物へ包んでも長い区切りは 1 つの範囲になり、family 名を読むのは字体選択の答えの数まで・同じ行の 2 回目は 0 回）。

## 追補（2026-10-27・#894）: 主字体に無い文字が無い行

主字体に無い文字が無い行は区切りを作らずに返す: `assign` は `AnalyzeScript` の前に行全体を決定 2 と同じ規則（`has_missing`・幅 0 の code point は直前に従う）で 1 回走査し、無い文字が 1 つも無ければ当てる範囲の無いまま `S_OK` を返し、1 つでもあれば今までどおり区切りごとに当てる（区切り・覚え・字体を選ぶ規則は変えない・契約は `nib_window_tests --fallback-ranges` の 2 つ）。
