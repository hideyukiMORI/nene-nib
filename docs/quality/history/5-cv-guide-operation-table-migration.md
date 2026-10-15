# 5-cv — 操作表の案内と旧設定を残す移行（Issue #304・ADR 0079・D42）

出典: docs/quality/gate-proofs.md の節 5-cv（2026-10-16 に D50 で移動・本文は移動時のまま）

[PR #313](https://github.com/hideyukiMORI/nene-nib/pull/313)。hide の今回の分担指示により、設計・受理・画面と性能確認は設計サナ（Astra）、実装と検証器は実装サナ（SOL）、文書・整理の調査はLUNA、独立レビューは実装していないSOLが担当。デザインリナのCLIレビューも採用済みHintA/Bの読み取りに使用した。固定委任を次の作業の既定にはしない。

**変更と確認する退行。** 操作表から本文3行とステータス2組の表示値を作り、applicationが表示文脈、coreが排他的な配置を決める。未編集の無題にB、入力後/保存済み空ファイルには余白があるときA。入力面・通知・録画・IME中は隠す。設定は既存persist経路で保存成功後だけ更新する。v2を優先し、不在時だけ厳密なv1を読む。最初の実変更でv2へ保存し、旧v1は書き換えない。両snapshot/lockで移行競合を拒否する。旧設定/本文/undoの保持、表示の重なり、F1の共通keycap抽出、追加UI書式とframeの費用を確認した。

| 対象・退行 | 実行したコマンド | 結果 |
| --- | --- | --- |
| 設定型・署名・新frameの直接呼出しと描画資源 | 固定toolchainでDebugの `cmake --build build --target NeNeNib nib_tests nib_adapter_tests nib_theme_tests`、UI工程は `--target NeNeNib nib_tests --parallel 4` | clang-tidyを含め成功。最終の試験修正はnib_testsの対象TUだけ再ビルド |
| Ex保存成功/失敗・同値・他設定変更時のguide保持・本文/選択/undo不変 | `build/nib_tests.exe --ex-settings` | 209 checks成功 |
| 共通候補とCtrl+Pのguide設定 | `build/nib_tests.exe --command-palette` | 375 checks成功 |
| v1/v2の版・全必須鍵・不正値・往復 | `build/nib_adapter_tests.exe --settings-codec` | 34 checks成功 |
| 両側競合・両lock・旧版不変・壊れたv2の保存拒否・失敗後の再保存・移行後のv1非参照 | `build/nib_adapter_tests.exe --settings` | 227 checks成功 |
| 直接変更した利用者テーマ設定の復元と保存拒否 | `build/nib_theme_tests.exe --catalog` | 302 checks成功 |
| 表示条件・配置・未編集/undo/タブ・本文/面IME・設定失敗 | `build/nib_tests.exe --guide` | 519 checks成功。96/120/192 DPI、360/640/960 DIP、200/560 DIP、8/40 ptの36組と物理1pxの境界 |
| 短縮名追加で既存F1の検索/名前/鍵を変えない | `build/nib_tests.exe --operations` | 516 checks成功 |
| profile整理が両設定を残す | `python -m unittest discover -s tests/conformance -p test_frame_capture.py -k ProfileReset -v` | 2 tests成功。変更した4検証道具の構文とv2出力/v1入力も確認 |
| 追加型・実依存・OS境界・正準整形 | `python eng/conformance.py --build-dir build`、`python eng/symbols.py --build-dir build --require core application`、変更C++の `clang-format --dry-run --Werror`、`git diff --check` | 0違反、symbolsは2 libraries。すべて終了0 |

初期のlint拒否（Ex/試験mainの複雑度、新試験のoptional/ネスト）は分割・明示処理で修正し、抑制は足していない。最初のguide試験は120DPIの中央配置で6/519失敗した。228DIP=285pxを偶数幅へ置くと左右に1px差が生じるため、製品を変えず試験へ整数丸めを明記し、519成功を確認した。失敗ログも保持した。

**Releaseと画。** `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` → `c89cf00`、SHA-256 `94654A18679A27E49C3038BC39568E997C11DEBC986F7D9EC80606F67245A0BC`、1430528 bytes。構成22.068秒・ビルド174.258秒。専用profile、実機120DPIで次を実行した。

- `python -B D:/NeNeNib/scripts/304-guide-verify.py --mode guide --name candidate-guide --executable <Release>` → 終了0、43場面。設計サナが全場面の画像（9テーマの原寸切出しと25場面の一覧、重要場面は原寸全画面）を確認。B/Aの排他、入力/undo/新規タブ/保存済み空、各入力状態、8/40pt、360DIP、低い200DIPのAへの切替、guide保存・再起動を受理。v1不変と起動だけではv2を作らないことも自動確認。
- 同じ道具の `--mode palette-reference` を基準Release `4d84334` と候補で各1回、`--mode palette-compare` で比較。dark13.5/light13.5/dark24の3場面はF1領域0画素差。基準は `git diff --exit-code 4d84334 be455a6 -- src CMakeLists.txt eng/targets.cmake eng/tool-versions.json` 終了0で製品同一を確認した。24ptで既存キー表示が切れる問題は基準画像で見つけ、#312へ分離した。
- `python -B D:/NeNeNib/scripts/304-guide-ime.py --name candidate-real-ime --executable <Release>` → 終了0。実日本語IME/SendInputの6原寸画像を確認。未編集Bと入力済みAが変換中に消え、Esc取消でそれぞれ戻る。変換下線は0→189→0/0→195→0画素、本文inkも元へ復帰。IMEopenは0→0に復元。合成したIMEメッセージではない。

**性能は直接影響する3本だけ。** `python -B eng/measure-speed.py --check --bench <以下の名前> --executable <Release>`。機械 `bc8a356f37c68491`、120DPI。起動の追加書式/本文案内と、案内を持つframe/ステータス保持の短行・長行の費用を確認する。ファイル読込や大きな候補一覧の経路は変更していないので他5本は実行していない。

| bench | 5有効試行の中央値（範囲）ms | out/speedの記録 |
| --- | --- | --- |
| startup-first-frame | 235.064（224.677〜242.036） | `2026-10-07T15-22-09Z.json` |
| key-to-frame-single | 0.549（0.452〜0.743） | `2026-10-07T15-23-03Z.json` |
| key-to-frame-single-long-line | 5.079（4.686〜5.510） | `2026-10-07T15-24-37Z.json` |

各1 bench checked・0 regression・0 unmeasurable。基準値/許容は不変。測定前CPU14〜30%、測定後43%で、ビルドのプロセスは0。静かな機械だったとは主張しない。長行の最初の呼出しは親が通常打鍵のexec終了を待たずに開始したため、準備のexeコピーでWinError32になった（測定前）。このログも保存し、通常打鍵の正常終了を確認してから初めて長行を測定した。成功した測定は繰り返していない。

**独立レビューと再利用。** 設定 `be455a6..7cbba2f` とUI `7cbba2f..c89cf00` を読み取りで別々に確認し、must-fixなし。設定codec/adapter/Exの規則と試験は後続工程で不変なので再利用し、追加frame/描画はguide/operations/実機で確認した。以後は文書だけで、`git diff --exit-code c89cf00 HEAD -- src tests eng CMakeLists.txt` の一致をもってReady/mergeで再利用する。全件検証は実行していない。

恒久記録: `D:/NeNeNib/evidence/304-operation-guide/out/`（成功・失敗ログ40ファイル、manifestで元のSHA-256と照合）、画像・独立レビュー・設計レビューは `D:/NeNeNib/outputs/304-guide/`。実行ファイルとRelease JSONは同フォルダの `release-c89cf00/`。画の判定は `visual-review.md`、撮影時点の `record.json` のpendingとは別に明記した。

未確認/限界: 96/192 DPIは純粋配置試験だけでモニター跨ぎは実機未検証。専用processはdriverで停止し、自然終了の再検証はしていない。lockを無視する外部編集との比較後の競合は従来どおり保証外。Solarizedのmutedは淡いが判読でき、色トークンは変えていない（コントラスト規格の適合試験ではない）。既存#309/#312は別件。

適用: #304 / ADR 0079 / D42 / FR-018 / ARC-001 / ARC-004 / ARC-008 / ARC-009 / ARC-010 / ARC-011 / CPP-002 / CPP-004 / CPP-011 / CPP-012 / CPP-017 / QLT-001 / QLT-002 / QLT-012 / QLT-013 / QLT-014 / GIT-003 / GIT-004。保存schemaはv2追加、v1は保持。Waivers: none。
