# 5-dn — 表示行一覧の既存範囲を一度予約する（Issue #367・ADR0100）

出典: docs/quality/gate-proofs.md の節 5-dn（2026-10-16 に D50 で移動・本文は移動時のまま）

停止時の5modified/2untrackedをSHA照合・保全して再開。#372はPR374/main decd4f1で保留記録を統合、恒久収載・監査整理済み。#367は同mainの文書を同期し、先行ADR0100のdispatch設計を0d05b7aへ固定した。製品はbefore ffa57afからafter e7fd0c7へ、EditorController::visible_linesのvector直後に非空rangeのreserveを一度足す四行だけ。値・順序・所有・first/last・loop・line_view・検索・選択は保持する。

規則ARC-001/002/003/004/007/008/011、CPP-003/004/007/008/011/012/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004。公開API/schema/基準/許容/flags/抑制の変更なし、waiver none。OUTは実験時`D:/NeNeNib/outputs/20261010-frame-rows`、Rは`OUT/resume-1911`。恒久収載先はwave3の`snapshot-367/files/outputs/20261010-frame-rows`、統合・監査整理は`acceptance-367`に記録する。原raw内の旧pathは書き換えない。

### 一経路の計測器と対象検証

旧53enum/4要素登録/処理本体を保持し、新4条件だけ別の閉じたFrameRowsWorkloadとした。型付きProbeDispatchRowを一つの57行表へ載せ、ProbeSelectionのvariant訪問も同表を選ぶ。各enumの網羅switch、constexprのindex/名前/関数/size検査、型付きfactory/runnerが閉集合を強制する。選択は区間外。CPP-012の60行/nesting3を緩めず、各型を同名headerへ置きCPP-011も守った。

| コマンド / 原証拠 | 直接確認する退行 | 結果 |
| --- | --- | --- |
| `python -m unittest tests.conformance.test_compare_probes.ProbeComparisonTests.test_stage_six_frame_inputs tests.conformance.test_compare_probes.ProbeComparisonTests.test_frame_rows_inputs_and_registry tests.conformance.test_compare_probes.ProbeComparisonTests.test_frame_rows_invalid_names` | 新4入力/FNV/registry・旧53末尾境界・不正名 | 停止前の初回3tests成功を再利用。Python2ファイルは保全稿とbytes一致。原証拠implementation/raw/stage1-tooltests.* |
| `python R/source-proof-harness.py` / nesting-fixの訂正後proof | 旧53登録/enum/本体/網羅switch、製品不変、CMakeは登録だけ | exit0。constexpr付与以外の旧switch保持、旧関数block SHA242a520c2a1f5aa072923081c0bac8ccaab2306efab020f92a688aa1db266c35 |
| `pwsh -NoProfile -File OUT/implementation/build.ps1 -Phase before-release -Configuration Release -BuildDir build/probes-before-release` | 正規compiler/tidyと追加dispatch | 初回exit1、workload_ofのlambdaがnesting4。関数抽出で修正、ffa57afのPhase before-release-nestingでexit0。初回raw保持、規約変更なし |
| `python R/dispatch-proof.py` | 網羅性と型一致がcompilerで拒否されること | 正例exit0。旧case欠落/new case欠落/未知variant alternative/型違いfactoryの4負例各exit1・想定diagnostic、総合exit0。正規compile_commandsからD専用copyへsource/Fo/Fdだけ変更 |
| `python OUT/implementation/source-proof.py product ffa57af` | 製品変更が指定四行だけ、同じharness | exit0。他srcとtests/eng/CMake/tidy/formatは同一 |
| `cmake --build build/367-debug --target NeNeNib nib_tests nib_perf_probes -j 2`（build.ps1のDebug configure後） / `-Phase after-release -Configuration Release -BuildDir build/probes-after-release` | 変更sourceと直接callerを正規設定でbuild | 各初回exit0。Debug3targets、Releaseはprobe。ASan/UBSan/norecoverの実flags確認、全source/commit/clean/tools/flagsは各pre/post JSON |
| `pwsh -NoProfile -File OUT/implementation/verify.ps1 -Stage debug` | frame/scroll/selection/所有と新4probe oracle | 初回exit0、`nib_tests --application`16009checks、4条件各1iteration成功。frame-selectionを重複実行しない |
| `pwsh -NoProfile -File OUT/implementation/verify.ps1 -Stage self` | 同一exe/metadata/marks/新入力の接続 | 初回exit0、4条件各iterations1/blocks1。性能利益には使わない |
| `python OUT/implementation/scoped-static.py product build/367-debug` / `python eng/symbols.py --build-dir build/367-debug --require application core` | 追加source登録/依存graph、予約が禁止OS依存を導入しないこと | 各exit0、source/graph0違反、2libraries/0違反、waiver none。coreはapplicationの依存解決対象 |
| `python R/run-comparison.py` | 64frame生成破棄の利益/空短文費用 | 初回exit0、4条件48process/1920marks、各120組。下表。欠測/補完/除外/再試行なし |
| `pwsh -NoProfile -File eng/build-release.ps1 -Ref e7fd0c7932ee45c7f1ed5ded4389b509dfce47e6` | 通常製品とprobe以外のbuild | 初回exit0、configure1.791s/build266.313s、1450496bytes。out/release/e7fd0c7.json |
| `python R/gui-rows.py` → `python R/complete-gui-empty.py` → `python R/continue-gui-rows.py` | 空/2行/多行scroll/EOF/selectionの全文と全client画素 | 初回固定待ち失敗を保持し同じ保存の完了を別観測、未実施場面のみ続行。前後7場面すべて0px、全14PNG目視。下記に限界 |
| `python R/formal-speed.py --check --executable <WT>/build/release-e7fd0c7/NeNeNib.exe` / `python R/audit-formal.py` | 共通frame callerの正式速度と有効標本/基準 | 初回各exit0、8指標各5/5、0退行/0計測不能。原30trial保持。下表 |
| `python eng/protected-diff.py --base decd4f1 --head e7fd0c7` | 既存fixture/基準/許可/規約設定の差分 | exit0、fixtures1853→1853、metadata/deleted/changed/added各0、protected files changed none。out/protected/e7fd0c7.json。exe/buildを指定せず45scopes全未測、件数同一や全test成功は主張しない |

