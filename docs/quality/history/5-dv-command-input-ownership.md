# 5-dv — 完成したコマンド入力の所有を移す（Issue #392、ADR0108）

出典: docs/quality/gate-proofs.md の節 5-dv（2026-10-16 に D50 で移動・本文は移動時のまま）

先行設計7a23a454723bac08efe58672f3747cb29abf3ae6、同一harness aa4763ded1ed2daef8b0e8020dc2ebea8fb0d7f9、製品c6ce1ce3c0fbb8c297ace9c979aaf62a19886289。基準main c203f0e7d6f22221f6f31e3db340838486e86998からの製品変更はSearchLine/CommandLine/CommandPalette/CommandInput/EditorControllerの5files・完成した局所値の8所有境界だけ。失敗時保持、元state/borrow、helperの防御copy、themes共有、EditorState更新経路を保つ。caretは消費前に退避する。API/cache/parser/設定/schema変更なし、waiver none。

規則: ARC-001/003/004/005/007/008/011、CPP-002/003/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004。関連ADR0032/0041/0060/0062/0081/0082/0108。C2の未評価部分であり、元26候補の採否件数は変えない。

WT=D:/NeNeNib/worktrees/392-command-input、OUT=D:/NeNeNib/outputs/20261011-command-input。以下の相対証拠はOUT配下。採用後の保存先はD:/NeNeNib/evidence/speed-optimizations-20261011-wave4/snapshot-392/files/outputs/20261011-command-input。統合/収載/整理の実完了は別のacceptance-392へ記録し、予定を完了と扱わない。

### 直接影響と実行結果

