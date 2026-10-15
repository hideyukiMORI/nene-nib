# 5-du — タブ表示は省略後の範囲だけを所有する（Issue #388、ADR0107）

出典: docs/quality/gate-proofs.md の節 5-du（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-11 09:32 JSTのhideの明示再開により、00:36の停止を履歴として保持して改善を再開した。今回指定された分担に従い、Astraが設計・採否、SOLが実装/対象検証、別SOLが独立レビュー/GUI準備、LUNAが次候補の読取調査を担当。GUI/性能は親が一席で排他実行した。先行設計2d2bf3c → 同一harness3d599b7 → 製品346796a。製品差分はTabTitle.cppの22追加14削除だけ。prefixなしfitを直接parseし、他はUTF-8境界へ戻した最大256bytesだけ一時所有する。同じDisplayText::parseを唯一の所有検証にする。

ARC-001/004/007/008/011、CPP-002/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004。公開API/settings/schema/本文/許可/基準/閾値/抑制の変更なし、waiver none。

以下のOUTはD:/NeNeNib/outputs/20261011-tab-display、WTはD:/NeNeNib/worktrees/388-tab-display。完了後の原記録はD:/NeNeNib/evidence/speed-optimizations-20261011-wave4/snapshot-388/files/outputs/20261011-tab-displayへ対応し、統合・収載・監査整理の実結果は同wave4/acceptance-388へ記録する。元停止時3原稿はwave3/pending-388-20261011-0034にも残す。固定入力SHA24e3b2b7d9f301f8d82651e6495e15bdf6f4918757b851961c51790e2b10fbc4は再開前後不変。

### 対象と実行済み検証

| コマンド・記録入口 | 検出する退行 | 結果 |
| --- | --- | --- |
| `pwsh -NoProfile -File OUT/implementation/check-harness.ps1`、`source-proof.py`、`enum-proof.py` | 既存入力/selector/assertionを変えず新9条件を登録し、closed enumの漏れを拒否する | Python対象4成功、旧89probe/47selector/default/assertions保持。compiler正例0/未処理値反例1。初回証明scriptの誤りは下記 |
| `pwsh -NoProfile -File OUT/implementation/build-before.ps1`、`build-after-release.ps1`、`build-after-debug.ps1` | 唯一の製品変更と新harnessのstrict build、tidy、sanitizer | before Release122steps、通常Release117+probe/unit65steps、Debug170steps成功。clang-cl/tidy19.1.5、ASan/UBSan/no-recover。全native stdoutの保存不足は下記 |
| `pwsh -NoProfile -File ./eng/build-release.ps1 -Ref 346796ac366ef5ef5fff4832d7154b6a4b4b8a34`（after-release script内） | GUI/正式速度の通常Release provenance | out/release/346796a.json、製品SHA76965ccf74da69d10066241576accfd5a4b62416e0885e2ab5a54b7297b2a418。build/release-346796a/NeNeNib.exe |
| `build/tab-display-debug-346796a/nib_tests.exe --file-path`、`--tabs`、`--command-palette` | UTF-8省略境界/保存印/一覧/切替の直接caller | 26+291+495=812checks成功。旧806+新6。Release unitは未実行 |
| `python -X utf8 OUT/implementation/oracle-smoke.py <side> <build> <commit> <config>` | new9+frame4の全文/code points/optional/元入力/保持値、metadata/marks | before Release/after Release/after Debug各13を1iterationずつ、計39成功。速度採否には流用しない |
| `python -X utf8 OUT/implementation/source-graph.py tab-display-debug-346796a`、`python eng/symbols.py --root WT --build-dir WT/build/tab-display-debug-346796a --require core application` | 変更source規約と実依存/OS漏れ | source/CMake graph違反0、2library symbols違反0。変更C++整形/whitespace成功 |
| `python eng/protected-diff.py --base 6536c9681a5b911a1cf8e69ab6cfe193721fa23e --head 346796ac366ef5ef5fff4832d7154b6a4b4b8a34` | fixture/基準/許可/既存scopeを維持 | 1853fixtures不変、metadata/追加/削除/変更0。旧47scopeは未測表示であり全件実行ではない |
| `python -X utf8 OUT/implementation/audit-provenance.py` | source/flags/tool/exe/harnessの比較同一性 | 全3capture照合成功。before sourceTree e5387e72062b951481206bef813fa403f70041e7、after efccd021764f640afeaa30cf5c8c47dc9458b398 |
| `python -X utf8 OUT/runtime/run-comparison.py`、`audit-comparison.py` | 3利益と6境界/4直接frame費用 | 初回13条件各120対応組、全条件成立。156runs/3120durations/6240marks/1560pairsを全rawから再計算一致 |
| `python -X utf8 OUT/gui-preparation/gui-tab-display.py run`、`OUT/runtime/audit-gui.py` | 保存/未保存/undo/一覧/選択/通常・Vimの表示と本文 | 初回9対全client0px、18本文copy、162checkpoints、22mode観測、2正常close。親が全22PNG目視。長いfolderが未描画の制限は下記 |
| `python -X utf8 OUT/gui-preparation/folder-supplement/supplement.py prepare`、`run`、`audit.py` | 短い題名の横で長いfolderが実描画される分岐 | 別固定1対0px、2本文copy、28checkpoints、4mode観測、2正常close。親が全4PNG目視。元9場面再取得なし |
| `python -X utf8 OUT/runtime/run-formal-guarded.py --executable WT/build/release-346796a/NeNeNib.exe --sha256 76965ccf74da69d10066241576accfd5a4b62416e0885e2ab5a54b7297b2a418`、`audit-formal.py` | 名前のある長行文書の入力→frameという直接caller | 初回5/5有効、中央値5.939ms、上限7.431ms内。全raw再計算一致。他7指標未測 |

