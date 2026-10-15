# 5-dq — 一時の文書表示値をframeへ所有移管する（Issue #380・ADR0104）

出典: docs/quality/gate-proofs.md の節 5-dq（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-10 22:00〜22:36 JST。前件#378はPR379/必須Check38053915117でmain6195a1547db60ee4aa0f96b5d7c84957dc559398へ統合。snapshot-378の505records/120023951bytesをSHA照合収載し、追加WT/OUTを21:59:59に監査整理済み。続いてframeの一時DocumentViewの再コピーを独立調査した。

対象はARC-001/004/005/007/008/011、CPP-002/004/006/007/008/011/012/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004、ADR0010/0056/0059/0075/0082/0084/0104。waiver none。公開API/保存schema/設定/基準/許容は不変。

取得前ADR d697e73、同一harness b6de9327bfbd63df94c8271471fdf3b2e735a330、製品b82b0ed68724b641ef6d1c5bb72231a9baf8eb30。EditorController.cppだけの意味変更で、既存delivery()の所有DocumentViewをtab_viewsがcopyした後、encoding enumをconst localへ退避してdocumentをstd::moveし、statusは退避値を使う。移動後objectを読まず、共通表示値をstateから再生成する第二経路を作らない。整形によるaggregate再配置を含む全file token比較で期待する3意味変更だけと証明した。

WT=`D:/NeNeNib/worktrees/380-frame-document`、OUT=`D:/NeNeNib/outputs/20261010-frame-document`、branch=`refactor/380-frame-document`。恒久先は`D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-380`、統合/整理証拠は`acceptance-380`。rawの元pathを保持し、収載後はsnapshotの`files/outputs/20261010-frame-document/`から読む。

### 変更から選んだ検証

| コマンド・範囲 | 回帰リスク・選定理由 | 実結果 |
| --- | --- | --- |
| `python OUT/source-proof.py`、`python -m unittest tests.conformance.test_compare_probes.ProbeComparisonTests.test_frame_document_inputs_and_registry tests.conformance.test_compare_probes.ProbeComparisonTests.test_frame_document_invalid_names tests.conformance.test_compare_probes.ProbeComparisonTests.test_preview_caret_inputs_and_registry` | 新4登録のpath/hash/不正名と旧末尾7登録、旧72の区間/selectorを壊さない | source証明0、Python3tests初回0。old72入力/既存関数/typed表の旧行/selector template不変 |
| 変更6sourceの`clang-format --dry-run --Werror`、`python OUT/scoped-static.py` | 計測器の網羅性/型/規約 | 違反0、waiver none。旧関数ブロックSHA db8d5d4703142f479ba48dd76a0d9cb2155bdc49addc241ba9be0b21c580b50c |
| `python OUT/dispatch-proof.py`、`python OUT/audit-dispatch.py` | 新FrameDocumentWorkload欠落をconstexprとswitchで拒否 | 実compiler正例0/short_saved欠落1を一度取得。初回checkerは旧long_thirty regexで1、原本を保全し診断読み取り監査0。compiler再実行なし |
| `pwsh -NoProfile -File OUT/build.ps1 -Phase before-release -Configuration Release -BuildDir build/probes-before-release`、同after-release/build/probes-after-release、`python OUT/smoke-before.py` | 同一正規最適化/計測器/入力で前後実行 | 両Release初回0、変更前新4各1iteration0。初回の短い時刻値を利益に使わない |
| `pwsh -NoProfile -File OUT/build.ps1 -Phase debug -Configuration Debug -BuildDir build/380-debug -Targets @('nib_perf_probes','nib_tests','NeNeNib')`、`pwsh -NoProfile -File OUT/verify-debug.ps1` | 所有移管後の保持snapshot、document全欄、frame caller、tab先行copy/切替 | 正規tidy+ASan/UBSan/no-recover、--application16009/--tabs291の16300checks、新4各1iteration全0。重複unit追加なし |
| `python OUT/source-proof-product.py`、`python OUT/check-product.py`、`python eng/symbols.py --build-dir build/probes-after-release --require core application` | 予定したmove/enum退避だけ、他製品/計測器不変、依存逆流なし | 期待変換一致、source/実CMake graph違反0、2libs違反0 |
| `python -X utf8 OUT/run-comparison.py` | 2named利益、保存失敗/無題の費用と前後値同値 | `fixed-comparison-observed`に4条件各120対応組、全事前条件成立。初回preflight失敗は下記 |
| `pwsh -NoProfile -File ./eng/build-release.ps1 -Ref b82b0ed`、`python OUT/capture.py normal-release-post build/release-b82b0ed` | 実アプリの正規Releaseで表示と正式速度を確認 | 初回0、out/release/b82b0ed.json、703source/実flags/tools/SHAを保存 |
| `python -X utf8 OUT/gui-document.py <normal-exe> <sha>`、`python -X utf8 OUT/audit-gui.py` | 文書title/path/save/encoding、変更済み印、無題/tab/実mode、本文を保つ | 初回0、8組全client0px、16全文copy、132checkpoint、8実modeRGB、2正常終了。親24PNG全目視 |
| `python -X utf8 OUT/formal-speed.py --check --bench key-to-frame-single --executable <normal-exe>`、`python -X utf8 OUT/audit-formal.py` | body量/検索によらない共通DocumentView移管を普通入力のframeで代表確認 | 初回5有効/欠測0/退行0/計測不能0、全5原marksから再計算一致。他7指標は今回未測 |
| `python eng/protected-diff.py --base 6195a15 --head b82b0ed` | fixture/基準/規約/許可を変えない | exit0、1853→1853、metadata/追加/削除/変更0、protected none。exe/build未指定、45scope未測で全件試験成功ではない |

