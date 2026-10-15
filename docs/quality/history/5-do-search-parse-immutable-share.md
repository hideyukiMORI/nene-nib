# 5-do — 確定検索の解析結果を不変値として共有する（Issue #376・ADR0102）

出典: docs/quality/gate-proofs.md の節 5-do（2026-10-16 に D50 で移動・本文は移動時のまま）

#367はPR375/CI38046836784でmain bfa1b32へ統合、snapshot-367へ全5563files/131301307bytesをSHA照合して収載し、追加WT/OUT/briefを監査整理済み（acceptance-367/closeout.md）。その製品sourceを基準にC8の確定検索を独立実験とした。ADR0102は実装前の41fae51、通常入力上限の修正aa26973を固定。before共通harnessは07c346c、製品変更はc9d1e17、最後の新unit修正だけを含む検証HEADはb26704e。

規則ARC-001/002/003/004/005/007/008/011、CPP-002/003/004/005/006/007/008/011/012/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004。waiver none。実験時OUTは`D:/NeNeNib/outputs/20261010-search-snapshot`、WTは`D:/NeNeNib/worktrees/376-search-snapshot`。恒久先はwave3の`snapshot-376/files/outputs/20261010-search-snapshot`、統合・監査整理は`acceptance-376`。原rawの旧pathは書き換えない。

### 製品・計測器と対象検証

製品6sourceはVimSearchSnapshotの新hpp/cpp、VimState.hpp、VimStep.cpp、EditorControllerのhpp/cpp。privateなconst Storageが防御copyした文字列・方向・既存parserの成功/失敗を一つだけ所有し、state copyは共有する。明示copyでrvalueの元をnullにせず、text/parsedのconst&&借用を禁止する。n/Nは記憶した解析結果を使い、空検索の再確定は新しい方向で同じfactoryを通す。入力中patternは従来の一時所有、確定済みだけ同期frame内で借用する。coreのlast_search型/APIは変わるが、鍵DTO・record形式・保存schema・parser/照合器・利用者の検索構文は変えない。

旧57条件を保持した型付き表へ別の閉enum8条件を追加。選択処理は時間外、before/after同じharness・入力・区間・oracle。入力a×256は正規SearchLine経路で到達確認し、4096文字の語検索は意味試験だけとする。採否を得るまで入力や閾値を調整しない。

