# 5-dx — 補完prefixを同期呼出しの間だけ借用する（Issue #396、ADR0110）

出典: docs/quality/gate-proofs.md の節 5-dx（2026-10-16 に D50 で移動・本文は移動時のまま）

先行設計b47f54a58d75ad256c501aa9c8aec917d64427ff、harness1917db467202a52e8d7fc79adc52dc4b3787e978、製品1ec1264893b11be230ce9c516adb61c93e78fefa。基準main062f973041afb02f4551f7fcb36d1e7f5012d222からの製品差分はCommandLine.cpp::completionsだけ。optional<string>::value_orの一時所有を、has_value()?value():text_のconst lvalue参照へ置き換え、既存同期command_completions(string_view, themes)へ渡す。completed()の所有seed代入と他の全文はbyte不変。空engaged seedとraw空を区別し、返却vector/stringの所有を維持。API/settings/schema/cache/候補正本経路の変更なし、waiver none。

規則: ARC-001/003/004/005/007/008/011、CPP-002/004/005/007/008/012/014/016、QLT-001/002/004/007/008/010/012/013/014、GIT-001〜004。関連ADR0021/0022/0023/0032/0060/0062/0075/0082/0108/0109/0110。C2の追加評価で元26候補の採否件数は増やさない。

WT=D:/NeNeNib/worktrees/396-completion-prefix、OUT=D:/NeNeNib/outputs/20261011-completion-prefix。下記相対証拠はOUT内。恒久保存先はD:/NeNeNib/evidence/speed-optimizations-20261011-wave4/snapshot-396/files/outputs/20261011-completion-prefix。実統合・収載・整理は別のacceptance-396へ記録し、予定を完了扱いしない。

### 退行根拠と実行結果

| 退行リスク / 対象 | 実行コマンドと結果 / 証拠 |
| --- | --- |
| Tab順/逆順/reset/失敗保持、commands paletteとcatalogの直接caller | Debug `nib_tests.exe --ex-settings`225、`--command-palette`495、`--user-theme-selection`97、既存817checksを各1回成功。ASan/UBSan/no-recover。`implementation/commands/debug-unit-*`。unit追加0、Release unit build/run0 |
| 新typed登録・入力・不正名、旧登録破壊 | `python -m unittest tests.conformance.test_compare_probes.ProbeComparisonTests.test_completion_prefix_fixed_inputs tests.conformance.test_compare_probes.ProbeComparisonTests.test_completion_prefix_bad_name tests.conformance.test_compare_probes.ProbeComparisonTests.test_command_input_inputs_and_registry` 3tests成功、`implementation/commands/python-targets` |
| enum漏れ拒否、旧122条件/47selector/default/assertions保持 | `implementation/enum-proof.py` actual checked_index正例exit0/一値欠落負例exit1（-Werror/-Wswitch）。`harness-proof.py`/`source-proof.py`成功。登録129、旧3frameも不変。7metadata/literal/checksumを独立確認 |
| strict compiler/tidy、同一harnessの実exe | before Release1917db4、after通常Release1ec1264、Debug1ec1264の3build成功。after `pwsh -NoProfile -File eng/build-release.ps1 -Ref 1ec1264893b11be230ce9c516adb61c93e78fefa`、同Releaseへprobeのみ追加。全configure/build/compiler/tidy stdout/stderrを開始時から保持 |
| raw/seeded/空seedと返却所有、元値・保持vector/view、直接frame | `implementation/oracle-smoke.py` 新7pure+旧3frameを各build1iteration/warmup1、計30oracle成功。`implementation/{before,after-release,after-debug}/oracle-smoke/record.json`。速度採否試料とは区別 |
| C++規約/CMake依存/core境界/既存fixture | 差分8C++のclang-format dry-run/Werror成功。`source-graph.py completion-prefix-debug-1ec1264`0違反。`python eng/symbols.py --root WT --build-dir WT/build/completion-prefix-debug-1ec1264 --require core application`2libraries/0違反。`python eng/protected-diff.py --base 062f973041afb02f4551f7fcb36d1e7f5012d222 --head 1ec1264893b11be230ce9c516adb61c93e78fefa`fixtures1853→1853/差分0、旧47runtime scope未測 |
| source/harness/flags/exeと記録の来歴 | `implementation/audit-provenance.py`exit0。3source archive/Git・physical SHA/compile commands/File API/Ninja/native/30oracle/3unitを照合。製品差分はCommandLine.cppの指定二行だけ。Release/Debugの異なるflagsを各構成別に確認 |
| 長prefix copyの利益、短い入力/空/seeded/直接frameの費用 | `runtime/run-comparison.py --before-commit 1917db467202a52e8d7fc79adc52dc4b3787e978 --after-commit 1ec1264893b11be230ce9c516adb61c93e78fefa`初回exit0、10条件成立。`audit-comparison.py`原120runs/2400durations/4800marks/1200pairs再計算一致 |
| 表示/選択/入力caret/本文/mode/status | `gui-preparation/gui-completion-prefix.py inputs`→`freeze --after <normal Release> --after-sha256 2c140...482d7`→`run --exclusive-parent`を一度実行。`runtime/audit-gui.py`成功。7caption+14guardの21対/42PNGが1280×800 RGB無mask0px、14本文copy/232checkpoint/30mode/2正常close。親全42PNG目視 |
| 無関係な正式測定の再取得を避けるsource/callerと環境確認 | `runtime/audit-formal-reuse.py --product-commit 1ec1264893b11be230ce9c516adb61c93e78fefa`初回exit0。#392 files palette5000旧結果だけ限定reuse、新正式取得0。`formal-reuse.json` |