全コマンド引数/終了/各unit・probe・静的stdout/stderrはimplementation/verification-raw、oracle-smoke、enum-proof等を参照。実CMake/flags/tools/source SHAは各side/build-captureへ保存。before probe SHA a76ff72f37d4855b524fbc2eedec0fe73bcdf99879bc2a6e9f1e6d17c4752505、after Release110bdf2e671589a2936de01157765bbe13229fd2581a481f6cf8d31801875710、Debug c5d339a40ae68637fc1a0b17f6fb20cc1af420c0c9f3fced0524f75f3c84f65e。compile commandsはbuildpath/commit metadataだけを正規化して前後一致。比較と正式速度のLastInput/環境/電源/exeは前後一致。連続監視とはしない。

### 固定比較と利益・費用

| 条件 | before / after中央値µs | 対応比中央値 | 対応差中央値µs | 短縮/120 | cycle比中央値 |
| --- | ---: | ---: | ---: | ---: | --- |
| tab-display-ordinary-saved-1024 | 336 / 243.5 | 0.7242932533 | -93 | 116 | 0.7204131653 / 0.7297297297 / 0.7210657932 |
| tab-display-deep-saved-1024 | 327 / 313 | 0.9570552147 | -14 | 111 | 0.9570552147 / 0.9587808607 / 0.9570552147 |
| tab-display-wide-saved-1024 | 1043 / 1024 | 0.9793781848 | -21.5 | 107 | 0.9760496183 / 0.9826501148 / 0.9769673704 |
| tab-display-short-saved-1024 | 73 / 57 | 0.7808219178 | -16 | 118 | 0.7808219178 / 0.7808219178 / 0.7777777778 |
| tab-display-exact-saved-1024 | 499 / 446 | 0.8953722334 | -52 | 110 | 0.8991935484 / 0.8902191235 / 0.8937875752 |
| tab-display-modified-boundary-1024 | 514 / 507 | 0.9863013174 | -7 | 98 | 0.9863013174 / 0.9844357977 / 0.9902248196 |
| tab-display-folder-wide-1024 | 552 / 546 | 0.9890909091 | -6 | 101 | 0.9872958258 / 0.9890909091 / 0.9909173404 |
| tab-display-root-saved-1024 | 65 / 53 | 0.8030303030 | -13 | 120 | 0.8030303030 / 0.8000000000 / 0.8153846154 |
| tab-display-untitled-modified-1024 | 46 / 42 | 0.9130434783 | -4 | 114 | 0.8913043478 / 0.9130434783 / 0.9241134752 |
| frame-document-short-saved-256 | 272 / 258 | 0.9486236802 | -14 | 111 | 0.9516728625 / 0.9456735497 / 0.9483387800 |
| frame-document-long-saved-256 | 288 / 273.5 | 0.9479166667 | -15 | 87 | 0.9477351916 / 0.9510489510 / 0.9495680894 |
| frame-document-long-failed-256 | 240 / 240 | 1.0000000000 | 0 | 56 | 0.9957716513 / 1.0000000000 / 1.0000000000 |
| frame-document-untitled-256 | 120 / 122 | 0.9829059829 | -2 | 88 | 0.9829059829 / 0.9970588235 / 0.9675074444 |

20iterations/ABBA3/各120組を一度取得。最初3利益は比<1・全3cycle<1・短縮>=90、残り10費用は比<=1.10かつ差<=50µsをすべて満たした。利益の通常/深いASCII/日本語の1024表示生成・破棄で約27.57/4.29/2.06%短縮。UI全体の率へ換算・別改善と加算しない。long-failed frameは比1.0/差0、untitled frameは対応差-2µsだが非対応の各群中央値120→122µsであり、全入力一様の改善とはしない。除外/補完/再取得なし。

