# 5-dz — 一致した操作候補だけ表示値を所有する（Issue #400、ADR0112）

出典: docs/quality/gate-proofs.md の節 5-dz（2026-10-16 に D50 で移動・本文は移動時のまま）

#400 / ADR0112。operation_choicesは利用可能な操作のkeyを一度生成し、静的name/readingとそのkeyを既存scorekernelへ借用する。一致した行だけ従来のDisplayText::parseでname/descriptionを所有し、同じkeyを返却値へmoveする。CommandChoice.cppの三関数以外と他製品source525filesは不変。空query、mode別鍵、全fields、同点順、返却所有を維持し、description/short_nameは従来どおり照合しない。公開API/settings/schema/table/cache/防御copy/正本経路の変更なし、waiver none。

先行設計5cedb47d70d9684aa907740e16f33bc90c636ba3、harness333b4de6a439107a3a5277b8e838a7ffbbccbbf9、製品f8cbda10cc806f4b1f2420fc4c8405bac8688db7。初回2利益/8費用、Debug既存1530checks/30oracle、旧138probe/47selector/default/assertions/1853fixtures保持、GUI18対0px/36PNG・12本文copy・264checkpoint・36mode・2正常close・親全画像目視で2026-10-11 16:07 JSTに技術受理。

queryほぞん（返却2候補）の1024回生成・破棄区間で約51.79%、qqq（返却0候補）の同1024回で約63.17%短縮。全件を含む費用8条件は[gate-proofs 5-dz](../../quality/gate-proofs.md#5-dz--一致した操作候補だけ表示値を所有するissue-400adr0112)。局所率を合算/UI全体へ換算しない。新正式取得0、旧#392 files palette5000だけをsource/caller/112compile flags/tool/input/host/power不変で限定reuseし、新exeの正式速度/全8成功とはしない。

規則: ARC-001/003/004/005/007/008/011、CPP-002/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、GIT-001〜004。関連ADR0021/0023/0032/0060/0061/0062/0078/0079/0081/0082/0091/0100/0111。基準main72e68633badafc69b03d84456a85068001793e30。C2の追加評価で元26候補の採否件数は増やさない。

WT=D:/NeNeNib/worktrees/400-operation-choice、OUT=D:/NeNeNib/outputs/20261011-operation-choice。相対証拠はOUT内、恒久先wave4/snapshot-400/files/outputs/20261011-operation-choice。実統合・収載・監査整理はwave4/acceptance-400へ記録し、予定を完了扱いしない。

### 変更に関係する検証

| 退行リスク / 対象 | 実行したコマンド・結果・証拠 |
| --- | --- |
| 操作表/照合/key/全行parse、唯一のcallerとoperation_request、short_name契約 | Debug `nib_tests.exe --operations`516、`--command-palette`495、`--guide`519、既存1530checksを各1回成功。ASan/UBSan/no-recover、implementation/commands/debug-unit-*。新unit0、Release unit build/run0 |
| 新typed登録/固定入力/不正名と旧登録 | `python -X utf8 -m unittest tests.conformance.test_compare_probes.ProbeComparisonTests.test_operation_choice_fixed_inputs tests.conformance.test_compare_probes.ProbeComparisonTests.test_operation_choice_bad_name tests.conformance.test_compare_probes.ProbeComparisonTests.test_operation_choice_preserves_old_registry`3tests成功。implementation/commands/python-focused |
| 閉enum漏れ拒否と旧138条件/47selector/default/assertions保持 | implementation/enum-proof.py正例exit0・一値欠落負例exit1（-Werror/-Wswitch）、harness-proof.py/source-proof.py成功。新閉enum10値、登録148 |
| 全58期待行/全fields/labelとdetail codepoints/全順位/元queryとmode/保持vectorとview/生成元query寿命終了後の返却所有 | implementation/oracle-smoke.py、新10pureをbefore Release/after通常Release/Debug各10invocations、1iteration/warmup1で30oracle成功。速度採否の試料とは区別 |
| strict compiler/tidy・同一harnessの実exe | clean harness333b4de before Release、製品通常Release/Debugの3build成功。after `pwsh -NoProfile -File eng/build-release.ps1 -Ref f8cbda10cc806f4b1f2420fc4c8405bac8688db7`、同buildへprobeのみ追加。native configure/build/compiler/tidy stdout/stderrを開始から全保存 |
| 差分規約/CMake依存/core境界/既存fixture | 差分9C++ clang-format dry-run/Werror、source-graph.py operation-choice-debug-f8cbda1 0違反、eng/symbols.py --require core application 2libraries/0違反、eng/protected-diff.py --base 72e68633badafc69b03d84456a85068001793e30 --head f8cbda10cc806f4b1f2420fc4c8405bac8688db7 fixtures1853→1853/差分0。全commandはimplementation/commands |
| source/harness/flags/exe来歴 | implementation/audit-provenance.py成功。Git blobとphysical source SHAを分離し、構成別strict/no-recover flagsを確認。FileAPI/Ninja/cache/compile commands/source archivesを保持 |
| 非一致行の表示所有を省く利益と全件等の費用 | runtime/run-comparison.py --before-commit 333b4de6a439107a3a5277b8e838a7ffbbccbbf9 --after-commit f8cbda10cc806f4b1f2420fc4c8405bac8688db7を初回一度、10条件成立。runtime/audit-comparison.py原120runs/2400durations/4800marks/1200pairs再計算一致 |
| operation表示/順位/key/件数/入力caret/本文/mode/status | gui-preparation/gui-operation-choice.py inputs→freeze --after通常Release --after-sha256 9a6d117291d560385535675b665eec2c7248512cd8e860bbf82fb167e7136b5c→run --exclusive-parent。runtime/audit-gui.py成功、親全36PNG目視。18対1280×800 RGB無mask0px/12本文copy/264checkpoint/36mode/2正常close |
| 無関係な正式測定の再取得を避けるsource/caller/環境 | runtime/audit-formal-reuse.py --product-commit f8cbda10cc806f4b1f2420fc4c8405bac8688db7成功。旧#392 files palette5000の一指標だけ限定reuse、新正式取得0。formal-reuse.json |

全実command/時刻/exit/原stdout/stderrはimplementation/commandsとcommand-capture。verification-summary.json/provenance-audit.jsonに全build/source/flags/strict設定を保持。通常Releaseは1445376bytes/SHA9a6d117291d560385535675b665eec2c7248512cd8e860bbf82fb167e7136b5c、40hex Ref/cleanHEAD/builtFromWorktree:false。before probe c2cc2f21fa81655ec0dd7dae33c11e41fea141245eb2cd1a59c0cd98040ea322、after Release probe 783da45b6e013a0ce153c522b88ca2d07cd61f5329101e396edfad6134f4ae81。

### 初回固定比較

| workload | 前 / 後の各群中央値 µs | 対応比中央値 | 対応差中央値 µs | 短縮 / 120 | cycle比1 / 2 / 3 |
| --- | ---: | ---: | ---: | ---: | --- |
| operation-choice-reading-ordinary-1024 | 5965.5 / 2861 | 0.4820913757 | -3095.5 | 120 | 0.4822824465 / 0.4816708517 / 0.4768257897 |
| operation-choice-miss-ordinary-1024 | 5559 / 2039.5 | 0.3682794165 | -3503 | 120 | 0.3676910555 / 0.3704108380 / 0.3684736251 |
| operation-choice-all-ordinary-256 | 1436.5 / 1383 | 1.0000000000 | 0 | 59 | 0.9990996399 / 0.9974489796 / 1.0006806130 |
| operation-choice-all-vim-256 | 1270 / 1280.5 | 1.0087649625 | 11 | 42 | 1.0107231317 / 0.9996047431 / 1.0111865922 |
| operation-choice-space-ordinary-256 | 1521 / 1482.5 | 0.9890323112 | -17.5 | 87 | 0.9790790374 / 0.9958594380 / 0.9931903295 |
| operation-choice-key-vim-1024 | 5892 / 3717.5 | 0.6330712329 | -2148 | 120 | 0.6301357225 / 0.6345314211 / 0.6353647961 |
| operation-choice-name-ordinary-1024 | 6481 / 4118.5 | 0.6412496423 | -2313 | 120 | 0.6388349318 / 0.6398253411 / 0.6451667866 |
| operation-choice-close-key-vim-1024 | 5814.5 / 2510 | 0.4331948771 | -3297.5 | 120 | 0.4313813275 / 0.4334053258 / 0.4339540809 |
| operation-choice-description-miss-1024 | 5590 / 2086 | 0.3736212491 | -3504.5 | 120 | 0.3727736289 / 0.3714759012 / 0.3750337291 |
| operation-choice-short-name-miss-1024 | 5625 / 2089 | 0.3736839686 | -3519 | 120 | 0.3703317710 / 0.3750678547 / 0.3727975018 |

fixed-inputs.json SHA698da4b0ebeafadb6bf3621f3a0bb9b575b8ff0b7aefeb31d699ce060c611b47を製品変更前に固定。手書き16表literalと10query/mode/rounds/期待順を製品を呼ばない独立Unicode scalar参照・別席全58行レビューで照合。metadata bytes/FNV1a64/SHAとchecksumを固定した。外部createdUtcは凍結来歴であり、別再生成時にJSON全SHAが同じとはしない。排他作成した同artifactのみ使い、計測入力へ時計を入れない。

20iterations/warmup1/ABBA3/各120対応組を初回一度取得。利益2は対応比<1/全3cycle<1/短縮>=90、費用8は比<=1.10かつ対応差<=50µs/block。host/power/LastInput不変、他build/test/GUI/hash/copy停止。除外/補完/合格まで再取得なし。正の対応費用: operation-choice-all-vim-256: 比1.008765/対応差11µs。各群中央値差と対応差中央値は別量。

全10条件がpure operation_choices生成・破棄とchecksumだけ。query/mode/期待準備/全field oracle/保持view/query所有元寿命終了確認は区間外。checksumは1+件数+全行label bytes/cps+detail bytes/cps+key bytes+operation ordinal+1。empty ordinary16/Vim14、space16はkey長と同点表順、reading2/key4/name5/close-key1/各miss0。全件は256rounds、ほかは1024rounds。controller全体/UIの計測をしたとはしない。

### GUI・正式旧結果・限定

beforeは#398通常Release5cc9727/SHA05090cc9bebf7989c525c4e1795b5b0bbeb4e7ed308d204fe89c16613fe2a8f7をsnapshot-398から参照。基準mainまでdocsだけで製品/build不変。専用D:/NeNeNib/evidence/t400/probe.txt37bytes、private profile/settings/13.5pt/client1280×800/DPI120とhelper/全キー列/input/exeを起動前freeze。capture-plan SHA8569dec75a5f02c88317ae84b8df4210e45bc3264ba5e35391be9dd93ae36d82、preparation-plan SHA8261cac9a23255763024d60ac6fdbbfa5cfaa8d7d9a1a6a30053dbf389093fbd。

6sceneはordinaryほぞん/qqq/全件/空白、Vim全件/cTrL+s。F1入口、mode別key/件数/先頭選択/no-keyを確認。captionを本文copy用Escより先に取得し操作をSubmitしない。全caption/前後guardをsceneの実modeへ戻して36mode、本文copy時だけVimにして17mode transitions/side。全36原画像の親記録visual-inspection.json。元bytes/mtime/settings/closedIME/foreground/title/非被覆2560点/LastInput/正常closeを確認。BMPと可視行の確認であり、全行は別の独立literal oracleが担う。

現在の固定16行は有効UTF8でDisplayTextは正規化しないため、借用照合のbytesは従来と同じ。将来不正な非一致表を入れた際のparse例外タイミングと割当失敗順は変わり得る。既存--operationsのempty ordinary全16parseと新literal oracleで不正表を失敗させ、第二validator/cacheを導入しない。key viewは同期score中だけ、返却はowning。vector局所値で部分結果を公開しない。

files palette5000はlisted_positions側で今回三操作関数/旧palette_choices/CommandLine::completionsを呼ばない。旧c6からの製品差分はExResult.cpp/CommandLine.cpp/CommandChoice.cppだけ、今回三関数と旧#398二所有箇所を逆照合し他本文/112compile flags/tool/input/host/power不変を確認。旧#392の5試料[2.673,2.159,2.152,2.648,2.011]ms、中央値2.159/基準2.630/上限4.630だけをQLT-001/012・ADR0021で限定reuse。同一TU layout波及は新exeで未測、新正式測定/正式8成功/操作UI速度とは呼ばない。旧chord内入力帰属限界も維持。

### 全試行・採否・後続

製品/計測harnessのbuild・対象試験・比較・GUI失敗0。準備のsource-graph旧型pathは実行前に実changed-file集合へ是正し原source-graph-initial-paths.pyを保持。GUI bodycopy順draft0、case-field名の照合停止、親の推測path/大出力切詰めはsession-journal.md、GUI journal、implementation-notes.jsonに残し、成功や製品失敗へ置換しない。

独立fixed/harness/product/所有callerレビューと親の原raw・全36PNG確認で受理。正式旧結果の同一TU未測と外部fixed JSON再生成差は限定として保持。通常draft PR/最終独立記録レビュー/Ready/必須CI/eng/merge-pr.py同head plan→execute/main同期/恒久収載/絶対D path・取込・dirty/untracked/ignored・唯一成果・稼働/link監査整理へ進む。実完了はacceptance-400、branch/commit保持。

製品commit以後は結果8文書だけ。src/tests/入力/依存/必要環境不変なら成功1530checks/30oracle/比較/GUIをpush/review/mergeで再実行しない。全件回帰/coverage/旧47runtime/正式8/無関係Exは対象外。

#398はPR399/main72e6863へ15:15:11 JST統合、snapshot-398の1103records/154968496bytesを恒久収載し15:17:16 JSTに追加WT/OUT/t398を監査整理完了。次のstatus行・桁の中間所有案はOUT/next-candidate-status-number-investigation.mdの読取調査だけで未Issue/未実装/未測。opened/extended防御copy削除はADR0091に反するため見送り。C8/R8/IO6〜IO8、#365/#373、元26候補の処分不変。hideの停止まで継続。
