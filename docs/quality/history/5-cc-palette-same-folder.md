# 5-cc — Ctrl+P の同じフォルダ（Issue #272・ADR 0062）

出典: docs/quality/gate-proofs.md の節 5-cc（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-03。基点 `ddde80b`、製品の実装は `eb5b0e7` → `f6ec509` → `10cab8c`、最後のコメント整形は `edeb38b`。記号 `/`、既知の非テキスト拡張子70件の除外、同じフォルダの非同期列挙、タブ・履歴との重複除外、追加後の選択保持、D35の持続する打ち切り案内を実装した。フォルダの取得は `FolderPort`、受信は `WorkCompleted`、絞り込みは `listed_positions`、開く道は `open_listed` の既存の正典を使う。

### 再利用する製品検証

下記は前の実装席・設計席の成功記録を読み、対象コードと依存の不変性を確認したもの。2026-10-03の再開後に同じ製品テストを再実行してはいない（QLT-001 / QLT-012・ADR 0021）。

| コマンド・対象 | 結果と記録 | 再利用する根拠 |
| --- | --- | --- |
| `cmake --build build`（Debug・ASan / UBSan・clang-tidy） | 成功・警告0。`out/272-rework1-build.log` | `10cab8c` の後の製品差分はコメントだけ |
| `build/nib_tests.exe --background-work` | 89 checks成功。`out/272-rework1-bg.log` | 券・重複・静かな道・選択・打ち切りの実装とテストは不変 |
| `build/nib_tests.exe --command-palette` / `--history` | 356 / 38 checks成功。`out/272-step2-palette.log` / `out/272-step2-history.log` | 記号・拡張子・列の追加・履歴の対象実装は不変。持続表示の変更は上の89 checksが確認 |
| `build/nib_tests.exe`（既存記録） | 19449 checks成功。`out/272-rework1-all.log` | 既存の成功を参照。工程3のPython・文書のために全件を繰り返さない |
| `python eng/conformance.py --build-dir build` / `python eng/symbols.py --build-dir build --require core application` | 各0 violation。`out/272-rework1-conformance.log` / `out/272-rework1-symbols.log` | 製品の依存・シンボル・検査設定は不変。新しい文書の整合は別途確認 |
| `python eng/protected-diff.py --base origin/main --build --allow=--background-work --allow=--command-palette` | 終了0。`out/272-rework1-protected2.log` / `out/protected/10cab8c.json` | fixtures 1853→1853・metadata/deleted/changed/added各0、scope28のうち26不変、palette276→356・background46→89だけ許可。以後の製品・fixture・単体テストは不変 |

券の比較、開く時の読み残し破棄、出どころを条件にした履歴削除の3箇所を外した変異で、`--background-work` が11件失敗した既存記録も確認した（`out/272-step2-mutant.log`）。

再開後の追加確認: `clang-format --dry-run --Werror src/application/EditorController.cpp` は終了0（`out/272-sana-format.log`）。前の `out/272-rework1-format2.log` は成功ではなく、コメントの折り返し4件の指摘であった。`edeb38b` の修正を今回このファイルだけで確認した。既存5コミットの `pwsh -NoProfile -File eng/validate-git.ps1` は成功（`out/272-sana-git-existing.log`）。

### 再利用する実機の確認

前の設計席がhideの了承の後に撮った画と終了記録。125% DPI。撮影は `D:\NeNeNib\scripts\samefolder_frames.py`。新しい計測器と文書は製品の描画を変えないため撮り直さない。

- 小フォルダ: Release `8d0e6ff`、`out/frames-272/s*.png` / `small.json`。タブ2本に続く5候補、既知のバイナリ・隠し・下のフォルダの除外、`/`・日本語の絞り込み、新しいタブへのOpen、消えた候補の1行の知らせと履歴書込なしを確認。終了0。
- 8200ファイル: Release `edeb38b`、`out/frames-272/edeb38b/`。8192件までで打ち切り、`/8191` は候補あり、`/8192` は候補なし。入力・選択後もD35の案内が残り、閉じると消える。終了0。再開後も `b3-last-listed.png` を目視した。
- ReleaseのSHA-256は `F0FEC8C0BD99EE356ED5E9D788605C30E6B1FCFEBB4C50FCBBC7F33FE6E40857`（`out/release/edeb38b.json`）。再開後の `Get-FileHash build/release-edeb38b/NeNeNib.exe -Algorithm SHA256` が一致し、`git diff --exit-code edeb38b -- src CMakeLists.txt eng/targets.cmake eng/tool-versions.json` は終了0。実機の計測にも同じexeを使える。

