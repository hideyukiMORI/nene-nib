# 5-dt — ファイル名の探索を末尾の区切りで止める（Issue #386・ADR0106）

出典: docs/quality/gate-proofs.md の節 5-dt（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-10 23:39〜2026-10-11 00:20 JST。#384はPR385/必須Check38060262470（head5ef768b）で、新道具eng/merge-pr.pyのplan→executeを実施してmain0c1d399e2e830ac9653e609e377bc0704fb17619へ統合。実subjectはPRtitleそのままの`chore(git): 統合件名は検証したPRタイトルから取得する (#384)`。snapshot-384は31records/7924556bytes、manifest ea32a0fc4d2a5c6ff7bc8ed151f4e3262f6e77c4e439643d13463a5cc8b64be8。全SHA照合/7ignored分類/未保存・唯一成果・稼働・linkなしの監査後23:38:23に追加WT/OUTを整理しbranch/commitを保持した。統合後の正本はacceptance-384/closeout.md。前日の「当PRで実施予定」は履歴であり未完了ではない。

対象はARC-001/004/007/008/011、CPP-002/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004、ADR0010/0021/0057/0082/0106。waiver none。先行ADR4322d31、初版harness b8273cc（build失敗）、修正harness a1630ab414fd0105cbd39a6b21bd20f9476270c0、製品9388207f04ffaf5922d357bb17d59a11507aef06。

製品はsrc/core/FilePath.cppのprivate name_startだけ4追加5削除。経路全体の前方走査を、右端の\または/で止める後方走査に替える。index>0のときindex-1を見るためunderflowせず、UTF-8継続byteにASCII区切りは現れない。最後の区切り直後/末尾の空名/区切りなしのoffset0と借用寿命は不変。parse/公開API/所有/正規化/表示/保存schema/設定/閾値/抑制/allowlistを変更しない。TabTitleのcopy案は未実装・未実験で見送る。

WT=`D:/NeNeNib/worktrees/386-file-name`、OUT=`D:/NeNeNib/outputs/20261010-file-name`、branch=`refactor/386-file-name`。恒久先はwave3/snapshot-386、統合/整理証拠acceptance-386。収載後はsnapshotのfiles/outputs/20261010-file-nameから元pathへ対応させ、rawのpathは書き換えない。

### 差分から選んだ検証と実結果

| コマンド・範囲 | 回帰リスク・選定理由 | 実結果 |
| --- | --- | --- |
| `python OUT/source-proof.py`、Python `ProbeComparisonTests`の新file_name入力/不正名と既存status/frame登録の4tests | 新7入力/hash/登録と旧82の区間・typed表・旧unit/defaultを保つ | 4tests初回0、0.006s。旧82入力/既存関数block/旧46selectors/assertions不変。初回class誤指定は別記 |
| 変更9test sourceのclang-format、`python OUT/scoped-static.py`、`python OUT/dispatch-proof.py` | 新FileNameWorkloadの網羅性/規約 | 整形・規約・waiver0。正例compiler0、windows欠落のcompiler1と期待診断を確認。5引数helperは実tidyが拒否し後述の修正後build0 |
| `pwsh -NoProfile -File OUT/build.ps1 -Phase before-release-parameters -Configuration Release -BuildDir build/probes-before-release`、同after-release/build/probes-after-release、`python OUT/smoke-before.py` | 同一正規harness/最適化で内容・借用位置・保持viewを確認 | 修正後before/after Release0、新7+旧frame短長のbefore各1iteration全0。smokeの時間は利益に使わない |
| PowerShell内`& OUT/build.ps1 -Phase debug -Configuration Debug -BuildDir build/386-debug -Targets @('nib_perf_probes','nib_tests','NeNeNib')`、`pwsh -NoProfile -File OUT/verify-debug.ps1` | 名前取得と直接callerのタブ・候補表示/切替、借用境界 | 正規tidy/ASan/UBSan/no-recoverで3targets0。--file-path20/--tabs291/--command-palette495の806checks、新7+旧2の各1iteration全0 |
| 製品整形、`python OUT/source-proof-product.py`、`python OUT/check-product.py`、`python eng/symbols.py --build-dir build/probes-after-release --require core application` | 私有探索だけ、他src不変と依存逆流なし | 全fileの期待変換一致、source/実CMake graph違反0、2lib違反0 |
| `python -X utf8 OUT/run-comparison.py`、`python -X utf8 OUT/audit-comparison.py` | 3利益と裸名/根/末尾/直接frameの6費用 | 初回9条件各120組、全条件成立。108process/2160durations/4320marks/1080pairsを全rawから再計算し一致 |
| `pwsh -NoProfile -File ./eng/build-release.ps1 -Ref 9388207`、`python OUT/capture.py normal-release-post build/release-9388207` | 正規Releaseの表示/正式速度 | 初回0、out/release/9388207.json、711source/実flags/tools/exe hashと全stdout/stderrを保存 |
| `python -X utf8 OUT/gui-file-name.py <normal-exe> <sha>`、`python -X utf8 OUT/resume-gui.py`、`python -X utf8 OUT/audit-gui-resumed.py` | 名前/場所/未保存印/undo/一覧/選択、本文と外乱 | 初回before5場面後にnotation拒否。元記録を保持し未送信最後1場面だけ再開。未実施after6を一度取得、6対全client0px/12copy一致/100checkpoint/4modeRGB/2正常close。下記の復旧根拠を参照 |
| `python -X utf8 OUT/formal-speed.py --check --bench key-to-frame-single-long-line --executable <normal-exe>`、`python -X utf8 OUT/audit-formal.py` | 名前ある文書のframeという直接caller | 初回5有効、欠測/退行/計測不能0、全raw再計算一致。他7指標は今回未測 |
| `python eng/protected-diff.py --base 0c1d399 --head 9388207` | fixture/基準/規約/許可/既存scopeを保つ | exit0、1853件のmetadata/追加/削除/変更0、protected none。exe/build未指定、47scope未測の記録であり全件実行ではない |

