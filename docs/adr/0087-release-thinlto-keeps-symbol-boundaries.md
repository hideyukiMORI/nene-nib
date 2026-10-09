# ADR 0087 — ReleaseのThinLTOは既存のシンボル境界を維持して比較する

- 状態: 構成実験の設計は受理、製品採用対象外（現ゲート未対応、閉じた比較のみ）
- 日付: 2026-10-09
- Issue: #333
- 影響する規則: ARC-001 / ARC-002 / ARC-003 / ARC-007 / CPP-013 / CPP-015 / CPP-016 / CPP-018 / QLT-001 / QLT-010 / QLT-012 / QLT-013 / QLT-014 / ADR 0003 / ADR 0006 / ADR 0016 / ADR 0075 / ADR 0082

## 文脈

#334の統合候補2a31bb4の製品・試験・比較道具のsourceを変えず、翻訳単位間の最適化をReleaseの構成だけで比較する。親設計席の依頼書はD:/NeNeNib/briefs/impl-333-thinlto-sana-20261009.md。効果が無ければ統合しない。既存の基準値や許容を変更せず、窓なし固定比較と正式な速度受理を区別する。

## 決定

1. flagsの正本は既存eng/targets.cmakeのnenenib_targetだけ。Releaseのtarget_compile_optionsにgenerator expressionで-flto=thinを加え、EXECUTABLEのRelease target_link_optionsに/opt:lldltojobs=2を加える。構成は候補枝のRelease全体を置き換える一経路で、実装切替flagや環境flag注入を作らない。
2. 固定clang-cl/llvm-lib/lld-link、C++23、/MT、警告集合とclang-tidyは維持する。DebugのASan/UBSanと回復禁止は変更しない。SIMDの全target指定、PGO、新しいruntime依存、ThinLTO専用cacheは加えない。
3. ARC-003/007/CPP-013の既存symbols検査を、実際のRelease bitcodeのcore/application .libへそのまま適用する。検査器・allowlist・抑制・waiverを変更しない。未知の違反が出たら親へ報告し、境界を弱めて通さない。
4. 正典eng/build-release.ps1 -Ref HEADでclean commitの製品Releaseを作り、同じcacheへ明示configureしてnib_perf_probes/nib_tests/nib_window_testsだけ追加buildする。現在はbuild並列1、lld backend並列は2に固定する。製品とprobeのcommit/SHA/metadataを区別して記録し、実装席では製品GUIとprobe性能測定を起動しない。
5. 最適化が全翻訳単位の生成コードへ及ぶため、今回だけRelease nib_testsの既定core/application全契約とnib_window_testsの既定UI/WIC全契約を各一回確認する。狭い機能scopeでは未選択の型/分岐/境界を覆えない。eng/check.ps1の全面実行、外部Vim oracle、coverage、GUI、正式速度は選ばない。Debugのflags/sourceは不変なのでconfigureによるflags確認だけを行い、全再build/testはしない。
6. CMake File APIのgraph/conformance、Release symbols、Git/protected/whitespace、compile/link flags、PE import/sizeを確認する。非LTO製品とsourceが同じこと、全C++targetでReleaseにだけThinLTOが掛かること、Debugのsanitizerが維持されることを記録する。性能比較・正式GUI受理・PR/push/mergeと採否は親が担当する。

## 先行証拠と一次仕様