| 確認する退行 / 対象・依存 | 実行したコマンドと結果 / 原証拠 |
| --- | --- |
| 完成入力move後のUTF-8、256bytes境界、元値・方向・caret・補完・検索preview/取消と直接Controller | Debug `nib_tests.exe --ex-settings` 209、`--command-palette` 495、`--vim-search-incremental` 147、計851checks成功。旧845+SearchLine不足契約6、ASan/UBSan/no-recover。`implementation/commands/debug-unit-*`。Release unit実行は0、全件実行なし |
| 新typed enum/入力/dispatchと従来登録の破壊 | `python -X utf8 -m unittest tests.conformance.test_compare_probes.ProbeComparisonTests.test_command_input_inputs_and_registry ...test_command_input_invalid_names ...test_tab_display_inputs_and_registry ...test_file_name_inputs_and_registry` の4tests成功（同クラスの完全引数は`implementation/check-harness.ps1`）。その後のproof失敗と区別し、成功tests再実行なし |
| 新12閉enumの漏れ、110登録の名前重複がcompilerで拒否されること | `implementation/enum-proof.py` 正例0/一値欠落負例1。`duplicate-gate.py`がactual TUの新規一行の名前だけ旧名へ置換しstatic_assert拒否1、step limit診断なし。正例は是正後before buildを再利用。`enum-proof/record.json`・`duplicate-gate/record.json` |
| 旧98probe/47selector/default/既存assertions/入力・比較器・規則・flagsの意図しない変更 | `harness-proof.py`/`harness-correction-proof.py`/`local-optionals-proof.json`/`source-proof.json`。固定12の独立bytes/FNV/SHA/code points/caret/checksum、旧部分保持。登録総数98→110のみ更新 |
| 製品と同じ正規compiler/strict flags/tidyで前後比較すること | before Release aa4763d、after通常Release c6ce1ce、Debug c6ce1ceのbuild成功。afterは`pwsh -NoProfile -File eng/build-release.ps1 -Ref c6ce1ce3c0fbb8c297ace9c979aaf62a19886289`。同Release dirへprobe/unitを追加。全native stdout/stderrをprocess開始から`implementation/commands`へ保存、3buildのFile API/flags/source/exe SHAも保持 |
| 各局所workloadの結果/所有/variant/scope/frame本文/補完等がRelease/Debugで一致 | `implementation/oracle-smoke.py`のnew12+old palette1をbefore Release/after Release/after Debug各1iteration、39oracle全成功。これは速さの試料ではない。`implementation/{before,after-release,after-debug}/oracle-smoke/record.json` |
| 変更14C++の規約、実CMake graph、core/application境界 | 差分clang-format dry-run/Werror・diff check成功、`implementation/source-graph.py command-input-debug-c6ce1ce` 0違反、`python eng/symbols.py --root WT --build-dir WT/build/command-input-debug-c6ce1ce --require core application` 2libraries/0違反。`commands/changed-source-graph`・`changed-symbols` |
| 既存fixtures/旧試験が損なわれていないこと | `python eng/protected-diff.py --base c203f0e7d6f22221f6f31e3db340838486e86998 --head c6ce1ce3c0fbb8c297ace9c979aaf62a19886289` 1853→1853、追加/変更/削除0。runtime47scopeは未測であり、全既存試験成功とは呼ばない |
| 実行物・harness・source・flagsと結果の対応 | `implementation/audit-provenance.py`是正版exit0。Git source archiveと物理bytesを別監査。Release前後compile commandsをbuild path/commit metadataだけ正規化して一致。Release/Debug flagsは異なり、各構成のstrict/tidy/sanitizerを別検査。ラベルの意味は`provenance-label-clarification.json` |
| 実入力所有移管の利益と短入力/Controller/既存5000候補の費用 | `runtime/run-comparison.py --before-commit aa4763ded1ed2daef8b0e8020dc2ebea8fb0d7f9 --after-commit c6ce1ce3c0fbb8c297ace9c979aaf62a19886289` 初回exit0・全13条件成立。`audit-comparison.py`が156runs/3120durations/6240marks/1560pairsを原rawから再計算一致 |
| Ex/search/paletteの途中入力、候補/検索match/入力caret、通常mode本文の描画/本文保持 | `gui-preparation/gui-command-input.py freeze`後、`run --exclusive-parent`を一度実行。10対の全1280×800 RGB無maskで0px、20本文copy/344checkpoints/56mode観測/2正常close。`runtime/audit-gui.py`成功。親が全66PNGを目視 |
| 直接callerであるfile palette5000の正式応答と候補到着 | `runtime/run-formal-guarded.py --executable WT/build/release-c6ce1ce/NeNeNib.exe --sha256 be474374731a5a56c26d82ab82a66754c69c61b25f054bdb578a87c97a2e7247`内で既存計測器の`--check --bench key-to-frame-palette-5000 --repetitions 5`を実行。初回5/欠測0/退行0。`audit-formal.py`がrawを再計算、5000file bytes不変。親が全5PNGのf/1 / 5000を確認 |

通常Release exe SHA256 be474374731a5a56c26d82ab82a66754c69c61b25f054bdb578a87c97a2e7247、1448448bytes。canonical記録`WT/out/release/c6ce1ce.json`。probe SHAはbefore c9eab9ecb34365c2f0f65bd1a26837833cf46c1af2a876ed96451ee79e16e8a0、after Release a1aa29f1e74cd3d1fc11709164d32b149873f67d043db8d728e80ee5e58c800e、Debug f45c436e01d496a785dcc172a7259c5871a1fa5bba767665a23e9cfbf9064d54。

### 固定局所比較