新--file-pathは既存verify_file_path/verify_tab_titlesをwrapperから呼ぶ。既定verify_text_and_caretの呼出し・旧46selector・全既存assertionsは不変。新7probeはparseを区間外に置き、file_name4096回とbytes/空でない端byteのchecksumを測る。全文/名前の独立literal/借用data offsetと初回保持viewは前後区間外で照合する。input/byte/hash/checksumはADR0106の先行値で、同じ実装をoracleとして複製しない。

before probe SHA eff40e92f62a7a203ef2025dc199bc3676ef2150df8a0193e72365ca247cd081、after d20c7baff71c3eb533f22a7f2711e2946232b827b1db186153abcd50ca6e727e、通常Release 71fbe5ed83009141a46c534ee3c6cfa692e3c06afce6ba00962e3d729a5143d4。GUI beforeはsnapshot-382のrelease-9c6031a（SHA7de1cd6822718c8a63f17d6df02cdd13c87502fb8ab6b5b11ecbbd52ff1477ee）、srcはmain0c1d399/beforeと同一。全compile commandsはbuildpath/commit metadataだけを正規化して一致、711source/harness/入力/toolsを保存照合した。指紋bc8a356f37c68491、i9-10850K/RTX3090/120dpi、環境の前後snapshot一致。連続監視とは主張しない。

### 固定比較と限界

| 条件 | before / after中央値µs | 対応比中央値 | 対応差中央値µs | 短縮/120 | cycle比中央値 |
| --- | ---: | ---: | ---: | ---: | --- |
| file-name-windows-4096 | 114.0 / 19.0 | 0.1666666667 | -95.0 | 120 | 0.1666666667 / 0.1666666667 / 0.1666666667 |
| file-name-deep-ascii-4096 | 2005.0 / 19.0 | 0.0094881404 | -1986.0 | 120 | 0.0094952524 / 0.0094597965 / 0.0094881404 |
| file-name-mixed-utf8-4096 | 1798.0 / 29.0 | 0.0161469933 | -1769.0 | 120 | 0.0161380078 / 0.0161425006 / 0.0161559889 |
| file-name-bare-short-4096 | 23.0 / 19.0 | 0.8260869565 | -4.0 | 112 | 0.9090909091 / 0.8260869565 / 0.8260869565 |
| file-name-bare-long-4096 | 1747.0 / 1745.0 | 0.9988541964 | -2.0 | 70 | 0.9999998359 / 1.0002863688 / 0.9977090493 |
| file-name-trailing-4096 | 1978.0 / 8.0 | 0.0040424457 | -1970.0 | 120 | 0.0040424457 / 0.0040444894 / 0.0040424457 |
| file-name-root-4096 | 11.0 / 8.0 | 0.7272727273 | -3.0 | 120 | 0.7272727273 / 0.7272727273 / 0.7272727273 |
| frame-document-short-saved-256 | 277.5 / 272.0 | 0.9881656805 | -4.0 | 82 | 0.9834895497 / 0.9889093891 / 0.9896492564 |
| frame-document-long-saved-256 | 313.0 / 291.0 | 0.9246376812 | -25.0 | 102 | 0.9255663430 / 0.9347906461 / 0.9161290323 |

20iterations/ABBA3/各120組を一度取得。最初3利益は比<1・全3cycle比<1・短縮>=90/120、残り6費用は比<=1.10かつ差<=50µs/固定区間を満たした。裸長1024bytesはほぼ横ばいでcycle2は1.000286、短縮70組。費用条件を事後に利益へ替えない。通常/深いASCII/混在UTF8の局所取得4096回で約83.3/99.1/98.4%短縮を観測したが、UI全体や単発callの倍率ではない。direct frame短長は費用として比較し、対応比.988166/.924638。除外/補完/再取得なし。

