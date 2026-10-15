# 5-dw — Ex補完の候補は一致後に所有する（Issue #394、ADR0109）

出典: docs/quality/gate-proofs.md の節 5-dw（2026-10-16 に D50 で移動・本文は移動時のまま）

先行設計82bb6ffe99a76b4f50b2920e7b151b4c68598ca7、同一harness5c3ccfd7bfc5f0ea24cb262d125d807ab2456929、製品4d9aaef18fde6825ffe7fbf0135f9fe51d6e808f。基準main5d22ceb376396aba360c14d24b35fd4fe170655cからの製品差分はExResult.cppだけ。唯一のprivate列挙でstem/suffixの一致を確かめてからowning stringを作る。全候補nulloptと通常補完の空prefixを区別し、quit!、失敗custom名、候補順、既存組立式、返却所有、paletteの全catalog経路を維持する。公開API/parser/設定/schema/cache変更なし、waiver none。

規則: ARC-001/003/004/005/007/008/011、CPP-002/003/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004。関連ADR0016/0021/0032/0060/0062/0066/0067/0075/0081/0082/0109。C2の追加評価で、元26候補の採否件数は変えない。

WT=D:/NeNeNib/worktrees/394-ex-completion、OUT=D:/NeNeNib/outputs/20261011-ex-completion。以下の相対証拠はOUT内。恒久保存先はD:/NeNeNib/evidence/speed-optimizations-20261011-wave4/snapshot-394/files/outputs/20261011-ex-completion。実統合・収載・整理は別のacceptance-394へ記録し、予定を完了と扱わない。

### 退行根拠と実行結果

| 確認する退行 / 対象・依存 | 実行したコマンドと結果 / 原証拠 |
| --- | --- |
| prefix境界、Tab循環/編集reset、paletteのrank/fill、成功/失敗custom名の契約 | Debug `nib_tests.exe --ex-settings` 225、`--command-palette` 495、`--user-theme-selection` 97、合計817checksを各1回成功。旧798+独立境界19。ASan/UBSan/no-recover。`implementation/commands/debug-unit-*`。Release unit実行0、全件回帰なし |
| 新typed入力・登録・不正名と旧登録の破壊 | `python -X utf8 -m unittest tests.conformance.test_compare_probes.ProbeComparisonTests.test_ex_completion_fixed_inputs tests.conformance.test_compare_probes.ProbeComparisonTests.test_ex_completion_bad_name tests.conformance.test_compare_probes.ProbeComparisonTests.test_command_input_inputs_and_registry` 3tests成功、`implementation/commands/python-targets` |
| 新閉enumの欠落をcompilerが拒否すること、旧110probe/47selector/default/assertions保持 | `implementation/enum-proof.py` actual checked_index正例0/一値欠落負例1（-Werror,-Wswitch）、`harness-proof.py`/`source-proof.py`成功。登録総数のみ110→122。固定JSONから12metadataのbytes/FNV/SHA/全literal順/checksumを独立照合 |
| 正規strict compiler/tidyと同一harnessでの比較 | before Release5c3ccfd、after通常Release4d9aaef、Debug4d9aaefの3build成功。afterは`pwsh -NoProfile -File eng/build-release.ps1 -Ref 4d9aaef18fde6825ffe7fbf0135f9fe51d6e808f`、同Releaseへprobe/unitを追加。全native configure/build/tidy/compiler stdout/stderrをprocess開始から保存 |
| 直接pure/frameの候補・元値・保持snapshot/view、失敗catalog名、本文/入力/caret等 | `implementation/oracle-smoke.py` 新12+旧fill1をbefore Release/after Release/after Debug各1iteration、39oracle成功。warmup1あり。速度受理の試料と区別する。`implementation/{before,after-release,after-debug}/oracle-smoke/record.json` |
| 差分C++規約、CMake依存、core/application境界、既存fixtures | 差分12C++のclang-format dry-run/Werror成功。`source-graph.py ex-completion-debug-4d9aaef` 0違反。`python eng/symbols.py --root WT --build-dir WT/build/ex-completion-debug-4d9aaef --require core application` 2libraries/0違反。`python eng/protected-diff.py --base 5d22ceb376396aba360c14d24b35fd4fe170655c --head 4d9aaef18fde6825ffe7fbf0135f9fe51d6e808f` fixtures1853→1853/差分0、runtime47scope未測 |
| source/harness/flags/exeと試験結果の来歴 | `implementation/audit-provenance.py` exit0。全3source archive・Git/physical SHA・compile commands・File API・Ninja/native raw・39oracle・3unitを照合。前後差分はExResult.cppだけ。Release/Debug生成flagsは異なり、各構成のstrict/tidy/sanitizerを別検査 |
| 一致後所有の利益と空/広いprefix・全catalog・直接frame/fillの費用 | `runtime/run-comparison.py --before-commit 5c3ccfd7bfc5f0ea24cb262d125d807ab2456929 --after-commit 4d9aaef18fde6825ffe7fbf0135f9fe51d6e808f` 初回exit0・13条件成立。`audit-comparison.py`で156runs/3120durations/6240marks/1560pairsを原rawから再計算一致 |
| 補完内容/順序/選択/スクロール、入力caret、本文保持、mode/statusの描画 | `gui-preparation/gui-ex-completion.py inputs`→`freeze --after <normal Release> --after-sha256 239b...194c`→`run --exclusive-parent`を一度実行。`runtime/audit-gui.py`成功。7caption+14guardの全21対/42PNGが1280×800 RGB無maskで0px。14本文copy/232checkpoint/30mode観測/2正常close、親が全42PNG目視 |
| 無関係な正式指標の再取得を避けるためのsource/caller・必要環境確認 | `runtime/audit-formal-reuse.py --product-commit 4d9aaef18fde6825ffe7fbf0135f9fe51d6e808f` 初回exit0。#392 palette5000の旧結果だけ限定再利用。新正式測定0、詳細は下記と`formal-reuse.json` |

