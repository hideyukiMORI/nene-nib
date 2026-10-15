# 5-ea — ステータス位置の数値を局所領域から所有する（Issue #402、ADR0113）

出典: docs/quality/gate-proofs.md の節 5-ea（2026-10-16 に D50 で移動・本文は移動時のまま）

#402 / ADR0113。StatusItems.cppの行・桁表示だけでto_string所有値の連結を除き、局所50byte範囲に固定literalと2個のunsigned十進数をstd::to_charsで書く。private helperは実ecをexpectedへ保持し、最大桁数を保証したcallerでvalue()を使う。書込済み範囲だけを既存fixed→DisplayText::parseで検証・所有し、stack viewは返さない。他製品525files/公開API/入力値/表示文言・順/encoding/ending/validator/settings/schema/flags/allowlistは不変、waiver none。

先行設計e2ca247ffbe05ca9823a55490b096a6fd544a104、harness3433f1f5e92af5ef337a5a860b4ee26c91493d8c、失敗製品42fed2957cd1e7f20a44752ced87aae27ea39727、失敗後設計00847fd8a729093402980e79ffb412b5bd1dedca、修正版製品cf27a3ad2aacc0ffd3384fa8cee7a6522c80b41f。初回3利益/4費用、Debug --status-items 24checks（旧8+境界16）/3構成21oracle、既存148probe/47selector/default/1853fixtures保持、GUI12対0px/24PNG・12本文copy・272checkpoint・24mode・2正常close・親全画像目視で2026-10-11 17:16 JSTに技術受理。