全command/時刻/exit/原stdout/stderrはimplementation/commandsとcommand-capture、SHA/source proofはimplementation/verification-summary.json/provenance-audit.json。通常Release exeは1446400bytes、SHA2c1401d0f87a316c044fbaa87f77332557a4759f42e55cbcd2accf124ba482d7、WT/out/release/1ec1264.json（40hex Ref/cleanHEAD/builtFromWorktree:false）。before probe b0fed701e58c6e92fd5a0e1c74158b6b11a1da08d6447aaf4aef1b31d7c81dbb、after Release probe 3a6821752ff2640df1d5fd98cf4bb5a93133700566b6ae346d611bb15b6aef8a。Debug SHAは同summaryのbuilds.after-debugへ保持。

### 初回固定比較

| workload | 前 / 後の各群中央値 µs | 対応比中央値 | 対応差中央値 µs | 短縮 / 120 | cycle比1 / 2 / 3 |
| --- | ---: | ---: | ---: | ---: | --- |
| completion-prefix-long-miss-4096 | 448 / 277 | 0.6196868009 | -170 | 120 | 0.6203802167 / 0.6165696311 / 0.6196868009 |
| completion-prefix-theme-raw-4096 | 2288 / 2137 | 0.9344118808 | -150 | 117 | 0.9322896479 / 0.9440761906 / 0.9337126781 |
| completion-prefix-theme-seeded-4096 | 2278.5 / 2125.5 | 0.9305584241 | -158 | 111 | 0.9382443758 / 0.9295368900 / 0.9270994376 |
| completion-prefix-short-raw-4096 | 476 / 474 | 0.9852476127 | -7 | 70 | 0.9926952580 / 0.9852476127 / 0.9771568568 |
| completion-prefix-short-seeded-4096 | 473 / 468 | 0.9915074310 | -4 | 93 | 0.9883896234 / 0.9853556485 / 1.0000000000 |
| completion-prefix-empty-raw-4096 | 1486 / 1457.5 | 0.9780634491 | -32.5 | 102 | 0.9771505273 / 0.9878787824 / 0.9754203774 |
| completion-prefix-empty-seeded-4096 | 1478 / 1454.5 | 0.9827710575 | -25.5 | 100 | 0.9837397702 / 0.9827825174 / 0.9795719233 |
| ex-completion-frame-ex-option-64 | 78 / 78 | 1.0000000000 | 0 | 40 | 1.0000000000 / 1.0000000000 / 1.0000000000 |
| ex-completion-frame-ex-empty-64 | 103 / 104 | 1.0245098039 | 2.5 | 48 | 0.9813917258 / 1.0396039604 / 1.0297029703 |
| ex-completion-frame-palette-64 | 64 / 61 | 0.9375000000 | -4 | 115 | 0.9319852941 / 0.9402985075 / 0.9467787115 |

fixed-inputs.json SHA73b368c2ef29da2f63ebc2c3cb1147c5a087837fef6fc483c6bfc63b2c29b3a4を先行固定。20iterations/ABBA3/各120対応組を一度だけ取得。1利益は比<1・全cycle<1・短縮>=90、9費用は比<=1.10かつ対応差<=50µs/固定区間を満たす。host/power/LastInput不変。他build/test/GUI/hash/copyを停止し排他取得。除外/補完/合格までの再取得なし。

longは有効UTF8 256bytes/98codepointsのedit pathで候補なし。同const CommandLineのcompletions生成・破棄4096回で約38.03%短縮。theme raw/Tab後、short raw/Tab後、empty raw/Tab後も4096回。準備/insert/Tab/期待生成は区間外。空からTab1の公開textはcolorschemeだが保持seedは空なので26候補のまま。この操作と公開状態をoracleで確かめ、private seedを直接読んだとはしない。正の対応費用: ex-completion-frame-ex-empty-64: 比1.024510/対応差2.5µs。各群中央値差と対応差中央値を混同しない。