| コマンド / 原証拠 | 直接確認する退行 | 結果 |
| --- | --- | --- |
| `python -m unittest tests.conformance.test_compare_probes.ProbeComparisonTests.test_frame_rows_inputs_and_registry tests.conformance.test_compare_probes.ProbeComparisonTests.test_search_snapshot_inputs_and_registry tests.conformance.test_compare_probes.ProbeComparisonTests.test_search_snapshot_invalid_names` | 旧末尾境界、新8入力/FNV/名前登録、不正名拒否 | 初回新入力の誤分岐を修正、3tests成功。原失敗はjournal、成功raw/python-inputs.* |
| `python OUT/source-proof-harness.py` / `python OUT/source-proof-product.py` | 旧57登録/関数/enum/typed visitor、製品基準・同一計測器・規約flagを保つ | 各exit0。製品差分6source、CMakeは新core/unit/probeの登録だけ。parser/evaluator/scan/keyDTO/記録形式は不変 |
| `pwsh -NoProfile -File OUT/build.ps1 -Phase before-release -Configuration Release -BuildDir build/probes-before-release` | 正規compiler/tidy、新probeの複雑度・依存 | 初回exit1、snapshot_line_equalのcognitive13>10。関心別helperへ分割、07c346cのPhase before-release-splitでexit0。区間/期待値/規約は不変 |
| `python OUT/before-smoke.py` / `python OUT/dispatch-proof.py` | beforeで全oracleが成立し、新enum登録漏れを拒否する | 8条件各1iteration成功。実compile commandの正例0、新case欠落1・想定-Wswitch/constexpr拒否、総合0。旧型不一致/未知alternative負例は同一template/visitorの#367成功を再利用 |
| `cmake --build build/376-debug --target NeNeNib nib_tests nib_perf_probes -j 2`（build.ps1のDebug configure後） / `-Phase after-release -Configuration Release -BuildDir build/probes-after-release` | 新所有型と全直接callerを正規compiler/tidy/sanitizerでbuild | after Release初回0。Debugは新unitの匿名helper参照、optionalの存在保証を各修正しPhase debug-optionalで0。ASan/UBSan/norecover flags確認。各phaseの原失敗/pre/postを保持 |
| `pwsh -NoProfile -File OUT/verify-debug.ps1` | 所有/方向/失敗/空再利用/入力中/表示/タブの契約 | 初回0。検索1487・highlight83・incremental141・tabs291、計2002checks成功。新8Debug probe各1iteration成功。既存135検索fixture＋共有境界6を含む直接scopeだけ |
| after同一exeの新8条件、各iterations1/blocks1 / self-after/results.json | 比較器のmetadata/marks/checksum/oracleの接続 | 全成功。Debug buildと並行した時間値は採否へ不算入 |
| 限定conformance API / raw/scoped-static-fixed・product-static・architecture-unit、`python OUT/check-final-unit-source.py`、`clang-format --dry-run --Werror tests/unit/VimSearchSnapshotTests.cpp` | 追加source/型/関数規約、最新unit修正、実build graph | 6source/11source/新unitとgraphの各0違反。最後のunitだけb26704eを再確認し0、format0。active waiverなし |
| `python eng/symbols.py --build-dir build/probes-after-release --require core application` | core/applicationへの禁止依存 | exit0、2libraries/0違反 |
| `python OUT/run-comparison.py` | 固定3利益・5費用、同一harness/実flags/環境 | 初回0、96process/3840marks/各120組、全8条件成立。欠測/除外/補完/再取得なし。下表 |
| `pwsh -NoProfile -File eng/build-release.ps1 -Ref b26704eff2a60f1b8309c816f93fb4f2ee2a7241` | 通常Release製品の正規build | 初回0、configure2.147s/build263.724s、1451520bytes。normal-release-post.jsonとout/release/b26704e.json |
| `python OUT/gui-search.py <normal-exe> <sha>` / `python OUT/gui-search-mode-supplement.py <normal-exe> <sha>` | 検索状態・通常復帰・tabの本文/実画面 | 原15組0pxだがmode2場面は目視で無効。別固定系列の未確認2場面が0px、実mode到達も確認。原13＋補足2が有効、全40PNG目視、16全文copy一致、4正常終了。下記参照 |
| `python OUT/formal-speed.py --check --executable <normal-exe>` / `python OUT/audit-formal.py` | 状態copyとframeの直接callerに必要な正式速度・有効標本・基準 | 初回各0、原30trial、8指標各5有効/欠測0/退行0/計測不能0。基準/許容不変 |
| `python eng/protected-diff.py --base bfa1b32 --head b26704e` | 既存fixture/基準/許可/規約設定を変更していないこと | exit0、1853→1853、metadata/deleted/changed/added各0、protected changed none。exe/build未指定、45scopes全未測。全件試験成功は主張しない |

before probe SHA `97accf93471ca0b5fc4f08aa767eec09257ed547ce72452deb1371b8f6e83650`、after probe `fa67bd1e223bf009ad5003373d7df63a6db6e7cf6e4222593a783008e5b61998`、通常Release `0c68928eba770573a0b29578e03ec7dfb0552eac17a29b6173effb5b6b920f8b`。GUI beforeは#367のe7fd0c7製品（SHA bb2bc96e92c9509ed5d70b3e25f933288c3b048c55d4dc13728b64121555ac4b）で、src treeはbfa1b32/07c346cと一致。after製品sourceはc9から不変。実compile commandsの共通186件はcommit/buildpathだけが異なり、新core/unitの2件だけ増えた。環境前後のbc8a356f37c68491・i9-10850K/RTX3090/120dpi/HP推奨は一致し、連続監視とはしない。

### 固定比較の利益と費用

| 条件 | before / after中央値µs | 対応比中央値 | 対応差中央値µs | 短縮組/120 | 周期比中央値 |
| --- | ---: | ---: | ---: | ---: | --- |
| 短frame64 | 124 / 122 | .9760000000 | -3 | 114 | .9838709677 / .9758064516 / .976 |
| 長frame64 | 350 / 66 | .1885714286 | -284 | 120 | .1885714286 / .1891117479 / .1880341880 |
| 長n/N64 | 1057 / 777 | .7331107721 | -282.5 | 118 | .7330143541 / .7312248978 / .7337442875 |
| 長履歴・入力200 | 427 / 413.5 | .9715975934 | -12 | 111 | .9763593381 / .9672131148 / .9638344227 |
| 短履歴・入力200 | 412 / 422 | 1.0255744534 | +10.5 | 13 | 1.0218979399 / 1.0291616469 / 1.0267315097 |
| 未検索・入力200 | 421 / 422 | 1.0011682243 | +0.5 | 52 | 1 / .9929659927 / 1.0083978731 |
| 短確定64 | 580.5 / 572 | .9894551520 | -6 | 76 | .9917494500 / 1.0017574692 / .9674002589 |
| 入力中frame64 | 124 / 131 | 1.0569105691 | +7 | 5 | 1.0483870968 / 1.0569105691 / 1.0650406504 |

