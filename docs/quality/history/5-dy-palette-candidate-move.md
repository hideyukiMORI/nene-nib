# 5-dy — パレット候補の所有値をmoveで渡す（Issue #398、ADR0111）

出典: docs/quality/gate-proofs.md の節 5-dy（2026-10-16 に D50 で移動・本文は移動時のまま）

#398 / ADR0111。CommandChoice.cppのpalette_choicesは一時owning vectorの要素をauto &で受け、一致したcommandを既存choice_ofへmoveする。choice_ofはDisplayTextを非constで所有し、返却値へmoveする。二箇所以外の全文と他製品source、rank/同点順/parse/kind/catalog/input/返却所有は不変。direct/fallbackのinitializer_list返却copyは残る。API/settings/schema/cache/正本経路の変更なし、waiver none。

先行設計55f51ace45b2fe4882338552e482e1fd4fce15cd、最終harnessd353aee3f30a092fbc6ed8b143b85fe64579dcc0、製品5cc97270646d0cf705a81f15ab3be04dbad1f141。初回2利益/8費用、Debug既存592checks/30oracle、旧129probe/47selector/default/assertions/1853fixtures保持、GUI21対0px/42PNG・14本文copy・232checkpoint・42mode・2正常close・親全画像目視で2026-10-11 15:07 JSTに技術受理。

空queryのbuiltin36候補を512回生成・破棄する固定区間で約10.54%、custom失敗名256を含む292候補を128回で約17.06%短縮。異なる回数の局所率を合算/直接比較したりUI全体へ換算しない。費用8条件と全試行は[gate-proofs 5-dy](../../quality/history/5-dy-palette-candidate-move.md)。新正式取得0、旧#392 files palette5000だけをsource/caller/112compile flags/tool/input/host/power不変により限定reuse。新exeの正式速度/全8成功ではない。

規則: ARC-001/003/004/005/007/008/011、CPP-002/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、GIT-001〜004。関連ADR0021/0023/0032/0060/0061/0062/0066/0067/0075/0081/0082/0091/0109/0110/0111。基準main4f6419ebd93c0b634692b6ccf311cd0ab783d966。C2の追加評価で元26候補の採否件数は増やさない。

WT=D:/NeNeNib/worktrees/398-palette-choice、OUT=D:/NeNeNib/outputs/20261011-palette-choice。相対証拠はOUT内。恒久先はwave4/snapshot-398/files/outputs/20261011-palette-choice。実統合・収載・監査整理はwave4/acceptance-398へ記録し、予定を完了扱いしない。

### 変更に関係する検証

| 退行リスク / 対象 | 実行したコマンド・結果・証拠 |
| --- | --- |
| 候補順位/fill/直接設定/custom成功失敗/入力本文保持 | Debug `nib_tests.exe --command-palette`495と`--user-theme-selection`97、既存592checksを各1回成功。ASan/UBSan/no-recover、implementation/commands/debug-unit-*。新unit0、Release unit build/run0。--ex-settingsは直接callerでないので実行なし |
| 新typed登録/固定入力/不正名と旧登録 | `python -m unittest tests.conformance.test_compare_probes.ProbeComparisonTests.test_palette_choice_fixed_inputs tests.conformance.test_compare_probes.ProbeComparisonTests.test_palette_choice_bad_name tests.conformance.test_compare_probes.ProbeComparisonTests.test_command_input_inputs_and_registry`3tests成功。implementation/commands/python-targets |
| 閉enumの漏れ拒否、旧129条件と47selector/default/assertions保持 | implementation/enum-proof.py正例exit0・一値欠落負例exit1（-Werror/-Wswitch）。harness-proof.py/correction proof/source-proof.py成功。新閉enum9値で登録138。旧fill64定義不変 |
| 全36/292候補の順/所有、元input/catalog/保持vector/view、生成元寿命終了後の返却値 | implementation/oracle-smoke.py、新9pure+旧fill64をbefore Release/after通常Release/Debug各10invocations、1iteration/warmup1で30oracle成功。速度採否とは区別 |
| strict compiler/tidy・同一harnessの実exe | 最終harness d353aee before Release、製品通常Release/Debugの3build成功。after `pwsh -NoProfile -File eng/build-release.ps1 -Ref 5cc97270646d0cf705a81f15ab3be04dbad1f141`、同buildへprobeのみ追加。開始からのnative configure/build/compiler/tidy stdout/stderr保存 |
| 差分規約/CMake依存/core境界/既存fixture | 差分9C++ clang-format dry-run/Werror、source-graph.py palette-choice-debug-5cc9727 0違反、eng/symbols.py --require core application 2libraries/0違反、eng/protected-diff.py --base 4f6419ebd93c0b634692b6ccf311cd0ab783d966 --head 5cc97270646d0cf705a81f15ab3be04dbad1f141 fixtures1853→1853/差分0。全コマンドはimplementation/commands |
| source/harness/flags/exeの来歴 | implementation/audit-provenance.py成功。Git blobとphysical source SHAを分離しCRLFを明示、Release/Debug flagsを各構成別に確認。FileAPI/Ninja/cache/compile commands/source archivesを保持 |
| コピー削減の利益と直接caller費用 | runtime/run-comparison.py --before-commit d353aee3f30a092fbc6ed8b143b85fe64579dcc0 --after-commit 5cc97270646d0cf705a81f15ab3be04dbad1f141を初回一度実行、10条件成立。runtime/audit-comparison.py原120runs/2400durations/4800marks/1200pairs再計算一致 |
| 候補表示/入力caret/本文/mode/status | gui-preparation/gui-palette-choice.py inputs→freeze --after通常Release --after-sha256 05090cc9bebf7989c525c4e1795b5b0bbeb4e7ed308d204fe89c16613fe2a8f7→run --exclusive-parent。runtime/audit-gui.py成功、親全42PNG目視。21対1280×800 RGB無mask0px、14本文copy/232checkpoint/42mode/2正常close |
| 無関係な正式測定の再取得を避けるsource/caller/環境 | runtime/audit-formal-reuse.py --product-commit 5cc97270646d0cf705a81f15ab3be04dbad1f141成功。旧#392 files palette5000だけ限定reuse、新正式取得0。formal-reuse.json |