before probe SHA `bb33dd4e26e01a9313a37f76b2592e62ad50e61937b36cbe6fc037bbd25eaf16`、after `5d139269216a61028766f54df75d0256d7b0da8a4b0176bf35d2eabe62019c6b`、通常製品Release `bb2bc96e92c9509ed5d70b3e25f933288c3b048c55d4dc13728b64121555ac4b`。Debugの3exeと691sourceのSHA、実flagsはimplementationのcapture JSON。独立レビューで全185compile commandはcommit/build先以外一致、CMakeCacheはbuild先だけ、Ninja残差は構成入力一覧だけと確認した。comparison前後のfingerprintbc8a356f37c68491、i9-10850K/RTX3090/120dpi/HP推奨は一致。snapshotであり連続監視ではない。

### 固定比較の利益と費用

| 条件 | before / after中央値µs | 対応比中央値 | 対応差中央値µs | 短縮組/120 | cycle比中央値 |
| --- | ---: | ---: | ---: | ---: | --- |
| 空・1行 | 44 / 43 | 0.9772727273 | -1 | 97 | 費用条件内 |
| 2行 | 69 / 59 | 0.8550724638 | -10 | 119 | 費用条件内 |
| 30行 | 520 / 361 | 0.6928231821 | -160 | 120 | .6920105385 / .6875006934 / .6951587765 |
| 120行 | 2244 / 1409 | 0.6322791141 | -822 | 120 | .6178293503 / .6090123959 / .6476575320 |

全条件20iterations・ABBA3・各120組。空/2行は事前の差≤50µsかつ比≤1.10、30/120行は対応比/全cycle<1かつ短縮≥90組を満たす。比は対応比の中央値で、表の二中央値を割った値とは別。測定するのは64回の生成・破棄と固定checksumの合計。UI応答の31%/37%改善や純1frame時間、実確保回数/RSSは主張しない。各oracle/metadata/全試料/原marks/集計はfixed-comparisonに保持する。

