# ADR 0006 — 速さはゲートで見張り、SIMD は区画に閉じ、Vim の分岐は表で書く

- 状態: 受理
- 日付: 2026-09-15
- Issue: #1
- 影響する規則: CPP-012 / CPP-018 / QLT-014 / CNF-009

## 文脈

施主の優先順位は速さが 1 番である。厳格規約のゲートは CI とビルド時に効くもので実行時の速さは削らないが、速さそのものを
守る規則が無ければ退行に気づけない。施主は「ベンチ 3 本の退行で CI を落とす」「SIMD の許可範囲を ADR に明記する」
「Vim のコマンド分岐は表駆動で複雑度に収める」の方向を了承している（SPEC-DRAFT 3.1）。

Phase 0 で実測した（[phase0-results.json](../quality/phase0-results.json)）:

- S1〜S4: clang-cl は target feature の無い関数で `_mm256_*` を書くとコンパイルエラーにする。`/arch:AVX2` を target 単位に付けるか、
  `[[gnu::target("avx2")]]` を関数に付けたときだけ通る。cl は何も無くても通す（S3-avx2-without-arch-msvc-hole）
- T8: 60 分岐の `switch` を持つ関数は clang-tidy `readability-function-size`（60 行）で落ち、同じ 60 対応を `constexpr std::array` の表と
  5 行の検索関数にすると通る。認知的複雑度（10）は `switch` を 1 と数えるので分岐数の上限にはならず、**関数長 60 行が実際の上限**である

## 決定

1. **ベンチ 3 本（起動→最初の描画 / キー→画面 / 1 GB を開く）を `eng/` の計測スクリプトで測り、基準値からの退行でゲートを落とす（QLT-014）。**
   目標値は初版を実測してから決める。基準値は `eng/perf-reference.json`（「baseline」の語は CNF-005 が禁じる設定ファイル名なので使わない）に
   置き、下げる（許容退行を広げる）には ADR が要る。CI の機械は揺れるので、CI では「同じ CI 機での前回値からの相対退行」だけを見て、
   絶対値の目標は施主の実機で測る（QLT-013）
2. **SIMD の組み込み関数は `src/core/simd/` 配下にだけ書き、関数ごとに `[[gnu::target("…")]]` で機能を宣言する（CPP-018）。**
   target 単位の `/arch` は付けない。同じ処理には必ず組み込み関数を使わない実装（fallback）を置き、どちらを使うかは合成ルートが
   起動時に 1 回 `cpuid` で決めて注入する。中核は「どの実装が選ばれたか」を知らない
3. **Vim のキー列 → 動作は `constexpr` の表で書き、`switch` の巨大な分岐にしない（CPP-012）。** 表の検索と各動作は別関数で、
   それぞれが 60 行・複雑度 10・引数 4 に収まる。閾値を破る場合は計測値を添えた ADR が要る

## 強制

- QLT-014: **planned**（ベンチも基準値もまだ無い。Phase 3 の縦切りで最初の値を測ってから結線する）
- CPP-018: **planned** → clang-cl の target feature 検査（S1・コンパイルエラー）＋ CNF-009 の字句検査（SIMD ヘッダの置き場）。
  検査器の正例・反例がゲートで回った時点で CNF-009 は active、`src/core/simd` が生まれて prove-gates が実ビルドで落とした時点で CPP-018 は active
- CPP-012: **planned** → clang-tidy `readability-function-size` / `readability-function-cognitive-complexity`（T7 / T8）

## 結果

得られるもの:

- 速さの退行がゲートで見える。「速い」が README の主張ではなく実測になる
- SIMD が中核の 1 区画に閉じ、fallback が必ずある。実装の切り替えが合成ルートの 1 か所
- Vim の分岐が表なので、fixture（ADR 0005）と表を突き合わせられる

失うもの:

- ベンチの計測は実機と CI で条件が違い、CI の値は粗い。厳密な数値は施主の実機で記録する
- SIMD の実装は 2 本（組み込みと fallback）を保守する。fallback だけでも要件を満たすことを先に示す

正直に記録しておくこと:

- 「複雑度 10 で表駆動に収まる」は、正確には「関数長 60 行で `switch` が落ち、表が通る」である（T8）。複雑度の指標は分岐数を測っていない
- `[[gnu::target]]` は GNU 属性である。clang-cl では `-Wpedantic` の下でも警告にならなかった（S4）
- Windows の CI ランナーに AVX2 が無い可能性がある。fallback の経路は CI で、組み込みの経路は実機で走る

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 速さのゲートを置かない | 退行が見えない。「爆速」が主張だけになる |
| CI で絶対値の目標を課す | 共有ランナーの揺れで偽の失敗が出る。相対退行だけを見る |
| target 単位に `/arch:AVX2` を付ける | 中核全体が AVX2 を仮定し、fallback が無くなる。関数単位の宣言に限る |
| SIMD をどこにでも書く（cl の流儀） | clang-cl でも `/arch` を付ければ通ってしまう。区画と属性で範囲を機械に見せる |
| 巨大な `switch` を waiver で通す | 60 行の上限は表で守れる（T8）。waiver は最後の手段 |

## 関連

- [ADR 0003](0003-cpp23-clang-cl-foundation-and-measured-limits.md)（S1〜S4・T7 / T8）
- [ADR 0005](0005-own-vim-engine-verified-against-real-vim.md)