コマンド全文・終了時刻・exit・全rawは`implementation/commands`と`command-capture`に保持。通常Release exeは1446400bytes、SHA239b624debca978767756a739dfd0772fda09b5073e6ba0757041cc31cfe194c、canonical記録`WT/out/release/4d9aaef.json`（40hex Ref/cleanHEAD/builtFromWorktree:false）。probe SHA before ce57a0f62639d1c7f2ac602ef9d24e7bd50eae8dc3d4a17c5abfdb7b7314b0a7、after Release ec219f0c9ff6780e6fe3cf2db02d493bd1087252d86c39df818e91c1a7b667db、Debug b3cb6b3ac10ae0b219b9dd1c6cd59b5d0c3e862cebbe177a61edc8c6a07d77aa。

### 初回の固定比較

| workload | 前 / 後の各群中央値 µs | 対応比中央値 | 対応差中央値 µs | 短縮 / 120 | cycle比1 / 2 / 3 |
| --- | ---: | ---: | ---: | ---: | --- |
| ex-completion-option-1024 | 1357.5 / 112 | 0.0827178730 | -1245 | 120 | 0.0827484413 / 0.0832423738 / 0.0824742268 |
| ex-completion-miss-1024 | 1339 / 80 | 0.0598804106 | -1258 | 120 | 0.0592401061 / 0.0594655711 / 0.0601953018 |
| ex-completion-custom-narrow-128 | 3059 / 776 | 0.2533946917 | -2286 | 120 | 0.2557102587 / 0.2484589323 / 0.2587498924 |
| ex-completion-empty-1024 | 1371 / 364 | 0.2654770904 | -1007 | 120 | 0.2650602763 / 0.2664704078 / 0.2653061224 |
| ex-completion-set-wide-1024 | 1374 / 141 | 0.1025920950 | -1233 | 120 | 0.1031339249 / 0.1024515322 / 0.1021152443 |
| ex-completion-theme-wide-1024 | 1735 / 1247 | 0.7167147694 | -493 | 117 | 0.7134941341 / 0.7180285842 / 0.7206003649 |
| ex-completion-all-builtins-1024 | 1621 / 1629.5 | 1.0093473617 | 15 | 31 | 1.0222774227 / 1.0043290308 / 1.0114187657 |
| ex-completion-custom-wide-128 | 3424 / 2848.5 | 0.8943184535 | -335 | 104 | 0.8907881549 / 0.8994027835 / 0.8935272124 |
| ex-completion-all-custom-128 | 2865 / 2726.5 | 0.9786498020 | -59.5 | 76 | 0.9930837997 / 0.9852175476 / 0.9541118709 |
| ex-completion-frame-ex-option-64 | 192 / 85 | 0.4394720075 | -108 | 120 | 0.4322916667 / 0.4424067982 / 0.4404145078 |
| ex-completion-frame-ex-empty-64 | 197 / 114 | 0.5757575758 | -83 | 120 | 0.5739815176 / 0.5736040609 / 0.5816326531 |
| ex-completion-frame-palette-64 | 197 / 70 | 0.3556701031 | -127 | 120 | 0.3589743590 / 0.3556701031 / 0.3553299492 |
| command-input-controller-fill-64 | 632 / 654.5 | 1.0303758741 | 19 | 18 | 1.0271350318 / 1.0342666667 / 1.0342058095 |

実装前fixed-inputs.json SHA ec844c86220934dd7a639b38770ff2cfbd4d1c262a3670fabe0bcbd25f1050e4。20iterations/ABBA3/各120対応組を一度だけ取得。最初3利益は比<1・全cycle<1・短縮>=90、残り10費用は比<=1.10かつ対応差<=50µs/固定区間を満たした。13条件の途中除外/補完/合格までの再取得なし。host/power/LastInput不変、他agent/build/test/GUI/hash/copyを停止し親が排他取得した。

option/missは候補生成・破棄1024回で約91.73/94.01%短縮。custom-narrowは失敗custom名256件を持つcatalogから128回で約74.66%短縮。全builtinの1024回は対応+15µs（比1.009347）、controller-fill64は対応+19µs（比1.030376）で事前費用内。各群中央値の差と対応差中央値を混同しない。frame3条件はcontroller.frame()の生成・破棄64回で、入力apply/deliveryやWin32描画を含まない。fillは再open/entry/filter/folder requestと足場vector成長を含み、fill単独の速度と呼ばない。局所率をUI全体へ換算・他改善と加算しない。