### 計測器とCIの限定検証

- `python -m unittest discover -s tests/conformance -p test_speed.py -v`: **64 tests / OK**（`out/272-step3-sana-tests-02.log`）。`measured_palette` の最後の入力と次の描画、入力不足・描画欠如、Ctrl+P失敗で文字を送らないこと、本文を変えた試行の欠測、計測区間の前の候補件数の撮影、覆いと撮影失敗、各benchの対象選択、過去6本の記録再利用、部分記録で全基準値を置き換えないこと、基準値なしと欠測の区別を確認。製品のテストは起動しない。
- `python eng/measure-speed.py --help`・`git diff --check`: 終了0（`out/272-step3-sana-help-02.log` / `out/272-step3-sana-diff-check-02.log`）。新しい計測器と引数の入口を確認した。
- 新規 `.github/workflows/measure-speed.yml`: YAML解析・起動条件5ケース・既存pin・単一job・正典Release・失敗即停止・artifact90日・既存check.yml不変を確認（`out/272-ci-sana-structure.log`）。PNG保存追加後のartifact構造も成功（`out/272-ci-sana-artifact-structure.log`）。PowerShellの3ブロックは構文エラー0（`out/272-ci-sana-pwsh-syntax.log`）。通常push/同期では実行せず、明示ラベルか手動で新しい1本だけを測る。

計測の変更は `eng/measure-speed.py`・基準値の説明と `tests/conformance/test_speed.py`。既存の6個の数値と許容25% / floor2msは変えていない。5000ファイルを用意し、Ctrl+P→1.5秒待ち→暖機f→0の1文字から描画までを測る。待機時間だけでは候補の到着は証明できないため、各試行の0直前のPNGで件数を確認してから採用する。

実機はhideの了承後、Release `edeb38b` で `python eng/measure-speed.py --check --bench key-to-frame-palette-5000 --executable build/release-edeb38b/NeNeNib.exe` を実行。5試行すべて有効、中央値 **2.630 ms**（2.526〜2.715 ms・`out/speed/2026-10-03T05-15-16Z.json`・`out/272-sana-speed-palette.log`）。各試行の `palette-2026-10-03T05-15-*.png` 5枚すべてに暖機fと件数 **1 / 5000** が写り、設計サナが目視した。`--adopt --bench key-to-frame-palette-5000 --values out/speed/2026-10-03T05-15-16Z.json` で、この1本だけを実機 `bc8a356f37c68491` の新しい基準値として採用した（`out/272-sana-adopt.log`）。1024件のbatchと8192件の上限は、今回の5000件の入力速度と前の8200件の実機確認を根拠に維持する。

共通の `command_message()` が全frameで呼ばれるため、同じexeで `--check --bench key-to-frame-single` も実行し、5試行すべて有効、中央値 **1.016 ms**（0.829〜1.614 ms）、1 bench checked・0 regression・0 unmeasurable（`out/speed/2026-10-03T05-15-53Z.json`・`out/272-sana-speed-single.log`）。CIの新しい測定と基準値の採用は下記のとおり完了。それ以外の起動・大容量ファイルを開く処理は不変で、#271の成功（`out/speed/2026-10-02T15-08-39Z.json`・5-cb）を再利用する。全件の製品テストと全性能ベンチを工程のために繰り返さない。

自己レビュー: FR-006 / D33〜D35 / ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-010 / CPP-002 / CPP-004 / CPP-011 / QLT-001 / QLT-012 / QLT-013 / QLT-014。公開の値とportの境界を保ち、時刻・ファイル・スレッドを中核へ持ち込まず、閉じた分岐を網羅し、閾値・除外・依存を弱めていない。製品の保存スキーマ変更なし。Waivers: none。