全実コマンド/時刻/exit/原stdout/stderrはimplementation/commandsとcommand-capture。implementation/verification-summary.jsonとimplementation/provenance-audit.jsonに各build/source/flags/strict設定を保持。通常Releaseは1445376bytes/SHA05090cc9bebf7989c525c4e1795b5b0bbeb4e7ed308d204fe89c16613fe2a8f7、40hex Ref/cleanHEAD/builtFromWorktree:false。before probe e1c5d73c5a3969c31e3f09b83458c057d60330254241aa0fbfd63ea23c691cd2、after Release probe 2ab79fb1bdd4c71e5ac00c58ebab5dd3974dc7e13fdce997f1758d6f3384be97。Debugは同summaryに記録。

### 初回固定比較

| workload | 前 / 後の各群中央値 µs | 対応比中央値 | 対応差中央値 µs | 短縮 / 120 | cycle比1 / 2 / 3 |
| --- | ---: | ---: | ---: | ---: | --- |
| palette-choice-builtin-all-512 | 5715.5 / 5106 | 0.8946398764 | -604.5 | 112 | 0.8976379084 / 0.8886439041 / 0.9019287906 |
| palette-choice-custom-all-128 | 16230.5 / 13459.5 | 0.8294068509 | -2780.5 | 120 | 0.8317613755 / 0.8338725088 / 0.8226194468 |
| palette-choice-miss-1024 | 3233.5 / 2876.5 | 0.8897877704 | -355 | 118 | 0.8876127344 / 0.8931627999 / 0.8909822803 |
| palette-choice-short-1024 | 3384.5 / 3019.5 | 0.8917001161 | -367 | 113 | 0.8947291627 / 0.8917001161 / 0.8891024669 |
| palette-choice-theme-512 | 1741.5 / 1524 | 0.8760472983 | -216 | 117 | 0.8747106481 / 0.8774420578 / 0.8770607743 |
| palette-choice-direct-fill-1024 | 89.5 / 86 | 0.9662921348 | -3 | 106 | 0.9662921348 / 0.9662921348 / 0.9649169007 |
| palette-choice-direct-long-1024 | 634 / 598 | 0.9445324881 | -35 | 111 | 0.9414093061 / 0.9481945348 / 0.9416863310 |
| palette-choice-fallback-long-512 | 2219.5 / 1997 | 0.9073276388 | -204 | 116 | 0.9092569150 / 0.9071036518 / 0.8908277186 |
| palette-choice-custom-narrow-128 | 7030 / 5597.5 | 0.7980674527 | -1419 | 120 | 0.8075955583 / 0.7956681890 / 0.7921710541 |
| command-input-controller-fill-64 | 638 / 617 | 0.9708240036 | -18.5 | 90 | 0.9714737794 / 0.9618960546 / 0.9784675729 |

fixed-inputs.json SHAc2c929f74c89ab0d964cc4eea2c74f8b13af499bb04c707838dc909f73502d1fを製品変更前に固定。20iterations/ABBA3/各120対応組を初回一度取得。利益2は対応比<1/全3cycle<1/短縮>=90、費用8は比<=1.10かつ対応差<=50µs/固定区間。host/power/LastInput不変、他build/test/GUI/hash/copy停止。除外/補完/合格までの再取得なし。正の対応費用: 費用8条件の対応差中央値に正の値はない。各群中央値差と対応差中央値は別の量。