### GUIと正式結果の限定再利用

before GUIは#392通常Release SHA be474374731a5a56c26d82ab82a66754c69c61b25f054bdb578a87c97a2e7247をsnapshot-392から参照。旧製品c6ce1ce→基準main5d22cebの差はdocs8filesだけ。afterは上記通常Release。窓0でhelper/input/exe/全鍵列とcapture-plan SHA7602f12089f4a8a336ada86c310286ce86d2ceb2d87f37e5c01d1c4b902d4f8eを固定した。

7場面は:set f、同Tab、空ExのTab26回→saveas、:colorscheme drac、同Tab→dracula、colorscheme空白のTab10回→system、:tabsからHome/Deleteで:fzを入力したcommands palette。全captionを本文copy用Escより先に撮影。全guardはVim/NORMAL・実body caret先頭a・行1/桁1、captionは入力caret末尾・同statusを目視。Ex中のmode toggleは非表示なので前後guardのRGBで確認した。空補完末尾はquit/quit!/wq/xit/exit/saveas、theme末尾はmonokai/dracula/one-dark/night-owl/night-owl-light/systemを表示し、最下段が選択された。paletteは:fz、set fontsize=一件、1 / 1。完全26/10件の順は独立core/frame oracleと区別する。

本文alpha 日界 omega<CR><LF>alpha 日界 lastの37bytes、元mtime、private profile/settings/13.5pt、closedIME/foreground/title/client/2560点非被覆/LastInputを確認。全画像の親目視は`visual-inspection.json`、元auditのpending表示は別記録で完了し原文を改変しない。GUIはBMP日本語で、4byte UTF-8や外部IMEを新たに確認したとはしない。

canonical正式8指標はEx/commands候補列挙を刺激しない。QLT-001/012・ADR0021の変更範囲検証を適用し、基準/許容/toolを変更しない。canonical Ctrl+P→f→0はfiles scopeのlisted_positionsと空completionsへ進みExResultを呼ばない。#392の正式palette5000旧5試料[2.673,2.159,2.152,2.648,2.011]ms、中央値2.159/基準2.630/上限4.630を一指標の限定記録として再利用した。

旧c6ce1ce→新4d9aaefでExResult以外の製品source、112製品compile commands（root/build/commit表記だけ正規化）、tool versions、正式tool/input定義/基準が不変。host fingerprint bc8a356f37c68491、i9-10850K/RTX3090/DPI120、HP推奨powerも一致。元invocation/raw/guard/audit/visualの5SHAをformal-reuse.jsonに保持する。これは旧exeの結果であり、新exeの新正式測定、正式8本成功、Ex UI速度の測定ではない。旧chord-call区間内の完全な入力出所識別不能という制約も継承。成功済みraw再計算/PNG目視/試料取得は繰り返していない。

### 全試行・採否・再利用

製品build/対象unit/39oracle/性能/GUIの失敗は0。準備時のfixed JSON key誤認、ADR/ログ/file basename推測、Windows glob、出力切れ、GUI準備のtuple/list比較とsource名表記、レビュー抽出のconst/constexpr/blank line誤認、次候補memoのnew APIという語は原版/失敗raw/訂正理由を保持した。非製品の読取・記録失敗を成功検証と数えない。helper原版とjournal、`implementation/implementation-notes.json`、`session-journal.md`が全試行記録である。GUI/性能の再取得なし。

独立fixed-input/harness/productレビューに未解決P0/P1/P2なし。親が全試料監査・全42PNG・事前条件を確認し2026-10-11 12:57 JSTに技術受理。通常draft PR→必要な最終記録レビュー→Ready/必須CI→eng/merge-pr.py plan/同expected-head execute→main同期→snapshot-394へ恒久収載→追加WT/OUT/t394の絶対D path/取込/dirty・untracked・ignored/唯一成果/稼働/link監査整理へ進む。実完了はacceptance-394。branch/commitを保持する。

4d9aaef以後は結果8文書だけで関連src/tests/入力/依存/必要環境が不変なら、817checks/39oracle/比較/GUI/限定正式再利用をpush・review・mergeで再利用する。工程やSHAだけの再試験、全件回帰/coverage/無関係な正式7本は行わない。

#392はPR393/main5d22cebへ12:01:08 JST統合済み。snapshot-392の6295records/155241400bytesを全SHA収載し12:04:12 JSTに追加WT/OUT/t392を監査整理した。次候補はCommandLine::completionsのoptional<string>::value_orが既存string_view APIの前に作る所有copy。ローカルMSVC headerと同期呼出しの寿命を読取確認しただけで、未Issue/未実装/未測。completed()の所有seedは別契約として維持する案。空palette hintのreserve案も未実験。C8/R8/IO6〜IO8と#365/#373の処分を変えず、hideの停止まで改善を継続する。