### GUI取得の失敗と限定した再開

固定script SHA5f5d3081a5403a621008690bede06a6a5897c169d4c1a1ae52cb2618cbe2d71a。私有profileと同一本文のshort.txt/日本語親経路+日誌🖋.txtを使う。6場面はshort-saved、long-saved、long-modified、long-restored、tabs-long、selected-short。Z追加/undo、:tabs一覧の2行目から上矢印・Enterでshortへ選択。本文:w別pathは名前/dirty状態を保持。最初5画像Vim NORMAL、最後は実通常mode。全1280×800無mask、2560点非被覆、IME閉、LastInput/foreground/title/client/DPI guard。

初回beforeは5場面取得後、selected-shortの`<Up>`をwindow_driver.parse_keysが未対応として拒否した。postは状態確認→parse→commands記録→送信の順であり、Up/Enterは未送信。原record.jsonはstatus failed、failure.png/元5capture/5copy/1mode/plan/scriptをSHA付きで保持し上書きしない。afterはまだ未着手だった。

別gui-resume-planを先に固定し、Windows SDK WinUser.hのVK_UP=0x26を確認してimport済みdriver.KEY_NAMESだけにnotationを足した。既存press/WM_KEYDOWN経路を使い、repo driverと製品は無変更。同PID37736/HWND4065772/LastInput54391937/foreground/title/client1280×800/DPI120/IME0を再確認、現在画面は停止前tabs-longとPNG SHA/全RGBが同一だった。元runのstate/post/mode/body_copy内関数はAST一致を要求。SCENES[5:]だけを同processへ実行してbefore/resume-record.jsonへ別保存、正常exit0を実process handleで確認した。after6場面は元runにnotation追加だけで一度取得しexit0。

comparison-resumed/audit-resumedは6対0px、本文12physical bytes一致、100checkpoint（before51/after49）、4modeRGB、2正常close、元input/settings/原ファイル不変を確認。親は全16最終PNGとfailureを直接表示し、visual-qa.mdへ記録した。原auditの目視待ちtrueは取得時点のまま保持。初回一括成功・撮り直し・製品差のmaskとは扱わない。日本語の長い場所は幅で省略され、全pathが画面上で読めるとはしない。

### 正式速度・その他の失敗・再利用

正式long-lineは[4.833,6.364,5.079,4.480,6.140]ms、中央値5.079、基準5.431/上限7.431。5/5有効、欠測/退行/計測不能0。全5rawの202input/第2inputから直後frame/連番/hashを再計算一致。基準内を示す結果であり、この差分単独のGUI短縮率は算出しない。他7指標は今回未測。取得中はbuild/test/GUI/hash/copyを並走していない。

初版正式singleは無題でfile_nameを通らないとsourceから判明し、全試料取得前に名前あるfixtureのlong-lineへ訂正した。初版wrapper/auditと事前訂正文を保存。初版harness b8273ccはname_matches5引数がCPP-012により実tidyで拒否されexeなし。全文照合をrunnerへ移してhelper4引数へ修正し、区間/入力/oracle要件を保ったa1630abから未取得系列を開始した。原cpp/generator/flags/cache/build logsを保存する。

C++ literalの二重escapeは生成直後・build前に発見し初版保存後に修正。source証明の誤SHA置換とunittestの誤class指定（loader4errors）、Debug flags補助assertの結合表記誤認を原転記と区別して残した。実Debugは別々の-fsanitize=address/undefinedとno-recoverで正常でありflagsを変更していない。推測path/Windows globの読取失敗、出力切れ、終了済みsessionへの照会失敗もjournalへ記録。これらは製品成功や性能試料として数えない。

GUI再開の妥当性は00:21:31の独立読取レビューでP0/P1/P2なし。原21commands/5場面が不変の先頭部分、追加4commandsだけであること、保存16files/4関数AST/同窓/全RGB/本文/guardsを独立照合し、閉じたIMEの固定6場面の同値性根拠として受理した（OUT/review-gui-resume-accepted.md）。全事前条件と合わせ技術受理。製品の小さな私有探索変更は単体自己レビュー済みであり、今回は取得途中の復旧に限定して必要な独立確認を追加した。通常PR/必須CI→eng/merge-pr.py plan確認/execute→main同期→恒久収載→監査整理へ進む。関連src/tests/入力/依存/環境が9388207から不変の成功は、工程/文書/SHAだけで再実行しない。全件回帰なし、公開API/schema変更なし、waiver none。#365/#373、C8入力中parse保持/R8/IO6〜IO8/TabTitleコピーは未解決または未実験のまま。hideの停止まで改善を続ける。