新9pureは同const CommandLineでpalette_choices生成・破棄を固定回数行い、checksumだけ加算。準備/insert/catalog/期待値/oracleは区間外。全候補順・8fieldsは独立literal、input/caret/index/catalog全record・保持結果/view・生成元寿命終了後の所有を確認した。custom256はuser-0000〜0255/invalid_color。direct/fallback raw256bytes、colon除外後255bytes/97または93codepoints。全copy解消とは主張しない。

旧controller-fill64はSubmit/Cancel/OpenPalette/CommandText(:fz)のapply/delivery/request-vector成長を含む64cycle。CommandText→refiltered→result_of→palette_choicesが区間内の直接callerで、fillだけの時間ではない。旧Ex frameは候補を再生成しないため除外。pureの異なる回数と64cycleを混同しない。

### GUI・正式旧結果・限定

beforeは#396通常Release1ec1264/SHA2c1401d0f87a316c044fbaa87f77332557a4759f42e55cbcd2accf124ba482d7をsnapshot-396から参照。基準mainまでdocsだけで製品/build不変。起動前に専用D:/NeNeNib/evidence/t398/probe.txtの37bytes、private profile/settings/13.5pt、helper/全キー列/input/exeをfreeze。capture-plan SHA278f7a7dec7db3eb49077f55c98714d2ac524f451a47d4f1cb1aa9d1b72e8f42、preparation-plan SHA78b93ab4b572a381819f18642dbfd97b1080a923f064f0fc8b8af8adc2af71f3。

7sceneはcommands paletteのall(:)/fz/miss/DRAC/direct-fill/direct-long/fallback-long。設定文字列はSubmitせずcaptionを本文copy用Escより先に取得した。allの可視先頭8行(ls,wq,xit,edit,exit,quit,tabs,files)/1/36と、全36/292順を保証するcore oracleを区別。長入力は入力欄末尾/候補prefixのclipをPNGで確認し、全255bytes所有はoracleで確認。全caption/guardでmodeが可視、42modeを検証。全42画像の親記録visual-inspection.json。元bytes/mtime/settings/closedIME/foreground/title/client/非被覆2560点/LastInput/正常closeを確認。BMPのみ、外部IME/4byteUTF8新検証ではない。

files palette5000はlisted_positions側でpalette_choices/choice_ofを呼ばない。旧c6からの製品差分はExResult.cpp/CommandLine.cpp/CommandChoice.cppだけ、指定箇所以外全文と112compile flags/tool/input/host/power不変を確認。旧#392の5試料[2.673,2.159,2.152,2.648,2.011]ms、中央値2.159/基準2.630/上限4.630だけをQLT-001/012・ADR0021に従い限定reuse。同一翻訳単位のlayout波及を新exeで測定したとはせず、新正式測定/正式8成功/commands UI速度とは呼ばない。旧chord内入力帰属限界も維持。

### 全試行・採否・後続

初回harness067cd24はclang-tidy catalog_equal complexity11/limit10でnative exit1、probeなし。builtin名照合helper抽出後282bf83はspan.atが実MSVC14.44に無いことをレビュー/ヘッダで確認し、親が当taskの実PID/treeだけ停止。native exit1は中断でありcompiler診断を観測したとはしない。最終d353aeeはconst vector<string>&のchecked atを維持しbuild/tidy成功。失敗2buildの原source/Git archive/cache/compile commands/Ninja/FileAPI/nativeと停止recordを保持。入力/区間/期待/登録は不変なので成功済みPython3/enum/旧登録proofを再利用し、gateを弱めていない。

準備/読取の推測path・JSON key・生成proofのescape・誤ったJSON終端precondition・長文previewはsession-journal.mdとimplementation-notes.jsonの原attempt/訂正に保持。製品build/対象試験/比較/GUIの失敗0と、harness失敗/中断2を混同しない。

独立fixed/harness/型是正/productレビューと親の全raw/42PNGを確認して技術受理。独立所有callerレビューのP2（新exe同一TU layout未測定）は上記の限定として保持。通常draft PR/最終独立記録レビュー/Ready/必須CI/eng/merge-pr.py同head plan→execute/main同期/恒久収載/絶対D path・取込・dirty/untracked/ignored・唯一成果・稼働/link監査整理へ進む。実完了はacceptance-398、branch/commit保持。

製品commit以後は結果8文書だけ。src/tests/入力/依存/必要環境不変なら成功592checks/30oracle/比較/GUI/正式限定reuseをpush/review/mergeで再実行しない。全件回帰/coverage/旧47runtime/正式8は対象外。

#396はPR397/main4f6419eへ14:03:36 JST統合、snapshot-396に829records/126946660bytesを恒久収載し14:06:13 JSTに追加WT/OUT/t396整理完了。次候補operation_choicesのname/detailを一致後だけ所有する案は読取調査・独立レビューだけで未Issue/未実装/未測。将来の不正table検証時点は限定として保持。opened/extended防御copy削除はADR0091に反するため見送り。C8/R8/IO6〜IO8、#365/#373、元26候補の処分不変。hideの停止まで継続。