3項目の生成・破棄1024回という各局所区間は行123/桁456で約30.48%、size_t最大値同士で約41.69%、行123456/桁654321で約29.29%短縮。encoding/endingも各入力の一部であり、数値長だけの単因子比較ではない。費用4条件と全数値は[gate-proofs 5-ea](../../quality/gate-proofs.md#5-ea--ステータス位置の数値を局所領域から所有するissue-402adr0113)。局所率の加算/UI全体への換算はしない。共通status callerの正式key-to-frame-singleを初回5試料[0.609, 0.673, 0.551, 0.519, 0.652]msで新取得し、中央値0.609ms/基準0.906ms/上限2.906ms、欠測0。元基準内の確認であり変更前後のUI改善率ではない。他7正式指標は未測。

規則: ARC-001/003/004/007/008/010/011/012、CPP-001/002/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、GIT-001〜004。関連ADR0021/0022/0070/0105。基準main b13a04b41e708fb198a6b61bdbf84a31f2499278。CPP-014の一般format文とADR0022のcore locale拒否の関係を、位置表示のto_chars→既存parseだけの狭い例外として明記した。planned/閾値/抑制/allowlistの状態変更なし。他の数値callerは移行しない。

WT=D:/NeNeNib/worktrees/402-status-number、OUT=D:/NeNeNib/outputs/20261011-status-number。以下の相対証拠はOUT、恒久先wave4/snapshot-402/files/outputs/20261011-status-number。実統合・収載・監査整理はwave4/acceptance-402へ別記録し、予定を完了扱いしない。

### 変更に関係する検証

| 退行リスク / 対象 | 実行したコマンド・結果・証拠 |
| --- | --- |
| 0/桁境界/表示bytes・codepoints/元の所有値とview保持 | Debug `nib_tests.exe --status-items`24checksを1回成功。旧8body不変、新16を別harnessで先行。ASan/UBSan/no-recover、implementation/commands/debug-unit-status-items。Release unit build/run0 |
| encoding/ending全6組、size_t最大、6桁、返却所有、直接caller全frame fields | 既存7probeをbefore Release/修正版通常Release/Debugで各1iteration/warmup1、計21oracle成功。implementation/oracle-smoke.pyと各side/oracle-smoke/record.json。性能試料とは区別 |
| strict compiler/tidy・clean refからの実exe | harness3433f1f before Release成功、最初の製品42fed29通常Release失敗を保存、修正版通常Release/Debug成功。通常Release `pwsh -NoProfile -File eng/build-release.ps1 -Ref cf27a3ad2aacc0ffd3384fa8cee7a6522c80b41f`、同buildへprobeのみ追加。全native configure/build/tidy stdout/stderrを保存 |
| 差分規約/CMake依存/core境界/fixture | 変更StatusItems.cpp format dry-run/Werror、先行CoreTests.cpp format成功を再利用。source-graph.py source/actualCMake 0違反、`eng/symbols.py --require core application`2libraries/0違反、`eng/protected-diff.py --base b13a04b41e708fb198a6b61bdbf84a31f2499278 --head cf27a3ad2aacc0ffd3384fa8cee7a6522c80b41f`1853→1853/差分0。implementation/commands |
| 同一harness/計測入力/実compile flagsとsource/exe来歴 | implementation/audit-provenance.py成功。Git blobとphysical SHAを別保持し、before/after Releaseはmetadata/build path以外の全compile flags同一。DebugのASan/UBSan/no-recoverを別確認。全source archive/cache/Ninja/FileAPI/compile commands保存 |
| 中間所有を除く利益と短値/直接callerの費用 | runtime/run-comparison.py --before-commit 3433f1f5e92af5ef337a5a860b4ee26c91493d8c --after-commit cf27a3ad2aacc0ffd3384fa8cee7a6522c80b41f初回1度。runtime/audit-comparison.pyで84runs/1680durations/3360marks/840pairsを原rawから再計算一致 |
| 表示行・桁literal/encoding/ending/mode/本文 | gui-preparation/gui-status-number.py inputs→freeze→run --exclusive-parent、runtime/audit-gui.py成功。12対1280×800 RGB全client無mask0px、親全24原PNG目視 |
| 共通status callerの正式応答が元基準内か | run-formal.py --executable D:/NeNeNib/worktrees/402-status-number/build/release-cf27a3a/NeNeNib.exeがcanonical measure-speed.py --check --bench key-to-frame-singleを実行、audit-formal.pyで5rawを再計算。formal-guardとformal-speed/audit.json、他7未測 |

実command/時刻/exit/原stdout/stderrはimplementation/commandsとcommand-capture。通常Releaseは1444864bytes/SHA3dc8a0a8feec808609b30d6d1feea870801bb093cc507628ac12f2b9025a2b5c、40hex Ref/clean source。構成別の成功/失敗をverification-summary.jsonと各build captureで区別する。例外関連の未許可symbolなし、gate緩和なし。

### 初回固定比較

| workload / 役割 | 前 / 後の各群中央値 µs | 対応比中央値 | 対応差中央値 µs | 短縮 / 120 | cycle比1 / 2 / 3 |
| --- | ---: | ---: | ---: | ---: | --- |
| status-items-sjis-crlf-1024 / 利益 | 189 / 131 | 0.6951871658 | -57 | 119 | 0.7005347594 / 0.6842105263 / 0.6959978382 |
| status-items-max-utf8-lf-1024 / 利益 | 308 / 179 | 0.5830618893 | -128 | 120 | 0.5811688312 / 0.5841354462 / 0.5792880259 |
| status-items-large-bom-crlf-1024 / 利益 | 199 / 141 | 0.7070632350 | -58 | 118 | 0.7092713568 / 0.6947810453 / 0.7070632350 |
| status-items-utf8-crlf-1024 / 費用 | 104 / 84 | 0.7980769231 | -21 | 118 | 0.7980769231 / 0.7904761905 / 0.8076923077 |
| status-items-bom-lf-1024 / 費用 | 107 / 87 | 0.8130841121 | -20 | 119 | 0.8207547170 / 0.8037383178 / 0.8130841121 |
| status-items-sjis-lf-1024 / 費用 | 104 / 84 | 0.8058252427 | -20 | 120 | 0.8067587752 / 0.7980769231 / 0.8076923077 |
| frame-document-short-saved-256 / 費用 | 258 / 252 | 0.9796311645 | -6 | 108 | 0.9804304534 / 0.9765165441 / 0.9804304534 |

fixed-inputs.json SHAb657467f8fb57db028d24e23972b637d66cc47c372007bf847e7fd7056145a7cを製品前に固定。既存6StatusItemsWorkloadとframe-document-short-saved-256を登録/source/独立literal oracle/計測区間ごと不変で再利用し、新probe/enum/runnerは0。6pureはstatus3項目の1024回生成・破棄とbytes+codepoints checksum、callerは256framesの全field oracleを区間外に持つ。各群中央値の差と対応差中央値は別量。

20iterations/warmup1/ABBA3/各120対応組の初回1回。利益3は対応比<1/全3cycle<1/短縮>=90、費用4は対応比<=1.10かつ対応差<=50µs/固定区間。host/power/LastInput不変、他agent/build/test/hash/copy停止。除外/穴埋め/合格までの再取得なし。正の対応費用: 費用4条件の対応差中央値に正の値はない。

### GUI・正式・限界

beforeは#400通常Releasef8cbda1/SHA9a6d117291d560385535675b665eec2c7248512cd8e860bbf82fb167e7136b5cをsnapshot-400から参照。基準mainまでdocsだけで製品不変。専用D:/NeNeNib/evidence/t402の6ファイル、各124行/行本文は日界+998個x。private profile/settings、13.5pt/client1280×800/DPI120、全キー列/入力/exeを起動前freeze。capture-plan SHA53e8506a5548eba2d0886361d66de9cf6e9ee396059c5f065b922075ec667dfa、preparation-plan SHAb523fd91c71220b27e7a754a454c324aa8c45bcf8bdeba7bc077d308d37791cd。

6sceneはUTF8/CRLF1/1 ordinary、BOM/LF9/24 Vim、ShiftJIS/CRLF10/100 ordinary、UTF8/LF99/999 Vim、BOM/CRLF100/999 ordinary、ShiftJIS/LF123/456 Vim。Vim count gg/0/count lを正規キー経路へ送り、copy用Escより前のcaptionをbeforeguardと共用、:wは私有copyだけへ出力、元位置/modeへ戻したafterguardを別取得。各side136checkpoint/42posted sequences/13mode transitions、計24mode/12本文copy/2正常close。元bytes/mtime/settings/IME/foreground/title/非被覆2560点/LastInputを確認。親の全24原PNG目視記録visual-inspection.json。

現行UIには本文の水平scrollがない。長桁caretは画面外にclipされるので、GUIはステータスのliteralと本文保持の証拠であり、画面外caretが可視だったとはしない。0/size_t最大はGUIへ注入せずcore oracleで扱う。BMPの本文場面であり、ここから全Unicode描画を一般化しない。

共通status callerの正式key-to-frame-singleを初回5試料[0.609, 0.673, 0.551, 0.519, 0.652]msで新取得し、中央値0.609ms/基準0.906ms/上限2.906ms、欠測0。元基準内の確認であり変更前後のUI改善率ではない。他7正式指標は未測。 canonical入力と内側retryを改変せず全raw5試料を保存、各202input marksから第2input→後続frameを独立再計算。outerguardはexe/source/host/power/LastInput/foreground・cover警告を照合。旧#392 files結果は今回の共通status変更へreuseしない。

最初のthrow案はtidyに拒否された。修正版は標準to_charsの実errorをexpectedへ保存し、容量50/各十進span20の証明下でvalue()を使う。容量不足は実入力では起きない不変条件の破れで、部分文字列/黙った代替出力/unchecked pointerを返さない。allocation失敗のタイミングと順は旧実装と同一と主張しない。実SDKの旧to_stringも局所数字配列を使うため、locale改善/割当回数を実測したとも主張しない。

### 全試行・採否・後続

最初のharness生成はCRLF仮定で変更前assertion停止。元sourceと観測tool結果は保存したが、その1件のみbinary原stdout/stderrの捕捉がなく、この限界を保持する。before初回7oracleのうち先頭nativeは成功したが、helperがFNV文字列と整数を比較して停止し、raw保存後に比較型だけ訂正、既存1件を再利用して残り6件を実行した。nativeを都合よく再取得していない。初回product準備ではformatterがscope外fixedコメントをreflowしsource-proofで停止、失敗source/diffを保存し元2行を復元した。修正版format準備はbare clang-formatがPATH外でPopen WinError2となりnative未起動、試行dirとtool tracebackを保持し、既定toolchainの絶対pathで実formatを成功させた。この未起動にはnative stdout/stderr自体が存在しない。

製品42fed29の通常Releaseは39/117のStatusItems.cpp tidyがcannot use 'throw' with exceptions disabledで失敗、40工程後Ninja exit1。Debug/後続oracle/性能/GUIは未開始のまま停止し、source/全native raw/partial build/FileAPI/無exeをafter-release-failed-product-v1へ保全。ADR00847fdで既存expected/value経路へ改訂し、独立設計review後に修正版を別commit/別buildで作成した。既存flags/allowlist/抑制は変更0。最初の失敗を修正版成功で上書きしない。親の推測path/rg glob/補助読取の失敗はsession-journal.md、全準備原版はOUTに保持する。

独立設計改訂/修正版製品レビューと親の原raw/全24PNG確認で受理。通常draft PR/最終独立記録レビュー/Ready/必須CI/eng/merge-pr.py同head plan→execute/main同期/恒久収載/絶対D path・取込・dirty/untracked/ignored・唯一成果・稼働/link監査整理へ進む。実完了はacceptance-402、branch/commit保持。3成功buildに加えて失敗release-42fed29の全partial filesとGit6refsを収載する。

製品commit以後は結果8文書だけなら成功済み24checks/21oracle/固定比較/正式single/GUIをpush/review/mergeで再実行しない。全件回帰/coverage/47無関係runtime/正式全8/未変更probe Python検証は対象外。#400はPR401/mainb13a04bへ16:16:43 JST統合、snapshot-400の889records/128721205bytesを収載し16:17:56 JSTに追加WT/OUT/t400を監査整理済み。次のpalette prefix組立と単一候補返却copyは読取調査だけ、未Issue/未実装/未測。元26候補の採否件数、#365/#373は不変。hideの2026-10-11 17:05 JST指示に従い、#402の区切り・日報引継ぎ・統合・収載・整理でいったん停止する。以降の新実験はhideの再開指示待ち。