| workload | 前/後の各群中央値 µs | 対応比中央値 | 対応差中央値 µs | 短縮/120 | cycle比1 / 2 / 3 |
| --- | ---: | ---: | ---: | ---: | --- |
| command-input-search-long-1024 | 151 / 74 | 0.4899328859 | -77 | 120 | 0.4851633987 / 0.4933333333 / 0.4883874956 |
| command-input-ex-long-1024 | 203 / 120 | 0.5891089109 | -83.5 | 120 | 0.5800989749 / 0.5975467005 / 0.5930496035 |
| command-input-palette-long-1024 | 378 / 241 | 0.6390374332 | -135.5 | 120 | 0.6250000000 / 0.6490765172 / 0.6373333333 |
| command-input-search-short-1024 | 35 / 35 | 1.0000000000 | 0 | 5 | 1.0000000000 / 1.0000000000 / 1.0432773109 |
| command-input-ex-short-1024 | 51 / 45 | 0.8823529412 | -6 | 113 | 0.8653846154 / 0.8823529412 / 0.8823529412 |
| command-input-palette-short-1024 | 176 / 151 | 0.8579545455 | -25 | 115 | 0.8579545455 / 0.8920454545 / 0.8551251009 |
| command-input-ex-left-long-1024 | 129 / 90 | 0.6976744186 | -39 | 120 | 0.6977163462 / 0.6976744186 / 0.7031250000 |
| command-input-palette-fill-long-1024 | 3849.5 / 3719.5 | 0.9682994275 | -124 | 103 | 0.9692993054 / 0.9577493531 / 0.9780902064 |
| command-input-controller-search-64 | 347 / 346 | 0.9971014493 | -1 | 69 | 0.9971014493 / 1.0000000000 / 0.9942937614 |
| command-input-controller-ex-64 | 245 / 247 | 1.0081967213 | 2 | 47 | 0.9879759036 / 1.0123203805 / 1.0123203805 |
| command-input-controller-palette-64 | 130 / 127 | 0.9769230769 | -3 | 97 | 0.9769230769 / 0.9769230769 / 0.9844961240 |
| command-input-controller-fill-64 | 644 / 651.5 | 1.0077814890 | 5 | 45 | 0.9968651961 / 1.0031570816 / 1.0249132519 |
| palette-append-narrow-5000-to-50 | 38 / 37 | 0.9736842105 | -1 | 83 | 0.9736842105 / 0.9736842105 / 0.9743589744 |

固定input SHA eac377e57d999400fba4f5979ab0cb6217af3608f0d17cb7da8fc00bd11d7f74。20iterations/ABBA3/各120対応組を一度取得。最初3利益は比<1・全cycle<1・短縮>=90を満たし、長い入力から1024回生成/破棄する区間でSearch/Ex/Paletteの対応比が約.489933/.589109/.639037（約51.01/41.09/36.10%短縮）。入力生成/期待生成/独立oracleは区間外。Paletteは空候補なので5000件処理の利益には換算しない。残り10費用は全て比<=1.10かつ差<=50µs。

短Searchは35→35µsで横ばい、cycle3比1.043277と最大82µsの試料も保持。Controller Exは対応+2µs、fill全cycleは対応+5µs（各群中央値644→651.5µs）で小幅増。fill単独ではなく再open/entry/filter/folder requestと足場vector成長を含む。全入力が速いとはしない。対応中央値と各群中央値の差を混同せず、除外/補完/合格までの再取得なし。局所率をUI全体へ換算・他改善と加算しない。環境/LastInput不変、全agent停止中に親が排他取得した。

### GUI・正式指標・観測範囲

before GUIは#388通常Release SHA76965ccf74da69d10066241576accfd5a4b62416e0885e2ab5a54b7297b2a418を恒久archiveから使用。製品346796a→基準main c203f0eの差はdocs8filesだけで、製品/build不変。afterは上記canonical exe。capture-plan SHA d5473b91ad5e640368e0fc275ffe59be93cd2652feb3359e866d2b1101ad181eを窓0で固定した。

10場面はEx短補完/日本語中間edit、検索forward/中間edit/backward、palette tab/fill/edit/日本語0件、通常本文。本文alpha 日界 omega<CR><LF>alpha 日界 last、実body caret/status1,1を維持。検索current-match枠と実caretを区別する。Ex/search中の非表示mode toggleは直前/Esc後の実RGBで確認。私有profile/settings/13.5pt、closedIME/foreground/title/client/2560点非被覆、入力bytes/mtime不変を監査した。GUIはBMP日本語だけであり、4byte UTF-8はcore unit/probeの別契約。全66GUI PNGと正式5PNGの親目視は`visual-inspection.json`。原auditのpending注記は改変せず別記録で完了させた。

