# 5-cd — 明示ブックマークとCtrl+Pの共有候補（Issue #278・ADR 0063）

出典: docs/quality/gate-proofs.md の節 5-cd（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-03、設計・実装・検証をサナが単体で実施。基点は #272 統合後の `2096565`。FR-006 / FR-010 と D36・D37 に従い、通常 Ctrl+D / Vim Ctrl+Shift+D の明示操作、`*` の候補、消えた登録の保持を追加した。保存先は独立した `bookmarks.v1`。既存の `FilePort::write` の原子的な置換と `same_file` を使い、名前のあるタブも `open_listed` → `open_document` の経路へ揃える。無題は既存の `tabnext N`。中核へOSのI/Oを持ち込まない。

### 対象検証と根拠

| 対象・退行の理由 | コマンド | 結果・記録 |
| --- | --- | --- |
| 新しい型・port・閉じた分岐とキー経路、未検査optional・複雑度 | `cmake --build build --target NeNeNib nib_tests nib_bookmark_tests`（`eng/toolchain.ps1`、Debug・clang-tidy・ASan/UBSan） | 成功。`out/278-sana-build05.log`。テストの期待値調整後は `--target nib_tests` で成功（`out/278-sana-build06.log`） |
| 更新・上限・キー対応・I/Oの時機・失敗時の保護・Vim待ち・IME・各出どころの重複 | `build/nib_tests.exe --bookmarks` | 40 checks成功、`out/278-bookmarks.log` |
| 名前ありタブのopen経路・一覧の順と確定 | `build/nib_tests.exe --tabs` | 291 checks成功、`out/278-tabs.log` |
| 新しいscope分岐・印・候補の保持・名前あり候補のパス | `build/nib_tests.exe --command-palette` | 356 checks成功、`out/278-command-palette.log` |
| ブックマークと分けた履歴の記録・終了時のI/O | `build/nib_tests.exe --history` | 38 checks成功、`out/278-history.log` |
| 既表示のファイルを除くフォルダの列・選択維持・静かな完了合図 | `build/nib_tests.exe --background-work` | 89 checks成功、`out/278-background-work.log` |
| 新schemaの正例・反例・上限・絶対パス・本物の保存の往復と再作成、読取専用の原本を失敗時に保護 | `ctest --test-dir build -R '^nib_bookmarks$' --output-on-failure` | 1 test / 1245 checks / 0 failures。`out/278-adapter.log`、`build/Testing/Temporary/LastTest.log` |
| 専用profileの初期化にbookmarksを追加し、本物のprofileを消さない | `python tests/conformance/test_frame_capture.py ProfileReset -v`（TEMP/TMP=`D:/NeNeNib/evidence/temp`） | 2 tests / OK。製品は起動しない |
| 新型とモジュール境界・規則・整形 | `python eng/conformance.py --build-dir build`、変更したC++への `clang-format --dry-run --Werror` | 違反0・終了0。`out/278-conformance.log` / `out/278-format.log` |
| core/applicationへOS依存を追加しない | `python eng/symbols.py --build-dir build --require core application` | 2 libraries / 0 violations、`out/278-symbols.log` |

最初のビルドでcodecと候補作成の複雑度を検出し、責務で分割した。続くoptional参照と新規テストの型指定を修正。旧タブ候補の `tabnext N` の期待値をパスによるopenへ更新し、IMEは変換を受ける通常モードでbodyとpaletteを測った。最終の上記実行はすべて成功。関係しないVim fixtureの全再生・全性能ベンチは実行していない。

再利用: 成功後に製品・関連テスト・依存・環境が変わらない検証はpush・レビュー・mergeでも再利用する。対象の期待値だけが変わった再ビルド後には `--bookmarks` / `--tabs` のみ実行し、成功済みの候補・履歴・裏の仕事・adapterは繰り返していない。実機表示・キーと絞り込みの新しい性能測定、保護対象の静的差分はこの時点では未実施。

規則: ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-010 / ARC-011 / CPP-002 / CPP-004 / CPP-005 / CPP-011 / QLT-001 / QLT-012 / QLT-013 / QLT-014。閾値・除外・依存追加なし。Waivers: none。