旧3frameはcontroller.frame生成・破棄64回で、apply/delivery/Win32描画を含まない。commands-scope paletteもcompletionsを呼ぶ。旧controller-fill64の計測内は今回関数を呼ばずframe oracleだけなので除外した。4096pureと64frameを混同せず、局所率をUI全体へ換算/加算しない。

### GUIと正式旧結果の限定再利用

beforeは#394通常Release（SHA239b624debca978767756a739dfd0772fda09b5073e6ba0757041cc31cfe194c）、旧4d9aaefから基準main062f973はdocsだけ。afterは上記通常Release。窓0でhelper/input/exe/全鍵列をfreezeしcapture-plan SHA846fa1b8f1de8886a69c47454a58f50a495b8ed7b3e3f1e5dcc79e0e3e5f9d91。

7sceneは長いBMP edit path、set f、同Tab、colorscheme drac、同Tab、空からTab1、commands palette raw:fz。長入力の全256bytesはcore oracleで確認し、PNGは水平clipされた末尾Z/入力caretを確認。空Tabはcolorscheme選択と先頭6候補、完全26順は独立core oracleと区別。各captionは本文copy用Escより先に撮影。全guardのVim NORMAL/先頭a caret/行1桁1、captionの入力caret末尾/同statusを確認。Exのmode toggleは非表示なので前後guardで確認した。

専用D:/NeNeNib/evidence/t396/probe.txtの本文alpha 日界 omega<CR><LF>alpha 日界 last、37bytes/元mtime、private settings/13.5pt、closedIME/foreground/title/client/非被覆2560点/LastInputを確認。全42画像の親観察はvisual-inspection.json。原auditのpending表示は別記録で完了させ原文を改変しない。GUIはBMPのみで4byte UTF8/外部IME新検証ではない。

files palette5000のcanonical Ctrl+P→f→0はlisted_positionsへ進み、completions_ofがfiles scopeで空を返す。旧c6ce1ceからの差分はExResult.cppとCommandLine.cppで、CommandLineのcompletions以外全文、他製品source、112製品compile flags、tool/input/基準/host/power不変を確認。QLT-001/012・ADR0021に従い旧#392の5試料[2.673,2.159,2.152,2.648,2.011]ms、中央値2.159/基準2.630/上限4.630だけ限定再利用した。新exeの正式測定/正式8成功/Ex UI速度ではない。旧chord内の入力帰属限界も継承。旧raw再計算/PNG目視/試料取得は繰り返さない。

### 全試行・採否・次工程

製品build/対象試験/比較/GUIの失敗は0。準備の推測path/ADR名、GUI準備AST抽出、子担当の返信時刻、formal audit外側consoleのencoding表示は原raw/journal/訂正を保持。formal原UTF8 stdout/JSONは正しく、再実行なし。成功した検証とこれら読取・記録失敗を混同しない。全試行はsession-journal.md、implementation/implementation-notes.json、各helper原版とcommand-capture。

独立fixed/harness/borrow/productレビュー未解決P0/P1/P2なし。親が事前10条件・全raw監査・全42PNGを確認し2026-10-11 13:57 JSTに技術受理。draft PR/独立最終記録レビュー/Ready/必須CI/eng/merge-pr.py plan・同head execute/main同期/恒久収載/絶対D path・取込・dirty/untracked/ignored・唯一成果・稼働/link監査整理へ進む。実完了はacceptance-396、branch/commit保持。

製品1ec1264893b11be230ce9c516adb61c93e78fefa以後に結果8文書だけを編集しsrc/tests/入力/依存/必要環境が不変なら、817checks/30oracle/比較/GUI/正式限定reuseをpush/review/mergeで再利用する。工程やSHAだけで再検証せず、全件回帰/coverage/旧47runtime/正式8を実行しない。

#394はPR395/main062f973へ13:09:16 JST統合、snapshot-394に923records/135266016bytesを恒久収載し13:12:50 JSTに追加WT/OUT/t394の監査整理を完了した。次候補はCommandChoice::palette_choicesのrange string copyとchoice_ofのDisplayText copyで、独立読取reviewまで。未Issue/未固定/未実装/未測。将来probeには独立全候補順位literalが必要。旧controller-fill64はこの次案なら直接caller費用に該当する。空palette hint reserve案も未実験。元26件、C8/R8/IO6〜IO8、#365/#373の処分不変。hideの停止まで継続する。
