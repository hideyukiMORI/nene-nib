# ADR 0187 — 規則の状態と文を、現に機械が強制している形に合わせる（CPP-002 / 012 / 018 / 019 を active に・CPP-003 / 006 / 009 / 016 に例外を名指しする）

- 状態: 受理（設計席の提案・施主の了承 2026-11-15）
- 日付: 2026-11-15
- Issue: #1718
- 影響する規則: CPP-002 / CPP-003 / CPP-006 / CPP-009 / CPP-012 / CPP-016 / CPP-018 / CPP-019、QLT-007
- 前提: [ADR 0001](0001-strictness-is-mechanically-enforced.md)（`planned` を `active` と書かない）

## 文脈

2026-11-15 の規約文書の棚卸し（#1716）で、CODING_RULES の 8 つの規則が現物と食い違っていた。

- 4 つの規則は、機械が既に違反を落とし、反例の証明もあるのに `planned` のままだった。ADR 0001 が禁じるのは未実装の強制を実装済みに見せることだが、逆向きの食い違い（強制しているのに `planned`）も、読む人と席に「守らなくてよい」と読ませる。同じ表の CPP-003 / CPP-004 / CPP-016 は、一部の強制と「守れていない部分」の明記で active にしてある。
- 4 つの規則は、文が禁じている形を現物が正しい理由で使っていた。検査器はその形を通している。文だけを読むと、現物を「直す」方向に誤る。

## 決定

**規則の状態は「対応する検査を実行すると違反を機械が拒否し、その反例がある」ことで決め、強制していない部分は「守れていない部分」に書く。規則の文は、現物の正しい形を場所つきの例外として名指しする。検査器とコードは変えない。**

1. **CPP-002 を active にする。** `eng/targets.cmake` の `-Wswitch-enum -Wcovered-switch-default` が `/WX` で落ち、`eng/prove-gates.py` に反例が 2 本ある。守れていない部分: `if` / `else` で閉じた選択肢を分ける形の分類。
2. **CPP-012 を active にする。** `.clang-tidy` の `readability-function-size`（60 行・入れ子 3・引数 4）と `readability-function-cognitive-complexity`（10）が `WarningsAsErrors: '*'` で落ち、`prove-gates.py` に引数の数の反例が 1 本ある。守れていない部分: 行数・入れ子・認知的複雑度の反例は未登録。「閾値を満たすためだけに割らない」はレビュー事項。
3. **CPP-018 を active にする。** target feature の無い関数の組み込み関数はコンパイルエラーで、`prove-gates.py` に反例が 1 本ある。SIMD ヘッダの置き場は CNF-009。守れていない部分: fallback の存在と、合成ルートが選ぶことはレビュー事項。いまは SIMD の実装が 0 件で `src/core/simd/` も無い。
4. **CPP-019 を active にする。** CNF-012 が印の文法・表・次の行の抑制・定義の関数名・本体 6 行・置き場を見て、`tests/conformance` に反例がある。守れていない部分: 表の引数の数と実際の署名の照合、ループと保持を書かないことはレビュー事項。
5. **CPP-009: `#pragma once` は禁じない。** include guard はこの形の 1 つだけ。禁じるのはそれ以外の `#pragma` で、機械（CNF-003）が拒否するのは抑制の pragma（`warning`・`clang diagnostic`・`GCC diagnostic`・`_Pragma`・`__pragma`）。そのほかの `#pragma`（`comment`・`pack` など）はレビュー事項。
6. **CPP-003: `mutable` は `src/adapters/win32` の同期原始にだけ許す。** const の読み口から `std::mutex` を取る `mutable std::mutex` の形で、スレッドと同期原始を持てる場所（ARC-007 / CPP-013）の中だけ。core / application / ui / app には書かない。字句検査を足すときは、この例外を表に持つ。
7. **CPP-006: `void *` は、取り込んだ C のライブラリのコールバックの境界にも許す。** md4c の C ABI が固定する `void *userdata` / `void *detail` を受ける `src/core/MarkdownParse.cpp` の 1 か所だけ（`md4c.h` を読む唯一の場所・ADR 0151）。受けた直後に型のある値へ写す。
8. **CPP-016: `new` は、非公開のコンストラクタを持つ型を `std::unique_ptr` のコンストラクタの引数として作る形にだけ許す。** `std::make_unique` が非公開のコンストラクタを呼べないため。所有者を伴わない `new` と、`delete` は書かない。

## 強制

- 決定 1〜4 の状態: **active**（上に挙げた道具と反例。QUALITY_GATES の表と CODING_RULES の本文が同じ状態を言う）
- 決定 5: **active**（抑制の pragma は CNF-003）。そのほかの `#pragma` は **planned**（レビュー事項）
- 決定 6・7: **planned**（`mutable` と core / application の `void *` の字句検査は未実装。レビュー事項）
- 決定 8: **planned**（所有者のいない `new` は clang-tidy の `NewDeleteLeaks` が一部を落とす。形の検査はレビュー事項）

## 結果

得られるもの: 規則の状態が現物と合い、席が `planned` を「任意」と読まなくなる。現物の正しい形を、規則の文を読んだ席が消しにいかない。
失うもの・残る穴: 例外が 4 つ増える。場所を名指ししたので、同じ形を別の場所に書くときは ADR が要る。検査器は変えていないので、例外の外の `mutable` と `void *` は今までどおりレビューでしか止まらない。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 4 つの規則を `planned` のままにし、「一部は強制済み」と注記する | 同じ表の CPP-003 / 004 / 016 と扱いが揃わない。状態の語が 2 通りの意味になる |
| 規則の文に合わせて現物を直す（`#pragma once` を include guard のマクロに・Shelf の `mutable` を外す・md4c を包む） | `#pragma once` は 1081 ファイル。`mutable std::mutex` は const の読み口を保つ標準の形。md4c のコールバックの型は上流が固定する（ソースを 1 バイトも変えない・ADR 0151）。直す利益が無い |
| 例外を場所を書かずに一般の形で許す | 「ここで書いたほうが早いから」で同じ形が広がる（ARC-001 / ARC-012） |
