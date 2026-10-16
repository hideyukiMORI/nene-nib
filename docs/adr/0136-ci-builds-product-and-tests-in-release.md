# ADR 0136 — マージ直前の CI は製品と単体テストを Release でビルドして回す

- 状態: 提案（設計席 2026-10-16。実装は #514 で。初回の run が通った時点で受理）
- 日付: 2026-10-16
- Issue: #514（施主決定 D46・外部講評 2026-10-15）
- 規則: QLT-001 / QLT-002 / QLT-007 / QLT-012 / QLT-014、ADR 0016 / 0021 / 0118

## 文脈

CI の中身は Git 規約・検証記録・差分空白の確認だけで（ADR 0118）、製品のビルドと単体テストは実機の `eng/check.ps1` と PR の検証記録に頼っている。外部講評は「AI が書いた物を AI の検証記録で通す」構造の一番弱い所と指摘し、施主は D46 で「CI で製品をビルドし単体テストを回す。速さのベンチは実機のまま。差分検証の方針は変えず、PR の最終 HEAD の機械の裏付けを足す」と決めた。`measure-speed.yml` が `windows-2022` の runner で `eng/build-release.ps1` の Release を作れることは実測済み（ADR 0016 / 0062）。

## 決定

1. **`check.yml` に job `build-and-test` を足す。** 既存の `check` job と並列に、同じ `inputs.expected_head` を checkout する。起動の契機は今日どおり `eng/merge-pr.py --execute` の `workflow_dispatch` だけで、文書のみの PR では起動しない（ADR 0118）。`check` job は規約と記録に加えて、その前の step で `python eng/test-conformance.py`（conformance の自己テスト・約 12 秒）も回す。#554 のような契約の遅れを merge 前に止めるため（#568）。
2. **構成は Release 1 つ。** `check.ps1` の Release の段と同じ `cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release` で configure し、`cmake --build build-release`（全 target・製品と試験の実行ファイル）→ `ctest --test-dir build-release --output-on-failure --no-tests=error`。警告集合と clang-tidy は Release にも同じものが掛かる（`CMAKE_CXX_CLANG_TIDY` は構成を問わない）ので、迂回路にならない。
3. **Debug（ASan / UBSan）の全件と速さは CI に載せない。** Debug は実機の `check.ps1` の範囲のまま、速さは QLT-014 のとおり実機で測る。CI の `build-and-test` は「PR の最終 HEAD が製品としてビルドでき、単体テストが通る」ことの機械の裏付けであって、検証記録（QLT-012）の代わりではない。
4. **`merge-pr.py` は変えない。** run の conclusion は全 job を含むので、どちらかの job が落ちれば統合は止まる。失敗を自動で再試行しない（ADR 0118）。timeout は job ごと（`check` 5 分のまま・`build-and-test` 45 分）。
5. **道具の版は今日の経路で照合する。** `eng/tool-versions.json` の Python と、`toolchain.ps1` が固定する clang-cl（`measure-speed.yml` と同じ）。runner の道具が版と違えば configure で落ちる。
6. **規則の表記は証明を足してから直す。** QLT-007 / QLT-012 の「機械強制」の行は、初回の run（run ID・所要時間）を gate-proofs に引用した commit で active に直す（D49・ADR 0001）。

## 却下した選択肢

- Debug 構成を CI で回す: ASan / UBSan ＋ clang-tidy の全件は 4 core の runner で長く、合図の 45 分に収まる保証が無い。Debug は実機の gate が守る。
- 別の workflow にする: `merge-pr.py` が 2 本の run を起動・照合する形になり、ADR 0118 の「1 つの run の conclusion」が崩れる。
- required status check に戻す: ADR 0118 が文書免除のために外した。
- `build-release.ps1` で製品だけ作る: 試験の実行ファイルを作らないので単体テストが回らない。配布用の記録（SHA-256・所要時間）は配布の道の持ち物のまま。

## 検証

- この PR 自身の merge の手動実行（`merge-pr.py --execute`）で 2 job が緑。`gh run view <id> --json jobs` の job 名・所要時間を記録に残す。
- 製品の動作は変えないので、実機の製品試験は不要（QLT-012）。

## 限界

- CI の成功は製品の動作・検証記録の真実性を保証しない（ADR 0118 と同じ）。
- runner の GPU は WARP 相当で、`nib_window` のような窓を作る試験は runner の desktop で動く前提。動かなければその試験だけ CI で外す ADR を別に書く（黙って外さない）。
