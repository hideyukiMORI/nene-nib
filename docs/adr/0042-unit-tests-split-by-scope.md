# ADR 0042 — 単体テストは scope ごとの翻訳単位に分け、`VimStep.cpp` は純関数の切り出しだけで縮める

- 状態: 受理（設計席 2026-09-23・Issue #160。hide 未確認・引き継ぎの「次の順」の 2 番目。決定 1〜3 の本数は #160 の実測で補正済み）
- 日付: 2026-09-23
- Issue: #160
- 影響する規則: CPP-008 / CPP-011 / CNF-002 / QLT-001 / QLT-012 / ARC-001 / ARC-012
- 前提: [ADR 0031](0031-vim-text-objects-as-one-range-function.md)（範囲関数 1 本を 1 ファイルに切り出した前例）・[ADR 0039](0039-implementation-seat-per-step-and-small-tool-output.md)（実装席は必要な範囲だけ読む）・[ADR 0041](0041-vim-incsearch-preview-outside-the-engine.md)（`vim_find_match` を `VimSearch.cpp` に閉じた前例）・`eng/protected-diff.py`（Issue #130・scope ごとの checks 数を `tests/unit/NibTests.cpp` の `scopes` 表から読む）

## 文脈

`tests/unit/NibTests.cpp` は 8224 行（#148 の後・main `3863fbe`。棚卸しの時点では 7805 行）、`src/core/VimStep.cpp` は 3655 行（2026-09-23）。どちらも今のゲートには落ちていない（CPP-011 / CPP-012 / CNF-002 はすべて **planned**・`docs/QUALITY_GATES.md`）。困っているのは 2 つ。(1) 実装席が 1 つの scope を足すたびに 7805 行のファイルの末尾に関数と表の行を足すので、同じ日の複数の枝が末尾で衝突する（09-23 に 3 回）。(2) ADR 0039 の「必要な範囲だけ読む」が、scope の位置を grep で探してから範囲を読む 2 段になり、席の文脈を太らせる。

Sonnet の棚卸し（scratchpad `probe-split.md`）の要点: `NibTests.cpp` は 148–628 が基盤（`expect` / 計数・using）、630–2687 が Vim 以外、2704–2929 が Vim の共有ヘルパ（`struct VimKeyName` と 7 関数）、2930–7706 が scope ごとに固まった `verify_vim_*` 約 120 関数、末尾が `contracts`（11 件）と `scopes`（#148 の後は 21 件）の 2 表。`VimStep.cpp` は無名名前空間 1 本（81–3635）に約 150 関数、公開は `vim_step` / `vim_cancelled_input` / `vim_interrupted` の 3 つだけ。`VimStep.cpp` をテーマで割ると無名名前空間の関数を非公開の内部ヘッダで共有することになり、CPP-008「翻訳単位に閉じるものは無名名前空間に置く」と正面から衝突する。既存の切り出し（`VimWordMotion` / `VimTextObjectRange` / `VimSearch` など 8 本）はすべて「公開ヘッダ 1 本＋純関数」の形で、内部関数を割った前例は無い。

## 決定

**単体テストは「1 scope = 1 翻訳単位」に分け、登録の表と `main` は `NibTests.cpp` に残す。`VimStep.cpp` はテーマで割らず、公開ヘッダを持つ純関数の切り出しだけで縮める（1 Issue に 1 本・その Issue が触る帯だけ）。ファイル長の閾値は入れない（CNF-002 は planned のまま）。**

