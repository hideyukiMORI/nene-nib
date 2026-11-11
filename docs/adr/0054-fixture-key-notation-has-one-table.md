# ADR 0054 — fixture の鍵の記法の表は oracle の 1 つで、C++ の表はそこから作る生成物

- 状態: 受理（設計席 2026-09-29・Issue #229。施主の指示「1，2 を続けて」の 2）
- 日付: 2026-09-29
- Issue: #229
- 影響する規則: ARC-001 / ARC-012 / CPP-002 / QLT-001 / QLT-012 / QLT-013 / CNF-010
- 前提: [ADR 0012](0012-vim-engine-first-slice-and-oracle-fixtures.md)（fixture は oracle が生成・決定 7）・[ADR 0042](0042-unit-tests-split-by-scope.md)（テストの足場 `VimTestSupport`）・[ADR 0048](0048-named-registers-as-one-text-table.md)（決定 9・`<NL>` を 2 か所に足した）・[ADR 0049](0049-space-backspace-wrap-motions.md)（決定 5・`<Space>` と矢印を 2 か所に足した）・[ADR 0038](0038-model-per-seat-and-scripted-preparation.md)（同じ手順を 2 回踏んだら 3 回目は機械に）

## 文脈

fixture の `keys` は `<Esc>` `<CR>` `<NL>` `<Space>` `<Left>` などの名前で特殊鍵を書く。この記法の表が 2 か所にある。

- `eng/vim-oracle.py` の `KEY_NAMES`（名前 → Vim の二重引用符つき文字列の書き方・19 行）。oracle が本物の Vim へ鍵を流すときと、`q` の拒否の状態機械が鍵を 1 つずつ読むときに使う。
- `tests/unit/VimTestSupport.hpp` の `vim_key_names`（名前 → Nib の `VimKey`・19 行）。fixture を Nib で再生するときに使う。

2 つは「fixture の書き方」という 1 つの約束の両端で、ARC-012 の既知の複製として扱ってきた。#193（`<NL>`）と #200（`<Space>` と矢印）で同じ 2 か所を 2 回触った。片方だけ足すと、Vim は特殊鍵として読み Nib は `<` `L` `e` `f` `t` `>` の 6 文字として読むので、fixture が黙って違う意味で落ちるか、悪ければ通る。

oracle には部分再生成の再利用の証明がある（`measurement_sources_match`）: 測定の領域（`literal()` より上）の定数と関数の AST が `--reuse-ref` の版と同じときだけ、選んでいない fixture の行を再利用する。`KEY_NAMES` はその領域にある。生成物 `tests/vim/VimFixtures.hpp` と入力 `fixtures.json` の一致は CNF-010 が守る。

## 決定

**記法の表は `eng/vim-oracle.py` の測定の領域の 1 つだけにし、1 行が「名前・Vim の書き方・Nib の鍵」の 3 つを持つ。C++ の表はそこから作る生成物 `tests/vim/VimKeyNames.hpp` で、手で書かない。生成物が表と一致することは CNF-010 が守る。**

1. **表（正本）**: `eng/vim-oracle.py` の測定の領域に `KEY_TABLE`（行の列）。1 行は名前（`<Esc>`）・Vim の二重引用符つき文字列の書き方（`\<Esc>`）・Nib の鍵。Nib の鍵は、特殊鍵なら `VimSpecialKey` の列挙子の名前（`escape`）、文字ならコードポイント（`<NL>` は 0x0A・`<Space>` は 0x20）。既存の `KEY_NAMES`（名前 → Vim の書き方）は `KEY_TABLE` から作る導出の値にして、今の使い手は変えない。
2. **表の検査（oracle が読み込むとき）**: 名前は重複しない。名前は `<` で始まり `>` で終わる。ある名前が別の名前の接頭辞にならない。違反は oracle が例外で止まる。
3. **生成物**: `tests/vim/VimKeyNames.hpp`（先頭に生成物の印の 1 行・`// clang-format off`）。`inline constexpr std::array<VimKeyName, N> vim_key_names` を表の行の順で持つ。形を決めるのは oracle の `key_names_header()` の 1 か所（Vim を要らない純関数）。`--regenerate` が `VimFixtures.hpp` と一緒に書き、`--key-names` は Vim なしでこの生成物だけを書く。
4. **型の置き場**: `VimNamedKey` と `VimKeyName` は手書きの `tests/vim/VimKeyName.hpp`（`VimFixture.hpp` の隣）へ移す。生成物がそれを include する。`tests/unit/VimTestSupport.hpp` は生成物を include し、手書きの `vim_key_names` の配列を消す。
5. **列挙子の名前の誤りは機械が落とす**: 生成物は `VimSpecialKey::<名前>` をそのまま書くので、表の名前が列挙子に無ければコンパイルが落ちる。
6. **CNF-010 に足す**: 保存されている `tests/vim/VimKeyNames.hpp` のバイト列が `key_names_header()` の出力と一致すること。無い・違う、のいずれも CNF-010。SHA の 1 行ではなく全文の一致を見る（入力が Python の表で、Vim を要らずに作り直せるため）。正例と反例は `tests/conformance`。`docs/QUALITY_GATES.md` の CNF-010 の文言と `docs/quality/gate-proofs.md` の反例を足す。
7. **再利用の証明はそのまま効く**: `KEY_TABLE` は測定の領域の定数なので、表を変えると `measurement_ast` が変わり、部分再生成は「測定コードが変わった」で拒む（今の `KEY_NAMES` と同じ性質）。記法を足す Issue は、oracle の変更だけを先に commit してそれを `--reuse-ref` にする（引き継ぎの約束 35）。
8. **今回の移行の証明**: 表の形を変えるだけで記法の意味は変えない。全件を 1 回再生成し（`--regenerate`・`--only` なし）、`tests/vim/VimFixtures.hpp` がバイト一致することを確かめる。全件が 20 分を超えるなら、約束 35 の手順（oracle の変更を先に commit → `--reuse-ref`）で代え、その旨を PR に書く。
9. **範囲の外**: fixture の記法に新しい名前を足すこと・`fixtures.json` の形（CNF-011）・oracle の `q` の拒否の状態機械の規則。