before probe SHA `578f1c13226c8b43854248a24ea3e54ec0eb1ef8d3b88907bdb76645911ed3f3`、after `b4623b0b023e34150b25b0323c900ad29c1f5f4599225ce4465cf8677c4c71d3`、通常Release `d83401cc64fe51459356875bc12211d4bd690977a649dcea0afec8aedcb04d7d`。GUI beforeは#378通常Release13655cb（SHA8b9465c118c370670a2f1e4845f533b2359779672e52bda228f6cfeffb26675a）で、srcはmain6195/beforeと一致。前後190compile commands（C++189/resource1）はbuildpath/commit metadataだけ相違、703sourceとharness/入力/toolsを保存・照合。指紋bc8a356f37c68491、i9-10850K/RTX3090/120dpi/HP推奨電源の前後snapshot一致。環境の連続監視を主張しない。

### 固定比較

FrameDocumentWorkloadの閉enum4行を既存72の後へ同じtyped表/TimingPortで追加。input metadataはpath UTF-8 bytesで31/241/241/0bytes、FNVは3165114504819470625 / 3259355916674798828 / 3259355916674798828 / 14695981039346656037。shortは`C:\nib-probe\frame-document.txt`、longは同prefix+`segment\`×20+`a`×64+`.txt`。

short本文はa日・Tab・🖋・U+0001・LF・endの14bytes/UTF-8、longはBOM+CRLFで物理18/本文15bytes。failedは先頭xを加えcaretを1/1へ戻し、access_deniedの保存失敗を保持（本文16/title72bytes）。untitledは空0bytes/1行/無題6bytes/pathなし/UTF-8/CRLF。全条件ordinary/viewport2、tab1/active0、caret1/1。準備と前後の全文/display/map/spans/current/document全5欄/status3/tabs/delivery/documents・保有snapshotの独立literal照合を区間外、const frame生成・破棄256回と長さchecksumを区間内に置く。checksum14592/82176/83200/3328。

| 条件 | before / after中央値µs | 対応比中央値 | 対応差中央値µs | 短縮/120 | cycle比中央値 |
| --- | ---: | ---: | ---: | ---: | --- |
| frame-document-short-saved-256 | 291.0 / 272.0 | 0.9381443299 | -18.5 | 118 | 0.7599431818 / 0.9426115467 / 0.9510489510 |
| frame-document-long-saved-256 | 332.0 / 314.0 | 0.9452053525 | -19.0 | 114 | 0.9489540497 / 0.9410065553 / 0.9488044670 |
| frame-document-long-failed-256 | 283.0 / 264.0 | 0.9327380952 | -19.0 | 105 | 0.9458746898 / 0.9308025536 / 0.9283154122 |
| frame-document-untitled-256 | 120.0 / 118.0 | 0.9942196532 | -1.0 | 62 | 0.9916316527 / 1.0000000000 / 0.9943473957 |

short/long savedは比<1・全cycle<1・短縮>=90/120を満たす。failed/untitledは比<=1.10・差<=50µs/256framesを満たす。無題cycle2=1、非短縮組も残し、全組/全cycleの改善とはしない。20iterations/ABBA3を一度、除外/穴埋めなし。局所区間の比をUI全体や単独1frameの改善率へ置換しない。

### GUI・正式速度・原失敗

GUIの取得前レビューで`:b 1`はpalette queryでtab切替にならないP2を発見。未実行元script `gui-document-pre-review.py`と元plan（SHA882a9cce721574d1ebc91f7535e04d4401890841e409087d2dce08cd4236b0c0）を保全、2箇所だけ`:tabnext 1`へ訂正して実bytesSHA dd6108f78b1454d370664f6849ab947782eda59dfe20e18e3fe18813b3a2097fへ再固定。取得前独立レビューでP2解消、誤台本のGUI取得は0。

固定8場面は長saved/modified/resaved、短saved/modified/resaved、無題、shortへtab復帰。本文をundo+saveで元へ戻し、無題は撮影後にcopy先を初pathとして付与して閉じる。全1280×800 RGB8対0px、16本文copyがliteralへ一致、各66checkpointでLastInput/foreground/title/client、実RGBでVim→通常→Vim→通常の到達を確認。親は全16scene+8mode PNGを目視しvisual-qa.mdへ記録した。正常終了2、元input/設定不変。原visualInspectionPending:trueは上書きせず後続目視記録と結び付ける。閉IMEだけで#365/#373解決とはしない。

固定比較の初回起動はpowercfgのUTF-8出力を既定cp932で読んでenvironment-before保存前に失敗。原script `run-comparison-initial.py`、raw/fixed-comparison stderr、原directoryの空process logsとpowercfg生bytesを保持。probe loop前で性能試料0。差分は出力先をfixed-comparison-observedへ替える一行だけ、-X utf8を指定して未取得系列を一度取得した。採否/入力/製品/計測器/区間を変更せず、初回成功とも取得済みデータの再試行とも扱わない。

新enum負例の実compilerは正例0/short_saved欠落1だったが、dispatch-proofのregexが旧long_thirtyで初回checker1。元script/summary/rawを保存し、audit-dispatchが全source変換/SHAとshort_saved診断を読んで0、compiler再実行なし。Debug台本の旧suffixは未実行で訂正。推測path/glob/名称の読取失敗・広すぎる出力切れ・通常Release console一部切れもjournalへ残し、全stdout保存と誤記しない。正本release metadata/ninja logを収載する。

正式singleは[.584,.651,.640,.629,.628]ms、中央値.629、基準.906/上限2.906。5/5有効、欠測0/退行0/計測不能0。正規measure-speedの出力先と上書き前raw退避だけのwrapperを使い、全5rawの第2input→直後frame、202input、連番/hashを監査して一致。body量/検索に依存しないdocument共通欄の移管のため単発普通入力を選択。他7指標は今回未測、以前の成功を今回実測へ置換しない。計測中はbuild/test/GUI/hash/copyを並走しない。

### 採否・再利用・残る限界

2利益/2費用、直接契約、画面/本文同値、正式singleが成立。独立最終レビューでP0/P1/P2なし、技術受理。 独立読取レビューは全48process/1920marks/960durations/480pairs、前後703source/190commands、GUI全RGB/16本文/132checkpoint/8実modeと代表5画像、正式全5rawを確認して一致した。文書8件の限定CNF-006/waiverと空白を確認し、通常PR/必須CI/main同期/恒久収載/監査整理へ進む。関連source/tests/依存/環境がb82b0edから不変の成功は文書commit/push/review/mergeだけで再実行しない。

本件は元26候補と別の追加実験で20採用/2実験不採用/4未実験を変更しない。C8入力中parse保持、R8、IO6〜IO8は未実験、#365/#373は未解決。全path長/全字体/DPI/資源枯渇/RSS/実capacityは未測。永続cache/新API/schemaなし、waiver none。統合/収載後だけ絶対D path/取込/未保存・未追跡・ignored/唯一成果/稼働参照/linkを監査して追加物を整理、branch/commit保持。hideの停止まで改善を続ける。