[Clang ThinLTOの公式仕様](https://clang.llvm.org/docs/ThinLTO.html)はlld-linkではcompile時だけ-fltoを指定し、bitcodeをlinkできることと、/opt:lldltojobs=Nによるbackend並列の上限を示す。固定版の現物で親がD:/NeNeNib/outputs/333-thinlto/へ先行証拠を保存している。

- probe_good.cppのpure countをbitcode .libにしたgood-symbols.logは既存eng/symbols.pyのcore検査で0違反、親記録exit0。
- probe_bad.cppでQPCを故意importしたbad-symbols.logは__imp_QueryPerformanceCounterをARC-007違反1として拒否、親記録の期待exit1。bitcodeでも禁止入力が検査に現れる正/反例を同じ固定環境で再利用する。
- 最初のlink.logはclang-clの-fuse-ld=lld不足で失敗した。修正後のlink-lld.logとprobe_main.exeは親記録でexit0。最初の失敗は保存し、成功だけの履歴にしない。本席では先行probeを再実行しない。

## 現ゲートでの採否（2026-10-09）

clean ec8abd7の正典Release製品とprobeは構築できたが、変更していないeng/symbols.pyは実bitcodeのcore/applicationで303違反を報告した。llvm-nmのbitcode定義はハイフンのaddressで表示され、現parserが定義を認識しない。読み取り診断でaddress表示だけを補正すると、依存解決後の違反は両moduleの__ImageBase各1件になる。固定LLVMのMicrosoftCXXABIは標準例外/RTTIのimage-relative constantにこの参照を生成し、lldはsynthetic symbolとして解決するが、現allowlistでは許可していない。

親設計席は、ゲート・parser・allowlistを広げて候補を通さず、現ゲート未対応のため製品採用対象外とする。元のsymbols失敗を合格に読み替えない。保存した同一sourceのprobeで12workload各warmup1/sample1の正しさだけ確認し、閉じた前後比較は親席へ渡す。採用しない候補の全Release unit/UI契約は実行不要とした。固定source、警告、Debug計装、依存境界、検査器は不変。

実験exeとmetadataは実装commit ec8abd772adde2d9940e078900bb48a46dd8eac4として先に保存し、この採否注記の文書commitと区別する。証拠は同作業木out/reports/done-333-thinlto.md、out/333-symbols.log、out/333-symbol-diagnosis.log、out/333-smoke/、out/preserved/ec8abd7/。正式な速度・GUIゲートは未実施。

## 固定比較の結果（2026-10-09、親席batch3）

親席は同一sourceの非LTO 2a31bb4とThinLTO ec8abd7を、下記4caseに限定してABBA 3block×20iterationで比較した。各caseは12processすべてexit0、各processのwarmup1は区間外、before/after各120sample、対応比120組。各JSONはstatus observedでbatch3-summary.jsonと一致した。これは窓なし局所比較でQLT-014ではない。

| 区間 | before中央値us [範囲] | after中央値us [範囲] | 対応after/before比中央値 [範囲] |
|---|---|---|---|
| buffer 16MiB | 7105 [6122,11464] | 7266.5 [6252,12297] | 1.009759 [0.607591,1.553420] |
| open 16MiB | 11271.5 [9284,39456] | 11566.5 [9328,37715] | 1.007808 [0.464289,2.863053] |
| ordinary insert 200 | 512.5 [322,1250] | 520.5 [308,3667] | 0.964231 [0.691156,6.615152] |
| display long | 20 [20,70] | 18 [17,43] | 0.900000 [0.246377,2.150000] |

対応比は対応sampleごとの比の中央値で、独立したbefore/after中央値の商とは異なる。insertの対応比だけから利益を主張しない。displayの中央値差2us以外の利益は確認できず、主要区間に明瞭な改善はない。全caseでばらつきを残し、外れ値を除去していない。GUI描画・起動・ディスクI/O・未選択8workloadの改善へ一般化しない。

原記録はD:/NeNeNib/outputs/scoped-probes-batch3-20261009/の333-buffer.json、333-open.json、333-insert.json、333-display.json、batch3-summary.jsonと各.runs/。上表の比表示は丸め、原JSONの全精度・全sample・metadata・marksを保持する。測定の成功はsymbols失敗を取り消さず、現ゲート未対応による製品採用対象外を維持する。

## 限界と保存

bitcodeのシンボル検査は生成コード全体の意味や最終PEの全依存の証明ではない。最終importと12workloadの短い正しさ確認は実施し、採用対象外とした候補の全Release unit/UI契約は未実行。ThinLTOの利益・build時間・製品sizeは事前推定で採用を決めない。source/harnessは同一で、親の固定比較は上記4workloadに限定された。基準値・閾値・保存schemaの変更なし、waiverなし。

作業木D:/NeNeNib/worktrees/333-thinltoとD側のbuild/out/先行証拠は比較の参照として保持する。統合または不採用確定後に親が取込・未保存/ignored・唯一成果物・プロセス/リンクを確認して整理し、branch/commitは保持する。