### 直接GUIと正式速度

通常製品のbeforeはsnapshot-362のrelease-9218124（受理済みsrcはmain decd4f1と同じ）、afterは上記e7fd0c7。同じ空/2/120行混在CRLF、同じ設定/1280×800で、empty先頭、short先頭/EOF、full先頭/中間/EOF/visual選択を比較した。before7とafter空の撮影後、after空の最終:w別pathコピーは.75秒後に見つからずFileNotFoundError。原failed record/PNG/scriptを保存した。

同じcopyは後の観測で存在。PID/HWND/実exe/入力tick/保存title/foreground/寸法/原文書/設定を確認し、追加save/再撮影なしで0bytes全文を確認して正常終了した。非同期PostMessage後の原子配置完了待ち不足と整合するがmtimeだけで所要時間を断定しない。未実施after short/fullだけは一度送信後のdestination出現待ちを加えて取得。すべての全文初終copyと正常終了を確認し、7scene0px・全14PNG目視で文字/行番号/選択/caret/状態表示が同一。元失敗を一括成功へ書き換えない。2560点検査だけを全面遮蔽の証明にせず、#365のIME未解決とは分けた。

正式8指標は6刺激groupで測った。EditorWindow::start_rendering/paintがframe→visible_linesへ到達するため、空起動、空の単/連続入力、大容量初回/編集、一覧paint、長行payloadの各境界が直接影響する。window-shownは同じ起動に付随し追加刺激なし。旧tint非到達による再利用は不可。全gate/無関係な86場面は実行していない。

| 正式指標 | 中央値ms | 有効/予定 |
| --- | ---: | ---: |
| startup-first-frame | 197.8746 | 5/5 |
| startup-window-shown | 32.2822 | 5/5 |
| key-to-frame-single | 0.618 | 5/5 |
| key-to-frame-burst-200 | 2.106 | 5/5 |
| open-large-file-16mib | 212.0635 | 5/5 |
| key-to-frame-burst-200-16mib | 2.247 | 5/5 |
| key-to-frame-palette-5000 | 2.071 | 5/5 |
| key-to-frame-single-long-line | 5.860 | 5/5 |

正式結果はformal-speed/canonical-output/2026-10-10T10-51-31Z.json。既存reference/toleranceを変更せず8項目すべて合格、missing0。起動/長行には基準中央値を上回る値もあり、全指標の高速化とはしない。wrapperは未変更measure-speed.mainへ委譲し、OUTPUT先と上書き前raw保存だけ変更。30trialの連番/bytes/SHAを照合。paletteの5PNGは全同SHA0d2f023dea73a5b14798f44c356b144de91a918b7e0130170a61b29b71a1da98で、親が暖機f/1・5000候補/遮蔽なしをすべて目視した。

### 採否・全失敗・残る限界

固定局所条件、通常製品の同値、正式速度を満たしたため技術受理。独立読取レビューは設計/差分/負例/全48process・1920marksを監査しP0/P1/P2なし。最終レビューも7組PNG/本文/原失敗と別完了観測、正式30rawから8指標の全samples/中央値/range、コピーexe/道具識別を再確認し阻害なし。R/acceptance-review.mdに記録。GUI比較JSONのvisualInspectionPending:trueは撮影直後の原記録として保持し、完了済みvisual-qa.mdを関連付ける。通常PR/必須CI/main同期/恒久収載/監査整理へ進む。

初回before buildのnesting違反、英語件名のhook拒否（commit未作成）、ADR索引merge競合と両方保持、初回GUI固定待ち失敗、誤ったpath/glob/読取範囲と訂正をjournal/rawへ保存。成功した検証は関連source/test/依存/環境が不変なのでpush/文書/mergeのために反復しない。文書には限定CNF-006/空白だけを追加する。任意viewport/資源枯渇/全字体/DPIの網羅、RSS/実capacity、GPU完了時間は未測。#365/#373は別の未解決として残す。