各20iterations・ABBA3・120対応組。長3条件は対応比/全周期<1、短縮>=90を満たす。費用5条件は比<=1.10、対応差<=50µs（64回）/<=100µs（200入力）を満たす。短履歴入力と入力中frameは全周期で増えた代償である。frameは64回の生成・破棄・checksum、操作は64/200回のapplyを含む局所値で、1回単独やUI応答の改善率ではない。全range/試料はsummary/rawに保存。解析した原子/集合範囲O(A+R)とraw/共有制御領域を通常モード・tabでも最後の所有者まで保持する。実capacity・allocation回数・RSSは未測であり、メモリ削減を主張しない。

### GUIの失敗と限定追加、正式速度

元15場面のordinary-retains-searchとvim-restores-searchは全画素同一でも両側ともVimだった。`:set hlsearch`の報せが残り、既存EditorWindow::click_clientはstatusクリックをCancelCommandだけで終了する。台本がmode_nameの代入を観測とみなした欠陥で、原JSON/PNG/比較は保持する。独立レビューと親のsource確認を経て、未確認2場面だけ別系列へ固定した。Escで報せを閉じ、click一回の後に両groundのRGBをassertしてからmode_nameを更新。通常active[233,84,32]/Vim inactive[74,30,61]と逆を確認した。切替の間に検索を再確定せず、通常で強調なし・復帰で同じa?強調/caret2/1を全画面比較・目視した。

原13有効場面・補足2場面すべて0px。原30＋補足10PNGを親が全目視し、本文・cursor・tab・highlight・閉IME・非遮蔽を確認。全16copyは固定本文と一致し、設定/title/LastInput/foreground/clientを保持して4process正常終了。元visualInspectionPending:trueは原記録として残し、visual-qa.md/gui-audit.jsonへ関連付ける。#365の候補背後の取得、#373の入力元特定は解決していない。

正式中央値msは起動199.4038、window表示31.0439、単発.633、200入力2.188、16MiB open211.4939、16MiB 200入力2.259、一覧5000候補2.007、長行5.898。8指標すべて各5/5、0退行/0計測不能。frameとVimState copyの直接callerに届く6刺激群を一回だけ実行し、whole gateは走らせない。wrapperは正規measure-speedのOUTPUT先と上書き前raw保存だけを変更し、親auditは全30trialの連番/bytes/SHAと集計samples/中央値/基準を監査し、独立読取レビューはrawから全8指標の40sample・中央値・範囲を再計算して一致を確認した。palette5PNGは全同SHA6e2c98fe9859bacde4e70c925fe1ce9cb624a6708d72a171bdcd3074e0f9f1c3、親がf/1of5000/非遮蔽を全目視しformal-visual-qa.mdへ記録。

### 採否と再利用

固定3利益・5費用、所有/検索意味、通常製品の画面同値、正式速度の条件を満たしたため技術受理。設計・実装・全96raw性能・GUI追加方法・最終証拠の独立読取レビューで阻害なし。通常PR/必須CI/main同期/恒久収載/監査整理へ進む。

初期4096入力の到達性指摘と256への取得前修正、Python registry分岐誤り、harness複雑度、新unitのhelper/optional build失敗、static scriptのpath escape構文エラー、GUI2場面の誤判定、補助script生成のPowerShell引用エラー、推測path/読取範囲の失敗と訂正をjournal/rawへ保存。製品/測定/採否を後から緩めず、元記録を上書きしない。新unitはb26704eで対象source/formatを補足したが、c9以後のsrc/perf/eng/CMakeは不変なのでRelease・局所比較・既受理レビューを再利用した。関連入力が不変の成功結果をpush/文書/mergeだけで再実行しない。文書には限定CNF-006と空白確認だけを加える。全字体/DPI/任意資源枯渇、RSS/保持容量、IME候補窓の完全撮影、入力元の連続追跡は未測・別件として残す。