残る制約: 既存のsame_fileは区切り文字混在を同一視しない。複数窓が同時にread-modify-writeしたときの直列化は範囲外。ほかの窓の変更は次に面を開くときに読み直す。登録解除後も履歴や同じフォルダとして出ることは正常。


### Releaseと保護対象の静的比較

`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` は `764fa20` で成功（`out/278-release.log` / `out/release/764fa20.json`）。`build/release-764fa20/NeNeNib.exe`、1,283,072 bytes、SHA-256 `0328FFAD811C0E77F2166C4002995C090358A9D08FDC99E49BF7EEA085DBF693`。このコマンドでは起動していない。

`python eng/protected-diff.py --base 2096565 --head 764fa20` は終了0（`out/278-protected.log` / `out/protected/764fa20.json`）。実出力は `fixtures 1853 -> 1853 / metadata 0 / deleted 0 / changed 0 / added 0`、`files changed none`。保護対象はperf-reference・symbol-allowlist・conformance-rules・SettingsCodec。既存scopeの件数比較は28本すべて「未測」、新規 `--bookmarks` もこの道具では未測。比較用exeも `--build` も渡していないためで、全scope不変の証拠とは扱わない。対象scopeの成否と件数は上の限定実行の記録が正。


### 実機の限定確認と性能

hideが2026-10-03 15:37 JSTに前面操作を了承した後、`python D:/NeNeNib/scripts/bookmarks-278.py --executable C:/Users/info/WORKS/NeNeNib/build/release-764fa20/NeNeNib.exe` を実行。終了0、11枚の画とJSONを `out/frames-278/` に保存（`out/278-window.log`）。専用profileと文書は `D:/NeNeNib/evidence/bookmarks-278/`。サナが11枚すべてを目視し、以下を受理した。

- 通常Ctrl+Dは長押しの4 key-downでも登録1回。版と登録内容を実ファイルで照合した。
- 空のCtrl+Pで5種類の記号の案内が収まり、登録済みのタブは「タブ・ブックマーク」の1行。`*`では同じ登録が1件だけ。
- Vim Ctrl+Dは行1から行5へ半画面移動し、登録のbytesは不変。Ctrl+Shift+Dで解除・再登録できる。
- 再起動後、開いた登録と閉じた登録が同じ `*` に並ぶ。存在しないgone.txtは「開けませんでした」の1行と登録保持、一覧で解除してもタブは増えない。
- 閉じたbeta.txtは新しいタブで開き、開いているalpha.txtは既存タブへ切り替わる。
- version=9の登録ファイルへ付け外しを試みてもbytesを変えず、読めなかったことを1行知らせる。

同じexeで `python eng/measure-speed.py --check --bench key-to-frame-palette-5000 --executable build/release-764fa20/NeNeNib.exe` を実行。5試行すべて有効、中央値 **2.708ms**、最小2.614・最大2.831ms（samples 2.614 / 2.708 / 2.831 / 2.658 / 2.797）。実機 `bc8a356f37c68491` の既存基準値2.630msへ比較して **1 bench checked / 0 regressions / 0 unmeasurable**。基準値は変更していない。記録は `out/278-speed-palette.log` / `out/speed/2026-10-03T06-38-44Z.json`。

測定直前のPNG5枚 `palette-2026-10-03T06-38-*.png` すべてを読み、暖機fと **1 / 5000** の表示を確認した。5枚のSHA-256もすべて `5873681DFF7C111BA6876363CC5D835A9E70F0CE908FB405C4967A42BB12B5E9` で一致。打鍵区間で5000候補が揃っていることを証明する。15:38 JSTにhideへ前面操作終了を通知済み。

自己レビュー: 既存の原子的な保存、パス同一性、開く経路、閉じたenum、状態の所有を維持した。追加されたI/OはBookmarkPortのread/writeに限定し、通常の起動/開く/保存/入力/切替/終了では呼ばないことを契約で確認。完全同時の複数窓の競合はADRで明示した範囲外。最終製品は実測した `764fa20` と同一で、以後の文書変更では成功済みの検証を再実行しない。PR #279に同じ検証と再利用根拠を記録する。