## 強制

- 生成物と表の一致: **active**（CNF-010・`eng/conformance.py` の `key_names_checks`・正例と反例は `tests/conformance/test_conformance.py`）。
- 列挙子の名前: **active**（コンパイル。`arrow_leftward` に書き換えると `no member named 'arrow_leftward'` で落ちることを実測・gate-proofs 5-bn）。
- 表の検査（重複・形・接頭辞）: **active**（oracle の読み込みの `key_names_of`・反例は `tests/conformance/test_vim_oracle.py`）。
- 表を変えたときの部分再生成の拒否: **active**（既存の `measurement_sources_match`・`tests/conformance/test_vim_oracle.py`）。

## 結果

得られるもの: 記法を足す場所が 1 つになる。片方だけ足す誤りが起きない。C++ の表の書き忘れと列挙子の名前の誤りを機械が落とす。
失うもの・残る穴: 生成物が 1 つ増える（`VimKeyNames.hpp`）。C++ のテストの足場が Python の表に依存する（生成物を通して）。表の正本がテストの側ではなく道具の側（`eng/`）にある。`VimSpecialKey` に値を足しても記法が無ければ fixture から打てないことは、機械は言わない（記法を足すのは fixture が要るとき）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| JSON の表（`tests/vim/key-names.json`）を正本にして Python と C++ の両方が読む | 表が測定の領域の外へ出るので、表を変えても `measurement_ast` が変わらず、部分再生成の再利用の証明が記法の変更を見逃す。証明に JSON の中身を足す変更が別に要る |
| C++ の表を正本にして Python が解析する | oracle が C++ のヘッダを字句で読むことになる。コンパイラの知っている形を Python で写すのは壊れやすい |
| 2 か所のまま、一致を検査するだけにする（名前の集合の比較） | 複製は残る。名前の集合が同じでも、名前 → 鍵の対応の誤り（`<Left>` を `arrow_right` に写す）は検査が見ない |
| 生成物を `VimFixtures.hpp` の中に入れる | `VimFixtures.hpp` は Vim が要る生成物で、SHA の 1 行で守る。記法の表は Vim を要らずに全文を作り直せるので、守り方が違う |

## 追補（2026-11-10・#1326）fixture を窓の道でも再生する

文脈: fixture の再生（`verify_vim_fixture`）は入口を通らない `press_vim_keys` で鍵を流す。入口 `EditorController::apply` の処理（`map` の入口・出口の寄せ `keep_vim_character`・合図の後始末）が本文やキャレットを Vim と変えても fixture は落ちない。PR #1323 のレビュー（M1・F2）で、INSERT の寄せが窓の道でだけ本文を Vim と変えていたのに fixture が通った。

決定:

1. fixture の記法の鍵を、窓（`src/ui/win32/EditorWindowKeys.cpp`）が送るのと同じ意図に写して 1 つずつ `apply` し、窓の描画と同じく 1 鍵ごとに `frame` を読む（`tests/unit/VimFixtureWindowPathTests.cpp`）。入力行が開いていれば `CommandText` / `SubmitCommand` / `CancelCommand` / `EditCommand`（Tab は補完・上下は履歴）/ `PasteCommand`（`<C-v>`）、閉じていれば `VimKeyPress`。窓が Vim の鍵より先に引く操作・ブックマーク・タブの表に fixture の鍵の和音が Vim モードで無いことを `operation_for` と `tab_command_for` で確かめる。
2. 期待は engine の道の結果（本文・キャレット・無名レジスタと種類・窓の先頭と先頭の画面行・横位置・半画面の行数・モード）。窓の道は鍵を失敗で打ち切らないので、打った鍵の道（`press_vim_key`）と比べる。失敗で残りを捨てない fixture では打った鍵の道と `:normal!` の道（`press_vim_keys`）が一致し、その値は `verify_vim_fixture` が oracle と突き合わせている。失敗で残りを捨てる fixture（#230）も件ごとに打った鍵の道と比べ、その件数は数えて出すだけで固定の数では断言しない（main `c67cf493` の上で全件 148 + 折り返す窓の 8）。
3. 窓の準備（`viewport`・`settings` の `scrolloff`・`register` 欄のマクロ・折り返す窓）は engine の道と同じ部品（`arrange_vim_viewport`・`store_vim_fixture_macro`）で置くので、どの種類も飛ばさない。窓が Vim の鍵として送らない鍵（Tab 以外の制御文字・INSERT の Ctrl+V / A / X / E / Y）を含む fixture は理由ごとに数えて飛ばす。今の fixture ではどちらも 0 件で、飛ばす道は `<NL>` と INSERT の `<C-a>` の小さな反例で 1 件ずつ踏む。
4. 件数は fixture の数との関係で断言する: 選んだ件数 == 再生した件数（全件では `vim_fixtures.size()`）・飛ばし 0・食い違い 0。絶対の件数は書かないので、ほかの PR が fixture を足してもこの検査は古くならない（#1356 のレビュー M2）。
5. 既定実行は並びの 40 件に 1 件（先頭から・決定的・`--vim-fixture-window-path`）と、名前が `window-path-` で始まる fixture の全部（変異で落ちる fixture を並びの位置に頼らず入れる・#1356 のレビュー L4）、全件は名指しの scope `--vim-window-path-all`（`nib_unit_scopes` が回す）。Debug の所要は全件 約 205 s（3,944 件・engine の道の 2 本と窓の道の 3 回ずつ）、10 件に 1 件で約 26 s（施主の機械・ほかの席と並べた値）。#1356 の乗せ替えで 40 件に 1 件にし、main `c67cf493` の上で全件 3,750 + 332 件が約 224 s、既定実行の 104 + 9 件が約 9 s（Debug・施主の機械）。
7. 窓の鍵の振り分けの写し（`command_line_send` と `vim_send`）は `EditorWindowKeys.cpp` と `VimSpecialKeys.hpp` の複製のまま置く。module `unit_tests` は core と application にだけ依存し（`eng/architecture.json`）、ui/win32 の表 `vim_specials` を include できない。代わりに写しの `switch` は `VimSpecialKey` の全部の値を `default` なしで並べ、値が増えると試験がコンパイルで落ちる（CPP-002）。`<Del>` は閉じていれば `VimKeyPress{delete_key}`、入力行では `EditCommand::erase`（#1325）。
6. 窓の道でだけ違う形（INSERT の挿入点が Vim の 1 文字の途中に立つ）は既存の fixture に 1 件も無かったので、`VimWindowPathTests` の実測の 10 行を oracle の fixture `window-path-1326-*` として足した（oracle の変更は無し・`--regenerate --only window-path-1326-`）。

強制: **active**。変異で確かめた。`keep_vim_character` の INSERT の早い return を `break` にし、`VimKeyPress` の早い return も消す（PR #1323 の最初の形）と、全件は `window-path-1326-` の 5 件・既定実行の 10 件に 1 件は 1 件（`insert-ctrl-r-before-a-lone-mark`）で落ちる。#1356 の乗せ替えの後（40 件に 1 件と `window-path-` の全部）は既定実行も全件も同じ 5 件で落ちる。INSERT の return だけを `break` にした変異は、窓の鍵が `VimKeyPress` の早い return で寄せを通らないので落ちない（窓の鍵の道では観測できない形）。

却下した選択肢:

| 選択肢 | 却下の理由 |
| --- | --- |
| 窓の道でも失敗で残りの鍵を捨てる | 入口の外から鍵の失敗は見えない（返り値は `EditorDelivery`）。見えるようにするには製品の口が要る。窓は実際に捨てないので、打った鍵の道と比べるほうが窓の事実に合う |
| 結合文字を含む fixture（約 149 件）だけを再生する | 入口の処理は結合文字に限らない（`map` の入口・入力行の写し）。全件でも 10 件に 1 件なら既定実行は約 26 s で済む |