1. **共有の基盤はヘッダ**: `tests/unit/TestSupport.hpp` / `.cpp`（`expect` / `failure_count` / `check_count`）と `tests/unit/VimTestSupport.hpp` / `.cpp`（`VimKeyName` 1 型と `vim_keys_of` / `vim_body` / `command_key` / `vim_replay` / `empty_vim_state` / `vim_fixture_position` / `arrange_vim_viewport`・`vim_key_names` は `inline constexpr`）。テスト専用のコードも CPP-011（1 ファイル 1 型）に従うので、複数の翻訳単位が使う型（`Editing` と `Scripted*` の 6 つ）はそれぞれ 1 型 1 ヘッダ（`Editing.hpp` / `ScriptedAppearance.hpp` など・実測で 8 本）。名前空間は `nenenib::tests`（無名名前空間は翻訳単位の外へ出せない・CPP-008 の「テストのためだけに公開しない」は製品コードの話で、テストの基盤はテストの公開面）。
2. **scope ごとの翻訳単位**: `scopes` 表の 21 件（Vim 以外の scope も同じ）それぞれを `tests/unit/<Scope>Tests.cpp`（例: `VimSearchTests.cpp`・`VimDotTests.cpp`・`DisplayLineTests.cpp`）に移す。13〜40 行の小さい scope も 1 本にする（表の 1 行 = 1 ファイルという対応を崩さない）。翻訳単位をまたいで呼ぶ関数（scope の入口 `verify_<scope>_scope()`・`contracts` 表が呼ぶ `verify_<scope>_contracts()`・既定実行の入口・2 つの scope が共有する検証）はすべて `tests/unit/Scopes.hpp` の 1 本に宣言し、それ以外は各 `.cpp` の無名名前空間に閉じる（実測 65 本）。1 つの関数が 2 つの scope から呼ばれている場合はその関数を最初に呼ぶ scope のファイルに置き、もう一方は `Scopes.hpp` 経由で呼ぶ（経路を 2 本にしない・ARC-001）。**関数の中身と呼ぶ順は変えない**（scope ごとの checks 数が不変であることが受理条件）。
3. **scope に属さない既定実行のテスト**は module で分ける: `tests/unit/CoreTests.cpp`（text / caret / history / palette / theme / layout / font / encoding）・`tests/unit/ApplicationTests.cpp`（scroll / settings / controller / document）・`tests/unit/VimEngineTests.cpp`（Vim エンジンの既定実行・fixture の再生）。scope を持つものは決定 2 が優先で、ここには入れない。`main` の呼ぶ順は今のまま。
4. **`NibTests.cpp` に残すもの**: `main`・`report`・`verify_selected_scope` の `scopes` 表・`verify_vim_scope_contracts` の `contracts` 表。`eng/protected-diff.py` の `UNIT_TESTS` と `SCOPES_BLOCK` は変えない（表がここに残るので読める）。新しい scope を足すときは「ファイル 1 本＋ `Scopes.hpp` の 2 行＋表の 1 行」になり、末尾の衝突は表の 1 行だけになる。
5. **CMake**: `nenenib_target(nib_tests unit_tests EXECUTABLE tests/unit/NibTests.cpp tests/unit/TestSupport.cpp tests/unit/CoreTests.cpp ... )` と並べる（実測 27 本）（`eng/targets.cmake` は各ソースが `tests/unit` 配下であることだけを検査する・`eng/architecture.json` は変えない）。`add_test(NAME nib_unit ...)` はそのまま。
6. **`VimStep.cpp`**: テーマ別の分割はしない。縮めるのは「公開ヘッダ 1 本を持つ純関数を 1 Issue に 1 本」だけで、その Issue が触る帯から取る（ADR 0031 / 0041 の形）。最初の候補は鍵の表（`action_for` / `motion_for` / `text_object_for` など 253–462・`VimKeyTable`）だが本 ADR では起票せず、次に鍵の表を触る Issue で行う。内部ヘッダ（無名名前空間の関数を複数の `.cpp` で共有するためのヘッダ）は作らない。
7. **移す手は 1 度きりのスクリプト**（実装席の scratchpad の python・行範囲で切って貼る）で行い、手で写さない。スクリプトは `eng/` に入れない（2 回目が無い・ADR 0038 決定 5 の対象外）。

## 強制

- scope ごとの checks 数と fixture の不変: **active**（`python eng/protected-diff.py --base origin/main --build` が `scopes 20 / same 20`・fixtures 1339 -> 1339・`--allow` 無し）。既定の `nib_tests` の総数も不変。
- 1 scope = 1 翻訳単位・表は `NibTests.cpp`・関数の中身は不変: **planned**（レビュー事項。`Scopes.hpp` の宣言と `scopes` 表の対応を目で確かめる。#160 では実体 383 個のうち 382 個が一字一句同じことを 1 度きりのスクリプトで照合した）。
- ファイル長の閾値: 入れない（CNF-002 は planned のまま・QLT-010 の「閾値は ADR 相当」に該当しないことを明記）。

## 結果

得られるもの: 実装席が 1 scope の 150〜400 行だけを読んで足せる。末尾の衝突が表の 1 行になる。`NibTests.cpp` は 137 行になった（最大は `CoreTests.cpp` の 1387 行）。
失うもの・残る穴: テストの基盤がヘッダとして公開面を持つ（`nenenib::tests`・`Scopes.hpp` の宣言と `scopes` 表の対応は機械では守られない）。`VimStep.cpp` は当面 3655 行のまま（席は grep で帯を当ててから範囲を読む今の手が続く）。翻訳単位が 27 本になり、`nib_tests` のクリーンビルドは 87.8 s → 94.0 s と約 7 % 遅い（翻訳単位ごとに製品ヘッダと clang-tidy を読み直す。差分ビルドは 1 scope の 1 本だけになるので速くなる見込み・未測）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `VimStep.cpp` をテーマで 4 本または 15 本に割る | 無名名前空間の約 150 関数を内部ヘッダで共有することになり CPP-008 と衝突する。粗く割っても最大 1580 行が残り分割の意味が薄い |
| ファイル長の上限を CNF-002 として今 active にする | 閾値の導入は ADR 相当（QLT-010）で、いま落ちている検査が無い。分割の動機は席の読む範囲と衝突であり、閾値ではない |
| `scopes` 表も各ファイルへ移し登録を分散する | `eng/protected-diff.py` が 1 ファイルの 1 表を読む形（#130）を壊す。表が 1 か所にあるほうが scope の一覧として読める |
| `tests/unit/vim/` のサブディレクトリに置く | 平らな `tests/unit/` で `Vim` 接頭辞のほうが `eng/targets.cmake` の検査と `architecture.json` を触らずに済む |