残るリスク: 8192件は拡張子・重複除外前の上限であり、画像多数のフォルダではテキスト候補が少なくても後ろが打ち切られる。パスの比較は既存の `same_file`（大文字小文字を無視する文字列比較）であり、`/` と `\` の混在を同一視しない既存の制約がある。入れ子のメッセージループ中の完了合図と、停止したネットワークI/Oの解除は未確認。候補なしの字が左へ寄る既存の表示は本件で修正していない。

### CI の新しい基準値（2026-10-03）

[Actions run 37099450677](https://github.com/hideyukiMORI/nene-nib/actions/runs/37099450677) は成功。正典 `eng/build-release.ps1 -Ref HEAD` が `05792e5` の Release をビルドし、同じ host `e7a87d5b6ac1e14b`（AMD EPYC 7763 / Microsoft Hyper-V Video / 96 DPI）で `python eng/measure-speed.py --check --bench key-to-frame-palette-5000 --executable build/release-05792e5/NeNeNib.exe` を独立に3回実行した。新しい値はまだ基準が無いので記録だけと明示し、5試行ずつ15試行すべて有効・欠測0。通常のPR必須checkからは起動しない。

| 記録（UTC） | 中央値 ms | 5試行の最小〜最大 ms |
| --- | --- | --- |
| `2026-10-03T05-24-43Z.json` | 4.097 | 3.938〜4.337 |
| `2026-10-03T05-24-58Z.json` | 4.038 | 3.878〜4.085 |
| `2026-10-03T05-25-14Z.json` | 4.179 | 4.047〜4.428 |

artifact `palette-speed-records`（保持90日）には上記JSON・各試行の測定直前のPNG15枚・Releaseの記録がある。取得先は `D:/NeNeNib/evidence/272-ci-37099450677/`、ログは `out/272-sana-ci-speed.log`。Release SHA-256は `74089D9528CE3680778DDD736F6AF1407AA6FFFC709E2589DEF5B1B571B2FFA2`。PNG15枚すべてのSHA-256が `0E034FE4653BBDFF921256209503EE4A832F6A32C21AA87F24807B74774BE387` と一致し、設計サナが代表画像で暖機fと **1 / 5000** を目視した。従って15試行すべての測定直前の表示に5000候補が揃っている。

ADR 0016 / 0062決定19に従い、3中央値の中央値 **4.097 ms** をこの指紋の新ベンチだけの基準値として採用。minimumMs / maximumMsは3中央値の幅 **4.038〜4.179 ms**。出典・日付・集計方法を基準値のnoteへ追記した。既存6本の値・既存metadata・25% / floor2msの許容を機械比較し不変（`out/272-sana-ci-adopt.log`）。3つの保存記録それぞれへ `python eng/measure-speed.py --check --bench key-to-frame-palette-5000 --values <記録>` を適用し、各 **1 bench checked・0 regression・0 unmeasurable**（`out/272-sana-ci-reference-check.log`）。これは採用したデータと比較器の整合確認であり、新たな性能測定ではない。

CI後の変更は基準値と文書だけ。製品・計測器・テスト・workflowは不変であり、成功済みの64試験・実機2本・CI3回を統合時にも再利用する。文書のリンク・規則ID・強制状態の整合とGit/PR規約・空白だけを最終確認する。

### main の文書更新の取り込み

統合前に `origin/main` の `c328e0d`（PR #276・前日の日報と引き継ぎ）へrebaseした。競合はCLAUDE.mdとcurrent.mdの説明・進捗だけで、mainのワーカーの説明と履歴を残して今回の状態へ揃えた。受理・測定時の番号は証拠の出典として書き換えず残す。対応は `eb5b0e7→4d2babe`・`f6ec509→7fb002d`・`8d0e6ff→f8d7b19`・`10cab8c→3682ad3`・`edeb38b→2787f72`・`05792e5→6018734`。`git diff --exit-code 001debf -- src tests eng .github CMakeLists.txt` は終了0で、製品・試験・計測・workflowがrebase前後で同一と確認した。性能や製品テストを繰り返す変更ではない。

rebase後の最終文書確認: conformance.document_checksは0 violation（out/272-sana-docs-rebase.log）、git diff --checkとPR本文のgit-conventions.py --pr-bodyは終了0。確認対象は文書の相対リンク・規則ID・強制状態と差分の空白・検証記録の存在。