### GUIの見える範囲と補足

beforeは#386通常Release（SHA71fbe5ed83009141a46c534ee3c6cfa692e3c06afce6ba00962e3d729a5143d4）を再利用。9388207から基準6536c968までの差はdocs9件だけで、製品/build/input/resources不変を確認した。取得前script SHA d31784733ff804a6f19fe1dfbbebcc35fa3c1952d16eaced6d5eea24eae1b07a、capture-plan SHA313a415714ba5bdbc4b701bb02d471f45481e457a7e0ffae669d45bc9a174142。全1280×800無mask/2560点非被覆/私有profile/settings/closedIME/LastInput/foreground/title/client/modeRGBを確認。専用4入力の本文はa日<TAB>end<CR><LF>last、元bytes/mtime不変。

元9場面の:tabsは3行を表示するが、exact/long題名が行幅を消費して長いfolder detailを描画しない。これは画面一致の失敗ではないが、長いfolderの目視検証として不十分。独立読取レビューでDirect2DRendererの残り幅判定と最大640DIP panelを確認し、元の資料×40親にf.txtを新規作成する別1場面を先に固定した。supplement plan SHA8f467666dd474675527208e284836d890dab95728c0aa09d92422340e58d7585。helperは元scriptからexpectedPaletteRowsに補足scene=1を足すmetadata一箇所だけの派生で、actions/guardsは不変。

補足の各画面はf.txt一行、灰色のD:\NeNeNib\evidence\t388\と日本語folderを実描画し、全client0px。rendererは残り幅で末尾を省略するので、完全なfolder文字列/最後のcore ellipsisまで目視したとはしない。全bytes/code points/UTF-8境界は独立core oracleで確認する。元9のraw/auditとvisibilityLimitを保持し、撮り直し・mask・合格条件の緩和なし。

### 正式速度・失敗・受理と再利用

正式long-line初回5試料は[5.333,6.040,5.939,6.043,5.441]ms、中央値5.939、基準5.431/上限7.431。欠測/退行/計測不能0、各202inputの第2inputから直後frameを全rawで再計算一致。基準中央値より0.508ms遅い観測であり、今回差分によるGUI速度向上の証拠とはしない。別commit/別時刻の#386中央値5.079msとの対応比較でもない。基準/許容/計測器は変更せず、他7指標未測。取得中は他build/test/GUI/hash/copyを止めた。

PowerShell Start-Transcriptがnative build stdoutを捕捉せず、全3buildの完全stdoutは未保存。以前の全raw保存済み表現を訂正した。toolで観測したNinja終了0、canonical record、保存Ninja log、source/flags/exe SHAを別証拠として保持し、ログ目的の再buildなし。Debug終了後のbarrier75/wrapper1はNinja170steps終了0とcapture成功後の意図的な収集切替で、build失敗ではない。各unit/probe/enum/static/性能の完全rawは保存されている。

初回source-proofは同selectorを二重計数、enum抽出は#pragma onceを直接TUへ入れたため警告をerrorとして拒否。失敗版/原rawを保存し、証明scriptのみ修正した。補足GUI監査初版はcheckpointsを16と誤記したためexit1。実helperの1startup+3mode+2scene+4copy+1palette+2capture+1close=14を名前列で照合する監査へ訂正し、同じ原記録で成功。GUI再操作なし。GUI期待folderのslash訂正は窓0の時点で原script/plan保全後に実施。推測path/rg glob/出力切れの読取失敗、報告の推測日時誤記もsession journalに残し、製品失敗/計測試料とは区別する。

独立harness/productレビューはP0/P1/P2なし。補足の必要性も独立読取レビューで確認し、親が全26PNG・全raw再計算・条件を確認して10:23 JSTに技術受理。通常draft PR→Ready/必須CI→eng/merge-pr.py plan/execute→main同期→恒久収載→監査整理を行い、実結果をacceptance-388へ残す。branch/commitは保持。製品346796a以降の関連src/tests/入力/依存/必要環境が不変なら成功済み結果を再利用し、docs/SHA/工程だけで製品検証を繰り返さない。全件回帰なし。

C8入力中parse保持はnoincsearchの追加費用/所有設計が未解決で未実験。次の読取候補はC2の残りのcommand入力成功一時値のcopyで、まだIssue/設計固定/実装/測定なし。#365/#373、R8、IO6〜IO8の処分は変えない。元26候補の採否件数は不変。hideの停止まで改善を継続する。