正式palette初回5試料は[2.673,2.159,2.152,2.648,2.011]ms、中央値2.159、基準2.630/上限4.630、欠測/退行/計測不能0。host bc8a356f37c68491。各trial5inputのrawを再計算一致、専用5000fileはnib<CR><LF>（hex6e69620d0a/SHA4dabb2cdef8196759094b534dc438fb22d3312d12c9fce1d8ab563d25320b8ce）。全5PNGはquery f/selected f0000.txt/1 / 5000を実描画。過去基準との比較であり、本差分のUI改善率は主張しない。他7指標は未測。

canonical chordは実入力tickを変えるため、LastInputの全区間不変とはしない。自動chord-callの直前/直後を記録し他区間の変化なしを確認した。窓内の別入力・同tick別イベント・tick非単調による完全な出所識別は保証しない。observerは刺激前I/Oを加えるが、計測input0のpost後へ観測I/Oを追加しない。既存の刺激/間隔/待ち/基準/許容を変更しない。取得中の他build/test/GUI/hash/copyは停止。#365/#373は未解決のまま。

### 失敗・是正・採否と再利用

最初のharness25098deはController palette rawの先頭colonを補完文字列と混同した期待と、魔法整数/closed set fallback分岐を親レビューで指摘。oracle/計測0のまま、独立期待を空completionsと1 fill候補へ分離し、既存閉enumの網羅switchへ修正した。入力/checksum/区間/採否は不変。

before初回compileは110登録のO(n²) repeated variant visitでconstexpr既定step上限へ到達。唯一のdispatchesから名前を一度だけ派生し、全重複組と完全性assertを維持した9488218へ是正。step上限/flags/gateは緩めず、actual TUの重複反例で拒否を確認。v2はnew Controller harnessのunchecked optional4件でclang-tidy失敗。has_value直後のempty検証集約と直接局所guardへ直したaa4763dで成功。両失敗source archive/全native stdout/stderr/build artifactsはbefore-failed-harness-v1/v2に保持。製品の性能失敗へ読み替えず、成功exeを捏造しない。

proof scriptのmap括弧誤り、format改行を想定した比較の誤り、provenanceのWindows backslash判定誤りは失敗原版/rawを残して道具だけ是正。provenanceはPath.as_posix一行修正で同じ成功build/testを監査した。harnessInputsFixturesFlagsRawIdenticalという出力ラベルはharness/configuration source bytes一致を意味し、Release/Debug生成flags一致ではない。`provenance-label-clarification.json`に明示し原成功rawは変更しない。

GUI案の検索previewと実caret混同、formal fixture LF誤記、取得前LastInput observerのI/O境界等は初回操作前にsource/一次資料で訂正し旧script/planを保全。推測path/Windows glob/出力切れの読取失敗、推測時刻の報告訂正もsession-journal.md/implementation-notes.jsonへ残す。失敗・未読を成功試験に数えない。#388と異なり本件は全native build stdout/stderrをprocess開始から保存した。

独立harness/product/runtimeレビューの未解決P0/P1/P2なし。親が全raw監査/画像/条件を確認し、2026-10-11 11:47 JSTに技術受理。draft PR→独立最終記録レビュー→Ready/必須CI→eng/merge-pr.py planと同expected-head execute→main同期→恒久収載→D追加WT/OUT/t392監査整理へ進む。実完了はacceptance-392に保持。branch/commitは残す。製品c6ce1ce以後に関連src/tests/入力/依存/必要環境が不変なら成功結果を再利用し、docs/commit SHA/工程だけで再実行しない。全件回帰/coverage/無関係な正式7指標は実行しない。

#388はPR391/main c203f0e、snapshot-388の703records/111398935bytesを全SHA収載し、10:35:05 JSTに追加WT/OUT/専用t388を監査整理済み。次候補はEx補完catalogのprefix照合後生成を読取設計中で、未Issue/未実装/未測定。通常files scopeのformal palette5000はこの次候補を通らず、正式指標/結果再利用の根拠を先に詰める。C8入力parse保持・R8・IO6〜IO8は未実験、#365/#373の処分は不変。hideの停止まで改善継続。
